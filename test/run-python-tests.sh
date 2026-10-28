#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=python.sh
source "$project_dir/test/python.sh"

# Host-side Python tests for the Hub-side control logic (no Pybricks required).
# Each test imports hub/control.py, the same pure implementation used by both
# Hub programs. The native C++ drive tests are intentionally left to
# run-native-tests.sh.
"$PYTHON" "$project_dir/test/test_assist.py"
"$PYTHON" "$project_dir/test/test_steering.py"
"$PYTHON" "$project_dir/test/test_link.py"

echo "Python assist, steering, and link tests passed"
