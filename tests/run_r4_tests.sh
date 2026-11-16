#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sh tests/run_r3_tests.sh
CXX="${CXX:-clang++}"
"$CXX" -std=c++23 -Wall -Wextra -Werror -pedantic tests/mendocino_ip_discovery_test.cpp -o /tmp/nootedred-r4-ip-edid-test
/tmp/nootedred-r4-ip-edid-test
python3 -m unittest discover -s tests -p 'test_ip_report_verify.py' -v
python3 -B -m py_compile Tools/verify-mendocino-ip-report.py
