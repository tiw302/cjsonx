// fuzzer.c — libfuzzer harness with dom walk and stringify.
#include <stdint.h>
#include <stddef.h>

#include "cjsonx.h"

// libfuzzer entry point
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // we limit size to prevent massive memory allocations during fuzzing
    // since cjsonx allocates dom nodes based on tape size.
    // 1 mb is more than enough to find edge cases.
    if (size > 1024 * 1024) return 0;
    
    // parse the json
    cjsonx_doc_t* doc = cjsonx_parse((const char*)data, size);
    
    // if parsed successfully, we can test dom iteration or mutation
    if (doc) {
        if (doc->is_valid) {
            // walk the dom to ensure memory is properly linked
            for (uint32_t i = 1; i < doc->node_count; i++) {
                volatile cjsonx_type_t type = cjsonx_node_type(&doc->nodes[i]);
                (void)type;
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
