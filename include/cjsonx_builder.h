// updated 2026-06-13
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


#include "cjsonx_dom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// allocation internal helper (expands flat arena)
static inline uint32_t cjsonx_builder_alloc_node(cjsonx_doc_t* doc) {
    if (!doc || doc->is_static) return UINT32_MAX; // static docs cannot be mutated
    if (CJSONX_UNLIKELY(doc->node_count >= UINT32_MAX - 1)) return UINT32_MAX; // limit count to prevent uint32 index overflow
    if (doc->node_count >= doc->node_capacity) {
        size_t new_cap = doc->node_capacity == 0 ? 128 : doc->node_capacity * 2;
        
        // dev note: integer overflow check is spot on. prevents oom vulnerabilities.
        if (CJSONX_UNLIKELY(new_cap < doc->node_capacity || new_cap > (size_t)-1 / sizeof(cjsonx_node_t))) return UINT32_MAX;
        cjsonx_node_t* new_nodes = (cjsonx_node_t*)cjsonx_realloc(&doc->alloc, doc->nodes, doc->node_capacity * sizeof(cjsonx_node_t), new_cap * sizeof(cjsonx_node_t));
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
    doc->nodes[idx].next_sibling = idx + 1; // point to nowhere for now
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
    if (!str) str = ""; // treat null as empty string, same as cjsonx_get's behavior
    uint32_t idx = cjsonx_builder_alloc_node(doc);
    if (idx == UINT32_MAX) return cjsonx_make_null_handle();
    size_t len = strlen(str);
    cjsonx_node_set_type_len(&doc->nodes[idx], CJSONX_STRING, len);
    doc->nodes[idx].next_sibling = idx + 1;
    
    // duplicate string into arena
    char* s = (char*)cjsonx_arena_alloc(doc, len + 1);
    if (!s) {
        doc->node_count--; // rollback allocated node slot on failure
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
static inline bool cjsonx_object_set_len(cjsonx_val_t obj_handle, const char* key, size_t key_len, cjsonx_val_t val_handle) {
    if (!obj_handle.doc || !val_handle.doc || obj_handle.doc != val_handle.doc) return false;
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
        if (cjsonx_node_length(k_node) == key_len && memcmp(k_node->val.str, key, key_len) == 0) {
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
        obj_handle.doc->node_count--; // rollback allocated node slot on failure
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

static inline bool cjsonx_object_set(cjsonx_val_t obj_handle, const char* key, cjsonx_val_t val_handle) {
    return cjsonx_object_set_len(obj_handle, key, key ? strlen(key) : 0, val_handle);
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

static inline bool cjsonx_object_remove_len(cjsonx_val_t obj_handle, const char* key, size_t key_len) {
    if (!obj_handle.doc) return false;
    cjsonx_node_t* obj = &obj_handle.doc->nodes[obj_handle.node_idx];
    if (cjsonx_node_type(obj) != CJSONX_OBJECT) return false;
    
    uint32_t curr = obj->val.first_child;
    uint32_t prev_val_idx = UINT32_MAX; // the value node of the previous pair, which points to current key
    size_t len = cjsonx_node_length(obj);
    
    for (size_t i = 0; i < len; i++) {
        cjsonx_node_t* k_node = &obj_handle.doc->nodes[curr];
        uint32_t val_idx = k_node->next_sibling;
        cjsonx_node_t* v_node = &obj_handle.doc->nodes[val_idx];
        
        if (cjsonx_node_length(k_node) == key_len && memcmp(k_node->val.str, key, key_len) == 0) {
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
CJSONX_API CJSONX_NODISCARD cjsonx_doc_t* cjsonx_read_file_ex(const char* path, cjsonx_allocator_t* alloc);
CJSONX_API bool cjsonx_write_file(const char* path, cjsonx_doc_t* doc);
CJSONX_API bool cjsonx_write_file_format(const char* path, cjsonx_doc_t* doc, int indent_spaces);
CJSONX_API cjsonx_val_t cjsonx_clone_val(cjsonx_doc_t* dest_doc, cjsonx_val_t src_val);
CJSONX_API cjsonx_val_t cjsonx_merge_patch(cjsonx_val_t target, cjsonx_val_t patch);

#ifdef CJSONX_IMPLEMENTATION

#define CJSONX_FRACMASK  0x000FFFFFFFFFFFFFU
#define CJSONX_EXPMASK   0x7FF0000000000000U
#define CJSONX_HIDDENBIT 0x0010000000000000U
#define CJSONX_SIGNMASK  0x8000000000000000U
#define CJSONX_EXPBIAS   (1023 + 52)

#define CJSONX_ABS(n) ((n) < 0 ? -(n) : (n))
#define CJSONX_MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    uint64_t frac;
    int exp;
} cjsonx_fp_t;

// powers of ten table for grisu2
static const cjsonx_fp_t cjsonx_powers_ten[] = {
    { 18054884314459144840U, -1220 }, { 13451937075301367670U, -1193 },
    { 10022474136428063862U, -1166 }, { 14934650266808366570U, -1140 },
    { 11127181549972568877U, -1113 }, { 16580792590934885855U, -1087 },
    { 12353653155963782858U, -1060 }, { 18408377700990114895U, -1034 },
    { 13715310171984221708U, -1007 }, { 10218702384817765436U, -980 },
    { 15227053142812498563U, -954 },  { 11345038669416679861U, -927 },
    { 16905424996341287883U, -901 },  { 12595523146049147757U, -874 },
    { 9384396036005875287U,  -847 },  { 13983839803942852151U, -821 },
    { 10418772551374772303U, -794 },  { 15525180923007089351U, -768 },
    { 11567161174868858868U, -741 },  { 17236413322193710309U, -715 },
    { 12842128665889583758U, -688 },  { 9568131466127621947U,  -661 },
    { 14257626930069360058U, -635 },  { 10622759856335341974U, -608 },
    { 15829145694278690180U, -582 },  { 11793632577567316726U, -555 },
    { 17573882009934360870U, -529 },  { 13093562431584567480U, -502 },
    { 9755464219737475723U,  -475 },  { 14536774485912137811U, -449 },
    { 10830740992659433045U, -422 },  { 16139061738043178685U, -396 },
    { 12024538023802026127U, -369 },  { 17917957937422433684U, -343 },
    { 13349918974505688015U, -316 },  { 9946464728195732843U,  -289 },
    { 14821387422376473014U, -263 },  { 11042794154864902060U, -236 },
    { 16455045573212060422U, -210 },  { 12259964326927110867U, -183 },
    { 18268770466636286478U, -157 },  { 13611294676837538539U, -130 },
    { 10141204801825835212U, -103 },  { 15111572745182864684U, -77 },
    { 11258999068426240000U, -50 },   { 16777216000000000000U, -24 },
    { 12500000000000000000U,   3 },   { 9313225746154785156U,   30 },
    { 13877787807814456755U,  56 },   { 10339757656912845936U,  83 },
    { 15407439555097886824U, 109 },   { 11479437019748901445U, 136 },
    { 17105694144590052135U, 162 },   { 12744735289059618216U, 189 },
    { 9495567745759798747U,  216 },   { 14149498560666738074U, 242 },
    { 10542197943230523224U, 269 },   { 15709099088952724970U, 295 },
    { 11704190886730495818U, 322 },   { 17440603504673385349U, 348 },
    { 12994262207056124023U, 375 },   { 9681479787123295682U,  402 },
    { 14426529090290212157U, 428 },   { 10748601772107342003U, 455 },
    { 16016664761464807395U, 481 },   { 11933345169920330789U, 508 },
    { 17782069995880619868U, 534 },   { 13248674568444952270U, 561 },
    { 9871031767461413346U,  588 },   { 14708983551653345445U, 614 },
    { 10959046745042015199U, 641 },   { 16330252207878254650U, 667 },
    { 12166986024289022870U, 694 },   { 18130221999122236476U, 720 },
    { 13508068024458167312U, 747 },   { 10064294952495520794U, 774 },
    { 14996968138956309548U, 800 },   { 11173611982879273257U, 827 },
    { 16649979327439178909U, 853 },   { 12405201291620119593U, 880 },
    { 9242595204427927429U,  907 },   { 13772540099066387757U, 933 },
    { 10261342003245940623U, 960 },   { 15290591125556738113U, 986 },
    { 11392378155556871081U, 1013 },  { 16975966327722178521U, 1039 },
    { 12648080533535911531U, 1066 }
};

static const uint64_t cjsonx_tens[] = {
    10000000000000000000U, 1000000000000000000U, 100000000000000000U,
    10000000000000000U, 1000000000000000U, 100000000000000U,
    10000000000000U, 1000000000000U, 100000000000U,
    10000000000U, 1000000000U, 100000000U,
    10000000U, 1000000U, 100000U,
    10000U, 1000U, 100U,
    10U, 1U
};

static inline uint64_t cjsonx_get_dbits(double d) {
    union {
        double dbl;
        uint64_t i;
    } dbl_bits = { d };
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
    while ((fp->frac & CJSONX_HIDDENBIT) == 0) {
        fp->frac <<= 1;
        fp->exp--;
    }
    int shift = 64 - 52 - 1;
    fp->frac <<= shift;
    fp->exp -= shift;
}

static void cjsonx_get_normalized_boundaries(cjsonx_fp_t* fp, cjsonx_fp_t* lower, cjsonx_fp_t* upper) {
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
    cjsonx_fp_t fp = {
        ah_bh + (ah_bl >> 32) + (al_bh >> 32) + (tmp >> 32),
        a->exp + b->exp + 64
    };
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

static void cjsonx_round_digit(char* digits, int ndigits, uint64_t delta, uint64_t rem, uint64_t kappa, uint64_t frac) {
    while (rem < frac && delta - rem >= kappa &&
           (rem + kappa < frac || frac - rem > rem + kappa - frac)) {
        digits[ndigits - 1]--;
        rem += kappa;
    }
}

static int cjsonx_generate_digits(cjsonx_fp_t* fp, cjsonx_fp_t* upper, cjsonx_fp_t* lower, char* digits, int* K) {
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
        dest[0] = 'n'; dest[1] = 'u'; dest[2] = 'l'; dest[3] = 'l';
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
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1, // 0-15
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1, // 16-31
    0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, // 32-47 (34 is '"')
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, // 48-63
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, // 64-79
    0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, // 80-95 (92 is '\\')
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};


typedef struct {
    char* buf;
    size_t len;
    size_t cap;
    cjsonx_allocator_t* alloc;
    bool oom;
} cjsonx_strbuf_t;

static cjsonx_always_inline void cjsonx_strbuf_append(cjsonx_strbuf_t* __restrict sb, const char* __restrict str, size_t len) {
    if (CJSONX_UNLIKELY(sb->oom)) return;
    // unsigned wraparound: if sum < either operand, an overflow occurred
    if (CJSONX_UNLIKELY(sb->len + len < sb->len)) {
        sb->oom = true;
        return;
    }
    if (CJSONX_UNLIKELY(sb->len + len >= sb->cap)) {
        size_t new_cap = sb->cap == 0 ? 2048 : sb->cap * 2;
        if (new_cap <= sb->len + len) new_cap = sb->len + len + 1024;
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
    if (CJSONX_UNLIKELY(sb->len + 1 < sb->len)) { // overflow check
        sb->oom = true;
        return;
    }
    if (CJSONX_UNLIKELY(sb->len + 1 >= sb->cap)) {
        size_t new_cap = sb->cap == 0 ? 2048 : sb->cap * 2;
        if (CJSONX_UNLIKELY(new_cap < sb->cap)) new_cap = sb->len + 1; // clamp on overflow
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
                        case '"':  cjsonx_strbuf_append(sb, "\\\"", 2); break;
                        case '\\': cjsonx_strbuf_append(sb, "\\\\", 2); break;
                        case '\b': cjsonx_strbuf_append(sb, "\\b", 2); break;
                        case '\f': cjsonx_strbuf_append(sb, "\\f", 2); break;
                        case '\n': cjsonx_strbuf_append(sb, "\\n", 2); break;
                        case '\r': cjsonx_strbuf_append(sb, "\\r", 2); break;
                        case '\t': cjsonx_strbuf_append(sb, "\\t", 2); break;
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
                case '"':  cjsonx_strbuf_append(sb, "\\\"", 2); break;
                case '\\': cjsonx_strbuf_append(sb, "\\\\", 2); break;
                case '\b': cjsonx_strbuf_append(sb, "\\b", 2); break;
                case '\f': cjsonx_strbuf_append(sb, "\\f", 2); break;
                case '\n': cjsonx_strbuf_append(sb, "\\n", 2); break;
                case '\r': cjsonx_strbuf_append(sb, "\\r", 2); break;
                case '\t': cjsonx_strbuf_append(sb, "\\t", 2); break;
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
            ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
            ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
            ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
            ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '
        };
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
            if (n->val.b) cjsonx_strbuf_append(sb, "true", 4);
            else cjsonx_strbuf_append(sb, "false", 5);
            break;
        case CJSONX_NUMBER: {
            double val_num = n->val.f64;
            union { double d; uint64_t u; } uval = { val_num };
            if (uval.u == 0x8000000000000000ULL) {
                cjsonx_strbuf_append(sb, "-0", 2);
            } else if (val_num >= -9007199254740991.0 && val_num <= 9007199254740991.0 && val_num == (double)(int64_t)val_num) {
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
    if (initial_cap < 2048) initial_cap = 2048;

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
            if (sb.alloc && sb.alloc->free_fn) sb.alloc->free_fn(sb.buf, sb.alloc->user_data);
            else free(sb.buf);
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

    // ftell returns long, which caps file size at 2gb on 32-bit platforms.
    // also reject files larger than 4gb to prevent tape index overflow.
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
        if (alloc && alloc->free_fn) alloc->free_fn(string, alloc->user_data);
        else free(string);
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
        if (alloc && alloc->free_fn) alloc->free_fn(string, alloc->user_data);
        else free(string);
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
        if (doc->alloc.free_fn) doc->alloc.free_fn(json_str, doc->alloc.user_data);
        else free(json_str);
        return false;
    }

    size_t len = strlen(json_str);
    size_t written = fwrite(json_str, 1, len, fp);
    fclose(fp);
    if (doc->alloc.free_fn) doc->alloc.free_fn(json_str, doc->alloc.user_data);
    else free(json_str);

    return written == len;
}

bool cjsonx_write_file_format(const char* path, cjsonx_doc_t* doc, int indent_spaces) {
    if (!path || !doc) return false;

    char* json_str = cjsonx_stringify_format(doc, indent_spaces);
    if (!json_str) return false;

    FILE* fp = fopen(path, "wb");
    if (!fp) {
        if (doc->alloc.free_fn) doc->alloc.free_fn(json_str, doc->alloc.user_data);
        else free(json_str);
        return false;
    }

    size_t len = strlen(json_str);
    size_t written = fwrite(json_str, 1, len, fp);
    fclose(fp);
    if (doc->alloc.free_fn) doc->alloc.free_fn(json_str, doc->alloc.user_data);
    else free(json_str);

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
                dest_doc->node_count--; // rollback allocated node slot on failure
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
                    // clone value if it belongs to a different document to ensure proper arena ownership
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
#endif // cjsonx_implementation

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

#endif // cjsonx_builder_h
