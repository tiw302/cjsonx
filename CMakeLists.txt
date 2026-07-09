# =========================================================================
# CMakeLists.txt -- build orchestration for cjsonx
# project url: https://github.com/tiw302/cjsonx
#
# this file handles the compilation of tests, benchmarks, and examples.
# =========================================================================

cmake_minimum_required(VERSION 3.21)
project(cjsonx VERSION 1.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ensure executables go to build/ directly, matching previous behavior
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# =========================================================================
# options
# =========================================================================
option(CJSONX_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(CJSONX_ENABLE_COVERAGE "Enable gcov code coverage generation" OFF)

if(MSVC)
    add_compile_options(/W4)
    # enable avx2 by default for performance testing on msvc
    add_compile_options(/arch:AVX2)
else()
    add_compile_options(-Wall -Wextra -O3)
    add_compile_definitions(_GNU_SOURCE)
    # avx2 + pclmul: pclmulqdq is required for the string-in-quote bitmask trick
    # -march=native usually enables it but explicit is safer for non-native builds
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64)")
        add_compile_options(-march=native -mavx2 -mpclmul)
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

# =========================================================================
# library target
# =========================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/cjsonx.c")
add_library(cjsonx STATIC src/cjsonx.c)
target_include_directories(cjsonx PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

endif()

# =========================================================================
# subdirectories
# =========================================================================
option(CJSONX_BUILD_EXAMPLES "Build cjsonx examples" ON)
if(CJSONX_BUILD_EXAMPLES AND TARGET cjsonx)
    add_subdirectory(examples)
endif()

option(CJSONX_BUILD_TESTS "Build cjsonx tests" ON)
if(CJSONX_BUILD_TESTS AND TARGET cjsonx)
    enable_testing()
    add_subdirectory(tests)
endif()

option(CJSONX_BUILD_BENCHMARKS "Build cjsonx benchmarks" ON)
if(CJSONX_BUILD_BENCHMARKS AND TARGET cjsonx)
    add_subdirectory(benchmarks)
endif()

