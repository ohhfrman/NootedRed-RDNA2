#!/usr/bin/env python3
"""Verify built KEXT contains Stage3C opt-in IOKit matching personality.
Safe offline check. This is NOT proof of macOS binding, X6000, or Metal.
"""
import argparse
import pathlib
import plistlib
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('kext', type=pathlib.Path,
                   help='Path to NootedRed.kext built by LLVM/AppleForge')
    args = p.parse_args()
    info_path = args.kext / 'Contents/Info.plist'
    bin_path = args.kext / 'Contents/MacOS/NootedRed'
    if not info_path.is_file() or not bin_path.is_file():
        p.error('Missing built Contents/Info.plist or Contents/MacOS/NootedRed')
    info = plistlib.loads(info_path.read_bytes())
    entry = info.get('IOKitPersonalities', {}).get(
        'org.ChefKiss.NootedRed.MendocinoProviderStage3C', {})
    required = {
        'IOClass': 'NRedMendocinoProvider',
        'IOProviderClass': 'IOPCIDevice',
        'IOMatchCategory': 'NRedStage3CProvider',
        'IOPCIMatch': '0x15061002',
        'IOPropertyMatch': {'NRed,EnableStage3CProvider': True},
    }
    errors = [f'{k} != {v!r}' for k, v in required.items() if entry.get(k) != v]
    if 'NRedMendocinoProvider' not in bin_path.read_bytes().decode('latin-1'):
        errors.append('NRedMendocinoProvider missing in Mach-O string table')
    if errors:
        print('Stage3C bundle verification FAILED:')
        for error in errors:
            print(' -', error)
        return 1
    print('PASS: Stage3C own PCI IOService + opt-in personality in built KEXT')
    print('NOTE: Actual IOService match and Apple X6000 acceleration NOT verified')
    return 0


if __name__ == '__main__':
    sys.exit(main())
