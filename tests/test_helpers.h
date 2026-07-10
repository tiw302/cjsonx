/*
 * test_helpers.h - shared utility header for the c test suite.
 * provides common helper functions (like reading files into memory)
 * to keep test binaries dry and reduce boilerplate.
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>

/* shared file-reading helper used by all test binaries (dry) */
static inline char* test_read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, len, f);
    fclose(f);

    if (read_bytes != (size_t)len) {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    *out_len = (size_t)len;
    return buf;
}

#endif  // test_helpers_h
