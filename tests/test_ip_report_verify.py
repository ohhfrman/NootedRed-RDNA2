import unittest
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Tools'))
from importlib import import_module
mod=import_module('verify-mendocino-ip-report')

class TestReport(unittest.TestCase):
    def test_valid_mendocino(self):
        blocks=[('11','10','3','7'),('271','3','1','6'),('42','5','2','7')]
        records=[{'hw_id':a,'major':b,'minor':c,'revision':d} for a,b,c,d in blocks]
        doc={'gpu':[{'pci_address':'0000:02:00.0','pci':{'vendor':'0x1002','device':'0x1506'},'ip_discovery':records}]}
        result=mod.inspect(doc)
        self.assertTrue(result['devices'][0]['gc_dcn_versions_match'])
        doc['gpu'][0]['ip_discovery'][1]['revision']='2'
        self.assertFalse(mod.inspect(doc)['devices'][0]['gc_dcn_versions_match'])
    def test_rejects_other_device(self):
        doc={'gpu':[{'pci':{'vendor':'0x1002','device':'0x1681'},'ip_discovery':[]}]}
        self.assertFalse(mod.inspect(doc)['devices'])
    def test_missing_or_invalid_values(self):
        doc={'gpu':[{'pci':{'vendor':'0x1002','device':'0x1506'},'ip_discovery':[{'hw_id':'11','major':'not a version'}]}]}
        self.assertFalse(mod.inspect(doc)['devices'][0]['gc_dcn_versions_match'])

if __name__ == '__main__':unittest.main()
