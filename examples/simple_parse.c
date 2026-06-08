#include "cjsonx.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* json_str =
        "{"
        "  \"name\": \"Alice\","
        "  \"age\": 28,"
        "  \"skills\": [\"C\", \"SIMD\", \"Performance\"],"
        "  \"active\": true"
        "}";

    printf("Parsing JSON:\n%s\n\n", json_str);

    // parse the json string
    cjsonx_doc* doc = cjsonx_parse(json_str, strlen(json_str));
    if (!doc) {
        printf("Failed to parse JSON!\n");
        return 1;
    }

    // extract values
    cjsonx_val root = doc->root;
    cjsonx_val name = cjsonx_get(root, "name");
    cjsonx_val age = cjsonx_get(root, "age");
    cjsonx_val skills = cjsonx_get(root, "skills");
    cjsonx_val active = cjsonx_get(root, "active");

    // print primitive values
    int name_len = (int)cjsonx_str_len(name);
    printf("Name: %.*s\n", name_len, cjsonx_str(name));
    printf("Age: %d\n", (int)cjsonx_num(age));
    printf("Active: %s\n", cjsonx_bool(active) ? "Yes" : "No");

    // iterate over an array using flat dom iterator api
    if (cjsonx_get_type(skills) == CJSONX_ARRAY) {
        printf("Skills:\n");
        cjsonx_iter iter = cjsonx_iter_init(skills);
        while (cjsonx_iter_next(&iter)) {
            int skill_len = (int)cjsonx_str_len(iter.value);
            printf("  - %.*s\n", skill_len, cjsonx_str(iter.value));
        }
    }

    // clean up
    cjsonx_doc_free(doc);
    return 0;
}
