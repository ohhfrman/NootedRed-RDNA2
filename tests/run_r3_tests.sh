#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
export PYTHONDONTWRITEBYTECODE=1
sh tests/run_r2_tests.sh
CXX="${CXX:-clang++}"
"$CXX" -std=c++23 -Wall -Wextra -Werror tests/mendocino_vbios_test.cpp -o /tmp/nootedred-r3-vfct-test
/tmp/nootedred-r3-vfct-test
python3 -m unittest discover -s tests -p 'test_vfct_extraction.py' -v
python3 -B -m py_compile Tools/extract-mendocino-vbios.py
