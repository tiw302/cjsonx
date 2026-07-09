// updated 2026-06-23
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_FASTFLOAT_H
#define CJSONX_FASTFLOAT_H

// ███████  █████  ███████ ████████     ███████ ██       ██████   █████  ████████
// ██      ██   ██ ██         ██        ██      ██      ██    ██ ██   ██    ██
// █████   ███████ ███████    ██        █████   ██      ██    ██ ███████    ██
// ██      ██   ██      ██    ██        ██      ██      ██    ██ ██   ██    ██
// ██      ██   ██ ███████    ██        ██      ███████  ██████  ██   ██    ██
//
// >>fast float parsing


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "cjsonx_eisel_lemire.h"

// CJSONX_CLZLL is defined in cjsonx_config.h (included via cjsonx_stage2.h chain)

#ifdef __cplusplus
extern "C" {
#endif

// fast integer to double string parser using clinger's fast path and eisel-lemire algorithm.

static const double cjsonx_power_of_10[] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
    1e20, 1e21, 1e22
};

/*
 * reciprocals of cjsonx_power_of_10[]: multiplying by these avoids fp division.
 * fp division is ~3-5x slower than multiplication on modern cpus.
 */
static const double cjsonx_power_of_10_neg[] = {
    1.0/1e0,  1.0/1e1,  1.0/1e2,  1.0/1e3,  1.0/1e4,  1.0/1e5,
    1.0/1e6,  1.0/1e7,  1.0/1e8,  1.0/1e9,  1.0/1e10, 1.0/1e11,
    1.0/1e12, 1.0/1e13, 1.0/1e14, 1.0/1e15, 1.0/1e16, 1.0/1e17,
    1.0/1e18, 1.0/1e19, 1.0/1e20, 1.0/1e21, 1.0/1e22
};

// 64x64 -> 128 bit multiplication
static inline uint64_t cjsonx_mul64_high(uint64_t a, uint64_t b) {
#if defined(__SIZEOF_INT128__)
    return (uint64_t)((((unsigned __int128)a) * b) >> 64);
#else
    uint32_t a32 = a >> 32, a00 = a & 0xFFFFFFFF;
    uint32_t b32 = b >> 32, b00 = b & 0xFFFFFFFF;
    uint64_t p00 = (uint64_t)a00 * b00;
    uint64_t p32 = (uint64_t)a32 * b00 + (p00 >> 32);
    uint64_t p03 = (uint64_t)a00 * b32 + (uint32_t)p32;
    uint64_t p33 = (uint64_t)a32 * b32 + (p32 >> 32) + (p03 >> 32);
    return p33;
#endif
}

// convert (mantissa × 10^exponent) to ieee-754 double using fast path or eisel-lemire
static cjsonx_always_inline bool cjsonx_compute_float(uint64_t mantissa, int exponent, bool has_truncated, double* out) {
    if (CJSONX_UNLIKELY(has_truncated)) return false;
    if (CJSONX_UNLIKELY(mantissa == 0)) {
        // zero is zero, no need to compute
        *out = 0.0;
        return true;
    }
    
    /*
     * clinger's fast path:
     * if the parsed mantissa fits exactly inside 53 bits (ieee-754 mantissa size, max 9007199254740991)
     * and the exponent is between -22 and 22, the conversion is mathematically exact using double multiplication or division.
     * this avoids any rounding error estimation or multi-precision arithmetic.
     */
    if (mantissa <= 9007199254740991ULL && exponent >= -22 && exponent <= 22) {
        double d = (double)mantissa;
        // fix: multiply by precomputed reciprocal instead of dividing;
        // fp division is ~3-5x slower than multiplication.
        if (exponent < 0) d *= cjsonx_power_of_10_neg[-exponent];
        else d *= cjsonx_power_of_10[exponent];
        *out = d;
        return true;
    }
    
    /*
     * eisel-lemire algorithm:
     * used as an extremely fast fallback when clinger's fast path is not applicable but the value is still
     * representable. it computes the double using a 128-bit multiplication of the normalized mantissa
     * and precomputed powers of 10.
     */
    if (exponent < -348) { *out = 0.0; return true; }
    // use infinity constant — 1e308*10 is ub under -ffast-math / -ffinite-math-only
    if (exponent > 342) { *out = INFINITY; return true; }
    
    // lookup precomputed powers of 10.
    // cjsonx_eisel_lemire_mantissa stores the high 64 bits of 10^exponent scaled by 2^q.
    int index = exponent + 348;
    uint64_t table_m = cjsonx_eisel_lemire_mantissa[index];
    int16_t table_e = cjsonx_eisel_lemire_exp[index];
    
    // normalize mantissa: align it to the most significant bit to maximize precision
    int lz = CJSONX_CLZLL(mantissa);
    uint64_t w = mantissa << lz;
    
    // perform 64x64 -> 128 bit multiplication, retaining the high 64 bits of the product
    uint64_t high = cjsonx_mul64_high(w, table_m);
    
    // find the most significant bit of the product to determine the exponent shift
    int msb = (high >> 63) == 1 ? 63 : 62;
    int shift = msb - 52;
    
    // extract the 53-bit significand for the double-precision float
    uint64_t mantissa_53 = high >> shift;
    
    // check the discarded bits for rounding and exact halfway cases
    uint64_t mask = (1ULL << shift) - 1;
    uint64_t discarded = high & mask;
    
    if (discarded == 0 || discarded == (1ULL << (shift - 1))) {
        // ambiguous halfway case: attempt a second multiplication using the next
        // table entry per the eisel-lemire paper. this resolves most ambiguous cases
        // without falling back to the slow strtod path.
        if (index + 1 > 690) return false; // bounds check: table has 691 entries (0..690)
        uint64_t table_m2   = cjsonx_eisel_lemire_mantissa[index + 1];
        uint64_t high2      = cjsonx_mul64_high(w, table_m2);
        uint64_t mask2      = (1ULL << shift) - 1;
        uint64_t discarded2 = high2 & mask2;
        if (discarded2 == 0 || discarded2 == (1ULL << (shift - 1))) {
            // still ambiguous after second pass; fall back to slow path.
            return false;
        }
        // second pass resolved the ambiguity; apply its rounding decision.
        if (discarded2 > (1ULL << (shift - 1))) mantissa_53++;
        if (mantissa_53 >= (1ULL << 53)) { mantissa_53 >>= 1; shift++; }
        int final_exp2 = table_e - lz + 116 + shift;
        if (final_exp2 <= -1023) return false;
        uint64_t d_bits2 = (mantissa_53 & 0xFFFFFFFFFFFFF) | ((uint64_t)(final_exp2 + 1023) << 52);
        memcpy(out, &d_bits2, 8);
        return true;
    }
    
    // round up if the discarded bits are greater than the halfway point (round-to-nearest)
    if (discarded > (1ULL << (shift - 1))) {
        mantissa_53++;
    }
    
    // handle potential carry overflow from rounding
    if (mantissa_53 >= (1ULL << 53)) {
        mantissa_53 >>= 1;
        shift++;
    }
    
    int final_exp = table_e - lz + 116 + shift;
    
    // subnormal numbers or extremely small numbers go to fallback
    // dev note: delegating subnormals to the slow path is standard practice in fastfloat
    // to maintain exact accuracy in edge cases.
    if (final_exp <= -1023) {
        return false;
    }
    
    /*
     * note: the sign bit is intentionally not set here.
     * the caller (cjsonx_parse_fast_float) applies the sign: `negative ? -val : val`.
     * assemble the bits directly: 52-bit mantissa + 11-bit biased exponent (final_exp + 1023) shifted by 52
     */
    uint64_t d_bits = (mantissa_53 & 0xFFFFFFFFFFFFF) | ((uint64_t)(final_exp + 1023) << 52);
    memcpy(out, &d_bits, 8);
    return true;
}

/*
 * fast float parser implementation:
 * parses the input string into a mantissa (up to 19 digits to fit inside uint64_t)
 * and an integer exponent, then invokes cjsonx_compute_float for conversion.
 * 
 * 1. sign: checks for leading '-' to determine positive/negative.
 * 2. integer part: checks for leading zeros (which must not be followed by digits).
 *    accumulates digits into mantissa, scaling by 10. if digits exceed 19, they
 *    are discarded but increment the exponent to maintain scale.
 * 3. fractional part: processes digits after the decimal point, decrementing the
 *    exponent for each digit to shift the decimal point.
 * 4. exponent suffix (e/E): parses the scientific notation suffix and adjusts
 *    the exponent value accordingly.
 * 
 * dev note: when digits reach 19, the mantissa is already at maximum uint64 precision.
 * integer overflow digits are counted as positive exponent adjustments (scale correction).
 * fractional overflow digits are silently dropped (they are beyond ieee-754 double precision).
 * in both cases we proceed to eisel-lemire rather than bailing out to strtod.
 */
static cjsonx_always_inline bool cjsonx_parse_fast_float(const char* __restrict s, const char* __restrict limit, const char** __restrict out_end, double* __restrict out_val) {
    const char* p = s;
    if (CJSONX_UNLIKELY(p >= limit)) return false;
    
    bool negative = false;
    if (*p == '-') { negative = true; p++; }
    if (CJSONX_UNLIKELY(p >= limit)) return false;
    
    uint64_t mantissa = 0;
    int digits = 0;
    bool has_truncated = false; // true when digits beyond 19 were dropped
    
    int exponent = 0;
    if (*p == '0') {
        p++;
        if (CJSONX_UNLIKELY(p < limit && *p >= '0' && *p <= '9')) return false; 
    } else if (CJSONX_LIKELY(*p >= '1' && *p <= '9')) {
        while (p < limit && *p >= '0' && *p <= '9') {
            if (CJSONX_UNLIKELY(digits >= 19)) {
                // mantissa is full (19 significant digits saturates uint64 precision).
                // count remaining integer digits as positive exponent adjustment so we
                // can still proceed through eisel-lemire instead of falling back to strtod.
                exponent++;
                has_truncated = true;
            } else {
                mantissa = mantissa * 10 + (*p - '0');
                digits++;
            }
            p++;
        }
    } else return false;
    
    if (p < limit && *p == '.') {
        p++;
        if (CJSONX_UNLIKELY(p >= limit || *p < '0' || *p > '9')) return false;
        while (p < limit && *p >= '0' && *p <= '9') {
            if (CJSONX_UNLIKELY(mantissa == 0 && *p == '0')) {
                exponent--;
            } else {
                if (CJSONX_UNLIKELY(digits >= 19)) {
                    // mantissa is full — consume the fractional digit without accumulating.
                    // digits past the 19-digit limit are beyond ieee-754 double precision
                    // and do not adjust the exponent (they are already past the decimal point).
                    has_truncated = true;
                } else {
                    mantissa = mantissa * 10 + (*p - '0');
                    exponent--;
                    digits++;
                }
            }
            p++;
        }
    }
    
    if (p < limit && (*p == 'e' || *p == 'E')) {
        p++;
        bool exp_negative = false;
        if (p < limit && (*p == '+' || *p == '-')) { exp_negative = (*p == '-'); p++; }
        if (CJSONX_UNLIKELY(p >= limit || *p < '0' || *p > '9')) return false;
        int exp_val = 0;
        while (p < limit && *p >= '0' && *p <= '9') {
            if (CJSONX_LIKELY(exp_val < 10000)) exp_val = exp_val * 10 + (*p - '0');
            p++;
        }
        exponent += exp_negative ? -exp_val : exp_val;
    }
    
    *out_end = p;
    
    double val;
    if (CJSONX_UNLIKELY(!cjsonx_compute_float(mantissa, exponent, has_truncated, &val))) {
        return false;
    }
    
    *out_val = negative ? -val : val;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif // cjsonx_fastfloat_h
