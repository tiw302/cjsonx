// updated 2026-06-13
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_STAGE1_H
#define CJSONX_STAGE1_H

// ███████ ████████  █████   ██████  ███████      ██
// ██         ██    ██   ██ ██       ██          ███
// ███████    ██    ███████ ██   ███ █████        ██
//      ██    ██    ██   ██ ██    ██ ██           ██
// ███████    ██    ██   ██  ██████  ███████      ██
//
// >>stage 1 structural indexing


#include <stdlib.h>

// detect architecture and select best available backend.
// note: if you need to support runtime dispatch (e.g. avx2 on modern cpu, fallback to scalar on old),
// you should build multiple objects and dispatch at runtime via cpuid, rather than compile-time macros.
#if defined(__AVX2__)
    #include "cjsonx_backends/cjsonx_avx2.h"
#elif defined(__ARM_NEON)
    #include "cjsonx_backends/cjsonx_neon.h"
#elif defined(__wasm_simd128__)
    #include "cjsonx_backends/cjsonx_wasm.h"
#else
    #include "cjsonx_backends/cjsonx_scalar.h"
#endif

#ifdef CJSONX_IMPLEMENTATION
// dispatches to the correct stage 1 implementation based on the detected architecture.
bool cjsonx_stage1_build_tape(const char* json, size_t length, cjsonx_tape_t* tape) {
#if defined(__AVX2__)
    return cjsonx_stage1_avx2(json, length, tape);
#elif defined(__ARM_NEON)
    return cjsonx_stage1_neon(json, length, tape);
#elif defined(__wasm_simd128__)
    return cjsonx_stage1_wasm(json, length, tape);
#else
    return cjsonx_stage1_scalar(json, length, tape);
#endif
}
#endif

#endif  // cjsonx_stage1_h
