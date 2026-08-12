#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
remote_links=false

if [[ "${1:-}" == "--remote-links" ]]; then
  remote_links=true
  shift
fi
if [[ $# -ne 0 ]]; then
  echo "usage: $0 [--remote-links]" >&2
  exit 2
fi

python3 "$project_dir/test/check_contracts.py"
python3 "$project_dir/test/check_plan_metadata.py"
if [[ "$remote_links" == true ]]; then
  python3 "$project_dir/test/check_links.py" --remote
else
  python3 "$project_dir/test/check_links.py"
fi
"$project_dir/test/run-native-tests.sh"
"$project_dir/test/run-python-tests.sh"
pio run --project-dir "$project_dir" -e uno_bench -e uno_bts7960

echo "Full non-hardware verification passed"
