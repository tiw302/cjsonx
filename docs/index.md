# cjsonx Documentation

Welcome to the official documentation for **cjsonx**.

The `cjsonx` library is a high-performance, single-header C11 JSON parser that achieves parsing speeds exceeding 1.0 GB/s. By decoupling the parsing process into SIMD structural scanning and flat-arena recursive descent, it provides blazing fast DOM access. Uniquely, it utilizes the state-of-the-art Eisel-Lemire algorithm to guarantee 64-bit IEEE 754 precision for extremely large numbers at maximum speed.

## Architecture & Features

- **Hardware Acceleration**: Native SIMD utilization via AVX2, NEON, and WASM-SIMD128 for rapid bitmask generation and whitespace skipping.
- **Cache Locality**: Uses a Flat Arena DOM allocator that packs all nodes sequentially into memory. 4 nodes fit in a single 64-byte cache line — no `malloc` per node.
- **Native Eisel-Lemire Parsing**: Handles edge-case scientific and financial numerical formats natively using the fastest known 64-bit float algorithm, with a secondary `cjsonx_fastfloat` fallback before resorting to `strtod`.
- **WebAssembly Ready**: Compiles natively to WASM with WASM-SIMD128 acceleration, bringing near-native C parsing speeds directly to the browser or Node.js.
- **Zero OS-Dependencies**: Pure C11 code, allowing deployment on embedded ARM architectures lacking POSIX layers.

## Quick Links

- [GitHub Repository](https://github.com/tiw302/cjsonx)
- [API Reference](api_reference.md)
- [Algorithm Deep-Dive](algorithm.md)
- [Live WebAssembly Demo](https://tiw302.github.io/cjsonx/demo/)

## Installation

### C/C++ (Header Only)

The library is designed to be easily integrated into any C or C++ project without complex build systems.

**Option 1: Drop-in Single Header (Recommended)**
Copy the amalgamated `single_include/cjsonx.h` into your project. In **exactly one** C file, define the implementation macro before including it:

```c
#define CJSONX_IMPLEMENTATION
#include "cjsonx.h"
```

**Option 2: System Install via CMake**

```bash
git clone https://github.com/tiw302/cjsonx.git
cd cjsonx
cmake -S . -B build
sudo cmake --install build
```

Then in your project's `CMakeLists.txt`:

```cmake
find_package(cjsonx REQUIRED)
target_link_libraries(your_target PRIVATE cjsonx::cjsonx)
```

### Node.js & Web

The WASM build leverages WASM-SIMD128 for near-native parsing performance directly in the browser or Node.js runtime. Compile via Emscripten:

```bash
emcmake cmake -S . -B build_wasm
cmake --build build_wasm
```

## Quick Start Example (C)

```c
#define CJSONX_IMPLEMENTATION
#include "cjsonx.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* json = "{\"name\": \"Alice\", \"score\": 99.9}";

    // Parse the JSON
    cjsonx_doc_t* doc = cjsonx_parse(json, strlen(json));

    if (doc && doc->is_valid) {
        // Extract values
        cjsonx_val_t name = cjsonx_get(doc->root, "name");
        cjsonx_val_t score = cjsonx_get(doc->root, "score");

        printf("Name: %.*s\n", (int)cjsonx_str_len(name), cjsonx_str(name));
        printf("Score: %f\n", cjsonx_num(score));
    }

    // Free the entire document (O(1) arena destruction)
    cjsonx_doc_free(doc);
    return 0;
}
```

## Performance & Benchmarking

The library includes an independent benchmarking suite. On modern x86 architecture (AVX2), `cjsonx` outperforms traditional DOM parsers like `cJSON` by roughly 5x-7x, reaching speeds of over **1.0 GB/s**.

To run the benchmarks locally:

```bash
cmake -S . -B build
cmake --build build
./build/bench_compare benchmarks/datasets/twitter.json
```
