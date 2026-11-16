#!/usr/bin/env python3
"""Read-only Mendocino 0x1506 PCI & AMDGPU IP discovery inventory from Linux sysfs.

Example: python3 Tools/collect-mendocino-linux.py mendocino-report.json
This does not read memory or upload any data. It deliberately avoids serial numbers.
"""
import json
import platform
import sys
from pathlib import Path

FIELDS = ('hw_id', 'major', 'minor', 'revision', 'harvest', 'num_base_addresses', 'num_instance', 'base_addr')
PCI_FIELDS = ('vendor', 'device', 'revision', 'class', 'subsystem_vendor', 'subsystem_device')


def read(path):
    try:
        return path.read_text(encoding='ascii').strip()
    except (OSError, UnicodeError):
        return None


def collect(root=Path('/sys/bus/pci/devices')):
    found = []
    if root.is_dir():
        for pci in sorted(root.iterdir()):
            if read(pci / 'vendor') != '0x1002' or read(pci / 'device') != '0x1506':
                continue
            gpu = {'pci_address': pci.name, 'pci': {k: read(pci / k) for k in PCI_FIELDS},
                   'ip_discovery': [], 'driver': None}
            try:
                driver = pci / 'driver'
                if driver.is_symlink():
                    gpu['driver'] = driver.resolve().name
            except OSError:
                pass
            # Linux exposes each AMD IP block as ip_discovery/die/DIE/HWID/INSTANCE.
            base = pci / 'ip_discovery' / 'die'
            if base.is_dir():
                for die in sorted(base.iterdir()):
                    if not die.is_dir():
                        continue
                    for hwid in sorted(die.iterdir()):
                        if not hwid.is_dir():
                            continue
                        for inst in sorted(hwid.iterdir()):
                            if not inst.is_dir():
                                continue
                            info = {'die': die.name, 'hwid_path': hwid.name, 'instance': inst.name}
                            info.update({key: read(inst / key) for key in FIELDS})
                            gpu['ip_discovery'].append(info)
            found.append(gpu)
    return {'report_format': 'NRed-RDNA2-Mendocino-R2', 'kernel': platform.release(),
            'host_os': platform.system(), 'gpu': found, 'read_only': True}


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if len(argv) > 1:
        print('usage: collect-mendocino-linux.py [output.json]', file=sys.stderr)
        return 2
    destination = Path(argv[0]) if argv else Path('mendocino-ip-discovery.json')
    report = collect()
    destination.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(f"Saved {destination} ({len(report['gpu'])} matching PCI device(s))")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
