// test_static_buffer.c — tests for cjsonx_parse_with_buffer (zero-alloc static mode).
//
// note: static buffer documents are read-only. the builder api (cjsonx_object_set,
// cjsonx_array_push etc.) will return failure on static docs because they cannot
// grow the internal nodes array. stage 2 pre-allocates tape.count+1 nodes in the
// static buffer, which is always enough since cjsonx_next_token checks capacity for
// every tape entry (including commas and closers that don't allocate a node).
#include <stdio.h>
#include <string.h>

#include "cjsonx.h"

// helper macro: print pass/fail and exit on failure
#define CHECK(cond, msg)                  \
    do {                                  \
        if (!(cond)) {                    \
            printf("[fail] %s\n", (msg)); \
            return 1;                     \
        }                                 \
        printf("[pass] %s\n", (msg));     \
    } while (0)

// 8 kb static buffer — enough for basic test cases
static uint8_t s_buf[8192];

int main(void) {
    printf("running static buffer tests...\n");

    // basic parse into static buffer
    memset(s_buf, 0, sizeof(s_buf));
    {
        const char* json = "{\"name\":\"alice\",\"age\":30}";
        cjsonx_doc_t* doc = cjsonx_parse_with_buffer(json, strlen(json), s_buf, sizeof(s_buf));
        CHECK(doc != NULL, "parse_with_buffer returned non-null");
        CHECK(doc->is_valid, "parse_with_buffer result is valid");
        CHECK(doc->is_static, "doc is marked as static");

        cjsonx_val_t name = cjsonx_get(doc->root, "name");
        CHECK(cjsonx_get_type(name) == CJSONX_STRING, "name field is string");
        // zero-copy strings point into the json buffer — they are NOT null-terminated.
        // always use cjsonx_str_len() to bound the comparison.
        CHECK(cjsonx_str_len(name) == 5 && strncmp(cjsonx_str(name), "alice", 5) == 0,
              "name value is 'alice'");

        cjsonx_val_t age = cjsonx_get(doc->root, "age");
        CHECK(cjsonx_get_type(age) == CJSONX_NUMBER, "age field is number");
        CHECK((int)cjsonx_num(age) == 30, "age value is 30");

        // cjsonx_doc_free on a static doc must be a no-op (no free call)
        cjsonx_doc_free(doc);
        printf("[pass] cjsonx_doc_free on static buffer is safe\n");
    }

    // buffer too small: should return a doc with CJSONX_ERROR_OOM or NULL
    {
        const char* json = "{\"key\":\"value\"}";
        uint8_t tiny[8];  // far too small
        cjsonx_doc_t* doc = cjsonx_parse_with_buffer(json, strlen(json), tiny, sizeof(tiny));
        // doc may be null if it can't even fit the cjsonx_doc_t struct
        if (doc) {
            CHECK(!doc->is_valid, "undersized buffer gives invalid doc");
            CHECK(doc->error == CJSONX_ERROR_OOM, "undersized buffer error is OOM");
            printf("[pass] undersized buffer correctly returns OOM\n");
        } else {
            printf("[pass] undersized buffer correctly returns NULL\n");
        }
    }

    // test with array — fresh buffer
    memset(s_buf, 0, sizeof(s_buf));
    {
        const char* json = "[1,2,3,4,5]";
        cjsonx_doc_t* doc = cjsonx_parse_with_buffer(json, strlen(json), s_buf, sizeof(s_buf));
        CHECK(doc != NULL && doc->is_valid, "array parse into static buffer is valid");
        CHECK(cjsonx_get_type(doc->root) == CJSONX_ARRAY, "root is array");
        CHECK(cjsonx_size(doc->root) == 5, "array has 5 elements");
        cjsonx_val_t first = cjsonx_get_index(doc->root, 0);
        CHECK((int)cjsonx_num(first) == 1, "first element is 1");
        cjsonx_doc_free(doc);
    }

    // test with a different object to confirm re-parse works on fresh buffer
    memset(s_buf, 0, sizeof(s_buf));
    {
        const char* json = "{\"a\":1,\"b\":2,\"c\":3}";
        cjsonx_doc_t* doc = cjsonx_parse_with_buffer(json, strlen(json), s_buf, sizeof(s_buf));
        CHECK(doc != NULL && doc->is_valid, "second parse into fresh buffer is valid");
        CHECK(cjsonx_get_type(doc->root) == CJSONX_OBJECT, "root is object");
        CHECK(cjsonx_size(doc->root) == 3, "object has 3 key-value pairs");
        // verify iteration
        size_t count = 0;
        cjsonx_iter_t it = cjsonx_iter_init(doc->root);
        while (cjsonx_iter_next(&it)) count++;
        CHECK(count == 3, "iterator yields 3 pairs");
        cjsonx_doc_free(doc);
    }

    // test invalid json
    memset(s_buf, 0, sizeof(s_buf));
    {
        const char* json = "{invalid}";
        cjsonx_doc_t* doc = cjsonx_parse_with_buffer(json, strlen(json), s_buf, sizeof(s_buf));
        CHECK(doc != NULL, "parse_with_buffer returns doc even on invalid json");
        CHECK(!doc->is_valid, "invalid json gives invalid doc");
        cjsonx_doc_free(doc);
    }

    // test empty input
    memset(s_buf, 0, sizeof(s_buf));
    {
        const char* json = "   ";
        cjsonx_doc_t* doc = cjsonx_parse_with_buffer(json, strlen(json), s_buf, sizeof(s_buf));
        CHECK(doc != NULL, "parse_with_buffer returns doc on whitespace-only input");
        CHECK(!doc->is_valid, "whitespace-only gives invalid doc");
        CHECK(doc->error == CJSONX_ERROR_EMPTY_INPUT, "error is empty input");
        cjsonx_doc_free(doc);
    }

    // test nested object
    memset(s_buf, 0, sizeof(s_buf));
    {
        const char* json = "{\"x\":{\"y\":42}}";
        cjsonx_doc_t* doc = cjsonx_parse_with_buffer(json, strlen(json), s_buf, sizeof(s_buf));
        CHECK(doc != NULL && doc->is_valid, "nested object parse is valid");
        cjsonx_val_t x = cjsonx_get(doc->root, "x");
        CHECK(cjsonx_get_type(x) == CJSONX_OBJECT, "x is object");
        cjsonx_val_t y = cjsonx_get(x, "y");
        CHECK((int)cjsonx_num(y) == 42, "y == 42");
        cjsonx_doc_free(doc);
    }

    printf("\nall static buffer tests passed!\n");
    return 0;
}
