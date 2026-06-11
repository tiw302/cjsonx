/**
 * @file cjsonx_fastfloat.h
 * @brief fast float parser using clinger and eisel-lemire
 *
 * @note architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
#ifndef CJSONX_FASTFLOAT_H
#define CJSONX_FASTFLOAT_H

/*==============================================================================
 * mark: - fast float parsing
 *============================================================================*/


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "cjsonx_eisel_lemire.h"

// msvc compat: wrap clzll so we don't touch reserved __builtin_* names (c11 §7.1.3)
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
static inline int cjsonx_clzll(uint64_t x) {
    unsigned long idx;
    _BitScanReverse64(&idx, x);
    return 63 - (int)idx;
}
#define CJSONX_CLZLL(x) cjsonx_clzll(x)
#else
#define CJSONX_CLZLL(x) __builtin_clzll(x)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// fast integer to double string parser using clinger's fast path and eisel-lemire algorithm.

static const double cjsonx_power_of_10[] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
    1e20, 1e21, 1e22
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
static cjsonx_always_inline bool cjsonx_compute_float(uint64_t mantissa, int exponent, double* out) {
    if (CJSONX_UNLIKELY(mantissa == 0)) {
        // zero is zero, no need to compute
        *out = 0.0;
        return true;
    }
    
    // clinger's fast path
    if (mantissa <= 9007199254740991ULL && exponent >= -22 && exponent <= 22) {
        double d = (double)mantissa;
        if (exponent < 0) d /= cjsonx_power_of_10[-exponent];
        else d *= cjsonx_power_of_10[exponent];
        *out = d;
        return true;
    }
    
    // eisel-lemire algorithm
    if (exponent < -348) { *out = 0.0; return true; }
    // use INFINITY constant — 1e308*10 is UB under -ffast-math / -ffinite-math-only
    if (exponent > 342) { *out = INFINITY; return true; }
    
    // lookup precomputed powers of 10
    int index = exponent + 348;
    uint64_t table_m = cjsonx_eisel_lemire_mantissa[index];
    int16_t table_e = cjsonx_eisel_lemire_exp[index];
    
    // normalize mantissa
    int lz = CJSONX_CLZLL(mantissa);
    uint64_t w = mantissa << lz;
    
    uint64_t high = cjsonx_mul64_high(w, table_m);
    
    int msb = (high >> 63) == 1 ? 63 : 62;
    int shift = msb - 52;
    
    uint64_t mantissa_53 = high >> shift;
    
    uint64_t mask = (1ULL << shift) - 1;
    uint64_t discarded = high & mask;
    
    if (discarded == 0 || discarded == (1ULL << (shift - 1))) {
        // exactly halfway, need bignum tie-breaking (fallback to strtod)
        return false;
    }
    
    // round up if halfway or more
    if (discarded > (1ULL << (shift - 1))) {
        mantissa_53++;
    }
    
    if (mantissa_53 >= (1ULL << 53)) {
        mantissa_53 >>= 1;
        shift++;
    }
    
    int final_exp = table_e - lz + 116 + shift;
    
    // subnormal numbers or extremely small numbers
    if (final_exp <= -1023) {
        return false;
    }
    
    // note: the sign bit is intentionally not set here.
    // the caller (cjsonx_parse_fast_float) applies the sign: `negative ? -val : val`
    uint64_t d_bits = (mantissa_53 & 0xFFFFFFFFFFFFF) | ((uint64_t)(final_exp + 1023) << 52);
    memcpy(out, &d_bits, 8);
    return true;
}

static cjsonx_always_inline bool cjsonx_parse_fast_float(const char* __restrict s, const char* __restrict limit, const char** __restrict out_end, double* __restrict out_val) {
    const char* p = s;
    if (CJSONX_UNLIKELY(p >= limit)) return false;
    
    bool negative = false;
    if (*p == '-') { negative = true; p++; }
    if (CJSONX_UNLIKELY(p >= limit)) return false;
    
    uint64_t mantissa = 0;
    int digits = 0;
    
    int exponent = 0;
    if (*p == '0') {
        p++;
        if (CJSONX_UNLIKELY(p < limit && *p >= '0' && *p <= '9')) return false; 
    } else if (CJSONX_LIKELY(*p >= '1' && *p <= '9')) {
        while (p < limit && *p >= '0' && *p <= '9') {
            if (CJSONX_LIKELY(digits < 19)) {
                mantissa = mantissa * 10 + (*p - '0');
            } else {
                exponent++;
            }
            digits++; p++;
        }
    } else return false;
    
    if (p < limit && *p == '.') {
        p++;
        if (CJSONX_UNLIKELY(p >= limit || *p < '0' || *p > '9')) return false;
        while (p < limit && *p >= '0' && *p <= '9') {
            if (CJSONX_UNLIKELY(mantissa == 0 && *p == '0')) {
                exponent--;
            } else if (CJSONX_LIKELY(digits < 19)) {
                mantissa = mantissa * 10 + (*p - '0');
                exponent--;
                digits++;
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
    if (CJSONX_UNLIKELY(!cjsonx_compute_float(mantissa, exponent, &val))) {
        return false;
    }
    
    *out_val = negative ? -val : val;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif // cjsonx_fastfloat_h
