// test_tier5.c -- Tier 5: Spectral verification, oscillator convergence,
//                         deterministic reproducibility
#define _POSIX_C_SOURCE 199309L
#include "../src/include/sublimation.h"
#include "verify.h"
#include <assert.h>
#include <math.h>

// SPECTRAL NUMERICAL VERIFICATION (#30)
// Verify eigendecomposition properties on known matrices.

static void test_spectral_properties(void) {
    // sort 128 nearly-sorted elements (triggers spectral merge with 3-16 runs)
    // then verify the output is correct (implicitly tests eigendecomposition)
    {
        int64_t arr[128];
        for (size_t i = 0; i < 128; i++) arr[i] = (int64_t)i;
        // introduce a few swaps to create runs
        arr[30] = 50; arr[50] = 30;
        arr[60] = 80; arr[80] = 60;
        VERIFY_SORT(arr, 128, sublimation_i64, "spectral_nearly_sorted_128");
    }
    // pipe organ (2 runs, spectral merge handles)
    {
        int64_t arr[200];
        for (size_t i = 0; i < 100; i++) arr[i] = (int64_t)i;
        for (size_t i = 100; i < 200; i++) arr[i] = (int64_t)(200 - i);
        VERIFY_SORT(arr, 200, sublimation_i64, "spectral_pipe_organ_200");
    }
    // 3 runs (minimal spectral bisection case)
    {
        int64_t arr[300];
        for (size_t i = 0; i < 100; i++) arr[i] = (int64_t)(100 + i);    // 100-199
        for (size_t i = 100; i < 200; i++) arr[i] = (int64_t)(i - 100);   // 0-99
        for (size_t i = 200; i < 300; i++) arr[i] = (int64_t)(200 + i - 200); // 200-299
        VERIFY_SORT(arr, 300, sublimation_i64, "spectral_3_runs_300");
    }
}

// OSCILLATOR CONVERGENCE (#31)
// Run patterns that repeatedly trigger/relax CUSUM.
// Verify sort still produces correct output (oscillator doesn't diverge).

static void test_oscillator_convergence(void) {
    // alternating sorted/random segments (stresses CUSUM + oscillator)
    {
        int64_t arr[10000];
        uint64_t seed = 0xF00D;
        for (size_t i = 0; i < 10000; i++) {
            if ((i / 100) % 2 == 0) {
                arr[i] = (int64_t)i; // sorted segment
            } else {
                seed = seed * 6364136223846793005ull + 1442695040888963407ull;
                arr[i] = (int64_t)(seed >> 16); // random segment
            }
        }
        VERIFY_SORT(arr, 10000, sublimation_i64, "oscillator_alternating_10K");
    }
    // increasingly adversarial: 10 rounds of sort + corrupt
    {
        int64_t arr[5000];
        for (size_t i = 0; i < 5000; i++) arr[i] = (int64_t)i;
        uint64_t seed = 0xBEEF;
        for (int round = 0; round < 10; round++) {
            // corrupt a random 10% of elements
            for (size_t k = 0; k < 500; k++) {
                seed = seed * 6364136223846793005ull + 1442695040888963407ull;
                size_t idx = (size_t)(seed >> 33) % 5000;
                seed = seed * 6364136223846793005ull + 1442695040888963407ull;
                arr[idx] = (int64_t)(seed >> 16);
            }
            int64_t *saved = verify_save(arr, 5000);
            sublimation_i64(arr, 5000);
            verify_sort(arr, saved, 5000, "oscillator_repeated_corrupt");
            free(saved);
        }
    }
}

// DETERMINISTIC REPRODUCIBILITY (#33)
// Same seed -> same input -> same output -> same checksum.

static uint64_t checksum(const int64_t *arr, size_t n) {
    uint64_t h = 0;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint64_t)arr[i] * 0x9E3779B97F4A7C15ull;
        h = (h << 13) | (h >> 51);
    }
    return h;
}

static void test_reproducibility(void) {
    size_t n = 100000;
    uint64_t checksums[5];

    for (int trial = 0; trial < 5; trial++) {
        int64_t *arr = (int64_t *)malloc(n * sizeof(int64_t));
        assert(arr);
        verify_fill_random(arr, n, 314159); // same seed every trial
        sublimation_i64(arr, n);
        checksums[trial] = checksum(arr, n);
        free(arr);
    }

    int ok = 1;
    for (int i = 1; i < 5; i++) {
        if (checksums[i] != checksums[0]) {
            fprintf(stderr, "  [FAIL] reproducibility: trial %d checksum %lx != trial 0 %lx\n",
                    i, (unsigned long)checksums[i], (unsigned long)checksums[0]);
            _verify_fail++;
            ok = 0;
        }
    }
    if (ok) {
        printf("  %-50s PASS (5 trials, checksum %lx)\n",
               "deterministic_100K", (unsigned long)checksums[0]);
        _verify_pass++;
    }

    // also test parallel path reproducibility
    for (int trial = 0; trial < 5; trial++) {
        int64_t *arr = (int64_t *)malloc(n * sizeof(int64_t));
        assert(arr);
        verify_fill_random(arr, n, 314159);
        sublimation_i64_parallel(arr, n, 4);
        checksums[trial] = checksum(arr, n);
        free(arr);
    }

    ok = 1;
    for (int i = 1; i < 5; i++) {
        if (checksums[i] != checksums[0]) {
            fprintf(stderr, "  [FAIL] parallel reproducibility: trial %d checksum %lx != trial 0 %lx\n",
                    i, (unsigned long)checksums[i], (unsigned long)checksums[0]);
            _verify_fail++;
            ok = 0;
        }
    }
    if (ok) {
        printf("  %-50s PASS (5 trials, checksum %lx)\n",
               "deterministic_parallel_100K", (unsigned long)checksums[0]);
        _verify_pass++;
    }
}

int main(void) {
    printf("[sublimation] Tier 5: spectral + oscillator + reproducibility\n\n");

    printf("  --- Spectral Properties ---\n");
    test_spectral_properties();

    printf("\n  --- Oscillator Convergence ---\n");
    test_oscillator_convergence();

    printf("\n  --- Deterministic Reproducibility ---\n");
    test_reproducibility();

    return verify_summary();
}
