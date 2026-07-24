#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Host-side Python tests for the Hub-side control logic (no Pybricks required).
# Each test mirrors a class/function in the Hub programs so the pure logic is
# checked independently, the same way test/native/test_main.cpp mirrors
# Vehicle::shapeThrottle. The native C++ drive tests are intentionally left to
# run-native-tests.sh.
python3 "$project_dir/test/test_assist.py"
python3 "$project_dir/test/test_steering.py"

echo "Python assist and steering tests passed"
