#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../src/include/sublimation.h"

static int cmp_i64(const void *a, const void *b) {
    int64_t va = *(const int64_t *)a, vb = *(const int64_t *)b;
    return (va > vb) - (va < vb);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    size_t n = size / sizeof(int64_t);
    if (n < 1 || n > 100000) return 0;

    int64_t *a = malloc(n * sizeof(int64_t));
    int64_t *b = malloc(n * sizeof(int64_t));
    if (!a || !b) { free(a); free(b); return 0; }

    memcpy(a, data, n * sizeof(int64_t));
    memcpy(b, data, n * sizeof(int64_t));

    sublimation_i64(a, n);
    qsort(b, n, sizeof(int64_t), cmp_i64);

    // Must produce identical output
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) __builtin_trap();
    }

    free(a);
    free(b);
    return 0;
}
