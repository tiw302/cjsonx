#!/bin/bash -eu

# this script is executed by clusterfuzzlite.
# $CC, $CFLAGS, $LIB_FUZZING_ENGINE, and $OUT are provided by the environment.

# build the fuzzer executable linking against our source and the fuzzing engine.
$CC $CFLAGS -Iinclude -Isrc src/cjsonx.c tests/fuzzer.c -o $OUT/fuzzer $LIB_FUZZING_ENGINE
