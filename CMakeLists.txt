cmake_minimum_required(VERSION 3.21)
project(cjsonx VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------
# options
# -----------------------------------------------------------------------
option(CJSONX_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(CJSONX_ENABLE_COVERAGE "Enable gcov code coverage generation" OFF)

if(MSVC)
    add_compile_options(/W4)
    # enable avx2 by default for performance testing on msvc
    add_compile_options(/arch:AVX2)
else()
    add_compile_options(-Wall -Wextra -O3)
    # enable avx2 by default for performance testing on gcc/clang (x86_64)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64)")
        add_compile_options(-march=native -mavx2)
    endif()
    if(NOT WIN32)
        link_libraries(m)
    endif()

    if(CJSONX_ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address,undefined)
    endif()

    if(CJSONX_ENABLE_COVERAGE)
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs -ftest-coverage)
    endif()
endif()

# library target
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/cjsonx.c")
add_library(cjsonx STATIC src/cjsonx.c)
target_include_directories(cjsonx PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

endif()

# examples
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/dom_access.c")
    add_executable(example_dom_access examples/dom_access.c)
    target_link_libraries(example_dom_access PRIVATE cjsonx)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/float128_precision.c")
    add_executable(example_float128 examples/float128_precision.c)
    target_link_libraries(example_float128 PRIVATE cjsonx)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/error_handling.c")
    add_executable(example_error_handling examples/error_handling.c)
    target_link_libraries(example_error_handling PRIVATE cjsonx)
endif()

enable_testing()

# tests
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_suite.c")
    add_executable(test_suite tests/test_suite.c)
    target_link_libraries(test_suite PRIVATE cjsonx)
    # note: test_suite requires a json file argument so it is not added to ctest
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stage1.c")
    add_executable(test_stage1 tests/test_stage1.c)
    target_link_libraries(test_stage1 PRIVATE cjsonx)
    add_test(NAME test_stage1 COMMAND test_stage1)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stage2.c")
    add_executable(test_stage2 tests/test_stage2.c)
    target_link_libraries(test_stage2 PRIVATE cjsonx)
    add_test(NAME test_stage2 COMMAND test_stage2)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_allocator.c")
    add_executable(test_allocator tests/test_allocator.c)
    target_link_libraries(test_allocator PRIVATE cjsonx)
    add_test(NAME test_allocator COMMAND test_allocator)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_builder_remove.c")
    add_executable(test_builder_remove tests/test_builder_remove.c)
    target_link_libraries(test_builder_remove PRIVATE cjsonx)
    add_test(NAME test_builder_remove COMMAND test_builder_remove)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pointer.c")
    add_executable(test_pointer tests/test_pointer.c)
    target_link_libraries(test_pointer PRIVATE cjsonx)
    add_test(NAME test_pointer COMMAND test_pointer)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bomb.c")
    add_executable(test_bomb tests/test_bomb.c)
    target_link_libraries(test_bomb PRIVATE cjsonx)
    add_test(NAME test_bomb COMMAND test_bomb)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stringify.c")
    add_executable(test_stringify tests/test_stringify.c)
    target_link_libraries(test_stringify PRIVATE cjsonx)
    add_test(NAME test_stringify COMMAND test_stringify)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_builder.c")
    add_executable(test_builder tests/test_builder.c)
    target_link_libraries(test_builder PRIVATE cjsonx)
    add_test(NAME test_builder COMMAND test_builder)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_conformance.c")
    add_executable(test_conformance tests/test_conformance.c)
    target_link_libraries(test_conformance PRIVATE cjsonx)
    add_test(NAME test_conformance COMMAND test_conformance ${CMAKE_CURRENT_SOURCE_DIR}/tests/JSONTestSuite/test_parsing)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fastfloat.c")
    add_executable(test_fastfloat tests/test_fastfloat.c)
    target_link_libraries(test_fastfloat PRIVATE cjsonx)
    add_test(NAME test_fastfloat COMMAND test_fastfloat)
endif()
if(TARGET cjsonx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_utf8.c")
    add_executable(test_utf8 tests/test_utf8.c)
    target_link_libraries(test_utf8 PRIVATE cjsonx)
endif()

# benchmark
# note: bench_compare requires third_party libraries (yyjson, cjson, jsmn)
# to be downloaded into benchmarks/third_party/
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/third_party/yyjson.c")
    add_executable(bench_compare
        benchmarks/bench_compare.c
        benchmarks/third_party/yyjson.c
        benchmarks/third_party/cJSON.c
    )
    target_include_directories(bench_compare PRIVATE benchmarks/third_party)
    target_link_libraries(bench_compare PRIVATE cjsonx)
endif()
