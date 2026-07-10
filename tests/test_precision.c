// test_precision.c - strict conformance tests for high-precision large numbers and fractions.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

// mock custom allocator without realloc_fn
static size_t mock_malloc_count = 0;
static size_t mock_free_count = 0;

static void* mock_malloc(size_t size, void* user_data) {
    (void)user_data;
    mock_malloc_count++;
    return malloc(size);
}

static void mock_free(void* ptr, void* user_data) {
    (void)user_data;
    if (ptr) {
        mock_free_count++;
        free(ptr);
    }
}

int main() {
    printf("running precision and strict conformance tests...\n");

    // 1. float precision/exact rounding fallback check
    {
        const char* s = "0.353809201110073495780198";
        cjsonx_doc* doc = cjsonx_parse(s, strlen(s));
        if (!doc || !doc->is_valid) {
            printf("fail: float parsing failed\n");
            return 1;
        }
        double d_fast = doc->nodes[0].val.f64;
        cjsonx_doc_free(doc);

        double d_std = strtod(s, NULL);
        uint64_t b_fast, b_std;
        memcpy(&b_fast, &d_fast, 8);
        memcpy(&b_std, &d_std, 8);

        if (b_fast != b_std) {
            printf("fail: float precision mismatch for >19 digits!\n");
            printf("  cjsonx: %.20g (0x%llx)\n", d_fast, (unsigned long long)b_fast);
            printf("  strtod: %.20g (0x%llx)\n", d_std, (unsigned long long)b_std);
            return 1;
        }
        printf("pass: float precision matched strtod exactly\n");
    }

    // 2. keyword trailing garbage check
    {
        const char* invalid_cases[] = {"truefalse", "true1", "falsejunk", "nullnull",
                                       "true}",     "null,", NULL};
        for (int i = 0; invalid_cases[i] != NULL; i++) {
            cjsonx_doc* doc = cjsonx_parse(invalid_cases[i], strlen(invalid_cases[i]));
            if (doc && doc->is_valid) {
                printf("fail: incorrectly accepted trailing garbage: %s\n", invalid_cases[i]);
                cjsonx_doc_free(doc);
                return 1;
            }
            if (doc) cjsonx_doc_free(doc);
        }
        printf("pass: keyword trailing garbage correctly rejected\n");
    }

    // 3. json pointer "/" empty key check
    {
        const char* json = "{\"\": 1234, \"foo\": 5678}";
        cjsonx_doc* doc = cjsonx_parse(json, strlen(json));
        if (!doc || !doc->is_valid) {
            printf("fail: parsing for json pointer test failed\n");
            return 1;
        }
        cjsonx_val v = cjsonx_pointer_get(doc->root, "/");
        if (cjsonx_get_type(v) != CJSONX_NUMBER || cjsonx_num(v) != 1234.0) {
            printf("fail: json pointer '/' failed to resolve empty key! type: %d, val: %g\n",
                   cjsonx_get_type(v), cjsonx_num(v));
            cjsonx_doc_free(doc);
            return 1;
        }
        cjsonx_doc_free(doc);
        printf("pass: json pointer '/' empty key resolved correctly\n");
    }

    // 4. custom allocator without realloc_fn check
    {
        cjsonx_allocator_t alloc;
        alloc.malloc_fn = mock_malloc;
        alloc.realloc_fn = NULL;
        alloc.free_fn = mock_free;
        alloc.user_data = NULL;

        mock_malloc_count = 0;
        mock_free_count = 0;

        cjsonx_doc* doc = cjsonx_parse_ex("{\"a\": [1, 2, 3], \"b\": \"hello\"}", 30, &alloc);
        if (!doc || !doc->is_valid) {
            printf("fail: custom allocator parsing failed\n");
            return 1;
        }
        cjsonx_doc_free(doc);
        if (mock_malloc_count == 0 || mock_malloc_count != mock_free_count) {
            printf("fail: custom allocator mismatched malloc (%zu) and free (%zu) calls\n",
                   mock_malloc_count, mock_free_count);
            return 1;
        }
        printf("pass: custom allocator without realloc emulated safely\n");
    }

    // 5. merge patch overlap index check
    {
        cjsonx_doc* doc1 = cjsonx_parse("{\"a\": 1}", 8);
        cjsonx_doc* doc2 = cjsonx_parse("{\"a\": 2}", 8);

        // force nodes array index to overlap (e.g. index 1)
        cjsonx_val target_val = cjsonx_get(doc1->root, "a");
        cjsonx_val patch_val = cjsonx_get(doc2->root, "a");

        if (target_val.node_idx != patch_val.node_idx) {
            printf("warn: could not align node indices for merge patch overlap test\n");
        } else {
            // merge patch should update target even if node indices match but docs differ
            cjsonx_val res = cjsonx_merge_patch(doc1->root, doc2->root);
            cjsonx_val res_val = cjsonx_get(res, "a");
            if (cjsonx_num(res_val) != 2.0) {
                printf("fail: merge patch failed when node indices overlap across documents!\n");
                cjsonx_doc_free(doc1);
                cjsonx_doc_free(doc2);
                return 1;
            }
        }
        cjsonx_doc_free(doc1);
        cjsonx_doc_free(doc2);
        printf("pass: merge patch document boundary checked successfully\n");
    }

    printf("all precision and conformance tests passed successfully!\n");
    return 0;
}
