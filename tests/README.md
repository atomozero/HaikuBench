# Test Suite

114 checks across 4 test suites, covering the statistical pipeline
used by the system benchmark.

## Running

```sh
./tests/run_all.sh
```

Builds and runs all suites. Exits non-zero on first failure.

## Suites

### test_bench_stats (45 checks)

Unit tests for `BenchStats` — the statistics module that computes
mean, median, p95, stddev, CoV, and trimmed mean from benchmark
samples.

| Test | What it verifies |
|------|------------------|
| empty / NULL input | Returns invalid, no crash |
| all-invalid samples | Recognizes all negative values as invalid |
| single sample | Correct stats when n=1 (stddev=0, cov=0) |
| known values {10,20,30,40,50} | Mean=30, median=30, stddev=sqrt(250), p95=48 |
| unsorted input | Percentiles computed correctly regardless of input order |
| mixed valid/invalid | Drops negatives, computes over valid subset |
| trimmed mean with outlier | Raw mean contaminated (>150), trimmed mean clean (<105) |
| trimmed mean on clean data | Trimmed mean equals raw mean when no outliers |
| CoV helper | Matches manual calculation; 0 on constant data |
| CoV with invalid samples | Ignores negatives in CoV computation |

### test_adaptive_runner (24 checks)

Unit tests for `AdaptiveRunner` — the convergence-driven sampling
loop that decides how many times to run each benchmark.

| Test | What it verifies |
|------|------------------|
| stable data | Stops at minRuns (CoV already below threshold) |
| constant data | Stops at exactly minRuns, CoV=0 |
| noisy-then-stable | Takes more than minRuns, converges before maxRuns |
| always noisy | Hits maxRuns cap, reports high CoV |
| all-invalid returns | Reports invalid without spinning forever |
| NULL function | Returns invalid, no crash |
| maxRuns > kMaxSamples | Clamped to kMaxSamples (32) |
| progress callback | Called once per sample with monotonic count |

### test_bench_warmup (25 checks)

Unit tests for `BenchWarmup` — CPU warm-up and buffer touch
utilities.

| Test | What it verifies |
|------|------------------|
| SpinMs(50) timing | Elapsed between 45ms and 500ms |
| SpinMs(0) and SpinMs(-5) | Returns 0, no-op |
| TouchBuffer(NULL, ...) | No crash on NULL pointer |
| TouchBuffer on 3-page buffer | Bytes at page boundaries written |
| Longer spin > shorter spin | SpinMs(80) elapsed > SpinMs(20) elapsed |

### test_phase_a_integration (20 checks)

Integration smoke test exercising the full pipeline: warm-up,
adaptive runner, and statistics on a synthetic CPU workload (~50ms
per sample).

| Test | What it verifies |
|------|------------------|
| pipeline produces valid stats | All fields populated, ordering invariants hold (min <= median <= p95 <= max), CoV < 30%, trimmed mean in [min, max] |
| progress callback counts | Monotonically increasing, final count matches nSamples |

## Adding tests

Each suite is a standalone C++ file with no external test framework.
The pattern is:

```cpp
#define CHECK(cond) do { \
    sChecks++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d : %s\n", __FILE__, __LINE__, #cond); \
        sFailures++; \
    } \
} while (0)
```

Add new test functions, call them from `main()`, and update
`run_all.sh` with the build command.
