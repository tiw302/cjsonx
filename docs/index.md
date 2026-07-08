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

### Python / PyPI

Install the Python bindings directly from PyPI \u2014 pre-built wheels for Linux, macOS, and Windows:

```bash
pip install cjsonx
```

Then parse and query JSON natively from Python:

```python
import cjsonx

doc = cjsonx.parse('{"name": "alice", "scores": [10, 20, 30]}')
print(doc["name"])           # alice
print(doc["scores"][0])      # 10
print(doc.get("/scores/2"))  # 30 (json pointer)
```

### Node.js / npm

Install the pre-built WASM package from npm — no native compilation required:

```bash
npm install @tiw302/cjsonx
```

Then use the full DOM query API directly in Node.js or the browser:

```js
const cjsonx = require('@tiw302/cjsonx');

await cjsonx.ready;

const ok = cjsonx.parse('{"user": "alice", "scores": [10, 20, 30]}');
if (ok) {
    const root  = cjsonx.getRoot();
    const user  = root.get('user').str;       // 'alice'
    const first = root.get('scores').getIndex(0).num; // 10
    const deep  = root.pointer('/scores/2').num;       // 30
    const obj   = root.toJS();               // native JS object
    cjsonx.free();
}
```

> **Advanced:** If you need to compile the WASM module yourself (e.g. to enable debug symbols or custom flags), use Emscripten from the repository root:
> ```bash
> cd js && node build.js
> ```

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
