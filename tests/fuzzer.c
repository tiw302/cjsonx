// fuzzer.c — libfuzzer harness with dom walk and stringify.
#include <stddef.h>
#include <stdint.h>

#include "cjsonx.h"

// libfuzzer entry point
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    /*
     * we limit size to prevent massive memory allocations during fuzzing
     * since cjsonx allocates dom nodes based on tape size.
     * 1 mb is more than enough to find edge cases.
     */
    if (size > 1024 * 1024) return 0;

    // parse the json
    cjsonx_doc_t* doc = cjsonx_parse((const char*)data, size);

    // if parsed successfully, we can test dom iteration or mutation
    if (doc) {
        if (doc->is_valid) {
            // walk all nodes including root (index 0) to catch any corrupted type fields
            for (uint32_t i = 0; i < doc->node_count; i++) {
                volatile cjsonx_type_t type = cjsonx_node_type(&doc->nodes[i]);
                (void)type;
            }

            // test builder functions
            cjsonx_val_t root = doc->root;
            cjsonx_type_t root_type = cjsonx_get_type(root);
            if (root_type == CJSONX_OBJECT) {
                cjsonx_val_t val = cjsonx_create_bool(doc, true);
                cjsonx_object_set(root, "fuzz_key", val);
                cjsonx_object_set(root, "fuzz_key", cjsonx_create_null(doc));
                cjsonx_object_remove(root, "fuzz_key");
            } else if (root_type == CJSONX_ARRAY) {
                cjsonx_val_t val = cjsonx_create_number(doc, 42.0);
                cjsonx_array_push(root, val);
                cjsonx_array_remove(root, cjsonx_size(root) - 1);
            }
            // test value cloning
            cjsonx_doc_t* cloned_doc = cjsonx_doc_new();
            if (cloned_doc) {
                cjsonx_val_t cloned_val = cjsonx_clone_val(cloned_doc, root);
                (void)cloned_val;
                cjsonx_doc_free(cloned_doc);
            }

            // test stringify
            char* str = cjsonx_stringify(doc);
            if (str) {
                free(str);
            }
        }

        cjsonx_doc_free(doc);
    }

    return 0;
}
