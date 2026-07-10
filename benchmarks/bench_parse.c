/*
 * file: bench_parse.c
 * description: isolated benchmarking tool for measuring cjsonx parsing throughput.
 *              includes cpu cache warmup and dynamic iteration scaling for fairness.
 * 
 * how to compile (via cmake):
 *   cmake -S .. -B ../build && cmake --build ../build
 *   ../build/bench_parse datasets/twitter.json
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../tests/test_helpers.h"
#include "cjsonx.h"

// get time in seconds
double get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
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

    printf("benchmarking file: %s (%.2f mb)\n", file_path, (double)len / (1024 * 1024));

    // parse once to warm up cpu caches and prevent cold start bias
    cjsonx_doc* doc = cjsonx_parse(json, len);
    if (!doc) {
        fprintf(stderr, "failed to parse json!\n");
        free(json);
        return 1;
    }
    cjsonx_doc_free(doc);

    // adjust iterations based on size to keep runtime sensible
    int iterations = 1000;
    if (len > 1024 * 1024) iterations = 200;
    if (len > 5 * 1024 * 1024) iterations = 50;

    // run the primary benchmarking loop
    double start_time = get_time();
    for (int i = 0; i < iterations; i++) {
        doc = cjsonx_parse(json, len);
        cjsonx_doc_free(doc);
    }

    double end_time = get_time();
    double total_time = end_time - start_time;
    double avg_time = total_time / iterations;

    // calculate throughput in mb/s and gb/s
    double bytes_per_sec = (double)len / avg_time;
    double mb_per_sec = bytes_per_sec / (1024 * 1024);
    double gb_per_sec = mb_per_sec / 1024;

    printf("iterations: %d\n", iterations);
    printf("total time: %.4f s\n", total_time);
    printf("avg time per parse: %.4f ms\n", avg_time * 1000.0);
    printf("speed: %.2f mb/s (%.3f gb/s)\n", mb_per_sec, gb_per_sec);

    free(json);
    return 0;
}
