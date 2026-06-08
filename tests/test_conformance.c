// test_conformance.c — json test suite conformance runner.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "cjsonx.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <path_to_test_parsing_dir>\n", argv[0]);
        return 1;
    }
    
    DIR* dir = opendir(argv[1]);
    if (!dir) {
        perror("opendir");
        return 1;
    }
    
    struct dirent* ent;
    int total = 0, passed = 0, failed = 0, i_passed = 0, i_failed = 0;
    
    while ((ent = readdir(dir)) != NULL) {
        if (strstr(ent->d_name, ".json") == NULL) continue;
        
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", argv[1], ent->d_name);
        
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* buf = (char*)malloc(len + 1);
        fread(buf, 1, len, f);
        buf[len] = '\0';
        fclose(f);
        
        cjsonx_doc* doc = cjsonx_parse(buf, len);
        bool success = (doc && doc->is_valid);
        
        char type = ent->d_name[0];
        
        if (type == 'y') {
            total++;
            if (success) passed++;
            else { failed++; printf("[FAIL y] Expected success, got error: %s\n", ent->d_name); }
        } else if (type == 'n') {
            total++;
            if (!success) passed++;
            else { failed++; printf("[FAIL n] Expected error, got success: %s\n", ent->d_name); }
        } else if (type == 'i') {
            if (success) i_passed++;
            else i_failed++;
        }
        
        if (doc) cjsonx_doc_free(doc);
        free(buf);
    }
    closedir(dir);
    
    printf("\n--- Conformance Results ---\n");
    printf("Strict Tests (y/n): %d total\n", total);
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("\nImplementation Defined (i): %d parsed, %d rejected\n", i_passed, i_failed);
    
    return failed == 0 ? 0 : 1;
}
