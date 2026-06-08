// test_builder.c — tests for dom builder and stringify api.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

int main() {
    printf("Running builder tests...\n");
    
    // parse an empty object
    cjsonx_doc* doc = cjsonx_parse("{}", 2);
    if (!doc || !doc->is_valid) {
        printf("[FAIL] Failed to parse empty object\n");
        return 1;
    }
    
    // create primitives
    cjsonx_val name = cjsonx_create_string(doc, "cjsonx");
    cjsonx_val version = cjsonx_create_number(doc, 2.1);
    cjsonx_val fast = cjsonx_create_bool(doc, true);
    
    // set to root object
    cjsonx_object_set(doc->root, "name", name);
    cjsonx_object_set(doc->root, "version", version);
    cjsonx_object_set(doc->root, "is_fast", fast);
    
    // create array
    cjsonx_val features = cjsonx_create_array(doc);
    cjsonx_array_push(features, cjsonx_create_string(doc, "simd"));
    cjsonx_array_push(features, cjsonx_create_string(doc, "strict"));
    cjsonx_object_set(doc->root, "features", features);
    
    char* json_str = cjsonx_stringify(doc);
    printf("Generated Minified JSON:\n%s\n\n", json_str);
    
    char* json_fmt = cjsonx_stringify_format(doc, 4);
    printf("Generated Formatted JSON:\n%s\n\n", json_fmt);
    
    // test key overwrite behavior
    cjsonx_val new_version = cjsonx_create_number(doc, 3.0);
    cjsonx_object_set(doc->root, "version", new_version);
    
    char* json_str_overwrite = cjsonx_stringify(doc);
    
    // verify
    const char* expected_overwrite = "{\"name\":\"cjsonx\",\"version\":3,\"is_fast\":true,\"features\":[\"simd\",\"strict\"]}";
    if (strcmp(json_str_overwrite, expected_overwrite) == 0) {
        printf("[PASS] Builder overwrite and Stringify working perfectly!\n");
    } else {
        printf("[FAIL] Output did not match expected after overwrite.\n");
        printf("Expected: %s\n", expected_overwrite);
        printf("Got:      %s\n", json_str_overwrite);
        return 1;
    }
    
    free(json_str);
    free(json_fmt);
    free(json_str_overwrite);
    cjsonx_doc_free(doc);

    // Test 1: cjsonx_doc_new() and builder from scratch
    printf("Testing cjsonx_doc_new()...\n");
    cjsonx_doc* new_doc = cjsonx_doc_new();
    if (!new_doc || !new_doc->is_valid) {
        printf("[FAIL] Failed to create new document from scratch\n");
        return 1;
    }
    // Set root to a newly created object
    cjsonx_val root_obj = cjsonx_create_object(new_doc);
    new_doc->root = root_obj;
    cjsonx_object_set(new_doc->root, "status", cjsonx_create_string(new_doc, "success"));
    cjsonx_object_set(new_doc->root, "code", cjsonx_create_number(new_doc, 200));

    char* new_json = cjsonx_stringify(new_doc);
    const char* expected_new = "{\"status\":\"success\",\"code\":200}";
    if (strcmp(new_json, expected_new) != 0) {
        printf("[FAIL] cjsonx_doc_new output mismatch. Got: %s\n", new_json);
        free(new_json);
        cjsonx_doc_free(new_doc);
        return 1;
    }
    printf("[PASS] cjsonx_doc_new() and builder from scratch passed!\n");

    // Test 2: cjsonx_stringify_val()
    printf("Testing cjsonx_stringify_val()...\n");
    cjsonx_val status_val = cjsonx_get(new_doc->root, "status");
    char* val_str = cjsonx_stringify_val(status_val);
    const char* expected_val = "\"success\"";
    if (strcmp(val_str, expected_val) != 0) {
        printf("[FAIL] cjsonx_stringify_val output mismatch. Got: %s\n", val_str);
        free(val_str);
        free(new_json);
        cjsonx_doc_free(new_doc);
        return 1;
    }
    printf("[PASS] cjsonx_stringify_val() passed!\n");
    free(val_str);
    free(new_json);
    cjsonx_doc_free(new_doc);

    // Test 3: cjsonx_parse_str() macro
    printf("Testing cjsonx_parse_str()...\n");
    cjsonx_doc* parsed_macro = cjsonx_parse_str("{\"macro\":true}");
    if (!parsed_macro || !parsed_macro->is_valid) {
        printf("[FAIL] cjsonx_parse_str failed to parse\n");
        return 1;
    }
    char* macro_str = cjsonx_stringify(parsed_macro);
    if (strcmp(macro_str, "{\"macro\":true}") != 0) {
        printf("[FAIL] cjsonx_parse_str output mismatch. Got: %s\n", macro_str);
        free(macro_str);
        cjsonx_doc_free(parsed_macro);
        return 1;
    }
    printf("[PASS] cjsonx_parse_str() macro passed!\n");
    free(macro_str);
    cjsonx_doc_free(parsed_macro);

    return 0;
}
