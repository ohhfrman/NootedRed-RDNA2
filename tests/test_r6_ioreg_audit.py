import importlib.util
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('mendocino_ioreg', ROOT/'Tools/audit-mendocino-ioreg.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class IoregTests(unittest.TestCase):
    def gpu(self, child):
        return {'IOClass': 'IOPCIDevice', 'vendor-id': b'\x02\x10\x00\x00',
                'device-id': b'\x06\x15\x00\x00', 'IORegistryEntryChildren': [child]}

    def test_no_gpu(self):
        r = mod.audit([{'IOClass': 'IOPCIDevice', 'vendor-id': b'\x02\x10',
                        'device-id': b'\x05\x15'}])
        self.assertEqual(r['device_count'], 0)

    def test_gpu_no_binding(self):
        r = mod.audit([self.gpu({'IOClass': 'GenericDisplay'})])
        self.assertEqual(r['devices'][0]['x6000_child_services'], [])
        self.assertEqual(r['metal'], 'NOT_VERIFIED')

    def test_gpu_attached_service_but_not_metal(self):
        r = mod.audit([self.gpu({'IOClass': 'AMDRadeonX6000_AmdGpuWrangler'})])
        self.assertEqual(r['devices'][0]['x6000_child_services'], ['AMDRadeonX6000_AmdGpuWrangler'])
        self.assertEqual(r['metal'], 'NOT_VERIFIED')
        self.assertEqual(r['gc1037_initialized'], 'NOT_VERIFIED')

    def test_non_pci_metadata(self):
        self.assertIsNone(mod.pci_number('0x1506'))
        self.assertFalse(mod.is_mendocino({'vendor-id': 0x1002, 'device-id': 0x1507}))


if __name__ == '__main__':
    unittest.main()
