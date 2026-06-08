// test_builder_remove.c — tests for object/array element removal.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

int main() {
    cjsonx_doc* doc = cjsonx_parse("{}", 2);
    
    cjsonx_val v1 = cjsonx_create_string(doc, "A");
    cjsonx_val v2 = cjsonx_create_string(doc, "B");
    cjsonx_val v3 = cjsonx_create_string(doc, "C");
    
    cjsonx_object_set(doc->root, "a", v1);
    cjsonx_object_set(doc->root, "b", v2);
    cjsonx_object_set(doc->root, "c", v3);
    
    char* str1 = cjsonx_stringify(doc);
    printf("Original: %s\n", str1);
    free(str1);
    
    // remove "b" (middle)
    bool rm = cjsonx_object_remove(doc->root, "b");
    if (!rm) { printf("FAIL: could not remove b\n"); return 1; }
    
    char* str2 = cjsonx_stringify(doc);
    printf("After removing b: %s\n", str2);
    if (strstr(str2, "\"b\"") != NULL) {
        printf("FAIL: b still exists\n");
        return 1;
    }
    free(str2);
    
    // remove "a" (first)
    cjsonx_object_remove(doc->root, "a");
    char* str3 = cjsonx_stringify(doc);
    printf("After removing a: %s\n", str3);
    if (strstr(str3, "\"a\"") != NULL) {
        printf("FAIL: a still exists\n");
        return 1;
    }
    free(str3);
    
    // test array
    cjsonx_val arr = cjsonx_create_array(doc);
    cjsonx_array_push(arr, cjsonx_create_number(doc, 1));
    cjsonx_array_push(arr, cjsonx_create_number(doc, 2));
    cjsonx_array_push(arr, cjsonx_create_number(doc, 3));
    
    cjsonx_object_set(doc->root, "arr", arr);
    
    // remove index 1 (value 2)
    cjsonx_array_remove(arr, 1);
    char* str4 = cjsonx_stringify(doc);
    printf("Array after remove index 1: %s\n", str4);
    if (strstr(str4, "2") != NULL) {
        printf("FAIL: 2 still exists in array\n");
        return 1;
    }
    free(str4);
    
    cjsonx_doc_free(doc);
    printf("PASS\n");
    return 0;
}
