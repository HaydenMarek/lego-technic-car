#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary="${TMPDIR:-/tmp}/technic-rc-native-tests"

g++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -Werror \
  -I"$project_dir/test/native" \
  -I"$project_dir/src" \
  "$project_dir/test/native/test_main.cpp" \
  "$project_dir/src/MotorDriver.cpp" \
  "$project_dir/src/Protocol.cpp" \
  "$project_dir/src/Vehicle.cpp" \
  "$project_dir/src/Watchdog.cpp" \
  -o "$binary"

"$binary"
echo "Native tests passed"
