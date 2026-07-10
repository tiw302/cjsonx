// test_pointer.c - rfc 6901 json pointer evaluation tests.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

int main() {
    const char* json =
        "{\n"
        "  \"foo\": [\"bar\", \"baz\"],\n"
        "  \"\": 0,\n"
        "  \"a/b\": 1,\n"
        "  \"c%d\": 2,\n"
        "  \"e^f\": 3,\n"
        "  \"g|h\": 4,\n"
        "  \"i\\\\j\": 5,\n"
        "  \"k\\\"l\": 6,\n"
        "  \" \": 7,\n"
        "  \"m~n\": 8\n"
        "}";

    cjsonx_doc* doc = cjsonx_parse(json, strlen(json));


    cjsonx_val v0 = cjsonx_pointer_get(doc->root, "");
    if (cjsonx_get_type(v0) != CJSONX_OBJECT) {
        printf("FAIL: \"\" should return whole document\n");
        return 1;
    }

    cjsonx_val v1 = cjsonx_pointer_get(doc->root, "/foo");
    if (cjsonx_get_type(v1) != CJSONX_ARRAY) {
        printf("FAIL: \"/foo\" should return array\n");
        return 1;
    }

    cjsonx_val v2 = cjsonx_pointer_get(doc->root, "/foo/0");
    if (strncmp(cjsonx_str(v2), "bar", 3) != 0) {
        printf("FAIL: \"/foo/0\" should return bar\n");
        return 1;
    }

    cjsonx_val v3 = cjsonx_pointer_get(doc->root, "/");
    if (cjsonx_num(v3) != 0) {
        printf("FAIL: \"/\" should return 0\n");
        return 1;
    }

    cjsonx_val v4 = cjsonx_pointer_get(doc->root, "/a~1b");
    if (cjsonx_num(v4) != 1) {
        printf("FAIL: \"/a~1b\" should return 1\n");
        return 1;
    }

    cjsonx_val v8 = cjsonx_pointer_get(doc->root, "/m~0n");
    if (cjsonx_num(v8) != 8) {
        printf("FAIL: \"/m~0n\" should return 8\n");
        return 1;
    }

    /* check rfc 6901 compliance: leading zero and signs must be rejected */
    cjsonx_val v_invalid1 = cjsonx_pointer_get(doc->root, "/foo/01");
    if (v_invalid1.doc != NULL) {
        printf("FAIL: \"/foo/01\" should be invalid (leading zero)\n");
        return 1;
    }

    cjsonx_val v_invalid2 = cjsonx_pointer_get(doc->root, "/foo/+1");
    if (v_invalid2.doc != NULL) {
        printf("FAIL: \"/foo/+1\" should be invalid (sign)\n");
        return 1;
    }

    cjsonx_doc_free(doc);
    printf("PASS\n");
    return 0;
}
