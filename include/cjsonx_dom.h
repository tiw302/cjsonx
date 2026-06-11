/**
 * @file cjsonx_dom.h
 * @brief dom node types, document structure, and accessor declarations
 *
 * @note architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
#ifndef CJSONX_DOM_H
#define CJSONX_DOM_H

/*==============================================================================
 * mark: - dom (document object model)
 *============================================================================*/


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cjsonx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* (*malloc_fn)(size_t size, void* user_data);
    void* (*realloc_fn)(void* ptr, size_t size, void* user_data);
    void  (*free_fn)(void* ptr, void* user_data);
    void* user_data;
} cjsonx_allocator_t;

typedef enum {
    CJSONX_NULL,
    CJSONX_BOOL,
    CJSONX_NUMBER,
    CJSONX_STRING,
    CJSONX_ARRAY,
    CJSONX_OBJECT
} cjsonx_type_t;

// 16-byte flat node, packed for cache efficiency.
// max length/count supported per node is 16,777,215 (24-bit).
typedef struct {
    uint32_t type_and_length;  // 8-bit type | 24-bit length (max 16,777,215)
    uint32_t next_sibling;     // next sibling index for fast skipping
    union {
        double f64;            // number val
        const char* str;       // zero-copy string ptr
        bool b;                // bool val
        uint32_t first_child;  // first child index for object/array
    } val;
} cjsonx_node_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(cjsonx_node_t) == 16, "cjsonx_node_t must be 16 bytes for cache alignment");
#endif

static cjsonx_always_inline cjsonx_type_t cjsonx_node_type(const cjsonx_node_t* n) {
    return (cjsonx_type_t)(n->type_and_length & 0xFF);
}

static cjsonx_always_inline uint32_t cjsonx_node_length(const cjsonx_node_t* n) {
    return (n->type_and_length >> 8);
}

static cjsonx_always_inline void cjsonx_node_set_type_len(cjsonx_node_t* __restrict n, cjsonx_type_t type, uint32_t length) {
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
    cjsonx_val_t root;          // root handle
    cjsonx_node_t* nodes;       // flat dom array
    size_t node_count;          // total nodes
    size_t node_capacity;       // allocated capacity

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

    cjsonx_allocator_t alloc;   // custom allocator hooks
    bool is_static;             // true if user provided static memory
    char* owned_json;           // owned copy of json buffer (set by cjsonx_read_file)
};

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
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_parse_with_buffer(const char* json, size_t length, void* buffer, size_t buffer_size);

// lifecycle
CJSONX_API cjsonx_doc_t* cjsonx_doc_new(void);
CJSONX_API cjsonx_doc_t* cjsonx_doc_new_ex(cjsonx_allocator_t* alloc);
CJSONX_API void cjsonx_doc_free(cjsonx_doc_t* doc);

// container lookup
// o(n): linear key scan.
CJSONX_API cjsonx_val_t cjsonx_get(cjsonx_val_t obj, const char* key);
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
