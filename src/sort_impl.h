// sort_impl.h -- Template body for the main sort (included once per type)
//
// Requires SUB_TYPE and SUB_SUFFIX to be defined before inclusion.

// Reverse an array in-place
static void SUB_TYPED(reverse)(SUB_TYPE *arr, size_t n) {
    size_t lo = 0, hi = n - 1;
    while (lo < hi) {
        SUB_SWAP(SUB_TYPE, arr[lo], arr[hi]);
        lo++;
        hi--;
    }
}

// COUNTING SORT FOR FEW UNIQUE VALUES
#ifndef COUNTING_SORT_MAX_K
#define COUNTING_SORT_MAX_K 64
#endif

static bool SUB_TYPED(counting_sort_few_unique)(SUB_TYPE *arr, size_t n,
                                                 uint64_t *comparisons,
                                                 uint64_t *swaps) {
    SUB_TYPE uniq[COUNTING_SORT_MAX_K];
    size_t  histogram[COUNTING_SORT_MAX_K];
    size_t k = 0;

    // Early-exit probe: after 64 elements, if k==1 (all equal), bail.
    // The classifier's O(n) sorted-detection is faster for equal data.
    #ifndef COUNTING_SORT_PROBE
    #define COUNTING_SORT_PROBE 64
    #endif

    for (size_t i = 0; i < n; i++) {
        SUB_TYPE val = arr[i];

        if (i == COUNTING_SORT_PROBE && k == 1) {
            return false;
        }

        size_t lo = 0, hi = k;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (uniq[mid] < val) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }

        if (lo < k && uniq[lo] == val) {
            histogram[lo]++;
            continue;
        }

        if (k >= COUNTING_SORT_MAX_K) {
            return false;
        }

        for (size_t j = k; j > lo; j--) {
            uniq[j] = uniq[j - 1];
            histogram[j] = histogram[j - 1];
        }
        uniq[lo] = val;
        histogram[lo] = 1;
        k++;
    }

    if (k <= 1) return true;

    size_t write = 0;
    for (size_t j = 0; j < k; j++) {
        size_t count = histogram[j];
        SUB_TYPE val = uniq[j];
        size_t c = 0;
        for (; c + 4 <= count; c += 4) {
            arr[write]     = val;
            arr[write + 1] = val;
            arr[write + 2] = val;
            arr[write + 3] = val;
            write += 4;
        }
        for (; c < count; c++) {
            arr[write++] = val;
        }
        *swaps += count;
    }

    (void)comparisons;
    return true;
}

// LIGHTWEIGHT QUICKSORT
static void SUB_TYPED(light_insertion_sort)(SUB_TYPE *arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        SUB_TYPE key = arr[i];
        size_t j = i;
        while (j > 0 && arr[j - 1] > key) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = key;
    }
}

static void SUB_TYPED(light_siftdown)(SUB_TYPE *arr, size_t root, size_t n) {
    while (2 * root + 1 < n) {
        size_t child = 2 * root + 1;
        if (child + 1 < n && arr[child] < arr[child + 1]) child++;
        if (arr[root] >= arr[child]) break;
        SUB_SWAP(SUB_TYPE, arr[root], arr[child]);
        root = child;
    }
}

static void SUB_TYPED(light_heapsort)(SUB_TYPE *arr, size_t n) {
    if (n < 2) return;
    for (size_t i = n / 2; i > 0; i--) SUB_TYPED(light_siftdown)(arr, i - 1, n);
    for (size_t i = n - 1; i > 0; i--) {
        SUB_SWAP(SUB_TYPE, arr[0], arr[i]);
        SUB_TYPED(light_siftdown)(arr, 0, i);
    }
}

static void SUB_TYPED(light_qsort)(SUB_TYPE *arr, size_t n, int depth) {
    while (n > 24) {
        if (depth == 0) {
            SUB_TYPED(light_heapsort)(arr, n);
            return;
        }
        depth--;

        size_t mid = n / 2;
        if (arr[0] > arr[mid]) SUB_SWAP(SUB_TYPE, arr[0], arr[mid]);
        if (arr[mid] > arr[n - 1]) SUB_SWAP(SUB_TYPE, arr[mid], arr[n - 1]);
        if (arr[0] > arr[mid]) SUB_SWAP(SUB_TYPE, arr[0], arr[mid]);
        SUB_TYPE pivot = arr[mid];

        size_t i = 0, j = n - 1;
        while (i <= j) {
            while (arr[i] < pivot) i++;
            while (arr[j] > pivot) j--;
            if (i <= j) {
                SUB_SWAP(SUB_TYPE, arr[i], arr[j]);
                i++;
                if (j == 0) break;
                j--;
            }
        }

        if (j + 1 < n - i) {
            SUB_TYPED(light_qsort)(arr, j + 1, depth);
            arr += i;
            n -= i;
        } else {
            SUB_TYPED(light_qsort)(arr + i, n - i, depth);
            n = j + 1;
        }
    }
    SUB_TYPED(light_insertion_sort)(arr, n);
}

static void SUB_TYPED(light_sort)(SUB_TYPE *arr, size_t n) {
    int depth = 0;
    size_t t = n;
    while (t > 1) { t >>= 1; depth++; }
    depth *= 2;
    SUB_TYPED(light_qsort)(arr, n, depth);
}

// BINARY INSERTION SORT
static void SUB_TYPED(binary_isort)(SUB_TYPE *arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        SUB_TYPE key = arr[i];
        if (key >= arr[i - 1]) continue;
        size_t lo = 0, hi = i;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (arr[mid] > key) hi = mid;
            else lo = mid + 1;
        }
        memmove(arr + lo + 1, arr + lo, (i - lo) * sizeof(SUB_TYPE));
        arr[lo] = key;
    }
}

// PARTITION PRIMITIVES
static size_t SUB_TYPED(median_of_three)(SUB_TYPE *arr, size_t a, size_t b, size_t c,
                                          uint64_t *comparisons) {
    *comparisons += 3;
    if (arr[a] > arr[b]) SUB_SWAP(SUB_TYPE, arr[a], arr[b]);
    if (arr[b] > arr[c]) SUB_SWAP(SUB_TYPE, arr[b], arr[c]);
    if (arr[a] > arr[b]) SUB_SWAP(SUB_TYPE, arr[a], arr[b]);
    return b;
}

static size_t SUB_TYPED(choose_pivot)(SUB_TYPE *arr, size_t lo, size_t hi,
                                       uint64_t *comparisons) {
    size_t n = hi - lo + 1;
    size_t mid = lo + n / 2;

    if (n <= SUB_MEDIUM_THRESHOLD) {
        return SUB_TYPED(median_of_three)(arr, lo, mid, hi, comparisons);
    }

    size_t step = n / 8;
    SUB_TYPED(median_of_three)(arr, lo, lo + step, lo + 2 * step, comparisons);
    SUB_TYPED(median_of_three)(arr, mid - step, mid, mid + step, comparisons);
    SUB_TYPED(median_of_three)(arr, hi - 2 * step, hi - step, hi, comparisons);
    return SUB_TYPED(median_of_three)(arr, lo + step, mid, hi - step, comparisons);
}

// BLOCK LOMUTO PARTITION
static size_t SUB_TYPED(partition_block)(SUB_TYPE *arr, size_t lo, size_t hi,
                                          uint64_t *comparisons, uint64_t *swaps) {
    size_t n = hi - lo + 1;
    if (n < 2) return lo;

    size_t pivot_idx = SUB_TYPED(choose_pivot)(arr, lo, hi, comparisons);
    SUB_TYPE pivot = arr[pivot_idx];

    SUB_SWAP(SUB_TYPE, arr[pivot_idx], arr[hi]);
    (*swaps)++;

    size_t write = lo;

    size_t read = lo;
    for (; read + 1 < hi; read += 2) {
        sub_prefetch_w(&arr[write + 4]);

        (*comparisons) += 2;
        SUB_TYPE v0 = arr[read];
        SUB_TYPE v1 = arr[read + 1];
        int less0 = v0 < pivot;
        int less1 = v1 < pivot;

        arr[read] = arr[write];
        arr[write] = v0;
        write += (size_t)less0;

        arr[read + 1] = arr[write];
        arr[write] = v1;
        write += (size_t)less1;
    }
    for (; read < hi; read++) {
        (*comparisons)++;
        SUB_TYPE val = arr[read];
        int less = val < pivot;
        arr[read] = arr[write];
        arr[write] = val;
        write += (size_t)less;
    }

    SUB_SWAP(SUB_TYPE, arr[write], arr[hi]);
    (*swaps)++;

    return write;
}

// THREE-WAY PARTITION
static void SUB_TYPED(partition_three_way)(SUB_TYPE *arr, size_t lo, size_t hi,
                                            size_t *out_lt, size_t *out_gt,
                                            uint64_t *comparisons, uint64_t *swaps) {
    size_t pivot_idx = SUB_TYPED(choose_pivot)(arr, lo, hi, comparisons);
    SUB_TYPE pivot = arr[pivot_idx];

    size_t lt = lo;
    size_t i = lo;
    size_t gt = hi;

    while (i <= gt) {
        (*comparisons)++;
        if (arr[i] < pivot) {
            SUB_SWAP(SUB_TYPE, arr[lt], arr[i]);
            (*swaps)++;
            lt++;
            i++;
        } else if (arr[i] > pivot) {
            SUB_SWAP(SUB_TYPE, arr[i], arr[gt]);
            (*swaps)++;
            if (gt == 0) break;
            gt--;
        } else {
            i++;
        }
    }

    *out_lt = lt;
    *out_gt = gt;
}

// DFS FRAME
typedef struct {
    size_t        lo;
    size_t        hi;
    sub_disorder_t disorder;
} SUB_TYPED(sub_dfs_frame_t);

// PARTITION + ADAPTIVE MONITOR
static size_t SUB_TYPED(partition_one_level)(SUB_TYPE *arr, size_t lo, size_t hi,
                                              sub_adaptive_t *state, sub_disorder_t *disorder,
                                              size_t *out_lt, size_t *out_gt) {
    size_t n = hi - lo + 1;

    // EQUAL ELEMENT DETECTION
#ifdef SUB_TYPE_IS_I64
    if (*disorder != SUB_FEW_UNIQUE && state->has_last_pivot) {
        size_t mid = lo + n / 2;
        if (arr[mid] == state->last_pivot) {
            *disorder = SUB_FEW_UNIQUE;
        }
    }
#endif

    if (*disorder == SUB_FEW_UNIQUE) {
        SUB_TYPED(partition_three_way)(arr, lo, hi, out_lt, out_gt,
                                       &state->comparisons, &state->swaps);
        state->levels_built++;
        if (*out_lt > lo + 1) state->gap_prunes++;
        return SIZE_MAX;
    }

    size_t pivot_pos = SUB_TYPED(partition_block)(arr, lo, hi,
                                                   &state->comparisons, &state->swaps);
    state->levels_built++;

#ifdef SUB_TYPE_IS_I64
    state->last_pivot = arr[pivot_pos];
    state->has_last_pivot = true;
#endif

    float quality = (float)(pivot_pos - lo) / (float)n;
    if (quality > 0.5f) quality = 1.0f - quality;
    quality *= 2.0f;
    sub_ewma_update(&state->partition_quality_ewma, quality);

    bool degraded = sub_cusum_update(&state->cusum_s, quality,
                                     state->partition_quality_ewma,
                                     state->osc_position);

    sub_oscillator_update(&state->osc_position, &state->osc_velocity, degraded);

    if (sub_unlikely(degraded)) {
        state->rescans++;
        state->cusum_s = 0.0f;

        // spectral fallback (only for i64 currently)
#ifdef SUB_TYPE_IS_I64
        if (!state->spectral_attempted
            && n >= SUB_SPECTRAL_MIN_N && n <= SUB_SPECTRAL_MAX_N) {
            sub_spectral_ws_t *ws = sub_spectral_ws_alloc(n);
            if (ws) {
                sub_spectral_result_t sr = sub_spectral_sort_i64(
                    arr + lo, n, ws, state);
                sub_spectral_ws_free(ws);
                state->spectral_attempted = true;
                if (sr.gap_sufficient && sr.converged) {
                    return pivot_pos;
                }
            }
        }
#endif

        state->rescan_trigger = (size_t)((float)state->rescan_trigger * SUB_RESCAN_GROWTH);
    }

    return pivot_pos;
}

// RECURSIVE DFS
static void SUB_TYPED(push_recursive)(SUB_TYPE *arr, size_t lo, size_t hi,
                                       sub_adaptive_t *state, sub_disorder_t disorder,
                                       int depth) {
    size_t n = hi - lo + 1;

    if (n <= SUB_SMALL_THRESHOLD) {
        SUB_TYPED(sub_small_sort)(arr + lo, n, &state->comparisons, &state->swaps);
        return;
    }

    if (sub_unlikely(depth >= SUB_STACK_LIMIT)) {
        goto SUB_TYPED(iterative);
    }

    {
        sub_disorder_t local_disorder = disorder;
        if (depth > 0 && depth % SUB_RECLASSIFY_INTERVAL == 0 && n > SUB_SMALL_THRESHOLD * 4) {
            sub_profile_t sub = SUB_TYPED(sub_classify_internal)(arr + lo, n);
            if (sub.disorder != disorder) {
                local_disorder = sub.disorder;
                state->rescans++;
            }
        }

        size_t lt, gt;
        size_t pivot = SUB_TYPED(partition_one_level)(arr, lo, hi, state, &local_disorder, &lt, &gt);

        if (pivot == SIZE_MAX) {
            if (lt > lo) SUB_TYPED(push_recursive)(arr, lo, lt - 1, state, local_disorder, depth + 1);
            if (gt < hi) SUB_TYPED(push_recursive)(arr, gt + 1, hi, state, local_disorder, depth + 1);
        } else {
            if (pivot > lo + 1) SUB_TYPED(push_recursive)(arr, lo, pivot - 1, state, local_disorder, depth + 1);
            if (pivot + 1 < hi) SUB_TYPED(push_recursive)(arr, pivot + 1, hi, state, local_disorder, depth + 1);
        }
        return;
    }

SUB_TYPED(iterative):
    {
        SUB_CONSTEXPR size_t MAX_FRAMES = 128;
        SUB_TYPED(sub_dfs_frame_t) stack[128];
        size_t sp = 0;

        stack[sp++] = (SUB_TYPED(sub_dfs_frame_t)){lo, hi, disorder};

        while (sp > 0) {
            SUB_TYPED(sub_dfs_frame_t) frame = stack[--sp];
            size_t fn = frame.hi - frame.lo + 1;

            if (fn <= SUB_SMALL_THRESHOLD) {
                SUB_TYPED(sub_small_sort)(arr + frame.lo, fn,
                                          &state->comparisons, &state->swaps);
                continue;
            }

            size_t lt, gt;
            sub_disorder_t fd = frame.disorder;
            size_t pivot = SUB_TYPED(partition_one_level)(arr, frame.lo, frame.hi,
                                                           state, &fd, &lt, &gt);

            if (pivot == SIZE_MAX) {
                if (gt < frame.hi && sp < MAX_FRAMES) {
                    stack[sp++] = (SUB_TYPED(sub_dfs_frame_t)){gt + 1, frame.hi, fd};
                }
                if (lt > frame.lo && sp < MAX_FRAMES) {
                    stack[sp++] = (SUB_TYPED(sub_dfs_frame_t)){frame.lo, lt - 1, fd};
                }
            } else {
                size_t left_n = pivot > frame.lo ? pivot - frame.lo : 0;
                size_t right_n = pivot < frame.hi ? frame.hi - pivot : 0;

                if (left_n > right_n) {
                    if (left_n > 1 && sp < MAX_FRAMES) {
                        stack[sp++] = (SUB_TYPED(sub_dfs_frame_t)){frame.lo, pivot - 1, fd};
                    }
                    if (right_n > 1 && sp < MAX_FRAMES) {
                        stack[sp++] = (SUB_TYPED(sub_dfs_frame_t)){pivot + 1, frame.hi, fd};
                    }
                } else {
                    if (right_n > 1 && sp < MAX_FRAMES) {
                        stack[sp++] = (SUB_TYPED(sub_dfs_frame_t)){pivot + 1, frame.hi, fd};
                    }
                    if (left_n > 1 && sp < MAX_FRAMES) {
                        stack[sp++] = (SUB_TYPED(sub_dfs_frame_t)){frame.lo, pivot - 1, fd};
                    }
                }
            }
        }
    }
}

// HYBRID DFS SORT
static void SUB_TYPED(push)(SUB_TYPE *arr, size_t lo, size_t hi,
                             sub_adaptive_t *state, sub_disorder_t disorder) {
    SUB_TYPED(push_recursive)(arr, lo, hi, state, disorder, 0);
}

// PHASED SORT
static void SUB_TYPED(sort_phased)(SUB_TYPE *arr, size_t n, size_t boundary,
                                    sub_adaptive_t *state) {
    size_t suffix_len = n - boundary;

    if (boundary > 1) {
        bool prefix_sorted = true;
        for (size_t i = 1; i < boundary; i++) {
            if (arr[i] < arr[i - 1]) { prefix_sorted = false; break; }
        }
        if (!prefix_sorted) {
            SUB_TYPED(push)(arr, 0, boundary - 1, state, SUB_RANDOM);
        }
    }

    if (suffix_len > 1) {
        SUB_TYPED(push)(arr, boundary, n - 1, state, SUB_RANDOM);
    }

    if (arr[boundary - 1] <= arr[boundary]) return;

    // Gallop to find where suffix minimum lands in prefix
    SUB_TYPE key = arr[boundary];
    size_t skip = 0, ofs = 1;
    while (ofs < boundary && arr[ofs] < key) { skip = ofs; ofs = (ofs << 1) + 1; }
    if (ofs > boundary) ofs = boundary;
    while (skip < ofs) {
        size_t mid = skip + (ofs - skip) / 2;
        if (arr[mid] < key) skip = mid + 1; else ofs = mid;
    }
    size_t prefix_overlap = boundary - skip;

    if (prefix_overlap <= suffix_len) {
        SUB_TYPE *tmp = malloc(prefix_overlap * sizeof(SUB_TYPE));
        if (!tmp) {
            SUB_TYPED(sub_spectral_merge)(arr, n, &state->comparisons);
            return;
        }
        memcpy(tmp, arr + skip, prefix_overlap * sizeof(SUB_TYPE));

        size_t a = 0, b = boundary, w = skip;
        while (a < prefix_overlap && b < n) {
            state->comparisons++;
            if (tmp[a] <= arr[b]) arr[w++] = tmp[a++];
            else                   arr[w++] = arr[b++];
        }
        while (a < prefix_overlap) arr[w++] = tmp[a++];
        free(tmp);
    } else {
        SUB_TYPE *tmp = malloc(suffix_len * sizeof(SUB_TYPE));
        if (!tmp) {
            SUB_TYPED(sub_spectral_merge)(arr, n, &state->comparisons);
            return;
        }
        memcpy(tmp, arr + boundary, suffix_len * sizeof(SUB_TYPE));

        size_t a = prefix_overlap;
        size_t b = suffix_len;
        size_t w = n;
        while (a > 0 && b > 0) {
            state->comparisons++;
            if (arr[skip + a - 1] > tmp[b - 1]) arr[--w] = arr[skip + --a];
            else                                  arr[--w] = tmp[--b];
        }
        while (b > 0) arr[--w] = tmp[--b];
        free(tmp);
    }
}

// ROTATED SORTED FIX: O(n) via 3 reverses
// arr[0..rot-1] and arr[rot..n-1] are each sorted, arr[n-1] <= arr[0]
// reverse [0..rot-1], reverse [rot..n-1], reverse [0..n-1]
static void SUB_TYPED(fix_rotation)(SUB_TYPE *arr, size_t n, size_t rot, uint64_t *swaps) {
    if (rot == 0 || rot >= n) return;
    // reverse [0..rot-1]
    for (size_t i = 0, j = rot - 1; i < j; i++, j--) {
        SUB_SWAP(SUB_TYPE, arr[i], arr[j]);
        (*swaps)++;
    }
    // reverse [rot..n-1]
    for (size_t i = rot, j = n - 1; i < j; i++, j--) {
        SUB_SWAP(SUB_TYPE, arr[i], arr[j]);
        (*swaps)++;
    }
    // reverse [0..n-1]
    for (size_t i = 0, j = n - 1; i < j; i++, j--) {
        SUB_SWAP(SUB_TYPE, arr[i], arr[j]);
        (*swaps)++;
    }
}

// INTERNAL SORT ENTRY
void SUB_TYPED(sub_sort_internal)(SUB_TYPE *restrict arr, size_t n, sub_adaptive_t *state) {
    if (n <= 1) return;

    sub_profile_t profile = SUB_TYPED(sub_classify_internal)(arr, n);

    switch (profile.disorder) {
    case SUB_SORTED:
        return;

    case SUB_REVERSED:
        SUB_TYPED(reverse)(arr, n);
        state->swaps += n / 2;
        return;

    case SUB_PHASED:
        SUB_TYPED(sort_phased)(arr, n, profile.phase_boundary, state);
        return;

    case SUB_NEARLY_SORTED:
        // Rotated sorted array: O(n) fix
        if (profile.rotation_point > 0) {
            SUB_TYPED(fix_rotation)(arr, n, profile.rotation_point, &state->swaps);
            return;
        }
        if (profile.run_count <= 16) {
            SUB_TYPED(sub_spectral_merge)(arr, n, &state->comparisons);
        } else {
            size_t sqrt_n = 1;
            while (sqrt_n * sqrt_n < n) sqrt_n++;
            if (profile.max_descent_gap <= (int64_t)sqrt_n) {
                SUB_TYPED(binary_isort)(arr, n);
            } else {
                SUB_TYPED(light_sort)(arr, n);
            }
        }
        return;

    case SUB_FEW_UNIQUE:
        if (SUB_TYPED(counting_sort_few_unique)(arr, n, &state->comparisons, &state->swaps)) {
            return;
        }
        SUB_TYPED(push)(arr, 0, n - 1, state, profile.disorder);
        return;

    case SUB_RANDOM:
        SUB_TYPED(push)(arr, 0, n - 1, state, profile.disorder);
        return;

    case SUB_SPECTRAL:
        SUB_TYPED(push)(arr, 0, n - 1, state, SUB_RANDOM);
        return;
    }

    unreachable();
}

// PUBLIC API ENTRY POINT
void SUB_TYPED(sublimation)(SUB_TYPE *restrict arr, size_t n) {
    if (n <= 1) return;

    // Counting sort: O(n) for k <= 64
    {
        uint64_t cmp = 0, swp = 0;
        if (SUB_TYPED(counting_sort_few_unique)(arr, n, &cmp, &swp)) return;
    }

    sub_profile_t profile = SUB_TYPED(sub_classify_internal)(arr, n);

    if (profile.disorder == SUB_SORTED) return;
    if (profile.disorder == SUB_REVERSED) {
        SUB_TYPED(reverse)(arr, n);
        return;
    }

    if (profile.disorder == SUB_NEARLY_SORTED) {
        // Rotated sorted array: O(n) fix
        if (profile.rotation_point > 0) {
            uint64_t swp = 0;
            SUB_TYPED(fix_rotation)(arr, n, profile.rotation_point, &swp);
            return;
        }
        if (profile.run_count <= 16) {
            uint64_t cmp = 0;
            SUB_TYPED(sub_spectral_merge)(arr, n, &cmp);
        } else {
            size_t sqrt_n = 1;
            while (sqrt_n * sqrt_n < n) sqrt_n++;
            if (profile.max_descent_gap <= (int64_t)sqrt_n) {
                SUB_TYPED(binary_isort)(arr, n);
            } else {
                SUB_TYPED(light_sort)(arr, n);
            }
        }
        return;
    }

    // Parallel path: i64 only for now
#ifdef SUB_TYPE_IS_I64
    if (n >= SUB_PARALLEL_THRESHOLD) {
        sublimation_i64_parallel(arr, n, sub_default_num_workers());
        return;
    }
#endif

    sub_adaptive_t state;
    sub_adaptive_init(&state, n);
    SUB_TYPED(sub_sort_internal)(arr, n, &state);
}
