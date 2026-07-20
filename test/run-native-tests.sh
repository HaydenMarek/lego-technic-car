#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary_base="${TMPDIR:-/tmp}/technic-rc-native-tests"

compile_and_run() {
  local name="$1"
  shift
  local binary="${binary_base}-${name}"

  g++ \
    -std=c++17 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Werror \
    "$@" \
    -I"$project_dir/test/native" \
    -I"$project_dir/src" \
    "$project_dir/test/native/test_main.cpp" \
    "$project_dir/src/MotorDriver.cpp" \
    "$project_dir/src/Protocol.cpp" \
    "$project_dir/src/Vehicle.cpp" \
    "$project_dir/src/Watchdog.cpp" \
    -o "$binary"

  "$binary"
}

compile_and_run bench
compile_and_run single -DTECHNIC_RC_ENABLE_BTS7960=1

echo "Native bench and single-bridge tests passed"
