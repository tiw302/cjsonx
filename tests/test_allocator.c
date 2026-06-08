// test_allocator.c — custom allocator hook tests.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

// mock allocator that tracks allocations
static size_t g_alloc_count = 0;
static size_t g_free_count = 0;

void* my_malloc(size_t size, void* user_data) {
    (void)user_data;
    g_alloc_count++;
    return malloc(size);
}

void* my_realloc(void* ptr, size_t size, void* user_data) {
    (void)user_data;
    // realloc always produces a (possibly new) allocation.
    // if ptr is non-null the old block is conceptually freed.
    g_alloc_count++;
    if (ptr) g_free_count++;
    return realloc(ptr, size);
}

void my_free(void* ptr, void* user_data) {
    (void)user_data;
    g_free_count++;
    free(ptr);
}

int main() {
    cjsonx_alc alloc = {
        .malloc_fn = my_malloc,
        .realloc_fn = my_realloc,
        .free_fn = my_free,
        .user_data = NULL
    };
    
    const char* json = "{\"name\": \"Alice\", \"age\": 30}";
    
    // parse using custom allocator
    cjsonx_doc* doc = cjsonx_parse_ex(json, strlen(json), &alloc);
    
    if (!doc || !doc->is_valid) {
        printf("FAIL: Failed to parse using custom allocator\n");
        return 1;
    }
    
    cjsonx_val name = cjsonx_get(doc->root, "name");
    if (strncmp(cjsonx_str(name), "Alice", 5) != 0) {
        printf("FAIL: Value check failed\n");
        return 1;
    }
    
    // add something using builder to trigger realloc/malloc
    cjsonx_val new_val = cjsonx_create_string(doc, "Test");
    cjsonx_object_set(doc->root, "new_key", new_val);
    
    char* str = cjsonx_stringify(doc);
    if (!str) {
        printf("FAIL: Failed to stringify\n");
        return 1;
    }
    
    alloc.free_fn(str, alloc.user_data);
    cjsonx_doc_free(doc);
    
    printf("Allocator Hook Trace: %zu allocs, %zu frees\n", g_alloc_count, g_free_count);
    
    if (g_alloc_count == 0 || g_free_count == 0) {
        printf("FAIL: Hooks were not called!\n");
        return 1;
    }
    
    if (g_alloc_count != g_free_count) {
        printf("FAIL: Memory leak detected through allocator hook!\n");
        return 1;
    }
    
    printf("PASS\n");
    return 0;
}
