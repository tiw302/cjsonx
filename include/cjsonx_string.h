/**
 * @file cjsonx_string.h
 * @brief JSON string parser with escape and UTF-8 handling
 *
 * @note Architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
#ifndef CJSONX_STRING_H
#define CJSONX_STRING_H

/*==============================================================================
 * MARK: - string processing
 *============================================================================*/


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "cjsonx_utf8.h"

#ifdef __cplusplus
extern "C" {
#endif

// parse 4-digit hex escape (\uxxxx) to codepoint
static inline bool cjsonx_parse_hex4(const char* p, uint32_t* out) {
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        else return false;
    }
    *out = val;
    return true;
}

// encode unicode codepoint to utf-8 bytes, returns byte count
static inline size_t cjsonx_encode_utf8(uint32_t cp, char* out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    else if (cp < 0x800) { out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    else if (cp < 0x10000) { out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
    else if (cp <= 0x10FFFF) { out[0] = (char)(0xF0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); out[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F)); return 4; }
    return 0;
}

// flat string parser: writes directly to out_node
static cjsonx_always_inline bool cjsonx_parse_string_impl(cjsonx_doc_t* doc, cjsonx_node_t* out_node, const char* json, uint32_t start_pos, uint32_t end_pos) {
    size_t len = end_pos - start_pos - 1;
    const char* str_start = json + start_pos + 1;

    // strict utf-8 and escape validation on raw string
    size_t i = 0;
    bool has_escape = false;
    bool has_non_ascii = false;
    bool has_control = false;

#if defined(__AVX2__)
    __m256i escape_char = _mm256_set1_epi8('\\');
    for (; i + 32 <= len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(str_start + i));
        
        // check for non-ascii (highest bit set)
        if (CJSONX_UNLIKELY(!_mm256_testz_si256(chunk, _mm256_set1_epi8((char)0x80)))) {
            has_non_ascii = true;
        }
        
        // check for escape character
        __m256i cmp_esc = _mm256_cmpeq_epi8(chunk, escape_char);
        
        // check for control char (< 0x20)
        __m256i cmp_ctrl = _mm256_cmpeq_epi8(_mm256_subs_epu8(chunk, _mm256_set1_epi8(0x1F)), _mm256_setzero_si256());
        
        __m256i bad = _mm256_or_si256(cmp_esc, cmp_ctrl);
        if (CJSONX_UNLIKELY(!_mm256_testz_si256(bad, bad))) {
            if (!_mm256_testz_si256(cmp_ctrl, cmp_ctrl)) has_control = true;
            has_escape = true;
            break; // found escape or control, break to handle it
        }
    }
#elif defined(__ARM_NEON)
    uint8x16_t escape_char = vdupq_n_u8('\\');
    uint8x16_t ctrl_limit = vdupq_n_u8(0x20);
    for (; i + 16 <= len; i += 16) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*)(str_start + i));
        
        // check for non-ascii (signed < 0)
        uint8x16_t non_ascii = vcltq_s8(vreinterpretq_s8_u8(chunk), vdupq_n_s8(0));
        
        // check for escape character
        uint8x16_t cmp_esc = vceqq_u8(chunk, escape_char);
        
        // check for control char (< 0x20)
        uint8x16_t cmp_ctrl = vcltq_u8(chunk, ctrl_limit);
        
        uint8x16_t bad = vorrq_u8(cmp_esc, cmp_ctrl);
        
        // bitwise OR across vector to check if any condition matched
        uint32x4_t bad_u32 = vreinterpretq_u32_u8(bad);
        uint32x4_t non_ascii_u32 = vreinterpretq_u32_u8(non_ascii);
        
        if (CJSONX_UNLIKELY(vmaxvq_u32(bad_u32) != 0)) {
            if (vmaxvq_u32(vreinterpretq_u32_u8(cmp_ctrl)) != 0) has_control = true;
            has_escape = true;
            break;
        }
        if (CJSONX_UNLIKELY(vmaxvq_u32(non_ascii_u32) != 0)) {
            has_non_ascii = true;
        }
    }
#elif defined(__wasm_simd128__)
    v128_t escape_char = wasm_i8x16_splat('\\');
    v128_t ctrl_limit = wasm_i8x16_splat(0x20);
    v128_t zero = wasm_i8x16_splat(0);
    for (; i + 16 <= len; i += 16) {
        v128_t chunk = wasm_v128_load((const v128_t*)(str_start + i));
        
        // check for non-ascii (signed < 0)
        v128_t non_ascii = wasm_i8x16_lt(chunk, zero);
        
        v128_t cmp_esc = wasm_i8x16_eq(chunk, escape_char);
        v128_t cmp_ctrl = wasm_u8x16_lt(chunk, ctrl_limit);
        
        v128_t bad = wasm_v128_or(cmp_esc, cmp_ctrl);
        
        if (CJSONX_UNLIKELY(wasm_v128_any_true(bad))) {
            if (wasm_v128_any_true(cmp_ctrl)) has_control = true;
            has_escape = true;
            break;
        }
        if (CJSONX_UNLIKELY(wasm_v128_any_true(non_ascii))) {
            has_non_ascii = true;
        }
    }
#endif

    // scalar fallback for remaining bytes or if simd is disabled
    uint64_t mask = 0;
    for (; i + 8 <= len; i += 8) {
        uint64_t chunk;
        memcpy(&chunk, str_start + i, 8);
        mask |= chunk;
        if (!has_escape) {
            uint64_t x = chunk ^ 0x5C5C5C5C5C5C5C5CULL;
            if (CJSONX_UNLIKELY((x - 0x0101010101010101ULL) & ~x & 0x8080808080808080ULL)) {
                has_escape = true;
            }
            // check for control char (< 0x20)
            uint64_t msb = chunk & 0x8080808080808080ULL;
            uint64_t no_msb = chunk ^ msb;
            uint64_t t = no_msb + 0x6060606060606060ULL;
            if (CJSONX_UNLIKELY((~t & ~msb) & 0x8080808080808080ULL)) {
                 has_control = true;
                 has_escape = true;
            }
        }
    }
    for (; i < len; i++) {
        mask |= (uint8_t)str_start[i];
        if (CJSONX_UNLIKELY(str_start[i] == '\\')) has_escape = true;
        if (CJSONX_UNLIKELY((unsigned char)str_start[i] < 0x20)) { has_control = true; has_escape = true; }
    }
    if (CJSONX_UNLIKELY(mask & 0x8080808080808080ULL)) has_non_ascii = true;
    
    if (CJSONX_UNLIKELY(has_control)) {
        doc->error = CJSONX_ERROR_INVALID_CONTROL_CHAR;
        return false;
    }

    if (CJSONX_UNLIKELY(has_non_ascii)) {
        uint32_t state = CJSONX_UTF8_ACCEPT;
        uint32_t codep;
        for (size_t j = 0; j < len; j++) {
            cjsonx_utf8_decode(&state, &codep, (uint8_t)str_start[j]);
            if (CJSONX_UNLIKELY(state == CJSONX_UTF8_REJECT)) { doc->error = CJSONX_ERROR_INVALID_UTF8; return false; }
        }
        if (CJSONX_UNLIKELY(state != CJSONX_UTF8_ACCEPT)) { doc->error = CJSONX_ERROR_INVALID_UTF8; return false; }
    }

    // zero-copy fast path
    if (!has_escape) {
        cjsonx_node_set_type_len(out_node, CJSONX_STRING, (uint32_t)len);
        out_node->val.str = str_start;
        return true;
    }

    // slow path with arena allocation
    char* out = (char*)cjsonx_arena_alloc(doc, len + 1);
    if (!out) return false;

    const char* p = str_start;
    const char* end = str_start + len;
    char* d = out;

    while (p < end) {
        if (*p == '\\') {
            p++;
            if (p >= end) return false;
            switch (*p) {
                case '"': *d++ = '"'; p++; break;
                case '\\': *d++ = '\\'; p++; break;
                case '/': *d++ = '/'; p++; break;
                case 'b': *d++ = '\b'; p++; break;
                case 'f': *d++ = '\f'; p++; break;
                case 'n': *d++ = '\n'; p++; break;
                case 'r': *d++ = '\r'; p++; break;
                case 't': *d++ = '\t'; p++; break;
                case 'u': {
                    p++;
                    if (p + 4 > end) return false;
                    uint32_t cp = 0;
                    if (!cjsonx_parse_hex4(p, &cp)) return false;
                    p += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (p + 6 > end || p[0] != '\\' || p[1] != 'u') return false;
                        uint32_t cp2 = 0;
                        if (!cjsonx_parse_hex4(p + 2, &cp2)) return false;
                        if (cp2 < 0xDC00 || cp2 > 0xDFFF) return false;
                        cp = (((cp - 0xD800) << 10) | (cp2 - 0xDC00)) + 0x10000;
                        p += 6;
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        // lone low surrogate is invalid
                        return false;
                    }
                    size_t enc_len = cjsonx_encode_utf8(cp, d);
                    if (enc_len == 0) return false;
                    d += enc_len;
                    break;
                }
                default: return false;
            }
        } else {
            *d++ = *p++;
        }
    }
    *d = '\0';
    cjsonx_node_set_type_len(out_node, CJSONX_STRING, (uint32_t)(d - out));
    out_node->val.str = out;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif // CJSONX_STRING_H
