#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CXX="${CXX:-clang++}"
"$CXX" -std=c++23 -Wall -Wextra -Werror tests/hardware_profile_test.cpp -o /tmp/nootedred-r2-hardware-test
/tmp/nootedred-r2-hardware-test
"$CXX" -std=c++23 -Wall -Wextra -Werror tests/pci_config_test.cpp -o /tmp/nootedred-r2-pci-test
/tmp/nootedred-r2-pci-test
python3 -m unittest discover -s tests -p test_ip_discovery.py -v
python3 -m py_compile Tools/collect-mendocino-linux.py
