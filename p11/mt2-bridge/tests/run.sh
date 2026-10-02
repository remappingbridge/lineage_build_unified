#!/bin/bash
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  "$here/bridge_test.cc" -o "$tmp/bridge-test"
# LeakSanitizer cannot inspect /proc/task under the Work sandbox. ASan/UBSan stay on.
ASAN_OPTIONS=detect_leaks=0 "$tmp/bridge-test" "$tmp/descriptor.bin"
python3 "$here/descriptor_test.py" "$tmp/descriptor.bin"
