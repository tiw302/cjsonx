#include "cjsonx.h"

#include "third_party/yyjson.h"
#include "third_party/cJSON.h"

#define JSMN_STATIC
#include "third_party/jsmn.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "../tests/test_helpers.h"

// ---------------------------------------------------------
// memory tracking allocator
// ---------------------------------------------------------
static size_t g_mem = 0;
static size_t g_mem_peak = 0;

static void reset_mem(void) {
    g_mem = 0;
    g_mem_peak = 0;
}

static void* track_malloc(size_t size) {
    g_mem += size;
    if (g_mem > g_mem_peak) g_mem_peak = g_mem;
    size_t* ptr = (size_t*)malloc(size + sizeof(size_t));
    *ptr = size;
    return ptr + 1;
}
static void* track_realloc(void* ptr, size_t size) {
    if (!ptr) return track_malloc(size);
    size_t* old = (size_t*)ptr - 1;
    g_mem -= *old;
    g_mem += size;
    if (g_mem > g_mem_peak) g_mem_peak = g_mem;
    size_t* new_ptr = (size_t*)realloc(old, size + sizeof(size_t));
    *new_ptr = size;
    return new_ptr + 1;
}
static void track_free(void* ptr) {
    if (!ptr) return;
    size_t* old = (size_t*)ptr - 1;
    g_mem -= *old;
    free(old);
}

// cjsonx wrappers
static void* cx_malloc(size_t size, void* user) { (void)user; return track_malloc(size); }
static void* cx_realloc(void* ptr, size_t size, void* user) { (void)user; return track_realloc(ptr, size); }
static void cx_free(void* ptr, void* user) { (void)user; track_free(ptr); }

// yyjson wrappers
static void* yy_malloc(void* ctx, size_t size) { (void)ctx; return track_malloc(size); }
static void* yy_realloc(void* ctx, void* ptr, size_t old, size_t size) { (void)ctx; (void)old; return track_realloc(ptr, size); }
static void yy_free(void* ctx, void* ptr) { (void)ctx; track_free(ptr); }

// cjson wrappers
static void* cj_malloc(size_t size) { return track_malloc(size); }
static void cj_free(void* ptr) { track_free(ptr); }

// ---------------------------------------------------------

// get time in seconds
static double get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void bench_cjsonx(const char* json, size_t len, int iterations, double* out_parse_time, double* out_stringify_time, double* out_mem_mb) {
    cjsonx_alc alloc = { cx_malloc, cx_realloc, cx_free, NULL };
    
    // memory measurement
    reset_mem();
    cjsonx_doc* doc = cjsonx_parse_ex(json, len, &alloc);
    *out_mem_mb = (double)g_mem_peak / (1024 * 1024);
    cjsonx_doc_free(doc);

    // parse speed
    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        doc = cjsonx_parse(json, len);
        cjsonx_doc_free(doc);
    }
    double end = get_time();
    *out_parse_time = (end - start) * 1000.0 / iterations;
    
    // stringify speed
    doc = cjsonx_parse(json, len);
    start = get_time();
    for (int i = 0; i < iterations; i++) {
        char* s = cjsonx_stringify(doc);
        free(s);
    }
    end = get_time();
    *out_stringify_time = (end - start) * 1000.0 / iterations;
    cjsonx_doc_free(doc);
}

void bench_yyjson(const char* json, size_t len, int iterations, double* out_parse_time, double* out_stringify_time, double* out_mem_mb) {
    yyjson_alc alc = { yy_malloc, yy_realloc, yy_free, NULL };
    
    // memory measurement
    reset_mem();
    yyjson_doc* doc = yyjson_read_opts((char*)json, len, 0, &alc, NULL);
    *out_mem_mb = (double)g_mem_peak / (1024 * 1024);
    yyjson_doc_free(doc);

    // parse speed
    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        doc = yyjson_read(json, len, 0);
        yyjson_doc_free(doc);
    }
    double end = get_time();
    *out_parse_time = (end - start) * 1000.0 / iterations;
    
    // stringify speed
    doc = yyjson_read(json, len, 0);
    start = get_time();
    for (int i = 0; i < iterations; i++) {
        char* s = yyjson_write(doc, 0, NULL);
        free(s);
    }
    end = get_time();
    *out_stringify_time = (end - start) * 1000.0 / iterations;
    yyjson_doc_free(doc);
}

void bench_cjson(const char* json, size_t len, int iterations, double* out_parse_time, double* out_stringify_time, double* out_mem_mb) {
    (void)len;
    cJSON_Hooks hooks = { cj_malloc, cj_free };
    cJSON_InitHooks(&hooks);
    
    // memory measurement
    reset_mem();
    cJSON* doc = cJSON_Parse(json);
    *out_mem_mb = (double)g_mem_peak / (1024 * 1024);
    cJSON_Delete(doc);

    // parse speed (restore default hooks so speed test is fair)
    cJSON_InitHooks(NULL);
    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        doc = cJSON_Parse(json);
        cJSON_Delete(doc);
    }
    double end = get_time();
    *out_parse_time = (end - start) * 1000.0 / iterations;
    
    // stringify speed
    doc = cJSON_Parse(json);
    start = get_time();
    for (int i = 0; i < iterations; i++) {
        char* s = cJSON_PrintUnformatted(doc);
        free(s);
    }
    end = get_time();
    *out_stringify_time = (end - start) * 1000.0 / iterations;
    cJSON_Delete(doc);
}

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

    printf("Dataset: %s (%.2f MB)\n", file_path, (double)len / (1024 * 1024));
    printf("========================================================================\n");
    printf("%-10s | %-15s | %-15s | %-15s\n", "Library", "Parse (MB/s)", "Stringify (MB/s)", "Peak Mem (MB)");
    printf("-----------|-----------------|------------------|-----------------------\n");

    // scale down iterations for larger files to keep benchmark runtime reasonable
    int iterations = 1000;
    if (len > 1024 * 1024) iterations = 200;
    if (len > 5 * 1024 * 1024) iterations = 50;

    double p_time, s_time, mem;
    double file_mb = (double)len / (1024 * 1024);

    bench_cjsonx(json, len, iterations, &p_time, &s_time, &mem);
    printf("%-10s | %-15.2f | %-15.2f | %-15.2f\n", "cjsonx", file_mb / (p_time / 1000.0), file_mb / (s_time / 1000.0), mem);

    bench_yyjson(json, len, iterations, &p_time, &s_time, &mem);
    printf("%-10s | %-15.2f | %-15.2f | %-15.2f\n", "yyjson", file_mb / (p_time / 1000.0), file_mb / (s_time / 1000.0), mem);

    bench_cjson(json, len, iterations / 10 > 0 ? iterations / 10 : 1, &p_time, &s_time, &mem);
    printf("%-10s | %-15.2f | %-15.2f | %-15.2f\n", "cJSON", file_mb / (p_time / 1000.0), file_mb / (s_time / 1000.0), mem);

    printf("========================================================================\n\n");

    free(json);
    return 0;
}
