// test_tier4.c -- Tier 4: Large scale testing (10M, 100M)
#define _POSIX_C_SOURCE 199309L
#include "../src/include/sublimation.h"
#include "verify.h"
#include <assert.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void test_large(size_t n, const char *label) {
    char name[80];
    snprintf(name, sizeof(name), "large_%s_%zuM", label, n / 1000000);

    int64_t *arr = (int64_t *)malloc(n * sizeof(int64_t));
    if (!arr) {
        printf("  %-50s SKIP (OOM)\n", name);
        return;
    }

    verify_fill_random(arr, n, 42 + n);
    int64_t *saved = verify_save(arr, n);
    if (!saved) {
        printf("  %-50s SKIP (OOM for copy)\n", name);
        free(arr);
        return;
    }

    double t0 = now_sec();
    sublimation_i64(arr, n);
    double t1 = now_sec();

    double ns_per = (t1 - t0) * 1e9 / (double)n;

    int ok = verify_sort(arr, saved, n, name);
    if (ok) {
        printf("    (%.1f ns/elem, %.3f sec)\n", ns_per, t1 - t0);
    }

    free(saved);
    free(arr);
}

int main(void) {
    printf("[sublimation] Tier 4: large scale testing\n\n");

    test_large(10000000, "random");   // 10M
    test_large(100000000, "random");  // 100M

    return verify_summary();
}
