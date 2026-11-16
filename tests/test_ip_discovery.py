import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

S = Path(__file__).resolve().parents[1] / 'Tools' / 'collect-mendocino-linux.py'
spec = importlib.util.spec_from_file_location('mendocino_collect', S)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class TestIPDiscovery(unittest.TestCase):
    def test_detects_only_mendocino_and_parses_ip(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            supported = root / '0000:03:00.0'
            unsupported = root / '0000:04:00.0'
            for device, did in ((supported, '0x1506'), (unsupported, '0x1636')):
                device.mkdir()
                (device / 'vendor').write_text('0x1002\n')
                (device / 'device').write_text(did + '\n')
            ip = supported / 'ip_discovery/die/0/11/0'
            ip.mkdir(parents=True)
            for k, v in (('hw_id', '11'), ('major', '10'), ('minor', '3'), ('revision', '7'), ('base_addr', '0x3e0')):
                (ip / k).write_text(v)
            report = module.collect(root)
            self.assertEqual(len(report['gpu']), 1)
            self.assertEqual(report['gpu'][0]['pci_address'], '0000:03:00.0')
            self.assertEqual(report['gpu'][0]['ip_discovery'][0]['major'], '10')
            self.assertEqual(report['gpu'][0]['ip_discovery'][0]['revision'], '7')
            self.assertTrue(report['read_only'])
            self.assertNotIn('serial', json.dumps(report))

    def test_absent_sysfs_is_empty(self):
        with tempfile.TemporaryDirectory() as t:
            self.assertEqual(module.collect(Path(t) / 'missing')['gpu'], [])


if __name__ == '__main__':
    unittest.main()
