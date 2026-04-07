// sublimation.h -- Public API for libsublimation
//
// Flow-model adaptive sorting. C23 core with Python, Rust, Go interfaces.
// In-place. No allocation. Caller owns the buffer.
#ifndef SUBLIMATION_H
#define SUBLIMATION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "internal/c23_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

// API version
#define SUBLIMATION_API_VERSION 1

// Disorder classification (result of the initial BFS)
typedef enum {
    SUB_SORTED       = 0,   // already sorted, O(n) verified
    SUB_REVERSED     = 1,   // fully reversed, O(n) reverse
    SUB_NEARLY_SORTED = 2,  // low displacement, merge-based
    SUB_FEW_UNIQUE   = 3,   // small distinct count, partition-based
    SUB_RANDOM       = 4,   // no exploitable structure
    SUB_PHASED       = 5,   // phase boundary detected (sorted + random)
    SUB_SPECTRAL     = 6,   // spectral seriation path was used
} sub_disorder_t;

// Classification profile (the initial level graph)
typedef struct {
    size_t n;                    // array size
    size_t run_count;            // ascending runs
    size_t mono_count;           // monotone runs (ascending + descending)
    size_t max_run_len;          // longest natural run
    int64_t max_descent_gap;     // max value gap at descents (arr[i] - arr[i+1] where arr[i] > arr[i+1])
    size_t lis_length;           // longest increasing subsequence (patience sort)
    size_t lds_length;           // longest decreasing subsequence (= number of tableau rows)
    size_t tableau_num_rows;     // same as lds_length (alias for clarity)
    float  info_theoretic_bound; // log2(f^lambda) via hook length formula (0 if not computed)
    size_t interleave_k;         // detected k-interleaved sorted sequences (0 if none)
    size_t distinct_estimate;    // approximate distinct values
    float  inversion_ratio;      // estimated inversion fraction (sampled)
    size_t phase_boundary;       // index of detected regime change (0 = none)
    size_t rotation_point;       // rotation point for rotated sorted arrays (0 = none)
    float  spectral_gap;         // lambda_2 of comparison graph (0 if not computed)
    float  spectral_gap_ratio;   // lambda_2 / lambda_n (0 if not computed)
    sub_disorder_t disorder;      // overall classification
} sub_profile_t;

// Sort statistics (returned from sort calls)
typedef struct {
    uint64_t comparisons;        // total comparisons performed
    uint64_t swaps;              // total element swaps
    uint64_t levels_built;       // level graph constructions
    uint64_t gap_prunes;         // empty-region bulk reclassifications
    uint64_t rescans;            // full classification rescans
    uint64_t spectral_decompositions; // eigendecompositions performed
    double   spectral_gap;       // last spectral gap observed
    double   info_theoretic_bound;  // minimum comparisons needed (from hook length formula)
    double   comparison_efficiency;  // info_bound / actual_comparisons (1.0 = perfect)
    sub_disorder_t disorder;      // detected disorder class
    double   wall_ns;            // wall-clock nanoseconds
} sub_stats_t;

// Generic sort (qsort-compatible interface, no stats)
SUB_API void sublimation(void *base, size_t nmemb, size_t size,
                         int (*compar)(const void *, const void *));

// Type-specific fast paths (no function pointer overhead, no indirection)
SUB_API void sublimation_i32(int32_t *SUB_RESTRICT arr, size_t n);
SUB_API void sublimation_i64(int64_t *SUB_RESTRICT arr, size_t n);
SUB_API void sublimation_u32(uint32_t *SUB_RESTRICT arr, size_t n);
SUB_API void sublimation_u64(uint64_t *SUB_RESTRICT arr, size_t n);
SUB_API void sublimation_f32(float *SUB_RESTRICT arr, size_t n);
SUB_API void sublimation_f64(double *SUB_RESTRICT arr, size_t n);

// Type-specific with stats
SUB_API void sublimation_i64_stats(int64_t *SUB_RESTRICT arr, size_t n, sub_stats_t *stats);

// Classification only (inspect without sorting)
SUB_NODISCARD SUB_API SUB_PURE
sub_profile_t sublimation_classify_i64(const int64_t *arr, size_t n);

// _Generic dispatch macro: sublimation_typed(arr, n)
// Routes to the correct type-specific function at compile time.
#define sublimation_typed(arr, n) _Generic((arr),        \
    int32_t *:  sublimation_i32,                         \
    int64_t *:  sublimation_i64,                         \
    uint32_t *: sublimation_u32,                         \
    uint64_t *: sublimation_u64,                         \
    float *:    sublimation_f32,                          \
    double *:   sublimation_f64                           \
)(arr, n)

// Parallel sort (explicit thread count)
SUB_API void sublimation_i64_parallel(int64_t *SUB_RESTRICT arr, size_t n, size_t num_threads);

// Version query
SUB_API SUB_CONST int sublimation_api_version(void);

#ifdef __cplusplus
}
#endif

#endif // SUBLIMATION_H
