#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CXX="${CXX:-clang++}"
"$CXX" -std=c++23 -Wall -Wextra -Werror -pedantic tests/mendocino_atom_display_test.cpp -o /tmp/nootedred-r6-atom-display-test
/tmp/nootedred-r6-atom-display-test
echo "PASS: R6 ATOM display topology and DCE v4.4 parser"
python3 -m unittest discover -s tests -p 'test_r6_atom_offline.py' -v
