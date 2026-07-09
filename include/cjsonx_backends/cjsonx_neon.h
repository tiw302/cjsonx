// updated 2026-06-18
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_NEON_H
#define CJSONX_NEON_H

// ███    ██ ███████  ██████  ███    ██
// ████   ██ ██      ██    ██ ████   ██
// ██ ██  ██ █████   ██    ██ ██ ██  ██
// ██  ██ ██ ██      ██    ██ ██  ██ ██
// ██   ████ ███████  ██████  ██   ████
//
// >>neon backend


#include <arm_neon.h>
#include <stdint.h>
#include <stdbool.h>

// msvc compat: neon doesn't have __builtin_prefetch, mirror the avx2 shim
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#define __builtin_prefetch(addr, ...) (void)(addr)
#endif

/*
 * neon-based emulation of the x86 _mm_movemask_epi8 instruction.
 * since arm neon lacks a direct movemask instruction, we achieve this by:
 * 1. masking each byte with a specific power-of-two bit corresponding to its index (bitmask vector).
 *    this maps each byte's boolean result (0x00 or 0xff) to a single unique bit (e.g. 0x01 for byte 0, 0x02 for byte 1, etc.).
 * 2. performing three rounds of pairwise addition (vpaddq_u8). pairwise addition sums adjacent bytes,
 *    accumulating the individual mapped bits from the lower and upper halves.
 * 3. extracting the final combined 16-bit mask from the vector lane.
 */
static inline uint16_t neon_movemask_u8(uint8x16_t v) {
    const uint8x16_t bitmask = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
                                0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
    uint8x16_t masked = vandq_u8(v, bitmask);
    uint8x16_t tmp0 = vpaddq_u8(masked, masked);
    uint8x16_t tmp1 = vpaddq_u8(tmp0, tmp0);
    uint8x16_t tmp2 = vpaddq_u8(tmp1, tmp1);
    return vgetq_lane_u16(vreinterpretq_u16_u8(tmp2), 0);
}

static inline bool cjsonx_stage1_neon(const char* json, size_t length, cjsonx_tape_t* tape) {
    uint32_t prev_in_string = 0; // 0 or 0xffffffff
    bool escaped = false;
    bool prev_was_sep = true;
    size_t i = 0;

    while (i < length) {
        // prefetch json data 128 bytes ahead into l1 cache
        __builtin_prefetch(json + i + 128, 0, 0);

        if (i + 31 < length) {
            uint8x16_t v1 = vld1q_u8((const uint8_t*)(json + i));
            uint8x16_t v2 = vld1q_u8((const uint8_t*)(json + i + 16));

            if (prev_in_string) {
                // if inside string, we only care about quotes and backslashes
                uint8x16_t q1 = vceqq_u8(v1, vdupq_n_u8('"'));
                uint8x16_t q2 = vceqq_u8(v2, vdupq_n_u8('"'));
                uint8x16_t b1 = vceqq_u8(v1, vdupq_n_u8('\\'));
                uint8x16_t b2 = vceqq_u8(v2, vdupq_n_u8('\\'));

                uint32_t q_mask = neon_movemask_u8(q1) | (neon_movemask_u8(q2) << 16);
                uint32_t b_mask = neon_movemask_u8(b1) | (neon_movemask_u8(b2) << 16);

                if (q_mask == 0 && b_mask == 0) {
                    /*
                     * safe bulk string processing: no quotes or backslashes in this 32-byte block.
                     * note: control chars (< 0x20) inside strings are NOT checked here —
                     * the check is intentionally delegated to cjsonx_parse_string_impl in stage 2,
                     * which scans the string content explicitly. this matches the avx2 backend behavior.
                     */
                    i += 32;
                    continue;
                } else {
                    goto scalar_fallback; // boundary logic complex, fallback to scalar
                }
            } else {
                // not in string. check if string starts or backslashes exist
                uint8x16_t q1 = vceqq_u8(v1, vdupq_n_u8('"'));
                uint8x16_t q2 = vceqq_u8(v2, vdupq_n_u8('"'));
                uint8x16_t b1 = vceqq_u8(v1, vdupq_n_u8('\\'));
                uint8x16_t b2 = vceqq_u8(v2, vdupq_n_u8('\\'));

                uint32_t q_mask = neon_movemask_u8(q1) | (neon_movemask_u8(q2) << 16);
                uint32_t b_mask = neon_movemask_u8(b1) | (neon_movemask_u8(b2) << 16);

                if (q_mask != 0 || b_mask != 0 || escaped) {
                    goto scalar_fallback; // entering string or escaping, fallback to scalar
                }

                // if no strings started, just find structurals and whitespaces
                uint8x16_t m_lcb1 = vceqq_u8(v1, vdupq_n_u8('{'));
                uint8x16_t m_lcb2 = vceqq_u8(v2, vdupq_n_u8('{'));
                uint8x16_t m_rcb1 = vceqq_u8(v1, vdupq_n_u8('}'));
                uint8x16_t m_rcb2 = vceqq_u8(v2, vdupq_n_u8('}'));
                uint8x16_t m_lsb1 = vceqq_u8(v1, vdupq_n_u8('['));
                uint8x16_t m_lsb2 = vceqq_u8(v2, vdupq_n_u8('['));
                uint8x16_t m_rsb1 = vceqq_u8(v1, vdupq_n_u8(']'));
                uint8x16_t m_rsb2 = vceqq_u8(v2, vdupq_n_u8(']'));
                uint8x16_t m_col1 = vceqq_u8(v1, vdupq_n_u8(':'));
                uint8x16_t m_col2 = vceqq_u8(v2, vdupq_n_u8(':'));
                uint8x16_t m_com1 = vceqq_u8(v1, vdupq_n_u8(','));
                uint8x16_t m_com2 = vceqq_u8(v2, vdupq_n_u8(','));

                uint8x16_t s1 = vorrq_u8(vorrq_u8(vorrq_u8(m_lcb1, m_rcb1), vorrq_u8(m_lsb1, m_rsb1)), vorrq_u8(m_col1, m_com1));
                uint8x16_t s2 = vorrq_u8(vorrq_u8(vorrq_u8(m_lcb2, m_rcb2), vorrq_u8(m_lsb2, m_rsb2)), vorrq_u8(m_col2, m_com2));
                uint32_t struct_mask = neon_movemask_u8(s1) | (neon_movemask_u8(s2) << 16);

                uint8x16_t m_sp1 = vceqq_u8(v1, vdupq_n_u8(' '));
                uint8x16_t m_sp2 = vceqq_u8(v2, vdupq_n_u8(' '));
                uint8x16_t m_tb1 = vceqq_u8(v1, vdupq_n_u8('\t'));
                uint8x16_t m_tb2 = vceqq_u8(v2, vdupq_n_u8('\t'));
                uint8x16_t m_nl1 = vceqq_u8(v1, vdupq_n_u8('\n'));
                uint8x16_t m_nl2 = vceqq_u8(v2, vdupq_n_u8('\n'));
                uint8x16_t m_cr1 = vceqq_u8(v1, vdupq_n_u8('\r'));
                uint8x16_t m_cr2 = vceqq_u8(v2, vdupq_n_u8('\r'));

                uint8x16_t ws1 = vorrq_u8(vorrq_u8(m_sp1, m_tb1), vorrq_u8(m_nl1, m_cr1));
                uint8x16_t ws2 = vorrq_u8(vorrq_u8(m_sp2, m_tb2), vorrq_u8(m_nl2, m_cr2));
                uint32_t ws_mask = neon_movemask_u8(ws1) | (neon_movemask_u8(ws2) << 16);

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

#endif // cjsonx_neon_h
