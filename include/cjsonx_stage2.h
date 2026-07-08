// updated 2026-06-13
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


#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <errno.h>

#include "cjsonx_string.h"

#include "cjsonx_fastfloat.h"

typedef enum {
    CJSONX_S_VAL     = 1,
    CJSONX_S_KEY     = 2,
    CJSONX_S_COL     = 4,
    CJSONX_S_COM     = 8,
    CJSONX_S_OBJ_END = 16,
    CJSONX_S_ARR_END = 32,
    CJSONX_S_EOF     = 64
} cjsonx_state_mask_t;

static cjsonx_always_inline uint8_t cjsonx_get_next_mask(uint32_t parent_depth, const uint8_t* __restrict parent_type_stack) {
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
static cjsonx_always_inline bool cjsonx_is_valid_number(const char* __restrict str, const char* __restrict limit) {
    if (str >= limit) return false;
    if (*str == '-') str++;
    if (str >= limit) return false;
    
    if (*str == '0') {
        str++;
        if (str < limit && *str >= '0' && *str <= '9') return false; // no leading zero allowed e.g. 01
    } else if (*str >= '1' && *str <= '9') {
        str++;
        while (str < limit && *str >= '0' && *str <= '9') str++;
    } else {
        return false;
    }
    
    if (str < limit && *str == '.') {
        str++;
        if (str >= limit || *str < '0' || *str > '9') return false; // require digit after dot
        while (str < limit && *str >= '0' && *str <= '9') str++;
    }
    
    if (str < limit && (*str == 'e' || *str == 'E')) {
        str++;
        if (str < limit && (*str == '+' || *str == '-')) str++;
        if (str >= limit || *str < '0' || *str > '9') return false; // require digit after exp
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
    if (CJSONX_UNLIKELY(new_cap >= UINT32_MAX - 1 || new_cap > (size_t)-1 / sizeof(cjsonx_node_t))) return false; // check for index overflow
    cjsonx_node_t* new_nodes = (cjsonx_node_t*)cjsonx_realloc(&doc->alloc, doc->nodes, doc->node_capacity * sizeof(cjsonx_node_t), new_cap * sizeof(cjsonx_node_t));
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
#   include <xlocale.h>
    locale_t loc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    if (loc != (locale_t)0) {
        double val = strtod_l(buf, endptr, loc);
        freelocale(loc);
        return val;
    }
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    locale_t loc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    if (loc != (locale_t)0) {
        double val = strtod_l(buf, endptr, loc);
        freelocale(loc);
        return val;
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
            *dot = original_dot; // restore original char
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
    #define CJSONX_PARSE_FAIL_EXPECTED(allowed) do { \
        if ((allowed) & CJSONX_S_COL) doc->error = CJSONX_ERROR_MISSING_COLON; \
        else if ((allowed) & CJSONX_S_COM) doc->error = CJSONX_ERROR_MISSING_COMMA; \
        else doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN; \
        goto fail; \
    } while (0)

    // skip allocation if nodes were already pre-allocated (e.g. static buffer)
    if (!doc->nodes) {
        size_t alloc_count = tape->count / 2 + 1;
        if (CJSONX_UNLIKELY(alloc_count >= UINT32_MAX - 1 || alloc_count > (size_t)-1 / sizeof(cjsonx_node_t))) {
            doc->error = CJSONX_ERROR_OOM;
            return false;
        }
        if (doc->alloc.malloc_fn) {
            doc->nodes = (cjsonx_node_t*)doc->alloc.malloc_fn(alloc_count * sizeof(cjsonx_node_t), doc->alloc.user_data);
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
     * instead of consuming stack frames via recursive function calls (which risks stack overflows on deeply nested json),
     * we use a single, fast loop with a lightweight state machine.
     * - parent_stack: holds the indices of active parent nodes (objects and arrays).
     * - parent_type_stack: stores whether the parent is an object or array to enforce syntax rules.
     * - count_stack: tracks the child count for the current container level.
     * - parent_depth: tracks the current nesting depth, bounded by cjsonx_max_depth.
     */
    uint32_t parent_stack[CJSONX_MAX_DEPTH];
    uint8_t parent_type_stack[CJSONX_MAX_DEPTH];
    uint32_t count_stack[CJSONX_MAX_DEPTH]; // l1 hot-stack for element counts
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

    #define CJSONX_ENSURE_CAPACITY() do { \
        if (CJSONX_UNLIKELY(node_idx >= doc->node_capacity)) { \
            if (CJSONX_UNLIKELY(!cjsonx_grow_nodes(doc, node_idx + 1))) { \
                doc->error = CJSONX_ERROR_OOM; \
                goto fail; \
            } \
        } \
        node = &doc->nodes[node_idx]; \
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
        [0 ... 255] = &&l_default,
        ['"'] = &&l_string,
        ['{'] = &&l_obj_arr_start,
        ['['] = &&l_obj_arr_start,
        ['}'] = &&l_obj_arr_end,
        [']'] = &&l_obj_arr_end,
        [':'] = &&l_colon,
        [','] = &&l_comma,
        ['t'] = &&l_true,
        ['f'] = &&l_false,
        ['n'] = &&l_null
    };
#if defined(__clang__)
    #pragma clang diagnostic pop
#elif defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif


    #define CJSONX_NEXT_TOKEN() do { \
        if (CJSONX_UNLIKELY(tape_idx >= tape->count)) goto end_loop; \
        err_tape_idx = tape_idx; \
        pos = tape->indices[tape_idx]; \
        c = json[pos]; \
        goto *dispatch_table[(uint8_t)c]; \
    } while (0)
    
    #define CJSONX_CASE(lbl, ch) l_##lbl:
    #define CJSONX_CASE_MULTI(lbl, ch1, ch2) l_##lbl:
    #define CJSONX_CASE_DEFAULT() l_default:
    
    CJSONX_NEXT_TOKEN();
#else
    #define CJSONX_NEXT_TOKEN() break
    #define CJSONX_CASE(lbl, ch) case ch:
    #define CJSONX_CASE_MULTI(lbl, ch1, ch2) case ch1: case ch2:
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
                if (allowed_mask & CJSONX_S_KEY) allowed_mask = CJSONX_S_COL;
                else allowed_mask = cjsonx_get_next_mask(parent_depth, parent_type_stack);
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
                cjsonx_node_set_type_len(node, t, 0); // temp length
                node->val.first_child = (uint32_t)(node_idx + 1); // set first child
                if (parent_depth > 0) count_stack[parent_depth - 1]++;
                parent_stack[parent_depth] = (uint32_t)node_idx;
                parent_type_stack[parent_depth] = t;
                count_stack[parent_depth] = 0;
                parent_depth++;
                node_idx++;
                tape_idx++;
                allowed_mask = (c == '{') ? (CJSONX_S_KEY | CJSONX_S_OBJ_END) : (CJSONX_S_VAL | CJSONX_S_ARR_END);
                CJSONX_NEXT_TOKEN();
            }
            CJSONX_CASE_MULTI(obj_arr_end, '}', ']') {
                if (CJSONX_UNLIKELY(c == '}' && !(allowed_mask & CJSONX_S_OBJ_END))) {
                    if (allowed_mask & CJSONX_S_KEY) doc->error = CJSONX_ERROR_TRAILING_COMMA;
                    else doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN;
                    goto fail;
                }
                if (CJSONX_UNLIKELY(c == ']' && !(allowed_mask & CJSONX_S_ARR_END))) {
                    if (allowed_mask & CJSONX_S_VAL) doc->error = CJSONX_ERROR_TRAILING_COMMA;
                    else doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN;
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
                if (CJSONX_UNLIKELY((c == '}' && ptype != CJSONX_OBJECT) || (c == ']' && ptype != CJSONX_ARRAY))) {
                    doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN;
                    goto fail;
                }
                pnode->next_sibling = (uint32_t)node_idx;
                if (ptype == CJSONX_OBJECT) count /= 2;
                if (CJSONX_UNLIKELY(count > 0xFFFFFF)) {
                    doc->error = CJSONX_ERROR_TOO_LARGE; // container exceeds 24b limit
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
                const char* limit = (tape_idx + 1 < tape->count) ? (json + tape->indices[tape_idx + 1]) : (json + doc->json_len);
                if (CJSONX_UNLIKELY(limit - (json + pos) < 4 || memcmp(json + pos, "true", 4) != 0 || !cjsonx_is_all_whitespace(json + pos + 4, limit))) {
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
                const char* limit = (tape_idx + 1 < tape->count) ? (json + tape->indices[tape_idx + 1]) : (json + doc->json_len);
                if (CJSONX_UNLIKELY(limit - (json + pos) < 5 || memcmp(json + pos, "false", 5) != 0 || !cjsonx_is_all_whitespace(json + pos + 5, limit))) {
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
                const char* limit = (tape_idx + 1 < tape->count) ? (json + tape->indices[tape_idx + 1]) : (json + doc->json_len);
                if (CJSONX_UNLIKELY(limit - (json + pos) < 4 || memcmp(json + pos, "null", 4) != 0 || !cjsonx_is_all_whitespace(json + pos + 4, limit))) {
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
            CJSONX_CASE_DEFAULT() { // number
                CJSONX_ENSURE_CAPACITY();
                if (CJSONX_UNLIKELY(!(allowed_mask & CJSONX_S_VAL))) {
                    CJSONX_PARSE_FAIL_EXPECTED(allowed_mask);
                }
                const char* limit = (tape_idx + 1 < tape->count) ? (json + tape->indices[tape_idx + 1]) : (json + doc->json_len);
                if (CJSONX_UNLIKELY(!cjsonx_is_valid_number(json + pos, limit))) {
                    doc->error = CJSONX_ERROR_INVALID_NUMBER;
                    goto fail;
                }
                cjsonx_node_set_type_len(node, CJSONX_NUMBER, 0);
                
                double val = 0;
                const char* end = NULL;
                if (CJSONX_UNLIKELY(!cjsonx_parse_fast_float(json + pos, limit, &end, &val))) {
                    const char* num_end = json + pos;
                    while (num_end < limit && (
                        (*num_end >= '0' && *num_end <= '9') ||
                        *num_end == '.' ||
                        *num_end == 'e' || *num_end == 'E' ||
                        *num_end == '+' || *num_end == '-'
                    )) {
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
                     * if the raw float string exceeds 511 chars (due to trailing zeros or extremely long decimals),
                     * we allocate a dynamic buffer using the allocator hooks to prevent heap/stack overflows,
                     * and safely free it after strtod parses the double.
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
                        if (doc->alloc.free_fn) doc->alloc.free_fn(buf, doc->alloc.user_data);
                        else free(buf);
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
        if (allowed_mask & CJSONX_S_COL) doc->error = CJSONX_ERROR_MISSING_COLON;
        else if (allowed_mask & CJSONX_S_COM) doc->error = CJSONX_ERROR_MISSING_COMMA;
        else doc->error = CJSONX_ERROR_UNEXPECTED_TOKEN;
        goto fail;
    }
    
    doc->node_count = node_idx;
    doc->is_valid = true;
    doc->root.doc = doc;
    doc->root.node_idx = 0;
    return true;

fail:
    doc->is_valid = false;
    if (doc->error == CJSONX_SUCCESS) {
        doc->error = (parent_depth >= CJSONX_MAX_DEPTH) ? CJSONX_ERROR_MAX_DEPTH : CJSONX_ERROR_UNEXPECTED_TOKEN;
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
        init_node = (cjsonx_arena_node_t*)doc->alloc.malloc_fn(sizeof(cjsonx_arena_node_t) + CJSONX_ARENA_CHUNK_SIZE, doc->alloc.user_data);
    } else {
        init_node = (cjsonx_arena_node_t*)malloc(sizeof(cjsonx_arena_node_t) + CJSONX_ARENA_CHUNK_SIZE);
    }
    if (!init_node) {
        if (doc->alloc.free_fn) doc->alloc.free_fn(doc, doc->alloc.user_data);
        else free(doc);
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
        nodes = (cjsonx_node_t*)doc->alloc.malloc_fn(16 * sizeof(cjsonx_node_t), doc->alloc.user_data);
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
        if (alloc->free_fn) alloc->free_fn(doc->nodes, alloc->user_data);
        else free(doc->nodes);
    }
    // free owned json buffer (set by cjsonx_read_file)
    if (doc->owned_json) {
        if (alloc->free_fn) alloc->free_fn(doc->owned_json, alloc->user_data);
        else free(doc->owned_json);
    }
    if (!doc->is_static) {
        cjsonx_arena_node_t* current = doc->head;
        while (current) {
            cjsonx_arena_node_t* next = current->next;
            if (alloc->free_fn) alloc->free_fn(current, alloc->user_data);
            else free(current);
            current = next;
        }
        if (alloc->free_fn) alloc->free_fn(doc, alloc->user_data);
        else free(doc);
    }
}

// dom value accessors — get by key, index, and json pointer
cjsonx_val_t cjsonx_get_len(cjsonx_val_t obj_handle, const char* key, size_t key_len) {
    if (!obj_handle.doc) return cjsonx_make_null_handle();
    if (!key) { key = ""; key_len = 0; }
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
        p++; // skip '/'
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
                if (p[i+1] == '1') { token[unescaped_len++] = '/'; i++; continue; }
                if (p[i+1] == '0') { token[unescaped_len++] = '~'; i++; continue; }
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
    iter.end_idx = cjsonx_node_length(n); // repurpose end_idx to count_remaining
    return iter;
}

bool cjsonx_iter_next(cjsonx_iter_t* iter) {
    if (!iter || !iter->doc || iter->end_idx == 0) {
        if (iter) iter->valid = false;
        return false;
    }
    
    iter->end_idx--; // decrement count_remaining
    
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
        init_node = (cjsonx_arena_node_t*)doc->alloc.malloc_fn(sizeof(cjsonx_arena_node_t) + CJSONX_ARENA_CHUNK_SIZE, doc->alloc.user_data);
    } else {
        init_node = (cjsonx_arena_node_t*)malloc(sizeof(cjsonx_arena_node_t) + CJSONX_ARENA_CHUNK_SIZE);
    }
    if (!init_node) { cjsonx_doc_free(doc); return NULL; }
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
        doc->owned_json = copy; // owns copy to prevent use after free
        doc->original_json = copy;
    } else {
        if (alloc && alloc->free_fn) alloc->free_fn(copy, alloc->user_data);
        else free(copy);
    }
    return doc;
}

cjsonx_doc_t* cjsonx_parse_copy(const char* json, size_t length) {
    return cjsonx_parse_copy_ex(json, length, NULL);
}

cjsonx_doc_t* cjsonx_parse_with_buffer(const char* json, size_t length, void* buffer, size_t buffer_size) {
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
    
    // use tape.count+1 as the node capacity upper bound.
    // tape.count/2+1 is the typical estimate, but cjsonx_next_token checks
    // node_idx >= capacity for EVERY tape entry (including commas and closers),
    // so a tight estimate causes spurious cjsonx_grow_nodes calls which fail
    // for static docs (is_static prevents realloc). tape.count+1 guarantees
    // node_idx never reaches capacity before the parse finishes.
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

#endif // cjsonx_stage2_h
