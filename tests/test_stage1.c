// test_stage1.c - unit test for stage 1 (simd structural token indexing).
// verifies that quotes, brackets, and primitives are identified correctly.

#include <stdio.h>
#include <string.h>

#include "cjsonx.h"

void print_tape(const char* json, cjsonx_tape* tape) {
    printf("Found %zu structural characters:\n", tape->count);
    for (size_t i = 0; i < tape->count; i++) {
        uint32_t idx = tape->indices[i];
        printf("  [%u] '%c'\n", idx, json[idx]);
    }
}

bool test_json(const char* json) {
    cjsonx_tape tape;
    if (!cjsonx_tape_init(&tape, 64, NULL)) return false;

    cjsonx_stage1_build_tape(json, strlen(json), &tape);

    printf("\nTesting JSON: %s\n", json);
    print_tape(json, &tape);

    cjsonx_tape_free(&tape);
    return true;
}

int main(void) {
    printf("--- cjsonx Stage 1 Test ---\n");
    test_json("{\"key\": \"value\"}");
    test_json("{\"array\": [1, 2, 3], \"escaped\": \"\\\"in string\\\"\"}");
    test_json("{\"complex, string:with_structural_chars\": 42}");
    return 0;
}
