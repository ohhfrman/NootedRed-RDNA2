import tempfile
import plistlib
import unittest
from pathlib import Path
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('x6000_audit', ROOT / 'Tools/audit-x6000-personalities.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

class X6000AuditTests(unittest.TestCase):
    def test_exact(self):
        self.assertTrue(audit.match_token('0x15061002'))
        self.assertFalse(audit.match_token('0x73BF1002'))

    def test_masked(self):
        self.assertTrue(audit.match_token('0x15001002&0xFFF0FFFF'))
        self.assertFalse(audit.match_token('0x15001002&0xFFFFFFFF'))
        self.assertIsNone(audit.match_token('garbage'))

    def test_mask_with_whitespace(self):
        p = audit.evaluate_personality('masked', {'IOPCIMatch': '0x15001002 & 0xfff0ffff'})
        self.assertEqual(p['status'], 'EXPLICIT_PCI_ID_MATCH')

    def test_local_personality_resources_have_no_1506(self):
        res = audit.load_kext(ROOT / 'NootedRed/Personalities/com.apple.kext.AMDRadeonX6000Framebuffer.xml')
        self.assertTrue(res['personalities'])
        self.assertEqual(res['bundle_id'], 'LOCAL_PERSONALITY_RESOURCE_NOT_NATIVE_KEXT')
        self.assertFalse(any(x['status'] == 'EXPLICIT_PCI_ID_MATCH' for x in res['personalities']))

    def test_unknown_must_not_count_as_match(self):
        self.assertEqual(audit.evaluate_personality('unknown', {'IOPCIMatch': 'not-hex'})['status'],
                         'PCI_MATCH_RULE_INDETERMINATE')
        self.assertEqual(audit.evaluate_personality('class-only', {'IOPCIClassMatch': '0x03000000&0xff000000'})['status'],
                         'NO_EXPLICIT_PCI_ID_RULE_INDETERMINATE')

    def test_real_plist_parser(self):
        with tempfile.TemporaryDirectory() as root:
            kext = Path(root) / 'Example.kext'
            (kext / 'Contents').mkdir(parents=True)
            with (kext / 'Contents' / 'Info.plist').open('wb') as f:
                plistlib.dump({'CFBundleIdentifier': 'com.example.test',
                               'IOKitPersonalities': {'device': {'IOPCIMatch': '0x15061002'}}}, f)
            res = audit.load_kext(kext)
            self.assertEqual(res['bundle_id'], 'com.example.test')
            self.assertEqual(res['personalities'][0]['status'], 'EXPLICIT_PCI_ID_MATCH')

if __name__ == '__main__':
    unittest.main()
