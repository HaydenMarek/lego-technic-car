# Sourced by the test scripts. Sets PYTHON to a working Python 3 interpreter.
# python3 is tried by running it, not just by name: on Windows the python.org
# installer provides only python, and python3 may be a Microsoft Store stub
# that exists on PATH but fails when run.
if python3 -c 'import sys; sys.exit(sys.version_info < (3,))' >/dev/null 2>&1; then
  PYTHON=python3
elif python -c 'import sys; sys.exit(sys.version_info < (3,))' >/dev/null 2>&1; then
  PYTHON=python
else
  echo "Python 3 is required (python3 or python on PATH)" >&2
  exit 1
fi
