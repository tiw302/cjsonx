// test_allocator.c — custom allocator hook tests.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

// tracks active allocations to check for leaks and double-frees
#define MAX_TRACKED_ALLOCS 1024
static void* g_tracked_ptrs[MAX_TRACKED_ALLOCS];
static size_t g_tracked_count = 0;
static size_t g_total_allocs = 0;
static size_t g_total_frees = 0;

static void track_alloc(void* ptr) {
    if (!ptr) return;
    g_total_allocs++;
    if (g_tracked_count < MAX_TRACKED_ALLOCS) {
        g_tracked_ptrs[g_tracked_count++] = ptr;
    } else {
        printf("FAIL: Exceeded max tracked allocations capacity\n");
        exit(1);
    }
}

static void track_free(void* ptr) {
    if (!ptr) return;
    g_total_frees++;
    for (size_t i = 0; i < g_tracked_count; i++) {
        if (g_tracked_ptrs[i] == ptr) {
            g_tracked_ptrs[i] = g_tracked_ptrs[--g_tracked_count];
            return;
        }
    }
    printf("FAIL: Double free or untracked pointer free detected: %p\n", ptr);
    exit(1);
}

void* my_malloc(size_t size, void* user_data) {
    (void)user_data;
    void* ptr = malloc(size);
    track_alloc(ptr);
    return ptr;
}

void* my_realloc(void* ptr, size_t size, void* user_data) {
    (void)user_data;
    if (ptr) track_free(ptr);
    void* new_ptr = realloc(ptr, size);
    if (new_ptr) track_alloc(new_ptr);
    return new_ptr;
}

void my_free(void* ptr, void* user_data) {
    (void)user_data;
    if (ptr) track_free(ptr);
    free(ptr);
}

int main() {
    cjsonx_alc alloc = {
        .malloc_fn = my_malloc, .realloc_fn = my_realloc, .free_fn = my_free, .user_data = NULL};

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

    printf("Allocator Hook Trace: %zu allocs, %zu frees, %zu active\n", g_total_allocs,
           g_total_frees, g_tracked_count);

    if (g_total_allocs == 0 || g_total_frees == 0) {
        printf("FAIL: Hooks were not called!\n");
        return 1;
    }

    if (g_tracked_count > 0) {
        printf("FAIL: Memory leak detected! %zu allocations were not freed\n", g_tracked_count);
        return 1;
    }

    printf("PASS\n");
    return 0;
}
