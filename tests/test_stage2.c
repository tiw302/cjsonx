// test_stage2.c — visual test for stage 2 dom parser.
// parses json strings into a dom tree and prints field types for inspection.

#include "cjsonx.h"
#include <stdio.h>
#include <string.h>

void test_dom(const char* json) {
    printf("\nTesting JSON: %s\n", json);

    cjsonx_doc* doc = cjsonx_parse(json, strlen(json));
    if (!doc) {
        printf("Failed to parse JSON\n");
        return;
    }

    cjsonx_val root = doc->root;
    if (cjsonx_get_type(root) == CJSONX_OBJECT) {
        printf("Root is OBJECT with %zu fields\n", cjsonx_size(root));
        cjsonx_iter iter = cjsonx_iter_init(root);
        while (cjsonx_iter_next(&iter)) {
            const char* key_str = cjsonx_str(iter.key);
            size_t key_len = cjsonx_str_len(iter.key);
            printf("  Field '%.*s': ", (int)key_len, key_str);
            
            cjsonx_type t = cjsonx_get_type(iter.value);
            if (t == CJSONX_STRING) {
                printf("STRING '%.*s'\n", (int)cjsonx_str_len(iter.value), cjsonx_str(iter.value));
            } else if (t == CJSONX_NUMBER) {
                printf("NUMBER %f\n", cjsonx_num(iter.value));
            } else if (t == CJSONX_BOOL) {
                printf("BOOL %s\n", cjsonx_bool(iter.value) ? "true" : "false");
            } else if (t == CJSONX_ARRAY) {
                printf("ARRAY of size %zu\n", cjsonx_size(iter.value));
            } else if (t == CJSONX_OBJECT) {
                printf("OBJECT with %zu fields\n", cjsonx_size(iter.value));
            } else if (t == CJSONX_NULL) {
                printf("NULL\n");
            } else {
                printf("UNKNOWN TYPE\n");
            }
        }
    }

    cjsonx_doc_free(doc);
}

void test_error(const char* json, cjsonx_error_t expected_error, size_t expected_offset) {
    cjsonx_doc* doc = cjsonx_parse(json, strlen(json));
    if (!doc) {
        printf("Failed to parse JSON (doc was null)\n");
        exit(1);
    }
    if (doc->is_valid) {
        printf("FAIL: Expected invalid JSON for '%s' but parsed successfully\n", json);
        exit(1);
    }
    if (doc->error != expected_error) {
        printf("FAIL: Expected error %d, got %d for '%s'\n", (int)expected_error, (int)doc->error, json);
        exit(1);
    }
    if (doc->error_offset != expected_offset) {
        printf("FAIL: Expected error offset %zu, got %zu for '%s'\n", expected_offset, doc->error_offset, json);
        exit(1);
    }
    printf("PASS error: '%s' failed at offset %zu with correct error code\n", json, doc->error_offset);
    cjsonx_doc_free(doc);
}

int main(void) {
    printf("--- cjsonx Stage 2 Test ---\n");

    // basic types: string, number, boolean
    test_dom("{\"key\": \"value\", \"age\": 30, \"is_dev\": true}");

    // nested containers: array + sub-object
    test_dom("{\"array\": [1, 2, 3], \"nested\": {\"a\": 1}}");

    // escape sequences + utf-16 surrogate pair (rocket emoji u+1f680)
    test_dom("{\"escape\": \"Hello\\nWorld! \\u2764\\ufe0f\", \"emoji\": \"\\ud83d\\ude80\"}");

    printf("\n--- Running Error Offset and Correctness Tests ---\n");
    test_error("{\"key\" 30}", CJSONX_ERROR_MISSING_COLON, 7);
    test_error("{\"key\": }", CJSONX_ERROR_UNEXPECTED_TOKEN, 8);
    test_error("{\"key\": \"val\",}", CJSONX_ERROR_TRAILING_COMMA, 14);
    test_error("[1, 2, ]", CJSONX_ERROR_TRAILING_COMMA, 7);
    test_error("{\"key\": \"abc\\x\"}", CJSONX_ERROR_INVALID_ESCAPE, 8);
    test_error("", CJSONX_ERROR_EMPTY_INPUT, 0);
    test_error("   ", CJSONX_ERROR_EMPTY_INPUT, 3);

    return 0;
}
