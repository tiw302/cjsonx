#!/bin/bash
# cjsonx helper build script
set -euo pipefail

# define paths based on the script's location
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${script_dir}/build"
run_tests=false
clean_first=false

# parse arguments using standard while loop
while [ $# -gt 0 ]; do
    case "$1" in
        --test)
            run_tests=true
            shift
            ;;
        --clean)
            clean_first=true
            shift
            ;;
        *)
            echo "unknown option: $1"
            exit 1
            ;;
    esac
done

# handle clean build option
if [ "$clean_first" = true ] && [ -d "$build_dir" ]; then
    echo "cleaning build directory..."
    rm -rf "$build_dir"
fi

# create build directory if it doesn't exist
if [ ! -d "$build_dir" ]; then
    echo "creating build directory: $build_dir"
    mkdir -p "$build_dir"
fi

# run cmake configuration
echo "configuring build with cmake..."
cmake -B "$build_dir" -S "$script_dir" -DCMAKE_BUILD_TYPE=Release

# build targets
echo "building cjsonx target binaries..."
cmake --build "$build_dir" --config Release

# run test suite if requested using --test-dir
if [ "$run_tests" = true ]; then
    echo "running cjsonx unit tests..."
    ctest --test-dir "$build_dir" --output-on-failure
fi

echo "build complete!"
