#define _GNU_SOURCE
#define CJSONX_IMPLEMENTATION
/*
 * file: dom_access.c
 * description: shows how to navigate the parsed json document object model (dom)
 * to extract objects, arrays, strings, and numbers safely.
 */

#include <stdio.h>
#include <string.h>

#include "cjsonx.h"

int main() {
    const char* json =
        "{\n"
        "  \"name\": \"cjsonx\",\n"
        "  \"fast\": true,\n"
        "  \"tags\": [\"C11\", \"JSON\", \"SIMD\"]\n"
        "}";


    cjsonx_doc* doc = cjsonx_parse(json, strlen(json));

    if (!doc || !doc->is_valid) {
        printf("failed to parse json\n");
        return 1;
    }


    cjsonx_val root = doc->root;


    cjsonx_val name = cjsonx_get(root, "name");
    if (cjsonx_get_type(name) == CJSONX_STRING) {
        printf("name: %.*s\n", (int)cjsonx_str_len(name), cjsonx_str(name));
    }


    cjsonx_val fast = cjsonx_get(root, "fast");
    if (cjsonx_get_type(fast) == CJSONX_BOOL) {
        printf("fast: %s\n", cjsonx_bool(fast) ? "true" : "false");
    }


    cjsonx_val tags = cjsonx_get(root, "tags");
    if (cjsonx_get_type(tags) == CJSONX_ARRAY) {
        printf("tags:\n");
        size_t count = cjsonx_size(tags);
        for (size_t i = 0; i < count; i++) {
            cjsonx_val item = cjsonx_get_index(tags, i);
            printf("  [%zu]: %.*s\n", i, (int)cjsonx_str_len(item), cjsonx_str(item));
        }
    }


    cjsonx_doc_free(doc);

    return 0;
}
