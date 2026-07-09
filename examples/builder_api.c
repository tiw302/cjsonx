#include <stdio.h>
#include <stdlib.h>

#include "cjsonx.h"

int main() {
    // 1. create a new empty document
    cjsonx_doc* doc = cjsonx_doc_new();
    if (!doc) {
        printf("Failed to create document\n");
        return 1;
    }

    // 2. create the root object
    doc->root = cjsonx_create_object(doc);

    // 3. add simple key-value pairs
    cjsonx_object_set(doc->root, "name", cjsonx_create_string(doc, "cjsonx"));
    cjsonx_object_set(doc->root, "version", cjsonx_create_number(doc, 1.0));

    // fast append (o(1) without duplicate check) - ideal for building large objects
    cjsonx_object_add_unchecked(doc->root, "is_fast", cjsonx_create_bool(doc, true));

    // 4. add an array of strings
    cjsonx_val features = cjsonx_create_array(doc);
    cjsonx_array_push(features, cjsonx_create_string(doc, "SIMD parsing"));
    cjsonx_array_push(features, cjsonx_create_string(doc, "Zero allocations"));
    cjsonx_array_push(features, cjsonx_create_string(doc, "DOM builder"));

    // attach array to root object
    cjsonx_object_set(doc->root, "features", features);

    // 5. stringify the document (minified)
    char* json_minified = cjsonx_stringify(doc);
    printf("Minified JSON:\n%s\n\n", json_minified);

    // 6. stringify the document (formatted with 4 spaces)
    char* json_formatted = cjsonx_stringify_format(doc, 4);
    printf("Formatted JSON:\n%s\n", json_formatted);

    // 7. cleanup
    // note: strings returned by stringify must be freed manually
    free(json_minified);
    free(json_formatted);
    cjsonx_doc_free(doc);

    return 0;
}
