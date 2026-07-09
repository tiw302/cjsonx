<p align="center">
  <img src="https://raw.githubusercontent.com/tiw302/cjsonx/master/assets/images/logo.png" width="355" alt="cjsonx Logo">
  <br>
  <b>Extreme-performance JSON parser for C11/C++ featuring a 16-byte ultra-compact DOM.</b>
</p>

# cjsonx

[![Linux](https://github.com/tiw302/cjsonx/actions/workflows/linux.yml/badge.svg)](https://github.com/tiw302/cjsonx/actions)
[![macOS](https://github.com/tiw302/cjsonx/actions/workflows/macos.yml/badge.svg)](https://github.com/tiw302/cjsonx/actions)
[![Windows](https://github.com/tiw302/cjsonx/actions/workflows/windows.yml/badge.svg)](https://github.com/tiw302/cjsonx/actions)
[![WASM](https://github.com/tiw302/cjsonx/actions/workflows/wasm.yml/badge.svg)](https://github.com/tiw302/cjsonx/actions)
[![Sanitizers](https://github.com/tiw302/cjsonx/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/tiw302/cjsonx/actions)
[![Fuzzing](https://github.com/tiw302/cjsonx/actions/workflows/cflite_pr.yml/badge.svg)](https://github.com/tiw302/cjsonx/actions)

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Language](https://img.shields.io/badge/Language-C11-00599C.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Header-Only](https://img.shields.io/badge/Library-Header--Only-brightgreen.svg)](#installation)
[![Dependencies](https://img.shields.io/badge/Dependencies-None-blueviolet.svg)](#introduction)
[![npm](https://img.shields.io/npm/v/%40tiw302%2Fcjsonx.svg?label=npm)](https://www.npmjs.com/package/@tiw302/cjsonx)
[![PyPI](https://img.shields.io/pypi/v/cjsonx.svg)](https://pypi.org/project/cjsonx/)

**[Read the Official Documentation: docs/index.md](https://tiw302.github.io/cjsonx/)**<br>
**[Try the Live WebAssembly Demo: https://tiw302.github.io/cjsonx/demo/](https://tiw302.github.io/cjsonx/demo/)**

> **Verified Compatibility — Cross-Platform Passing**

| Architecture | Platform | Verified Backend |
| :--- | :--- | :--- |
| **x86_64 (Modern)** | Linux / Windows | **AVX2** (Vectorized) |
| **ARM64 (Apple)** | macOS (M1/M2/M3) | **NEON** (Vectorized) |
| **WebAssembly** | Chrome / Node.js | **WASM-SIMD128** |
| **RISC-V64** | Linux (QEMU) | **Scalar** C11 |
| **General Desktop** | Linux / Windows | **Scalar** C11 Fallback |

---

## Table of Contents

- [Introduction](#introduction)
  - [Why cjsonx?](#why-cjsonx)
  - [Trade-offs & Alternatives](#trade-offs--alternatives-when-not-to-use-cjsonx)
  - [Design Philosophy](#design-philosophy)
- [Limits & Guarantees](#limits--guarantees)
- [Requirements](#requirements)
  - [Verified Toolchains](#verified-toolchains)
- [Build and Installation](#build-and-installation)
  - [Single-Header Distribution](#single-header-distribution-recommended)
  - [CMake (System Install)](#cmake-system-install)
  - [Python / PyPI](#python--pypi)
  - [Node.js / npm](#nodejs--npm)
  - [Developer Build Flags](#developer-build-flags)
- [Configuration Macros](#configuration-macros)
- [API Reference](#api-reference)
  - [Core Parsing](#core-parsing)
  - [Owned-Copy Parsing](#owned-copy-parsing)
  - [DOM Access](#dom-access)
  - [Iteration](#iteration)
  - [Mutation & Builder API](#mutation--builder-api)
  - [File I/O Utilities](#file-io-utilities)
  - [Type Aliases](#type-aliases)
- [Documentation](#documentation)
- [Examples](#examples)
  - [Quick Start: Basic Parsing & Iteration](#quick-start-basic-parsing--iteration)
  - [Quick Start: Zero-Allocation Mode](#quick-start-zero-allocation-mode-embeddedrtos)
- [Benchmark Results](#benchmark-results)
- [Development Methodology & AI Assistance](#development-methodology--ai-assistance)
- [Author's Note](#authors-note)
- [License](#license)

---

## Introduction

**cjsonx** is a header-only C library for parsing JSON. It is designed to achieve high parsing speeds (exceeding 1.0 GB/s on modern hardware) while offering a fully mutable, ultra-compact 16-byte Flat-DOM.

Built on top of a highly optimized dual-stage architecture, `cjsonx` validates structural characters using SIMD bitmasks (AVX2/NEON/WASM-SIMD) before applying a recursive descent parsing phase that utilizes the state-of-the-art Eisel-Lemire algorithm for blazing-fast 64-bit IEEE 754 floating-point numerical conversions.

---

## Why cjsonx?

Standard JSON parsers often face specific limitations: they can be slower due to heavy heap allocation per node (using `malloc` recursively), or they consume excessive memory per node (e.g., standard parsers often require 56-64 bytes per node).

`cjsonx` was built to address these specific use cases by providing a fully mutable DOM while drastically reducing memory overhead and maximizing computational throughput:

| Parser | Speed (Large Payload) | DOM Node Size | Allocation Strategy | Portability |
|---|---|---|---|---|
| `cJSON` | ~130 MB/s | ~64 bytes | Heavy (O(N) Malloc) | Universal |
| `jsmn` | ~600 MB/s | Tokenizer Only | None | Universal |
| `yyjson` | ~1000+ MB/s | 16-24 bytes | Arena | High |
| **cjsonx** | **~1000+ MB/s** | **16 bytes (Fixed)** | **Flat Arena** | **Universal** |

cjsonx aims to provide an alternative: **delivering high throughput and a fully mutable DOM while maintaining an incredibly dense 16-byte memory footprint.**

---

## Trade-offs & Alternatives (When NOT to use cjsonx)

We believe in engineering honesty. `cjsonx` is built for a specific niche and is *not* a silver bullet. You should evaluate alternatives if your requirements match the following:

- **Need the absolute fastest C++ parser?** Use [simdjson](https://github.com/simdjson/simdjson). It runs at 3-6 GB/s and is the industry gold standard for C++ server backends. `cjsonx` is pure C11 and cannot compete with their multi-year optimized C++ engine.
- **Need a battle-tested, general-purpose C parser?** Use [yyjson](https://github.com/ibireme/yyjson). It is incredibly fast, highly optimized for general use cases, and has a massive community.
- **Need to drop in a ubiquitous, legacy C parser?** Use [cJSON](https://github.com/DaveGamble/cJSON). It's older and much slower, but it works on ancient C89 compilers and has no modern standard requirements. (Note: `cjsonx` also runs without SIMD on any platform via its Scalar fallback, but requires a C11-compliant compiler).

**So when *should* you use cjsonx?**

1. **High-Performance Mutable Data:** You need a pure C11 parser that allows you to read, edit, add, and remove JSON nodes rapidly, and stringify them back to JSON text without rebuilding the entire document.
2. **Strict Memory Constraints (IoT/RTOS):** You need high-speed parsing but absolutely **refuse to waste memory**. Our 16-byte nodes use 4x less RAM than traditional parsers like cJSON. Additionally, `cjsonx_parse_with_buffer()` provides a True Zero-Allocation mode for embedded systems.
3. **WASM / Node.js / Browser:** The `@tiw302/cjsonx` npm package brings full DOM querying to JavaScript. After parsing, you can walk the tree field-by-field via `getRoot()`, `.get(key)`, `.getIndex(i)`, `.pointer(path)`, and `.toJS()` — no JSON.parse re-serialization needed.

---

## Design Philosophy

The library is built around three strict constraints:

**Flat Arena DOM.** There are no calls to `malloc` per node. The entire document tree is parsed sequentially into a continuous array of 16-byte structs. This guarantees cache locality and enables O(1) skipping over complex objects and arrays during iteration.

**State-of-the-art Number Parsing.** `cjsonx` incorporates the Eisel-Lemire fast float algorithm directly into its lexical analysis phase. It parses 99.9% of all IEEE 754 floating-point numbers natively using a single fast path, falling back to strict standard library parsing only on extreme mathematical edge cases.

**Zero OS-Dependencies.** The library is built entirely on standard C11. It does not rely on OS-specific file I/O or POSIX headers. It compiles seamlessly to WebAssembly, embedded ARM targets, and standard desktop operating systems.

**True Zero-Allocation Mode.** For strict embedded constraints, the `cjsonx_parse_with_buffer()` API completely bypasses `malloc` by parsing the JSON entirely into a user-provided fixed-size stack buffer or RTOS memory pool.

---

## Limits & Guarantees

Professional-grade software requires transparent technical boundaries. Here is exactly what `cjsonx` guarantees, and where it draws the line:

- **RFC 8259 Compliance:** `cjsonx` strictly adheres to RFC 8259 and ECMA-404. It correctly rejects structural anomalies, unescaped control characters, and deeply nested bombs.
- **Thread Safety:** The core parsing engine is entirely stateless. Multiple threads can safely parse different JSON documents concurrently without any mutexes or locks.
- **Length Limit:** The maximum byte length of any single string or serialized container is 16MB (specifically, 16,777,215 bytes, due to the 24-bit length field packed in the 16-byte DOM node structure).
- **Nesting Depth Limit:** The maximum nesting depth is 1000 (`CJSONX_MAX_DEPTH`) to prevent stack overflow on deeply nested documents. This value is compile-time configurable.
- **Builder Performance:** Pushing elements to an array via `cjsonx_array_push` is an O(N) operation because it traverses the list of siblings to locate the end of the array. Repeated sequential pushes to build large arrays will result in O(N^2) complexity.
- **Static Buffer Read-Only:** Documents parsed with `cjsonx_parse_with_buffer()` are marked `is_static = true`. The entire DOM is read-only — calling any Builder API function (e.g., `cjsonx_object_set`, `cjsonx_array_push`) on a static document will return failure, as the internal node array cannot grow. `cjsonx_doc_free()` on a static document is a safe no-op.

---

## Requirements

| Component | Requirement |
|---|---|
| C Standard | C11 or later |
| Compiler | GCC 4.9+, Clang 3.5+, MSVC 2019+, Emscripten 3.0+ |
| Dependencies| None (Standard C Library only) |

---

## Verified Toolchains

The following toolchains are tested on every commit via GitHub Actions:

| Toolchain | Platform | Backend |
|---|---|---|
| GCC | Linux x86_64 | Scalar, AVX2 |
| GCC (riscv64-linux-gnu) | Linux RISC-V64 (QEMU) | Scalar |
| Clang | macOS Apple Silicon | NEON |
| MSVC | Windows x64 | Scalar, AVX2 |
| Emscripten | WASM (Node.js) | WASM-SIMD, Scalar |

---

## Build and Installation

`cjsonx` is entirely header-only.

### Single-Header Distribution (Recommended)

The simplest integration is copying the amalgamated `single_include/cjsonx.h` into your project. Define the implementation macro in **exactly one** C file to compile the core functions:

```c
#define CJSONX_IMPLEMENTATION
#include "cjsonx.h"
```

All other translation units should include the header without the macro.

### CMake (System Install)

You can build the test suites and install the library system-wide:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

Then in your project's `CMakeLists.txt`:

```cmake
find_package(cjsonx REQUIRED)
target_link_libraries(my_app PRIVATE cjsonx::cjsonx)
```

### Python / PyPI

Install the Python bindings via pip — no build tools required, wheels are pre-built for Linux, macOS, and Windows:

```bash
pip install cjsonx
```

Then use it directly from Python:

```python
import cjsonx

doc = cjsonx.parse('{"name": "alice", "scores": [10, 20, 30]}')
print(doc["name"])              # alice
print(doc["scores"][0])         # 10
print(doc.get("/scores/2"))     # 30 (json pointer)
```

### Node.js / npm

Install the pre-built WebAssembly package — no native compilation or Emscripten required:

```bash
npm install @tiw302/cjsonx
```

Then query the DOM directly from JavaScript:

```js
const cjsonx = require('@tiw302/cjsonx');
await cjsonx.ready;

const ok = cjsonx.parse('{"name": "alice", "scores": [10, 20, 30]}');
if (ok) {
    const root  = cjsonx.getRoot();
    const name  = root.get('name').str;             // 'alice'
    const first = root.get('scores').getIndex(0).num; // 10
    const obj   = root.toJS();                      // plain JS object
    cjsonx.free();
}
```

### Developer Build Flags

The CMake build exposes optional flags for contributors and CI pipelines:

```bash
# Enable AddressSanitizer + UndefinedBehaviorSanitizer
cmake -B build_san -DCJSONX_ENABLE_SANITIZERS=ON
cmake --build build_san
ctest --test-dir build_san

# Enable gcov code coverage instrumentation
cmake -B build_cov -DCJSONX_ENABLE_COVERAGE=ON
cmake --build build_cov
ctest --test-dir build_cov
```

---

## Configuration Macros

All constants can be overridden at compile time by defining them **before** including the header (or passing them as `-D` flags to your compiler). The defaults are suitable for most workloads.

| Macro | Default | Description |
|---|---|---|
| `CJSONX_MAX_DEPTH` | `1000` | Maximum JSON nesting depth. Documents exceeding this during parsing are rejected to prevent stack overflow. |
| `CJSONX_ARENA_CHUNK_SIZE` | `4096` | Byte size of each chunk allocated by the string arena. Increase for documents with many long escaped strings. |
| `CJSONX_INITIAL_TAPE_CAP` | `1024` | Initial capacity (in entries) of the Stage 1 structural token tape. |
| `CJSONX_INITIAL_CONTAINER_CAP` | `16` | Initial capacity (in nodes) of the flat DOM node array. |

**Example — embedded target with a tiny nesting limit:**

```c
#define CJSONX_MAX_DEPTH 32
#define CJSONX_ARENA_CHUNK_SIZE 512
#define CJSONX_IMPLEMENTATION
#include "cjsonx.h"
```

---

## API Reference

### Core Parsing

| Function | Signature | Description |
|---|---|---|
| `cjsonx_parse` | `cjsonx_doc_t* cjsonx_parse(const char* json, size_t length)` | Parses a JSON string into a managed document tree. **Zero-copy** — the input buffer must outlive the document. Returns `NULL` on fatal memory error. Check `doc->is_valid` for syntax status. |
| `cjsonx_parse_ex` | `cjsonx_doc_t* cjsonx_parse_ex(const char* json, size_t length, cjsonx_allocator_t* alloc)` | Parses a JSON string using custom memory allocation hooks. |
| `cjsonx_parse_with_buffer` | `cjsonx_doc_t* cjsonx_parse_with_buffer(const char* json, size_t length, void* buffer, size_t buffer_size)` | Zero-allocation mode. Parses JSON into a user-provided buffer. Result is read-only (`is_static = true`); Builder API calls will fail on this document. |
| `cjsonx_doc_free` | `void cjsonx_doc_free(cjsonx_doc_t* doc)` | Frees the entire document arena in a single call. |
| `cjsonx_error_string` | `const char* cjsonx_error_string(cjsonx_error_t err)` | Translates an error code into a human-readable string. |

### Owned-Copy Parsing

Use these when you don't want to manage the lifetime of the input buffer yourself. The document takes ownership of an internal copy of the JSON string — you can free or modify the original buffer immediately after the call.

| Function | Signature | Description |
|---|---|---|
| `cjsonx_parse_copy` | `cjsonx_doc_t* cjsonx_parse_copy(const char* json, size_t length)` | Copies the input buffer and parses it. The document owns the copy. |
| `cjsonx_parse_copy_ex` | `cjsonx_doc_t* cjsonx_parse_copy_ex(const char* json, size_t length, cjsonx_allocator_t* alloc)` | Same as above, but with a custom allocator. |
| `cjsonx_parse_copy_cstr` | `cjsonx_doc_t* cjsonx_parse_copy_cstr(const char* json)` | Convenience wrapper for null-terminated strings. |

### DOM Access

| Function | Signature | Description |
|---|---|---|
| `cjsonx_get` | `cjsonx_val_t cjsonx_get(cjsonx_val_t obj, const char* key)` | Retrieves a child node from an Object by its exact null-terminated string key. O(N) linear scan. |
| `cjsonx_get_len` | `cjsonx_val_t cjsonx_get_len(cjsonx_val_t obj, const char* key, size_t key_len)` | Same as `cjsonx_get` but accepts a key with explicit length. Useful for keys that are **not** null-terminated. |
| `cjsonx_get_index` | `cjsonx_val_t cjsonx_get_index(cjsonx_val_t arr, size_t index)` | Retrieves a child node from an Array by its index. O(N) sibling walk. |
| `cjsonx_get_type` | `cjsonx_type_t cjsonx_get_type(cjsonx_val_t val)` | Returns the type of the node (`CJSONX_STRING`, `CJSONX_NUMBER`, etc.). |
| `cjsonx_num` | `double cjsonx_num(cjsonx_val_t val)` | Retrieves the numerical value as a float. |
| `cjsonx_int` | `int64_t cjsonx_int(cjsonx_val_t val)` | Retrieves the numerical value as a 64-bit integer. |
| `cjsonx_str` | `const char* cjsonx_str(cjsonx_val_t val)` | Retrieves the string pointer. Note: zero-copy strings are **not** null-terminated — always use `cjsonx_str_len()` to bound the read. |
| `cjsonx_str_len` | `size_t cjsonx_str_len(cjsonx_val_t val)` | Returns the exact byte length of the string. |
| `cjsonx_size` | `size_t cjsonx_size(cjsonx_val_t val)` | Returns the element count of an Array or Object. |
| `cjsonx_bool` | `bool cjsonx_bool(cjsonx_val_t val)` | Retrieves the boolean value. |
| `cjsonx_is_null` | `bool cjsonx_is_null(cjsonx_val_t val)` | Returns `true` if the node is explicitly a JSON `null` or is empty/invalid. |
| `cjsonx_pointer_get` | `cjsonx_val_t cjsonx_pointer_get(cjsonx_val_t root, const char* path)` | Retrieves a node using a RFC 6901 JSON Pointer path. |

### Iteration

| Function | Signature | Description |
|---|---|---|
| `cjsonx_iter_init` | `cjsonx_iter_t cjsonx_iter_init(cjsonx_val_t val)` | Initializes a lightweight iterator for an Array or Object. |
| `cjsonx_iter_next` | `bool cjsonx_iter_next(cjsonx_iter_t* iter)` | Advances the iterator to the next element or key-value pair. |

### Mutation & Builder API

| Function | Signature | Description |
|---|---|---|
| `cjsonx_create_null` | `cjsonx_val_t cjsonx_create_null(cjsonx_doc_t* doc)` | Creates a `null` node. |
| `cjsonx_create_bool` | `cjsonx_val_t cjsonx_create_bool(cjsonx_doc_t* doc, bool val)` | Creates a boolean node. |
| `cjsonx_create_number` | `cjsonx_val_t cjsonx_create_number(cjsonx_doc_t* doc, double val)` | Creates a number node. |
| `cjsonx_create_string` | `cjsonx_val_t cjsonx_create_string(cjsonx_doc_t* doc, const char* str)` | Creates a string node (copies string to arena). |
| `cjsonx_create_object` | `cjsonx_val_t cjsonx_create_object(cjsonx_doc_t* doc)` | Creates an empty Object node. |
| `cjsonx_create_array` | `cjsonx_val_t cjsonx_create_array(cjsonx_doc_t* doc)` | Creates an empty Array node. |
| `cjsonx_object_set` | `bool cjsonx_object_set(cjsonx_val_t obj, const char* key, cjsonx_val_t val)` | Inserts or overwrites a key-value pair in an Object. |
| `cjsonx_array_push` | `bool cjsonx_array_push(cjsonx_val_t arr, cjsonx_val_t val)` | Appends a value to an Array. |
| `cjsonx_object_remove` | `bool cjsonx_object_remove(cjsonx_val_t obj, const char* key)` | Removes a key-value pair from an Object. |
| `cjsonx_array_remove` | `bool cjsonx_array_remove(cjsonx_val_t arr, size_t index)` | Removes a value at the given index from an Array. |
| `cjsonx_clone_val` | `cjsonx_val_t cjsonx_clone_val(cjsonx_doc_t* dest_doc, cjsonx_val_t src_val)` | Recursively clones a value node and its children into another document arena. |
| `cjsonx_merge_patch` | `cjsonx_val_t cjsonx_merge_patch(cjsonx_val_t target, cjsonx_val_t patch)` | Applies an RFC 7396 JSON Merge Patch to a target node. |
| `cjsonx_stringify` | `char* cjsonx_stringify(cjsonx_doc_t* doc)` | Converts document to minified JSON string (malloc'd). |
| `cjsonx_stringify_format` | `char* cjsonx_stringify_format(cjsonx_doc_t* doc, int indent)` | Converts document to pretty JSON string with indent spaces. |

### File I/O Utilities

| Function | Signature | Description |
|---|---|---|
| `cjsonx_read_file` | `cjsonx_doc_t* cjsonx_read_file(const char* path)` | Reads and parses a JSON file. |
| `cjsonx_read_file_ex` | `cjsonx_doc_t* cjsonx_read_file_ex(const char* path, cjsonx_allocator_t* alloc)` | Reads and parses a JSON file using a custom allocator. |
| `cjsonx_write_file` | `bool cjsonx_write_file(const char* path, cjsonx_doc_t* doc)` | Serializes a document to a file (minified). |
| `cjsonx_write_file_format` | `bool cjsonx_write_file_format(const char* path, cjsonx_doc_t* doc, int indent)` | Serializes a document to a file (pretty printed). |

### Type Aliases

All core types have `_t`-suffix canonical names and shorter aliases for convenience. Both forms compile identically and can be used interchangeably:

| Canonical (`_t`) | Short Alias | Description |
|---|---|---|
| `cjsonx_doc_t` | `cjsonx_doc` | Parsed document handle |
| `cjsonx_val_t` | `cjsonx_val` | Value / node handle |
| `cjsonx_iter_t` | `cjsonx_iter` | Iterator state |
| `cjsonx_type_t` | `cjsonx_type` | Node type enum |
| `cjsonx_allocator_t` | `cjsonx_alc` | Custom allocator struct |

---

## Documentation

Check out the `docs/` directory for deep-dives into the architecture and API:

- [The cjsonx Algorithm](docs/algorithm.md): Detailed explanation of the 2-stage SIMD scanning and Eisel-Lemire numerical parsing engine.
- [API Reference](docs/api_reference.md): Complete guide to all functions, structures, and memory safety guarantees.

## Examples

Runnable examples are provided in the `examples/` directory:

- **[simple_parse.c](file:///home/tiw/Public/cjsonx/examples/simple_parse.c)** — Demonstrates standard parsing, key retrieval, array iteration, and type checking using the iterator API.
- **[dom_access.c](file:///home/tiw/Public/cjsonx/examples/dom_access.c)** — Demonstrates basic JSON object parsing and index-based array access.
- **[embedded_noalloc.c](file:///home/tiw/Public/cjsonx/examples/embedded_noalloc.c)** — Demonstrates zero-allocation memory parsing using a pre-allocated static stack buffer (true zero `malloc`).
- **[error_handling.c](file:///home/tiw/Public/cjsonx/examples/error_handling.c)** — Demonstrates detailed parse error diagnostics, showing how to extract byte offset and display a code pointer to the error source.
- **[float128_precision.c](file:///home/tiw/Public/cjsonx/examples/float128_precision.c)** — Demonstrates parsing extreme, high-precision float and massive integer formats safely.

### Quick Start: Basic Parsing & Iteration

```c
#define CJSONX_IMPLEMENTATION
#include "cjsonx.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* json = "{\"name\": \"Alice\", \"skills\": [\"C\", \"SIMD\"]}";

    // Parse the JSON string
    cjsonx_doc* doc = cjsonx_parse(json, strlen(json));
    if (!doc || !doc->is_valid) {
        printf("Failed to parse JSON!\n");
        return 1;
    }

    // Retrieve name and skills
    cjsonx_val name = cjsonx_get(doc->root, "name");
    cjsonx_val skills = cjsonx_get(doc->root, "skills");

    printf("Name: %.*s\n", (int)cjsonx_str_len(name), cjsonx_str(name));

    // Iterate array using flat DOM iterator
    if (cjsonx_get_type(skills) == CJSONX_ARRAY) {
        printf("Skills:\n");
        cjsonx_iter iter = cjsonx_iter_init(skills);
        while (cjsonx_iter_next(&iter)) {
            printf("  - %.*s\n", (int)cjsonx_str_len(iter.value), cjsonx_str(iter.value));
        }
    }

    cjsonx_doc_free(doc);
    return 0;
}
```

### Quick Start: Zero-Allocation Mode (Embedded/RTOS)

```c
#define CJSONX_IMPLEMENTATION
#include "cjsonx.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* json = "{\"sensor\": \"temp\", \"value\": 24.5}";
    uint8_t static_buffer[4096]; // Static buffer on the stack (zero malloc!)

    cjsonx_doc* doc = cjsonx_parse_with_buffer(json, strlen(json), static_buffer, sizeof(static_buffer));
    if (doc && doc->is_valid) {
        cjsonx_val sensor = cjsonx_get(doc->root, "sensor");
        cjsonx_val value = cjsonx_get(doc->root, "value");

        printf("Sensor: %.*s, Value: %.1f\n", (int)cjsonx_str_len(sensor), cjsonx_str(sensor), cjsonx_num(value));
    }

    cjsonx_doc_free(doc); // No-op since we used static buffer
    return 0;
}
```

---

## Benchmark Results

Benchmarks were executed on a modern x86_64 CPU (GCC -O3 -march=native). We track **Parse Speed**, **Stringify Speed**, and the **Peak Memory** (Maximum RAM allocated during the parse operation).

> **Note on Memory**: `cjsonx` uses a Flat DOM approach with exactly 16 bytes per node. By optimizing initial node allocation capacity and performing a shrink-to-fit step at the end of parsing, `cjsonx` now achieves the lowest peak memory usage among tested libraries while maintaining high parsing throughput.

### 1. `twitter.json` (0.60 MB)

| Library | Parse (MB/s) | Stringify (MB/s) | Peak Mem (MB) |
|---------|--------------|------------------|---------------|
| **cjsonx** | 618.41 | 1611.66 | **0.92** |
| yyjson | **826.85** | **3773.87** | 1.20 |
| cJSON | 295.29 | 467.68 | 1.23 |

### 2. `citm_catalog.json` (1.65 MB)

| Library | Parse (MB/s) | Stringify (MB/s) | Peak Mem (MB) |
|---------|--------------|------------------|---------------|
| **cjsonx** | **1169.02** | 2110.53 | **2.07** |
| yyjson | 889.20 | **6780.16** | 3.29 |
| cJSON | 292.06 | 836.49 | 2.57 |

### 3. `canada.json` (2.15 MB) - Heavy Floating-Point Arrays

| Library | Parse (MB/s) | Stringify (MB/s) | Peak Mem (MB) |
|---------|--------------|------------------|---------------|
| **cjsonx** | 304.95 | 277.87 | **4.70** |
| yyjson | **800.68** | **614.18** | 7.87 |
| cJSON | 72.36 | 25.21 | 10.20 |

<details>
<summary><b>View raw console output from bench_compare</b></summary>

```console
tiw@tiw-CachyOS ~/Public/cjsonx (master)
❯ ./build/bench_compare benchmarks/datasets/citm_catalog.json && ./build/bench_compare benchmarks/datasets/twitter.json && ./build/bench_compare benchmarks/datasets/canada.json

Dataset: benchmarks/datasets/citm_catalog.json (1.65 MB)
========================================================================
Library    | Parse (MB/s)    | Stringify (MB/s) | Peak Mem (MB)
-----------|-----------------|------------------|-----------------------
cjsonx     | 1169.02         | 2110.53         | 2.07
yyjson     | 889.20          | 6780.16         | 3.29
cJSON      | 292.06          | 836.49          | 2.57
========================================================================
Dataset: benchmarks/datasets/twitter.json (0.60 MB)
========================================================================
Library    | Parse (MB/s)    | Stringify (MB/s) | Peak Mem (MB)
-----------|-----------------|------------------|-----------------------
cjsonx     | 618.41          | 1611.66         | 0.92
yyjson     | 826.85          | 3773.87         | 1.20
cJSON      | 295.29          | 467.68          | 1.23
========================================================================
Dataset: benchmarks/datasets/canada.json (2.15 MB)
========================================================================
Library    | Parse (MB/s)    | Stringify (MB/s) | Peak Mem (MB)
-----------|-----------------|------------------|-----------------------
cjsonx     | 304.95          | 277.87          | 4.70
yyjson     | 800.68          | 614.18          | 7.87
cJSON      | 72.36           | 25.21           | 10.20
========================================================================

tiw@tiw-CachyOS ~/Public/cjsonx (master)
❯
```

</details>

### Analysis

`cjsonx` demonstrates significant parsing throughput on large payloads, measuring up to **1169.02 MB/s** on `citm_catalog.json`. This provides a performance profile comparable to, and often exceeding, modern parsers like `yyjson` during tree construction, while dramatically outperforming legacy standards like `cJSON` in computational speed and maintaining the lowest peak memory overhead.

---

## Development Methodology & AI Assistance

Building a memory-safe, SIMD-accelerated C parser from scratch involves handling incredibly complex edge cases—from vectorized bit-masking to IEEE 754 catastrophic cancellation bounds.

To achieve this level of stability and performance within a short timeframe, this project was architected and rigorously verified in collaboration with **Advanced Agentic AI**. AI was specifically utilized to:

- Stress-test the Eisel-Lemire numerical engine against extreme floating-point edge cases and LibFuzzer.
- Assist in planning the memory layout and cache-locality of the 16-byte arena DOM.
- Automate the generation of robust cross-platform CI/CD pipelines (Linux, macOS, Windows, WASM, ClusterFuzzLite).

However, **human agency remains at the core of this project**. Every single line of code generated or suggested was manually inspected, audited, and strictly verified. The core architecture, algorithms, and memory design were meticulously human-planned. This hybrid approach—combining human architectural vision with AI-driven debugging and verification—allowed us to push the boundaries of performance and reliability in a modern C library without compromising security or code ownership.

---

## Author's Note

I'm just a kid building projects as a hobby. Thank you for showing interest in my little library! It really means a lot to me. :)

---

## License

This project is licensed under the [MIT License](LICENSE) - see the [LICENSE](LICENSE) file for details.
