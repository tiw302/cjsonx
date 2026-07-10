#!/bin/bash

# run_test_suite.sh — json test suite conformance test runner.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$script_dir/JSONTestSuite/test_parsing" || exit 1

# detect test_suite executable in build directories
if [ -f "../../../build/test_suite" ]; then
    exe="../../../build/test_suite"
elif [ -f "../../../build_san/test_suite" ]; then
    exe="../../../build_san/test_suite"
else
    echo "error: test_suite executable not found in build/ or build_san/"
    exit 1
fi

pass_y=0
fail_y=0
for f in y_*.json; do
    if "$exe" "$f" >/dev/null 2>&1; then
        ((pass_y++))
    else
        ((fail_y++))
    fi
done
echo "Valid JSON (y_*.json): $pass_y passed, $fail_y failed"

pass_n=0
fail_n=0
for f in n_*.json; do
    if "$exe" "$f" >/dev/null 2>&1; then
        ((fail_n++))
    else
        ((pass_n++))
    fi
done
echo "Invalid JSON (n_*.json): $pass_n passed (rejected correctly), $fail_n failed (accepted invalid JSON)"
