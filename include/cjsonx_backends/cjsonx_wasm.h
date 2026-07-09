// updated 2026-06-23
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_WASM_H
#define CJSONX_WASM_H

// ██     ██  █████  ███████ ███    ███
// ██     ██ ██   ██ ██      ████  ████
// ██  █  ██ ███████ ███████ ██ ████ ██
// ██ ███ ██ ██   ██      ██ ██  ██  ██
//  ███ ███  ██   ██ ███████ ██      ██
//
// >>wasm simd backend


#include <wasm_simd128.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * stage 1 webassembly simd128 scanner:
 * processes 32 bytes of json input per iteration using two 16-byte v128_t vectors.
 * 
 * 1. character comparison: uses wasm_i8x16_eq to compare lanes in parallel for
 *    structural and whitespace characters.
 * 2. mask extraction: uses wasm_i8x16_bitmask, which extracts the high bit of
 *    each of the 16 lanes into a 16-bit integer mask. this acts as the native
 *    wasm equivalent of x86 _mm_movemask_epi8, avoiding slow scalar processing.
 */
static inline bool cjsonx_stage1_wasm(const char* json, size_t length, cjsonx_tape_t* tape) {
    uint32_t prev_in_string = 0; // 0 or 0xffffffff
    bool escaped = false;
    bool prev_was_sep = true;
    size_t i = 0;

    while (i < length) {
        // prefetch json data 128 bytes ahead into l1 cache
        __builtin_prefetch(json + i + 128, 0, 0);

        if (i + 31 < length) {
            v128_t v1 = wasm_v128_load((const v128_t*)(json + i));
            v128_t v2 = wasm_v128_load((const v128_t*)(json + i + 16));

            if (prev_in_string) {
                v128_t q1 = wasm_i8x16_eq(v1, wasm_i8x16_splat('"'));
                v128_t q2 = wasm_i8x16_eq(v2, wasm_i8x16_splat('"'));
                v128_t b1 = wasm_i8x16_eq(v1, wasm_i8x16_splat('\\'));
                v128_t b2 = wasm_i8x16_eq(v2, wasm_i8x16_splat('\\'));

                uint32_t q_mask = wasm_i8x16_bitmask(q1) | (wasm_i8x16_bitmask(q2) << 16);
                uint32_t b_mask = wasm_i8x16_bitmask(b1) | (wasm_i8x16_bitmask(b2) << 16);

                // dev note: skipping control char checks inside strings during stage 1
                // is consistent with avx2/neon and avoids register pressure here.
                if (q_mask == 0 && b_mask == 0) {
                    i += 32;
                    continue;
                } else {
                    goto scalar_fallback;
                }
            } else {
                v128_t q1 = wasm_i8x16_eq(v1, wasm_i8x16_splat('"'));
                v128_t q2 = wasm_i8x16_eq(v2, wasm_i8x16_splat('"'));
                v128_t b1 = wasm_i8x16_eq(v1, wasm_i8x16_splat('\\'));
                v128_t b2 = wasm_i8x16_eq(v2, wasm_i8x16_splat('\\'));

                uint32_t q_mask = wasm_i8x16_bitmask(q1) | (wasm_i8x16_bitmask(q2) << 16);
                uint32_t b_mask = wasm_i8x16_bitmask(b1) | (wasm_i8x16_bitmask(b2) << 16);

                if (q_mask != 0 || b_mask != 0 || escaped) {
                    goto scalar_fallback;
                }

                v128_t m_lcb1 = wasm_i8x16_eq(v1, wasm_i8x16_splat('{'));
                v128_t m_lcb2 = wasm_i8x16_eq(v2, wasm_i8x16_splat('{'));
                v128_t m_rcb1 = wasm_i8x16_eq(v1, wasm_i8x16_splat('}'));
                v128_t m_rcb2 = wasm_i8x16_eq(v2, wasm_i8x16_splat('}'));
                v128_t m_lsb1 = wasm_i8x16_eq(v1, wasm_i8x16_splat('['));
                v128_t m_lsb2 = wasm_i8x16_eq(v2, wasm_i8x16_splat('['));
                v128_t m_rsb1 = wasm_i8x16_eq(v1, wasm_i8x16_splat(']'));
                v128_t m_rsb2 = wasm_i8x16_eq(v2, wasm_i8x16_splat(']'));
                v128_t m_col1 = wasm_i8x16_eq(v1, wasm_i8x16_splat(':'));
                v128_t m_col2 = wasm_i8x16_eq(v2, wasm_i8x16_splat(':'));
                v128_t m_com1 = wasm_i8x16_eq(v1, wasm_i8x16_splat(','));
                v128_t m_com2 = wasm_i8x16_eq(v2, wasm_i8x16_splat(','));

                v128_t s1 = wasm_v128_or(wasm_v128_or(wasm_v128_or(m_lcb1, m_rcb1), wasm_v128_or(m_lsb1, m_rsb1)), wasm_v128_or(m_col1, m_com1));
                v128_t s2 = wasm_v128_or(wasm_v128_or(wasm_v128_or(m_lcb2, m_rcb2), wasm_v128_or(m_lsb2, m_rsb2)), wasm_v128_or(m_col2, m_com2));
                uint32_t struct_mask = wasm_i8x16_bitmask(s1) | (wasm_i8x16_bitmask(s2) << 16);

                v128_t m_sp1 = wasm_i8x16_eq(v1, wasm_i8x16_splat(' '));
                v128_t m_sp2 = wasm_i8x16_eq(v2, wasm_i8x16_splat(' '));
                v128_t m_tb1 = wasm_i8x16_eq(v1, wasm_i8x16_splat('\t'));
                v128_t m_tb2 = wasm_i8x16_eq(v2, wasm_i8x16_splat('\t'));
                v128_t m_nl1 = wasm_i8x16_eq(v1, wasm_i8x16_splat('\n'));
                v128_t m_nl2 = wasm_i8x16_eq(v2, wasm_i8x16_splat('\n'));
                v128_t m_cr1 = wasm_i8x16_eq(v1, wasm_i8x16_splat('\r'));
                v128_t m_cr2 = wasm_i8x16_eq(v2, wasm_i8x16_splat('\r'));

                v128_t ws1 = wasm_v128_or(wasm_v128_or(m_sp1, m_tb1), wasm_v128_or(m_nl1, m_cr1));
                v128_t ws2 = wasm_v128_or(wasm_v128_or(m_sp2, m_tb2), wasm_v128_or(m_nl2, m_cr2));
                uint32_t ws_mask = wasm_i8x16_bitmask(ws1) | (wasm_i8x16_bitmask(ws2) << 16);

                uint32_t sep_mask = struct_mask | ws_mask;
                uint32_t non_sep_mask = ~sep_mask;
                
                uint32_t shifted_sep = (sep_mask << 1) | (prev_was_sep ? 1 : 0);
                uint32_t prim_starts = non_sep_mask & shifted_sep;

                uint32_t all_pushes = struct_mask | prim_starts;

                while (all_pushes != 0) {
                    uint32_t bit = (uint32_t)__builtin_ctz(all_pushes);
                    if (!cjsonx_tape_push(tape, (uint32_t)(i + bit))) return false;
                    all_pushes &= (all_pushes - 1);
                }

                prev_was_sep = (sep_mask >> 31) & 1;
                i += 32;
                continue;
            }
        }

    scalar_fallback:
        {
            size_t end = (i + 32 < length) ? (i + 32) : length;
            while (i < end) {
                char c = json[i];
                bool in_string = (prev_in_string != 0);
                
                if (in_string) {
                    if ((unsigned char)c < 0x20) return false; 
                    
                    if (escaped) {
                        escaped = false;
                    } else if (c == '\\') {
                        escaped = true;
                    } else if (c == '"') {
                        prev_in_string = 0;
                        if (!cjsonx_tape_push(tape, (uint32_t)i)) return false;
                        prev_was_sep = true;
                    } else {
                        prev_was_sep = false;
                    }
                } else {
                    if (c == '"') {
                        prev_in_string = 0xFFFFFFFF;
                        if (!cjsonx_tape_push(tape, (uint32_t)i)) return false;
                        prev_was_sep = true;
                    } else if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',') {
                        if (!cjsonx_tape_push(tape, (uint32_t)i)) return false;
                        prev_was_sep = true;
                    } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                        if (prev_was_sep) {
                            if (!cjsonx_tape_push(tape, (uint32_t)i)) return false;
                        }
                        prev_was_sep = false;
                    } else {
                        prev_was_sep = true;
                    }
                }
                i++;
            }
        }
    }
    
    if (prev_in_string != 0) return false; 
    if (tape->count == 0) return false;
    
    return true;
}

#endif // cjsonx_wasm_h
