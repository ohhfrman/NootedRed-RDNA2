"""Static guardrails for Stage3D: controller inputs, not controller initialization."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class Stage3DProviderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / 'NootedRed/NRedMendocinoProvider.cpp').read_text()
        cls.contract = (ROOT / 'NootedRed/MendocinoControllerInputs.hpp').read_text()
        cls.collector = (ROOT / 'NootedRed/MendocinoControllerSnapshot.cpp').read_text()
        cls.probe = (ROOT / 'NootedRed/MendocinoProbe.cpp').read_text()
        cls.lilu = (ROOT / 'NootedRed/NRed.cpp').read_text()

    def test_board_inputs_are_read_not_assumed(self):
        for key in ('ATY,bin_image', 'NRed,AMD-IPDiscovery', 'NRed,PanelEDID',
                    'IODisplayEDID', 'MendocinoIP::decodeInto', 'MendocinoVbios::isAtomBios'):
            self.assertIn(key, self.collector)
        for version in ('kGC, 10, 3, 7', 'kDMU, 3, 1, 6',
                        'kSDMA0, 5, 2, 7', 'kMP1, 13, 0, 8'):
            self.assertIn(version, self.contract)

    def test_own_provider_does_not_program_gpu_or_spoof_apple(self):
        for forbidden in ('mapDeviceMemory', 'setBusMasterEnable', 'setMemoryEnable',
                          'configWrite', 'IOCatalogue::', 'IOFramebuffer', 'AMDRadeonX6000_',
                          'writeReg', 'LoadController'):
            self.assertNotIn(forbidden, self.source)
        self.assertIn('if (!matches(provider) || !IOService::start(provider))', self.source)
        self.assertIn('NRed,Stage3D-InputStatus', self.source)
        self.assertIn('MendocinoControllerSnapshot::collect(gpu)', self.source)
        self.assertIn('MendocinoControllerSnapshot::collect(gpu)', self.probe)

    def test_output_never_asserts_initialization(self):
        self.assertIn('constexpr bool nativeControllerInitialized(Status) { return false; }', self.contract)
        self.assertIn('constexpr bool metalAccelerationVerified(Status) { return false; }', self.contract)
        self.assertIn('NRed,Stage3D-NativeControllerInitialized', self.source)
        self.assertIn('NRed,Stage3D-SnapshotOnly', self.source)
        self.assertIn('NRed,Stage3D-NativeControllerInitialized', self.probe)

    def test_legacy_vega_guard_remains_intact(self):
        self.assertIn('if (singleton().isMendocino()) { return; }', self.lilu)
        self.assertIn('if (this->isMendocino()) { return; }', self.lilu)

if __name__ == '__main__':
    unittest.main()
