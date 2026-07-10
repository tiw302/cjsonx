# changelog

all notable changes to this project will be documented in this file.

format follows [keep a changelog](https://keepachangelog.com/en/1.0.0/).
versioning follows [semantic versioning](https://semver.org/).

---

## [unreleased]

---

## [v1.4.1] — 2026-07-10

### Fixed
- **c++ wrapper**: removed `std::string_view` from `cjsonx.hpp` to ensure strict c++11 compatibility, fixing compilation errors on strict c++11 toolchains
- **rust bindings**: fixed `cargo package` failing to include c source files on `crates.io` by copying them into the crate directory during ci publishing

---

## [v1.4.0] — 2026-07-10

### Added
- **C++ Wrapper Enhancements**: Upgraded `cjsonx.hpp` to support modern C++11 Range-based Iterators for Arrays and Objects (`for (auto kv : obj)`).
- **C++ JSON Pointers**: Added `Node::pointer()` method for deep DOM querying using RFC 6901 in C++.
- **C++ Example**: Added `examples/cpp/iteration_and_pointers.cpp` to demonstrate the new RAII and Iterator features.
- **System Installation**: Added missing `install()` targets to `CMakeLists.txt` to support `sudo cmake --install build` and `find_package(cjsonx)` integration.

### Fixed
- **Broken Example Builds**: Fixed `examples/CMakeLists.txt`, `build.sh`, and `build.bat` which were failing to locate the `.c` and `.cpp` example files after the recent directory restructuring.

### Changed
- **Benchmark Scripts**: Improved safety of benchmark download scripts by adding `set -e` and `curl -f`, and added professional header comments to all `.c` benchmark files.

### Documentation
- **Project Structure**: Added comprehensive directory tree diagrams to both `README.md` and MkDocs to improve developer onboarding.
- **Docs Synchronization**: Synced `docs/index.md` with GitHub's `README.md`, porting over the Rust/Cargo guides, community links, and testing instructions.
- **AI Methodology**: Rewrote and upgraded the "Development Methodology & AI Assistance" section to highlight cross-platform CI, Eisel-Lemire stress-tests, and zero-cost language bindings.
- **UI/UX Cleanup**: Wrapped all language-specific examples in `README.md` into clean, collapsible `<details>` blocks.
- **C++ Docs**: Added the new `cjsonx.hpp` wrapper API documentation into `docs/api_reference.md`.

---

## [v1.3.0] — 2026-07-10

### added
- python bindings (`pip install cjsonx`) with full dom access via `[]`, `.get()`, and `.pointer()`
- javascript / webassembly bindings (`npm install @tiw302/cjsonx`) with `getRoot()`, `.get()`, `.getIndex()`, `.toJS()`, and `.pointer()`
- `CJSONX_ERROR_TOO_LARGE` error code for inputs exceeding node or string size limits
- `cjsonx_valid()` helper to check whether a parsed document is valid without accessing `is_valid` directly
- `cjsonx_parse_copy`, `cjsonx_parse_copy_ex`, `cjsonx_parse_copy_cstr` — owned-copy parsing variants that internalize the json buffer so callers can free the source immediately
- ci workflows for automated pypi and npm publishing on github releases
- ci workflow to release amalgamated single-header alongside each github release
- js/wasm test suite (`tests/test_js.js`)

### fixed
- **use-after-free in python bindings** — `PyDocument` now retains an internal copy of the json string; previously the binding held a reference to a potentially freed buffer
- **use-after-free in js/wasm wrapper** — the wasm side now clones the input string into wasm heap before returning; the js caller can safely GC the original
- **24-bit string size limit** not enforced at parse time — strings larger than `0xFFFFFF` bytes are now rejected with `CJSONX_ERROR_TOO_LARGE`
- **24-bit container size limit** not enforced — objects and arrays with more than `0xFFFFFF` children are now rejected with `CJSONX_ERROR_TOO_LARGE`
- **emscripten locale fallback** — `strtod` locale guard now compiles correctly under Emscripten where `uselocale` is unavailable
- **null key undefined behavior** in `cjsonx_get_len`, `cjsonx_object_set_len`, and `cjsonx_object_remove_len` — a `NULL` key with `key_len = 0` previously triggered `memcmp(ptr, NULL, 0)` which is ub in strict c; now safely treated as an empty key
- **redundant node lookup in parser hot loop** — `comma` case in stage 2 previously re-dereferenced `doc->nodes[parent_stack[...]]` to get the parent type; now reads directly from the pre-populated `parent_type_stack`, eliminating a potential cache miss

### changed
- `cjsonx_array_push` is `O(1)` — was previously documented as `O(n)` but the implementation already used the `last_child` pointer for constant-time appends; comment corrected

---

## [v1.2.4] — 2026-07-09

### fixed
- **python**: restore `setup.py` and update package versions

---

## [v1.2.3] — 2026-07-09

### changed
- **float**: optimize Eisel-Lemire bounds and locale initialization
- **ci**: skip 32-bit wheel builds, add verbosity and timeout

### fixed
- **msvc**: add `CJSONX_CLZLL` to `config.h` to fix `LNK2019 __builtin_clzll` error

---

## [v1.2.2] — 2026-07-09

### changed
- minor chores and style updates for test comments

---

## [v1.2.1] — 2026-07-08

### fixed
- **stage2**: resolve MSVC `C2374` val redefinition
- **macos**: fix `strtod_l` undeclared error

---

## [v1.2.0] — 2026-07-08

### changed
- _Note: Major features like Python/JS bindings, `cjsonx_parse_copy`, and size limit enforcements were introduced during the 1.2.x cycle but are fully documented as part of the v1.3.0 release above._

---

## [v1.1.1] — 2026-06-23

### fixed
- **msvc lnk2019** linker error when building on windows — missing symbol resolved
- **wasm ci** pipeline failure — emscripten path configuration corrected

---

## [v1.1.0] — 2026-06-23

### added
- asan / ubsan ci workflow (`sanitizers.yml`) — runs on every push and pull request
- wasm-simd128 ci workflow with smoke tests (`wasm.yml`)
- risc-v qemu testing in the linux ci pipeline
- benchmark step in ci reporting parse throughput for every commit
- `builder_api` usage example

### changed
- cmake build system fully modularized — tests, benchmarks, and examples each have their own `CMakeLists.txt`
- `build.sh` rewritten with an interactive menu and color output
- wasm backend cleaned up — obsolete build scripts removed

### fixed
- **unescaped control characters** (`< 0x20`) inside json strings were not rejected by the string unescaper in all paths; now consistently validated and rejected with `CJSONX_ERROR_INVALID_STRING`
- **`O(n)` array and object appends in builder api** — `cjsonx_array_push` and `cjsonx_object_set` were walking the sibling chain to find the tail; refactored to use the `last_child` pointer for true `O(1)` appends

---

## [v1.0.3] — 2026-06-18

### added
- `test_static_buffer` — conformance tests for zero-allocation `cjsonx_parse_with_buffer` mode

### fixed
- **`strtoul` overflow** — parsing a number string longer than 19 digits no longer silently wraps; falls back to standard `strtod`
- **static buffer capacity** — `cjsonx_parse_with_buffer` now correctly rejects inputs that would overflow the user-provided arena instead of writing past the end

### changed
- neon and avx2 stage 1 backends: clarified in comments that control character validation happens in stage 1, not stage 2

---

## [v1.0.2] — 2026-06-17

### added
- `cjsonx_get_len` — length-based key lookup to avoid requiring null-terminated key strings
- `cjsonx_object_set_len` and `cjsonx_object_remove_len` — length-based mutation variants

### fixed
- **object clone / merge-patch** — cloning an object with an empty string key (`""`) caused a crash in the builder; regression test added
- **builder object set** — `cjsonx_object_set` now uses length-based comparison throughout, correctly handling keys with embedded null bytes

---

## [v1.0.1] — 2026-06-16

### fixed
- **stage 2 node array overflow** — parsing deeply nested or very large documents no longer writes past the initial node capacity; the array now grows dynamically via `realloc`
- **unclosed containers** — unterminated objects `{` and arrays `[` now correctly return `CJSONX_ERROR_UNEXPECTED_END` instead of producing a partially constructed document

---

## [v1.0.0] — 2026-06-16

### added
- initial public release
- two-stage json parser: simd stage 1 (avx2 / neon / wasm-simd128 / scalar) + computed-goto stage 2 dom builder
- 16-byte flat-arena dom (`cjsonx_node_t`) — 4× smaller per-node than cjson
- eisel-lemire algorithm for ieee 754 double parsing
- zero-allocation parsing via `cjsonx_parse_with_buffer`
- mutable builder api: `cjsonx_array_push`, `cjsonx_object_set`, `cjsonx_object_remove`, `cjsonx_merge_patch`
- json pointer support (`cjsonx_pointer_get`)
- dfa-based utf-8 validation on string tokens
- `cjsonx_stringify` for serializing the dom back to json text
- single-header distribution (`single_include/cjsonx.h`)
- jsontestsuite conformance: 95 valid cases pass, 188 invalid cases correctly rejected
- mit license

[unreleased]: https://github.com/tiw302/cjsonx/compare/v1.4.1...HEAD
[v1.4.1]: https://github.com/tiw302/cjsonx/compare/v1.4.0...v1.4.1
[v1.4.0]: https://github.com/tiw302/cjsonx/compare/v1.3.0...v1.4.0
[v1.3.0]: https://github.com/tiw302/cjsonx/compare/v1.2.4...v1.3.0
[v1.2.4]: https://github.com/tiw302/cjsonx/compare/v1.2.3...v1.2.4
[v1.2.3]: https://github.com/tiw302/cjsonx/compare/v1.2.2...v1.2.3
[v1.2.2]: https://github.com/tiw302/cjsonx/compare/v1.2.1...v1.2.2
[v1.2.1]: https://github.com/tiw302/cjsonx/compare/v1.2.0...v1.2.1
[v1.2.0]: https://github.com/tiw302/cjsonx/compare/v1.1.1...v1.2.0
[v1.1.1]: https://github.com/tiw302/cjsonx/compare/v1.1.0...v1.1.1
[v1.1.0]: https://github.com/tiw302/cjsonx/compare/v1.0.3...v1.1.0
[v1.0.3]: https://github.com/tiw302/cjsonx/compare/v1.0.2...v1.0.3
[v1.0.2]: https://github.com/tiw302/cjsonx/compare/v1.0.1...v1.0.2
[v1.0.1]: https://github.com/tiw302/cjsonx/compare/v1.0.0...v1.0.1
[v1.0.0]: https://github.com/tiw302/cjsonx/releases/tag/v1.0.0
