// test compiling and basic parsing using the amalgamated single-header
#define CJSONX_IMPLEMENTATION
#include "../single_include/cjsonx.h"
#include <stdio.h>
#include <string.h>

int main() {
    const char* json = "{\"hello\": \"world\"}";
    cjsonx_doc_t* doc = cjsonx_parse(json, strlen(json));
    if (!doc) { printf("Failed\n"); return 1; }
    printf("Success! Nodes: %zu\n", doc->node_count);
    cjsonx_doc_free(doc);
    return 0;
}
