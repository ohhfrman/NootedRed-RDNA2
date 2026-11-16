#!/usr/bin/env python3
"""Read-only Apple IORegistry audit for AMD Mendocino 1002:1506.

Never changes kext matching, boot arguments, device properties, or PCI registers.
This can observe an attached service but cannot verify working GC/DCN/Metal.
Emits only a minimal summary, not raw IORegistry content (which may be private).
"""
from __future__ import annotations
import argparse
import json
import plistlib
import platform
import subprocess
from pathlib import Path

PCI_VENDOR = 0x1002
PCI_DEVICE = 0x1506


def pci_number(value):
    # macOS ioreg represents vendor-id/device-id as little-endian OSData.
    if isinstance(value, (bytes, bytearray)) and len(value) >= 2:
        return int.from_bytes(value[:2], "little")
    if isinstance(value, int) and not isinstance(value, bool):
        return value & 0xFFFF
    return None


def children(node):
    return node.get("IORegistryEntryChildren", []) if isinstance(node, dict) else []


def descendants(node):
    for item in children(node):
        if isinstance(item, dict):
            yield item
            yield from descendants(item)


def is_mendocino(node):
    if not isinstance(node, dict):
        return False
    return (pci_number(node.get("vendor-id")) == PCI_VENDOR and
            pci_number(node.get("device-id")) == PCI_DEVICE)


def candidate_class(node):
    if not isinstance(node, dict):
        return None
    names = [node.get("IOClass"), node.get("IORegistryEntryName")]
    for name in names:
        if isinstance(name, str) and (
            "AMDRadeonX6000" in name or "AMDFramebuffer" in name
        ):
            return name[:128]
    return None


def audit(tree):
    roots = tree if isinstance(tree, list) else [tree]
    devices = []
    for root in roots:
        if not isinstance(root, dict):
            continue
        for node in [root, *descendants(root)]:
            if is_mendocino(node):
                observed = sorted(set(filter(None, (candidate_class(item) for item in descendants(node)))))
                own_providers = [item for item in descendants(node)
                                 if item.get("IOClass") == "NRedMendocinoProvider"]
                own_provider_status = any(
                    item.get("NRed,Stage3C-Status") == "OWN_IOKIT_PROVIDER_ATTACHED_ONLY"
                    for item in own_providers)
                devices.append({
                    "pci": "1002:1506",
                    "stage3c_own_provider_child": own_provider_status,
                    "stage3c_own_provider_only": bool(own_providers),
                    "x6000_child_services": observed,
                    "status": "NATIVE_SERVICE_CHILD_OBSERVED_ACCELERATION_UNVERIFIED" if observed
                              else "NO_NATIVE_X6000_CHILD_SERVICE_OBSERVED",
                })
    return {
        "device_count": len(devices),
        "devices": devices,
        "gc1037_initialized": "NOT_VERIFIED",
        "dcn316_link_trained": "NOT_VERIFIED",
        "metal": "NOT_VERIFIED",
        "note": "An IOService descendant is evidence of attachment only, not GPU operation.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--input", type=Path, help="Previously saved ioreg -a plist (do not upload raw)")
    source.add_argument("--live", action="store_true", help="Collect on macOS, summarize locally")
    args = parser.parse_args()
    if args.live:
        if platform.system() != "Darwin":
            parser.error("--live is only available on macOS")
        try:
            raw = subprocess.run(["ioreg", "-a", "-l", "-w0", "-p", "IOService"],
                                 check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 timeout=30).stdout
        except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
            parser.error(f"Could not read IORegistry: {exc}")
    else:
        raw = args.input.read_bytes()
    try:
        report = audit(plistlib.loads(raw))
    except (plistlib.InvalidFileException, TypeError, ValueError) as exc:
        parser.error(f"Invalid IORegistry plist: {exc}")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
