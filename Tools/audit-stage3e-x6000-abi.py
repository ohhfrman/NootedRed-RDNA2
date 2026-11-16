#!/usr/bin/env python3
"""Stage 3E: inspect *actual* Apple X6000 Mach-O symbol tables, read-only.

Works with thin/fat x86_64 Mach-O files or a native .kext directory. An ABI
observation is NOT a compatible AMD controller, live IOKit attachment, DCN
initialization, or Metal support. No binary modification or PCI spoofing.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import plistlib
import struct
from pathlib import Path

MAX_FILE_BYTES = 256 * 1024 * 1024
MAX_COMMANDS = 8192
MAX_SYMBOLS = 2_000_000
# Do not derive support claims from naming. These are research targets only.
CLASSES = (
    "AMDRadeonX6000_AmdGpuWrangler",
    "AMDRadeonX6000_AmdRadeonControllerNavi10",
    "AMDRadeonX6000_AMDGraphicsAccelerator",
)


class MachOError(ValueError):
    pass


def span(blob: bytes, offset: int, length: int) -> bytes:
    if offset < 0 or length < 0 or offset > len(blob) or length > len(blob) - offset:
        raise MachOError("Truncated or out-of-range Mach-O structure")
    return blob[offset:offset + length]


def thin_offset(blob: bytes) -> tuple[int, int]:
    """Return (offset, size) for an x86_64 thin Mach-O, or fail explicitly."""
    if blob[:4] == b'\xcf\xfa\xed\xfe':
        return 0, len(blob)
    if blob[:4] not in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
        raise MachOError("Not a supported little-endian 64-bit or fat Mach-O")
    fat64 = blob[:4] == b'\xca\xfe\xba\xbf'
    n_arch = struct.unpack('>I', span(blob, 4, 4))[0]
    if not 1 <= n_arch <= 64:
        raise MachOError("Unreasonable Mach-O FAT architecture count")
    entry_size = 32 if fat64 else 20
    span(blob, 8, n_arch * entry_size)
    for index in range(n_arch):
        offset = 8 + index * entry_size
        if fat64:
            cpu, subtype, arch_offset, arch_size, align, reserved = struct.unpack(
                '>iiQQII', span(blob, offset, entry_size))
        else:
            cpu, subtype, arch_offset, arch_size, align = struct.unpack(
                '>iiIII', span(blob, offset, entry_size))
        if cpu == 0x01000007:  # CPU_TYPE_X86_64
            if span(blob, arch_offset, arch_size)[:4] != b'\xcf\xfa\xed\xfe':
                raise MachOError("FAT x86_64 slice is not a 64-bit Mach-O")
            return arch_offset, arch_size
    raise MachOError("No x86_64 slice in FAT Mach-O")


def inspect_macho(blob: bytes) -> dict:
    base, size = thin_offset(blob)
    sub = span(blob, base, size)
    magic, cpu, subtype, filetype, ncmds, sizeofcmds, flags, reserved = struct.unpack(
        '<IiiIIIII', span(sub, 0, 32))
    if magic != 0xFEEDFACF or cpu != 0x01000007:
        raise MachOError("Unsupported Mach-O CPU type or header")
    if ncmds > MAX_COMMANDS:
        raise MachOError("Unreasonable Mach-O load command count")
    span(sub, 32, sizeofcmds)
    cmd_offset = 32
    symtab = None
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack('<II', span(sub, cmd_offset, 8))
        if cmdsize < 8 or cmd_offset + cmdsize > 32 + sizeofcmds:
            raise MachOError("Invalid Mach-O load command length")
        span(sub, cmd_offset, cmdsize)
        if cmd == 0x2:  # LC_SYMTAB
            if cmdsize < 24 or symtab is not None:
                raise MachOError("Duplicate or truncated LC_SYMTAB")
            symtab = struct.unpack('<IIII', span(sub, cmd_offset + 8, 16))
        cmd_offset += cmdsize
    if cmd_offset != 32 + sizeofcmds:
        raise MachOError("Load commands do not fill header region")
    response = {
        "architecture": "x86_64",
        "macho_filetype": filetype,
        "symbol_table_state": "NO_LC_SYMTAB",
        "symbol_count": None,
        "observed_class_symbols": {name: [] for name in CLASSES},
    }
    if symtab is None:
        return response
    symoff, nsyms, stroff, strsize = symtab
    if nsyms > MAX_SYMBOLS:
        raise MachOError("Excessive Mach-O symbol count")
    symbols = span(sub, symoff, nsyms * 16)
    strings = span(sub, stroff, strsize)
    observed = {name: set() for name in CLASSES}
    for i in range(nsyms):
        strx, typ, sect, desc, value = struct.unpack_from('<IBBHQ', symbols, i * 16)
        if strx == 0:
            continue
        if strx >= len(strings):
            raise MachOError("Mach-O symbol name outside string table")
        end = strings.find(b'\0', strx)
        if end < 0:
            raise MachOError("Unterminated Mach-O symbol name")
        name = strings[strx:end].decode('utf-8', errors='replace')
        # Exact class prefix as part of a C++ mangled symbol, or documented
        # vtable metadata. Both are evidence only, never a runtime ABI pledge.
        for cls in CLASSES:
            if cls in name and ('_ZN' in name or '_ZTV' in name or '_ZTI' in name):
                observed[cls].add(name)
    response["symbol_table_state"] = "PRESENT" if nsyms else "EMPTY"
    response["symbol_count"] = nsyms
    response["observed_class_symbols"] = {
        cls: sorted(names)[:64] for cls, names in observed.items()
    }
    return response


def find_binary(path: Path) -> tuple[Path, str | None, str | None]:
    if not path.is_dir():
        return path, None, None
    info = path / 'Contents' / 'Info.plist'
    if not info.is_file():
        raise MachOError('KEXT directory missing Contents/Info.plist')
    plist = plistlib.loads(info.read_bytes())
    if not isinstance(plist, dict):
        raise MachOError('Invalid KEXT Info.plist')
    executable = plist.get('CFBundleExecutable')
    if not isinstance(executable, str) or not executable or '/' in executable or '\\' in executable:
        raise MachOError('Invalid CFBundleExecutable')
    return path / 'Contents' / 'MacOS' / executable, plist.get('CFBundleIdentifier'), plist.get('CFBundleVersion')


def inspect_path(path: Path) -> dict:
    binary, bundle, version = find_binary(path)
    summary = {
        "requested_path": str(path), "binary_path": str(binary),
        "bundle_id": bundle, "bundle_version": version,
        "native_binding_verified": False,
        "gc1037_initialized": False,
        "dcn316_initialized": False,
        "metal_verified": False,
    }
    if not binary.is_file():
        summary.update({"status": "BINARY_NOT_PRESENT_OR_EMBEDDED_IN_KERNEL_COLLECTION",
                        "symbol_table_state": "UNAVAILABLE"})
        return summary
    if binary.stat().st_size > MAX_FILE_BYTES:
        raise MachOError('Binary exceeds read-only audit size bound')
    blob = binary.read_bytes()
    summary["sha256"] = hashlib.sha256(blob).hexdigest()
    summary.update(inspect_macho(blob))
    summary["status"] = "ABI_SYMBOL_EVIDENCE_ONLY"
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary_or_kext', nargs='+', type=Path,
                        help='Apple X6000 KEXT directories or actual Mach-O executables')
    parser.add_argument('--output', type=Path, help='Save read-only JSON result')
    args = parser.parse_args()
    try:
        reports = [inspect_path(p) for p in args.binary_or_kext]
    except (OSError, ValueError, plistlib.InvalidFileException) as err:
        parser.error(str(err))
    result = {
        "stage": "R6_STAGE3E_X6000_BINARY_ABI_AUDIT",
        "target_pci_id": "1002:1506",
        "reports": reports,
        "interpretation": "ABI observation is not Apple controller compatibility or live PCI binding",
        "native_x6000_binding_completed": False,
        "dcn_edp_initialized": False,
        "metal_acceleration_verified": False,
    }
    rendered = json.dumps(result, indent=2, ensure_ascii=False) + '\n'
    if args.output:
        args.output.write_text(rendered, encoding='utf-8')
        print(f'Saved {args.output}')
    else:
        print(rendered, end='')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
