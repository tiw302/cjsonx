#define _GNU_SOURCE
#define CJSONX_IMPLEMENTATION
/*
 * file: builder_api.c
 * description: demonstrates how to dynamically create, modify, and serialize
 * json trees from scratch using the cjsonx builder api.
 */

#include <stdio.h>
#include <stdlib.h>

#include "cjsonx.h"

int main() {

    cjsonx_doc* doc = cjsonx_doc_new();
    if (!doc) {
        printf("Failed to create document\n");
        return 1;
    }


    doc->root = cjsonx_create_object(doc);


    cjsonx_object_set(doc->root, "name", cjsonx_create_string(doc, "cjsonx"));
    cjsonx_object_set(doc->root, "version", cjsonx_create_number(doc, 1.0));

    /* fast append (o(1) without duplicate check) - ideal for building large objects */
    cjsonx_object_add_unchecked(doc->root, "is_fast", cjsonx_create_bool(doc, true));


    cjsonx_val features = cjsonx_create_array(doc);
    cjsonx_array_push(features, cjsonx_create_string(doc, "SIMD parsing"));
    cjsonx_array_push(features, cjsonx_create_string(doc, "Zero allocations"));
    cjsonx_array_push(features, cjsonx_create_string(doc, "DOM builder"));


    cjsonx_object_set(doc->root, "features", features);


    char* json_minified = cjsonx_stringify(doc);
    printf("Minified JSON:\n%s\n\n", json_minified);


    char* json_formatted = cjsonx_stringify_format(doc, 4);
    printf("Formatted JSON:\n%s\n", json_formatted);

    /* note: strings returned by stringify must be freed manually */
    free(json_minified);
    free(json_formatted);
    cjsonx_doc_free(doc);

    return 0;
}
