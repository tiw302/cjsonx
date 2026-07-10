/*
 * cjsonx.h -- high-performance json parser for c11.
 * project url: https://github.com/tiw302/cjsonx
 * do this:
 * #define CJSONX_IMPLEMENTATION
 * before you include this file in *one* c or c++ file to create the
 * implementation.
 * technical background:
 * ---------------------
 * this library uses a two-stage parsing algorithm inspired by simdjson.
 * stage 1 builds the tape (indices) via simd structure scan, and stage 2
 * parses into a 16-byte fixed node arena dom.
 * memory:
 * -------
 * uses cjsonx_arena. for real zero-alloc, use cjsonx_parse_with_buffer.
 * performance:
 * ------------
 * cjsonx_array_push is o(1) because it uses the last child index.
 * use the builder api for large arrays.
 * conformance:
 * ------------
 * keep JSONTestSuite passing. don't break it.
 * simd optimization:
 * ------------------
 * we've got backends for pretty much everything:
 * - avx2:     x86_64 modern (haswell+, ryzen+)
 * - neon:     arm64 (apple silicon, graviton, android)
 * - wasm:     webassembly with simd128
 * - scalar:   fallback for everything else (risc-v, ppc, etc.)
 * license:
 * --------
 * mit license
 * copyright (c) 2026 jirawat siripuk
 */

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1  // expose posix locale functions in standard headers
#endif

#ifndef CJSONX_H
#define CJSONX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// version macros
#define CJSONX_VERSION_MAJOR 1
#define CJSONX_VERSION_MINOR 3
#define CJSONX_VERSION_PATCH 0
#define CJSONX_VERSION_STRING "1.3.0"

// internal headers (order matters: config → error → dom → tape → arena)
// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_ARENA_H
#define CJSONX_ARENA_H

//  █████  ██████  ███████ ███    ██  █████
// ██   ██ ██   ██ ██      ████   ██ ██   ██
// ███████ ██████  █████   ██ ██  ██ ███████
// ██   ██ ██   ██ ██      ██  ██ ██ ██   ██
// ██   ██ ██   ██ ███████ ██   ████ ██   ██
//
// >>memory arena

#include <stdlib.h>

// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_CONFIG_H
#define CJSONX_CONFIG_H

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1  // expose posix locales like strtod_l
#endif

//  ██████  ██████  ███    ██ ███████ ██  ██████
// ██      ██    ██ ████   ██ ██      ██ ██
// ██      ██    ██ ██ ██  ██ █████   ██ ██   ███
// ██      ██    ██ ██  ██ ██ ██      ██ ██    ██
//  ██████  ██████  ██   ████ ██      ██  ██████
//
// >>configuration

// ██   ██ ██ ███    ██ ████████ ███████
// ██   ██ ██ ████   ██    ██    ██
// ███████ ██ ██ ██  ██    ██    ███████
// ██   ██ ██ ██  ██ ██    ██         ██
// ██   ██ ██ ██   ████    ██    ███████
//
// >>compiler hints

#if defined(__GNUC__) || defined(__clang__)
// dev note: using __builtin_expect for branch prediction helps the cpu pipeline stay full on hot
// paths.
#define CJSONX_LIKELY(x) __builtin_expect(!!(x), 1)
#define CJSONX_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define cjsonx_always_inline __attribute__((always_inline)) inline
#define CJSONX_NODISCARD __attribute__((warn_unused_result))
#else
#define CJSONX_LIKELY(x) (x)
#define CJSONX_UNLIKELY(x) (x)
#define cjsonx_always_inline inline
#define CJSONX_NODISCARD
#endif

// export macro for dynamic link libraries
#ifndef CJSONX_API
#define CJSONX_API
#endif

/*
 * msvc compat: wrap __builtin_clzll behind CJSONX_CLZLL.
 * _BitScanReverse64 is available on MSVC x64 only (not x86/win32).
 * all 32-bit targets are excluded from the build via CIBW_SKIP in CI.
 */
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
static inline int cjsonx_clzll_impl(unsigned long long x) {
    unsigned long idx;
    _BitScanReverse64(&idx, x);
    return 63 - (int)idx;
}
#define CJSONX_CLZLL(x) cjsonx_clzll_impl(x)
#else
#define CJSONX_CLZLL(x) __builtin_clzll(x)
#endif

// configuration constants

// maximum nesting level for arrays and objects to prevent stack overflow
#ifndef CJSONX_MAX_DEPTH
#define CJSONX_MAX_DEPTH 1000
#endif

// initial capacity of the structural tokens tape
#ifndef CJSONX_INITIAL_TAPE_CAP
#define CJSONX_INITIAL_TAPE_CAP 1024
#endif

// initial capacity of elements and fields inside arrays and objects
#ifndef CJSONX_INITIAL_CONTAINER_CAP
#define CJSONX_INITIAL_CONTAINER_CAP 16
#endif

/*
 * the block size allocated by the memory arena at once; larger initial chunk means fewer
 * malloc calls during parsing of documents with many strings.
 */
#ifndef CJSONX_ARENA_CHUNK_SIZE
#define CJSONX_ARENA_CHUNK_SIZE 65536
#endif

// initial capacity for string builder buffers
#ifndef CJSONX_INITIAL_STRBUF_CAP
#define CJSONX_INITIAL_STRBUF_CAP 2048
#endif

// margin to add when growing string builder buffers
#ifndef CJSONX_STRBUF_GROW_MARGIN
#define CJSONX_STRBUF_GROW_MARGIN 1024
#endif

#endif  // cjsonx_config_h
// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_DOM_H
#define CJSONX_DOM_H

// ██████   ██████  ███    ███
// ██   ██ ██    ██ ████  ████
// ██   ██ ██    ██ ██ ████ ██
// ██   ██ ██    ██ ██  ██  ██
// ██████   ██████  ██      ██
//
// >>dom document object model


// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_ERROR_H
#define CJSONX_ERROR_H

// ███████ ██████  ██████   ██████  ██████
// ██      ██   ██ ██   ██ ██    ██ ██   ██
// █████   ██████  ██████  ██    ██ ██████
// ██      ██   ██ ██   ██ ██    ██ ██   ██
// ███████ ██   ██ ██   ██  ██████  ██   ██
//
// >>error handling

// error codes

typedef enum {
    CJSONX_SUCCESS = 0,                 // parsed successfully
    CJSONX_ERROR_OOM,                   // out of memory
    CJSONX_ERROR_EMPTY_INPUT,           // input is empty or whitespace only
    CJSONX_ERROR_UNTERMINATED_STRING,   // string is not closed with a quote
    CJSONX_ERROR_INVALID_CONTROL_CHAR,  // raw control character inside string
    CJSONX_ERROR_INVALID_ESCAPE,        // invalid escape sequence inside string
    CJSONX_ERROR_INVALID_NUMBER,        // number format is invalid
    CJSONX_ERROR_INVALID_KEYWORD,       // keyword true, false, or null is misspelled
    CJSONX_ERROR_MAX_DEPTH,             // maximum nesting depth exceeded
    CJSONX_ERROR_MISSING_COMMA,         // missing comma between elements
    CJSONX_ERROR_MISSING_COLON,         // missing colon after key
    CJSONX_ERROR_TRAILING_COMMA,        // trailing comma is not allowed
    CJSONX_ERROR_UNEXPECTED_TOKEN,      // found unexpected token
    CJSONX_ERROR_UNCLOSED_CONTAINER,    // array or object is not closed
    CJSONX_ERROR_TRAILING_GARBAGE,      // extra data found after root value
    CJSONX_ERROR_INVALID_UTF8,          // string contains invalid utf-8 sequences
    CJSONX_ERROR_TOO_LARGE              // string or container exceeds 24-bit limit
} cjsonx_error_t;

// convert error code to string
static inline const char* cjsonx_error_string(cjsonx_error_t err) {
    switch (err) {
        case CJSONX_SUCCESS:
            return "success";
        case CJSONX_ERROR_OOM:
            return "out of memory";
        case CJSONX_ERROR_EMPTY_INPUT:
            return "empty input or whitespace only";
        case CJSONX_ERROR_UNTERMINATED_STRING:
            return "unterminated string (missing closing quote)";
        case CJSONX_ERROR_INVALID_CONTROL_CHAR:
            return "invalid raw control character in string";
        case CJSONX_ERROR_INVALID_ESCAPE:
            return "invalid escape sequence inside string";
        case CJSONX_ERROR_INVALID_NUMBER:
            return "invalid json number format";
        case CJSONX_ERROR_INVALID_KEYWORD:
            return "invalid keyword (expected true, false, or null)";
        case CJSONX_ERROR_MAX_DEPTH:
            return "nesting depth limit exceeded";
        case CJSONX_ERROR_MISSING_COMMA:
            return "missing comma separator";
        case CJSONX_ERROR_MISSING_COLON:
            return "missing colon separator";
        case CJSONX_ERROR_TRAILING_COMMA:
            return "trailing comma is not allowed";
        case CJSONX_ERROR_UNEXPECTED_TOKEN:
            return "unexpected token";
        case CJSONX_ERROR_UNCLOSED_CONTAINER:
            return "unclosed container";
        case CJSONX_ERROR_TRAILING_GARBAGE:
            return "trailing garbage after root value";
        case CJSONX_ERROR_INVALID_UTF8:
            return "invalid utf-8 sequence in string";
        case CJSONX_ERROR_TOO_LARGE:
            return "string or container size exceeds 24-bit maximum (16MB/16M elements)";
        default:
            return "unknown error";
    }
}

#endif  // cjsonx_error_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* (*malloc_fn)(size_t size, void* user_data);
    void* (*realloc_fn)(void* ptr, size_t size, void* user_data);
    void (*free_fn)(void* ptr, void* user_data);
    void* user_data;

    // dev note: you could also provide an aligned_alloc function pointer if strict simds
    // needed manually aligned blocks in the future, though the arena handles it well for now.
} cjsonx_allocator_t;

// reallocate memory safely supporting custom/standard fallback
static inline void* cjsonx_realloc(cjsonx_allocator_t* alloc, void* ptr, size_t old_size,
                                   size_t new_size) {
    if (alloc && alloc->realloc_fn) {
        return alloc->realloc_fn(ptr, new_size, alloc->user_data);
    }
    if (alloc && alloc->malloc_fn) {
        void* new_ptr = alloc->malloc_fn(new_size, alloc->user_data);
        if (!new_ptr) return NULL;  // allocation failed — caller keeps old ptr
        if (ptr) {
            memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
            if (alloc->free_fn) alloc->free_fn(ptr, alloc->user_data);
        }
        return new_ptr;
    }
    return realloc(ptr, new_size);
}

typedef enum {
    CJSONX_NULL,
    CJSONX_BOOL,
    CJSONX_NUMBER,
    CJSONX_STRING,
    CJSONX_ARRAY,
    CJSONX_OBJECT
} cjsonx_type_t;

/*
 * 16-byte flat node, packed for cache efficiency.
 * we pack each node into exactly 16 bytes. this means 4 nodes fit perfectly inside a
 * 64-byte cpu cache line, which greatly increases performance during dom walks by minimizing
 * l1/l2 cache misses.
 * - type_and_length: packs the 8-bit node type and a 24-bit length/count (max 16,777,215).
 * - next_sibling: stores the index of the next sibling node in the flat array. this is a
 *   critical performance feature; it allows us to skip an entire subtree (e.g. a large nested
 *   object or array) in o(1) time without recursively visiting its children.
 * - val: a union that overlaps type-specific payloads (double float, string pointer, bool, or
 *   first child index) to keep the memory size constrained to 16 bytes.
 */
typedef struct {
    uint32_t type_and_length;  // 8-bit type | 24-bit length (max 16,777,215)
    uint32_t next_sibling;     // next sibling index for fast skipping
    union {
        double f64;       // number val
        const char* str;  // zero-copy string ptr
        bool b;           // bool val
        struct {
            uint32_t first_child;  // first child index for object/array
            uint32_t last_child;   // last child index (o(1) append)
        };
    } val;
} cjsonx_node_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
// dev note: keeping this struct strictly at 16 bytes is crucial.
// if you ever add new fields, test the size with _Static_assert to ensure it doesn't inflate.
_Static_assert(sizeof(cjsonx_node_t) == 16, "cjsonx_node_t must be 16 bytes for cache alignment");
#endif

static cjsonx_always_inline cjsonx_type_t cjsonx_node_type(const cjsonx_node_t* n) {
    return (cjsonx_type_t)(n->type_and_length & 0xFF);
}

static cjsonx_always_inline uint32_t cjsonx_node_length(const cjsonx_node_t* n) {
    return (n->type_and_length >> 8);
}

static cjsonx_always_inline void cjsonx_node_set_type_len(cjsonx_node_t* __restrict n,
                                                          cjsonx_type_t type, uint32_t length) {
    // silently clamp length to 24-bit maximum (16,777,215).
    // strings longer than 16mb or collections with more than 16m elements will be capped.
    if (CJSONX_UNLIKELY(length > 0xFFFFFF)) length = 0xFFFFFF;
    n->type_and_length = ((uint32_t)type) | (length << 8);
}

// arena chunk node for escaped strings
typedef struct cjsonx_arena_node cjsonx_arena_node_t;
struct cjsonx_arena_node {
    cjsonx_arena_node_t* next;
};

// fwd decl
typedef struct cjsonx_doc cjsonx_doc_t;

// user handle pointing to dom array
typedef struct {
    cjsonx_doc_t* doc;
    uint32_t node_idx;
} cjsonx_val_t;

// parsed document
struct cjsonx_doc {
    cjsonx_val_t root;     // root handle
    cjsonx_node_t* nodes;  // flat dom array
    size_t node_count;     // total nodes
    size_t node_capacity;  // allocated capacity

    const char* original_json;
    size_t json_len;

    bool is_valid;
    cjsonx_error_t error;
    size_t error_offset;

    // string arena
    cjsonx_arena_node_t* head;
    size_t chunk_size;
    size_t chunk_used;
    uint8_t* current_chunk;

    cjsonx_allocator_t alloc;  // custom allocator hooks
    bool is_static;            // true if user provided static memory
    char* owned_json;          // owned copy of json buffer (set by cjsonx_read_file)
};

/*
 * container iterator:
 * enables zero-allocation, non-recursive traversal of elements inside
 * an array or key-value pairs inside an object.
 */
typedef struct {
    cjsonx_doc_t* doc;
    cjsonx_val_t key;
    cjsonx_val_t value;
    uint32_t next_idx;
    uint32_t end_idx;
    bool is_object;
    bool valid;
} cjsonx_iter_t;

// static buffer parsing
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse_with_buffer(const char* json, size_t length,
                                                                   void* buffer,
                                                                   size_t buffer_size);

// lifecycle
CJSONX_API cjsonx_doc_t* cjsonx_doc_new(void);
CJSONX_API cjsonx_doc_t* cjsonx_doc_new_ex(cjsonx_allocator_t* alloc);
CJSONX_API void cjsonx_doc_free(cjsonx_doc_t* doc);

// container lookup
// o(n): linear key scan.
CJSONX_API cjsonx_val_t cjsonx_get(cjsonx_val_t obj, const char* key);
CJSONX_API cjsonx_val_t cjsonx_get_len(cjsonx_val_t obj, const char* key, size_t key_len);
// o(n): walks sibling chain. use cjsonx_iter for sequential iteration.
CJSONX_API cjsonx_val_t cjsonx_get_index(cjsonx_val_t arr, size_t index);
CJSONX_API cjsonx_val_t cjsonx_pointer_get(cjsonx_val_t root, const char* path);

// value accessors
CJSONX_API const char* cjsonx_str(cjsonx_val_t val);
CJSONX_API size_t cjsonx_str_len(cjsonx_val_t val);
CJSONX_API double cjsonx_num(cjsonx_val_t val);
CJSONX_API int64_t cjsonx_int(cjsonx_val_t val);
CJSONX_API bool cjsonx_bool(cjsonx_val_t val);
CJSONX_API bool cjsonx_is_null(cjsonx_val_t val);
CJSONX_API cjsonx_type_t cjsonx_get_type(cjsonx_val_t val);
CJSONX_API size_t cjsonx_size(cjsonx_val_t val);

// iteration
CJSONX_API cjsonx_iter_t cjsonx_iter_init(cjsonx_val_t val);
CJSONX_API bool cjsonx_iter_next(cjsonx_iter_t* iter);

// create null handle
static inline cjsonx_val_t cjsonx_make_null_handle(void) {
    cjsonx_val_t v = {NULL, 0};
    return v;
}

/*
 * check if a value handle points to an actual node (i.e. was found).
 * use this to distinguish "key not found" from "value is json null":
 *   if (!cjsonx_valid(v)) { // key missing }
 *   else if (cjsonx_is_null(v)) { // key exists, value is null }
 */
static inline bool cjsonx_valid(cjsonx_val_t v) {
    return v.doc != NULL;
}

// type and function aliases for compatibility with non-t api
typedef cjsonx_doc_t cjsonx_doc;
typedef cjsonx_val_t cjsonx_val;
typedef cjsonx_iter_t cjsonx_iter;
typedef cjsonx_type_t cjsonx_type;
typedef cjsonx_allocator_t cjsonx_alc;

#ifdef __cplusplus
}
#endif

#endif  // cjsonx_dom_h

#ifdef __cplusplus
extern "C" {
#endif

/*
 * chunk-based arena allocator:
 * allocations are made sequentially from contiguous chunks of memory to avoid the overhead and
 * fragmentation associated with standard malloc/free.
 * 1. sizes are rounded up to 8-byte alignment to prevent misaligned memory access faults on
 * platforms like arm.
 * 2. if the current chunk does not have enough remaining space (or is null), a new chunk of size
 * max(needed, 2 * previous_size) is allocated and linked to the head of the document head list
 * (doc->head).
 * 3. all chunks are cleaned up simultaneously in one o(1) loop when cjsonx_doc_free is called,
 * avoiding tiny frees.
 */
static cjsonx_always_inline void* cjsonx_arena_alloc(cjsonx_doc_t* __restrict doc, size_t size) {
    if (CJSONX_UNLIKELY(size > (size_t)-8)) return NULL;
    // round up to 8-byte alignment for safe struct access
    size = (size + 7) & ~7;

    // current chunk exhausted or first allocation — grow arena with a new chunk
    if (CJSONX_UNLIKELY(!doc->current_chunk || doc->chunk_used + size > doc->chunk_size)) {
        if (CJSONX_UNLIKELY(doc->is_static)) return NULL;  // static buffers cannot be expanded
        size_t min_needed = size + sizeof(cjsonx_arena_node_t);
        if (CJSONX_UNLIKELY(min_needed < size)) return NULL;  // overflow check

        size_t new_chunk_size = doc->chunk_size ? doc->chunk_size * 2 : CJSONX_ARENA_CHUNK_SIZE;
        if (new_chunk_size < min_needed) {
            new_chunk_size = min_needed + CJSONX_ARENA_CHUNK_SIZE;
            if (CJSONX_UNLIKELY(new_chunk_size < min_needed)) {
                new_chunk_size = min_needed;  // handle overflow by allocating exact amount
            }
        }

        cjsonx_arena_node_t* node;
        if (doc->alloc.malloc_fn) {
            node = (cjsonx_arena_node_t*)doc->alloc.malloc_fn(new_chunk_size, doc->alloc.user_data);
        } else {
            node = (cjsonx_arena_node_t*)malloc(new_chunk_size);
        }
        if (CJSONX_UNLIKELY(!node)) return NULL;

        node->next = doc->head;
        doc->head = node;

        doc->current_chunk = (uint8_t*)node + sizeof(cjsonx_arena_node_t);
        doc->chunk_size = new_chunk_size - sizeof(cjsonx_arena_node_t);
        doc->chunk_used = 0;
    }

    void* ptr = doc->current_chunk + doc->chunk_used;
    doc->chunk_used += size;
    return ptr;
}

#ifdef __cplusplus
}
#endif

#endif  // cjsonx_arena_h
// updated 2026-07-09
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
    uint32_t* indices;          // array of 32-bit index offsets (limits input size to 4 gib)
    size_t count;               // number of indices inside the tape
    size_t capacity;            // capacity of the indices array
    bool is_static;             // if true, do not free or realloc
    cjsonx_allocator_t* alloc;  // optional custom allocator
} cjsonx_tape_t;

// alias for compatibility with non-t api
typedef cjsonx_tape_t cjsonx_tape;

// init tape with pre-alloc cap, false on oom
static cjsonx_always_inline bool cjsonx_tape_init(cjsonx_tape_t* tape, size_t capacity,
                                                  cjsonx_allocator_t* alloc) {
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
static cjsonx_always_inline void cjsonx_tape_init_static(cjsonx_tape_t* tape, uint32_t* buffer,
                                                         size_t capacity) {
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
        if (CJSONX_UNLIKELY(new_cap < tape->capacity || new_cap > (size_t)-1 / sizeof(uint32_t)))
            return false;
        uint32_t* new_indices =
            (uint32_t*)cjsonx_realloc(tape->alloc, tape->indices, tape->capacity * sizeof(uint32_t),
                                      new_cap * sizeof(uint32_t));
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

#endif  // cjsonx_tape_h

#ifdef __cplusplus
extern "C" {
#endif

// stage 1: scan json to build structural token tape
// dev note: keeping this decoupled from stage 2 makes testing and benchmarking incredibly easy.
CJSONX_API bool cjsonx_stage1_build_tape(const char* json, size_t length, cjsonx_tape_t* tape);

// main parser entry point
// warning: the input json buffer must outlive the returned document (zero-copy string references).
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse(const char* json, size_t length);
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse_ex(const char* json, size_t length,
                                                          cjsonx_allocator_t* alloc);

// parse owned copy of json buffer
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse_copy(const char* json, size_t length);
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse_copy_ex(const char* json, size_t length,
                                                               cjsonx_allocator_t* alloc);

// parse a null-terminated string safely without double evaluation
static inline CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse_cstr(const char* json) {
    return cjsonx_parse(json, json ? strlen(json) : 0);
}
#define cjsonx_parse_str(json) cjsonx_parse_cstr(json)

// parse a null-terminated string with owned copy
static inline CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse_copy_cstr(const char* json) {
    return cjsonx_parse_copy(json, json ? strlen(json) : 0);
}
#define cjsonx_parse_copy_str(json) cjsonx_parse_copy_cstr(json)

#ifdef __cplusplus
}
#endif

// builder included last so it can see cjsonx_parse_ex declarations
// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_BUILDER_H
#define CJSONX_BUILDER_H

// ██████  ██    ██ ██ ██      ██████  ███████ ██████
// ██   ██ ██    ██ ██ ██      ██   ██ ██      ██   ██
// ██████  ██    ██ ██ ██      ██   ██ █████   ██████
// ██   ██ ██    ██ ██ ██      ██   ██ ██      ██   ██
// ██████   ██████  ██ ███████ ██████  ███████ ██   ██
//
// >>builder api

#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

// allocation internal helper (expands flat arena)
static inline uint32_t cjsonx_builder_alloc_node(cjsonx_doc_t* doc) {
    if (!doc || doc->is_static) return UINT32_MAX;  // static docs cannot be mutated
    if (CJSONX_UNLIKELY(doc->node_count >= UINT32_MAX - 1))
        return UINT32_MAX;  // limit count to prevent uint32 index overflow
    if (doc->node_count >= doc->node_capacity) {
        size_t new_cap = doc->node_capacity == 0 ? 128 : doc->node_capacity * 2;

        // dev note: integer overflow check is spot on. prevents oom vulnerabilities.
        if (CJSONX_UNLIKELY(new_cap < doc->node_capacity ||
                            new_cap > (size_t)-1 / sizeof(cjsonx_node_t)))
            return UINT32_MAX;
        cjsonx_node_t* new_nodes = (cjsonx_node_t*)cjsonx_realloc(
            &doc->alloc, doc->nodes, doc->node_capacity * sizeof(cjsonx_node_t),
            new_cap * sizeof(cjsonx_node_t));
        if (!new_nodes) return UINT32_MAX;
        doc->nodes = new_nodes;
        doc->node_capacity = new_cap;
    }
    return doc->node_count++;
}

// create primitives
static inline cjsonx_val_t cjsonx_create_null(cjsonx_doc_t* doc) {
    uint32_t idx = cjsonx_builder_alloc_node(doc);
    if (idx == UINT32_MAX) return cjsonx_make_null_handle();
    cjsonx_node_set_type_len(&doc->nodes[idx], CJSONX_NULL, 0);
    doc->nodes[idx].next_sibling = idx + 1;  // point to nowhere for now
    cjsonx_val_t v = {doc, idx};
    return v;
}

static inline cjsonx_val_t cjsonx_create_bool(cjsonx_doc_t* doc, bool val) {
    uint32_t idx = cjsonx_builder_alloc_node(doc);
    if (idx == UINT32_MAX) return cjsonx_make_null_handle();
    cjsonx_node_set_type_len(&doc->nodes[idx], CJSONX_BOOL, 0);
    doc->nodes[idx].next_sibling = idx + 1;
    doc->nodes[idx].val.b = val;
    cjsonx_val_t v = {doc, idx};
    return v;
}

static inline cjsonx_val_t cjsonx_create_number(cjsonx_doc_t* doc, double val) {
    uint32_t idx = cjsonx_builder_alloc_node(doc);
    if (idx == UINT32_MAX) return cjsonx_make_null_handle();
    cjsonx_node_set_type_len(&doc->nodes[idx], CJSONX_NUMBER, 0);
    doc->nodes[idx].next_sibling = idx + 1;
    doc->nodes[idx].val.f64 = val;
    cjsonx_val_t v = {doc, idx};
    return v;
}

static inline cjsonx_val_t cjsonx_create_string(cjsonx_doc_t* doc, const char* str) {
    if (!str) str = "";  // treat null as empty string, same as cjsonx_get's behavior
    size_t len = strlen(str);
    if (CJSONX_UNLIKELY(len > 0xFFFFFF)) return cjsonx_make_null_handle();  // string too large
    uint32_t idx = cjsonx_builder_alloc_node(doc);
    if (idx == UINT32_MAX) return cjsonx_make_null_handle();
    cjsonx_node_set_type_len(&doc->nodes[idx], CJSONX_STRING, len);
    doc->nodes[idx].next_sibling = idx + 1;

    // duplicate string into arena
    char* s = (char*)cjsonx_arena_alloc(doc, len + 1);
    if (!s) {
        doc->node_count--;  // rollback allocated node slot on failure
        return cjsonx_make_null_handle();
    }
    memcpy(s, str, len + 1);
    doc->nodes[idx].val.str = s;

    cjsonx_val_t v = {doc, idx};
    return v;
}

static inline cjsonx_val_t cjsonx_create_object(cjsonx_doc_t* doc) {
    uint32_t idx = cjsonx_builder_alloc_node(doc);
    if (idx == UINT32_MAX) return cjsonx_make_null_handle();
    // zero the whole node — realloc doesn't clear, first_child would be garbage
    memset(&doc->nodes[idx], 0, sizeof(cjsonx_node_t));
    cjsonx_node_set_type_len(&doc->nodes[idx], CJSONX_OBJECT, 0);
    doc->nodes[idx].next_sibling = idx + 1;
    cjsonx_val_t v = {doc, idx};
    return v;
}

static inline cjsonx_val_t cjsonx_create_array(cjsonx_doc_t* doc) {
    uint32_t idx = cjsonx_builder_alloc_node(doc);
    if (idx == UINT32_MAX) return cjsonx_make_null_handle();
    // same as object: zero before use so first_child isn't garbage from realloc
    memset(&doc->nodes[idx], 0, sizeof(cjsonx_node_t));
    cjsonx_node_set_type_len(&doc->nodes[idx], CJSONX_ARRAY, 0);
    doc->nodes[idx].next_sibling = idx + 1;
    cjsonx_val_t v = {doc, idx};
    return v;
}

// mutation
static inline bool cjsonx_object_set_len(cjsonx_val_t obj_handle, const char* key, size_t key_len,
                                         cjsonx_val_t val_handle) {
    if (!obj_handle.doc || !val_handle.doc || obj_handle.doc != val_handle.doc) return false;
    if (!key) {
        key = "";
        key_len = 0;
    }
    if (CJSONX_UNLIKELY(key_len > 0xFFFFFF)) return false;  // key too large
    cjsonx_node_t* obj = &obj_handle.doc->nodes[obj_handle.node_idx];
    if (cjsonx_node_type(obj) != CJSONX_OBJECT) return false;

    // if the key already exists, overwrite its value inline to preserve order and key allocations
    uint32_t curr = obj->val.first_child;
    size_t obj_len = cjsonx_node_length(obj);
    uint32_t last_val_idx = UINT32_MAX;
    for (size_t i = 0; i < obj_len; i++) {
        cjsonx_node_t* k_node = &obj_handle.doc->nodes[curr];
        uint32_t val_idx = k_node->next_sibling;
        cjsonx_node_t* v_node = &obj_handle.doc->nodes[val_idx];
        // fast char match avoids memcmp overhead on mismatch
        if (cjsonx_node_length(k_node) == key_len &&
            (key_len == 0 ||
             (k_node->val.str[0] == key[0] && memcmp(k_node->val.str, key, key_len) == 0))) {
            uint32_t next_key_idx = v_node->next_sibling;
            k_node->next_sibling = val_handle.node_idx;
            cjsonx_node_t* new_v_node = &obj_handle.doc->nodes[val_handle.node_idx];
            new_v_node->next_sibling = next_key_idx;
            if (obj->val.last_child == val_idx) {
                obj->val.last_child = val_handle.node_idx;
            }
            return true;
        }
        last_val_idx = val_idx;
        curr = v_node->next_sibling;
    }

    // guard against maximum length limit for 24-bit field
    size_t len = cjsonx_node_length(obj);
    if (CJSONX_UNLIKELY(len >= 0xFFFFFF)) return false;

    // allocate key node — this may realloc doc->nodes (invalidates all node pointers).
    // obj and key_node are re-fetched below after the call.
    uint32_t key_idx = cjsonx_builder_alloc_node(obj_handle.doc);
    if (key_idx == UINT32_MAX) return false;

    cjsonx_node_t* key_node = &obj_handle.doc->nodes[key_idx];
    cjsonx_node_set_type_len(key_node, CJSONX_STRING, key_len);
    char* s = (char*)cjsonx_arena_alloc(obj_handle.doc, key_len + 1);
    if (!s) {
        obj_handle.doc->node_count--;  // rollback allocated node slot on failure
        return false;
    }
    memcpy(s, key, key_len);
    s[key_len] = '\0';
    key_node->val.str = s;

    // re-fetch obj and key_node: alloc_node above may have reallocated doc->nodes
    obj = &obj_handle.doc->nodes[obj_handle.node_idx];
    key_node = &obj_handle.doc->nodes[key_idx];

    // link key and value into object
    key_node->next_sibling = val_handle.node_idx;

    if (len == 0) {
        obj->val.first_child = key_idx;
        obj->val.last_child = val_handle.node_idx;
    } else {
        cjsonx_node_t* last_val = &obj_handle.doc->nodes[last_val_idx];
        last_val->next_sibling = key_idx;
        obj->val.last_child = val_handle.node_idx;
    }

    cjsonx_node_set_type_len(obj, CJSONX_OBJECT, len + 1);
    return true;
}

static inline bool cjsonx_object_set(cjsonx_val_t obj_handle, const char* key,
                                     cjsonx_val_t val_handle) {
    return cjsonx_object_set_len(obj_handle, key, key ? strlen(key) : 0, val_handle);
}

// fast o(1) append without checking for duplicate keys. use only when you are sure the key is
// unique.
static inline bool cjsonx_object_add_unchecked_len(cjsonx_val_t obj_handle, const char* key,
                                                   size_t key_len, cjsonx_val_t val_handle) {
    if (!obj_handle.doc || !val_handle.doc || obj_handle.doc != val_handle.doc) return false;
    if (!key) {
        key = "";
        key_len = 0;
    }
    if (CJSONX_UNLIKELY(key_len > 0xFFFFFF)) return false;  // key too large
    cjsonx_node_t* obj = &obj_handle.doc->nodes[obj_handle.node_idx];
    if (cjsonx_node_type(obj) != CJSONX_OBJECT) return false;

    // guard against maximum length limit for 24-bit field
    size_t len = cjsonx_node_length(obj);
    if (CJSONX_UNLIKELY(len >= 0xFFFFFF)) return false;

    uint32_t last_val_idx = obj->val.last_child;

    uint32_t key_idx = cjsonx_builder_alloc_node(obj_handle.doc);
    if (key_idx == UINT32_MAX) return false;

    cjsonx_node_t* key_node = &obj_handle.doc->nodes[key_idx];
    cjsonx_node_set_type_len(key_node, CJSONX_STRING, key_len);
    char* s = (char*)cjsonx_arena_alloc(obj_handle.doc, key_len + 1);
    if (!s) {
        obj_handle.doc->node_count--;  // rollback allocated node slot on failure
        return false;
    }
    memcpy(s, key, key_len);
    s[key_len] = '\0';
    key_node->val.str = s;

    // re-fetch obj and key_node: alloc_node above may have reallocated doc->nodes
    obj = &obj_handle.doc->nodes[obj_handle.node_idx];
    key_node = &obj_handle.doc->nodes[key_idx];

    // link key and value into object
    key_node->next_sibling = val_handle.node_idx;

    if (len == 0) {
        obj->val.first_child = key_idx;
        obj->val.last_child = val_handle.node_idx;
    } else {
        cjsonx_node_t* last_val = &obj_handle.doc->nodes[last_val_idx];
        last_val->next_sibling = key_idx;
        obj->val.last_child = val_handle.node_idx;
    }

    cjsonx_node_set_type_len(obj, CJSONX_OBJECT, len + 1);
    return true;
}

static inline bool cjsonx_object_add_unchecked(cjsonx_val_t obj_handle, const char* key,
                                               cjsonx_val_t val_handle) {
    return cjsonx_object_add_unchecked_len(obj_handle, key, key ? strlen(key) : 0, val_handle);
}

/*
 * note: array push is o(1) because we keep track of the last child node index
 * inside the object/array node struct, enabling rapid bulk construction.
 */
static inline bool cjsonx_array_push(cjsonx_val_t arr_handle, cjsonx_val_t val_handle) {
    if (!arr_handle.doc || !val_handle.doc || arr_handle.doc != val_handle.doc) return false;
    cjsonx_node_t* arr = &arr_handle.doc->nodes[arr_handle.node_idx];
    if (cjsonx_node_type(arr) != CJSONX_ARRAY) return false;

    // guard against maximum length limit for 24-bit field
    size_t len = cjsonx_node_length(arr);
    if (CJSONX_UNLIKELY(len >= 0xFFFFFF)) return false;

    if (len == 0) {
        arr->val.first_child = val_handle.node_idx;
        arr->val.last_child = val_handle.node_idx;
    } else {
        uint32_t last_idx = arr->val.last_child;
        cjsonx_node_t* last_val = &arr_handle.doc->nodes[last_idx];
        last_val->next_sibling = val_handle.node_idx;
        arr->val.last_child = val_handle.node_idx;
    }

    arr = &arr_handle.doc->nodes[arr_handle.node_idx];
    cjsonx_node_set_type_len(arr, CJSONX_ARRAY, len + 1);
    return true;
}

static inline bool cjsonx_object_remove_len(cjsonx_val_t obj_handle, const char* key,
                                            size_t key_len) {
    if (!obj_handle.doc) return false;
    if (!key) {
        key = "";
        key_len = 0;
    }
    cjsonx_node_t* obj = &obj_handle.doc->nodes[obj_handle.node_idx];
    if (cjsonx_node_type(obj) != CJSONX_OBJECT) return false;

    uint32_t curr = obj->val.first_child;
    uint32_t prev_val_idx =
        UINT32_MAX;  // the value node of the previous pair, which points to current key
    size_t len = cjsonx_node_length(obj);

    for (size_t i = 0; i < len; i++) {
        cjsonx_node_t* k_node = &obj_handle.doc->nodes[curr];
        uint32_t val_idx = k_node->next_sibling;
        cjsonx_node_t* v_node = &obj_handle.doc->nodes[val_idx];

        // fast char match avoids memcmp overhead on mismatch
        if (cjsonx_node_length(k_node) == key_len &&
            (key_len == 0 ||
             (k_node->val.str[0] == key[0] && memcmp(k_node->val.str, key, key_len) == 0))) {
            // found it. link previous sibling to the next sibling (skipping both key and val)
            uint32_t next_key_idx = v_node->next_sibling;
            if (prev_val_idx == UINT32_MAX) {
                obj->val.first_child = next_key_idx;
            } else {
                obj_handle.doc->nodes[prev_val_idx].next_sibling = next_key_idx;
            }
            if (obj->val.last_child == val_idx) {
                obj->val.last_child = prev_val_idx;
            }
            cjsonx_node_set_type_len(obj, CJSONX_OBJECT, len - 1);
            return true;
        }
        prev_val_idx = val_idx;
        curr = v_node->next_sibling;
    }
    return false;
}

static inline bool cjsonx_object_remove(cjsonx_val_t obj_handle, const char* key) {
    return cjsonx_object_remove_len(obj_handle, key, key ? strlen(key) : 0);
}

static inline bool cjsonx_array_remove(cjsonx_val_t arr_handle, size_t index) {
    if (!arr_handle.doc) return false;
    cjsonx_node_t* arr = &arr_handle.doc->nodes[arr_handle.node_idx];
    if (cjsonx_node_type(arr) != CJSONX_ARRAY) return false;

    size_t len = cjsonx_node_length(arr);
    if (index >= len) return false;

    uint32_t curr = arr->val.first_child;
    uint32_t prev_idx = UINT32_MAX;

    for (size_t i = 0; i < index; i++) {
        prev_idx = curr;
        curr = arr_handle.doc->nodes[curr].next_sibling;
    }

    uint32_t next_idx = arr_handle.doc->nodes[curr].next_sibling;
    if (prev_idx == UINT32_MAX) {
        arr->val.first_child = next_idx;
    } else {
        arr_handle.doc->nodes[prev_idx].next_sibling = next_idx;
    }
    if (arr->val.last_child == curr) {
        arr->val.last_child = prev_idx;
    }

    cjsonx_node_set_type_len(arr, CJSONX_ARRAY, len - 1);
    return true;
}

// stringify and utility declarations
CJSONX_API char* cjsonx_stringify(cjsonx_doc_t* doc);
CJSONX_API char* cjsonx_stringify_format(cjsonx_doc_t* doc, int indent_spaces);
CJSONX_API char* cjsonx_stringify_val(cjsonx_val_t val);
CJSONX_API char* cjsonx_stringify_val_format(cjsonx_val_t val, int indent_spaces);
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_read_file(const char* path);
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_read_file_ex(const char* path,
                                                              cjsonx_allocator_t* alloc);
CJSONX_API bool cjsonx_write_file(const char* path, cjsonx_doc_t* doc);
CJSONX_API bool cjsonx_write_file_format(const char* path, cjsonx_doc_t* doc, int indent_spaces);
CJSONX_API cjsonx_val_t cjsonx_clone_val(cjsonx_doc_t* dest_doc, cjsonx_val_t src_val);
CJSONX_API cjsonx_val_t cjsonx_merge_patch(cjsonx_val_t target, cjsonx_val_t patch);

#ifdef CJSONX_IMPLEMENTATION

#define CJSONX_FRACMASK 0x000FFFFFFFFFFFFFU
#define CJSONX_EXPMASK 0x7FF0000000000000U
#define CJSONX_HIDDENBIT 0x0010000000000000U
#define CJSONX_SIGNMASK 0x8000000000000000U
#define CJSONX_EXPBIAS (1023 + 52)

#define CJSONX_ABS(n) ((n) < 0 ? -(n) : (n))
#define CJSONX_MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    uint64_t frac;
    int exp;
} cjsonx_fp_t;

// powers of ten table for grisu2
static const cjsonx_fp_t cjsonx_powers_ten[] = {
    {18054884314459144840U, -1220}, {13451937075301367670U, -1193}, {10022474136428063862U, -1166},
    {14934650266808366570U, -1140}, {11127181549972568877U, -1113}, {16580792590934885855U, -1087},
    {12353653155963782858U, -1060}, {18408377700990114895U, -1034}, {13715310171984221708U, -1007},
    {10218702384817765436U, -980},  {15227053142812498563U, -954},  {11345038669416679861U, -927},
    {16905424996341287883U, -901},  {12595523146049147757U, -874},  {9384396036005875287U, -847},
    {13983839803942852151U, -821},  {10418772551374772303U, -794},  {15525180923007089351U, -768},
    {11567161174868858868U, -741},  {17236413322193710309U, -715},  {12842128665889583758U, -688},
    {9568131466127621947U, -661},   {14257626930069360058U, -635},  {10622759856335341974U, -608},
    {15829145694278690180U, -582},  {11793632577567316726U, -555},  {17573882009934360870U, -529},
    {13093562431584567480U, -502},  {9755464219737475723U, -475},   {14536774485912137811U, -449},
    {10830740992659433045U, -422},  {16139061738043178685U, -396},  {12024538023802026127U, -369},
    {17917957937422433684U, -343},  {13349918974505688015U, -316},  {9946464728195732843U, -289},
    {14821387422376473014U, -263},  {11042794154864902060U, -236},  {16455045573212060422U, -210},
    {12259964326927110867U, -183},  {18268770466636286478U, -157},  {13611294676837538539U, -130},
    {10141204801825835212U, -103},  {15111572745182864684U, -77},   {11258999068426240000U, -50},
    {16777216000000000000U, -24},   {12500000000000000000U, 3},     {9313225746154785156U, 30},
    {13877787807814456755U, 56},    {10339757656912845936U, 83},    {15407439555097886824U, 109},
    {11479437019748901445U, 136},   {17105694144590052135U, 162},   {12744735289059618216U, 189},
    {9495567745759798747U, 216},    {14149498560666738074U, 242},   {10542197943230523224U, 269},
    {15709099088952724970U, 295},   {11704190886730495818U, 322},   {17440603504673385349U, 348},
    {12994262207056124023U, 375},   {9681479787123295682U, 402},    {14426529090290212157U, 428},
    {10748601772107342003U, 455},   {16016664761464807395U, 481},   {11933345169920330789U, 508},
    {17782069995880619868U, 534},   {13248674568444952270U, 561},   {9871031767461413346U, 588},
    {14708983551653345445U, 614},   {10959046745042015199U, 641},   {16330252207878254650U, 667},
    {12166986024289022870U, 694},   {18130221999122236476U, 720},   {13508068024458167312U, 747},
    {10064294952495520794U, 774},   {14996968138956309548U, 800},   {11173611982879273257U, 827},
    {16649979327439178909U, 853},   {12405201291620119593U, 880},   {9242595204427927429U, 907},
    {13772540099066387757U, 933},   {10261342003245940623U, 960},   {15290591125556738113U, 986},
    {11392378155556871081U, 1013},  {16975966327722178521U, 1039},  {12648080533535911531U, 1066}};

static const uint64_t cjsonx_tens[] = {10000000000000000000U,
                                       1000000000000000000U,
                                       100000000000000000U,
                                       10000000000000000U,
                                       1000000000000000U,
                                       100000000000000U,
                                       10000000000000U,
                                       1000000000000U,
                                       100000000000U,
                                       10000000000U,
                                       1000000000U,
                                       100000000U,
                                       10000000U,
                                       1000000U,
                                       100000U,
                                       10000U,
                                       1000U,
                                       100U,
                                       10U,
                                       1U};

static inline uint64_t cjsonx_get_dbits(double d) {
    union {
        double dbl;
        uint64_t i;
    } dbl_bits = {d};
    return dbl_bits.i;
}

static cjsonx_fp_t cjsonx_build_fp(double d) {
    uint64_t bits = cjsonx_get_dbits(d);
    cjsonx_fp_t fp;
    fp.frac = bits & CJSONX_FRACMASK;
    fp.exp = (bits & CJSONX_EXPMASK) >> 52;
    if (fp.exp) {
        fp.frac += CJSONX_HIDDENBIT;
        fp.exp -= CJSONX_EXPBIAS;
    } else {
        fp.exp = -CJSONX_EXPBIAS + 1;
    }
    return fp;
}

static void cjsonx_normalize(cjsonx_fp_t* fp) {
    if (fp->frac == 0) return;  // clzll(0) is undefined behavior; nothing to normalize
    // collapse the old two-step normalization into a single shift:
    //   step 1 (old while loop): align leading 1 to bit 52 → shift = clzll - 11
    //   step 2 (old fixed shift): align to bit 63            → shift += 11
    //   combined:                                              shift = clzll(frac)
    int shift = CJSONX_CLZLL(fp->frac);
    fp->frac <<= shift;
    fp->exp -= shift;
}

static void cjsonx_get_normalized_boundaries(cjsonx_fp_t* fp, cjsonx_fp_t* lower,
                                             cjsonx_fp_t* upper) {
    upper->frac = (fp->frac << 1) + 1;
    upper->exp = fp->exp - 1;
    while ((upper->frac & (CJSONX_HIDDENBIT << 1)) == 0) {
        upper->frac <<= 1;
        upper->exp--;
    }
    int u_shift = 64 - 52 - 2;
    upper->frac <<= u_shift;
    upper->exp = upper->exp - u_shift;

    int l_shift = fp->frac == CJSONX_HIDDENBIT ? 2 : 1;
    lower->frac = (fp->frac << l_shift) - 1;
    lower->exp = fp->exp - l_shift;
    lower->frac <<= lower->exp - upper->exp;
    lower->exp = upper->exp;
}

static cjsonx_fp_t cjsonx_multiply(cjsonx_fp_t* a, cjsonx_fp_t* b) {
    const uint64_t lomask = 0x00000000FFFFFFFF;
    uint64_t ah_bl = (a->frac >> 32) * (b->frac & lomask);
    uint64_t al_bh = (a->frac & lomask) * (b->frac >> 32);
    uint64_t al_bl = (a->frac & lomask) * (b->frac & lomask);
    uint64_t ah_bh = (a->frac >> 32) * (b->frac >> 32);
    uint64_t tmp = (ah_bl & lomask) + (al_bh & lomask) + (al_bl >> 32);
    tmp += 1U << 31;
    cjsonx_fp_t fp = {ah_bh + (ah_bl >> 32) + (al_bh >> 32) + (tmp >> 32), a->exp + b->exp + 64};
    return fp;
}

static cjsonx_fp_t cjsonx_find_cachedpow10(int exp, int* k) {
    const double one_log_ten = 0.30102999566398114;
    int approx = -(exp + 87) * one_log_ten;
    int idx = (approx - (-348)) / 8;
    while (1) {
        if (idx < 0) idx = 0;
        if (idx >= 87) idx = 86;
        int current = exp + cjsonx_powers_ten[idx].exp + 64;
        if (current < -60) {
            if (idx == 86) {
                *k = (-348 + idx * 8);
                return cjsonx_powers_ten[idx];
            }
            idx++;
            continue;
        }
        if (current > -32) {
            if (idx == 0) {
                *k = (-348 + idx * 8);
                return cjsonx_powers_ten[idx];
            }
            idx--;
            continue;
        }
        *k = (-348 + idx * 8);
        return cjsonx_powers_ten[idx];
    }
}

static void cjsonx_round_digit(char* digits, int ndigits, uint64_t delta, uint64_t rem,
                               uint64_t kappa, uint64_t frac) {
    while (rem < frac && delta - rem >= kappa &&
           (rem + kappa < frac || frac - rem > rem + kappa - frac)) {
        digits[ndigits - 1]--;
        rem += kappa;
    }
}

static int cjsonx_generate_digits(cjsonx_fp_t* fp, cjsonx_fp_t* upper, cjsonx_fp_t* lower,
                                  char* digits, int* K) {
    uint64_t wfrac = upper->frac - fp->frac;
    uint64_t delta = upper->frac - lower->frac;
    cjsonx_fp_t one;
    one.frac = 1ULL << -upper->exp;
    one.exp = upper->exp;
    uint64_t part1 = upper->frac >> -one.exp;
    uint64_t part2 = upper->frac & (one.frac - 1);
    int idx = 0, kappa = 10;
    const uint64_t* divp;
    for (divp = cjsonx_tens + 10; kappa > 0; divp++) {
        uint64_t div = *divp;
        unsigned digit = part1 / div;
        if (digit || idx) {
            digits[idx++] = digit + '0';
        }
        part1 -= digit * div;
        kappa--;
        uint64_t tmp = (part1 << -one.exp) + part2;
        if (tmp <= delta) {
            *K += kappa;
            cjsonx_round_digit(digits, idx, delta, tmp, div << -one.exp, wfrac);
            return idx;
        }
    }
    const uint64_t* unit = cjsonx_tens + 18;
    while (true) {
        part2 *= 10;
        delta *= 10;
        kappa--;
        unsigned digit = part2 >> -one.exp;
        if (digit || idx) {
            digits[idx++] = digit + '0';
        }
        part2 &= one.frac - 1;
        if (part2 < delta) {
            *K += kappa;
            cjsonx_round_digit(digits, idx, delta, part2, one.frac, wfrac * *unit);
            return idx;
        }
        if (unit > cjsonx_tens) {
            unit--;
        }
    }
}

/*
 * grisu2 algorithm (florian loitsch):
 * an extremely fast, high-precision double-to-string formatting algorithm.
 *
 * 1. float decomposition: builds a custom floating-point struct (frac and exp)
 *    and calculates the exact boundaries (lower and upper) of the float value.
 * 2. scaling: multiplies the float and its boundaries by a cached power of 10
 *    to align the exponent with a predefined boundary range.
 * 3. digit generation: generates the shortest decimal representation by sequentially
 *    extracting digits from the scaled fraction and rounding them.
 */
static int cjsonx_grisu2(double d, char* digits, int* K) {
    cjsonx_fp_t w = cjsonx_build_fp(d);
    cjsonx_fp_t lower, upper;
    cjsonx_get_normalized_boundaries(&w, &lower, &upper);
    cjsonx_normalize(&w);
    int k;
    cjsonx_fp_t cp = cjsonx_find_cachedpow10(upper.exp, &k);
    w = cjsonx_multiply(&w, &cp);
    upper = cjsonx_multiply(&upper, &cp);
    lower = cjsonx_multiply(&lower, &cp);
    lower.frac++;
    upper.frac--;
    *K = -k;
    return cjsonx_generate_digits(&w, &upper, &lower, digits, K);
}

static int cjsonx_emit_digits(char* digits, int ndigits, char* dest, int K, bool neg) {
    int exp = CJSONX_ABS(K + ndigits - 1);
    int max_trailing_zeros = 7;
    if (neg) {
        max_trailing_zeros -= 1;
    }
    // write plain integer
    if (K >= 0 && (exp < (ndigits + max_trailing_zeros))) {
        memcpy(dest, digits, ndigits);
        memset(dest + ndigits, '0', K);
        return ndigits + K;
    }
    // write decimal without scientific notation
    if (K < 0 && (K > -7 || exp < 4)) {
        int offset = ndigits - CJSONX_ABS(K);
        // fp < 1.0 -> write leading zero
        if (offset <= 0) {
            offset = -offset;
            dest[0] = '0';
            dest[1] = '.';
            memset(dest + 2, '0', offset);
            memcpy(dest + offset + 2, digits, ndigits);
            return ndigits + 2 + offset;
        } else {
            memcpy(dest, digits, offset);
            dest[offset] = '.';
            memcpy(dest + offset + 1, digits + offset, ndigits - offset);
            return ndigits + 1;
        }
    }
    // write decimal with scientific notation
    ndigits = CJSONX_MIN(ndigits, 18 - neg);
    int idx = 0;
    dest[idx++] = digits[0];
    if (ndigits > 1) {
        dest[idx++] = '.';
        memcpy(dest + idx, digits + 1, ndigits - 1);
        idx += ndigits - 1;
    }
    dest[idx++] = 'e';
    char sign = K + ndigits - 1 < 0 ? '-' : '+';
    dest[idx++] = sign;
    int cent = 0;
    if (exp > 99) {
        cent = exp / 100;
        dest[idx++] = cent + '0';
        exp -= cent * 100;
    }
    if (exp > 9) {
        int dec = exp / 10;
        dest[idx++] = dec + '0';
        exp -= dec * 10;
    } else if (cent) {
        dest[idx++] = '0';
    }
    dest[idx++] = exp % 10 + '0';
    return idx;
}

static inline int cjsonx_fpconv_dtoa(double d, char dest[24]) {
    uint64_t bits = cjsonx_get_dbits(d);
    if ((bits & CJSONX_EXPMASK) == CJSONX_EXPMASK) {
        // nan or infinity: output null (strictly conforms to rfc 8259, no sign)
        dest[0] = 'n';
        dest[1] = 'u';
        dest[2] = 'l';
        dest[3] = 'l';
        return 4;
    }

    char digits[18];
    int str_len = 0;
    bool neg = false;
    if (bits & CJSONX_SIGNMASK) {
        dest[0] = '-';
        str_len++;
        neg = true;
    }
    if (d == 0.0) {
        dest[str_len] = '0';
        return str_len + 1;
    }
    int K = 0;
    int ndigits = cjsonx_grisu2(d, digits, &K);
    str_len += cjsonx_emit_digits(digits, ndigits, dest + str_len, K, neg);
    return str_len;
}

static const char cjsonx_digit_table[] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

/*
 * high-performance 64-bit integer formatter:
 * writes digits 2-at-a-time using a precomputed two-digit lookup table
 * (cjsonx_digit_table). this avoids half of the division/modulo operations
 * compared to standard digit-by-digit loops, substantially improving
 * stringification performance for integer values.
 */
// max int64_t is 19 digits + optional '-' sign = 20 chars. buf is 24 — safe.
static inline int cjsonx_write_i64(char* buf, int64_t val) {
    if (val == 0) {
        buf[0] = '0';
        return 1;
    }
    int len = 0;
    uint64_t uval;
    if (val < 0) {
        buf[0] = '-';
        len = 1;
        uval = (uint64_t)0 - (uint64_t)val;
    } else {
        uval = (uint64_t)val;
    }
    char temp[24];
    // _Static_assert would require c11. temp is 24; max usage is 20 chars (19 digits + sign).
    int t_idx = 24;
    while (uval >= 100) {
        uint32_t val2 = (uint32_t)(uval % 100);
        uval /= 100;
        temp[--t_idx] = cjsonx_digit_table[val2 * 2 + 1];
        temp[--t_idx] = cjsonx_digit_table[val2 * 2];
    }
    if (uval >= 10) {
        temp[--t_idx] = cjsonx_digit_table[uval * 2 + 1];
        temp[--t_idx] = cjsonx_digit_table[uval * 2];
    } else {
        temp[--t_idx] = (char)('0' + uval);
    }
    int bytes = 24 - t_idx;
    memcpy(buf + len, temp + t_idx, bytes);
    return len + bytes;
}
static const uint8_t cjsonx_need_escape[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0-15
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 16-31
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 32-47 (34 is '"')
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 48-63
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 64-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,  // 80-95 (92 is '\\')
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

typedef struct {
    char* buf;
    size_t len;
    size_t cap;
    cjsonx_allocator_t* alloc;
    bool oom;
} cjsonx_strbuf_t;

static cjsonx_always_inline void cjsonx_strbuf_append(cjsonx_strbuf_t* __restrict sb,
                                                      const char* __restrict str, size_t len) {
    if (CJSONX_UNLIKELY(sb->oom)) return;
    // unsigned wraparound: if sum < either operand, an overflow occurred
    if (CJSONX_UNLIKELY(sb->len + len < sb->len)) {
        sb->oom = true;
        return;
    }
    if (CJSONX_UNLIKELY(sb->len + len >= sb->cap)) {
        size_t new_cap = sb->cap == 0 ? CJSONX_INITIAL_STRBUF_CAP : sb->cap * 2;
        if (CJSONX_UNLIKELY(new_cap <= sb->len + len)) {
            new_cap = sb->len + len + CJSONX_STRBUF_GROW_MARGIN;
            // overflow guard for + 1024
            if (CJSONX_UNLIKELY(new_cap < sb->len + len)) new_cap = sb->len + len;
        }
        char* new_buf = (char*)cjsonx_realloc(sb->alloc, sb->buf, sb->cap, new_cap);
        if (CJSONX_UNLIKELY(!new_buf)) {
            sb->oom = true;
            return;
        }
        sb->buf = new_buf;
        sb->cap = new_cap;
    }
    memcpy(sb->buf + sb->len, str, len);
    sb->len += len;
}

static cjsonx_always_inline void cjsonx_strbuf_append_c(cjsonx_strbuf_t* __restrict sb, char c) {
    if (CJSONX_UNLIKELY(sb->oom)) return;
    if (CJSONX_UNLIKELY(sb->len + 1 < sb->len)) {  // overflow check
        sb->oom = true;
        return;
    }
    if (CJSONX_UNLIKELY(sb->len + 1 >= sb->cap)) {
        size_t new_cap = sb->cap == 0 ? CJSONX_INITIAL_STRBUF_CAP : sb->cap * 2;
        if (CJSONX_UNLIKELY(new_cap < sb->cap)) new_cap = sb->len + 1;  // clamp on overflow
        char* new_buf = (char*)cjsonx_realloc(sb->alloc, sb->buf, sb->cap, new_cap);
        if (CJSONX_UNLIKELY(!new_buf)) {
            sb->oom = true;
            return;
        }
        sb->buf = new_buf;
        sb->cap = new_cap;
    }
    sb->buf[sb->len++] = c;
}

static void cjsonx_stringify_string(cjsonx_strbuf_t* sb, const char* str, size_t len) {
    cjsonx_strbuf_append_c(sb, '"');
    if (len == 0) {
        cjsonx_strbuf_append_c(sb, '"');
        return;
    }

    size_t i = 0;
    uint64_t escape_mask = 0;

    // quick scan to see if there are any escape characters
    while (i + 8 <= len) {
        uint64_t w;
        memcpy(&w, str + i, 8);
        uint64_t low_chars = (w - 0x2020202020202020ULL) & ~w & 0x8080808080808080ULL;
        uint64_t x1 = w ^ 0x2222222222222222ULL;
        uint64_t quote_chars = (x1 - 0x0101010101010101ULL) & ~x1 & 0x8080808080808080ULL;
        uint64_t x2 = w ^ 0x5C5C5C5C5C5C5C5CULL;
        uint64_t slash_chars = (x2 - 0x0101010101010101ULL) & ~x2 & 0x8080808080808080ULL;
        escape_mask |= (low_chars | quote_chars | slash_chars);
        i += 8;
    }
    for (size_t j = i; j < len; j++) {
        unsigned char c = (unsigned char)str[j];
        if (cjsonx_need_escape[c]) {
            escape_mask |= 1;
        }
    }

    if (escape_mask == 0) {
        // fast path: absolutely no escape characters
        cjsonx_strbuf_append(sb, str, len);
        cjsonx_strbuf_append_c(sb, '"');
        return;
    }

    // slow path: process escapes
    size_t start = 0;
    i = 0;
    while (i + 8 <= len) {
        uint64_t w;
        memcpy(&w, str + i, 8);
        uint64_t low_chars = (w - 0x2020202020202020ULL) & ~w & 0x8080808080808080ULL;
        uint64_t x1 = w ^ 0x2222222222222222ULL;
        uint64_t quote_chars = (x1 - 0x0101010101010101ULL) & ~x1 & 0x8080808080808080ULL;
        uint64_t x2 = w ^ 0x5C5C5C5C5C5C5C5CULL;
        uint64_t slash_chars = (x2 - 0x0101010101010101ULL) & ~x2 & 0x8080808080808080ULL;
        if (low_chars | quote_chars | slash_chars) {
            for (size_t j = 0; j < 8; j++) {
                unsigned char c = (unsigned char)str[i + j];
                if (cjsonx_need_escape[c]) {
                    if (i + j > start) {
                        cjsonx_strbuf_append(sb, str + start, (i + j) - start);
                    }
                    switch (c) {
                        case '"':
                            cjsonx_strbuf_append(sb, "\\\"", 2);
                            break;
                        case '\\':
                            cjsonx_strbuf_append(sb, "\\\\", 2);
                            break;
                        case '\b':
                            cjsonx_strbuf_append(sb, "\\b", 2);
                            break;
                        case '\f':
                            cjsonx_strbuf_append(sb, "\\f", 2);
                            break;
                        case '\n':
                            cjsonx_strbuf_append(sb, "\\n", 2);
                            break;
                        case '\r':
                            cjsonx_strbuf_append(sb, "\\r", 2);
                            break;
                        case '\t':
                            cjsonx_strbuf_append(sb, "\\t", 2);
                            break;
                        default: {
                            char hex[7];
                            hex[0] = '\\';
                            hex[1] = 'u';
                            hex[2] = '0';
                            hex[3] = '0';
                            static const char hex_chars[] = "0123456789abcdef";
                            hex[4] = hex_chars[(c >> 4) & 0xF];
                            hex[5] = hex_chars[c & 0xF];
                            cjsonx_strbuf_append(sb, hex, 6);
                            break;
                        }
                    }
                    start = i + j + 1;
                }
            }
        }
        i += 8;
    }

    for (; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (cjsonx_need_escape[c]) {
            if (i > start) {
                cjsonx_strbuf_append(sb, str + start, i - start);
            }
            switch (c) {
                case '"':
                    cjsonx_strbuf_append(sb, "\\\"", 2);
                    break;
                case '\\':
                    cjsonx_strbuf_append(sb, "\\\\", 2);
                    break;
                case '\b':
                    cjsonx_strbuf_append(sb, "\\b", 2);
                    break;
                case '\f':
                    cjsonx_strbuf_append(sb, "\\f", 2);
                    break;
                case '\n':
                    cjsonx_strbuf_append(sb, "\\n", 2);
                    break;
                case '\r':
                    cjsonx_strbuf_append(sb, "\\r", 2);
                    break;
                case '\t':
                    cjsonx_strbuf_append(sb, "\\t", 2);
                    break;
                default: {
                    char hex[7];
                    hex[0] = '\\';
                    hex[1] = 'u';
                    hex[2] = '0';
                    hex[3] = '0';
                    static const char hex_chars[] = "0123456789abcdef";
                    hex[4] = hex_chars[(c >> 4) & 0xF];
                    hex[5] = hex_chars[c & 0xF];
                    cjsonx_strbuf_append(sb, hex, 6);
                    break;
                }
            }
            start = i + 1;
        }
    }

    if (len > start) {
        cjsonx_strbuf_append(sb, str + start, len - start);
    }
    cjsonx_strbuf_append_c(sb, '"');
}

static inline void cjsonx_strbuf_indent(cjsonx_strbuf_t* sb, int indent, int level) {
    if (indent > 0) {
        // bulk-append spaces: write up to 64 bytes at a time instead of one char per call
        static const char spaces[64] = {
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
        cjsonx_strbuf_append_c(sb, '\n');
        int n = level * indent;
        while (n > 0) {
            int chunk = n < 64 ? n : 64;
            cjsonx_strbuf_append(sb, spaces, (size_t)chunk);
            n -= chunk;
        }
    }
}

static void cjsonx_stringify_node(cjsonx_strbuf_t* sb, cjsonx_val_t val, int indent, int level) {
    if (!val.doc) return;
    // guard against stack overflow on deeply nested input (matches parser limit)
    if (level >= CJSONX_MAX_DEPTH) {
        cjsonx_strbuf_append(sb, "null", 4);
        return;
    }
    cjsonx_node_t* n = &val.doc->nodes[val.node_idx];
    cjsonx_type_t t = cjsonx_node_type(n);

    switch (t) {
        case CJSONX_NULL:
            cjsonx_strbuf_append(sb, "null", 4);
            break;
        case CJSONX_BOOL:
            if (n->val.b)
                cjsonx_strbuf_append(sb, "true", 4);
            else
                cjsonx_strbuf_append(sb, "false", 5);
            break;
        case CJSONX_NUMBER: {
            double val_num = n->val.f64;
            union {
                double d;
                uint64_t u;
            } uval = {val_num};
            if (uval.u == 0x8000000000000000ULL) {
                cjsonx_strbuf_append(sb, "-0", 2);
            } else if (val_num >= -9007199254740991.0 && val_num <= 9007199254740991.0 &&
                       val_num == (double)(int64_t)val_num) {
                char num[24];
                int len = cjsonx_write_i64(num, (int64_t)val_num);
                cjsonx_strbuf_append(sb, num, len);
            } else {
                char num[32];
                int len = cjsonx_fpconv_dtoa(val_num, num);
                cjsonx_strbuf_append(sb, num, len);
            }
            break;
        }
        case CJSONX_STRING:
            cjsonx_stringify_string(sb, n->val.str, cjsonx_node_length(n));
            break;
        case CJSONX_ARRAY: {
            size_t len = cjsonx_node_length(n);
            if (len == 0) {
                cjsonx_strbuf_append(sb, "[]", 2);
                break;
            }
            cjsonx_strbuf_append_c(sb, '[');
            uint32_t curr = n->val.first_child;
            for (size_t i = 0; i < len; i++) {
                if (i > 0) cjsonx_strbuf_append_c(sb, ',');
                if (indent > 0) cjsonx_strbuf_indent(sb, indent, level + 1);
                cjsonx_stringify_node(sb, (cjsonx_val_t){val.doc, curr}, indent, level + 1);
                curr = val.doc->nodes[curr].next_sibling;
            }
            if (indent > 0) cjsonx_strbuf_indent(sb, indent, level);
            cjsonx_strbuf_append_c(sb, ']');
            break;
        }
        case CJSONX_OBJECT: {
            size_t len = cjsonx_node_length(n);
            if (len == 0) {
                cjsonx_strbuf_append(sb, "{}", 2);
                break;
            }
            cjsonx_strbuf_append_c(sb, '{');
            uint32_t curr = n->val.first_child;
            for (size_t i = 0; i < len; i++) {
                if (i > 0) cjsonx_strbuf_append_c(sb, ',');
                if (indent > 0) cjsonx_strbuf_indent(sb, indent, level + 1);
                cjsonx_node_t* kn = &val.doc->nodes[curr];
                cjsonx_stringify_string(sb, kn->val.str, cjsonx_node_length(kn));
                cjsonx_strbuf_append_c(sb, ':');
                if (indent > 0) cjsonx_strbuf_append_c(sb, ' ');
                uint32_t val_idx = kn->next_sibling;
                cjsonx_stringify_node(sb, (cjsonx_val_t){val.doc, val_idx}, indent, level + 1);
                curr = val.doc->nodes[val_idx].next_sibling;
            }
            if (indent > 0) cjsonx_strbuf_indent(sb, indent, level);
            cjsonx_strbuf_append_c(sb, '}');
            break;
        }
    }
}

char* cjsonx_stringify_val(cjsonx_val_t val) {
    // delegate to the format variant with zero indent (compact output)
    return cjsonx_stringify_val_format(val, 0);
}

char* cjsonx_stringify_val_format(cjsonx_val_t val, int indent_spaces) {
    if (!val.doc) return NULL;
    // estimate size based on parsed json length or node count with a 2kb floor
    size_t initial_cap = val.doc->json_len > 0 ? val.doc->json_len : val.doc->node_count * 16;
    if (indent_spaces > 0) initial_cap += initial_cap / 2;
    if (initial_cap < CJSONX_INITIAL_STRBUF_CAP) initial_cap = CJSONX_INITIAL_STRBUF_CAP;

    cjsonx_strbuf_t sb;
    sb.cap = initial_cap;
    sb.len = 0;
    sb.alloc = &val.doc->alloc;
    sb.oom = false;
    if (sb.alloc && sb.alloc->malloc_fn) {
        sb.buf = (char*)sb.alloc->malloc_fn(sb.cap, sb.alloc->user_data);
    } else {
        sb.buf = (char*)malloc(sb.cap);
    }
    if (!sb.buf) return NULL;

    cjsonx_stringify_node(&sb, val, indent_spaces, 0);
    cjsonx_strbuf_append_c(&sb, '\0');
    if (sb.oom) {
        if (sb.buf) {
            if (sb.alloc && sb.alloc->free_fn)
                sb.alloc->free_fn(sb.buf, sb.alloc->user_data);
            else
                free(sb.buf);
        }
        return NULL;
    }
    return sb.buf;
}

char* cjsonx_stringify(cjsonx_doc_t* doc) {
    if (!doc || !doc->is_valid) return NULL;
    return cjsonx_stringify_val(doc->root);
}

char* cjsonx_stringify_format(cjsonx_doc_t* doc, int indent_spaces) {
    if (!doc || !doc->is_valid) return NULL;
    return cjsonx_stringify_val_format(doc->root, indent_spaces);
}

cjsonx_doc_t* cjsonx_read_file_ex(const char* path, cjsonx_allocator_t* alloc) {
    if (!path) return NULL;

    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* ftell returns long, which caps file size at 2gb on 32-bit platforms.
     * also reject files larger than 4gb to prevent tape index overflow.
     */
    if (fsize < 0 || (unsigned long)fsize > UINT32_MAX || (unsigned long)fsize >= (size_t)-1) {
        fclose(fp);
        return NULL;
    }

    char* string;
    if (alloc && alloc->malloc_fn) {
        string = (char*)alloc->malloc_fn((size_t)fsize + 1, alloc->user_data);
    } else {
        string = (char*)malloc((size_t)fsize + 1);
    }
    if (!string) {
        fclose(fp);
        return NULL;
    }

    size_t read_bytes = fread(string, 1, fsize, fp);
    fclose(fp);

    if (read_bytes != (size_t)fsize) {
        if (alloc && alloc->free_fn)
            alloc->free_fn(string, alloc->user_data);
        else
            free(string);
        return NULL;
    }
    string[fsize] = '\0';

    cjsonx_doc_t* doc = cjsonx_parse_ex(string, fsize, alloc);

    // transfer buffer ownership to the document so zero-copy string pointers
    // remain valid for the lifetime of the doc.
    if (doc) {
        doc->owned_json = string;
        doc->original_json = string;
    } else {
        if (alloc && alloc->free_fn)
            alloc->free_fn(string, alloc->user_data);
        else
            free(string);
    }

    return doc;
}

cjsonx_doc_t* cjsonx_read_file(const char* path) {
    return cjsonx_read_file_ex(path, NULL);
}

bool cjsonx_write_file(const char* path, cjsonx_doc_t* doc) {
    if (!path || !doc) return false;

    char* json_str = cjsonx_stringify(doc);
    if (!json_str) return false;

    FILE* fp = fopen(path, "wb");
    if (!fp) {
        if (doc->alloc.free_fn)
            doc->alloc.free_fn(json_str, doc->alloc.user_data);
        else
            free(json_str);
        return false;
    }

    size_t len = strlen(json_str);
    size_t written = fwrite(json_str, 1, len, fp);
    fclose(fp);
    if (doc->alloc.free_fn)
        doc->alloc.free_fn(json_str, doc->alloc.user_data);
    else
        free(json_str);

    return written == len;
}

bool cjsonx_write_file_format(const char* path, cjsonx_doc_t* doc, int indent_spaces) {
    if (!path || !doc) return false;

    char* json_str = cjsonx_stringify_format(doc, indent_spaces);
    if (!json_str) return false;

    FILE* fp = fopen(path, "wb");
    if (!fp) {
        if (doc->alloc.free_fn)
            doc->alloc.free_fn(json_str, doc->alloc.user_data);
        else
            free(json_str);
        return false;
    }

    size_t len = strlen(json_str);
    size_t written = fwrite(json_str, 1, len, fp);
    fclose(fp);
    if (doc->alloc.free_fn)
        doc->alloc.free_fn(json_str, doc->alloc.user_data);
    else
        free(json_str);

    return written == len;
}

cjsonx_val_t cjsonx_clone_val(cjsonx_doc_t* dest_doc, cjsonx_val_t src_val) {
    if (!dest_doc || !src_val.doc) return cjsonx_make_null_handle();
    cjsonx_node_t* src_node = &src_val.doc->nodes[src_val.node_idx];
    cjsonx_type_t t = cjsonx_node_type(src_node);

    switch (t) {
        case CJSONX_NULL:
            return cjsonx_create_null(dest_doc);
        case CJSONX_BOOL:
            return cjsonx_create_bool(dest_doc, src_node->val.b);
        case CJSONX_NUMBER:
            return cjsonx_create_number(dest_doc, src_node->val.f64);
        case CJSONX_STRING: {
            uint32_t len = cjsonx_node_length(src_node);
            // save pointer before allocation, as reallocation might invalidate src_node
            const char* src_str = src_node->val.str;
            uint32_t idx = cjsonx_builder_alloc_node(dest_doc);
            if (idx == UINT32_MAX) return cjsonx_make_null_handle();
            cjsonx_node_set_type_len(&dest_doc->nodes[idx], CJSONX_STRING, len);
            dest_doc->nodes[idx].next_sibling = idx + 1;
            char* s = (char*)cjsonx_arena_alloc(dest_doc, len + 1);
            if (!s) {
                dest_doc->node_count--;  // rollback allocated node slot on failure
                return cjsonx_make_null_handle();
            }
            // don't use len+1: zero-copy strings point into the json buffer where
            // src_str[len] is the closing '"', not '\0'. write nul explicitly.
            memcpy(s, src_str, len);
            s[len] = '\0';
            dest_doc->nodes[idx].val.str = s;
            return (cjsonx_val_t){dest_doc, idx};
        }
        case CJSONX_ARRAY: {
            cjsonx_val_t arr = cjsonx_create_array(dest_doc);
            if (!arr.doc) return cjsonx_make_null_handle();
            cjsonx_iter_t it = cjsonx_iter_init(src_val);
            while (cjsonx_iter_next(&it)) {
                cjsonx_val_t child = cjsonx_clone_val(dest_doc, it.value);
                cjsonx_array_push(arr, child);
            }
            return arr;
        }
        case CJSONX_OBJECT: {
            cjsonx_val_t obj = cjsonx_create_object(dest_doc);
            if (!obj.doc) return cjsonx_make_null_handle();
            cjsonx_iter_t it = cjsonx_iter_init(src_val);
            while (cjsonx_iter_next(&it)) {
                cjsonx_val_t child = cjsonx_clone_val(dest_doc, it.value);
                cjsonx_object_set_len(obj, cjsonx_str(it.key), cjsonx_str_len(it.key), child);
            }
            return obj;
        }
    }
    return cjsonx_make_null_handle();
}

cjsonx_val_t cjsonx_merge_patch(cjsonx_val_t target, cjsonx_val_t patch) {
    if (!target.doc || !patch.doc) return target;

    if (cjsonx_get_type(patch) == CJSONX_OBJECT) {
        if (cjsonx_get_type(target) != CJSONX_OBJECT) {
            target = cjsonx_create_object(target.doc);
            if (!target.doc) return cjsonx_make_null_handle();
        }

        cjsonx_iter_t it = cjsonx_iter_init(patch);
        while (cjsonx_iter_next(&it)) {
            size_t key_len = cjsonx_str_len(it.key);
            const char* key_ptr = cjsonx_str(it.key);
            cjsonx_val_t patch_val = it.value;

            if (cjsonx_is_null(patch_val)) {
                cjsonx_object_remove_len(target, key_ptr, key_len);
            } else {
                cjsonx_val_t target_val = cjsonx_get_len(target, key_ptr, key_len);
                cjsonx_val_t new_val;
                if (target_val.doc) {
                    new_val = cjsonx_merge_patch(target_val, patch_val);
                    if (target_val.node_idx != new_val.node_idx || target_val.doc != new_val.doc) {
                        if (target.doc != new_val.doc) {
                            new_val = cjsonx_clone_val(target.doc, new_val);
                        }
                        cjsonx_object_set_len(target, key_ptr, key_len, new_val);
                    }
                } else {
                    if (cjsonx_get_type(patch_val) == CJSONX_OBJECT) {
                        cjsonx_val_t empty_obj = cjsonx_create_object(target.doc);
                        new_val = cjsonx_merge_patch(empty_obj, patch_val);
                    } else {
                        new_val = patch_val;
                    }
                    // clone value if it belongs to a different document to ensure proper arena
                    // ownership
                    if (target.doc != new_val.doc) {
                        new_val = cjsonx_clone_val(target.doc, new_val);
                    }
                    cjsonx_object_set_len(target, key_ptr, key_len, new_val);
                }
            }
        }
        return target;
    } else {
        if (target.doc != patch.doc) {
            return cjsonx_clone_val(target.doc, patch);
        }
        return patch;
    }
}
#endif  // cjsonx_implementation

// clean up internal-only macros to avoid polluting downstream translation units
#ifdef CJSONX_ABS
#undef CJSONX_ABS
#endif
#ifdef CJSONX_MIN
#undef CJSONX_MIN
#endif

#ifdef __cplusplus
}
#endif

#endif  // cjsonx_builder_h

// implementation guard: include stage1 & stage2 source only once
#ifdef CJSONX_IMPLEMENTATION
#ifndef CJSONX_IMPLEMENTED
#define CJSONX_IMPLEMENTED
// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_STAGE1_H
#define CJSONX_STAGE1_H

// ███████ ████████  █████   ██████  ███████      ██
// ██         ██    ██   ██ ██       ██          ███
// ███████    ██    ███████ ██   ███ █████        ██
//      ██    ██    ██   ██ ██    ██ ██           ██
// ███████    ██    ██   ██  ██████  ███████      ██
//
// >>stage 1 structural indexing


/* detect architecture and select best available backend.
 * note: if you need to support runtime dispatch (e.g. avx2 on modern cpu, fallback to scalar on
 * old), you should build multiple objects and dispatch at runtime via cpuid, rather than
 * compile-time macros.
 */
#if defined(__AVX2__)
// updated 2026-07-09
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

// msvc compatibility for gcc/clang builtins
#if defined(_MSC_VER) && !defined(__clang__)
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
    uint32_t prev_in_string = 0;  // 0 or 0xffffffff
    bool escaped = false;
    bool prev_was_sep = true;
    size_t i = 0;

    while (i < length) {
        // prefetch 512 bytes ahead (~16 cache lines) for better throughput on modern cpus
        __builtin_prefetch(json + i + 512, 0, 0);

        if (i + 31 < length) {
            __m256i v = _mm256_loadu_si256((const __m256i*)(json + i));

            // if we have backslashes or an active escape, fall back to scalar for this block
            uint32_t bs_bits =
                (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, _mm256_set1_epi8('\\')));
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

            uint32_t quote_bits =
                (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, _mm256_set1_epi8('"')));

            /*
             * pure avx2 string mask using carry-less multiplication (prefix xor).
             * we use the pclmulqdq instruction to compute the prefix xor of the quote bits in
             * parallel. clmul of a bitmask with a sequence of 1s (0xff...ff) mathematically
             * computes the cumulative xor sum. this effectively sets all bits inside quotes to 1
             * and all bits outside to 0. string_mask is then combined with the state from the
             * previous block (prev_in_string).
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
                _mm256_or_si256(m_col, m_com));
            uint32_t struct_mask = (uint32_t)_mm256_movemask_epi8(structurals);

            // find whitespace
            __m256i m_sp = _mm256_cmpeq_epi8(v, _mm256_set1_epi8(' '));
            __m256i m_tab = _mm256_cmpeq_epi8(v, _mm256_set1_epi8('\t'));
            __m256i m_nl = _mm256_cmpeq_epi8(v, _mm256_set1_epi8('\n'));
            __m256i m_cr = _mm256_cmpeq_epi8(v, _mm256_set1_epi8('\r'));

            __m256i whitespace =
                _mm256_or_si256(_mm256_or_si256(m_sp, m_tab), _mm256_or_si256(m_nl, m_cr));
            uint32_t ws_mask = (uint32_t)_mm256_movemask_epi8(whitespace);

            // clear structurals and whitespaces that are inside strings
            uint32_t valid_structurals = struct_mask & ~string_mask;
            uint32_t valid_whitespace = ws_mask & ~string_mask;

            uint32_t sep_mask = valid_structurals | valid_whitespace | quote_bits;
            uint32_t non_sep_mask = ~sep_mask;

            /*
             * primitive starts:
             * a primitive (number, true, false, null) starts at any character that is not a
             * separator/whitespace/quote (non_sep_mask) but is preceded by a
             * separator/whitespace/quote (shifted_sep). we shift sep_mask by 1 to align with the
             * next character, inserting prev_was_sep at bit 0. primitives must also not be inside a
             * string (~string_mask).
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

    scalar_fallback: {
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

#endif  // cjsonx_avx2_h
#elif defined(__ARM_NEON)
// updated 2026-07-09
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

// msvc compat: neon doesn't have __builtin_prefetch, mirror the avx2 shim
#if defined(_MSC_VER) && !defined(__clang__)
#define __builtin_prefetch(addr, ...) (void)(addr)
#endif

/*
 * neon-based emulation of the x86 _mm_movemask_epi8 instruction.
 * since arm neon lacks a direct movemask instruction, we achieve this by:
 * 1. masking each byte with a specific power-of-two bit corresponding to its index (bitmask
 * vector). this maps each byte's boolean result (0x00 or 0xff) to a single unique bit (e.g. 0x01
 * for byte 0, 0x02 for byte 1, etc.).
 * 2. performing three rounds of pairwise addition (vpaddq_u8). pairwise addition sums adjacent
 * bytes, accumulating the individual mapped bits from the lower and upper halves.
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
    uint32_t prev_in_string = 0;  // 0 or 0xffffffff
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
                     * which scans the string content explicitly. this matches the avx2 backend
                     * behavior.
                     */
                    i += 32;
                    continue;
                } else {
                    goto scalar_fallback;  // boundary logic complex, fallback to scalar
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
                    goto scalar_fallback;  // entering string or escaping, fallback to scalar
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

                uint8x16_t s1 =
                    vorrq_u8(vorrq_u8(vorrq_u8(m_lcb1, m_rcb1), vorrq_u8(m_lsb1, m_rsb1)),
                             vorrq_u8(m_col1, m_com1));
                uint8x16_t s2 =
                    vorrq_u8(vorrq_u8(vorrq_u8(m_lcb2, m_rcb2), vorrq_u8(m_lsb2, m_rsb2)),
                             vorrq_u8(m_col2, m_com2));
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

    scalar_fallback: {
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

#endif  // cjsonx_neon_h
#elif defined(__wasm_simd128__)
// updated 2026-07-09
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
    uint32_t prev_in_string = 0;  // 0 or 0xffffffff
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

                v128_t s1 = wasm_v128_or(
                    wasm_v128_or(wasm_v128_or(m_lcb1, m_rcb1), wasm_v128_or(m_lsb1, m_rsb1)),
                    wasm_v128_or(m_col1, m_com1));
                v128_t s2 = wasm_v128_or(
                    wasm_v128_or(wasm_v128_or(m_lcb2, m_rcb2), wasm_v128_or(m_lsb2, m_rsb2)),
                    wasm_v128_or(m_col2, m_com2));
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

    scalar_fallback: {
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

#endif  // cjsonx_wasm_h
#else
// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_SCALAR_H
#define CJSONX_SCALAR_H

// ███████  ██████  █████  ██       █████  ██████
// ██      ██      ██   ██ ██      ██   ██ ██   ██
// ███████ ██      ███████ ██      ███████ ██████
//      ██ ██      ██   ██ ██      ██   ██ ██   ██
// ███████  ██████ ██   ██ ███████ ██   ██ ██   ██
//
// >>scalar backend


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
    bool prev_was_sep =
        true;  // true after whitespace/structural — used to detect start of primitive tokens

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

#endif  // cjsonx_scalar_h
#endif

#ifdef CJSONX_IMPLEMENTATION
// dispatches to the correct stage 1 implementation based on the detected architecture.
bool cjsonx_stage1_build_tape(const char* json, size_t length, cjsonx_tape_t* tape) {
#if defined(__AVX2__)
    return cjsonx_stage1_avx2(json, length, tape);
#elif defined(__ARM_NEON)
    return cjsonx_stage1_neon(json, length, tape);
#elif defined(__wasm_simd128__)
    return cjsonx_stage1_wasm(json, length, tape);
#else
    return cjsonx_stage1_scalar(json, length, tape);
#endif
}
#endif

#endif  // cjsonx_stage1_h
// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_STAGE2_H
#define CJSONX_STAGE2_H

// ███████ ████████  █████   ██████  ███████     ██████
// ██         ██    ██   ██ ██       ██               ██
// ███████    ██    ███████ ██   ███ █████        █████
//      ██    ██    ██   ██ ██    ██ ██          ██
// ███████    ██    ██   ██  ██████  ███████     ███████
//
// >>stage 2 dom building

#include <errno.h>
#include <locale.h>

// updated 2026-07-09
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

#include <math.h>

// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_EISEL_LEMIRE_H
#define CJSONX_EISEL_LEMIRE_H

// ███████ ██ ███████ ███████ ██          ██      ███████ ███    ███ ██ ██████  ███████
// ██      ██ ██      ██      ██          ██      ██      ████  ████ ██ ██   ██ ██
// █████   ██ ███████ █████   ██          ██      █████   ██ ████ ██ ██ ██████  █████
// ██      ██      ██ ██      ██          ██      ██      ██  ██  ██ ██ ██   ██ ██
// ███████ ██ ███████ ███████ ███████     ███████ ███████ ██      ██ ██ ██   ██ ███████
//
// >>eisel-lemire float parsing


static const uint64_t cjsonx_eisel_lemire_mantissa[] = {
    0xfa8fd5a0081c0288ULL,  // 10^-348
    0x9c99e58405118195ULL,  // 10^-347
    0xc3c05ee50655e1faULL,  // 10^-346
    0xf4b0769e47eb5a79ULL,  // 10^-345
    0x98ee4a22ecf3188cULL,  // 10^-344
    0xbf29dcaba82fdeaeULL,  // 10^-343
    0xeef453d6923bd65aULL,  // 10^-342
    0x9558b4661b6565f8ULL,  // 10^-341
    0xbaaee17fa23ebf76ULL,  // 10^-340
    0xe95a99df8ace6f54ULL,  // 10^-339
    0x91d8a02bb6c10594ULL,  // 10^-338
    0xb64ec836a47146faULL,  // 10^-337
    0xe3e27a444d8d98b8ULL,  // 10^-336
    0x8e6d8c6ab0787f73ULL,  // 10^-335
    0xb208ef855c969f50ULL,  // 10^-334
    0xde8b2b66b3bc4724ULL,  // 10^-333
    0x8b16fb203055ac76ULL,  // 10^-332
    0xaddcb9e83c6b1794ULL,  // 10^-331
    0xd953e8624b85dd79ULL,  // 10^-330
    0x87d4713d6f33aa6cULL,  // 10^-329
    0xa9c98d8ccb009506ULL,  // 10^-328
    0xd43bf0effdc0ba48ULL,  // 10^-327
    0x84a57695fe98746dULL,  // 10^-326
    0xa5ced43b7e3e9188ULL,  // 10^-325
    0xcf42894a5dce35eaULL,  // 10^-324
    0x818995ce7aa0e1b2ULL,  // 10^-323
    0xa1ebfb4219491a1fULL,  // 10^-322
    0xca66fa129f9b60a7ULL,  // 10^-321
    0xfd00b897478238d1ULL,  // 10^-320
    0x9e20735e8cb16382ULL,  // 10^-319
    0xc5a890362fddbc63ULL,  // 10^-318
    0xf712b443bbd52b7cULL,  // 10^-317
    0x9a6bb0aa55653b2dULL,  // 10^-316
    0xc1069cd4eabe89f9ULL,  // 10^-315
    0xf148440a256e2c77ULL,  // 10^-314
    0x96cd2a865764dbcaULL,  // 10^-313
    0xbc807527ed3e12bdULL,  // 10^-312
    0xeba09271e88d976cULL,  // 10^-311
    0x93445b8731587ea3ULL,  // 10^-310
    0xb8157268fdae9e4cULL,  // 10^-309
    0xe61acf033d1a45dfULL,  // 10^-308
    0x8fd0c16206306bacULL,  // 10^-307
    0xb3c4f1ba87bc8697ULL,  // 10^-306
    0xe0b62e2929aba83cULL,  // 10^-305
    0x8c71dcd9ba0b4926ULL,  // 10^-304
    0xaf8e5410288e1b6fULL,  // 10^-303
    0xdb71e91432b1a24bULL,  // 10^-302
    0x892731ac9faf056fULL,  // 10^-301
    0xab70fe17c79ac6caULL,  // 10^-300
    0xd64d3d9db981787dULL,  // 10^-299
    0x85f0468293f0eb4eULL,  // 10^-298
    0xa76c582338ed2622ULL,  // 10^-297
    0xd1476e2c07286faaULL,  // 10^-296
    0x82cca4db847945caULL,  // 10^-295
    0xa37fce126597973dULL,  // 10^-294
    0xcc5fc196fefd7d0cULL,  // 10^-293
    0xff77b1fcbebcdc4fULL,  // 10^-292
    0x9faacf3df73609b1ULL,  // 10^-291
    0xc795830d75038c1eULL,  // 10^-290
    0xf97ae3d0d2446f25ULL,  // 10^-289
    0x9becce62836ac577ULL,  // 10^-288
    0xc2e801fb244576d5ULL,  // 10^-287
    0xf3a20279ed56d48aULL,  // 10^-286
    0x9845418c345644d7ULL,  // 10^-285
    0xbe5691ef416bd60cULL,  // 10^-284
    0xedec366b11c6cb8fULL,  // 10^-283
    0x94b3a202eb1c3f39ULL,  // 10^-282
    0xb9e08a83a5e34f08ULL,  // 10^-281
    0xe858ad248f5c22caULL,  // 10^-280
    0x91376c36d99995beULL,  // 10^-279
    0xb58547448ffffb2eULL,  // 10^-278
    0xe2e69915b3fff9f9ULL,  // 10^-277
    0x8dd01fad907ffc3cULL,  // 10^-276
    0xb1442798f49ffb4bULL,  // 10^-275
    0xdd95317f31c7fa1dULL,  // 10^-274
    0x8a7d3eef7f1cfc52ULL,  // 10^-273
    0xad1c8eab5ee43b67ULL,  // 10^-272
    0xd863b256369d4a41ULL,  // 10^-271
    0x873e4f75e2224e68ULL,  // 10^-270
    0xa90de3535aaae202ULL,  // 10^-269
    0xd3515c2831559a83ULL,  // 10^-268
    0x8412d9991ed58092ULL,  // 10^-267
    0xa5178fff668ae0b6ULL,  // 10^-266
    0xce5d73ff402d98e4ULL,  // 10^-265
    0x80fa687f881c7f8eULL,  // 10^-264
    0xa139029f6a239f72ULL,  // 10^-263
    0xc987434744ac874fULL,  // 10^-262
    0xfbe9141915d7a922ULL,  // 10^-261
    0x9d71ac8fada6c9b5ULL,  // 10^-260
    0xc4ce17b399107c23ULL,  // 10^-259
    0xf6019da07f549b2bULL,  // 10^-258
    0x99c102844f94e0fbULL,  // 10^-257
    0xc0314325637a193aULL,  // 10^-256
    0xf03d93eebc589f88ULL,  // 10^-255
    0x96267c7535b763b5ULL,  // 10^-254
    0xbbb01b9283253ca3ULL,  // 10^-253
    0xea9c227723ee8bcbULL,  // 10^-252
    0x92a1958a7675175fULL,  // 10^-251
    0xb749faed14125d37ULL,  // 10^-250
    0xe51c79a85916f485ULL,  // 10^-249
    0x8f31cc0937ae58d3ULL,  // 10^-248
    0xb2fe3f0b8599ef08ULL,  // 10^-247
    0xdfbdcece67006ac9ULL,  // 10^-246
    0x8bd6a141006042beULL,  // 10^-245
    0xaecc49914078536dULL,  // 10^-244
    0xda7f5bf590966849ULL,  // 10^-243
    0x888f99797a5e012dULL,  // 10^-242
    0xaab37fd7d8f58179ULL,  // 10^-241
    0xd5605fcdcf32e1d7ULL,  // 10^-240
    0x855c3be0a17fcd26ULL,  // 10^-239
    0xa6b34ad8c9dfc070ULL,  // 10^-238
    0xd0601d8efc57b08cULL,  // 10^-237
    0x823c12795db6ce57ULL,  // 10^-236
    0xa2cb1717b52481edULL,  // 10^-235
    0xcb7ddcdda26da269ULL,  // 10^-234
    0xfe5d54150b090b03ULL,  // 10^-233
    0x9efa548d26e5a6e2ULL,  // 10^-232
    0xc6b8e9b0709f109aULL,  // 10^-231
    0xf867241c8cc6d4c1ULL,  // 10^-230
    0x9b407691d7fc44f8ULL,  // 10^-229
    0xc21094364dfb5637ULL,  // 10^-228
    0xf294b943e17a2bc4ULL,  // 10^-227
    0x979cf3ca6cec5b5bULL,  // 10^-226
    0xbd8430bd08277231ULL,  // 10^-225
    0xece53cec4a314ebeULL,  // 10^-224
    0x940f4613ae5ed137ULL,  // 10^-223
    0xb913179899f68584ULL,  // 10^-222
    0xe757dd7ec07426e5ULL,  // 10^-221
    0x9096ea6f3848984fULL,  // 10^-220
    0xb4bca50b065abe63ULL,  // 10^-219
    0xe1ebce4dc7f16dfcULL,  // 10^-218
    0x8d3360f09cf6e4bdULL,  // 10^-217
    0xb080392cc4349dedULL,  // 10^-216
    0xdca04777f541c568ULL,  // 10^-215
    0x89e42caaf9491b61ULL,  // 10^-214
    0xac5d37d5b79b6239ULL,  // 10^-213
    0xd77485cb25823ac7ULL,  // 10^-212
    0x86a8d39ef77164bdULL,  // 10^-211
    0xa8530886b54dbdecULL,  // 10^-210
    0xd267caa862a12d67ULL,  // 10^-209
    0x8380dea93da4bc60ULL,  // 10^-208
    0xa46116538d0deb78ULL,  // 10^-207
    0xcd795be870516656ULL,  // 10^-206
    0x806bd9714632dff6ULL,  // 10^-205
    0xa086cfcd97bf97f4ULL,  // 10^-204
    0xc8a883c0fdaf7df0ULL,  // 10^-203
    0xfad2a4b13d1b5d6cULL,  // 10^-202
    0x9cc3a6eec6311a64ULL,  // 10^-201
    0xc3f490aa77bd60fdULL,  // 10^-200
    0xf4f1b4d515acb93cULL,  // 10^-199
    0x991711052d8bf3c5ULL,  // 10^-198
    0xbf5cd54678eef0b7ULL,  // 10^-197
    0xef340a98172aace5ULL,  // 10^-196
    0x9580869f0e7aac0fULL,  // 10^-195
    0xbae0a846d2195713ULL,  // 10^-194
    0xe998d258869facd7ULL,  // 10^-193
    0x91ff83775423cc06ULL,  // 10^-192
    0xb67f6455292cbf08ULL,  // 10^-191
    0xe41f3d6a7377eecaULL,  // 10^-190
    0x8e938662882af53eULL,  // 10^-189
    0xb23867fb2a35b28eULL,  // 10^-188
    0xdec681f9f4c31f31ULL,  // 10^-187
    0x8b3c113c38f9f37fULL,  // 10^-186
    0xae0b158b4738705fULL,  // 10^-185
    0xd98ddaee19068c76ULL,  // 10^-184
    0x87f8a8d4cfa417caULL,  // 10^-183
    0xa9f6d30a038d1dbcULL,  // 10^-182
    0xd47487cc8470652bULL,  // 10^-181
    0x84c8d4dfd2c63f3bULL,  // 10^-180
    0xa5fb0a17c777cf0aULL,  // 10^-179
    0xcf79cc9db955c2ccULL,  // 10^-178
    0x81ac1fe293d599c0ULL,  // 10^-177
    0xa21727db38cb0030ULL,  // 10^-176
    0xca9cf1d206fdc03cULL,  // 10^-175
    0xfd442e4688bd304bULL,  // 10^-174
    0x9e4a9cec15763e2fULL,  // 10^-173
    0xc5dd44271ad3cdbaULL,  // 10^-172
    0xf7549530e188c129ULL,  // 10^-171
    0x9a94dd3e8cf578baULL,  // 10^-170
    0xc13a148e3032d6e8ULL,  // 10^-169
    0xf18899b1bc3f8ca2ULL,  // 10^-168
    0x96f5600f15a7b7e5ULL,  // 10^-167
    0xbcb2b812db11a5deULL,  // 10^-166
    0xebdf661791d60f56ULL,  // 10^-165
    0x936b9fcebb25c996ULL,  // 10^-164
    0xb84687c269ef3bfbULL,  // 10^-163
    0xe65829b3046b0afaULL,  // 10^-162
    0x8ff71a0fe2c2e6dcULL,  // 10^-161
    0xb3f4e093db73a093ULL,  // 10^-160
    0xe0f218b8d25088b8ULL,  // 10^-159
    0x8c974f7383725573ULL,  // 10^-158
    0xafbd2350644eead0ULL,  // 10^-157
    0xdbac6c247d62a584ULL,  // 10^-156
    0x894bc396ce5da772ULL,  // 10^-155
    0xab9eb47c81f5114fULL,  // 10^-154
    0xd686619ba27255a3ULL,  // 10^-153
    0x8613fd0145877586ULL,  // 10^-152
    0xa798fc4196e952e7ULL,  // 10^-151
    0xd17f3b51fca3a7a1ULL,  // 10^-150
    0x82ef85133de648c5ULL,  // 10^-149
    0xa3ab66580d5fdaf6ULL,  // 10^-148
    0xcc963fee10b7d1b3ULL,  // 10^-147
    0xffbbcfe994e5c620ULL,  // 10^-146
    0x9fd561f1fd0f9bd4ULL,  // 10^-145
    0xc7caba6e7c5382c9ULL,  // 10^-144
    0xf9bd690a1b68637bULL,  // 10^-143
    0x9c1661a651213e2dULL,  // 10^-142
    0xc31bfa0fe5698db8ULL,  // 10^-141
    0xf3e2f893dec3f126ULL,  // 10^-140
    0x986ddb5c6b3a76b8ULL,  // 10^-139
    0xbe89523386091466ULL,  // 10^-138
    0xee2ba6c0678b597fULL,  // 10^-137
    0x94db483840b717f0ULL,  // 10^-136
    0xba121a4650e4ddecULL,  // 10^-135
    0xe896a0d7e51e1566ULL,  // 10^-134
    0x915e2486ef32cd60ULL,  // 10^-133
    0xb5b5ada8aaff80b8ULL,  // 10^-132
    0xe3231912d5bf60e6ULL,  // 10^-131
    0x8df5efabc5979c90ULL,  // 10^-130
    0xb1736b96b6fd83b4ULL,  // 10^-129
    0xddd0467c64bce4a1ULL,  // 10^-128
    0x8aa22c0dbef60ee4ULL,  // 10^-127
    0xad4ab7112eb3929eULL,  // 10^-126
    0xd89d64d57a607745ULL,  // 10^-125
    0x87625f056c7c4a8bULL,  // 10^-124
    0xa93af6c6c79b5d2eULL,  // 10^-123
    0xd389b47879823479ULL,  // 10^-122
    0x843610cb4bf160ccULL,  // 10^-121
    0xa54394fe1eedb8ffULL,  // 10^-120
    0xce947a3da6a9273eULL,  // 10^-119
    0x811ccc668829b887ULL,  // 10^-118
    0xa163ff802a3426a9ULL,  // 10^-117
    0xc9bcff6034c13053ULL,  // 10^-116
    0xfc2c3f3841f17c68ULL,  // 10^-115
    0x9d9ba7832936edc1ULL,  // 10^-114
    0xc5029163f384a931ULL,  // 10^-113
    0xf64335bcf065d37dULL,  // 10^-112
    0x99ea0196163fa42eULL,  // 10^-111
    0xc06481fb9bcf8d3aULL,  // 10^-110
    0xf07da27a82c37088ULL,  // 10^-109
    0x964e858c91ba2655ULL,  // 10^-108
    0xbbe226efb628afebULL,  // 10^-107
    0xeadab0aba3b2dbe5ULL,  // 10^-106
    0x92c8ae6b464fc96fULL,  // 10^-105
    0xb77ada0617e3bbcbULL,  // 10^-104
    0xe55990879ddcaabeULL,  // 10^-103
    0x8f57fa54c2a9eab7ULL,  // 10^-102
    0xb32df8e9f3546564ULL,  // 10^-101
    0xdff9772470297ebdULL,  // 10^-100
    0x8bfbea76c619ef36ULL,  // 10^-99
    0xaefae51477a06b04ULL,  // 10^-98
    0xdab99e59958885c5ULL,  // 10^-97
    0x88b402f7fd75539bULL,  // 10^-96
    0xaae103b5fcd2a882ULL,  // 10^-95
    0xd59944a37c0752a2ULL,  // 10^-94
    0x857fcae62d8493a5ULL,  // 10^-93
    0xa6dfbd9fb8e5b88fULL,  // 10^-92
    0xd097ad07a71f26b2ULL,  // 10^-91
    0x825ecc24c8737830ULL,  // 10^-90
    0xa2f67f2dfa90563bULL,  // 10^-89
    0xcbb41ef979346bcaULL,  // 10^-88
    0xfea126b7d78186bdULL,  // 10^-87
    0x9f24b832e6b0f436ULL,  // 10^-86
    0xc6ede63fa05d3144ULL,  // 10^-85
    0xf8a95fcf88747d94ULL,  // 10^-84
    0x9b69dbe1b548ce7dULL,  // 10^-83
    0xc24452da229b021cULL,  // 10^-82
    0xf2d56790ab41c2a3ULL,  // 10^-81
    0x97c560ba6b0919a6ULL,  // 10^-80
    0xbdb6b8e905cb600fULL,  // 10^-79
    0xed246723473e3813ULL,  // 10^-78
    0x9436c0760c86e30cULL,  // 10^-77
    0xb94470938fa89bcfULL,  // 10^-76
    0xe7958cb87392c2c3ULL,  // 10^-75
    0x90bd77f3483bb9baULL,  // 10^-74
    0xb4ecd5f01a4aa828ULL,  // 10^-73
    0xe2280b6c20dd5232ULL,  // 10^-72
    0x8d590723948a535fULL,  // 10^-71
    0xb0af48ec79ace837ULL,  // 10^-70
    0xdcdb1b2798182245ULL,  // 10^-69
    0x8a08f0f8bf0f156bULL,  // 10^-68
    0xac8b2d36eed2dac6ULL,  // 10^-67
    0xd7adf884aa879177ULL,  // 10^-66
    0x86ccbb52ea94baebULL,  // 10^-65
    0xa87fea27a539e9a5ULL,  // 10^-64
    0xd29fe4b18e88640fULL,  // 10^-63
    0x83a3eeeef9153e89ULL,  // 10^-62
    0xa48ceaaab75a8e2bULL,  // 10^-61
    0xcdb02555653131b6ULL,  // 10^-60
    0x808e17555f3ebf12ULL,  // 10^-59
    0xa0b19d2ab70e6ed6ULL,  // 10^-58
    0xc8de047564d20a8cULL,  // 10^-57
    0xfb158592be068d2fULL,  // 10^-56
    0x9ced737bb6c4183dULL,  // 10^-55
    0xc428d05aa4751e4dULL,  // 10^-54
    0xf53304714d9265e0ULL,  // 10^-53
    0x993fe2c6d07b7facULL,  // 10^-52
    0xbf8fdb78849a5f97ULL,  // 10^-51
    0xef73d256a5c0f77dULL,  // 10^-50
    0x95a8637627989aaeULL,  // 10^-49
    0xbb127c53b17ec159ULL,  // 10^-48
    0xe9d71b689dde71b0ULL,  // 10^-47
    0x9226712162ab070eULL,  // 10^-46
    0xb6b00d69bb55c8d1ULL,  // 10^-45
    0xe45c10c42a2b3b06ULL,  // 10^-44
    0x8eb98a7a9a5b04e3ULL,  // 10^-43
    0xb267ed1940f1c61cULL,  // 10^-42
    0xdf01e85f912e37a3ULL,  // 10^-41
    0x8b61313bbabce2c6ULL,  // 10^-40
    0xae397d8aa96c1b78ULL,  // 10^-39
    0xd9c7dced53c72256ULL,  // 10^-38
    0x881cea14545c7575ULL,  // 10^-37
    0xaa242499697392d3ULL,  // 10^-36
    0xd4ad2dbfc3d07788ULL,  // 10^-35
    0x84ec3c97da624ab5ULL,  // 10^-34
    0xa6274bbdd0fadd62ULL,  // 10^-33
    0xcfb11ead453994baULL,  // 10^-32
    0x81ceb32c4b43fcf5ULL,  // 10^-31
    0xa2425ff75e14fc32ULL,  // 10^-30
    0xcad2f7f5359a3b3eULL,  // 10^-29
    0xfd87b5f28300ca0eULL,  // 10^-28
    0x9e74d1b791e07e48ULL,  // 10^-27
    0xc612062576589ddbULL,  // 10^-26
    0xf79687aed3eec551ULL,  // 10^-25
    0x9abe14cd44753b53ULL,  // 10^-24
    0xc16d9a0095928a27ULL,  // 10^-23
    0xf1c90080baf72cb1ULL,  // 10^-22
    0x971da05074da7befULL,  // 10^-21
    0xbce5086492111aebULL,  // 10^-20
    0xec1e4a7db69561a5ULL,  // 10^-19
    0x9392ee8e921d5d07ULL,  // 10^-18
    0xb877aa3236a4b449ULL,  // 10^-17
    0xe69594bec44de15bULL,  // 10^-16
    0x901d7cf73ab0acd9ULL,  // 10^-15
    0xb424dc35095cd80fULL,  // 10^-14
    0xe12e13424bb40e13ULL,  // 10^-13
    0x8cbccc096f5088ccULL,  // 10^-12
    0xafebff0bcb24aaffULL,  // 10^-11
    0xdbe6fecebdedd5bfULL,  // 10^-10
    0x89705f4136b4a597ULL,  // 10^-9
    0xabcc77118461cefdULL,  // 10^-8
    0xd6bf94d5e57a42bcULL,  // 10^-7
    0x8637bd05af6c69b6ULL,  // 10^-6
    0xa7c5ac471b478423ULL,  // 10^-5
    0xd1b71758e219652cULL,  // 10^-4
    0x83126e978d4fdf3bULL,  // 10^-3
    0xa3d70a3d70a3d70aULL,  // 10^-2
    0xcccccccccccccccdULL,  // 10^-1
    0x8000000000000000ULL,  // 10^0
    0xa000000000000000ULL,  // 10^1
    0xc800000000000000ULL,  // 10^2
    0xfa00000000000000ULL,  // 10^3
    0x9c40000000000000ULL,  // 10^4
    0xc350000000000000ULL,  // 10^5
    0xf424000000000000ULL,  // 10^6
    0x9896800000000000ULL,  // 10^7
    0xbebc200000000000ULL,  // 10^8
    0xee6b280000000000ULL,  // 10^9
    0x9502f90000000000ULL,  // 10^10
    0xba43b74000000000ULL,  // 10^11
    0xe8d4a51000000000ULL,  // 10^12
    0x9184e72a00000000ULL,  // 10^13
    0xb5e620f480000000ULL,  // 10^14
    0xe35fa931a0000000ULL,  // 10^15
    0x8e1bc9bf04000000ULL,  // 10^16
    0xb1a2bc2ec5000000ULL,  // 10^17
    0xde0b6b3a76400000ULL,  // 10^18
    0x8ac7230489e80000ULL,  // 10^19
    0xad78ebc5ac620000ULL,  // 10^20
    0xd8d726b7177a8000ULL,  // 10^21
    0x878678326eac9000ULL,  // 10^22
    0xa968163f0a57b400ULL,  // 10^23
    0xd3c21bcecceda100ULL,  // 10^24
    0x84595161401484a0ULL,  // 10^25
    0xa56fa5b99019a5c8ULL,  // 10^26
    0xcecb8f27f4200f3aULL,  // 10^27
    0x813f3978f8940984ULL,  // 10^28
    0xa18f07d736b90be5ULL,  // 10^29
    0xc9f2c9cd04674edfULL,  // 10^30
    0xfc6f7c4045812296ULL,  // 10^31
    0x9dc5ada82b70b59eULL,  // 10^32
    0xc5371912364ce305ULL,  // 10^33
    0xf684df56c3e01bc7ULL,  // 10^34
    0x9a130b963a6c115cULL,  // 10^35
    0xc097ce7bc90715b3ULL,  // 10^36
    0xf0bdc21abb48db20ULL,  // 10^37
    0x96769950b50d88f4ULL,  // 10^38
    0xbc143fa4e250eb31ULL,  // 10^39
    0xeb194f8e1ae525fdULL,  // 10^40
    0x92efd1b8d0cf37beULL,  // 10^41
    0xb7abc627050305aeULL,  // 10^42
    0xe596b7b0c643c719ULL,  // 10^43
    0x8f7e32ce7bea5c70ULL,  // 10^44
    0xb35dbf821ae4f38cULL,  // 10^45
    0xe0352f62a19e306fULL,  // 10^46
    0x8c213d9da502de45ULL,  // 10^47
    0xaf298d050e4395d7ULL,  // 10^48
    0xdaf3f04651d47b4cULL,  // 10^49
    0x88d8762bf324cd10ULL,  // 10^50
    0xab0e93b6efee0054ULL,  // 10^51
    0xd5d238a4abe98068ULL,  // 10^52
    0x85a36366eb71f041ULL,  // 10^53
    0xa70c3c40a64e6c52ULL,  // 10^54
    0xd0cf4b50cfe20766ULL,  // 10^55
    0x82818f1281ed44a0ULL,  // 10^56
    0xa321f2d7226895c8ULL,  // 10^57
    0xcbea6f8ceb02bb3aULL,  // 10^58
    0xfee50b7025c36a08ULL,  // 10^59
    0x9f4f2726179a2245ULL,  // 10^60
    0xc722f0ef9d80aad6ULL,  // 10^61
    0xf8ebad2b84e0d58cULL,  // 10^62
    0x9b934c3b330c8577ULL,  // 10^63
    0xc2781f49ffcfa6d5ULL,  // 10^64
    0xf316271c7fc3908bULL,  // 10^65
    0x97edd871cfda3a57ULL,  // 10^66
    0xbde94e8e43d0c8ecULL,  // 10^67
    0xed63a231d4c4fb27ULL,  // 10^68
    0x945e455f24fb1cf9ULL,  // 10^69
    0xb975d6b6ee39e437ULL,  // 10^70
    0xe7d34c64a9c85d44ULL,  // 10^71
    0x90e40fbeea1d3a4bULL,  // 10^72
    0xb51d13aea4a488ddULL,  // 10^73
    0xe264589a4dcdab15ULL,  // 10^74
    0x8d7eb76070a08aedULL,  // 10^75
    0xb0de65388cc8ada8ULL,  // 10^76
    0xdd15fe86affad912ULL,  // 10^77
    0x8a2dbf142dfcc7abULL,  // 10^78
    0xacb92ed9397bf996ULL,  // 10^79
    0xd7e77a8f87daf7fcULL,  // 10^80
    0x86f0ac99b4e8dafdULL,  // 10^81
    0xa8acd7c0222311bdULL,  // 10^82
    0xd2d80db02aabd62cULL,  // 10^83
    0x83c7088e1aab65dbULL,  // 10^84
    0xa4b8cab1a1563f52ULL,  // 10^85
    0xcde6fd5e09abcf27ULL,  // 10^86
    0x80b05e5ac60b6178ULL,  // 10^87
    0xa0dc75f1778e39d6ULL,  // 10^88
    0xc913936dd571c84cULL,  // 10^89
    0xfb5878494ace3a5fULL,  // 10^90
    0x9d174b2dcec0e47bULL,  // 10^91
    0xc45d1df942711d9aULL,  // 10^92
    0xf5746577930d6501ULL,  // 10^93
    0x9968bf6abbe85f20ULL,  // 10^94
    0xbfc2ef456ae276e9ULL,  // 10^95
    0xefb3ab16c59b14a3ULL,  // 10^96
    0x95d04aee3b80ece6ULL,  // 10^97
    0xbb445da9ca61281fULL,  // 10^98
    0xea1575143cf97227ULL,  // 10^99
    0x924d692ca61be758ULL,  // 10^100
    0xb6e0c377cfa2e12eULL,  // 10^101
    0xe498f455c38b997aULL,  // 10^102
    0x8edf98b59a373fecULL,  // 10^103
    0xb2977ee300c50fe7ULL,  // 10^104
    0xdf3d5e9bc0f653e1ULL,  // 10^105
    0x8b865b215899f46dULL,  // 10^106
    0xae67f1e9aec07188ULL,  // 10^107
    0xda01ee641a708deaULL,  // 10^108
    0x884134fe908658b2ULL,  // 10^109
    0xaa51823e34a7eedfULL,  // 10^110
    0xd4e5e2cdc1d1ea96ULL,  // 10^111
    0x850fadc09923329eULL,  // 10^112
    0xa6539930bf6bff46ULL,  // 10^113
    0xcfe87f7cef46ff17ULL,  // 10^114
    0x81f14fae158c5f6eULL,  // 10^115
    0xa26da3999aef774aULL,  // 10^116
    0xcb090c8001ab551cULL,  // 10^117
    0xfdcb4fa002162a63ULL,  // 10^118
    0x9e9f11c4014dda7eULL,  // 10^119
    0xc646d63501a1511eULL,  // 10^120
    0xf7d88bc24209a565ULL,  // 10^121
    0x9ae757596946075fULL,  // 10^122
    0xc1a12d2fc3978937ULL,  // 10^123
    0xf209787bb47d6b85ULL,  // 10^124
    0x9745eb4d50ce6333ULL,  // 10^125
    0xbd176620a501fc00ULL,  // 10^126
    0xec5d3fa8ce427b00ULL,  // 10^127
    0x93ba47c980e98ce0ULL,  // 10^128
    0xb8a8d9bbe123f018ULL,  // 10^129
    0xe6d3102ad96cec1eULL,  // 10^130
    0x9043ea1ac7e41393ULL,  // 10^131
    0xb454e4a179dd1877ULL,  // 10^132
    0xe16a1dc9d8545e95ULL,  // 10^133
    0x8ce2529e2734bb1dULL,  // 10^134
    0xb01ae745b101e9e4ULL,  // 10^135
    0xdc21a1171d42645dULL,  // 10^136
    0x899504ae72497ebaULL,  // 10^137
    0xabfa45da0edbde69ULL,  // 10^138
    0xd6f8d7509292d603ULL,  // 10^139
    0x865b86925b9bc5c2ULL,  // 10^140
    0xa7f26836f282b733ULL,  // 10^141
    0xd1ef0244af2364ffULL,  // 10^142
    0x8335616aed761f1fULL,  // 10^143
    0xa402b9c5a8d3a6e7ULL,  // 10^144
    0xcd036837130890a1ULL,  // 10^145
    0x802221226be55a65ULL,  // 10^146
    0xa02aa96b06deb0feULL,  // 10^147
    0xc83553c5c8965d3dULL,  // 10^148
    0xfa42a8b73abbf48dULL,  // 10^149
    0x9c69a97284b578d8ULL,  // 10^150
    0xc38413cf25e2d70eULL,  // 10^151
    0xf46518c2ef5b8cd1ULL,  // 10^152
    0x98bf2f79d5993803ULL,  // 10^153
    0xbeeefb584aff8604ULL,  // 10^154
    0xeeaaba2e5dbf6785ULL,  // 10^155
    0x952ab45cfa97a0b3ULL,  // 10^156
    0xba756174393d88e0ULL,  // 10^157
    0xe912b9d1478ceb17ULL,  // 10^158
    0x91abb422ccb812efULL,  // 10^159
    0xb616a12b7fe617aaULL,  // 10^160
    0xe39c49765fdf9d95ULL,  // 10^161
    0x8e41ade9fbebc27dULL,  // 10^162
    0xb1d219647ae6b31cULL,  // 10^163
    0xde469fbd99a05fe3ULL,  // 10^164
    0x8aec23d680043beeULL,  // 10^165
    0xada72ccc20054aeaULL,  // 10^166
    0xd910f7ff28069da4ULL,  // 10^167
    0x87aa9aff79042287ULL,  // 10^168
    0xa99541bf57452b28ULL,  // 10^169
    0xd3fa922f2d1675f2ULL,  // 10^170
    0x847c9b5d7c2e09b7ULL,  // 10^171
    0xa59bc234db398c25ULL,  // 10^172
    0xcf02b2c21207ef2fULL,  // 10^173
    0x8161afb94b44f57dULL,  // 10^174
    0xa1ba1ba79e1632dcULL,  // 10^175
    0xca28a291859bbf93ULL,  // 10^176
    0xfcb2cb35e702af78ULL,  // 10^177
    0x9defbf01b061adabULL,  // 10^178
    0xc56baec21c7a1916ULL,  // 10^179
    0xf6c69a72a3989f5cULL,  // 10^180
    0x9a3c2087a63f6399ULL,  // 10^181
    0xc0cb28a98fcf3c80ULL,  // 10^182
    0xf0fdf2d3f3c30b9fULL,  // 10^183
    0x969eb7c47859e744ULL,  // 10^184
    0xbc4665b596706115ULL,  // 10^185
    0xeb57ff22fc0c795aULL,  // 10^186
    0x9316ff75dd87cbd8ULL,  // 10^187
    0xb7dcbf5354e9beceULL,  // 10^188
    0xe5d3ef282a242e82ULL,  // 10^189
    0x8fa475791a569d11ULL,  // 10^190
    0xb38d92d760ec4455ULL,  // 10^191
    0xe070f78d3927556bULL,  // 10^192
    0x8c469ab843b89563ULL,  // 10^193
    0xaf58416654a6babbULL,  // 10^194
    0xdb2e51bfe9d0696aULL,  // 10^195
    0x88fcf317f22241e2ULL,  // 10^196
    0xab3c2fddeeaad25bULL,  // 10^197
    0xd60b3bd56a5586f2ULL,  // 10^198
    0x85c7056562757457ULL,  // 10^199
    0xa738c6bebb12d16dULL,  // 10^200
    0xd106f86e69d785c8ULL,  // 10^201
    0x82a45b450226b39dULL,  // 10^202
    0xa34d721642b06084ULL,  // 10^203
    0xcc20ce9bd35c78a5ULL,  // 10^204
    0xff290242c83396ceULL,  // 10^205
    0x9f79a169bd203e41ULL,  // 10^206
    0xc75809c42c684dd1ULL,  // 10^207
    0xf92e0c3537826146ULL,  // 10^208
    0x9bbcc7a142b17cccULL,  // 10^209
    0xc2abf989935ddbfeULL,  // 10^210
    0xf356f7ebf83552feULL,  // 10^211
    0x98165af37b2153dfULL,  // 10^212
    0xbe1bf1b059e9a8d6ULL,  // 10^213
    0xeda2ee1c7064130cULL,  // 10^214
    0x9485d4d1c63e8be8ULL,  // 10^215
    0xb9a74a0637ce2ee1ULL,  // 10^216
    0xe8111c87c5c1ba9aULL,  // 10^217
    0x910ab1d4db9914a0ULL,  // 10^218
    0xb54d5e4a127f59c8ULL,  // 10^219
    0xe2a0b5dc971f303aULL,  // 10^220
    0x8da471a9de737e24ULL,  // 10^221
    0xb10d8e1456105dadULL,  // 10^222
    0xdd50f1996b947519ULL,  // 10^223
    0x8a5296ffe33cc930ULL,  // 10^224
    0xace73cbfdc0bfb7bULL,  // 10^225
    0xd8210befd30efa5aULL,  // 10^226
    0x8714a775e3e95c78ULL,  // 10^227
    0xa8d9d1535ce3b396ULL,  // 10^228
    0xd31045a8341ca07cULL,  // 10^229
    0x83ea2b892091e44eULL,  // 10^230
    0xa4e4b66b68b65d61ULL,  // 10^231
    0xce1de40642e3f4b9ULL,  // 10^232
    0x80d2ae83e9ce78f4ULL,  // 10^233
    0xa1075a24e4421731ULL,  // 10^234
    0xc94930ae1d529cfdULL,  // 10^235
    0xfb9b7cd9a4a7443cULL,  // 10^236
    0x9d412e0806e88aa6ULL,  // 10^237
    0xc491798a08a2ad4fULL,  // 10^238
    0xf5b5d7ec8acb58a3ULL,  // 10^239
    0x9991a6f3d6bf1766ULL,  // 10^240
    0xbff610b0cc6edd3fULL,  // 10^241
    0xeff394dcff8a948fULL,  // 10^242
    0x95f83d0a1fb69cd9ULL,  // 10^243
    0xbb764c4ca7a44410ULL,  // 10^244
    0xea53df5fd18d5514ULL,  // 10^245
    0x92746b9be2f8552cULL,  // 10^246
    0xb7118682dbb66a77ULL,  // 10^247
    0xe4d5e82392a40515ULL,  // 10^248
    0x8f05b1163ba6832dULL,  // 10^249
    0xb2c71d5bca9023f8ULL,  // 10^250
    0xdf78e4b2bd342cf7ULL,  // 10^251
    0x8bab8eefb6409c1aULL,  // 10^252
    0xae9672aba3d0c321ULL,  // 10^253
    0xda3c0f568cc4f3e9ULL,  // 10^254
    0x8865899617fb1871ULL,  // 10^255
    0xaa7eebfb9df9de8eULL,  // 10^256
    0xd51ea6fa85785631ULL,  // 10^257
    0x8533285c936b35dfULL,  // 10^258
    0xa67ff273b8460357ULL,  // 10^259
    0xd01fef10a657842cULL,  // 10^260
    0x8213f56a67f6b29cULL,  // 10^261
    0xa298f2c501f45f43ULL,  // 10^262
    0xcb3f2f7642717713ULL,  // 10^263
    0xfe0efb53d30dd4d8ULL,  // 10^264
    0x9ec95d1463e8a507ULL,  // 10^265
    0xc67bb4597ce2ce49ULL,  // 10^266
    0xf81aa16fdc1b81dbULL,  // 10^267
    0x9b10a4e5e9913129ULL,  // 10^268
    0xc1d4ce1f63f57d73ULL,  // 10^269
    0xf24a01a73cf2dcd0ULL,  // 10^270
    0x976e41088617ca02ULL,  // 10^271
    0xbd49d14aa79dbc82ULL,  // 10^272
    0xec9c459d51852ba3ULL,  // 10^273
    0x93e1ab8252f33b46ULL,  // 10^274
    0xb8da1662e7b00a17ULL,  // 10^275
    0xe7109bfba19c0c9dULL,  // 10^276
    0x906a617d450187e2ULL,  // 10^277
    0xb484f9dc9641e9dbULL,  // 10^278
    0xe1a63853bbd26451ULL,  // 10^279
    0x8d07e33455637eb3ULL,  // 10^280
    0xb049dc016abc5e60ULL,  // 10^281
    0xdc5c5301c56b75f7ULL,  // 10^282
    0x89b9b3e11b6329bbULL,  // 10^283
    0xac2820d9623bf429ULL,  // 10^284
    0xd732290fbacaf134ULL,  // 10^285
    0x867f59a9d4bed6c0ULL,  // 10^286
    0xa81f301449ee8c70ULL,  // 10^287
    0xd226fc195c6a2f8cULL,  // 10^288
    0x83585d8fd9c25db8ULL,  // 10^289
    0xa42e74f3d032f526ULL,  // 10^290
    0xcd3a1230c43fb26fULL,  // 10^291
    0x80444b5e7aa7cf85ULL,  // 10^292
    0xa0555e361951c367ULL,  // 10^293
    0xc86ab5c39fa63441ULL,  // 10^294
    0xfa856334878fc151ULL,  // 10^295
    0x9c935e00d4b9d8d2ULL,  // 10^296
    0xc3b8358109e84f07ULL,  // 10^297
    0xf4a642e14c6262c9ULL,  // 10^298
    0x98e7e9cccfbd7dbeULL,  // 10^299
    0xbf21e44003acdd2dULL,  // 10^300
    0xeeea5d5004981478ULL,  // 10^301
    0x95527a5202df0ccbULL,  // 10^302
    0xbaa718e68396cffeULL,  // 10^303
    0xe950df20247c83fdULL,  // 10^304
    0x91d28b7416cdd27eULL,  // 10^305
    0xb6472e511c81471eULL,  // 10^306
    0xe3d8f9e563a198e5ULL,  // 10^307
    0x8e679c2f5e44ff8fULL,  // 10^308
    0xb201833b35d63f73ULL,  // 10^309
    0xde81e40a034bcf50ULL,  // 10^310
    0x8b112e86420f6192ULL,  // 10^311
    0xadd57a27d29339f6ULL,  // 10^312
    0xd94ad8b1c7380874ULL,  // 10^313
    0x87cec76f1c830549ULL,  // 10^314
    0xa9c2794ae3a3c69bULL,  // 10^315
    0xd433179d9c8cb841ULL,  // 10^316
    0x849feec281d7f329ULL,  // 10^317
    0xa5c7ea73224deff3ULL,  // 10^318
    0xcf39e50feae16bf0ULL,  // 10^319
    0x81842f29f2cce376ULL,  // 10^320
    0xa1e53af46f801c53ULL,  // 10^321
    0xca5e89b18b602368ULL,  // 10^322
    0xfcf62c1dee382c42ULL,  // 10^323
    0x9e19db92b4e31ba9ULL,  // 10^324
    0xc5a05277621be294ULL,  // 10^325
    0xf70867153aa2db39ULL,  // 10^326
    0x9a65406d44a5c903ULL,  // 10^327
    0xc0fe908895cf3b44ULL,  // 10^328
    0xf13e34aabb430a15ULL,  // 10^329
    0x96c6e0eab509e64dULL,  // 10^330
    0xbc789925624c5fe1ULL,  // 10^331
    0xeb96bf6ebadf77d9ULL,  // 10^332
    0x933e37a534cbaae8ULL,  // 10^333
    0xb80dc58e81fe95a1ULL,  // 10^334
    0xe61136f2227e3b0aULL,  // 10^335
    0x8fcac257558ee4e6ULL,  // 10^336
    0xb3bd72ed2af29e20ULL,  // 10^337
    0xe0accfa875af45a8ULL,  // 10^338
    0x8c6c01c9498d8b89ULL,  // 10^339
    0xaf87023b9bf0ee6bULL,  // 10^340
    0xdb68c2ca82ed2a06ULL,  // 10^341
    0x892179be91d43a44ULL,  // 10^342
};

static const int16_t cjsonx_eisel_lemire_exp[] = {
    -1220, -1216, -1213, -1210, -1206, -1203, -1200, -1196, -1193, -1190, -1186, -1183, -1180,
    -1176, -1173, -1170, -1166, -1163, -1160, -1156, -1153, -1150, -1146, -1143, -1140, -1136,
    -1133, -1130, -1127, -1123, -1120, -1117, -1113, -1110, -1107, -1103, -1100, -1097, -1093,
    -1090, -1087, -1083, -1080, -1077, -1073, -1070, -1067, -1063, -1060, -1057, -1053, -1050,
    -1047, -1043, -1040, -1037, -1034, -1030, -1027, -1024, -1020, -1017, -1014, -1010, -1007,
    -1004, -1000, -997,  -994,  -990,  -987,  -984,  -980,  -977,  -974,  -970,  -967,  -964,
    -960,  -957,  -954,  -950,  -947,  -944,  -940,  -937,  -934,  -931,  -927,  -924,  -921,
    -917,  -914,  -911,  -907,  -904,  -901,  -897,  -894,  -891,  -887,  -884,  -881,  -877,
    -874,  -871,  -867,  -864,  -861,  -857,  -854,  -851,  -847,  -844,  -841,  -838,  -834,
    -831,  -828,  -824,  -821,  -818,  -814,  -811,  -808,  -804,  -801,  -798,  -794,  -791,
    -788,  -784,  -781,  -778,  -774,  -771,  -768,  -764,  -761,  -758,  -754,  -751,  -748,
    -744,  -741,  -738,  -735,  -731,  -728,  -725,  -721,  -718,  -715,  -711,  -708,  -705,
    -701,  -698,  -695,  -691,  -688,  -685,  -681,  -678,  -675,  -671,  -668,  -665,  -661,
    -658,  -655,  -651,  -648,  -645,  -642,  -638,  -635,  -632,  -628,  -625,  -622,  -618,
    -615,  -612,  -608,  -605,  -602,  -598,  -595,  -592,  -588,  -585,  -582,  -578,  -575,
    -572,  -568,  -565,  -562,  -558,  -555,  -552,  -549,  -545,  -542,  -539,  -535,  -532,
    -529,  -525,  -522,  -519,  -515,  -512,  -509,  -505,  -502,  -499,  -495,  -492,  -489,
    -485,  -482,  -479,  -475,  -472,  -469,  -465,  -462,  -459,  -455,  -452,  -449,  -446,
    -442,  -439,  -436,  -432,  -429,  -426,  -422,  -419,  -416,  -412,  -409,  -406,  -402,
    -399,  -396,  -392,  -389,  -386,  -382,  -379,  -376,  -372,  -369,  -366,  -362,  -359,
    -356,  -353,  -349,  -346,  -343,  -339,  -336,  -333,  -329,  -326,  -323,  -319,  -316,
    -313,  -309,  -306,  -303,  -299,  -296,  -293,  -289,  -286,  -283,  -279,  -276,  -273,
    -269,  -266,  -263,  -259,  -256,  -253,  -250,  -246,  -243,  -240,  -236,  -233,  -230,
    -226,  -223,  -220,  -216,  -213,  -210,  -206,  -203,  -200,  -196,  -193,  -190,  -186,
    -183,  -180,  -176,  -173,  -170,  -166,  -163,  -160,  -157,  -153,  -150,  -147,  -143,
    -140,  -137,  -133,  -130,  -127,  -123,  -120,  -117,  -113,  -110,  -107,  -103,  -100,
    -97,   -93,   -90,   -87,   -83,   -80,   -77,   -73,   -70,   -67,   -63,   -60,   -57,
    -54,   -50,   -47,   -44,   -40,   -37,   -34,   -30,   -27,   -24,   -20,   -17,   -14,
    -10,   -7,    -4,    0,     3,     6,     10,    13,    16,    20,    23,    26,    30,
    33,    36,    39,    43,    46,    49,    53,    56,    59,    63,    66,    69,    73,
    76,    79,    83,    86,    89,    93,    96,    99,    103,   106,   109,   113,   116,
    119,   123,   126,   129,   132,   136,   139,   142,   146,   149,   152,   156,   159,
    162,   166,   169,   172,   176,   179,   182,   186,   189,   192,   196,   199,   202,
    206,   209,   212,   216,   219,   222,   226,   229,   232,   235,   239,   242,   245,
    249,   252,   255,   259,   262,   265,   269,   272,   275,   279,   282,   285,   289,
    292,   295,   299,   302,   305,   309,   312,   315,   319,   322,   325,   328,   332,
    335,   338,   342,   345,   348,   352,   355,   358,   362,   365,   368,   372,   375,
    378,   382,   385,   388,   392,   395,   398,   402,   405,   408,   412,   415,   418,
    422,   425,   428,   431,   435,   438,   441,   445,   448,   451,   455,   458,   461,
    465,   468,   471,   475,   478,   481,   485,   488,   491,   495,   498,   501,   505,
    508,   511,   515,   518,   521,   524,   528,   531,   534,   538,   541,   544,   548,
    551,   554,   558,   561,   564,   568,   571,   574,   578,   581,   584,   588,   591,
    594,   598,   601,   604,   608,   611,   614,   617,   621,   624,   627,   631,   634,
    637,   641,   644,   647,   651,   654,   657,   661,   664,   667,   671,   674,   677,
    681,   684,   687,   691,   694,   697,   701,   704,   707,   711,   714,   717,   720,
    724,   727,   730,   734,   737,   740,   744,   747,   750,   754,   757,   760,   764,
    767,   770,   774,   777,   780,   784,   787,   790,   794,   797,   800,   804,   807,
    810,   813,   817,   820,   823,   827,   830,   833,   837,   840,   843,   847,   850,
    853,   857,   860,   863,   867,   870,   873,   877,   880,   883,   887,   890,   893,
    897,   900,   903,   907,   910,   913,   916,   920,   923,   926,   930,   933,   936,
    940,   943,   946,   950,   953,   956,   960,   963,   966,   970,   973,   976,   980,
    983,   986,   990,   993,   996,   1000,  1003,  1006,  1009,  1013,  1016,  1019,  1023,
    1026,  1029,  1033,  1036,  1039,  1043,  1046,  1049,  1053,  1056,  1059,  1063,  1066,
    1069,  1073,
};

#endif  // cjsonx_eisel_lemire_h

// CJSONX_CLZLL is defined in cjsonx_config.h (included via cjsonx_stage2.h chain)

#ifdef __cplusplus
extern "C" {
#endif

// fast integer to double string parser using clinger's fast path and eisel-lemire algorithm.

static const double cjsonx_power_of_10[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                            1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                            1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

/*
 * reciprocals of cjsonx_power_of_10[]: multiplying by these avoids fp division.
 * fp division is ~3-5x slower than multiplication on modern cpus.
 */
static const double cjsonx_power_of_10_neg[] = {
    1.0 / 1e0,  1.0 / 1e1,  1.0 / 1e2,  1.0 / 1e3,  1.0 / 1e4,  1.0 / 1e5,  1.0 / 1e6,  1.0 / 1e7,
    1.0 / 1e8,  1.0 / 1e9,  1.0 / 1e10, 1.0 / 1e11, 1.0 / 1e12, 1.0 / 1e13, 1.0 / 1e14, 1.0 / 1e15,
    1.0 / 1e16, 1.0 / 1e17, 1.0 / 1e18, 1.0 / 1e19, 1.0 / 1e20, 1.0 / 1e21, 1.0 / 1e22};

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
static cjsonx_always_inline bool cjsonx_compute_float(uint64_t mantissa, int exponent,
                                                      bool has_truncated, double* out) {
    if (CJSONX_UNLIKELY(has_truncated)) return false;
    if (CJSONX_UNLIKELY(mantissa == 0)) {
        // zero is zero, no need to compute
        *out = 0.0;
        return true;
    }

    /*
     * clinger's fast path:
     * if the parsed mantissa fits exactly inside 53 bits (ieee-754 mantissa size, max
     * 9007199254740991) and the exponent is between -22 and 22, the conversion is mathematically
     * exact using double multiplication or division. this avoids any rounding error estimation or
     * multi-precision arithmetic.
     */
    if (mantissa <= 9007199254740991ULL && exponent >= -22 && exponent <= 22) {
        double d = (double)mantissa;
        // fix: multiply by precomputed reciprocal instead of dividing;
        // fp division is ~3-5x slower than multiplication.
        if (exponent < 0)
            d *= cjsonx_power_of_10_neg[-exponent];
        else
            d *= cjsonx_power_of_10[exponent];
        *out = d;
        return true;
    }

    /*
     * eisel-lemire algorithm:
     * used as an extremely fast fallback when clinger's fast path is not applicable but the value
     * is still representable. it computes the double using a 128-bit multiplication of the
     * normalized mantissa and precomputed powers of 10.
     */
    if (exponent < -348) {
        *out = 0.0;
        return true;
    }
    // use infinity constant — 1e308*10 is ub under -ffast-math / -ffinite-math-only
    if (exponent > 342) {
        *out = INFINITY;
        return true;
    }

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
        if (index + 1 > 690) return false;  // bounds check: table has 691 entries (0..690)
        uint64_t table_m2 = cjsonx_eisel_lemire_mantissa[index + 1];
        uint64_t high2 = cjsonx_mul64_high(w, table_m2);
        uint64_t mask2 = (1ULL << shift) - 1;
        uint64_t discarded2 = high2 & mask2;
        if (discarded2 == 0 || discarded2 == (1ULL << (shift - 1))) {
            // still ambiguous after second pass; fall back to slow path.
            return false;
        }
        // second pass resolved the ambiguity; apply its rounding decision.
        if (discarded2 > (1ULL << (shift - 1))) mantissa_53++;
        if (mantissa_53 >= (1ULL << 53)) {
            mantissa_53 >>= 1;
            shift++;
        }
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
     * assemble the bits directly: 52-bit mantissa + 11-bit biased exponent (final_exp + 1023)
     * shifted by 52
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
static cjsonx_always_inline bool cjsonx_parse_fast_float(const char* __restrict s,
                                                         const char* __restrict limit,
                                                         const char** __restrict out_end,
                                                         double* __restrict out_val) {
    const char* p = s;
    if (CJSONX_UNLIKELY(p >= limit)) return false;

    bool negative = false;
    if (*p == '-') {
        negative = true;
        p++;
    }
    if (CJSONX_UNLIKELY(p >= limit)) return false;

    uint64_t mantissa = 0;
    int digits = 0;
    bool has_truncated = false;  // true when digits beyond 19 were dropped

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
    } else
        return false;

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
        if (p < limit && (*p == '+' || *p == '-')) {
            exp_negative = (*p == '-');
            p++;
        }
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

#endif  // cjsonx_fastfloat_h
// updated 2026-07-09
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk
#ifndef CJSONX_STRING_H
#define CJSONX_STRING_H

// ███████ ████████ ██████  ██ ███    ██  ██████
// ██         ██    ██   ██ ██ ████   ██ ██
// ███████    ██    ██████  ██ ██ ██  ██ ██   ███
//      ██    ██    ██   ██ ██ ██  ██ ██ ██    ██
// ███████    ██    ██   ██ ██ ██   ████  ██████
//
// >>string processing


// updated 2026-07-09
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


#ifdef __cplusplus
extern "C" {
#endif

// copyright (c) 2008-2009 bjoern hoehrmann <bjoern@hoehrmann.de>
// see http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define CJSONX_UTF8_ACCEPT 0
#define CJSONX_UTF8_REJECT 1

static const uint8_t cjsonx_utf8d[] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 00..1f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 20..3f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 40..5f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 60..7f
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,   9,  // 80..9f
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,  // a0..bf
    8,   8,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,    // c0..df
    0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x4, 0x3, 0x3,  // e0..ef
    0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,  // f0..ff
    0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4, 0x6, 0x1, 0x1, 0x1, 0x1,  // s0..s0
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   0,   1,   1,   1,   1,   1,   0,   1,   0,   1,   1,   1,   1,   1,   1,  // s1..s2
    1,   2,   1,   1,   1,   1,   1,   2,   1,   2,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   2,   1,   1,   1,   1,   1,   1,   1,   1,  // s3..s4
    1,   2,   1,   1,   1,   1,   1,   1,   1,   2,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   3,   1,   3,   1,   1,   1,   1,   1,   1,  // s5..s6
    1,   3,   1,   1,   1,   1,   1,   3,   1,   3,   1,   1,   1,   1,   1,   1,
    1,   3,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1  // s7..s8
};

/*
 * bjoern hoehrmann's dfa-based utf-8 decoder.
 * this function decodes utf-8 octets using a finite state machine (dfa).
 * 1. it maps the current input byte to its character class (type) using the lookup table.
 * 2. if we are in the middle of decoding a multi-byte sequence (state is not cjsonx_utf8_accept),
 *    we shift the accumulated codepoint left by 6 bits and append the lower 6 bits of the byte.
 * 3. if starting a new character sequence, we extract the initial payload bits based on the type's
 * prefix.
 * 4. it transitions to the next state by indexing the transition matrix at (256 + state * 16 +
 * type).
 * 5. returns the new state: cjsonx_utf8_accept (0) if a character is successfully finished,
 *    cjsonx_utf8_reject (1) if the sequence is invalid, or a intermediate state if more bytes are
 * expected.
 */
static inline uint32_t cjsonx_utf8_decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
    uint32_t type = cjsonx_utf8d[byte];

    *codep =
        (*state != CJSONX_UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6) : (0xff >> type) & (byte);

    *state = cjsonx_utf8d[256 + *state * 16 + type];
    return *state;
}

#ifdef __cplusplus
}
#endif
#endif  // cjsonx_utf8_h

#ifdef __cplusplus
extern "C" {
#endif

// parse 4-digit hex escape (\uxxxx) to codepoint
static inline bool cjsonx_parse_hex4(const char* p, uint32_t* out) {
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        val <<= 4;
        if (c >= '0' && c <= '9')
            val |= (c - '0');
        else if (c >= 'a' && c <= 'f')
            val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            val |= (c - 'A' + 10);
        else
            return false;
    }
    *out = val;
    return true;
}

// encode unicode codepoint to utf-8 bytes, returns byte count
static inline size_t cjsonx_encode_utf8(uint32_t cp, char* out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else if (cp <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

// flat string parser: writes directly to out_node
static cjsonx_always_inline bool cjsonx_parse_string_impl(cjsonx_doc_t* doc,
                                                          cjsonx_node_t* out_node, const char* json,
                                                          uint32_t start_pos, uint32_t end_pos) {
    size_t len = end_pos - start_pos - 1;
    if (CJSONX_UNLIKELY(len > 0xFFFFFF)) {
        doc->error = CJSONX_ERROR_TOO_LARGE;  // string exceeds 24b limit
        return false;
    }
    const char* str_start = json + start_pos + 1;

    // strict utf-8 and escape validation on raw string
    size_t i = 0;
    bool has_escape = false;
    bool has_non_ascii = false;
    bool has_control = false;

#if defined(__AVX2__)
    __m256i escape_char = _mm256_set1_epi8('\\');
    for (; i + 32 <= len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(str_start + i));

        // check for non-ascii (highest bit set)
        if (CJSONX_UNLIKELY(!_mm256_testz_si256(chunk, _mm256_set1_epi8((char)0x80)))) {
            has_non_ascii = true;
        }

        // check for escape character
        __m256i cmp_esc = _mm256_cmpeq_epi8(chunk, escape_char);

        // check for control char (< 0x20)
        __m256i cmp_ctrl = _mm256_cmpeq_epi8(_mm256_subs_epu8(chunk, _mm256_set1_epi8(0x1F)),
                                             _mm256_setzero_si256());

        __m256i bad = _mm256_or_si256(cmp_esc, cmp_ctrl);
        if (CJSONX_UNLIKELY(!_mm256_testz_si256(bad, bad))) {
            if (!_mm256_testz_si256(cmp_ctrl, cmp_ctrl)) has_control = true;
            has_escape = true;
            break;  // found escape or control, break to handle it
        }
    }
#elif defined(__ARM_NEON)
    uint8x16_t escape_char = vdupq_n_u8('\\');
    uint8x16_t ctrl_limit = vdupq_n_u8(0x20);
    for (; i + 16 <= len; i += 16) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*)(str_start + i));

        // bytes >= 0x80 have bit 7 set, which makes them negative as signed int8 — clean trick
        uint8x16_t non_ascii = vcltq_s8(vreinterpretq_s8_u8(chunk), vdupq_n_s8(0));

        // check for escape character
        uint8x16_t cmp_esc = vceqq_u8(chunk, escape_char);

        // check for control char (< 0x20)
        uint8x16_t cmp_ctrl = vcltq_u8(chunk, ctrl_limit);

        uint8x16_t bad = vorrq_u8(cmp_esc, cmp_ctrl);

        // bitwise or across vector to check if any condition matched
        uint32x4_t bad_u32 = vreinterpretq_u32_u8(bad);
        uint32x4_t non_ascii_u32 = vreinterpretq_u32_u8(non_ascii);

        if (CJSONX_UNLIKELY(vmaxvq_u32(bad_u32) != 0)) {
            if (vmaxvq_u32(vreinterpretq_u32_u8(cmp_ctrl)) != 0) has_control = true;
            has_escape = true;
            break;
        }
        if (CJSONX_UNLIKELY(vmaxvq_u32(non_ascii_u32) != 0)) {
            has_non_ascii = true;
        }
    }
#elif defined(__wasm_simd128__)
    v128_t escape_char = wasm_i8x16_splat('\\');
    v128_t ctrl_limit = wasm_i8x16_splat(0x20);
    v128_t zero = wasm_i8x16_splat(0);
    for (; i + 16 <= len; i += 16) {
        v128_t chunk = wasm_v128_load((const v128_t*)(str_start + i));

        // check for non-ascii (signed < 0)
        v128_t non_ascii = wasm_i8x16_lt(chunk, zero);

        v128_t cmp_esc = wasm_i8x16_eq(chunk, escape_char);
        v128_t cmp_ctrl = wasm_u8x16_lt(chunk, ctrl_limit);

        v128_t bad = wasm_v128_or(cmp_esc, cmp_ctrl);

        if (CJSONX_UNLIKELY(wasm_v128_any_true(bad))) {
            if (wasm_v128_any_true(cmp_ctrl)) has_control = true;
            has_escape = true;
            break;
        }
        if (CJSONX_UNLIKELY(wasm_v128_any_true(non_ascii))) {
            has_non_ascii = true;
        }
    }
#endif

    /* scalar fallback for remaining bytes or to re-scan the failed simd chunk.
     * control and non-ascii flags from previous chunks are preserved.
     * dev note: using swar here for the remaining bytes is a very nice optimization,
     * avoiding byte-by-byte loops for the tail end of long strings.
     */
    uint64_t mask = 0;
    for (; i + 8 <= len; i += 8) {
        uint64_t chunk;
        memcpy(&chunk, str_start + i, 8);
        mask |= chunk;
        if (!has_escape) {
            /*
             * bitwise swar (simd within a register) scanning for backslash and control characters:
             *
             * 1. backslash search:
             *    - xor'ing the chunk with 0x5c (backslash ascii value) turns any backslash byte to
             * 0x00.
             *    - subtracting 0x01 from every byte and checking if the high bit is set (under ~x)
             *      determines if any byte in the xor product was 0x00 (classic null byte test).
             *
             * 2. control character search (< 0x20):
             *    - any control byte has the msb (bit 7) clear.
             *    - adding 0x60 to the 7-bit value of each byte causes a carry-out to the msb (bit
             * 7) if and only if the byte was >= 0x20. if it was < 0x20, no carry occurs, leaving
             * the msb clear.
             *    - checking (~t & ~msb) finds if the msb remains clear, indicating a control
             * character.
             */
            uint64_t x = chunk ^ 0x5C5C5C5C5C5C5C5CULL;
            if (CJSONX_UNLIKELY((x - 0x0101010101010101ULL) & ~x & 0x8080808080808080ULL)) {
                has_escape = true;
            }
            uint64_t msb = chunk & 0x8080808080808080ULL;
            uint64_t no_msb = chunk ^ msb;
            uint64_t t = no_msb + 0x6060606060606060ULL;
            if (CJSONX_UNLIKELY((~t & ~msb) & 0x8080808080808080ULL)) {
                has_control = true;
                has_escape = true;
            }
        }
    }
    for (; i < len; i++) {
        mask |= (uint8_t)str_start[i];
        if (CJSONX_UNLIKELY(str_start[i] == '\\')) has_escape = true;
        if (!has_escape && CJSONX_UNLIKELY((unsigned char)str_start[i] < 0x20)) {
            has_control = true;
            has_escape = true;
        }
    }
    if (CJSONX_UNLIKELY(mask & 0x8080808080808080ULL)) has_non_ascii = true;

    if (CJSONX_UNLIKELY(has_control)) {
        doc->error = CJSONX_ERROR_INVALID_CONTROL_CHAR;
        return false;
    }

    if (CJSONX_UNLIKELY(has_non_ascii)) {
        uint32_t state = CJSONX_UTF8_ACCEPT;
        uint32_t codep;
        for (size_t j = 0; j < len; j++) {
            cjsonx_utf8_decode(&state, &codep, (uint8_t)str_start[j]);
            if (CJSONX_UNLIKELY(state == CJSONX_UTF8_REJECT)) {
                doc->error = CJSONX_ERROR_INVALID_UTF8;
                return false;
            }
        }
        if (CJSONX_UNLIKELY(state != CJSONX_UTF8_ACCEPT)) {
            doc->error = CJSONX_ERROR_INVALID_UTF8;
            return false;
        }
    }

    // zero-copy fast path
    if (!has_escape) {
        cjsonx_node_set_type_len(out_node, CJSONX_STRING, (uint32_t)len);
        out_node->val.str = str_start;
        return true;
    }

    // slow path with arena allocation
    char* out = (char*)cjsonx_arena_alloc(doc, len + 1);
    if (!out) {
        doc->error = CJSONX_ERROR_OOM;
        return false;
    }

    const char* p = str_start;
    const char* end = str_start + len;
    char* d = out;

    while (p < end) {
        if (*p == '\\') {
            p++;
            if (p >= end) {
                doc->error = CJSONX_ERROR_INVALID_ESCAPE;
                return false;
            }
            switch (*p) {
                case '"':
                    *d++ = '"';
                    p++;
                    break;
                case '\\':
                    *d++ = '\\';
                    p++;
                    break;
                case '/':
                    *d++ = '/';
                    p++;
                    break;
                case 'b':
                    *d++ = '\b';
                    p++;
                    break;
                case 'f':
                    *d++ = '\f';
                    p++;
                    break;
                case 'n':
                    *d++ = '\n';
                    p++;
                    break;
                case 'r':
                    *d++ = '\r';
                    p++;
                    break;
                case 't':
                    *d++ = '\t';
                    p++;
                    break;
                case 'u': {
                    p++;
                    if (p + 4 > end) {
                        doc->error = CJSONX_ERROR_INVALID_ESCAPE;
                        return false;
                    }
                    uint32_t cp = 0;
                    if (!cjsonx_parse_hex4(p, &cp)) {
                        doc->error = CJSONX_ERROR_INVALID_ESCAPE;
                        return false;
                    }
                    p += 4;
                    /*
                     * decode utf-16 surrogate pairs (rfc8259 section 7):
                     * json represents characters outside the basic multilingual plane (bmp) using a
                     * surrogate pair.
                     * - cp (high surrogate): must be in the range 0xd800 to 0xdbff.
                     * - cp2 (low surrogate): must immediately follow as \uXXXX and be in the range
                     * 0xdc00 to 0xdfff.
                     * - combining the pair: (((high - 0xd800) << 10) | (low - 0xdc00)) + 0x10000.
                     *
                     * dev note: great job handling this. many parsers mess up surrogate pairs or
                     * accept lone surrogates which violates the rfc.
                     */
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (p + 6 > end || p[0] != '\\' || p[1] != 'u') {
                            doc->error = CJSONX_ERROR_INVALID_ESCAPE;
                            return false;
                        }
                        uint32_t cp2 = 0;
                        if (!cjsonx_parse_hex4(p + 2, &cp2)) {
                            doc->error = CJSONX_ERROR_INVALID_ESCAPE;
                            return false;
                        }
                        if (cp2 < 0xDC00 || cp2 > 0xDFFF) {
                            doc->error = CJSONX_ERROR_INVALID_ESCAPE;
                            return false;
                        }
                        cp = (((cp - 0xD800) << 10) | (cp2 - 0xDC00)) + 0x10000;
                        p += 6;
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        // lone low surrogate is invalid
                        doc->error = CJSONX_ERROR_INVALID_ESCAPE;
                        return false;
                    }
                    size_t enc_len = cjsonx_encode_utf8(cp, d);
                    if (enc_len == 0) {
                        doc->error = CJSONX_ERROR_INVALID_UTF8;
                        return false;
                    }
                    d += enc_len;
                    break;
                }
                default: {
                    doc->error = CJSONX_ERROR_INVALID_ESCAPE;
                    return false;
                }
            }
        } else {
            if (CJSONX_UNLIKELY((unsigned char)*p < 0x20)) {
                doc->error = CJSONX_ERROR_INVALID_CONTROL_CHAR;
                return false;
            }
            *d++ = *p++;
        }
    }
    *d = '\0';
    cjsonx_node_set_type_len(out_node, CJSONX_STRING, (uint32_t)(d - out));
    out_node->val.str = out;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif  // cjsonx_string_h

typedef enum {
    CJSONX_S_VAL = 1,
    CJSONX_S_KEY = 2,
    CJSONX_S_COL = 4,
    CJSONX_S_COM = 8,
    CJSONX_S_OBJ_END = 16,
    CJSONX_S_ARR_END = 32,
    CJSONX_S_EOF = 64
} cjsonx_state_mask_t;

static cjsonx_always_inline uint8_t
cjsonx_get_next_mask(uint32_t parent_depth, const uint8_t* __restrict parent_type_stack) {
    if (CJSONX_UNLIKELY(parent_depth == 0)) return CJSONX_S_EOF;
    uint8_t ptype = parent_type_stack[parent_depth - 1];
    if (ptype == CJSONX_OBJECT) return CJSONX_S_COM | CJSONX_S_OBJ_END;
    return CJSONX_S_COM | CJSONX_S_ARR_END;
}

// check if the character range is only whitespace
static cjsonx_always_inline bool cjsonx_is_all_whitespace(const char* str, const char* limit) {
    while (str < limit) {
        char c = *str;
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return false;
        }
        str++;
    }
    return true;
}

// validate json number grammar (no leading zeros, require digits after dot/exp)
static cjsonx_always_inline bool cjsonx_is_valid_number(const char* __restrict str,
                                                        const char* __restrict limit) {
    if (str >= limit) return false;
    if (*str == '-') str++;
    if (str >= limit) return false;

    if (*str == '0') {
        str++;
        if (str < limit && *str >= '0' && *str <= '9')
            return false;  // no leading zero allowed e.g. 01
    } else if (*str >= '1' && *str <= '9') {
        str++;
        while (str < limit && *str >= '0' && *str <= '9') str++;
    } else {
        return false;
    }

    if (str < limit && *str == '.') {
        str++;
        if (str >= limit || *str < '0' || *str > '9') return false;  // require digit after dot
        while (str < limit && *str >= '0' && *str <= '9') str++;
    }

    if (str < limit && (*str == 'e' || *str == 'E')) {
        str++;
        if (str < limit && (*str == '+' || *str == '-')) str++;
        if (str >= limit || *str < '0' || *str > '9') return false;  // require digit after exp
        while (str < limit && *str >= '0' && *str <= '9') str++;
    }

    return cjsonx_is_all_whitespace(str, limit);
}

// grow nodes array dynamically when capacity is exceeded due to invalid json structures
static inline bool cjsonx_grow_nodes(cjsonx_doc_t* doc, size_t required) {
    if (required <= doc->node_capacity) return true;
    if (doc->is_static) return false;
    size_t new_cap = doc->node_capacity == 0 ? 128 : doc->node_capacity * 2;
    if (new_cap < required) new_cap = required;
    if (CJSONX_UNLIKELY(new_cap >= UINT32_MAX - 1 || new_cap > (size_t)-1 / sizeof(cjsonx_node_t)))
        return false;  // check for index overflow
    cjsonx_node_t* new_nodes = (cjsonx_node_t*)cjsonx_realloc(
        &doc->alloc, doc->nodes, doc->node_capacity * sizeof(cjsonx_node_t),
        new_cap * sizeof(cjsonx_node_t));
    if (!new_nodes) return false;
    doc->nodes = new_nodes;
    doc->node_capacity = new_cap;
    return true;
}

// thread-safe locale-independent float parsing fallback
static inline double cjsonx_strtod(char* buf, char** endptr) {
#if defined(_MSC_VER)
    _locale_t loc = _create_locale(LC_ALL, "C");
    double val = _strtod_l(buf, endptr, loc);
    if (loc) _free_locale(loc);
    return val;
#elif defined(__APPLE__)
    /* xlocale.h exposes strtod_l on apple platforms */
#include <xlocale.h>
    /*
     * static locale cached for the process lifetime — never freed intentionally.
     * this avoids a newlocale/freelocale pair on every float parse fallback call.
     * note: initialization is not atomic; a benign double-init race is possible in
     * multi-threaded code. both threads create an equivalent "C" locale so the
     * result is always correct (worst case: one extra locale handle leaked on the
     * very first concurrent call).
     */
    static locale_t s_c_locale_apple = (locale_t)0;
    if (CJSONX_UNLIKELY(s_c_locale_apple == (locale_t)0)) {
        s_c_locale_apple = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    }
    if (s_c_locale_apple != (locale_t)0) {
        return strtod_l(buf, endptr, s_c_locale_apple);
    }
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    /*
     * static locale cached for the process lifetime — never freed intentionally.
     * this avoids a newlocale/freelocale pair on every float parse fallback call.
     * note: initialization is not atomic; a benign double-init race is possible in
     * multi-threaded code. both threads create an equivalent "C" locale so the
     * result is always correct (worst case: one extra locale handle leaked on the
     * very first concurrent call).
     */
    static locale_t s_c_locale_posix = (locale_t)0;
    if (CJSONX_UNLIKELY(s_c_locale_posix == (locale_t)0)) {
        s_c_locale_posix = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    }
    if (s_c_locale_posix != (locale_t)0) {
        return strtod_l(buf, endptr, s_c_locale_posix);
    }
#endif
    /* fallback for platforms without strtod_l (e.g. bare metal, msvc fallthrough):
     * temporarily swap the decimal point character to match the host locale */
    {
        struct lconv* lc = localeconv();
        char original_dot = '.';
        char* dot = NULL;
        double val_fallback;
        if (lc && lc->decimal_point && lc->decimal_point[0] != '.') {
            dot = strchr(buf, '.');
            if (dot) {
                original_dot = *dot;
                *dot = lc->decimal_point[0];
            }
        }
        val_fallback = strtod(buf, endptr);
        if (dot) {
            *dot = original_dot;  // restore original char
        }
        return val_fallback;
    }
}

// non-recursive flat dom parsing engine - strict grammar edition
static bool cjsonx_stage2_build(cjsonx_doc_t* doc, const char* json, cjsonx_tape_t* tape) {
    if (tape->count == 0) return false;

/*
 * helper to assign correct error based on allowed mask (all comments in lowercase only).
 * if allowed mask has colons or commas, we assign the specific missing colon/comma errors,
 * otherwise fallback to a generic unexpected token error.
 */
#define CJSONX_PARSE_FAIL_EXPECTED(allowed)             \
    do {                                                \
        if ((allowed) & CJSONX_S_COL)                   \
            doc->error = CJSONX_ERROR_MISSING_COLON;    \
        else if ((allowed) & CJSONX_S_COM)              \
            doc->error = CJSONX_ERROR_MISSING_COMMA;    \
        else                                            \
            doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN; \
        goto fail;                                      \
    } while (0)

    // skip allocation if nodes were already pre-allocated (e.g. static buffer)
    if (!doc->nodes) {
        size_t alloc_count = tape->count / 2 + 1;
        if (CJSONX_UNLIKELY(alloc_count >= UINT32_MAX - 1 ||
                            alloc_count > (size_t)-1 / sizeof(cjsonx_node_t))) {
            doc->error = CJSONX_ERROR_OOM;
            return false;
        }
        if (doc->alloc.malloc_fn) {
            doc->nodes = (cjsonx_node_t*)doc->alloc.malloc_fn(alloc_count * sizeof(cjsonx_node_t),
                                                              doc->alloc.user_data);
        } else {
            doc->nodes = (cjsonx_node_t*)malloc(alloc_count * sizeof(cjsonx_node_t));
        }
        if (!doc->nodes) {
            doc->error = CJSONX_ERROR_OOM;
            return false;
        }
        doc->node_capacity = alloc_count;
    }

    size_t node_idx = 0;
    size_t tape_idx = 0;
    size_t err_tape_idx = 0;

    /*
     * non-recursive parsing state tracking:
     * instead of consuming stack frames via recursive function calls (which risks stack overflows
     * on deeply nested json), we use a single, fast loop with a lightweight state machine.
     * - parent_stack: holds the indices of active parent nodes (objects and arrays).
     * - parent_type_stack: stores whether the parent is an object or array to enforce syntax rules.
     * - count_stack: tracks the child count for the current container level.
     * - parent_depth: tracks the current nesting depth, bounded by cjsonx_max_depth.
     */
    uint32_t parent_stack[CJSONX_MAX_DEPTH];
    uint8_t parent_type_stack[CJSONX_MAX_DEPTH];
    uint32_t count_stack[CJSONX_MAX_DEPTH];  // l1 hot-stack for element counts
    uint32_t parent_depth = 0;
    uint8_t allowed_mask = CJSONX_S_VAL;

    uint32_t pos;
    char c;
    cjsonx_node_t* node;

    /*
     * direct-threaded code (computed gotos):
     * when compiled with gcc or clang, we use the labels-as-values extension (&&)
     * to build a static dispatch table. this allows us to jump directly to the code handling
     * the next token (cjsonx_next_token) without the overhead of switch-case statement routing.
     * this maximizes instruction cache throughput and significantly improves branch prediction.
     */
#if defined(__GNUC__) || defined(__clang__)
#define CJSONX_USE_GOTOS 1
#endif

#define CJSONX_ENSURE_CAPACITY()                                          \
    do {                                                                  \
        if (CJSONX_UNLIKELY(node_idx >= doc->node_capacity)) {            \
            if (CJSONX_UNLIKELY(!cjsonx_grow_nodes(doc, node_idx + 1))) { \
                doc->error = CJSONX_ERROR_OOM;                            \
                goto fail;                                                \
            }                                                             \
        }                                                                 \
        node = &doc->nodes[node_idx];                                     \
    } while (0)

#ifdef CJSONX_USE_GOTOS
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
#endif
    static const void* dispatch_table[256] = {
        [0 ... 255] = &&l_default, ['"'] = &&l_string,      ['{'] = &&l_obj_arr_start,
        ['['] = &&l_obj_arr_start, ['}'] = &&l_obj_arr_end, [']'] = &&l_obj_arr_end,
        [':'] = &&l_colon,         [','] = &&l_comma,       ['t'] = &&l_true,
        ['f'] = &&l_false,         ['n'] = &&l_null};
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define CJSONX_NEXT_TOKEN()                                          \
    do {                                                             \
        if (CJSONX_UNLIKELY(tape_idx >= tape->count)) goto end_loop; \
        err_tape_idx = tape_idx;                                     \
        pos = tape->indices[tape_idx];                               \
        c = json[pos];                                               \
        goto* dispatch_table[(uint8_t)c];                            \
    } while (0)

#define CJSONX_CASE(lbl, ch) l_##lbl:
#define CJSONX_CASE_MULTI(lbl, ch1, ch2) l_##lbl:
#define CJSONX_CASE_DEFAULT() \
    l_default:

    CJSONX_NEXT_TOKEN();
#else
#define CJSONX_NEXT_TOKEN() break
#define CJSONX_CASE(lbl, ch) case ch:
#define CJSONX_CASE_MULTI(lbl, ch1, ch2) \
    case ch1:                            \
    case ch2:
#define CJSONX_CASE_DEFAULT() default:

    while (tape_idx < tape->count) {
        err_tape_idx = tape_idx;
        pos = tape->indices[tape_idx];
        c = json[pos];
        switch (c) {
#endif
    CJSONX_CASE(string, '"') {
        CJSONX_ENSURE_CAPACITY();
        if (CJSONX_UNLIKELY(!(allowed_mask & (CJSONX_S_KEY | CJSONX_S_VAL)))) {
            CJSONX_PARSE_FAIL_EXPECTED(allowed_mask);
        }
        tape_idx++;
        if (CJSONX_UNLIKELY(tape_idx >= tape->count)) goto fail;
        uint32_t end_pos = tape->indices[tape_idx];
        if (CJSONX_UNLIKELY(!cjsonx_parse_string_impl(doc, node, json, pos, end_pos))) goto fail;
        node->next_sibling = (uint32_t)(node_idx + 1);
        if (parent_depth > 0) count_stack[parent_depth - 1]++;
        node_idx++;
        tape_idx++;
        if (allowed_mask & CJSONX_S_KEY)
            allowed_mask = CJSONX_S_COL;
        else
            allowed_mask = cjsonx_get_next_mask(parent_depth, parent_type_stack);
        CJSONX_NEXT_TOKEN();
    }
    CJSONX_CASE_MULTI(obj_arr_start, '{', '[') {
        CJSONX_ENSURE_CAPACITY();
        if (CJSONX_UNLIKELY(!(allowed_mask & CJSONX_S_VAL))) {
            CJSONX_PARSE_FAIL_EXPECTED(allowed_mask);
        }
        if (CJSONX_UNLIKELY(parent_depth >= CJSONX_MAX_DEPTH)) {
            doc->error = CJSONX_ERROR_MAX_DEPTH;
            goto fail;
        }
        cjsonx_type_t t = (c == '{') ? CJSONX_OBJECT : CJSONX_ARRAY;
        cjsonx_node_set_type_len(node, t, 0);              // temp length
        node->val.first_child = (uint32_t)(node_idx + 1);  // set first child
        if (parent_depth > 0) count_stack[parent_depth - 1]++;
        parent_stack[parent_depth] = (uint32_t)node_idx;
        parent_type_stack[parent_depth] = t;
        count_stack[parent_depth] = 0;
        parent_depth++;
        node_idx++;
        tape_idx++;
        allowed_mask =
            (c == '{') ? (CJSONX_S_KEY | CJSONX_S_OBJ_END) : (CJSONX_S_VAL | CJSONX_S_ARR_END);
        CJSONX_NEXT_TOKEN();
    }
    CJSONX_CASE_MULTI(obj_arr_end, '}', ']') {
        if (CJSONX_UNLIKELY(c == '}' && !(allowed_mask & CJSONX_S_OBJ_END))) {
            if (allowed_mask & CJSONX_S_KEY)
                doc->error = CJSONX_ERROR_TRAILING_COMMA;
            else
                doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN;
            goto fail;
        }
        if (CJSONX_UNLIKELY(c == ']' && !(allowed_mask & CJSONX_S_ARR_END))) {
            if (allowed_mask & CJSONX_S_VAL)
                doc->error = CJSONX_ERROR_TRAILING_COMMA;
            else
                doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN;
            goto fail;
        }
        if (CJSONX_UNLIKELY(parent_depth == 0)) {
            doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN;
            goto fail;
        }
        uint32_t parent = parent_stack[--parent_depth];
        uint32_t count = count_stack[parent_depth];
        cjsonx_node_t* pnode = &doc->nodes[parent];
        cjsonx_type_t ptype = cjsonx_node_type(pnode);
        if (CJSONX_UNLIKELY((c == '}' && ptype != CJSONX_OBJECT) ||
                            (c == ']' && ptype != CJSONX_ARRAY))) {
            doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN;
            goto fail;
        }
        pnode->next_sibling = (uint32_t)node_idx;
        if (ptype == CJSONX_OBJECT) count /= 2;
        if (CJSONX_UNLIKELY(count > 0xFFFFFF)) {
            doc->error = CJSONX_ERROR_TOO_LARGE;  // container exceeds 24b limit
            goto fail;
        }
        cjsonx_node_set_type_len(pnode, ptype, count);
        tape_idx++;
        allowed_mask = cjsonx_get_next_mask(parent_depth, parent_type_stack);
        CJSONX_NEXT_TOKEN();
    }
    CJSONX_CASE(colon, ':') {
        if (CJSONX_UNLIKELY(!(allowed_mask & CJSONX_S_COL))) {
            CJSONX_PARSE_FAIL_EXPECTED(allowed_mask);
        }
        tape_idx++;
        allowed_mask = CJSONX_S_VAL;
        CJSONX_NEXT_TOKEN();
    }
    CJSONX_CASE(comma, ',') {
        if (CJSONX_UNLIKELY(!(allowed_mask & CJSONX_S_COM))) {
            CJSONX_PARSE_FAIL_EXPECTED(allowed_mask);
        }
        if (CJSONX_UNLIKELY(parent_depth == 0)) {
            doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN;
            goto fail;
        }
        tape_idx++;
        // optimization: read from parent_type_stack instead of doc->nodes to avoid cache miss
        cjsonx_type_t ptype = parent_type_stack[parent_depth - 1];
        allowed_mask = (ptype == CJSONX_OBJECT) ? CJSONX_S_KEY : CJSONX_S_VAL;
        CJSONX_NEXT_TOKEN();
    }
    CJSONX_CASE(true, 't') {
        CJSONX_ENSURE_CAPACITY();
        if (CJSONX_UNLIKELY(!(allowed_mask & CJSONX_S_VAL))) {
            CJSONX_PARSE_FAIL_EXPECTED(allowed_mask);
        }
        const char* limit = (tape_idx + 1 < tape->count) ? (json + tape->indices[tape_idx + 1])
                                                         : (json + doc->json_len);
        if (CJSONX_UNLIKELY(limit - (json + pos) < 4 || memcmp(json + pos, "true", 4) != 0 ||
                            !cjsonx_is_all_whitespace(json + pos + 4, limit))) {
            doc->error = CJSONX_ERROR_INVALID_KEYWORD;
            goto fail;
        }
        cjsonx_node_set_type_len(node, CJSONX_BOOL, 0);
        node->val.b = true;
        node->next_sibling = (uint32_t)(node_idx + 1);
        if (parent_depth > 0) count_stack[parent_depth - 1]++;
        node_idx++;
        tape_idx++;
        allowed_mask = cjsonx_get_next_mask(parent_depth, parent_type_stack);
        CJSONX_NEXT_TOKEN();
    }
    CJSONX_CASE(false, 'f') {
        CJSONX_ENSURE_CAPACITY();
        if (CJSONX_UNLIKELY(!(allowed_mask & CJSONX_S_VAL))) {
            CJSONX_PARSE_FAIL_EXPECTED(allowed_mask);
        }
        const char* limit = (tape_idx + 1 < tape->count) ? (json + tape->indices[tape_idx + 1])
                                                         : (json + doc->json_len);
        if (CJSONX_UNLIKELY(limit - (json + pos) < 5 || memcmp(json + pos, "false", 5) != 0 ||
                            !cjsonx_is_all_whitespace(json + pos + 5, limit))) {
            doc->error = CJSONX_ERROR_INVALID_KEYWORD;
            goto fail;
        }
        cjsonx_node_set_type_len(node, CJSONX_BOOL, 0);
        node->val.b = false;
        node->next_sibling = (uint32_t)(node_idx + 1);
        if (parent_depth > 0) count_stack[parent_depth - 1]++;
        node_idx++;
        tape_idx++;
        allowed_mask = cjsonx_get_next_mask(parent_depth, parent_type_stack);
        CJSONX_NEXT_TOKEN();
    }
    CJSONX_CASE(null, 'n') {
        CJSONX_ENSURE_CAPACITY();
        if (CJSONX_UNLIKELY(!(allowed_mask & CJSONX_S_VAL))) {
            CJSONX_PARSE_FAIL_EXPECTED(allowed_mask);
        }
        const char* limit = (tape_idx + 1 < tape->count) ? (json + tape->indices[tape_idx + 1])
                                                         : (json + doc->json_len);
        if (CJSONX_UNLIKELY(limit - (json + pos) < 4 || memcmp(json + pos, "null", 4) != 0 ||
                            !cjsonx_is_all_whitespace(json + pos + 4, limit))) {
            doc->error = CJSONX_ERROR_INVALID_KEYWORD;
            goto fail;
        }
        cjsonx_node_set_type_len(node, CJSONX_NULL, 0);
        node->next_sibling = (uint32_t)(node_idx + 1);
        if (parent_depth > 0) count_stack[parent_depth - 1]++;
        node_idx++;
        tape_idx++;
        allowed_mask = cjsonx_get_next_mask(parent_depth, parent_type_stack);
        CJSONX_NEXT_TOKEN();
    }
    CJSONX_CASE_DEFAULT() {  // number
        CJSONX_ENSURE_CAPACITY();
        if (CJSONX_UNLIKELY(!(allowed_mask & CJSONX_S_VAL))) {
            CJSONX_PARSE_FAIL_EXPECTED(allowed_mask);
        }
        const char* limit = (tape_idx + 1 < tape->count) ? (json + tape->indices[tape_idx + 1])
                                                         : (json + doc->json_len);
        if (CJSONX_UNLIKELY(!cjsonx_is_valid_number(json + pos, limit))) {
            doc->error = CJSONX_ERROR_INVALID_NUMBER;
            goto fail;
        }
        cjsonx_node_set_type_len(node, CJSONX_NUMBER, 0);

        double val = 0;
        const char* end = NULL;
        if (CJSONX_UNLIKELY(!cjsonx_parse_fast_float(json + pos, limit, &end, &val))) {
            const char* num_end = json + pos;
            while (num_end < limit &&
                   ((*num_end >= '0' && *num_end <= '9') || *num_end == '.' || *num_end == 'e' ||
                    *num_end == 'E' || *num_end == '+' || *num_end == '-')) {
                num_end++;
            }
            ptrdiff_t raw_len = num_end - (json + pos);
            if (CJSONX_UNLIKELY(raw_len <= 0)) {
                doc->error = CJSONX_ERROR_INVALID_NUMBER;
                goto fail;
            }
            size_t num_len = (size_t)raw_len;

            /*
             * support arbitrarily long numbers in fallback float parsing:
             * usually, json numbers fit within 511 chars, so we use a stack buffer local_buf.
             * if the raw float string exceeds 511 chars (due to trailing zeros or extremely long
             * decimals), we allocate a dynamic buffer using the allocator hooks to prevent
             * heap/stack overflows, and safely free it after strtod parses the double.
             */
            char local_buf[512];
            char* buf = local_buf;
            if (CJSONX_UNLIKELY(num_len >= 512)) {
                if (doc->alloc.malloc_fn) {
                    buf = (char*)doc->alloc.malloc_fn(num_len + 1, doc->alloc.user_data);
                } else {
                    buf = (char*)malloc(num_len + 1);
                }
                if (!buf) {
                    doc->error = CJSONX_ERROR_OOM;
                    goto fail;
                }
            }
            memcpy(buf, json + pos, num_len);
            buf[num_len] = '\0';

            val = cjsonx_strtod(buf, (char**)&end);
            bool parse_ok = (end != buf);
            if (CJSONX_UNLIKELY(num_len >= 512)) {
                if (doc->alloc.free_fn)
                    doc->alloc.free_fn(buf, doc->alloc.user_data);
                else
                    free(buf);
            }
            if (CJSONX_UNLIKELY(!parse_ok)) {
                doc->error = CJSONX_ERROR_INVALID_NUMBER;
                goto fail;
            }
            end = (json + pos) + (end - buf);
        }
        while (end < limit && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) end++;
        if (CJSONX_UNLIKELY(end != limit)) {
            doc->error = CJSONX_ERROR_INVALID_NUMBER;
            goto fail;
        }
        node->val.f64 = val;
        node->next_sibling = (uint32_t)(node_idx + 1);
        if (parent_depth > 0) count_stack[parent_depth - 1]++;
        node_idx++;
        tape_idx++;
        allowed_mask = cjsonx_get_next_mask(parent_depth, parent_type_stack);
        CJSONX_NEXT_TOKEN();
    }
#ifndef CJSONX_USE_GOTOS
}
}
#endif
end_loop:

    if (parent_depth != 0) {
    doc->error = CJSONX_ERROR_UNCLOSED_CONTAINER;
    goto fail;
}
if (allowed_mask != CJSONX_S_EOF) {
    if (allowed_mask & CJSONX_S_COL)
        doc->error = CJSONX_ERROR_MISSING_COLON;
    else if (allowed_mask & CJSONX_S_COM)
        doc->error = CJSONX_ERROR_MISSING_COMMA;
    else
        doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN;
    goto fail;
}

doc->node_count = node_idx;
doc->is_valid = true;
doc->root.doc = doc;
doc->root.node_idx = 0;
return true;

fail: doc->is_valid = false;
if (doc->error == CJSONX_SUCCESS) {
    doc->error =
        (parent_depth >= CJSONX_MAX_DEPTH) ? CJSONX_ERROR_MAX_DEPTH : CJSONX_ERROR_UNEXPECTED_TOKEN;
}
doc->error_offset = (tape_idx < tape->count) ? tape->indices[err_tape_idx] : doc->json_len;
return false;
}

cjsonx_doc_t* cjsonx_doc_new_ex(cjsonx_allocator_t* alloc) {
    cjsonx_doc_t* doc;
    if (alloc && alloc->malloc_fn) {
        doc = (cjsonx_doc_t*)alloc->malloc_fn(sizeof(cjsonx_doc_t), alloc->user_data);
        if (doc) memset(doc, 0, sizeof(cjsonx_doc_t));
    } else {
        doc = (cjsonx_doc_t*)calloc(1, sizeof(cjsonx_doc_t));
    }
    if (!doc) return NULL;

    if (alloc) doc->alloc = *alloc;
    doc->is_valid = true;

    cjsonx_arena_node_t* init_node;
    if (doc->alloc.malloc_fn) {
        init_node = (cjsonx_arena_node_t*)doc->alloc.malloc_fn(
            sizeof(cjsonx_arena_node_t) + CJSONX_ARENA_CHUNK_SIZE, doc->alloc.user_data);
    } else {
        init_node =
            (cjsonx_arena_node_t*)malloc(sizeof(cjsonx_arena_node_t) + CJSONX_ARENA_CHUNK_SIZE);
    }
    if (!init_node) {
        if (doc->alloc.free_fn)
            doc->alloc.free_fn(doc, doc->alloc.user_data);
        else
            free(doc);
        return NULL;
    }
    init_node->next = NULL;
    doc->head = init_node;
    doc->current_chunk = (uint8_t*)(init_node + 1);
    doc->chunk_size = CJSONX_ARENA_CHUNK_SIZE;
    doc->chunk_used = 0;

    // pre-allocate flat dom node array starting with 16 nodes capacity
    cjsonx_node_t* nodes;
    if (doc->alloc.malloc_fn) {
        nodes =
            (cjsonx_node_t*)doc->alloc.malloc_fn(16 * sizeof(cjsonx_node_t), doc->alloc.user_data);
    } else {
        nodes = (cjsonx_node_t*)malloc(16 * sizeof(cjsonx_node_t));
    }
    if (!nodes) {
        cjsonx_doc_free(doc);
        return NULL;
    }
    doc->nodes = nodes;
    doc->node_capacity = 16;
    doc->node_count = 1;
    doc->root.doc = doc;
    doc->root.node_idx = 0;
    cjsonx_node_set_type_len(&doc->nodes[0], CJSONX_NULL, 0);
    doc->nodes[0].next_sibling = UINT32_MAX;

    return doc;
}

cjsonx_doc_t* cjsonx_doc_new(void) {
    return cjsonx_doc_new_ex(NULL);
}

void cjsonx_doc_free(cjsonx_doc_t* doc) {
    if (!doc) return;
    cjsonx_allocator_t* alloc = &doc->alloc;
    if (doc->nodes && !doc->is_static) {
        if (alloc->free_fn)
            alloc->free_fn(doc->nodes, alloc->user_data);
        else
            free(doc->nodes);
    }
    // free owned json buffer (set by cjsonx_read_file)
    if (doc->owned_json) {
        if (alloc->free_fn)
            alloc->free_fn(doc->owned_json, alloc->user_data);
        else
            free(doc->owned_json);
    }
    if (!doc->is_static) {
        cjsonx_arena_node_t* current = doc->head;
        while (current) {
            cjsonx_arena_node_t* next = current->next;
            if (alloc->free_fn)
                alloc->free_fn(current, alloc->user_data);
            else
                free(current);
            current = next;
        }
        if (alloc->free_fn)
            alloc->free_fn(doc, alloc->user_data);
        else
            free(doc);
    }
}

// dom value accessors — get by key, index, and json pointer
cjsonx_val_t cjsonx_get_len(cjsonx_val_t obj_handle, const char* key, size_t key_len) {
    if (!obj_handle.doc) return cjsonx_make_null_handle();
    if (!key) {
        key = "";
        key_len = 0;
    }
    cjsonx_node_t* obj = &obj_handle.doc->nodes[obj_handle.node_idx];
    if (cjsonx_node_type(obj) != CJSONX_OBJECT) return cjsonx_make_null_handle();

    uint32_t curr = obj->val.first_child;
    size_t len = cjsonx_node_length(obj);
    for (size_t i = 0; i < len; i++) {
        cjsonx_node_t* k_node = &obj_handle.doc->nodes[curr];
        uint32_t val_idx = k_node->next_sibling;

        if (cjsonx_node_length(k_node) == key_len && memcmp(k_node->val.str, key, key_len) == 0) {
            cjsonx_val_t v = {obj_handle.doc, val_idx};
            return v;
        }
        curr = obj_handle.doc->nodes[val_idx].next_sibling;
    }
    return cjsonx_make_null_handle();
}

cjsonx_val_t cjsonx_get(cjsonx_val_t obj_handle, const char* key) {
    return cjsonx_get_len(obj_handle, key, key ? strlen(key) : 0);
}

cjsonx_val_t cjsonx_get_index(cjsonx_val_t arr_handle, size_t index) {
    if (!arr_handle.doc) return cjsonx_make_null_handle();
    cjsonx_node_t* arr = &arr_handle.doc->nodes[arr_handle.node_idx];
    if (cjsonx_node_type(arr) != CJSONX_ARRAY) return cjsonx_make_null_handle();

    size_t len = cjsonx_node_length(arr);
    if (index >= len) return cjsonx_make_null_handle();

    uint32_t curr = arr->val.first_child;
    for (size_t i = 0; i < len; i++) {
        if (i == index) {
            cjsonx_val_t v = {arr_handle.doc, curr};
            return v;
        }
        curr = arr_handle.doc->nodes[curr].next_sibling;
    }
    return cjsonx_make_null_handle();
}

cjsonx_val_t cjsonx_pointer_get(cjsonx_val_t root, const char* path) {
    if (!root.doc || !path) return cjsonx_make_null_handle();
    if (path[0] == '\0') return root;
    if (path[0] != '/') return cjsonx_make_null_handle();

    cjsonx_val_t curr = root;
    const char* p = path;

    while (*p == '/') {
        p++;  // skip '/'
        const char* next_slash = strchr(p, '/');
        size_t token_len = next_slash ? (size_t)(next_slash - p) : strlen(p);

        char unescaped[256];
        char* token = unescaped;
        bool needs_free = false;
        if (token_len >= sizeof(unescaped)) {
            if (root.doc->alloc.malloc_fn) {
                token = (char*)root.doc->alloc.malloc_fn(token_len + 1, root.doc->alloc.user_data);
            } else {
                token = (char*)malloc(token_len + 1);
            }
            if (!token) return cjsonx_make_null_handle();
            needs_free = true;
        }

        size_t unescaped_len = 0;
        for (size_t i = 0; i < token_len; i++) {
            if (p[i] == '~' && i + 1 < token_len) {
                if (p[i + 1] == '1') {
                    token[unescaped_len++] = '/';
                    i++;
                    continue;
                }
                if (p[i + 1] == '0') {
                    token[unescaped_len++] = '~';
                    i++;
                    continue;
                }
            }
            token[unescaped_len++] = p[i];
        }
        token[unescaped_len] = '\0';

        cjsonx_type_t t = cjsonx_get_type(curr);
        if (t == CJSONX_OBJECT) {
            curr = cjsonx_get(curr, token);
        } else if (t == CJSONX_ARRAY) {
            bool is_valid_index = true;
            if (token[0] == '\0') {
                is_valid_index = false;
            } else if (token[0] == '0' && token[1] != '\0') {
                is_valid_index = false;
            } else {
                for (size_t i = 0; token[i] != '\0'; i++) {
                    if (token[i] < '0' || token[i] > '9') {
                        is_valid_index = false;
                        break;
                    }
                }
            }
            if (!is_valid_index) {
                curr = cjsonx_make_null_handle();
            } else {
                // check errno so strtoul overflow (returns ulong_max) is handled correctly
                errno = 0;
                unsigned long index = strtoul(token, NULL, 10);
                if (errno == ERANGE || index > 0xFFFFFF) {
                    curr = cjsonx_make_null_handle();
                } else {
                    curr = cjsonx_get_index(curr, (size_t)index);
                }
            }
        } else {
            curr = cjsonx_make_null_handle();
        }

        if (needs_free) {
            if (root.doc->alloc.free_fn) {
                root.doc->alloc.free_fn(token, root.doc->alloc.user_data);
            } else {
                free(token);
            }
        }
        if (!curr.doc) return cjsonx_make_null_handle();

        p += token_len;
    }

    return curr;
}

const char* cjsonx_str(cjsonx_val_t handle) {
    if (!handle.doc) return NULL;
    cjsonx_node_t* n = &handle.doc->nodes[handle.node_idx];
    if (cjsonx_node_type(n) == CJSONX_STRING) return n->val.str;
    return NULL;
}

size_t cjsonx_str_len(cjsonx_val_t handle) {
    if (!handle.doc) return 0;
    cjsonx_node_t* n = &handle.doc->nodes[handle.node_idx];
    if (cjsonx_node_type(n) == CJSONX_STRING) return cjsonx_node_length(n);
    return 0;
}

double cjsonx_num(cjsonx_val_t handle) {
    if (!handle.doc) return 0.0;
    cjsonx_node_t* n = &handle.doc->nodes[handle.node_idx];
    if (cjsonx_node_type(n) == CJSONX_NUMBER) return n->val.f64;
    return 0.0;
}

int64_t cjsonx_int(cjsonx_val_t handle) {
    if (!handle.doc) return 0;
    cjsonx_node_t* n = &handle.doc->nodes[handle.node_idx];
    if (cjsonx_node_type(n) == CJSONX_NUMBER) return (int64_t)n->val.f64;
    return 0;
}

bool cjsonx_bool(cjsonx_val_t handle) {
    if (!handle.doc) return false;
    cjsonx_node_t* n = &handle.doc->nodes[handle.node_idx];
    if (cjsonx_node_type(n) == CJSONX_BOOL) return n->val.b;
    return false;
}

bool cjsonx_is_null(cjsonx_val_t handle) {
    /*
     * note: this returns true both when the handle is "not found" (doc == NULL)
     * and when the json value is actually null. if you need to distinguish between
     * these cases, check `handle.doc != NULL` first before calling this function.
     */
    if (!handle.doc) return true;
    cjsonx_node_t* n = &handle.doc->nodes[handle.node_idx];
    return cjsonx_node_type(n) == CJSONX_NULL;
}

cjsonx_type_t cjsonx_get_type(cjsonx_val_t handle) {
    if (!handle.doc) return CJSONX_NULL;
    cjsonx_node_t* n = &handle.doc->nodes[handle.node_idx];
    return cjsonx_node_type(n);
}

size_t cjsonx_size(cjsonx_val_t handle) {
    if (!handle.doc) return 0;
    cjsonx_node_t* n = &handle.doc->nodes[handle.node_idx];
    cjsonx_type_t t = cjsonx_node_type(n);
    if (t == CJSONX_ARRAY || t == CJSONX_OBJECT) return cjsonx_node_length(n);
    return 0;
}

cjsonx_iter_t cjsonx_iter_init(cjsonx_val_t handle) {
    cjsonx_iter_t iter = {NULL, {NULL, 0}, {NULL, 0}, 0, 0, false, false};
    if (!handle.doc) return iter;
    cjsonx_node_t* n = &handle.doc->nodes[handle.node_idx];
    cjsonx_type_t t = cjsonx_node_type(n);
    if (t != CJSONX_OBJECT && t != CJSONX_ARRAY) return iter;

    iter.doc = handle.doc;
    iter.is_object = (t == CJSONX_OBJECT);
    iter.next_idx = n->val.first_child;
    iter.end_idx = cjsonx_node_length(n);  // repurpose end_idx to count_remaining
    return iter;
}

bool cjsonx_iter_next(cjsonx_iter_t* iter) {
    if (!iter || !iter->doc || iter->end_idx == 0) {
        if (iter) iter->valid = false;
        return false;
    }

    iter->end_idx--;  // decrement count_remaining

    if (iter->is_object) {
        cjsonx_val_t key = {iter->doc, iter->next_idx};
        cjsonx_node_t* key_node = &iter->doc->nodes[iter->next_idx];
        uint32_t val_idx = key_node->next_sibling;
        cjsonx_val_t val = {iter->doc, val_idx};

        iter->key = key;
        iter->value = val;
        iter->next_idx = iter->doc->nodes[val_idx].next_sibling;
    } else {
        cjsonx_val_t val = {iter->doc, iter->next_idx};
        iter->key = cjsonx_make_null_handle();
        iter->value = val;
        iter->next_idx = iter->doc->nodes[iter->next_idx].next_sibling;
    }
    iter->valid = true;
    return true;
}

cjsonx_doc_t* cjsonx_parse_ex(const char* json, size_t length, cjsonx_allocator_t* alloc) {
    cjsonx_doc_t* doc;
    if (alloc && alloc->malloc_fn) {
        doc = (cjsonx_doc_t*)alloc->malloc_fn(sizeof(cjsonx_doc_t), alloc->user_data);
        if (doc) memset(doc, 0, sizeof(cjsonx_doc_t));
    } else {
        doc = (cjsonx_doc_t*)calloc(1, sizeof(cjsonx_doc_t));
    }
    if (!doc) return NULL;

    if (alloc) doc->alloc = *alloc;

    doc->original_json = json;
    doc->json_len = length;

    cjsonx_arena_node_t* init_node;
    if (doc->alloc.malloc_fn) {
        init_node = (cjsonx_arena_node_t*)doc->alloc.malloc_fn(
            sizeof(cjsonx_arena_node_t) + CJSONX_ARENA_CHUNK_SIZE, doc->alloc.user_data);
    } else {
        init_node =
            (cjsonx_arena_node_t*)malloc(sizeof(cjsonx_arena_node_t) + CJSONX_ARENA_CHUNK_SIZE);
    }
    if (!init_node) {
        cjsonx_doc_free(doc);
        return NULL;
    }
    init_node->next = NULL;
    doc->head = init_node;
    doc->current_chunk = (uint8_t*)(init_node + 1);
    doc->chunk_size = CJSONX_ARENA_CHUNK_SIZE;
    doc->chunk_used = 0;

    cjsonx_tape_t tape;
    size_t initial_cap = length / 8;
    if (initial_cap < 256) initial_cap = 256;
    if (!cjsonx_tape_init(&tape, initial_cap, &doc->alloc)) {
        doc->is_valid = false;
        doc->error = CJSONX_ERROR_OOM;
        return doc;
    }

    if (!cjsonx_stage1_build_tape(json, length, &tape)) {
        doc->is_valid = false;
        doc->error_offset = length;
        if (doc->error == CJSONX_SUCCESS) {
            bool is_empty = true;
            for (size_t idx = 0; idx < length; idx++) {
                char ch = json[idx];
                if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
                    is_empty = false;
                    break;
                }
            }
            doc->error = is_empty ? CJSONX_ERROR_EMPTY_INPUT : CJSONX_ERROR_UNEXPECTED_TOKEN;
        }
        cjsonx_tape_free(&tape);
        return doc;
    }

    if (!cjsonx_stage2_build(doc, json, &tape)) {
        // error already set inside cjsonx_stage2_build
    }
    cjsonx_tape_free(&tape);
    return doc;
}

cjsonx_doc_t* cjsonx_parse(const char* json, size_t length) {
    return cjsonx_parse_ex(json, length, NULL);
}

cjsonx_doc_t* cjsonx_parse_copy_ex(const char* json, size_t length, cjsonx_allocator_t* alloc) {
    if (!json) return NULL;
    char* copy;
    if (alloc && alloc->malloc_fn) {
        copy = (char*)alloc->malloc_fn(length + 1, alloc->user_data);
    } else {
        copy = (char*)malloc(length + 1);
    }
    if (!copy) return NULL;
    memcpy(copy, json, length);
    copy[length] = '\0';

    cjsonx_doc_t* doc = cjsonx_parse_ex(copy, length, alloc);
    if (doc) {
        doc->owned_json = copy;  // owns copy to prevent use after free
        doc->original_json = copy;
    } else {
        if (alloc && alloc->free_fn)
            alloc->free_fn(copy, alloc->user_data);
        else
            free(copy);
    }
    return doc;
}

cjsonx_doc_t* cjsonx_parse_copy(const char* json, size_t length) {
    return cjsonx_parse_copy_ex(json, length, NULL);
}

cjsonx_doc_t* cjsonx_parse_with_buffer(const char* json, size_t length, void* buffer,
                                       size_t buffer_size) {
    uintptr_t ptr = (uintptr_t)buffer;
    size_t offset = ptr % 8 ? 8 - (ptr % 8) : 0;
    if (buffer_size < offset + sizeof(cjsonx_doc_t)) return NULL;

    cjsonx_doc_t* doc = (cjsonx_doc_t*)(ptr + offset);
    memset(doc, 0, sizeof(cjsonx_doc_t));
    doc->is_static = true;
    doc->original_json = json;
    doc->json_len = length;

    offset += sizeof(cjsonx_doc_t);
    size_t tape_max_capacity = (buffer_size - offset) / sizeof(uint32_t);
    if (tape_max_capacity == 0) {
        doc->is_valid = false;
        doc->error = CJSONX_ERROR_OOM;
        return doc;
    }

    cjsonx_tape_t tape;
    cjsonx_tape_init_static(&tape, (uint32_t*)(ptr + offset), tape_max_capacity);

    if (!cjsonx_stage1_build_tape(json, length, &tape)) {
        doc->is_valid = false;
        doc->error_offset = length;
        if (tape.count >= tape.capacity) {
            doc->error = CJSONX_ERROR_OOM;
        } else {
            bool is_empty = true;
            for (size_t idx = 0; idx < length; idx++) {
                char ch = json[idx];
                if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
                    is_empty = false;
                    break;
                }
            }
            doc->error = is_empty ? CJSONX_ERROR_EMPTY_INPUT : CJSONX_ERROR_UNEXPECTED_TOKEN;
        }
        return doc;
    }

    offset += tape.count * sizeof(uint32_t);
    // explicit size_t mask: ~7 is signed int which sign-extends on 64-bit
    offset = (offset + 7u) & ~(size_t)7;

    /* use tape.count+1 as the node capacity upper bound.
     * tape.count/2+1 is the typical estimate, but cjsonx_next_token checks
     * node_idx >= capacity for EVERY tape entry (including commas and closers),
     * so a tight estimate causes spurious cjsonx_grow_nodes calls which fail
     * for static docs (is_static prevents realloc). tape.count+1 guarantees
     * node_idx never reaches capacity before the parse finishes.
     */
    size_t alloc_count = tape.count + 1;
    size_t nodes_size = alloc_count * sizeof(cjsonx_node_t);
    if (offset + nodes_size > buffer_size) {
        doc->is_valid = false;
        doc->error = CJSONX_ERROR_OOM;
        return doc;
    }

    doc->nodes = (cjsonx_node_t*)(ptr + offset);
    doc->node_capacity = alloc_count;
    offset += nodes_size;

    if (offset + sizeof(cjsonx_arena_node_t) <= buffer_size) {
        cjsonx_arena_node_t* init_node = (cjsonx_arena_node_t*)(ptr + offset);
        init_node->next = NULL;
        doc->head = init_node;
        doc->current_chunk = (uint8_t*)(init_node + 1);
        doc->chunk_size = buffer_size - offset - sizeof(cjsonx_arena_node_t);
        doc->chunk_used = 0;
    }

    cjsonx_stage2_build(doc, json, &tape);
    // note: error is stored in doc->error if build fails. return doc regardless
    // so caller can inspect doc->is_valid and doc->error.
    return doc;
}

#endif  // cjsonx_stage2_h
#endif
#endif

#endif  // cjsonx_h
