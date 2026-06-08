// test_suite.c — single-file parse harness for json test suite.
#include "cjsonx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_helpers.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <json_file>\n", argv[0]);
        return 1;
    }

    const char* file_path = argv[1];
    size_t len;
    char* json = test_read_file(file_path, &len);

    if (!json) {
        fprintf(stderr, "failed to read file: %s\n", file_path);
        return 1;
    }

    cjsonx_doc* doc = cjsonx_parse(json, len);
    free(json);

    if (doc && doc->is_valid) {
        cjsonx_doc_free(doc);
        return 0;  // success
    } else {
        return 1;  // error
    }
}
