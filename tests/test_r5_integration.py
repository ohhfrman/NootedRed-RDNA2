#!/usr/bin/env python3
"""R5 source wiring invariants; host-only check (not a KEXT runtime test)."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
def src(path): return (root / path).read_text()
nred = src('NootedRed/NRed.cpp')
h = src('NootedRed/NRed.hpp')
probe = src('NootedRed/MendocinoProbe.cpp')
kexts = src('NootedRed/Kexts.cpp')
backlight = src('NootedRed/Backlight.cpp')
checks = {
    'PCI classify via hardware profile': 'NRedHardware::classify(vendor, device)' in nred,
    'Mendocino early return ahead of switch off': nred.index('MendocinoProbe::publish(') < nred.index('devInfo->processSwitchOff()'),
    'Vega patcher excluded': 'if (singleton().isMendocino()) { return; }' in nred,
    'Vega MMIO excluded': 'if (this->isMendocino()) { return; }' in nred,
    'Vega PCI config MMIO guards': 'if (this->isMendocino()) { return 0; }' in nred,
    'architecture state': 'NRedHardware::Architecture architecture' in h,
    'X6000 accelerator observed': 'kextRadeonX6000.loadIndex' in nred and 'kextRadeonX6000 {' in kexts,
    'X6000 HWLibs observed': 'kextRadeonX6000HWLibs.loadIndex' in nred and 'kextRadeonX6000HWLibs {' in kexts,
    'backlight callback guards': backlight.count('NRed::singleton().isMendocino()') >= 2,
    'probe uses VFCT validator': 'MendocinoVbios::findVbios(' in probe,
    'probe uses IP discovery validator': 'MendocinoIP::decodeInto(' in probe,
    'probe uses EDID validator': 'MendocinoEDID::decode(' in probe,
    'probe tracks native kext arrivals': 'loadedComponentBits' in probe,
    'probe never states acceleration initialized': 'GC1037Initialized' in probe and 'state.observe(MendocinoBringup::GC1037Initialized)' not in probe,
    'build includes diagnostic probe': 'add_files("NootedRed/**.cpp")' in src('xmake.lua'),
    'preserved TSNPL license': 'Thou Shalt Not Profit License' in src('LICENSE'),
}
for k,v in checks.items(): print(('PASS' if v else 'FAIL') + ': ' + k)
if not all(checks.values()): sys.exit(1)
print(f'R5 integration checks PASS: {len(checks)}')
