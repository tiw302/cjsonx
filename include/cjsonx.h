/*
 * cjsonx.h -- high-performance json parser for c11.
 * project url: https://github.com/tiw302/cjsonx
 * do this:
 * #define CJSONX_IMPLEMENTATION
 * before you include this file in *one* c or c++ file to create the
 * implementation.
 * technical background:
 * ---------------------
 * this library uses a two-stage parsing algorithm inspired by simdjson.
 * stage 1 builds the tape (indices) via simd structure scan, and stage 2
 * parses into a 16-byte fixed node arena dom.
 * memory:
 * -------
 * uses cjsonx_arena. for real zero-alloc, use cjsonx_parse_with_buffer.
 * performance:
 * ------------
 * cjsonx_array_push is o(n) because it walks the siblings.
 * use the builder api for large arrays.
 * conformance:
 * ------------
 * keep JSONTestSuite passing. don't break it.
 * simd optimization:
 * ------------------
 * we've got backends for pretty much everything:
 * - avx2:     x86_64 modern (haswell+, ryzen+)
 * - neon:     arm64 (apple silicon, graviton, android)
 * - wasm:     webassembly with simd128
 * - scalar:   fallback for everything else (risc-v, ppc, etc.)
 * license:
 * --------
 * mit license
 * copyright (c) 2026 jirawat siripuk
 */

#ifndef CJSONX_H
#define CJSONX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// version macros
#define CJSONX_VERSION_MAJOR 1
#define CJSONX_VERSION_MINOR 1
#define CJSONX_VERSION_PATCH 1
#define CJSONX_VERSION_STRING "1.1.1"


// internal headers (order matters: config → error → dom → tape → arena)
#include "cjsonx_config.h"
#include "cjsonx_error.h"
#include "cjsonx_dom.h"
#include "cjsonx_tape.h"
#include "cjsonx_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

// stage 1: scan json to build structural token tape
// dev note: keeping this decoupled from stage 2 makes testing and benchmarking incredibly easy.
CJSONX_API bool cjsonx_stage1_build_tape(const char* json, size_t length, cjsonx_tape_t* tape);

// main parser entry point
// warning: the input json buffer must outlive the returned document (zero-copy string references).
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse(const char* json, size_t length);
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse_ex(const char* json, size_t length, cjsonx_allocator_t* alloc);

// parse a null-terminated string safely without double evaluation
static inline CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse_cstr(const char* json) {
    return cjsonx_parse(json, json ? strlen(json) : 0);
}
#define cjsonx_parse_str(json) cjsonx_parse_cstr(json)

#ifdef __cplusplus
}
#endif

// builder included last so it can see cjsonx_parse_ex declarations
#include "cjsonx_builder.h"

// implementation guard: include stage1 & stage2 source only once
#ifdef CJSONX_IMPLEMENTATION
#ifndef CJSONX_IMPLEMENTED
#define CJSONX_IMPLEMENTED
#include "cjsonx_stage1.h"
#include "cjsonx_stage2.h"
#endif
#endif

#endif  // cjsonx_h
