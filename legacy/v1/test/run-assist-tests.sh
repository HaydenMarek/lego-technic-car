#!/usr/bin/env bash
# Delegate to the combined Python test runner (kept for backward references).
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/run-python-tests.sh" "$@"
