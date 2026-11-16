"""Host-only synthetic VFCT tests. No vendor BIOS / proprietary firmware."""
import importlib.util
import pathlib
import struct
import sys
import unittest

mod_path = pathlib.Path(__file__).parents[1] / 'Tools' / 'extract-mendocino-vbios.py'
spec = importlib.util.spec_from_file_location('vfct_extractor', mod_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def mk_bios():
    rom = bytearray(128)
    rom[:2] = b'\x55\xaa'
    struct.pack_into('<H', rom, 0x48, 0x50)
    rom[0x54:0x58] = b'ATOM'
    return bytes(rom)


def mk_vfct(records):
    table = bytearray(0x4c)
    table[:4] = b'VFCT'
    struct.pack_into('<I', table, 0x34, 0x4C)
    for vendor, device, rom in records:
        table.extend(struct.pack('<IIIHHHHII', 0, 1, 0, vendor, device, 0x1043, 0x7777, 1, len(rom)))
        table.extend(rom)
    struct.pack_into('<I', table, 4, len(table))
    table[9] = (-sum(table)) & 255
    return bytes(table)


class VFCTTests(unittest.TestCase):
    def test_selects_mendocino_not_rembrandt(self):
        raw = mk_vfct([(0x1002, 0x1681, mk_bios()), (0x1002, 0x1506, mk_bios())])
        vbios, metadata = module.parse_vfct(raw)
        self.assertEqual(vbios, mk_bios())
        self.assertEqual(metadata['pci_device'], '1506')
        self.assertEqual(metadata['subsystem_vendor'], '1043')

    def test_rejects_mismatched_id(self):
        with self.assertRaisesRegex(module.VFCTError, 'no image'):
            module.parse_vfct(mk_vfct([(0x1002, 0x1681, mk_bios())]))

    def test_rejects_checksum_and_truncation(self):
        raw = bytearray(mk_vfct([(0x1002, 0x1506, mk_bios())]))
        raw[9] ^= 1
        with self.assertRaisesRegex(module.VFCTError, 'checksum'):
            module.parse_vfct(raw)
        raw[9] ^= 1
        with self.assertRaisesRegex(module.VFCTError, 'length'):
            module.parse_vfct(raw[:-10])

    def test_rejects_atom_pointer_overrun(self):
        rom = bytearray(mk_bios())
        struct.pack_into('<H', rom, 0x48, 0xFFFF)
        with self.assertRaisesRegex(module.VFCTError, 'ATOM'):
            module.parse_vfct(mk_vfct([(0x1002, 0x1506, rom)]))

    def test_rejects_zero_length(self):
        with self.assertRaisesRegex(module.VFCTError, 'ATOM'):
            module.parse_vfct(mk_vfct([(0x1002, 0x1506, b'')]))


if __name__ == '__main__':
    unittest.main()
