#!/bin/sh
set -eu
cd "$(dirname "$0")"
"${CXX:-c++}" -std=c++23 -Wall -Wextra -Werror -pedantic hardware_profile_test.cpp -o /tmp/nootedred_hardware_profile_test
/tmp/nootedred_hardware_profile_test
