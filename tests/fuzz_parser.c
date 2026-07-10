// fuzz_parser.c - minimal libfuzzer harness for the parser.

#include <stddef.h>
#include <stdint.h>

#include "cjsonx.h"

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
    cjsonx_doc* doc = cjsonx_parse((const char*)Data, Size);
    if (doc) {
        cjsonx_doc_free(doc);
    }
    return 0;
}
