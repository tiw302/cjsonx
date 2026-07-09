// test_bomb.c — depth bomb test to ensure no stack overflow.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

int main() {
    size_t depth = 100000;
    size_t len = depth * 2;
    char* bomb = malloc(len + 1);
    if (!bomb) return 1;

    for (size_t i = 0; i < depth; i++) {
        bomb[i] = '[';
        bomb[len - 1 - i] = ']';
    }
    bomb[len] = '\0';

    // try to parse - this shouldn't crash with a segfault/stack overflow
    cjsonx_doc* doc = cjsonx_parse(bomb, len);

    // it will probably fail because of cjsonx_error_depth or similar,
    // but the critical part is that it does not crash.
    if (doc) {
        if (!doc->is_valid) {
            printf("Parsing failed gracefully: %d\n", doc->error);
        } else {
            printf("Parsing succeeded surprisingly!\n");
        }
        cjsonx_doc_free(doc);
    } else {
        printf("Parsing failed completely (NULL doc)\n");
    }

    free(bomb);
    printf("PASS (No crash)\n");
    return 0;
}
