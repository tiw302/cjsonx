/**
 * @file cjsonx_tape.h
 * @brief Structural token tape for stage 1 output
 *
 * @note Architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
#ifndef CJSONX_TAPE_H
#define CJSONX_TAPE_H

/*==============================================================================
 * MARK: - parsing tape
 *============================================================================*/


#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// structural tokens tape
// stores index offsets of all structural characters like { } [ ] : , "
// helps stage 2 parser skip whitespace and parse faster

typedef struct {
    uint32_t* indices;   // array of index offsets
    size_t count;        // number of indices inside the tape
    size_t capacity;     // capacity of the indices array
    bool is_static;      // if true, do not free or realloc
    cjsonx_allocator_t* alloc; // optional custom allocator
} cjsonx_tape_t;

// alias for compatibility with non-t API
typedef cjsonx_tape_t cjsonx_tape;

// init tape with pre-alloc cap, false on oom
static inline bool cjsonx_tape_init(cjsonx_tape_t* tape, size_t capacity, cjsonx_allocator_t* alloc) {
    tape->alloc = alloc;
    if (alloc && alloc->malloc_fn) {
        tape->indices = (uint32_t*)alloc->malloc_fn(capacity * sizeof(uint32_t), alloc->user_data);
    } else {
        tape->indices = (uint32_t*)malloc(capacity * sizeof(uint32_t));
    }
    if (!tape->indices) return false;
    tape->count = 0;
    tape->capacity = capacity;
    tape->is_static = false;
    return true;
}

// init static tape with user buffer
static inline void cjsonx_tape_init_static(cjsonx_tape_t* tape, uint32_t* buffer, size_t capacity) {
    tape->indices = buffer;
    tape->count = 0;
    tape->capacity = capacity;
    tape->is_static = true;
    tape->alloc = NULL;
}

// free tape and reset
static inline void cjsonx_tape_free(cjsonx_tape_t* tape) {
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
static inline bool cjsonx_tape_push(cjsonx_tape_t* tape, uint32_t index) {
    if (tape->count >= tape->capacity) {
        if (tape->is_static) return false;
        size_t new_cap = tape->capacity ? tape->capacity * 2 : 64;
        uint32_t* new_indices;
        if (tape->alloc && tape->alloc->realloc_fn) {
            new_indices = (uint32_t*)tape->alloc->realloc_fn(tape->indices, new_cap * sizeof(uint32_t), tape->alloc->user_data);
        } else {
            new_indices = (uint32_t*)realloc(tape->indices, new_cap * sizeof(uint32_t));
        }
        if (!new_indices) return false;
        tape->indices = new_indices;
        tape->capacity = new_cap;
    }
    tape->indices[tape->count++] = index;
    return true;
}

#endif // CJSONX_TAPE_H
