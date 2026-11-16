import importlib.util
from pathlib import Path
import unittest

TOOL = Path(__file__).resolve().parents[1] / 'Tools' / 'analyze-mendocino-atom-r6.py'
spec = importlib.util.spec_from_file_location('atom_r6_audit', TOOL)
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class AtomR6Tests(unittest.TestCase):
    def make_vbios(self):
        b = bytearray(1024)
        def w16(off,value): b[off:off+2] = value.to_bytes(2,'little')
        b[0:2]=b'\x55\xaa';w16(0x48,0x100)
        w16(0x100,0x2c);b[0x104:0x108]=b'ATOM';w16(0x120,0x200)
        w16(0x200,74);b[0x202:0x204]=b'\x02\x01'
        w16(0x200+4+22*2,0x300);w16(0x200+4+27*2,0x380)
        w16(0x300,24);b[0x302:0x304]=b'\x01\x04';w16(0x304,2);b[0x306]=1
        w16(0x308,0x3114);w16(0x30c,0x211e);w16(0x314,0x0002)
        w16(0x380,48);b[0x382:0x384]=b'\x04\x04';b[0x380+42]=4
        return b

    def test_extract_paths(self):
        out=audit.parse_atom(self.make_vbios())
        self.assertEqual(out['display']['path_count'],1)
        self.assertTrue(out['display']['paths'][0]['lcd1'])
        self.assertEqual(out['dce44']['max_display_pipes'],4)

    def test_display_path_overrun(self):
        b=self.make_vbios();b[0x306]=9
        with self.assertRaises(audit.ParseError): audit.parse_atom(b)

    def test_invalid_master_data(self):
        b=self.make_vbios();b[0x202]=1
        with self.assertRaises(audit.ParseError): audit.parse_atom(b)

    def test_register_aperture_non_dereference(self):
        resource='\n'.join(['0x0 0x0 0x0']*5+['0xD0500000 0xD057FFFF 0x0'])
        bar=audit.parse_bar5(resource)
        self.assertEqual(bar['size_bytes'],524288)
        records={'gpu':[{'ip_discovery':[{'hw_id':'11','instance':'0','major':'10','minor':'3','revision':'7',
            'base_addr':'0x00001260\n0x02402C00','hwid_path':'11'}]*2}]}
        out=audit.parse_ip(records,bar)
        self.assertEqual(out['unique_records'],1)
        self.assertTrue(out['selected_blocks'][0]['bases'][0]['within_bar5_if_direct'])
        self.assertFalse(out['selected_blocks'][0]['bases'][1]['within_bar5_if_direct'])


if __name__=='__main__': unittest.main()
