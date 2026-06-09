/**
 * @file cjsonx_scalar.h
 * @brief Stage 1 structural indexer — portable scalar fallback
 *
 * @note Architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
#ifndef CJSONX_SCALAR_H
#define CJSONX_SCALAR_H

/*==============================================================================
 * MARK: - scalar backend
 *============================================================================*/


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// character classification helpers for stage 1 scanner

static inline bool cjsonx_is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline bool cjsonx_is_structural(char c) {
    return c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',' || c == '"';
}

/*
 * stage 1 scalar scanner: linear byte-by-byte pass over the json input.
 *
 * builds a "tape" of byte-offsets for every structural character ({, }, [, ], :, , ")
 * and the first byte of each primitive token (numbers, true/false/null).
 * handles string escaping to avoid indexing characters inside strings.
 *
 * returns false on:
 *   - raw control characters inside strings (< 0x20)
 *   - unterminated strings
 *   - empty input (no structural tokens found)
 *   - out of memory during tape growth
 */
static inline bool cjsonx_stage1_scalar(const char* json, size_t length, cjsonx_tape_t* tape) {
    bool in_string = false;
    bool escaped = false;
    bool prev_was_sep = true; // true after whitespace/structural — used to detect start of primitive tokens

    for (size_t i = 0; i < length; i++) {
        char c = json[i];
        
        if (in_string) {
            // no raw control chars allowed in strings, rfc8259 says so
            if ((unsigned char)c < 0x20) {
                return false; 
            }
            
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
                if (!cjsonx_tape_push(tape, (uint32_t)i)) {
                    return false;
                }
                prev_was_sep = true;
                continue;
            }
            prev_was_sep = false;
        } else {
            if (c == '"') {
                in_string = true;
                if (!cjsonx_tape_push(tape, (uint32_t)i)) {
                    return false;
                }
                prev_was_sep = true;
            } else if (cjsonx_is_structural(c)) {
                if (!cjsonx_tape_push(tape, (uint32_t)i)) {
                    return false;
                }
                prev_was_sep = true;
            } else if (!cjsonx_is_whitespace(c)) {
                // start of a primitive (number, true/false/null). only store the first byte.
                if (prev_was_sep) {
                    if (!cjsonx_tape_push(tape, (uint32_t)i)) {
                        return false; 
                    }
                }
                prev_was_sep = false;
            } else {
                // whitespace marks a token boundary
                prev_was_sep = true;
            }
        }
    }
    
    // unterminated string error
    if (in_string) {
        return false; 
    }
    
    // empty json is invalid
    if (tape->count == 0) {
        return false;
    }
    
    return true;
}

#endif // CJSONX_SCALAR_H
