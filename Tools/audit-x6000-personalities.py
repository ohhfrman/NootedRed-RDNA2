#!/usr/bin/env python3
"""Inspect actual Apple X6000 KEXT IOService personalities, without modifying them.

This is an offline prerequisite for a safe native driver adaptation. A matching
PCI ID in an Info.plist is never proof that GC/DCN firmware initialization works.
"""
from __future__ import annotations
import argparse
import json
import plistlib
import re
from pathlib import Path

MENDOCINO_PCI_KEY = (0x1506 << 16) | 0x1002
PCI_FIELDS = ("IOPCIMatch", "IOPCIPrimaryMatch", "IOPCISecondaryMatch")
TOKEN = re.compile(r"^(?:0x)?([0-9a-fA-F]{1,8})(?:\s*&\s*(?:0x)?([0-9a-fA-F]{1,8}))?$")


def match_token(raw: str, target: int = MENDOCINO_PCI_KEY) -> bool | None:
    matched = TOKEN.fullmatch(raw.strip())
    if not matched:
        return None
    value = int(matched.group(1), 16)
    mask = int(matched.group(2), 16) if matched.group(2) else 0xffffffff
    return (value & mask) == (target & mask)


def evaluate_personality(name: str, properties: dict) -> dict:
    details = []
    positive = False
    known = False
    for field in PCI_FIELDS:
        raw = properties.get(field)
        if not isinstance(raw, str):
            continue
        # Normalise spaces around '&': macOS PCI matching uses value&mask.
        # Preserve each whitespace-delimited *expression* as a single token.
        tokens = re.sub(r"\s*&\s*", "&", raw.strip()).split()
        results = [match_token(token) for token in tokens]
        details.append({"field": field, "tokens": tokens, "matching_tokens": [t for t, m in zip(tokens, results) if m is True],
                        "unparsed_tokens": [t for t, m in zip(tokens, results) if m is None]})
        known |= any(result is not None for result in results)
        positive |= any(result is True for result in results)
    if positive:
        status = "EXPLICIT_PCI_ID_MATCH"
    elif not details:
        # A class/name/provider-only personality may still match the GPU.
        status = "NO_EXPLICIT_PCI_ID_RULE_INDETERMINATE"
    elif not known or any(x["unparsed_tokens"] for x in details):
        status = "PCI_MATCH_RULE_INDETERMINATE"
    else:
        status = "NO_EXPLICIT_1506_MATCH"
    return {"name": name, "class": properties.get("IOClass"),
            "provider": properties.get("IOProviderClass"), "status": status,
            "class_match": properties.get("IOPCIClassMatch"), "rules": details}


def load_kext(path: Path) -> dict:
    original = path
    if path.is_dir():
        path = path / "Contents" / "Info.plist"
    with path.open("rb") as fp:
        raw = fp.read()
    # Local Personalities/*.xml files are plist fragments (<array>), not a
    # complete Info.plist. Wrap only that known, intentionally supplied form.
    if raw.lstrip().startswith(b"<array>"):
        raw = b"<plist version=\"1.0\">" + raw + b"</plist>"
    info = plistlib.loads(raw)
    if isinstance(info, list):
        # NootedRed's Personalities/*.xml is a plist ARRAY rather than a KEXT Info.plist.
        entries = [(f"personality_{i}", item) for i, item in enumerate(info) if isinstance(item, dict)]
        bundle = "LOCAL_PERSONALITY_RESOURCE_NOT_NATIVE_KEXT"
    elif isinstance(info, dict):
        personalities = info.get("IOKitPersonalities") or {}
        if not isinstance(personalities, dict):
            raise ValueError("IOKitPersonalities must be a dictionary")
        entries = list(personalities.items())
        bundle = info.get("CFBundleIdentifier")
    else:
        raise ValueError("Expected Info.plist dictionary or local resource personality array")
    return {"path": str(original), "bundle_id": bundle,
            "personalities": [evaluate_personality(name, props)
                              for name, props in entries if isinstance(props, dict)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kext", nargs="+", type=Path,
                        help="Native *.kext/Contents/Info.plist or local Personalities/*.xml")
    parser.add_argument("--json", action="store_true", help="Print JSON instead of summary")
    args = parser.parse_args()
    reports = []
    for path in args.kext:
        try:
            reports.append(load_kext(path))
        except (OSError, ValueError, plistlib.InvalidFileException) as exc:
            parser.error(f"Cannot inspect {path}: {exc}")
    if args.json:
        print(json.dumps(reports, indent=2))
        return
    print("R6 native X6000 personality audit — 1002:1506 (no binary patches performed)")
    for report in reports:
        print(f"\n{report['bundle_id'] or 'UNKNOWN_BUNDLE'}: {report['path']}")
        if not report['personalities']:
            print("  No IOKitPersonality entries; this is not proof of driver incompatibility")
        for item in report["personalities"]:
            print(f"  {item['name']}: {item['status']} (IOClass={item['class']}, provider={item['provider']})")
    print("\nThese are matching rules only. They do not verify GPU attach, firmware, display or Metal.")


if __name__ == "__main__":
    main()
