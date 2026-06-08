#!/bin/bash
set -e

echo "Building cjsonx wasm module..."

if ! command -v emcc &> /dev/null
then
    echo "Error: emscripten (emcc) could not be found."
    echo "Please install emscripten sdk first: https://emscripten.org/docs/getting_started/downloads.html"
    exit 1
fi

# we compile wasm_wrapper.c using emcc.
# we also include some necessary headers from the include folder.
# the cjsonx library itself is implemented by defining cjsonx_implementation in wasm_wrapper.c
emcc js/wasm_wrapper.c -O3 -s WASM=1 \
    -s EXPORTED_FUNCTIONS="['_cjsonx_wasm_parse', '_cjsonx_wasm_get_error', '_cjsonx_wasm_get_error_offset', '_cjsonx_wasm_dump', '_cjsonx_wasm_free', '_malloc', '_free']" \
    -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap', 'UTF8ToString', 'getValue', 'setValue', 'lengthBytesUTF8', 'stringToUTF8']" \
    -o docs/demo/cjsonx_wasm.js

echo "Build successful! output placed in docs/demo/"
