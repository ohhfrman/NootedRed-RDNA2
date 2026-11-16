#!/usr/bin/env python3
"""R6 native X6000 gate wiring. Source checks are not macOS hardware validation."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def src(name): return (ROOT / name).read_text()
probe = src('NootedRed/MendocinoProbe.cpp')
compat = src('NootedRed/MendocinoX6000Compatibility.hpp')
nred = src('NootedRed/NRed.cpp')
checks = {
    'native gate compiled into KEXT probe': '#include <MendocinoX6000Compatibility.hpp>' in probe,
    'native gate published in IORegistry': 'NRed,X6000-R6-Gate' in probe,
    'native kext arrivals update gate': 'updateStageProperties();' in probe.split('void MendocinoProbe::noteX6000Component')[1],
    'GC version must be observed': 'if (!state.has(GC1037Identified))' in compat,
    'DCN version must be observed': 'if (!state.has(DCN316Identified))' in compat,
    'validated IP input required': 'if (!state.has(IPDiscoveryValidated))' in compat,
    'native binding not falsely claimed': 'driverBindingVerified' in compat and 'return false;' in compat,
    'no Vega patch path for Mendocino': 'if (singleton().isMendocino()) { return; }' in nred,
    'native driver not spoofed': 'IOPCIMatch' not in probe and 'setProperty("device-id"' not in probe,
    'native gate not marked as initialized': 'state.observe(MendocinoBringup::GC1037Initialized)' not in probe,
}
for k, passed in checks.items(): print(('PASS' if passed else 'FAIL') + ': ' + k)
assert all(checks.values())
print(f'R6 native compatibility integration checks PASS: {len(checks)}')
