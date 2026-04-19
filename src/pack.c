// pack.c -- index sort by companion numeric key. Backs sublimation_pack.h.
//
// Pipeline:
//   1. For each (key, index) pair, map key -> monotonic uint32 via
//      sub_key_from_<T>, optionally bit-invert for descending, and pack
//      as (mono_key << 32) | indices[i] into a uint64.
//   2. Sort the packed array with sublimation_u64.
//   3. Unpack: indices[i] = packed[i].low.
//
// The high 32 bits dominate u64 ordering, so the result is sorted by key
// (ascending or descending); the low 32 bits being the original index
// keep the sort stable for equal keys.
#include "sublimation_pack.h"

#include <stdint.h>
#include <stdlib.h>

extern void sublimation_u64(uint64_t *arr, size_t n);

static inline uint64_t sub_pack_key_u32(uint32_t k, uint32_t idx, bool desc) {
    uint32_t mono = desc ? ~k : k;
    return ((uint64_t)mono << 32) | (uint64_t)idx;
}

void sublimation_pack_sort_u32_with_scratch(
    const uint32_t *keys, uint32_t *indices, size_t n, bool desc,
    uint64_t *scratch) {
    for (size_t i = 0; i < n; i++) {
        scratch[i] = sub_pack_key_u32(sub_key_from_u32(keys[i]), indices[i], desc);
    }
    sublimation_u64(scratch, n);
    for (size_t i = 0; i < n; i++) {
        indices[i] = (uint32_t)(scratch[i] & 0xFFFFFFFFu);
    }
}

void sublimation_pack_sort_i32_with_scratch(
    const int32_t *keys, uint32_t *indices, size_t n, bool desc,
    uint64_t *scratch) {
    for (size_t i = 0; i < n; i++) {
        scratch[i] = sub_pack_key_u32(sub_key_from_i32(keys[i]), indices[i], desc);
    }
    sublimation_u64(scratch, n);
    for (size_t i = 0; i < n; i++) {
        indices[i] = (uint32_t)(scratch[i] & 0xFFFFFFFFu);
    }
}

void sublimation_pack_sort_f32_with_scratch(
    const float *keys, uint32_t *indices, size_t n, bool desc,
    uint64_t *scratch) {
    for (size_t i = 0; i < n; i++) {
        scratch[i] = sub_pack_key_u32(sub_key_from_f32(keys[i]), indices[i], desc);
    }
    sublimation_u64(scratch, n);
    for (size_t i = 0; i < n; i++) {
        indices[i] = (uint32_t)(scratch[i] & 0xFFFFFFFFu);
    }
}

void sublimation_pack_sort_u32(
    const uint32_t *keys, uint32_t *indices, size_t n, bool desc) {
    if (n < 2) return;
    uint64_t *scratch = (uint64_t *)malloc(n * sizeof(uint64_t));
    if (!scratch) return;
    sublimation_pack_sort_u32_with_scratch(keys, indices, n, desc, scratch);
    free(scratch);
}

void sublimation_pack_sort_i32(
    const int32_t *keys, uint32_t *indices, size_t n, bool desc) {
    if (n < 2) return;
    uint64_t *scratch = (uint64_t *)malloc(n * sizeof(uint64_t));
    if (!scratch) return;
    sublimation_pack_sort_i32_with_scratch(keys, indices, n, desc, scratch);
    free(scratch);
}

void sublimation_pack_sort_f32(
    const float *keys, uint32_t *indices, size_t n, bool desc) {
    if (n < 2) return;
    uint64_t *scratch = (uint64_t *)malloc(n * sizeof(uint64_t));
    if (!scratch) return;
    sublimation_pack_sort_f32_with_scratch(keys, indices, n, desc, scratch);
    free(scratch);
}
