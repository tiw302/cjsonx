// test_stringify.c - stringify roundtrip and control character escape handling.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

int main() {
    cjsonx_doc* doc = cjsonx_parse("{}", 2);

    // create string with control characters and unicode
    // "\b \f \n \r \t \" \\ \u0001 🔥"
    const char* str = "\b \f \n \r \t \" \\ \x01 \xF0\x9F\x94\xA5";
    cjsonx_val v = cjsonx_create_string(doc, str);
    cjsonx_object_set(doc->root, "weird", v);

    char* out = cjsonx_stringify(doc);
    if (!out) {
        printf("FAIL: Stringify returned NULL\n");
        return 1;
    }

    // check if the output is correctly escaped
    const char* expected = "{\"weird\":\"\\b \\f \\n \\r \\t \\\" \\\\ \\u0001 \xF0\x9F\x94\xA5\"}";

    if (strcmp(out, expected) != 0) {
        printf("FAIL: Output mismatch\nExpected: %s\nGot: %s\n", expected, out);
        return 1;
    }

    free(out);
    cjsonx_doc_free(doc);
    printf("PASS\n");
    return 0;
}
