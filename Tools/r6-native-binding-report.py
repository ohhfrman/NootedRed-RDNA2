#!/usr/bin/env python3
"""R6 Stage 3B: non-invasive evidence for Mendocino 1002:1506 driver matching.

Compares *real* installed Apple IOService personalities with the 610M PCI ID
and optionally checks IORegistry for X6000 descendants. This does not install
personalities, bypass Apple's matching, program registers, or verify Metal.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import platform
import plistlib
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent


def load_module(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, HERE / filename)
    if spec is None or spec.loader is None:
        raise RuntimeError("Cannot load local auditing module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PLIST = load_module("stage3a_x6000_plist", "audit-x6000-personalities.py")
IOREG = load_module("stage3a_610m_ioreg", "audit-mendocino-ioreg.py")
NATIVE_PREFIX = "com.apple.kext.AMDRadeonX6000"


def summarize(plist_reports: list[dict], ioreg_tree=None, abi_audit=None) -> dict:
    native_personalities = []
    local_resource_personalities = []
    explicit_matches = []
    ambiguous_rules = []

    for report in plist_reports:
        bundle = report.get("bundle_id")
        is_native = isinstance(bundle, str) and bundle.startswith(NATIVE_PREFIX)
        for personality in report.get("personalities", []):
            row = {
                "bundle": bundle,
                "personality": personality.get("name"),
                "class": personality.get("class"),
                "provider": personality.get("provider"),
                "pci_status": personality.get("status"),
            }
            if not is_native:
                local_resource_personalities.append(row)
                continue
            native_personalities.append(row)
            if personality.get("status") == "EXPLICIT_PCI_ID_MATCH":
                explicit_matches.append(row)
            elif personality.get("status") in (
                "NO_EXPLICIT_PCI_ID_RULE_INDETERMINATE",
                "PCI_MATCH_RULE_INDETERMINATE",
            ):
                ambiguous_rules.append(row)

    # PCI matching alone is insufficient: IOPropertyMatch/IOPCIClassMatch,
    # macOS version, controller code, firmware and GC/DCN may still reject it.
    unvalidated_navi10 = [row for row in explicit_matches
                          if isinstance(row["class"], str) and "Navi10" in row["class"]]
    other_matches = [row for row in explicit_matches if row not in unvalidated_navi10]
    if unvalidated_navi10 and not other_matches:
        match_result = "ONLY_NAVI10_PERSONALITY_MATCHES_UNSUPPORTED_FOR_610M"
    elif explicit_matches:
        match_result = "NATIVE_PCI_RULE_CANDIDATE_NOT_DRIVER_COMPATIBILITY"
    elif ambiguous_rules:
        match_result = "UNKNOWN_NATIVE_PCI_MATCH_RULES_REQUIRE_REVIEW"
    else:
        match_result = "NO_EXPLICIT_NATIVE_1506_PERSONALITY_FOUND"

    if ioreg_tree is None:
        attachment = "IOREGISTRY_NOT_SUPPLIED"
        device_count = None
        children = []
    else:
        report = IOREG.audit(ioreg_tree)
        device_count = report["device_count"]
        children = sorted({name for device in report["devices"]
                           for name in device["x6000_child_services"]})
        if device_count == 0:
            attachment = "610M_NOT_FOUND_IN_IOREGISTRY"
        elif device_count != 1:
            attachment = "MULTIPLE_610M_DEVICES_REQUIRES_MANUAL_VALIDATION"
        elif children:
            attachment = "X6000_IOSERVICE_DESCENDANT_OBSERVED_ONLY"
        else:
            attachment = "NO_X6000_IOSERVICE_DESCENDANT_OBSERVED"

    # An actual binary's exported symbols are useful evidence for selecting
    # version-specific controller interface work. They cannot override an
    # absent PCI personality or unobserved IOService attachment.
    if abi_audit is None:
        abi_state = "NOT_PROVIDED"
        abi_classes = []
    elif not isinstance(abi_audit, dict) or abi_audit.get("stage") != "R6_STAGE3E_X6000_BINARY_ABI_AUDIT":
        raise ValueError("Not a Stage3E binary ABI audit")
    else:
        reports = abi_audit.get("reports")
        if not isinstance(reports, list):
            raise ValueError("Invalid Stage3E report array")
        symbol_maps = [report.get("observed_class_symbols", {}) for report in reports
                       if isinstance(report, dict)]
        if not all(isinstance(m, dict) for m in symbol_maps):
            raise ValueError("Invalid Stage3E symbol map")
        abi_classes = sorted({cls for sym_map in symbol_maps
                              for cls, names in sym_map.items()
                              if isinstance(cls, str) and isinstance(names, list) and names})
        abi_state = "SYMBOL_EVIDENCE_ONLY_NOT_COMPATIBILITY" if abi_classes else "NO_RELIABLE_CLASS_SYMBOL_EVIDENCE"

    return {
        "pci_target": "1002:1506",
        "native_personality_count": len(native_personalities),
        "native_pci_matching": match_result,
        "explicit_matches": explicit_matches,
        "unresolved_native_rules": ambiguous_rules,
        "local_resources_not_native_personality_count": len(local_resource_personalities),
        "local_resource_notice": "NootedRed XML resources are NOT installed Apple KEXT Info.plists",
        "io_registry_device_count": device_count,
        "native_service_attachment": attachment,
        "observed_x6000_child_classes": children,
        "gc1037_driver_initialized": "NOT_VERIFIED",
        "dcn316_initialized_or_edp_trained": "NOT_VERIFIED",
        "metal_acceleration": "NOT_VERIFIED",
        "autoinject_1506_into_navi10": False,
        "native_binding_completed": False,
        "stage3e_apple_binary_abi_state": abi_state,
        "stage3e_observed_symbol_classes": abi_classes,
        "stage3e_symbol_evidence_does_not_override_matching_or_attachment": True,
    }


def collect_live_registry():
    if platform.system() != "Darwin":
        raise ValueError("--live requires macOS")
    command = ["ioreg", "-a", "-l", "-w0", "-p", "IOService"]
    raw = subprocess.run(command, check=True, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, timeout=30).stdout
    return plistlib.loads(raw)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kext", nargs="+", metavar="KEXT_OR_PLIST", type=Path,
                        required=True, help="Installed Apple *.kext or local XML for comparison")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--ioreg", type=Path, help="Local ioreg -a plist; do not upload raw")
    group.add_argument("--live", action="store_true", help="Read IORegistry directly on macOS")
    parser.add_argument("--abi-report", type=Path,
                        help="Optional Stage3E output (Apple Mach-O symbol evidence only)")
    args = parser.parse_args()
    try:
        abi_audit = json.loads(args.abi_report.read_text(encoding='utf-8')) if args.abi_report else None
        plist_reports = [PLIST.load_kext(path) for path in args.kext]
        if args.live:
            tree = collect_live_registry()
        elif args.ioreg:
            tree = plistlib.loads(args.ioreg.read_bytes())
        else:
            tree = None
        print(json.dumps(summarize(plist_reports, tree, abi_audit), indent=2))
    except (OSError, ValueError, plistlib.InvalidFileException,
            subprocess.SubprocessError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
