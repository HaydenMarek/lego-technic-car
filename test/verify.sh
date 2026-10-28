#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=python.sh
source "$project_dir/test/python.sh"
remote_links=false

if [[ "${1:-}" == "--remote-links" ]]; then
  remote_links=true
  shift
fi
if [[ $# -ne 0 ]]; then
  echo "usage: $0 [--remote-links]" >&2
  exit 2
fi

"$PYTHON" "$project_dir/test/check_contracts.py"
"$PYTHON" "$project_dir/test/check_agent_safety.py"
"$PYTHON" "$project_dir/test/check_plan_metadata.py"
if [[ "$remote_links" == true ]]; then
  "$PYTHON" "$project_dir/test/check_links.py" --remote
else
  "$PYTHON" "$project_dir/test/check_links.py"
fi
"$project_dir/test/run-native-tests.sh"
"$project_dir/test/run-python-tests.sh"
pio run --project-dir "$project_dir" -e uno_bench -e uno_bts7960 -e nano_bench

echo "Full non-hardware verification passed"
