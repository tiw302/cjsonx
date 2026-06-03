#!/bin/bash
cd tests/JSONTestSuite/test_parsing || exit 1
pass_y=0
fail_y=0
for f in y_*.json; do
    if ../../../build_san/test_suite "$f" >/dev/null 2>&1; then
        ((pass_y++))
    else
        ((fail_y++))
    fi
done
echo "Valid JSON (y_*.json): $pass_y passed, $fail_y failed"

pass_n=0
fail_n=0
for f in n_*.json; do
    if ../../../build_san/test_suite "$f" >/dev/null 2>&1; then
        ((fail_n++))
    else
        ((pass_n++))
    fi
done
echo "Invalid JSON (n_*.json): $pass_n passed (rejected correctly), $fail_n failed (accepted invalid JSON)"
