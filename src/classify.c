// classify.c -- Initial BFS: input classification and disorder profiling
//
// Type-generic via macro template instantiation.
// Each inclusion of classify_impl.h generates a full set of typed functions.
#include "internal/sort_internal.h"
#include <stdlib.h>
#include <math.h>

// int32_t
#define SUB_TYPE int32_t
#define SUB_SUFFIX _i32
#include "classify_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// int64_t
#define SUB_TYPE int64_t
#define SUB_SUFFIX _i64
#include "classify_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// uint32_t
#define SUB_TYPE uint32_t
#define SUB_SUFFIX _u32
#include "classify_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// uint64_t
#define SUB_TYPE uint64_t
#define SUB_SUFFIX _u64
#include "classify_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// float
#define SUB_TYPE float
#define SUB_SUFFIX _f32
#include "classify_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// double
#define SUB_TYPE double
#define SUB_SUFFIX _f64
#include "classify_impl.h"
#undef SUB_TYPE
#undef SUB_SUFFIX

// Public API wrapper (i64 only for now)
sub_profile_t sublimation_classify_i64(const int64_t *arr, size_t n) {
    return sub_classify_internal_i64(arr, n);
}
