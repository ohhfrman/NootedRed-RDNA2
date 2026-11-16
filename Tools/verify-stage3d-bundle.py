#!/usr/bin/env python3
"""Verify that the Stage3D read-only input contract is in the *built* KEXT.
This does not assert matching, actual GPU initialization, or Metal support.
"""
import argparse
import pathlib
import plistlib
import sys

MARKERS = (
    b'NRed,Stage3D-InputStatus',
    b'CONTROLLER_INPUTS_VALIDATED_NO_NATIVE_INIT',
    b'NRed,Stage3D-NativeControllerInitialized',
    b'NRed,Stage3D-SnapshotOnly',
)

def verify(kext: pathlib.Path) -> list[str]:
    info_path = kext / 'Contents/Info.plist'
    binary_path = kext / 'Contents/MacOS/NootedRed'
    if not info_path.is_file() or not binary_path.is_file():
        return ['No built Info.plist or Mach-O executable at supplied path']
    info = plistlib.loads(info_path.read_bytes())
    entry = info.get('IOKitPersonalities', {}).get(
        'org.ChefKiss.NootedRed.MendocinoProviderStage3C', {})
    errors = []
    for key, value in {
        'IOClass': 'NRedMendocinoProvider',
        'IOProviderClass': 'IOPCIDevice',
        'IOMatchCategory': 'NRedStage3CProvider',
        'IOPCIMatch': '0x15061002',
        'IOPropertyMatch': {'NRed,EnableStage3CProvider': True},
    }.items():
        if entry.get(key) != value:
            errors.append(f'{key}: expected {value!r}, got {entry.get(key)!r}')
    blob = binary_path.read_bytes()
    # AppleForge produces a thin x86_64 64-bit Mach-O KEXT bundle (MH_KEXT_BUNDLE=0xB).
    # Do not accept arbitrary files that merely contain our marker strings.
    if (len(blob) < 32 or int.from_bytes(blob[:4], 'little') != 0xFEEDFACF
            or int.from_bytes(blob[12:16], 'little') != 0xB):
        errors.append('Binary is not a thin 64-bit Mach-O MH_KEXT_BUNDLE')
    for marker in MARKERS:
        if marker not in blob:
            errors.append(f'Compiled binary missing {marker!r}')
    return errors

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kext', type=pathlib.Path)
    args = parser.parse_args()
    errors = verify(args.kext)
    for item in errors:
        print('FAIL:', item)
    if errors:
        return 1
    print('PASS: Stage3D exact PCI personality and read-only controller input contract in built KEXT')
    print('NOTE: Apple X6000 binding, GC/DCN initialization, eDP link and Metal are NOT verified')
    return 0

if __name__ == '__main__':
    sys.exit(main())
