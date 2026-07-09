// test_truefalse.c — diagnostic tests for float precision and keyword boundaries.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

void dump_doc(cjsonx_doc* doc) {
    printf("nodes count: %zu\n", doc->node_count);
    for (size_t i = 0; i < doc->node_count; i++) {
        cjsonx_node_t* n = &doc->nodes[i];
        printf("node %zu: type=%d, len=%d, next_sibling=%u, val.f64=%g, val.str=%s\n", i,
               cjsonx_node_type(n), cjsonx_node_length(n), n->next_sibling, n->val.f64,
               (cjsonx_node_type(n) == CJSONX_STRING) ? n->val.str : "null");
    }
}

int main() {
    cjsonx_doc* doc1 = cjsonx_parse("{\"a\": 1}", 8);
    cjsonx_doc* doc2 = cjsonx_parse("{\"a\": 2}", 8);

    printf("--- doc1 before merge ---\n");
    dump_doc(doc1);

    cjsonx_val res = cjsonx_merge_patch(doc1->root, doc2->root);

    printf("--- doc1 after merge ---\n");
    dump_doc(doc1);

    cjsonx_val res_val = cjsonx_get(res, "a");
    printf("res_val: type=%d, val=%g\n", cjsonx_get_type(res_val), cjsonx_num(res_val));

    cjsonx_doc_free(doc1);
    cjsonx_doc_free(doc2);
    return 0;
}
