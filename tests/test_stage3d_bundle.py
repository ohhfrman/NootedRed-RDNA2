"""Exercise Stage3D built KEXT verifier with synthetic files (no macOS required)."""
import importlib.util
import pathlib
import plistlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'stage3d_verifier', ROOT / 'Tools/verify-stage3d-bundle.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)

class Stage3DBundleTests(unittest.TestCase):
    def test_missing_and_fake_binary_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            kext = pathlib.Path(td)
            self.assertTrue(MODULE.verify(kext))
            contents = kext / 'Contents'
            (contents / 'MacOS').mkdir(parents=True)
            plist = {
                'IOKitPersonalities': {
                    'org.ChefKiss.NootedRed.MendocinoProviderStage3C': {
                        'IOClass': 'NRedMendocinoProvider',
                        'IOProviderClass': 'IOPCIDevice',
                        'IOMatchCategory': 'NRedStage3CProvider',
                        'IOPCIMatch': '0x15061002',
                        'IOPropertyMatch': {'NRed,EnableStage3CProvider': True},
                    },
                },
            }
            (contents / 'Info.plist').write_bytes(plistlib.dumps(plist))
            exe = contents / 'MacOS/NootedRed'
            exe.write_bytes(b' '.join(MODULE.MARKERS))
            self.assertIn('Binary is not a thin 64-bit Mach-O MH_KEXT_BUNDLE', MODULE.verify(kext))
            header = bytearray(32)
            header[:4] = (0xFEEDFACF).to_bytes(4, 'little')
            header[12:16] = (0xB).to_bytes(4, 'little')
            exe.write_bytes(bytes(header) + b' '.join(MODULE.MARKERS))
            self.assertEqual(MODULE.verify(kext), [])
            exe.write_bytes(bytes(header) + b' '.join(MODULE.MARKERS[:-1]))
            self.assertTrue(any('Stage3D-SnapshotOnly' in e for e in MODULE.verify(kext)))

if __name__ == '__main__':
    unittest.main()
