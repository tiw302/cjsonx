// micro-benchmark to compare switch-case vs computed goto (labels as values) performance
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define N 100000000
char data[N];

// test performance using standard switch statement
uint64_t test_switch() {
    uint64_t sum = 0;
    for(int i=0; i<N; i++) {
        switch(data[i]) {
            case '0': sum += 1; break;
            case '1': sum += 2; break;
            case '2': sum += 3; break;
            case '3': sum += 4; break;
        }
    }
    return sum;
}

// test performance using direct threading with computed gotos
uint64_t test_goto() {
    uint64_t sum = 0;
    static const void* table[256] = { [0 ... 255] = &&L_END, ['0']=&&L_0, ['1']=&&L_1, ['2']=&&L_2, ['3']=&&L_3 };
    int i = 0;
    goto *table[(uint8_t)data[i]];
L_0: sum += 1; i++; if(i<N) goto *table[(uint8_t)data[i]]; else goto L_END;
L_1: sum += 2; i++; if(i<N) goto *table[(uint8_t)data[i]]; else goto L_END;
L_2: sum += 3; i++; if(i<N) goto *table[(uint8_t)data[i]]; else goto L_END;
L_3: sum += 4; i++; if(i<N) goto *table[(uint8_t)data[i]]; else goto L_END;
L_END: return sum;
}

int main() {
    for(int i=0; i<N; i++) data[i] = '0' + (rand() % 4);
    clock_t s1 = clock();
    uint64_t a = test_switch();
    clock_t e1 = clock();
    printf("Switch: %f, ans=%lu\n", (double)(e1-s1)/CLOCKS_PER_SEC, a);

    clock_t s2 = clock();
    uint64_t b = test_goto();
    clock_t e2 = clock();
    printf("Goto: %f, ans=%lu\n", (double)(e2-s2)/CLOCKS_PER_SEC, b);
    return 0;
}
