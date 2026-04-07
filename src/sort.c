#define _POSIX_C_SOURCE 199309L
// sort.c -- Flow-model sort entry point
//
// Type-generic via macro template instantiation.
// Each inclusion of sort_impl.h generates a full set of typed functions.
#include "internal/sort_internal.h"
#include "internal/pool.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Wall-clock nanoseconds (POSIX)
static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

// ============================================================================
// TYPE-GENERIC SORT (all types via template)
// ============================================================================

// int32_t
#define SUB_TYPE int32_t
#define SUB_SUFFIX _i32
#include "sort_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// int64_t (with parallel + spectral fallback)
#define SUB_TYPE int64_t
#define SUB_SUFFIX _i64
#define SUB_TYPE_IS_I64
#include "sort_impl.h"
#undef SUB_TYPE_IS_I64
#undef SUB_TYPE
#undef SUB_SUFFIX

// uint32_t
#define SUB_TYPE uint32_t
#define SUB_SUFFIX _u32
#include "sort_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// uint64_t
#define SUB_TYPE uint64_t
#define SUB_SUFFIX _u64
#include "sort_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// float
#define SUB_TYPE float
#define SUB_SUFFIX _f32
#include "sort_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// double
#define SUB_TYPE double
#define SUB_SUFFIX _f64
#include "sort_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// ============================================================================
// PUBLIC API: parallel sort (i64 only)
// ============================================================================

void sublimation_i64_parallel(int64_t *restrict arr, size_t n, size_t num_threads) {
    if (n <= 1) return;

    // Counting sort: O(n) for k <= 64
    {
        uint64_t cmp = 0, swp = 0;
        if (counting_sort_few_unique_i64(arr, n, &cmp, &swp)) return;
    }

    sub_profile_t profile = sub_classify_internal_i64(arr, n);

    if (profile.disorder == SUB_SORTED) return;
    if (profile.disorder == SUB_REVERSED) {
        size_t lo = 0, hi = n - 1;
        while (lo < hi) { SUB_SWAP(int64_t, arr[lo], arr[hi]); lo++; hi--; }
        return;
    }
    if (profile.disorder == SUB_NEARLY_SORTED) {
        // Rotated sorted array: O(n) fix
        if (profile.rotation_point > 0) {
            uint64_t swp = 0;
            fix_rotation_i64(arr, n, profile.rotation_point, &swp);
            return;
        }
        if (profile.run_count <= 16) {
            uint64_t cmp = 0;
            sub_spectral_merge_i64(arr, n, &cmp);
        } else {
            size_t sqrt_n = 1;
            while (sqrt_n * sqrt_n < n) sqrt_n++;
            if (profile.max_descent_gap <= (int64_t)sqrt_n) {
                binary_isort_i64(arr, n);
            } else {
                light_sort_i64(arr, n);
            }
        }
        return;
    }

    if (num_threads < 2) {
        sub_adaptive_t state;
        sub_adaptive_init(&state, n);
        sub_sort_internal_i64(arr, n, &state);
        return;
    }

    sub_parallel_sort_i64(arr, n, num_threads, (int)profile.disorder);
}

// ============================================================================
// PUBLIC API: i64 with stats
// ============================================================================

void sublimation_i64_stats(int64_t *restrict arr, size_t n, sub_stats_t *stats) {
    uint64_t t0 = now_ns();

    sub_adaptive_t state;
    sub_adaptive_init(&state, n);

    sub_profile_t profile = sub_classify_internal_i64(arr, n);
    sub_sort_internal_i64(arr, n, &state);

    uint64_t t1 = now_ns();

    if (stats) {
        stats->comparisons = state.comparisons;
        stats->swaps = state.swaps;
        stats->levels_built = state.levels_built;
        stats->gap_prunes = state.gap_prunes;
        stats->rescans = state.rescans;
        stats->spectral_decompositions = state.spectral_attempted ? 1 : 0;
        stats->spectral_gap = (double)state.last_spectral_gap;
        stats->info_theoretic_bound = (double)profile.info_theoretic_bound;
        stats->comparison_efficiency = 0.0;
        if (stats->comparisons > 0 && stats->info_theoretic_bound > 0.0) {
            stats->comparison_efficiency = stats->info_theoretic_bound / (double)stats->comparisons;
        }
        stats->disorder = profile.disorder;
        stats->wall_ns = (double)(t1 - t0);
    }
}

// ============================================================================
// PUBLIC API: generic qsort-compatible
// ============================================================================

void sublimation(void *base, size_t nmemb, size_t size,
               int (*compar)(const void *, const void *)) {
    qsort(base, nmemb, size, compar);
}

// ============================================================================
// PUBLIC API: version
// ============================================================================

int sublimation_api_version(void) {
    return SUBLIMATION_API_VERSION;
}
