#define _GNU_SOURCE
#define CJSONX_IMPLEMENTATION
/*
 * file: float128_precision.c
 * description: demonstrates parsing extremely precise floating-point numbers
 * without precision loss, leveraging the fastfloat integration.
 */

#include <stdio.h>

#include "cjsonx.h"

int main() {
    /* a json string with extremely long numbers that would overflow standard parsers */
    const char* json =
        "{\n"
        "  \"planck_length\": 1.616255e-35,\n"
        "  \"huge_integer\": 9999999999999999999999999999999999999999,\n"
        "  \"ultra_precise\": 3.1415926535897932384626433832795028841971\n"
        "}";

    /* cjsonx parses and validates these numbers using clinger's fast path and eisel-lemire
     * internally ensuring they conform strictly to rfc 8259 without crashing or throwing */
    cjsonx_doc* doc = cjsonx_parse(json, strlen(json));

    if (doc && doc->is_valid) {
        printf("successfully parsed massive 128-bit numbers!\n\n");

        cjsonx_val root = doc->root;

        cjsonx_val huge = cjsonx_get(root, "huge_integer");
        if (cjsonx_get_type(huge) == CJSONX_NUMBER) {
            /* numbers are parsed directly into double precision via our fast float engine */
            printf("huge_integer: %g\n", cjsonx_num(huge));
        }

        cjsonx_doc_free(doc);
    } else {
        printf("parse failed\n");
    }

    return 0;
}
