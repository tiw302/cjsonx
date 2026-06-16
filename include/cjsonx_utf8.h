// updated 2026-06-13
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_UTF8_H
#define CJSONX_UTF8_H

// ██    ██ ████████ ███████  █████
// ██    ██    ██    ██      ██   ██
// ██    ██    ██    █████    █████
// ██    ██    ██    ██      ██   ██
//  ██████     ██    ██       █████
//
// >>utf-8 validation


#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// copyright (c) 2008-2009 bjoern hoehrmann <bjoern@hoehrmann.de>
// see http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define CJSONX_UTF8_ACCEPT 0
#define CJSONX_UTF8_REJECT 1

static const uint8_t cjsonx_utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1  // s7..s8
};

/*
 * bjoern hoehrmann's dfa-based utf-8 decoder.
 * this function decodes utf-8 octets using a finite state machine (dfa).
 * 1. it maps the current input byte to its character class (type) using the lookup table.
 * 2. if we are in the middle of decoding a multi-byte sequence (state is not cjsonx_utf8_accept),
 *    we shift the accumulated codepoint left by 6 bits and append the lower 6 bits of the byte.
 * 3. if starting a new character sequence, we extract the initial payload bits based on the type's prefix.
 * 4. it transitions to the next state by indexing the transition matrix at (256 + state * 16 + type).
 * 5. returns the new state: cjsonx_utf8_accept (0) if a character is successfully finished,
 *    cjsonx_utf8_reject (1) if the sequence is invalid, or a intermediate state if more bytes are expected.
 */
static inline uint32_t cjsonx_utf8_decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
  uint32_t type = cjsonx_utf8d[byte];
  
  *codep = (*state != CJSONX_UTF8_ACCEPT) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);
    
  *state = cjsonx_utf8d[256 + *state * 16 + type];
  return *state;
}

#ifdef __cplusplus
}
#endif
#endif // cjsonx_utf8_h
