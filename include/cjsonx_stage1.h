/**
 * @file cjsonx_stage1.h
 * @brief Stage 1 backend dispatcher (AVX2 / NEON / WASM / scalar)
 *
 * @note Architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
#ifndef CJSONX_STAGE1_H
#define CJSONX_STAGE1_H

#include <stdlib.h>

// detect architecture and select best available backend
#if defined(__AVX2__)
    #include "cjsonx_backends/cjsonx_avx2.h"
    #define cjsonx_stage1_build_tape cjsonx_stage1_avx2

#elif defined(__ARM_NEON)
    #include "cjsonx_backends/cjsonx_neon.h"
    #define cjsonx_stage1_build_tape cjsonx_stage1_neon


#elif defined(__wasm_simd128__)
    #include "cjsonx_backends/cjsonx_wasm.h"
    #define cjsonx_stage1_build_tape cjsonx_stage1_wasm

#else
    #include "cjsonx_backends/cjsonx_scalar.h"
    #define cjsonx_stage1_build_tape cjsonx_stage1_scalar
#endif

#endif  // CJSONX_STAGE1_H
