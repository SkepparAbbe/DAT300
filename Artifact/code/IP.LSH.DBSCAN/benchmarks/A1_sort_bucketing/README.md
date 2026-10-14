# A1: sort-based bucketing in Phase I of IP.LSH.DBSCAN

**Change:** an optional Phase I that replaces the TBB concurrent hash maps with
"pack the M hash values into a uint64 key → parallel LSD radix sort → CSR table (`ids`, `offsets`, `keys`)
→ candidate buckets = runs with ≥ minPts members". It's enabled with `-s` in `LSHDBSCAN_exec`; without `-s`
the program behaves exactly as before.

**Result** (real program, K = 32–72 threads, 1 warm-up + 5 measured runs, final version with grouped keys):
- **Memory: A1 wins.**
  - Household: peak RSS is 30–59% lower (1975 → 801 MB at L=20).
  - MNIST: 3–13% lower, because the 376 MB dataset dominates RSS there.
- **Time: A1 is not faster.**

  | | Household | MNIST |
  |---|---|---|
  | Phase I (vs original) | 0.57–0.88× | 0.83–0.97× |
  | Core-bucket identification | 1.02–1.35× (A1 faster) | 0.87–0.96× |
  | All clustering phases | 0.75–0.94× | 0.89–0.94× |

- **Quality: unchanged.**
  - Buckets and candidate sets are identical in all 18 real-data checks.
  - Against exact DBSCAN (household), both variants give the same Rand index to 3 decimals (0.921 / 0.942 / 0.955 for L = 5 / 10 / 20).
- **Recommendation:** use `-s` when memory is the limit, with large L or large N. Don't use it to save time.

**Fix applied during the evaluation (v1 → v2):**
- **v1** computed keys *table by table*, so it re-read the whole dataset once per table. A `perf stat` on MNIST L=58 showed the same number of instructions as the original but +18% cycles and +44% last-level cache misses.
- **v2** computes keys point by point for a group of tables (key buffer ≤ 128 MB). That brought cache misses down to the original's level (25.3 M vs 26.6 M).
  - MNIST Phase I went from 0.34–0.82× to **0.83–0.97×**.
  - Household Phase I went from 0.52–0.79× to 0.57–0.88×; there, the remaining cost is the radix sort itself.
  - The key buffer adds 26–125 MB to A1's RSS. See `v1_vs_v2.md`; v1's raw data is in `v1_table_by_table/`.

## What changed in the code

| File | Change |
|---|---|
| `src/include/sortBucketing.h` (new) | `packKeyStep`/`packKey`: angular packs M sign bits (M ≤ 64); Euclidean mixes the M ints with splitmix64. `SortBuffers`: parallel stable LSD radix sort with 8-bit digits, per-thread histograms, a (digit, thread) prefix sum and a disjoint scatter; it skips constant digits and uses one counting pass when M ≤ 16. `buildCsr`: one parallel scan that builds `offsets` + `keys`. |
| `src/include/transformation.h`, `src/transformation.cc` | `HashTable` gains `csr`, `candidateBuckets` and `bucketKey()`. `identifyCoreBuckets` and `identifyCoreBuckets_densityStyle` iterate buckets through `forEachCandidateBucket` (CSR if it was filled, otherwise `myMap`); the per-bucket body is unchanged. |
| `src/include/ConcurrentLSHDBSCAN.h`, `src/ConcurrentLSHDBSCAN.cc` | New `sortBucketing` flag (default `false`). With the flag set, `populateHashTables()` calls `populateSortedTables()`, which runs inside a `tbb::task_arena(K)` and works in groups of tables. For each group it computes the keys point by point; then for each table it sorts, builds the CSR and filters the candidates. The sort buffers are reused across tables. |
| `src/globals.cc`, `src/include/globals.h`, `apps/main_general.cc` | `-s` option → `Parameters::sortBucketing` → constructor. |
| `tests/sort_bucketing_test.cc` (new), `CMakeLists.txt` | CTest test, described below. Build flags are unchanged. |

```bash
cd build && cmake .. && make && ctest                      # build + unit tests
./LSHDBSCAN_exec -f data.txt -m 100 -e 2000 -M 5 -L 20 -t 36 -s   # A1
```

## Correctness

- **`ctest` (`sort_bucketing_test` without arguments):**
  - radix sort compared with `std::stable_sort` on edge cases (empty, constant, sorted, reversed, top byte only, duplicates, the M ≤ 16 path), for 1, 8 and 72 threads;
  - CSR validity;
  - the original and A1 run inside the real `ConcurrentLSHDBSCAN` on synthetic data (both metrics, 1 and 8 threads), checking that every table has the same bucket partition and the same candidate set.
- **Real data** (`scripts/run_integration_check.sh` → `correctness.txt`): **18/18 PASS.** MNIST and household, every L, K ∈ {1, 36, 72}: every table has an identical partition and identical candidates. Examples: household L=20 has 2,427,035 buckets and 52,135 candidates; MNIST L=230 has 101,224 buckets and 25,700 candidates.
- **Final labels:** with 1 thread, the two variants agree at a Rand index of 0.9993–1.0. They are not always exactly 1 because candidate buckets are visited in a different order, and the original's core detection depends on that order.

## Results

**Setup:**
- **Machine:** Ithaca, 2× Xeon E5-2695 (36 cores / 72 threads, 2 NUMA nodes).
- **Build:** Release, unchanged flags (`-std=c++11 -O3`).
- **Runs:** each `LSHDBSCAN_exec` run is a separate process inside `resman run`, K ∈ {32, 36, 72}.
- **Times:** taken from the `benchmark.txt` the program itself writes. "Phase I" is `populate`, which includes key computation in both variants.
- **Data:**
  - **MNIST:** 60,000 × 784, angular, ε = 0.2, M = 9, L ∈ {58, 116, 230};
  - **household:** 2,049,280 × 7, Euclidean, ε = 2000, M = 5, L ∈ {5, 10, 20};
  - minPts = 100 for both.

Plots: `app_scaling_household.png` and `app_scaling_mnist.png` (time vs K, per phase and L); `app_memory_rss_vs_L.png` (peak RSS vs L, K = 36).

### household

Times from the app's `benchmark.txt`, in seconds as `median (min)` over 5 measured runs; x = original / A1 on medians. Total = all clustering phases (excludes reading data and writing labels). Rand index from the first measured run (exact = vanilla DBSCAN labels).

| L | K | Phase I orig | Phase I A1 | x | core buckets orig | core buckets A1 | x | total orig | total A1 | x | peak RSS orig / A1 (MB) | Rand orig vs A1 | Rand vs exact orig / A1 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 5 | 32 | 0.1688 (0.1654) | 0.2500 (0.2450) | **0.68** | 0.2741 (0.2690) | 0.2506 (0.2385) | **1.09** | 0.4717 (0.4661) | 0.5258 (0.5186) | **0.90** | 699 / 489 | 0.998021 | 0.921540 / 0.921411 |
| 5 | 36 | 0.1597 (0.1562) | 0.2486 (0.2468) | **0.64** | 0.2741 (0.2693) | 0.2443 (0.2059) | **1.12** | 0.4634 (0.4542) | 0.5230 (0.4867) | **0.89** | 697 / 489 | 0.997957 | 0.921546 / 0.921409 |
| 5 | 72 | 0.1447 (0.1369) | 0.2536 (0.2513) | **0.57** | 0.2736 (0.2721) | 0.2032 (0.1988) | **1.35** | 0.4473 (0.4426) | 0.4879 (0.4850) | **0.92** | 704 / 489 | 0.998105 | 0.921546 / 0.921543 |
| 10 | 32 | 0.3290 (0.3078) | 0.4423 (0.4268) | **0.74** | 0.2817 (0.2630) | 0.2602 (0.2248) | **1.08** | 0.6422 (0.6205) | 0.7264 (0.7242) | **0.88** | 1125 / 621 | 0.999179 | 0.942626 / 0.942226 |
| 10 | 36 | 0.3139 (0.3003) | 0.4661 (0.4590) | **0.67** | 0.2923 (0.2707) | 0.2249 (0.2202) | **1.30** | 0.6358 (0.6154) | 0.7290 (0.7174) | **0.87** | 1123 / 622 | 0.999177 | 0.942625 / 0.942230 |
| 10 | 72 | 0.2760 (0.2586) | 0.4801 (0.4787) | **0.57** | 0.2780 (0.2738) | 0.2714 (0.2188) | **1.02** | 0.5887 (0.5669) | 0.7835 (0.7418) | **0.75** | 1114 / 622 | 0.999143 | 0.942595 / 0.942269 |
| 20 | 32 | 0.7475 (0.6645) | 0.8490 (0.8412) | **0.88** | 0.3599 (0.3406) | 0.3424 (0.3331) | **1.05** | 1.1466 (1.0437) | 1.2256 (1.2229) | **0.94** | 1972 / 800 | 0.999822 | 0.955420 / 0.955363 |
| 20 | 36 | 0.7123 (0.6519) | 0.8410 (0.8023) | **0.85** | 0.3506 (0.3419) | 0.3373 (0.3303) | **1.04** | 1.1011 (1.0328) | 1.2169 (1.1818) | **0.90** | 1975 / 801 | 0.999813 | 0.955419 / 0.955360 |
| 20 | 72 | 0.6068 (0.6005) | 0.8836 (0.8804) | **0.69** | 0.3492 (0.3397) | 0.3434 (0.3279) | **1.02** | 0.9967 (0.9883) | 1.2699 (1.2636) | **0.78** | 1979 / 801 | 0.999854 | 0.955412 / 0.955407 |

### mnist

Times from the app's `benchmark.txt`, in seconds as `median (min)` over 5 measured runs; x = original / A1 on medians. Total = all clustering phases (excludes reading data and writing labels). Rand index from the first measured run.

| L | K | Phase I orig | Phase I A1 | x | core buckets orig | core buckets A1 | x | total orig | total A1 | x | peak RSS orig / A1 (MB) | Rand orig vs A1 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 58 | 32 | 0.9095 (0.9062) | 0.9403 (0.9369) | **0.97** | 0.8708 (0.8687) | 0.9463 (0.9442) | **0.92** | 1.7826 (1.7787) | 1.8884 (1.8854) | **0.94** | 544 / 526 | 0.990293 |
| 58 | 36 | 0.8162 (0.8060) | 0.8441 (0.8432) | **0.97** | 0.8715 (0.8700) | 0.9496 (0.9467) | **0.92** | 1.6898 (1.6782) | 1.7960 (1.7938) | **0.94** | 544 / 526 | 0.991405 |
| 58 | 72 | 0.4267 (0.4258) | 0.5143 (0.5097) | **0.83** | 0.9458 (0.9427) | 1.0206 (1.0182) | **0.93** | 1.3778 (1.3730) | 1.5419 (1.5357) | **0.89** | 535 / 527 | 0.990136 |
| 116 | 32 | 1.8030 (1.8020) | 1.8706 (1.8679) | **0.96** | 1.5422 (1.5357) | 1.7161 (1.7106) | **0.90** | 3.3484 (3.3429) | 3.5888 (3.5861) | **0.93** | 614 / 570 | 0.982197 |
| 116 | 36 | 1.6058 (1.6041) | 1.6808 (1.6778) | **0.96** | 1.5307 (1.5290) | 1.7372 (1.7286) | **0.88** | 3.1385 (3.1370) | 3.4194 (3.4170) | **0.92** | 615 / 570 | 0.983176 |
| 116 | 72 | 0.8451 (0.8441) | 1.0200 (1.0179) | **0.83** | 1.7302 (1.7220) | 1.8093 (1.5088) | **0.96** | 2.5797 (2.5715) | 2.8348 (2.5366) | **0.91** | 611 / 571 | 0.953504 |
| 230 | 32 | 3.5748 (3.5729) | 3.7089 (3.7021) | **0.96** | 2.5589 (2.5574) | 2.9208 (2.3660) | **0.88** | 6.1391 (6.1361) | 6.6272 (6.0859) | **0.93** | 749 / 656 | 0.971775 |
| 230 | 36 | 3.1777 (3.1743) | 3.3346 (3.3247) | **0.95** | 2.5456 (2.5373) | 2.9341 (2.9099) | **0.87** | 5.7242 (5.7169) | 6.2800 (6.2426) | **0.91** | 750 / 656 | 0.973147 |
| 230 | 72 | 1.7791 (1.7116) | 2.0123 (1.9991) | **0.88** | 2.7633 (2.5141) | 3.0836 (2.7515) | **0.90** | 4.5590 (4.2301) | 5.0965 (4.7716) | **0.89** | 754 / 656 | 0.970408 |


### Run-to-run label variation (reference)

`self_variation.csv`: each variant was run twice with identical inputs at K = 36, and the Rand index between the two runs was computed. With several threads, the original's union-find is already non-deterministic.

| dataset | L | original vs original | A1 vs A1 | original vs A1 (`accuracy.csv`, K=36) |
|---|---|---|---|---|
| household | 5 | 0.999746 | 0.999657 | 0.997957 |
| household | 10 | 0.999910 | 0.999941 | 0.999177 |
| household | 20 | 0.999989 | 0.999960 | 0.999813 |
| mnist | 58 | 0.993136 | 0.997436 | 0.991405 |
| mnist | 116 | 0.991703 | 0.988539 | 0.983176 |
| mnist | 230 | 0.993653 | 0.980718 | 0.973147 |

On household, every value is ≥ 0.998. On MNIST, each variant varies between its own runs by 0.3–2% of Rand index, and the two variants differ by 1–5% (0.954–0.991 over all K; the lowest value is L=116, K=72). The differences between variants are slightly larger than the variation within each one, but with a single run per case there isn't enough to call that a systematic effect.

### Before/after the fix (medians, seconds)

| dataset | L | K | Phase I orig | Phase I A1 v1 (table by table) | Phase I A1 v2 (grouped keys) | total orig | total A1 v1 | total A1 v2 | peak RSS A1 v1 / v2 (MB) |
|---|---|---|---|---|---|---|---|---|---|
| household | 5 | 32 | 0.169 | 0.249 | **0.250** | 0.472 | 0.530 | **0.526** | 411 / 489 |
| household | 5 | 36 | 0.160 | 0.255 | **0.249** | 0.463 | 0.523 | **0.523** | 411 / 489 |
| household | 5 | 72 | 0.145 | 0.275 | **0.254** | 0.447 | 0.506 | **0.488** | 411 / 489 |
| household | 10 | 32 | 0.329 | 0.476 | **0.442** | 0.642 | 0.770 | **0.726** | 498 / 621 |
| household | 10 | 36 | 0.314 | 0.494 | **0.466** | 0.636 | 0.753 | **0.729** | 497 / 622 |
| household | 10 | 72 | 0.276 | 0.527 | **0.480** | 0.589 | 0.830 | **0.784** | 496 / 622 |
| household | 20 | 32 | 0.748 | 0.944 | **0.849** | 1.147 | 1.322 | **1.226** | 675 / 800 |
| household | 20 | 36 | 0.712 | 0.954 | **0.841** | 1.101 | 1.331 | **1.217** | 676 / 801 |
| household | 20 | 72 | 0.607 | 1.036 | **0.884** | 0.997 | 1.418 | **1.270** | 676 / 801 |
| mnist | 58 | 32 | 0.910 | 1.174 | **0.940** | 1.783 | 2.110 | **1.888** | 500 / 526 |
| mnist | 58 | 36 | 0.816 | 1.152 | **0.844** | 1.690 | 2.102 | **1.796** | 500 / 526 |
| mnist | 58 | 72 | 0.427 | 1.244 | **0.514** | 1.378 | 2.232 | **1.542** | 501 / 527 |
| mnist | 116 | 32 | 1.803 | 2.279 | **1.871** | 3.348 | 3.975 | **3.589** | 517 / 570 |
| mnist | 116 | 36 | 1.606 | 2.304 | **1.681** | 3.138 | 4.018 | **3.419** | 517 / 570 |
| mnist | 116 | 72 | 0.845 | 2.409 | **1.020** | 2.580 | 4.202 | **2.835** | 517 / 571 |
| mnist | 230 | 32 | 3.575 | 4.371 | **3.709** | 6.139 | 6.736 | **6.627** | 550 / 656 |
| mnist | 230 | 36 | 3.178 | 4.279 | **3.335** | 5.724 | 6.672 | **6.280** | 550 / 656 |
| mnist | 230 | 72 | 1.779 | 4.406 | **2.012** | 4.559 | 6.963 | **5.096** | 551 / 656 |

## Interpretation

- **Memory: A1 wins in every case, and the gain grows with L.**
  - CSR stores 4 B per entry plus a small per-bucket cost. The hash maps store a pointer per entry, plus a node, a `HashedPoint` key and a `concurrent_vector` per bucket.
  - v2 adds the group key buffer, at most 128 MB.
  - Household L=20: 1975 → 801 MB (−59%).
  - MNIST L=230: 750 → 656 MB (−13%).
- **Phase I on MNIST: practically tied with the original at K = 32–36 (0.95–0.97×), 0.83–0.88× at K = 72.**
  - With the fix, A1 reads each point once, like the original.
  - The remaining difference is that A1 sorts the tables one after another, and 60,000 points per table is little work for 72 threads.
  - Keys (the 784-dimensional inner products) dominate Phase I on MNIST, so the bucketing structure barely matters there.
- **Phase I on household: A1 is still slower (0.57–0.88×).**
  - The Euclidean keys are 64-bit mixes that vary in every byte, so the radix sort makes all 8 passes over 2M keys per table, which is limited by memory bandwidth.
  - The original only inserts into hash maps, and its cost is spread over the batches.
  - The gap shrinks as L grows (0.64× at L=5 → 0.85× at L=20, K=36), because the hash maps' allocations grow with L.
- **Core-bucket identification:**
  - **Household:** CSR locality helps (1.02–1.35×).
  - **MNIST:** A1 is 4–13% slower, and this isn't explained yet. This phase's code is the same in v1 and v2, but at L=230 it measured 1.05–1.07× in v1 and 0.87–0.90× in v2, so it varies by about ±10% between sessions.
- **Overall:**
  - `-s` is a clear memory saving at a moderate time cost: total 0.75–0.94×.
  - To also win in time, the next steps would be:
    - sort several tables in parallel, for small N per table;
    - reduce radix passes for Euclidean keys, for example by compacting the keys before sorting.

## Notes on the original code (not changed)

- **Candidate tracking:** the paper's line 15 (insert into `candidateCoreBuckets` during Phase I) isn't implemented. `identifyCoreBuckets*` re-scans every map afterwards.
- **Build flags:** `CMakeLists.txt` overwrites `-Wall -Wextra` with `set(CMAKE_CXX_FLAGS "-std=c++11 -O3")`. No `NDEBUG` is set, so `assert`s stay on in "Release".
- **Batch sizing:** `batchSize = round(N / numberOfBatches)` does integer division before `round`. With N < 100, the batches are empty and `assert((end - begin) > 0)` aborts.
- **`HashedPoint` layout:** it inherits from `BasePoint` and shadows `features`, so every key carries an unused `std::vector<double>`.
- **Euclidean hash:** `⌊⟨p,h⟩/ε⌋` with no random offset b.
- **Angular distance:** `angularDistance` divides by `3.14` instead of π.
- **RNG:** `RandGenerator` leaks its `normal_distribution`, and it has no seed option.
- **Dead code:** `ConcurrentLSHDBSCAN::performMergeTasks` returns immediately.

## Files

| File | Contents |
|---|---|
| `correctness.txt` | Equivalence checks on real data with v2 (18/18 PASS) |
| `app_runs.csv` | Every run: per-phase times from `benchmark.txt`, peak RSS, wall time (rep 0 = warm-up) |
| `accuracy.csv` | Rand index per (dataset, L, K): original vs A1, and both vs exact DBSCAN (household) |
| `self_variation.csv` | Rand index of each variant against itself (K = 36) |
| `app_summary.csv`, `app_results_tables.md` | Medians and minimums per configuration (generated) |
| `app_scaling_*.png`, `app_memory_rss_vs_L.png` | Plots (generated) |
| `v1_vs_v2.md`, `v1_table_by_table/` | Before/after the key-grouping fix, and v1's raw data |
| `scripts/` | `configs.sh` (datasets, L, K), `run_integration_check.sh`, `run_app_bench.sh`, `run_self_variation.sh`, `rand_index.py`, `analyze_app.py` |

To reproduce (the plotting script needs the matplotlib virtualenv at `~/.venvs/a1plot`):
```bash
resman run --msg="A1 checks" -- scripts/run_integration_check.sh
resman run --msg="A1 bench"  -- scripts/run_app_bench.sh household mnist
resman run --msg="A1 ref"    -- scripts/run_self_variation.sh
~/.venvs/a1plot/bin/python scripts/analyze_app.py .
```
