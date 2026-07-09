#!/bin/bash

# scripts/format.sh
# Run clang-format on all C and Header files in the project.

cd "$(dirname "$0")/.." || exit 1

echo "[⚙] Running clang-format on source files..."

# Find all .c and .h files in src, include, examples, tests, and benchmarks
find src include examples tests benchmarks js \
    -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" \) \
    -not -path "*/third_party/*" \
    -not -path "*/single_include/*" \
    -not -path "*/build/*" \
    -exec clang-format -i -style=file {} +

echo "[✓] Formatting complete."
