"""Stage3E tests: only real Mach-O metadata counts; no native claims."""
import importlib.util
import pathlib
import plistlib
import struct
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def load_script(filename, module_name):
    spec = importlib.util.spec_from_file_location(module_name, ROOT / 'Tools' / filename)
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


ABI = load_script('audit-stage3e-x6000-abi.py', 'stage3e_x6000_abi')
BINDING = load_script('r6-native-binding-report.py', 'stage3e_native_binding')


def make_macho(names, include_symtab=True):
    strings = b'\0'
    symbols = b''
    for name in names:
        index = len(strings)
        strings += name.encode() + b'\0'
        symbols += struct.pack('<IBBHQ', index, 0x0f, 1, 0, 0x1000)
    commands = struct.pack('<IIIIII', 2, 24, 56, len(names), 56 + len(symbols), len(strings)) if include_symtab else b''
    header = struct.pack('<IiiIIIII', 0xFEEDFACF, 0x01000007, 3,
                         0xb, 1 if include_symtab else 0, len(commands), 0, 0)
    return header + commands + symbols + strings


class Stage3EX6000ABI(unittest.TestCase):
    def test_real_symbol_table_reports_class_but_not_binding(self):
        binary = make_macho(['__ZN30AMDRadeonX6000_AmdGpuWrangler5startEv',
                             '__ZN46AMDRadeonX6000_AmdRadeonControllerNavi105startEv'])
        result = ABI.inspect_macho(binary)
        self.assertEqual(result['symbol_table_state'], 'PRESENT')
        self.assertEqual(result['symbol_count'], 2)
        self.assertTrue(result['observed_class_symbols'][ABI.CLASSES[0]])
        self.assertTrue(result['observed_class_symbols'][ABI.CLASSES[1]])

    def test_embedded_plain_strings_are_not_symbols(self):
        binary = make_macho([], include_symtab=False) + b'AMDRadeonX6000_AmdGpuWrangler'
        result = ABI.inspect_macho(binary)
        self.assertEqual(result['symbol_table_state'], 'NO_LC_SYMTAB')
        self.assertFalse(any(result['observed_class_symbols'].values()))

    def test_truncated_or_forged_tables_fail_closed(self):
        binary = bytearray(make_macho(['__ZN30AMDRadeonX6000_AmdGpuWrangler5startEv']))
        struct.pack_into('<I', binary, 40, 0xffffff00)  # symoff
        with self.assertRaises(ABI.MachOError):
            ABI.inspect_macho(bytes(binary))
        with self.assertRaises(ABI.MachOError):
            ABI.inspect_macho(b'not mach-o')
        with self.assertRaises(ABI.MachOError):
            ABI.inspect_macho(bytes(binary[:49]))

    def test_fat_macho_selects_x86_64(self):
        thin = make_macho(['__ZN30AMDRadeonX6000_AmdGpuWrangler5startEv'])
        fat = struct.pack('>II', 0xcafebabe, 1) + struct.pack('>iiIII',
            0x01000007, 3, 32, len(thin), 0) + b'\0' * 4 + thin
        result = ABI.inspect_macho(fat)
        self.assertTrue(result['observed_class_symbols'][ABI.CLASSES[0]])

    def test_kext_directory_missing_binary_is_unknown_not_unsupported(self):
        with tempfile.TemporaryDirectory() as tmp:
            kext = pathlib.Path(tmp) / 'Native.kext'
            (kext / 'Contents').mkdir(parents=True)
            (kext / 'Contents' / 'Info.plist').write_bytes(plistlib.dumps({
                'CFBundleExecutable': 'AMDRadeonX6000',
                'CFBundleIdentifier': 'com.apple.kext.AMDRadeonX6000',
                'CFBundleVersion': '1.0',
            }))
            result = ABI.inspect_path(kext)
            self.assertEqual(result['status'], 'BINARY_NOT_PRESENT_OR_EMBEDDED_IN_KERNEL_COLLECTION')
            self.assertFalse(result['native_binding_verified'])

    def test_binary_evidence_does_not_override_missing_pci_match(self):
        abi_report = {
            'stage': 'R6_STAGE3E_X6000_BINARY_ABI_AUDIT',
            'reports': [{'observed_class_symbols': {
                ABI.CLASSES[1]: ['__ZN46AMDRadeonX6000_AmdRadeonControllerNavi105startEv']}}],
        }
        report = BINDING.summarize([], None, abi_report)
        self.assertEqual(report['native_pci_matching'], 'NO_EXPLICIT_NATIVE_1506_PERSONALITY_FOUND')
        self.assertFalse(report['native_binding_completed'])
        self.assertEqual(report['native_service_attachment'], 'IOREGISTRY_NOT_SUPPLIED')
        self.assertEqual(report['stage3e_apple_binary_abi_state'], 'SYMBOL_EVIDENCE_ONLY_NOT_COMPATIBILITY')

    def test_invalid_stage3e_report_rejected(self):
        with self.assertRaises(ValueError):
            BINDING.summarize([], None, {'stage': 'something_else', 'reports': []})


if __name__ == '__main__':
    unittest.main()
