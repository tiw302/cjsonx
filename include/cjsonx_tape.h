// updated 2026-06-13
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_TAPE_H
#define CJSONX_TAPE_H

// ████████  █████  ██████  ███████
//    ██    ██   ██ ██   ██ ██
//    ██    ███████ ██████  █████
//    ██    ██   ██ ██      ██
//    ██    ██   ██ ██      ███████
//
// >>parsing tape


#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * structural tokens tape
 * stores index offsets of all structural characters like { } [ ] : , "
 * helps stage 2 parser skip whitespace and parse faster
 * 
 * note: using uint32_t for index offsets limits the maximum supported json input
 * size to 4 gib (uint32_max).
 */
typedef struct {
    uint32_t* indices;   // array of 32-bit index offsets (limits input size to 4 gib)
    size_t count;        // number of indices inside the tape
    size_t capacity;     // capacity of the indices array
    bool is_static;      // if true, do not free or realloc
    cjsonx_allocator_t* alloc; // optional custom allocator
} cjsonx_tape_t;

// alias for compatibility with non-t api
typedef cjsonx_tape_t cjsonx_tape;

// init tape with pre-alloc cap, false on oom
static cjsonx_always_inline bool cjsonx_tape_init(cjsonx_tape_t* tape, size_t capacity, cjsonx_allocator_t* alloc) {
    if (CJSONX_UNLIKELY(capacity > (size_t)-1 / sizeof(uint32_t))) return false;
    tape->alloc = alloc;
    if (alloc && alloc->malloc_fn) {
        tape->indices = (uint32_t*)alloc->malloc_fn(capacity * sizeof(uint32_t), alloc->user_data);
    } else {
        tape->indices = (uint32_t*)malloc(capacity * sizeof(uint32_t));
    }
    if (CJSONX_UNLIKELY(!tape->indices)) return false;
    tape->count = 0;
    tape->capacity = capacity;
    tape->is_static = false;
    return true;
}

// init static tape with user buffer
static cjsonx_always_inline void cjsonx_tape_init_static(cjsonx_tape_t* tape, uint32_t* buffer, size_t capacity) {
    tape->indices = buffer;
    tape->count = 0;
    tape->capacity = capacity;
    tape->is_static = true;
    tape->alloc = NULL;
}

// free tape and reset
static cjsonx_always_inline void cjsonx_tape_free(cjsonx_tape_t* tape) {
    if (tape->indices && !tape->is_static) {
        if (tape->alloc && tape->alloc->free_fn) {
            tape->alloc->free_fn(tape->indices, tape->alloc->user_data);
        } else {
            free(tape->indices);
        }
    }
    tape->indices = NULL;
    tape->count = 0;
    tape->capacity = 0;
    tape->is_static = false;
}

// push offset to tape, grow 2x on full
static cjsonx_always_inline bool cjsonx_tape_push(cjsonx_tape_t* tape, uint32_t index) {
    if (CJSONX_UNLIKELY(tape->count >= tape->capacity)) {
        if (CJSONX_UNLIKELY(tape->is_static)) return false;
        size_t new_cap = tape->capacity ? tape->capacity * 2 : 64;
        if (CJSONX_UNLIKELY(new_cap < tape->capacity || new_cap > (size_t)-1 / sizeof(uint32_t))) return false;
        uint32_t* new_indices = (uint32_t*)cjsonx_realloc(tape->alloc, tape->indices, tape->capacity * sizeof(uint32_t), new_cap * sizeof(uint32_t));
        if (CJSONX_UNLIKELY(!new_indices)) return false;
        tape->indices = new_indices;
        tape->capacity = new_cap;
    }
    tape->indices[tape->count++] = index;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif // cjsonx_tape_h
