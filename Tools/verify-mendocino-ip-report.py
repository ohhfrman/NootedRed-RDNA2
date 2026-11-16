#!/usr/bin/env python3
"""Check a local R2/R3 Linux sysfs capture against Mendocino's AMD IP blocks.

Read-only. Reports version mismatches and register bases; does not touch GPU registers.
Usage: python3 Tools/verify-mendocino-ip-report.py mendocino-ip-discovery.json
"""
import json
import sys
from pathlib import Path

EXPECTED = {
    'GC': (11, (10, 3, 7)),
    'DCN_DMU': (271, (3, 1, 6)),
    'SDMA0': (42, (5, 2, 7)),
    'VCN': (12, (3, 1, 1)),
    'MP1': (1, (13, 0, 8)),
    'MP0': (255, (13, 0, 8)),
}


def as_int(value):
    if value is None:
        return None
    try:
        return int(str(value).strip(), 0)
    except (ValueError, TypeError):
        return None


def inspect(doc):
    result = []
    for gpu in doc.get('gpu', []):
        pci = gpu.get('pci', {})
        if as_int(pci.get('vendor')) != 0x1002 or as_int(pci.get('device')) != 0x1506:
            continue
        blocks = {}
        for rec in gpu.get('ip_discovery', []):
            hwid = as_int(rec.get('hw_id'))
            if hwid is None:
                hwid = as_int(rec.get('hwid_path'))
            if hwid is None:
                continue
            vals = tuple(as_int(rec.get(key)) for key in ('major', 'minor', 'revision'))
            if None not in vals:
                blocks.setdefault(hwid, []).append({'version': vals,
                   'instance': rec.get('instance'), 'base_addr': rec.get('base_addr'),
                   'harvest': rec.get('harvest')})
        matches = {}
        for label, (hwid, expected) in EXPECTED.items():
            observed = blocks.get(hwid, [])
            matches[label] = {'expected': '.'.join(map(str, expected)),
                              'observed': observed,
                              'version_match': any(b['version'] == expected for b in observed)}
        result.append({'pci_address': gpu.get('pci_address'), 'identity': '1002:1506',
                      'blocks': matches, 'gc_dcn_versions_match':
                      matches['GC']['version_match'] and matches['DCN_DMU']['version_match'],
                      'meaning': 'Linux-reported IP profile only; not macOS framebuffer/Metal validation'})
    return {'status': 'MENDOCINO_MATCHED' if result else 'NO_RADEON_610M_MATCH',
            'devices': result, 'read_only': True}


def main(argv=None):
    args = sys.argv[1:] if argv is None else list(argv)
    if len(args) != 1:
        print('Usage: verify-mendocino-ip-report.py capture.json', file=sys.stderr)
        return 2
    doc = json.loads(Path(args[0]).read_text(encoding='utf-8'))
    summary = inspect(doc)
    print(json.dumps(summary, indent=2, ensure_ascii=False, default=list))
    return 0 if summary['devices'] and all(x['gc_dcn_versions_match'] for x in summary['devices']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
