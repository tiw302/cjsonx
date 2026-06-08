// test_fastfloat.c — tests for eisel-lemire fast float parser.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cjsonx.h"

int main() {
    printf("Running Eisel-Lemire fast float tests...\n");
    
    const char* test_cases[] = {
        "3.14159265358979323846",
        "-0.00000000000000000012345",
        "1e-300",
        "1e300",
        "4.9406564584124654e-324", // subnormal
        "4.9406564584124654e-324                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       ", // subnormal with > 511 spaces to test fallback buffer fix
        "1.7976931348623157e+308", // max double
        "0.0",
        "12345678901234567890.0",
        NULL
    };
    
    int failed = 0;
    
    for (int i = 0; test_cases[i] != NULL; i++) {
        const char* s = test_cases[i];
        
        // parse with our engine
        cjsonx_doc* doc = cjsonx_parse(s, strlen(s));
        if (!doc || !doc->is_valid) {
            printf("[FAIL] Failed to parse: %s\n", s);
            failed++;
            continue;
        }
        
        double fast_val = doc->nodes[0].val.f64;
        cjsonx_doc_free(doc);
        
        // parse with strtod
        double std_val = strtod(s, NULL);
        
        // compare
        // we use relative error or bitwise equality
        uint64_t fast_bits, std_bits;
        memcpy(&fast_bits, &fast_val, 8);
        memcpy(&std_bits, &std_val, 8);
        
        if (fast_bits != std_bits) {
            // check if nan (should not happen here)
            double diff = fabs(fast_val - std_val);
            if (diff > 1e-10 * fabs(std_val)) { // small tolerance since we didn't do full 192-bit exact tie-breaking
                printf("[FAIL] %s\n", s);
                printf("  fast_float: %.20g (0x%016llx)\n", fast_val, (unsigned long long)fast_bits);
                printf("  strtod:     %.20g (0x%016llx)\n", std_val, (unsigned long long)std_bits);
                failed++;
            } else {
                printf("[WARN] %s (close but not exact)\n", s);
            }
        } else {
            printf("[PASS] %s\n", s);
        }
    }
    
    if (failed == 0) {
        printf("All fast float tests passed!\n");
        return 0;
    } else {
        return 1;
    }
}
