#include <stdio.h>
#include <string.h>
#include "cjsonx.h"

int main() {
    char bad_json_raw[128];
    // fill the buffer with spaces first to avoid any uninitialized garbage
    memset(bad_json_raw, ' ', sizeof(bad_json_raw));
    
    bad_json_raw[0] = '[';
    bad_json_raw[1] = '"';
    bad_json_raw[2] = '\\';
    bad_json_raw[3] = '\\';
    
    // put a control character (0x01) at index 40 of the string.
    // the string starts at index 2 (after the quote at index 1).
    // so string index 40 is buffer index 42.
    bad_json_raw[42] = 0x01; 
    
    // put the closing quote at string index 80, which is buffer index 82.
    bad_json_raw[82] = '"';
    bad_json_raw[83] = ']';
    bad_json_raw[84] = '\0';
    int total_len = 84;

    printf("Parsing JSON with length %d\n", total_len);
    cjsonx_doc* doc = cjsonx_parse(bad_json_raw, total_len);
    if (doc) {
        printf("is_valid: %d, error: %s, error_offset: %zu\n", doc->is_valid, cjsonx_error_string(doc->error), doc->error_offset);
        if (doc->is_valid) {
            printf("BUG: Parsed invalid JSON successfully!\n");
            cjsonx_doc_free(doc);
            return 1;
        }
        cjsonx_doc_free(doc);
    }
    printf("SUCCESS: Correctly rejected invalid JSON\n");
    return 0;
}
