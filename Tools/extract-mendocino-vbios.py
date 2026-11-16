#!/usr/bin/env python3
"""Extract *this machine's* Radeon 610M ATOM VBIOS from a VFCT ACPI dump.

The tool makes no PCI writes and never fetches firmware from the internet.
Examples:
  Linux: python3 Tools/extract-mendocino-vbios.py /sys/firmware/acpi/tables/VFCT --output mendocino.rom
  Dump:  python3 Tools/extract-mendocino-vbios.py VFCT.dat --output mendocino.rom
The output is machine-specific and should not be redistributed.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


class VFCTError(ValueError):
    pass


def atom_ok(blob):
    if len(blob) < 0x4A or blob[:2] != b'\x55\xaa':
        return False
    offset = struct.unpack_from('<H', blob, 0x48)[0]
    return offset + 8 <= len(blob) and blob[offset + 4:offset + 8] in (b'ATOM', b'MOTA')


def parse_vfct(table, vendor=0x1002, device=0x1506):
    if len(table) < 0x4C or table[:4] != b'VFCT':
        raise VFCTError('not a complete ACPI VFCT table')
    size = struct.unpack_from('<I', table, 4)[0]
    if size < 0x4C or size > len(table):
        raise VFCTError('invalid ACPI table length')
    if sum(table[:size]) % 256:
        raise VFCTError('ACPI VFCT checksum mismatch')
    first, lib_first = struct.unpack_from('<II', table, 0x34)
    end = lib_first if 0x4C <= lib_first <= size else size
    if not 0x4C <= first <= end - 28:
        raise VFCTError('invalid VBIOS list offset')
    offset = first
    while offset < end:
        if end - offset < 28:
            raise VFCTError('truncated VFCT image header')
        bus, slot, function, vid, did, subvid, subdid, revision, length = struct.unpack_from('<IIIHHHHII', table, offset)
        offset += 28
        if length > end - offset:
            raise VFCTError('truncated VFCT image payload')
        blob = table[offset:offset + length]
        if (vid, did) == (vendor, device):
            if not atom_ok(blob):
                raise VFCTError('matching device image has invalid ATOM ROM header')
            return blob, {'pci_vendor': f'{vid:04x}', 'pci_device': f'{did:04x}',
                          'subsystem_vendor': f'{subvid:04x}', 'subsystem_device': f'{subdid:04x}',
                          'bus': bus, 'slot': slot, 'function': function, 'vfct_image_revision': revision,
                          'vbios_size': len(blob), 'sha256': hashlib.sha256(blob).hexdigest()}
        if not length:
            raise VFCTError('zero-length VFCT image')
        offset += length
    raise VFCTError(f'VFCT contains no image for {vendor:04x}:{device:04x}')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('vfct', type=Path, help='ACPI VFCT table image, from this machine')
    parser.add_argument('--output', type=Path, required=True, help='local output VBIOS file (never bundled)')
    args = parser.parse_args(argv)
    try:
        vbios, meta = parse_vfct(args.vfct.read_bytes())
    except (OSError, VFCTError) as exc:
        parser.exit(1, f'VFCT extraction failed: {exc}\n')
    args.output.write_bytes(vbios)
    print(json.dumps({'output': str(args.output), 'source': str(args.vfct), **meta}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
