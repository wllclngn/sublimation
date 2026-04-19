// sublimation_pack.h -- index sort by companion numeric key array
//
// Sorts a caller-owned `indices` array by the values in a parallel `keys`
// array. Output: `indices[i]` is the original position of the i-th smallest
// key (or largest, when `descending` is true). The `keys` array is
// untouched. Uses the numeric pipeline (sublimation_u64) under the hood by
// packing (monotonic_key << 32) | index into uint64 and gathering.
//
// Typical caller pattern (montauk ProcessTable sort by CPU%):
//
//     float cpu[n];       // key per row
//     uint32_t idx[n];
//     for (size_t i = 0; i < n; i++) idx[i] = (uint32_t)i;
//     sublimation_pack_sort_f32(cpu, idx, n, true /* descending */);
//     // idx now holds the draw order; iterate idx[i] to visit rows in order.
//
// Stability: STABLE for equal keys. The pack (key<<32)|index keeps equal
// keys in ascending original-index order. Descending flips the key bits
// (keeps the tiebreak consistent) so both directions are stable.
//
// Capacity: n must be < 2^32. Larger n is undefined (the pack loses bits).
// The numeric entry point does not check; callers are responsible.
#ifndef SUBLIMATION_PACK_H
#define SUBLIMATION_PACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "internal/c23_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

// Monotonic key-flip inlines. Each maps its key type to a uint32_t such
// that sorting the uint32s ascending produces the same order as sorting
// the keys ascending in their native type.
//
// u32: identity.
// i32: flip sign bit -- INT_MIN maps to 0, INT_MAX maps to UINT_MAX.
// f32: flip sign bit for positives; flip all bits for negatives. Handles
//      -0 as equal to +0 in the resulting order; NaN handling is IEEE
//      undefined (caller should filter NaN before sorting).
static inline uint32_t sub_key_from_u32(uint32_t k) {
    return k;
}
static inline uint32_t sub_key_from_i32(int32_t k) {
    return (uint32_t)k ^ 0x80000000u;
}
static inline uint32_t sub_key_from_f32(float k) {
    uint32_t u;
    memcpy(&u, &k, sizeof(u));
    // sign_mask = 0xFFFFFFFF if negative, 0x80000000 if positive.
    uint32_t sign_bit = u >> 31;
    uint32_t mask = (uint32_t)(-(int32_t)sign_bit) | 0x80000000u;
    return u ^ mask;
}

// Sort an index array by a companion numeric key array.
//   keys      : read-only, length n
//   indices   : length n, permuted in-place to sort order
//   descending: when true, reverse the ordering
SUB_API void sublimation_pack_sort_u32(
    const uint32_t *keys, uint32_t *indices, size_t n, bool descending);
SUB_API void sublimation_pack_sort_i32(
    const int32_t *keys, uint32_t *indices, size_t n, bool descending);
SUB_API void sublimation_pack_sort_f32(
    const float *keys, uint32_t *indices, size_t n, bool descending);

// Caller-scratch variants: avoid the library-internal malloc. `scratch`
// must hold at least n uint64_t slots (n * 8 bytes).
SUB_API void sublimation_pack_sort_u32_with_scratch(
    const uint32_t *keys, uint32_t *indices, size_t n, bool descending,
    uint64_t *scratch);
SUB_API void sublimation_pack_sort_i32_with_scratch(
    const int32_t *keys, uint32_t *indices, size_t n, bool descending,
    uint64_t *scratch);
SUB_API void sublimation_pack_sort_f32_with_scratch(
    const float *keys, uint32_t *indices, size_t n, bool descending,
    uint64_t *scratch);

#ifdef __cplusplus
}
#endif

#endif // SUBLIMATION_PACK_H
