/**
 * @file cjsonx.h
 * @brief public api and single-header entry point
 *
 * @note i'm just a kid building projects as a hobby. thank you for showing interest in my little library! it really means a lot to me. :)
 * @note architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
/*
 * cjsonx.h -- extreme performance json parser for c11/c++
 * project url: https://github.com/tiw302/cjsonx
 *
 * how to use:
 * 1. include this file normally in headers to get types and api declarations.
 * 2. in exactly one c/c++ source file, define the implementation macro:
 *    #define cjsonx_implementation
 *    #include <cjsonx.h>
 *
 * technical background:
 * - two-stage parsing algorithm inspired by simdjson.
 *   - stage 1: structural indexing (finding {}[]:,")
 *   - stage 2: recursive descent parsing (building dom via arena allocator)
 *
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
#define CJSONX_VERSION_MINOR 0
#define CJSONX_VERSION_PATCH 0

// convenience macro for parsing null-terminated strings without specifying length
#define cjsonx_parse_str(json) cjsonx_parse((json), strlen(json))

// internal headers (order matters: config → error → dom → tape → arena)
#include "cjsonx_config.h"
#include "cjsonx_error.h"
#include "cjsonx_dom.h"
#include "cjsonx_tape.h"
#include "cjsonx_arena.h"
#include "cjsonx_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

// stage 1: scan json to build structural token tape
bool cjsonx_stage1_build_tape(const char* json, size_t length, cjsonx_tape_t* tape);

// main parser entry point
cjsonx_doc_t* cjsonx_parse(const char* json, size_t length);
cjsonx_doc_t* cjsonx_parse_ex(const char* json, size_t length, cjsonx_allocator_t* alloc);

#ifdef __cplusplus
}
#endif

// implementation guard: include stage1 & stage2 source only once
#ifdef CJSONX_IMPLEMENTATION
#ifndef CJSONX_IMPLEMENTED
#define CJSONX_IMPLEMENTED
#include "cjsonx_stage1.h"
#include "cjsonx_stage2.h"
#endif
#endif

#endif  // cjsonx_h
