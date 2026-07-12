# Changelog

## 1.2.0 — 2026-07-12

### GPU Benchmark: hardware acceleration proof

- Acceleration badge: classifies the active OpenGL renderer as hardware
  or software from `GL_RENDERER` (llvmpipe/softpipe/swrast = software)
  and displays the verdict prominently
- Automatic hardware-vs-software comparison: on a hardware renderer the
  same six tests are re-run in a child process on the software renderer
  (`HGL_SOFTWARE=1`, executed from a temporary copy of the binary since
  the app is B_SINGLE_LAUNCH) and shown as GPU / CPU / speedup columns
- CPU load measured during each pass via `get_team_usage_info()`:
  near-idle CPU on a GPU pass vs all cores saturated on a software pass
- Comparison summary in the main window results and exports
  (`gpu_speedup`, `gpu_sw_score`, `gpu_cpu_load`, `gpu_hardware`)

## 1.1.0 — 2026-05-18

### 2D Hardware Acceleration Detection

- Detect active accelerant driver via `ioctl B_GET_ACCELERANT_SIGNATURE`
  on `/dev/graphics/` and classify as hardware-accelerated or software-only
- Query device name, chipset and VRAM via `BScreen::GetDeviceInfo()`
- Display FillRect, ScreenBlit, InvertRect hook availability
- Known driver classification: intel_extreme, radeon_hd, nvidia, ati,
  matrox, via = hardware; vesa, framebuffer, virtio_gpu = software
- Fixed race condition: benchmark results are now cached before the
  drawing window is destroyed, preventing BString use-after-free

### Statistical Analysis — Phase A

- **A1 — BenchStats module**: new `BenchStats.h` / `BenchStats.cpp` providing
  a pure, testable statistics layer for benchmark samples.
  - Mean, median (p50), **p95**, min, max
  - Sample stddev (Bessel n−1) and coefficient of variation (CoV)
  - **Robust trimmed mean** using the MAD rule (3 × 1.4826 × median
    absolute deviation), which correctly rejects outliers even when a
    single bad sample inflates stddev
  - Handles invalid samples (`< 0.0f`) transparently, matching the
    existing SysBenchmark convention
  - 45-check unit test suite (`tests/test_bench_stats.cpp`), build with
    `g++ -std=c++14 -O2 -Wall -Wextra -o tests/test_bench_stats
    tests/test_bench_stats.cpp BenchStats.cpp`

- **A2 — Adaptive sampling**: new `AdaptiveRunner.h` / `AdaptiveRunner.cpp`.
  Replaces the fixed 3-runs-per-test loop with a **convergence-driven**
  runner:
  - Always takes `minRuns` samples (default 3), then keeps sampling as
    long as CoV is above `targetCoV` (default 2%), up to `maxRuns`
    (default 10).
  - Stable tests terminate as early as 3 runs; noisy tests automatically
    get more samples. Upper bound is `kMaxSamples` (32) regardless of
    the caller's request, which prevents pathological configurations.
  - Optional `ProgressFunc` callback for UI "run k / up to N, CoV x.x%".
  - 24-check unit test suite (`tests/test_adaptive_runner.cpp`) covering
    constant input, stable input, noisy-then-stable input, always-noisy
    input (maxRuns cap), all-invalid input, NULL function guard and
    `kMaxSamples` clamping.

- **A3 — Warm-up primitives**: new `BenchWarmup.h` / `BenchWarmup.cpp`.
  - `BenchWarmup::SpinMs(ms)` drives the CPU to peak frequency and
    resolves lazy initialisation paths before measurement. Uses a
    `volatile` accumulator with an asm barrier so `-O2` cannot elide
    the loop. Default warm-up duration is 200 ms
    (`BenchWarmup::kDefaultWarmupMs`).
  - `BenchWarmup::TouchBuffer(ptr, size)` maps one byte per page of a
    buffer, so cache/TLB latency does not contaminate the first timed
    pass of memory-bound tests.
  - 25-check unit test suite (`tests/test_bench_warmup.cpp`).

- **A4 — Integration into SysBenchmark**: the system benchmark now uses
  the Phase A stack end-to-end.
  - `SysBenchmark::Run` calls `BenchWarmup::SpinMs(200)` before every
    test and delegates sampling to `AdaptiveRunner::Run` (min 3, max
    10 runs, target CoV 2%).
  - `SysBenchResults` gains a `stats[20]` array of full `BenchStats`
    (p50, p95, min, max, CoV, trimmed mean, n samples, n dropped)
    alongside the original flat `mean` / `stddev` fields, which stay
    for backward compatibility. The flat field is now populated with
    the **trimmed mean** — identical to the raw mean on clean data,
    but automatically rejects a single contaminated run (MAD rule).
  - **Markdown export** gains per-test columns for p50, p95, CoV and
    n (actual sample count). The old "N runs per test" header becomes
    a methodology note: "adaptive sampling: 3 to 10 runs, stop when
    CoV < 2%".
  - **JSON export** now carries `median`, `p95`, `min`, `max`, `cov`,
    `trimmed_mean`, `n_samples`, `n_dropped` for every test, enabling
    richer leaderboards and automated regression analysis.
  - Integration smoke test (`tests/test_phase_a_integration.cpp`)
    validates the full warm-up → runner → stats pipeline against a
    realistic synthetic workload (20 checks, ~0.7 s).
  - Test orchestrator: `tests/run_all.sh` builds and runs all Phase A
    test suites (114 checks total).


## 1.0.0 — 2026-03-26

First public release.

### System Benchmark (20 tests, 3 runs each)

- **CPU**: Integer (MOPS), Float (MFLOPS), Multi-thread speedup ratio
- **Memory**: Sequential Read/Write, Copy (MB/s), Latency (ns)
  - Pointer-chasing with random chain to defeat prefetchers
  - ASM barriers to prevent compiler optimization
- **Cache**: L1/L2/L3 bandwidth with 8 independent accumulators
  - Batched passes to minimize system_time() overhead on small buffers
- **Kernel**: Semaphore create/delete/acquire/release/contention,
  Thread spawn, Port send/recv, Area create/delete, Atomic ops,
  Syscall overhead
- **Messaging**: BMessage flatten/unflatten, BLooper ping-pong

### 2D Benchmark (24 tests, 6 levels)

- Level 1: FillRect, StrokeRect, FillRoundRect, StrokeLine
- Level 2: FillTriangle, FillEllipse, FillPolygon, DrawString
- Level 3: BShape, gradients, alpha blending, bitmap scaling
- Level 4: 24-point star, clipped regions, conic gradients, text storm
- Level 5: Nebula particles, fractal trees, plasma renderer, everything combined
- Level 6: Full software 3D teapot renderer (wireframe, flat-shaded, x4, x16)
- Two-column layout matching main window style

### GPU Benchmark (6 tests, OpenGL)

- Fill rate, geometry, texture upload, alpha blending, stencil, combined stress
- Live rotating teapot preview (Phong shading, 30 FPS animation)
- Fixed 320x240 rendering viewport for consistent results
- GPU Device panel with Vendor, Renderer, OpenGL version, Mesa/driver version

### Vulkan Benchmark (4 tests)

- Memory bandwidth (host-visible buffer write)
- Compute shader (1M invocations, 256 FMA/thread, GFLOPS)
- Buffer copy (device-to-device vkCmdCopyBuffer)
- Buffer fill rate (vkCmdFillBuffer)
- Vulkan loaded dynamically via load_add_on (works without Vulkan installed)
- Shader compiled at runtime via glslc (shaderc)

### Teapot 3D Benchmark

- 1-64 Utah Teapots via BGLView + glutSolidTeapot
- Real-time FPS counter, add/remove with +/- buttons

### Statistics

- Each system test runs 3 times (configurable via kBenchRuns)
- Mean and standard deviation computed per test
- Results displayed as mean +/- stddev in UI and exports

### Export

- Markdown (.md) with full results table, machine ID, version, overall score
- JSON (.json) with mean/stddev per test for machine parsing and leaderboards
- Machine ID: FNV-1a hash of CPU + cores + RAM for hardware fingerprinting
- Overall composite score (CPU, Memory, Cache, Kernel, Messaging sub-scores)

### Temperature Monitoring

- ACPI thermal zone readout in real-time
- Displayed in main window and as overlay on OpenGL benchmark windows
- Color-coded: green (< 65 C), yellow (65-80 C), red (> 80 C)

### UI

- Dark theme (#0D1117) with consistent color palette across all windows
- Monospaced font (be_fixed_font 11pt) for aligned result columns
- BBox panels with titled sections (System, Benchmark Results)
- Section-colored headers, white result rows
- Unified style across System, 2D, GPU, and Vulkan benchmark windows

### Packaging

- HVIF icon on binary
- Makefile with src/obj directory structure
- Application signature: application/x-vnd.HaikuBench
