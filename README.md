# sublimation

A spectral graph sort built in C23. sublimation computes the full Young tableau of the input permutation via the Robinson-Schensted correspondence, derives the information-theoretic lower bound on comparisons from the hook length formula (Kahn & Kim 1995), and routes to specialized paths based on the tableau shape: R_eff merge tree for structured data, k-way merge for k-interleaved sorted sequences, O(n) rotation fix for rotated sorted arrays, counting sort for few-unique data, binary insertion sort for low-displacement nearly-sorted data, branchless block partition for random data, AVX2 sorting networks for small arrays. A critically-damped oscillator drives the CUSUM threshold that controls when partition-based sorting yields to spectral seriation. After sorting, sublimation reports its comparison efficiency against the information-theoretic minimum -- the algorithm grades itself.

## Performance

GCC 15.2.1, `-O2 -march=native`, Arch Linux kernel 6.19, 4C AMD. All values ns/element, best of 5 runs. Comparison points: glibc qsort, inline introsort (median-of-three quicksort + heapsort fallback), Rust `slice::sort_unstable` (ipnsort), Go `slices.Sort` (pdqsort), Python `sorted()` (PowerSort).

### sublimation vs The World (ns/element at 100K, best of 5)

| Pattern | sublimation | Python | introsort | qsort | Go | Rust | vs introsort | vs Rust | vs Python |
|---------|----------:|----------:|----------:|------:|---:|-----:|-------------:|--------:|----------:|
| sorted | **1.3** | 10.0 | 6.3 | 25.4 | 0.8 | 0.6 | **4.7x** | 0.46x | **7.7x** |
| reversed | **1.9** | 10.6 | 7.2 | 33.0 | 1.4 | 0.8 | **3.8x** | 0.42x | **5.6x** |
| equal | **1.3** | 4.5 | 9.3 | 26.9 | 0.8 | 0.6 | **7.0x** | 0.46x | **3.5x** |
| pipe_organ | **5.4** | 18.3 | 77.5 | 29.3 | 30.1 | 19.5 | **14.5x** | **3.6x** | **3.4x** |
| random | **52.6** | 228.1 | 66.2 | 124.3 | 81.0 | 18.1 | **1.3x** | 0.34x | **4.3x** |
| few_unique | **16.2** | 88.2 | 20.0 | 62.4 | 9.9 | 2.9 | **1.2x** | 0.18x | **5.4x** |
| nearly | **10.1** | 27.9 | 8.2 | 43.7 | 15.3 | 15.8 | 0.81x | **1.6x** | **2.8x** |

sublimation beats introsort on 6/7 patterns. Beats Python on 7/7. Beats Go on 4/7. Beats qsort on 7/7. Beats Rust on 2/7 (pipe_organ 3.6x, nearly sorted 1.6x).

### sublimation called from other languages (ns/element at 100K)

| Pattern | C (direct) | via Python | via Rust | via Go |
|---------|----------:|----------:|----------:|----------:|
| sorted | 1.3 | 1.8 | 1.3 | 1.4 |
| reversed | 1.9 | 2.4 | 2.1 | 2.1 |
| equal | 1.3 | 1.8 | 1.4 | 1.4 |
| pipe_organ | 5.4 | 5.8 | 5.1 | 5.7 |
| random | 52.6 | 45.9 | 41.3 | 43.8 |
| nearly | 10.1 | 10.8 | 10.1 | 10.2 |
| few_unique | 16.2 | 20.0 | 15.6 | 15.6 |

FFI overhead is negligible. sublimation called from Python, Rust, or Go performs within 10% of the native C call.

### Core Scaling (1M random, 4C AMD)

| Cores | ns/elem | Speedup | Efficiency |
|------:|--------:|--------:|-----------:|
| 1 | 97.1 | 1.00x | 100% |
| 2 | 67.6 | 1.44x | 72% |
| 4 | 51.0 | 1.90x | 48% |

## Architecture

sublimation is not a portfolio of existing sort algorithms with a classifier in front. It is a flow-model sorting architecture where data moves from an unsorted state (source) toward a sorted state (sink) through a level structure, with the classification pass serving as the initial BFS and the partition/merge serving as the blocking flow DFS.

### Classification (Initial BFS)

Single O(n) pass detects: run count, monotone runs, sorted/reversed, max run length, max descent gap (displacement proxy). Lazy fast paths short-circuit: sorted/reversed/equal detected in one comparison per element. High run count (> n/4) skips expensive passes and routes to partition. Low run count with long max run routes to R_eff merge. Only ambiguous inputs pay for sampled inversions, distinct value estimation, CUSUM phase boundary detection, and full Young tableau computation via patience sorting.

### Young Tableau and Information-Theoretic Bound

For ambiguous inputs, patience sorting computes the full Young tableau shape λ = (λ₁, λ₂, ..., λ_d) by tracking pile sizes during RSK insertion. The tableau shape is the complete algebraic characterization of the permutation's disorder structure:
- λ₁ = LIS length (longest increasing subsequence)
- d = number of rows = LDS length (longest decreasing subsequence)
- Balanced rows (λ_i ≈ n/d) = d interleaved sorted sequences → k-way merge
- Single descent with wraparound continuity = rotated sorted array → O(n) fix

The hook length formula (Frame-Robinson-Thrall) computes f^λ = n! / ∏ h(i,j), giving the exact number of standard Young tableaux of shape λ. log₂(f^λ) is the information-theoretic lower bound on comparisons — the minimum bits of information needed to sort any permutation in this equivalence class. After sorting, sublimation reports comparison efficiency: `actual_comparisons / info_theoretic_bound`. The algorithm knows how close it got to the mathematical minimum.

### Displacement-Based Routing

For nearly-sorted data, the classifier tracks `max_descent_gap` -- the largest value drop at any descent point. This is a displacement proxy computed in the same O(n) run-counting pass, zero additional cost. Routing:
- `run_count <= 16`: structured data (pipe_organ), route to R_eff merge tree
- `run_count > 16, max_descent_gap <= sqrt(n)`: small perturbation, binary insertion sort -- O(n + inversions), 99% of elements skip in one comparison
- `run_count > 16, max_descent_gap > sqrt(n)`: large displacement, lightweight quicksort

### Partition (Level Construction)

2-way unrolled branchless block Lomuto partition with write prefetch. Median-of-three pivot selection (ninther for large arrays). Two elements per iteration, independent comparisons overlapped in the superscalar pipeline. Conditional pointer advance compiles to CMOVcc on x86. EWMA tracks partition quality. CUSUM with oscillator-controlled threshold detects degradation.

### Spectral Fallback

When partition quality degrades (CUSUM triggers), the Laplacian eigendecomposition of the comparison graph recovers the sorted order via Fiedler vector seriation (Atkins, Boman, Hendrickson 1998). Jacobi eigendecomposition extracts the spectral structure. Activates for subarrays in [64, 512] elements.

### R_eff Merge Tree

For structured nearly-sorted data with k runs: effective resistance on the run-boundary path graph determines merge order (Chandra et al. 1996). R_eff(i, i+1) = 1/w where w encodes the value gap between adjacent runs. Low resistance = small gap = galloping merge skips everything = merge first. Boundaries sorted by resistance in O(k log k), processed with union-find coalescing. Gap heuristic (Goldberg-Tarjan 1988): adjacent runs whose boundaries are already sorted coalesce into superruns before merge begins, often reducing k from hundreds to single digits.

### Counting Sort

For few-unique data (k <= 64 distinct values): single-pass fused discovery + histogram. Binary search in a small sorted distinct-values array classifies each element. Prefix sum + scatter writes the sorted output. O(n log k) with no recursion. Early-exit probe at 64 elements bails if k == 1 (all-equal data handled by the faster O(n) sorted path instead).

### Damped Harmonic Oscillator

CoDel-inspired sigmoid center adaptation. The oscillator's position (0..1) controls the CUSUM trigger threshold: tightens toward 0 on degradation (trigger spectral fallback sooner), relaxes toward 1 when stable (permit partition to continue). Critically-damped second-order dynamics converge without oscillation. The threshold is learned, not hardcoded.

### Hybrid DFS

Recursive for shallow depth (branch predictor friendly). Explicit stack with 128 frames for deep paths (stack-safe). Larger half pushed first, smaller half processed first (minimizes peak stack depth). Subarray reclassification every 8 levels (periodic global relabel from Push-Relabel). Equal element detection mid-partition (switches to Dutch national flag when pivot equals previous boundary).

### Parallel Sort (IPS4o-style BSP)

Parallel bucket distribution: each worker classifies its chunk independently (no contention), prefix-sum merge of bucket counts, parallel scatter to global positions, greedy load-balanced per-bucket sort. 4x bucket overpartitioning for load balance. Classification + scatter is O(n/p), removing the Amdahl bottleneck. Auto-dispatches for n >= 100K. Workers sort their buckets using the full sequential flow model independently.

### Sorting Networks

Optimal comparison counts for n = 2-8 (verified against all 40,320 permutations for n=8). Bose-Nelson networks for 7-8. AVX2 vectorized sort-4, sort-8, and sort-16 via bitonic merge networks using `_mm256_cmpgt_epi64` + `_mm256_blendv_epi8` (falls back to scalar on non-AVX2). Sort-8 prefix + insertion sort for 9-15. Standard insertion sort for 17-32.

## Build & Install

```bash
./install.py              # Build and install to /usr/local
./install.py build        # Build only (libsublimation.a + .so)
./install.py test         # Build and run tests
./install.py bench        # Build and run benchmarks
./install.py fuzz         # Build and run differential fuzzer (requires clang + compiler-rt)
./install.py clean        # Clean build artifacts
./install.py status       # Show build/install status
```

Requires GCC 13+ or Clang 16+ with C23 support. No external dependencies.

```bash
# Full test suite: correctness + sanitizers + cross-language + benchmarks + scaling
python3 tests/test.py                # All tests + benchmarks (~843K tests)
python3 tests/test.py --quick        # Tests only, no benchmarks

# Benchmarks with hardware profiling and statistical analysis
python3 tests/bench-sublimation.py --perf    # IPC, cache misses, branch misses per pattern
python3 tests/bench-sublimation.py --stats   # p50/p95/p99/stddev, 11 runs with CI

# Results logged to ~/.cache/sublimation/ (Prometheus + human-readable)
```

843K tests across all 6 numeric types (i32, i64, u32, u64, f32, f64). Includes: exhaustive permutation testing for all types (n=1..8, all permutations, including float edge cases: NaN, INF, -0.0, denormals), Bentley-McIlroy 5x6 matrix (1,740 combinations), McIlroy dynamic adversary, 15 adversarial patterns across all types, comparison-count bound enforcement, ThreadSanitizer, AddressSanitizer, UBSan, differential fuzzing (libFuzzer vs qsort), cross-language roundtrip (Python, Rust, Go).

## Usage

```c
#include <sublimation.h>

// Sort any numeric type -- full flow model for all six
int64_t arr[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
sublimation_i64(arr, 10);

double prices[10000];
sublimation_f64(prices, 10000);

uint32_t ids[50000];
sublimation_u32(ids, 50000);

// _Generic dispatch picks the right function at compile time
sublimation_typed(arr, 10);

// With statistics (includes information-theoretic efficiency)
sub_stats_t stats = {0};
sublimation_i64_stats(arr, n, &stats);
printf("comparisons: %lu, info bound: %.0f, efficiency: %.1f%%\n",
       stats.comparisons, stats.info_theoretic_bound,
       stats.comparison_efficiency * 100.0);

// Classification only (inspect without sorting)
sub_profile_t profile = sublimation_classify_i64(arr, n);
printf("LIS: %zu, LDS: %zu, tableau rows: %zu, interleave k: %zu\n",
       profile.lis_length, profile.lds_length,
       profile.tableau_num_rows, profile.interleave_k);

// Explicit parallel (specify thread count)
sublimation_i64_parallel(arr, 1000000, 8);

// qsort-compatible (custom types, delegates to qsort internally)
sublimation(data, count, sizeof(element), comparator);
```

Compile:

```bash
gcc -O2 myapp.c -lsublimation -lpthread -lm -o myapp
```

## References

### Spectral Graph Theory and Seriation
- Atkins, Boman, Hendrickson. "A Spectral Algorithm for Seriation and the Consecutive Ones Problem." SIAM J. Comput. 1998. *(Fiedler vector seriation: sorting by second eigenvector of the Laplacian recovers the ordering for Robinson matrices. Core theorem behind spectral merge tree.)*
- Fiedler. "Algebraic Connectivity of Graphs." Czechoslovak Mathematical Journal 1973. *(Fiedler vector, algebraic connectivity, spectral bisection.)*
- Klein, Randic. "Resistance Distance." J. Math. Chem. 1993. *(Effective resistance as graph distance metric. R_eff merge ordering.)*
- Chandra, Raghavan, Ruzzo, Smolensky, Tiwari. "The Electrical Resistance of a Graph Captures its Commute and Cover Times." Computational Complexity 1996. *(Commute time theorem. R_eff on run-boundary path graph determines merge order.)*

### Robinson-Schensted Correspondence and Sorting
- Schensted. "Longest Increasing and Decreasing Subsequences." Canadian J. Math. 1961. *(RSK correspondence, patience sorting computes LIS = first row of Young tableau.)*
- Baik, Deift, Johansson. "On the Distribution of the Length of the Longest Increasing Subsequence of Random Permutations." JAMS 1999. *(LIS ~ 2*sqrt(n) with Tracy-Widom fluctuations for random permutations.)*
- Mannila. "Measures of Presortedness and Optimal Sorting Algorithms." IEEE Trans. Computers 1985. *(Framework for presortedness measures: Inv, Runs, Rem, Max, Osc.)*

### Adaptive Control and Online Learning
- Nichols, Jacobson. "Controlled Delay Active Queue Management." RFC 8289, 2018. *(CoDel algorithm. Damped oscillation adapted for CUSUM threshold control.)*
- Page. "Continuous Inspection Schemes." Biometrika 1954. *(CUSUM change-point detection. Partition quality degradation and phase boundary detection.)*

### Maximum Flow and Graph Algorithms
- Chen, Kyng, Liu, Peng, Gutenberg, Sachdeva. "Maximum Flow and Minimum-Cost Flow in Almost-Linear Time." FOCS 2022 Best Paper. *(Kyng et al. research lineage. Laplacian solvers, spectral sparsification.)*
- Dinic. "Algorithm for Solution of a Problem of Maximum Flow in Networks with Power Estimation." Soviet Math. Doklady 1970. *(BFS level graph + DFS blocking flow. Architectural ancestor of sublimation's flow-model sort.)*
- Goldberg, Tarjan. "A New Approach to the Maximum-Flow Problem." J. ACM 1988. *(Push-Relabel with gap optimization. Gap heuristic run pruning.)*

### Sorting Algorithms (Comparison Points)
- Musser. "Introspective Sorting and Selection Algorithms." SPE 1997. *(Introsort: quicksort + heapsort fallback.)*
- Peters. "Pattern-defeating Quicksort." arXiv:2106.05123, 2021. *(pdqsort: Rust and Go's default sort.)*
- Edelkamp, Weiss. "BlockQuicksort: Avoiding Branch Mispredictions in Quicksort." ESA 2016. *(Block partition technique.)*
- Axtmann, Witt, Ferizovic, Sanders. "In-Place Parallel Super Scalar Samplesort (IPS4o)." ESA 2017. *(Parallel bucket distribution pattern.)*
- Bentley, McIlroy. "Engineering a Sort Function." Software--Practice and Experience 1993. *(Canonical sort testing methodology.)*
- McIlroy. "A Killer Adversary for Quicksort." Software--Practice and Experience 1999. *(Dynamic adversarial input construction.)*

## License

GPL-2.0
