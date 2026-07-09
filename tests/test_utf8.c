// test_utf8.c — utf-8 validation acceptance/rejection tests.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

int main() {
    printf("Running UTF-8 validation tests...\n");

    // test 1: valid utf-8 string
    const char* valid_json = "{\"key\": \"Hello \\u2764\\ufe0f World! \xF0\x9F\x9A\x80\"}";
    cjsonx_doc* doc1 = cjsonx_parse(valid_json, strlen(valid_json));
    if (!doc1 || !doc1->is_valid) {
        printf("[FAIL] Valid UTF-8 string failed to parse. Error: %d\n",
               doc1 ? (int)doc1->error : -1);
        return 1;
    }
    printf("[PASS] Valid UTF-8 string parsed successfully.\n");
    cjsonx_doc_free(doc1);

    // test 2: invalid utf-8 string (invalid byte 0xff)
    const char* invalid_json1 = "{\"key\": \"Hello \xFF World!\"}";
    cjsonx_doc* doc2 = cjsonx_parse(invalid_json1, strlen(invalid_json1));
    if (doc2 && doc2->is_valid) {
        printf("[FAIL] Invalid UTF-8 string (0xFF) parsed successfully (should have failed).\n");
        return 1;
    }
    if (doc2->error != CJSONX_ERROR_INVALID_UTF8) {
        printf("[FAIL] Invalid UTF-8 string returned wrong error: %s\n",
               cjsonx_error_string(doc2->error));
        return 1;
    }
    printf("[PASS] Invalid UTF-8 string (0xFF) rejected correctly.\n");
    cjsonx_doc_free(doc2);

    // test 3: invalid utf-8 string (overlong encoding)
    const char* invalid_json2 = "{\"key\": \"Overlong \xC0\xAF\"}";
    cjsonx_doc* doc3 = cjsonx_parse(invalid_json2, strlen(invalid_json2));
    if (doc3 && doc3->is_valid) {
        printf("[FAIL] Overlong UTF-8 string parsed successfully (should have failed).\n");
        return 1;
    }
    if (doc3->error != CJSONX_ERROR_INVALID_UTF8) {
        printf("[FAIL] Overlong UTF-8 string returned wrong error: %s\n",
               cjsonx_error_string(doc3->error));
        return 1;
    }
    printf("[PASS] Overlong UTF-8 string rejected correctly.\n");
    cjsonx_doc_free(doc3);

    printf("All UTF-8 tests passed!\n");
    return 0;
}
