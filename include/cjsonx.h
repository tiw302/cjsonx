/**
 * @file cjsonx.h
 * @brief Public API and single-header entry point
 *
 * @note I'm just a kid building projects as a hobby. Thank you for showing interest in my little library! It really means a lot to me. :)
 * @note Architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
/*
 * cjsonx.h -- Extreme performance JSON parser for C11/C++
 * Project URL: https://github.com/tiw302/cjsonx
 *
 * How to use:
 * 1. Include this file normally in headers to get types and API declarations.
 * 2. In exactly one C/C++ source file, define the implementation macro:
 *    #define cjsonx_implementation
 *    #include <cjsonx.h>
 *
 * Technical background:
 * - Two-stage parsing algorithm inspired by simdjson.
 *   - Stage 1: Structural indexing (finding {}[]:,")
 *   - Stage 2: Recursive descent parsing (building DOM via arena allocator)
 *
 * MIT License
 * Copyright (c) 2026 Jirawat Siripuk
 */

#ifndef CJSONX_H
#define CJSONX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Version macros
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

#endif  // CJSONX_H
