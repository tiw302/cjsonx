/**
 * @file cjsonx_string.h
 * @brief JSON string parser with escape and UTF-8 handling
 *
 * @note Architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
#ifndef CJSONX_STRING_H
#define CJSONX_STRING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "cjsonx_utf8.h"

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
static inline bool cjsonx_parse_string_impl(cjsonx_doc_t* doc, cjsonx_node_t* out_node, const char* json, uint32_t start_pos, uint32_t end_pos) {
    size_t len = end_pos - start_pos - 1;
    const char* str_start = json + start_pos + 1;

    // strict utf-8 and escape validation on raw string
    size_t i = 0;
    bool has_escape = false;
    bool has_non_ascii = false;

#ifdef __AVX2__
    __m256i escape_char = _mm256_set1_epi8('\\');
    for (; i + 32 <= len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(str_start + i));
        
        // check for non-ascii (highest bit set)
        if (!_mm256_testz_si256(chunk, _mm256_set1_epi8((char)0x80))) {
            has_non_ascii = true;
        }
        
        // check for escape character
        __m256i cmp = _mm256_cmpeq_epi8(chunk, escape_char);
        if (!_mm256_testz_si256(cmp, cmp)) {
            has_escape = true;
            break; // found escape, we must use the slow path.
        }
    }
#endif

    // scalar fallback for remaining bytes or if avx2 is disabled
    uint64_t mask = 0;
    for (; i + 8 <= len; i += 8) {
        uint64_t chunk;
        memcpy(&chunk, str_start + i, 8);
        mask |= chunk;
        if (!has_escape && memchr(str_start + i, '\\', 8)) has_escape = true;
    }
    for (; i < len; i++) {
        mask |= (uint8_t)str_start[i];
        if (!has_escape && str_start[i] == '\\') has_escape = true;
    }
    if (mask & 0x8080808080808080ULL) has_non_ascii = true;
    
    if (has_non_ascii) {
        uint32_t state = CJSONX_UTF8_ACCEPT;
        uint32_t codep;
        for (size_t j = 0; j < len; j++) {
            cjsonx_utf8_decode(&state, &codep, (uint8_t)str_start[j]);
            if (state == CJSONX_UTF8_REJECT) { doc->error = CJSONX_ERROR_INVALID_UTF8; return false; }
        }
        if (state != CJSONX_UTF8_ACCEPT) { doc->error = CJSONX_ERROR_INVALID_UTF8; return false; }
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

#endif // CJSONX_STRING_H
