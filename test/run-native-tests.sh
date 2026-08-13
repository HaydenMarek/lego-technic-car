#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary="${TMPDIR:-/tmp}/technic-rc-v2-native-tests"

g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  -I"$project_dir/main" \
  "$project_dir/test/native/test_main.cpp" \
  "$project_dir/main/VehicleState.cpp" \
  -o "$binary"
"$binary"
echo "v2 native control-state tests passed"
