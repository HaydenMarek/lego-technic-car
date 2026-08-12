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
    "$project_dir/src/CurrentMonitor.cpp" \
    "$project_dir/src/CurrentProtection.cpp" \
    "$project_dir/src/MotorDriver.cpp" \
    "$project_dir/src/Protocol.cpp" \
    "$project_dir/src/Vehicle.cpp" \
    "$project_dir/src/Watchdog.cpp" \
    -o "$binary"

  "$binary"
}

compile_and_run bench \
  -DTECHNIC_RC_ENABLE_BTS7960=0 \
  -DTECHNIC_RC_ENABLE_CURRENT_PROTECTION=0 \
  -DTECHNIC_RC_ENABLE_DYNAMIC_BRAKING=0 \
  -DTECHNIC_RC_ENABLE_MONITOR_COMMANDS=1 \
  -DTECHNIC_RC_THROTTLE_CURVE_EXPONENT=1 \
  -DTECHNIC_RC_EXPECT_BTS7960=0 \
  -DTECHNIC_RC_EXPECT_DYNAMIC_BRAKING=0 \
  -DTECHNIC_RC_EXPECT_CURRENT_PROTECTION=0 \
  -DTECHNIC_RC_EXPECT_MONITOR_COMMANDS=1 \
  -DTECHNIC_RC_EXPECT_THROTTLE_CURVE_EXPONENT=1
compile_and_run bts7960 \
  -DTECHNIC_RC_ENABLE_BTS7960=1 \
  -DTECHNIC_RC_ENABLE_CURRENT_PROTECTION=1 \
  -DTECHNIC_RC_ENABLE_DYNAMIC_BRAKING=0 \
  -DTECHNIC_RC_ENABLE_MONITOR_COMMANDS=0 \
  -DTECHNIC_RC_THROTTLE_CURVE_EXPONENT=1 \
  -DTECHNIC_RC_EXPECT_BTS7960=1 \
  -DTECHNIC_RC_EXPECT_DYNAMIC_BRAKING=0 \
  -DTECHNIC_RC_EXPECT_CURRENT_PROTECTION=1 \
  -DTECHNIC_RC_EXPECT_MONITOR_COMMANDS=0 \
  -DTECHNIC_RC_EXPECT_THROTTLE_CURVE_EXPONENT=1
compile_and_run brake \
  -DTECHNIC_RC_ENABLE_BTS7960=1 \
  -DTECHNIC_RC_ENABLE_CURRENT_PROTECTION=1 \
  -DTECHNIC_RC_ENABLE_DYNAMIC_BRAKING=1 \
  -DTECHNIC_RC_ENABLE_MONITOR_COMMANDS=0 \
  -DTECHNIC_RC_THROTTLE_CURVE_EXPONENT=1 \
  -DTECHNIC_RC_EXPECT_BTS7960=1 \
  -DTECHNIC_RC_EXPECT_DYNAMIC_BRAKING=1 \
  -DTECHNIC_RC_EXPECT_CURRENT_PROTECTION=1 \
  -DTECHNIC_RC_EXPECT_MONITOR_COMMANDS=0 \
  -DTECHNIC_RC_EXPECT_THROTTLE_CURVE_EXPONENT=1

echo "Native bench, BTS7960, and dynamic-braking tests passed"
