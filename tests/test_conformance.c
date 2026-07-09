// test_conformance.c — json test suite conformance runner.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#include "cjsonx.h"

static void run_test_file(const char* dir_path, const char* file_name, int* total, int* passed,
                          int* failed, int* i_passed, int* i_failed) {
    if (strstr(file_name, ".json") == NULL) return;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir_path, file_name);

    FILE* f = fopen(path, "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    size_t read_bytes = fread(buf, 1, len, f);
    buf[read_bytes] = '\0';
    fclose(f);

    cjsonx_doc* doc = cjsonx_parse(buf, len);
    bool success = (doc && doc->is_valid);

    char type = file_name[0];

    if (type == 'y') {
        (*total)++;
        if (success) {
            (*passed)++;
        } else {
            (*failed)++;
            printf("[FAIL y] Expected success, got error: %s\n", file_name);
        }
    } else if (type == 'n') {
        (*total)++;
        if (!success) {
            (*passed)++;
        } else {
            (*failed)++;
            printf("[FAIL n] Expected error, got success: %s\n", file_name);
        }
    } else if (type == 'i') {
        if (success)
            (*i_passed)++;
        else
            (*i_failed)++;
    }

    if (doc) cjsonx_doc_free(doc);
    free(buf);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <path_to_test_parsing_dir>\n", argv[0]);
        return 1;
    }

    int total = 0, passed = 0, failed = 0, i_passed = 0, i_failed = 0;

#ifdef _WIN32
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*.json", argv[1]);
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open directory: %s\n", argv[1]);
        return 1;
    }
    do {
        run_test_file(argv[1], find_data.cFileName, &total, &passed, &failed, &i_passed, &i_failed);
    } while (FindNextFileA(hFind, &find_data));
    FindClose(hFind);
#else
    DIR* dir = opendir(argv[1]);
    if (!dir) {
        perror("opendir");
        return 1;
    }
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        run_test_file(argv[1], ent->d_name, &total, &passed, &failed, &i_passed, &i_failed);
    }
    closedir(dir);
#endif

    printf("\n--- Conformance Results ---\n");
    printf("Strict Tests (y/n): %d total\n", total);
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("\nImplementation Defined (i): %d parsed, %d rejected\n", i_passed, i_failed);

    return failed == 0 ? 0 : 1;
}
