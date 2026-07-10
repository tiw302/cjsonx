#define _GNU_SOURCE
#define CJSONX_IMPLEMENTATION
/*
 * file: error_handling.c
 * description: illustrates best practices for detecting and reporting syntax errors,
 * missing keys, and type mismatches during json parsing.
 */

#include <stdio.h>

#include "cjsonx.h"

int main() {
    /* a malformed json string (missing closing quote on "cjsonx") */
    const char* json =
        "{\n"
        "  \"name\": \"cjsonx,\n"
        "  \"fast\": true\n"
        "}";


    cjsonx_doc* doc = cjsonx_parse(json, strlen(json));

    if (doc) {
        if (!doc->is_valid) {
            printf("Error detected!\n");


            const char* err_msg = cjsonx_error_string(doc->error);


            size_t err_offset = doc->error_offset;

            printf("Message: %s\n", err_msg);
            printf("Offset:  %zu\n", err_offset);


            printf("\nJSON Snippet:\n");
            size_t start = (err_offset > 15) ? err_offset - 15 : 0;
            printf("%.30s...\n", json + start);

            int spaces = (int)(err_offset - start);
            for (int i = 0; i < spaces; i++) printf(" ");
            printf("^\n");
        }

        cjsonx_doc_free(doc);
    }

    return 0;
}
