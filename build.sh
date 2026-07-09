#!/bin/bash

# build.sh - build script for cjsonx engine
# optimized with parallel builds and consistent naming

build_lib() {
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target cjsonx --parallel
    echo ""
    echo "===================================================================================="
    echo " build complete! library is in build/"
    echo "===================================================================================="
    echo " "
}

build_examples() {
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target example_dom_access example_float128_precision example_error_handling example_simple_parse example_embedded_noalloc example_builder_api --parallel
    echo ""
    echo "===================================================================================="
    echo " build complete! to run examples:"
    echo "  * ./build/example_dom_access"
    echo "  * ./build/example_float128_precision"
    echo "  * ./build/example_error_handling"
    echo "  * ./build/example_simple_parse"
    echo "  * ./build/example_embedded_noalloc"
    echo "  * ./build/example_builder_api"
    echo "===================================================================================="
    echo " "
}

run_tests() {
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    echo ""
    echo "Running tests..."
    ctest --test-dir build --output-on-failure
    echo "===================================================================================="
    echo " tests complete!"
    echo "===================================================================================="
    echo " "
}

run_benchmarks() {
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target bench_compare --parallel
    echo ""
    echo "Running benchmarks..."
    
    if [ -x "./build/bench_compare" ]; then
        if [ -f "benchmarks/datasets/twitter.json" ]; then
            ./build/bench_compare benchmarks/datasets/twitter.json
        else
            echo "note: twitter.json not found in benchmarks/datasets/"
            echo "you can download datasets using the scripts in the benchmarks folder."
        fi
        
        if [ -f "benchmarks/datasets/canada.json" ]; then
            ./build/bench_compare benchmarks/datasets/canada.json
        fi
        
        if [ -f "benchmarks/datasets/citm_catalog.json" ]; then
            ./build/bench_compare benchmarks/datasets/citm_catalog.json
        fi
    else
        echo "error: benchmark executable not built."
        echo "make sure third_party dependencies are downloaded (see benchmarks/ folder)."
    fi

    echo ""
    echo "===================================================================================="
    echo " benchmarks complete!"
    echo "===================================================================================="
    echo " "
}

build_all() {
    build_lib && build_examples && run_tests && run_benchmarks
}

clean() {
    rm -rf build
    echo "clean complete!"
}

if [ $# -gt 0 ]; then
    case "$1" in
        lib)      build_lib ;;
        examples) build_examples ;;
        test)     run_tests ;;
        bench)    run_benchmarks ;;
        all)      build_all ;;
        clean)    clean ;;
        *)        echo "error: unknown option '$1'. usage: $0 {lib|examples|test|bench|all|clean}" ;;
    esac
    exit 0
fi

echo "===================================================================================="
echo "cjsonx engine build!!"
echo "===================================================================================="
echo "  1) build library"
echo "  2) build examples"
echo "  3) run tests"
echo "  4) run benchmarks"
echo "  5) build all"
echo "  6) clean"
echo "  q) quit"
echo "===================================================================================="
echo " "
read -p ">> " choice

case $choice in
    1) build_lib ;;
    2) build_examples ;;
    3) run_tests ;;
    4) run_benchmarks ;;
    5) build_all ;;
    6) clean ;;
    q|Q) exit 0 ;;
    *)
        echo "error: invalid choice '$choice'. please enter a number between 1-6, or 'q' to quit."
        exit 1
        ;;
esac
