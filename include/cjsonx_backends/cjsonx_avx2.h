// updated 2026-06-18
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_AVX2_H
#define CJSONX_AVX2_H

//  █████  ██    ██ ██   ██ ██████
// ██   ██ ██    ██  ██ ██       ██
// ███████ ██    ██   ███    █████
// ██   ██  ██  ██   ██ ██  ██
// ██   ██   ████   ██   ██ ███████
//
// >>avx2 backend


#include <immintrin.h>  // covers pclmulqdq / wmmintrin transitively
#include <stdint.h>
#include <stdbool.h>

// msvc compatibility for gcc/clang builtins
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
static inline uint32_t cjsonx_ctz32(uint32_t x) {
    unsigned long idx;
    _BitScanForward(&idx, x);
    return (uint32_t)idx;
}
#define __builtin_ctz cjsonx_ctz32
#define __builtin_prefetch(addr, ...) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#endif

// avx2 specific scanner utilizing 32-byte vectors and pclmulqdq
static inline bool cjsonx_stage1_avx2(const char* json, size_t length, cjsonx_tape_t* tape) {
    uint32_t prev_in_string = 0; // 0 or 0xffffffff
    bool escaped = false;
    bool prev_was_sep = true;
    size_t i = 0;

    while (i < length) {
        // prefetch 512 bytes ahead (~16 cache lines) for better throughput on modern cpus
        __builtin_prefetch(json + i + 512, 0, 0);

        if (i + 31 < length) {
            __m256i v = _mm256_loadu_si256((const __m256i*)(json + i));

            // if we have backslashes or an active escape, fall back to scalar for this block
            uint32_t bs_bits = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, _mm256_set1_epi8('\\')));
            if (bs_bits != 0 || escaped) {
                goto scalar_fallback;
            }

            /*
             * control chars (< 0x20) inside strings are intentionally NOT checked here.
             * they are handled by cjsonx_parse_string_impl in stage 2, which scans string
             * content byte-by-byte (or with simd) and rejects any raw control character.
             * this is a deliberate tradeoff: adding a control char check in stage 1 simd
             * would add extra instructions on the hot path for a case that stage 2 already handles.
             */

            uint32_t quote_bits = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, _mm256_set1_epi8('"')));
            
            /*
             * pure avx2 string mask using carry-less multiplication (prefix xor).
             * we use the pclmulqdq instruction to compute the prefix xor of the quote bits in parallel.
             * clmul of a bitmask with a sequence of 1s (0xff...ff) mathematically computes the cumulative xor sum.
             * this effectively sets all bits inside quotes to 1 and all bits outside to 0.
             * string_mask is then combined with the state from the previous block (prev_in_string).
             */
            __m128i q = _mm_set_epi64x(0, quote_bits);
            __m128i ones = _mm_set1_epi8(-1);
            __m128i prefix = _mm_clmulepi64_si128(q, ones, 0);
            uint32_t string_mask = (uint32_t)_mm_cvtsi128_si32(prefix);
            string_mask ^= prev_in_string;

            // update prev_in_string (broadcast the highest bit to all 32 bits)
            prev_in_string = (uint32_t)((int32_t)string_mask >> 31);

            // find structurals
            __m256i m_lcb = _mm256_cmpeq_epi8(v, _mm256_set1_epi8('{'));
            __m256i m_rcb = _mm256_cmpeq_epi8(v, _mm256_set1_epi8('}'));
            __m256i m_lsb = _mm256_cmpeq_epi8(v, _mm256_set1_epi8('['));
            __m256i m_rsb = _mm256_cmpeq_epi8(v, _mm256_set1_epi8(']'));
            __m256i m_col = _mm256_cmpeq_epi8(v, _mm256_set1_epi8(':'));
            __m256i m_com = _mm256_cmpeq_epi8(v, _mm256_set1_epi8(','));

            __m256i structurals = _mm256_or_si256(
                _mm256_or_si256(_mm256_or_si256(m_lcb, m_rcb), _mm256_or_si256(m_lsb, m_rsb)),
                _mm256_or_si256(m_col, m_com)
            );
            uint32_t struct_mask = (uint32_t)_mm256_movemask_epi8(structurals);

            // find whitespace
            __m256i m_sp  = _mm256_cmpeq_epi8(v, _mm256_set1_epi8(' '));
            __m256i m_tab = _mm256_cmpeq_epi8(v, _mm256_set1_epi8('\t'));
            __m256i m_nl  = _mm256_cmpeq_epi8(v, _mm256_set1_epi8('\n'));
            __m256i m_cr  = _mm256_cmpeq_epi8(v, _mm256_set1_epi8('\r'));

            __m256i whitespace = _mm256_or_si256(
                _mm256_or_si256(m_sp, m_tab),
                _mm256_or_si256(m_nl, m_cr)
            );
            uint32_t ws_mask = (uint32_t)_mm256_movemask_epi8(whitespace);

            // clear structurals and whitespaces that are inside strings
            uint32_t valid_structurals = struct_mask & ~string_mask;
            uint32_t valid_whitespace  = ws_mask & ~string_mask;

            uint32_t sep_mask = valid_structurals | valid_whitespace | quote_bits;
            uint32_t non_sep_mask = ~sep_mask;
            
            /*
             * primitive starts:
             * a primitive (number, true, false, null) starts at any character that is not a separator/whitespace/quote (non_sep_mask)
             * but is preceded by a separator/whitespace/quote (shifted_sep). we shift sep_mask by 1 to align with the next character,
             * inserting prev_was_sep at bit 0. primitives must also not be inside a string (~string_mask).
             */
            uint32_t shifted_sep = (sep_mask << 1) | (prev_was_sep ? 1 : 0);
            uint32_t prim_starts = (non_sep_mask & shifted_sep) & ~string_mask;

            // push to tape: valid structurals, quotes, and primitive starts
            uint32_t all_pushes = valid_structurals | quote_bits | prim_starts;

            while (all_pushes != 0) {
                uint32_t bit = (uint32_t)__builtin_ctz(all_pushes);
                if (!cjsonx_tape_push(tape, (uint32_t)(i + bit))) return false;
                all_pushes &= (all_pushes - 1);
            }

            prev_was_sep = (sep_mask >> 31) & 1;
            i += 32;
            continue;
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

#endif // cjsonx_avx2_h
