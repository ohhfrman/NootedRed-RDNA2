from __future__ import annotations
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("r6_binding", ROOT / "Tools/r6-native-binding-report.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def personality(name="Fb", cls="AMDRadeonX6000_AmdGpuWrangler", match="NO_EXPLICIT_1506_MATCH"):
    return {"name": name, "class": cls, "provider": "IOPCIDevice", "status": match}


def report(bundle, *personalities):
    return {"bundle_id": bundle, "personalities": list(personalities)}


def gpu(children):
    return [{"IOClass": "IOPCIDevice", "vendor-id": b"\x02\x10\x00\x00",
             "device-id": b"\x06\x15\x00\x00", "IORegistryEntryChildren": children}]


class NativeBindingReportTests(unittest.TestCase):
    def test_local_resource_does_not_count_as_native(self):
        r = module.summarize([report("LOCAL_PERSONALITY_RESOURCE_NOT_NATIVE_KEXT",
                                     personality(match="EXPLICIT_PCI_ID_MATCH"))])
        self.assertEqual(r["native_personality_count"], 0)
        self.assertFalse(r["native_binding_completed"])
        self.assertEqual(r["native_pci_matching"], "NO_EXPLICIT_NATIVE_1506_PERSONALITY_FOUND")

    def test_actual_native_no_match(self):
        r = module.summarize([report("com.apple.kext.AMDRadeonX6000Framebuffer", personality())])
        self.assertEqual(r["native_personality_count"], 1)
        self.assertEqual(r["native_pci_matching"], "NO_EXPLICIT_NATIVE_1506_PERSONALITY_FOUND")

    def test_navi10_match_is_not_support(self):
        p = personality(cls="AMDRadeonX6000_AmdRadeonControllerNavi10", match="EXPLICIT_PCI_ID_MATCH")
        r = module.summarize([report("com.apple.kext.AMDRadeonX6000Framebuffer", p)], gpu([]))
        self.assertEqual(r["native_pci_matching"], "ONLY_NAVI10_PERSONALITY_MATCHES_UNSUPPORTED_FOR_610M")
        self.assertFalse(r["autoinject_1506_into_navi10"])
        self.assertFalse(r["native_binding_completed"])

    def test_matching_rule_is_only_a_candidate(self):
        p = personality(match="EXPLICIT_PCI_ID_MATCH")
        r = module.summarize([report("com.apple.kext.AMDRadeonX6000Framebuffer", p)], gpu([]))
        self.assertEqual(r["native_pci_matching"], "NATIVE_PCI_RULE_CANDIDATE_NOT_DRIVER_COMPATIBILITY")
        self.assertEqual(r["native_service_attachment"], "NO_X6000_IOSERVICE_DESCENDANT_OBSERVED")
        self.assertEqual(r["metal_acceleration"], "NOT_VERIFIED")

    def test_non_native_bundle_not_counted(self):
        p = personality(match="EXPLICIT_PCI_ID_MATCH")
        r = module.summarize([report("com.apple.kext.AMDRadeonX5000", p)])
        self.assertEqual(r["native_personality_count"], 0)

    def test_ambiguous_native_rules_stay_unknown(self):
        p = personality(match="NO_EXPLICIT_PCI_ID_RULE_INDETERMINATE")
        r = module.summarize([report("com.apple.kext.AMDRadeonX6000", p)])
        self.assertEqual(r["native_pci_matching"], "UNKNOWN_NATIVE_PCI_MATCH_RULES_REQUIRE_REVIEW")

    def test_observed_child_not_metal(self):
        child = {"IOClass": "AMDRadeonX6000_AmdGpuWrangler"}
        r = module.summarize([], gpu([child]))
        self.assertEqual(r["native_service_attachment"], "X6000_IOSERVICE_DESCENDANT_OBSERVED_ONLY")
        self.assertFalse(r["native_binding_completed"])
        self.assertEqual(r["metal_acceleration"], "NOT_VERIFIED")

    def test_wrong_gpu_no_attachment(self):
        r = module.summarize([], [{"vendor-id": b"\x02\x10", "device-id": b"\x05\x15"}])
        self.assertEqual(r["native_service_attachment"], "610M_NOT_FOUND_IN_IOREGISTRY")

    def test_multiple_gpu_indeterminate(self):
        r = module.summarize([], gpu([]) + gpu([]))
        self.assertEqual(r["native_service_attachment"],
                         "MULTIPLE_610M_DEVICES_REQUIRES_MANUAL_VALIDATION")


if __name__ == "__main__":
    unittest.main()
