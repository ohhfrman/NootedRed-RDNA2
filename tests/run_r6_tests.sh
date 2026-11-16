#!/usr/bin/env bash
# Host-only R6 gate verification. Does not load or modify macOS kernel state.
set -euo pipefail
cd "$(dirname "$0")/.."
CXX=${CXX:-c++}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
"$CXX" -std=c++17 -Wall -Wextra -Werror -I NootedRed \
    tests/mendocino_x6000_compatibility_test.cpp -o "$TMP/mendocino_x6000_test"
"$TMP/mendocino_x6000_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror -I NootedRed \
    tests/mendocino_display_preflight_test.cpp -o "$TMP/mendocino_display_preflight_test"
"$TMP/mendocino_display_preflight_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror -I NootedRed \
    tests/mendocino_native_attachment_test.cpp -o "$TMP/mendocino_native_attachment_test"
"$TMP/mendocino_native_attachment_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror -I NootedRed \
    tests/mendocino_provider_match_test.cpp -o "$TMP/mendocino_provider_match_test"
"$TMP/mendocino_provider_match_test"
python3 -m unittest discover -v -s tests -p "test_stage3c_provider.py"
python3 tests/test_r6_integration.py
python3 -m unittest discover -v -s tests -p "test_x6000_plist_audit.py"

python3 -m unittest discover -v -s tests -p "test_r6_ioreg_audit.py"

python3 -m unittest discover -v -s tests -p "test_r6_native_binding_report.py"
