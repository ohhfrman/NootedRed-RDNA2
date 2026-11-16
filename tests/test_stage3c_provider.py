"""Validate the nonexclusive opt-in IOKit provider personality and no spoofing.
Does not prove the kernel class compiled or macOS matched it at runtime.
"""
import pathlib
import plistlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class Stage3CProviderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.info = plistlib.loads((ROOT / 'NootedRed/Info.plist').read_bytes())
        cls.table = cls.info['IOKitPersonalities']
        cls.p = cls.table['org.ChefKiss.NootedRed.MendocinoProviderStage3C']
        cls.source = (ROOT / 'NootedRed/NRedMendocinoProvider.cpp').read_text()

    def test_exact_device_and_separate_category(self):
        self.assertEqual(self.p['IOPCIMatch'], '0x15061002')
        self.assertEqual(self.p['IOClass'], 'NRedMendocinoProvider')
        self.assertEqual(self.p['IOProviderClass'], 'IOPCIDevice')
        self.assertEqual(self.p['IOMatchCategory'], 'NRedStage3CProvider')
        self.assertNotIn('ATIFramebuffer', self.p.values())
        self.assertNotIn('IOFramebuffer', self.p.values())

    def test_disabled_until_explicit_opt_in(self):
        self.assertEqual(self.p['IOPropertyMatch'], {'NRed,EnableStage3CProvider': True})
        self.assertIn('consent->isTrue()', self.source)
        self.assertIn('if (!matches(provider)) { return nullptr; }', self.source)
        self.assertIn('if (!matches(provider) || !IOService::start(provider))', self.source)

    def test_no_gpu_access_or_apple_controller_spoof(self):
        for forbidden in ('mapDeviceMemory', 'setBusMasterEnable', 'setMemoryEnable',
                          'configWrite', 'writeReg', 'IOCatalogue::',
                          'device-id', 'LoadController', 'AMDRadeonX6000_AmdRadeonControllerNavi10'):
            self.assertNotIn(forbidden, self.source)
        self.assertNotIn('AMDRadeonX6000_', self.p['IOClass'])
        self.assertIn('OWN_IOKIT_PROVIDER_ATTACHED_ONLY', self.source)

    def test_original_lilu_plugin_personality_preserved(self):
        self.assertIn('org.ChefKiss.NootedRed', self.table)
        self.assertEqual(self.table['org.ChefKiss.NootedRed']['IOProviderClass'], 'IOResources')


if __name__ == '__main__':
    unittest.main()
