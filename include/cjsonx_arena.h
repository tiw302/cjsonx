/**
 * @file cjsonx_arena.h
 * @brief arena allocator for document-scoped memory
 *
 * @note architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
#ifndef CJSONX_ARENA_H
#define CJSONX_ARENA_H

/*==============================================================================
 * mark: - memory arena
 *============================================================================*/


#include <stdlib.h>
#include <stdint.h>
#include "cjsonx_config.h"
#include "cjsonx_dom.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * arena allocator: allocate `size` bytes from the document's memory pool.
 *
 * returns an 8-byte aligned pointer, or null on oom.
 * all allocations are freed together when cjsonx_doc_free() is called.
 */
static cjsonx_always_inline void* cjsonx_arena_alloc(cjsonx_doc_t* __restrict doc, size_t size) {
    if (CJSONX_UNLIKELY(size > (size_t) - 8)) return NULL;
    // round up to 8-byte alignment for safe struct access
    size = (size + 7) & ~7;

    // current chunk exhausted or first allocation — grow arena with a new chunk
    if (CJSONX_UNLIKELY(!doc->current_chunk || doc->chunk_used + size > doc->chunk_size)) {
        if (CJSONX_UNLIKELY(doc->is_static)) return NULL; // static buffers cannot be expanded
        size_t min_needed = size + sizeof(cjsonx_arena_node_t);
        if (CJSONX_UNLIKELY(min_needed < size)) return NULL; // overflow check

        size_t new_chunk_size = doc->chunk_size ? doc->chunk_size * 2 : CJSONX_ARENA_CHUNK_SIZE;
        if (new_chunk_size < min_needed) {
            new_chunk_size = min_needed + CJSONX_ARENA_CHUNK_SIZE;
            if (CJSONX_UNLIKELY(new_chunk_size < min_needed)) {
                new_chunk_size = min_needed; // handle overflow by allocating exact amount
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

#endif // cjsonx_arena_h
