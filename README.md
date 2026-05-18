# HaikuBench

**System Benchmark Suite for Haiku OS**

HaikuBench is a native Haiku application that measures the performance of CPU,
memory, cache, kernel primitives, messaging, 2D graphics, OpenGL and Vulkan.
It produces reproducible results with statistical analysis (adaptive sampling,
trimmed mean, p50/p95, CoV) and exports them in Markdown and JSON format for
easy comparison and leaderboard aggregation.

Each machine gets a unique hardware fingerprint (Machine ID) so results from
different systems can be collected and ranked.

## Screenshots

![HaikuBench](HaikuBench_v0.1.0.jpg)

## Quick Start

```sh
# install dependencies
pkgman install mesa_devel glu_devel

# optional: Vulkan support
pkgman install vulkan_devel mesa_lavapipe shaderc_devel

# build and run
make
./HaikuBench
```

## Benchmarks

### System Bench — 20 tests, adaptive sampling

Every test warms up the CPU for 200 ms, then samples adaptively: at
least 3 runs, up to 10, stopping as soon as the **coefficient of
variation** drops below 2%. The UI and exports report the trimmed
mean (robust to a single outlier via the MAD rule) along with stddev,
p50, p95 and the actual number of samples taken. JSON export also
carries min/max, CoV and trimmed mean for automated leaderboards.

| Section | Tests | What it measures |
|---------|-------|------------------|
| **CPU** (3) | Integer, Float, Multi-thread | Arithmetic throughput, transcendentals, HT/multi-core scaling |
| **Memory** (4) | Seq Read, Seq Write, Copy, Latency | DDR bandwidth and random-access latency (pointer-chasing) |
| **Cache** (3) | L1, L2, L3 | Per-level bandwidth with 8 independent accumulators |
| **Kernel** (8) | Semaphores, Threads, Ports, Areas, Atomics, Syscalls | Haiku kernel primitive throughput and overhead |
| **Messaging** (2) | BMessage Flatten, BLooper | Haiku IPC serialization and looper round-trip |

### 2D Benchmark — 24 tests, 6 levels

All rendering through BView, no GPU required. Detects whether 2D
hardware acceleration is active by identifying the accelerant driver
via `BScreen::GetDeviceInfo()` and `ioctl B_GET_ACCELERANT_SIGNATURE`.
Reports device name, chipset, VRAM and individual hook availability
(FillRect, ScreenBlit, InvertRect).

| Level | Theme | Tests |
|-------|-------|-------|
| 1 | Basic Fills | FillRect, StrokeRect, FillRoundRect, StrokeLine |
| 2 | Geometry | FillTriangle, FillEllipse, FillPolygon, DrawString |
| 3 | Compositing | BShape curves, gradients, alpha blending, bitmap scaling |
| 4 | Advanced | 24-point star, clipped regions, conic gradients, text storm |
| 5 | Extreme | Nebula particles, fractal trees, plasma renderer, everything combined |
| 6 | Software 3D | Teapot wireframe, flat-shaded, x4 rotating, x16 army |

### GPU Benchmark — 6 tests, OpenGL

| Test | Description |
|------|-------------|
| Fill Rate | 200 full-screen quads per frame |
| Geometry | 10 000 small triangles per frame |
| Texture | 256x256 re-upload + 50 textured quads |
| Alpha Blending | 300 semi-transparent overlapping quads |
| Stencil Buffer | 100 stencil masks + 150 masked draws |
| Combined Stress | 25 rotating lit objects (teapots, spheres, tori) |

Includes a live rotating teapot preview during idle.

### Vulkan Benchmark — 4 tests

| Test | Description |
|------|-------------|
| Memory Bandwidth | Host-visible 64 MB buffer write throughput |
| Compute Shader | 1M invocations, 256 FMA ops each (GFLOPS) |
| Buffer Copy | Device-to-device 32 MB vkCmdCopyBuffer |
| Buffer Fill | 64 MB vkCmdFillBuffer |

Vulkan is loaded dynamically at runtime. If the Vulkan loader or a driver
(e.g. mesa_lavapipe) is not installed, the benchmark shows an error message
instead of crashing. The compute shader is compiled from GLSL at runtime
using glslc (shaderc).

### Teapot 3D — interactive OpenGL demo

Renders 1 to 64 Utah Teapots with Phong lighting via BGLView. Use the +/-
buttons to add or remove teapots and watch the FPS counter.

## 2D Hardware Acceleration Detection

The 2D benchmark panel reports the video card's acceleration status:

- **Driver**: active accelerant name (e.g. `intel_extreme.accelerant`)
- **Device**: GPU name, chipset and VRAM from `BScreen::GetDeviceInfo()`
- **2D Acceleration**: ACTIVE (hardware) or NONE (software rendering)
- **Hooks**: FillRect, ScreenBlit, InvertRect availability

Detection uses `ioctl B_GET_ACCELERANT_SIGNATURE` on `/dev/graphics/`
to identify the active driver, then classifies it against the known set
of Haiku accelerant drivers:

| Driver | 2D Acceleration |
|--------|-----------------|
| intel_extreme, radeon_hd, radeon, nvidia, ati, matrox, via, intel_810 | Hardware accelerated |
| vesa, framebuffer, virtio_gpu | Software only |

## Export

Clicking **Export .md** generates two files on the Desktop:

- `HaikuBench_YYYY-MM-DD_HH-MM-SS.md` — human-readable report
- `HaikuBench_YYYY-MM-DD_HH-MM-SS.json` — machine-readable data

Both files include:

- App version, date, Machine ID
- Full system info (CPU, cores, RAM, OS version)
- All benchmark results with mean, stddev, p50, p95, CoV
- Overall composite score broken down by subsystem

### JSON structure

```json
{
  "format_version": 1,
  "app_version": "1.1.0",
  "machine_id": "HB-A3F7B21C",
  "hardware": "Intel Core i3 M 370 | 4 cores | 3757 MB RAM",
  "os": "Haiku hrev59722",
  "system_bench": {
    "valid": true,
    "runs": 5,
    "results": {
      "cpu_integer_mops": {
        "mean": 59.10, "stddev": 0.30,
        "median": 59.05, "p95": 59.50,
        "min": 58.70, "max": 59.60,
        "cov": 0.005, "trimmed_mean": 59.08,
        "n_samples": 3, "n_dropped": 0
      },
      ...
    }
  },
  "graphics": {
    "teapot": "Teapot 3D: 25 FPS (4 teapots)",
    "bench_2d": "2D Bench: 18795 fill/s | nebula 5215.3/s | extreme 14.3/s",
    "gpu_opengl": "GPU Bench: 24.1 FPS (...)",
    "vulkan": null
  }
}
```

## Temperature Monitoring

ACPI thermal zones are read in real-time and displayed:

- In the main window (System panel)
- As an overlay on all OpenGL benchmark windows
- In the 2D and GPU benchmark result windows

Colors: green (< 65 C), yellow (65-80 C), red (> 80 C).

## Running the test suite

```sh
./tests/run_all.sh
```

Builds and runs all Phase A unit tests (BenchStats, AdaptiveRunner,
BenchWarmup) plus an integration smoke test (114 checks total).

## Architecture

```
HaikuBench/
├── HaikuBench              binary (with HVIF icon)
├── icon.hvif               application icon
├── Makefile
├── LICENSE                 MIT
├── README.md
├── CHANGELOG.md
├── haikubench-1.1.0.recipe  HaikuPorts recipe
├── WattHaiku.cpp           BApplication entry point
├── MainWindow.h/cpp        Main window, export, score
├── SysBenchmark.h/cpp      20 system tests
├── AdaptiveRunner.h/cpp    Convergence-driven multi-run sampler
├── BenchStats.h/cpp        Statistics (mean, median, p95, CoV, trimmed mean)
├── BenchWarmup.h/cpp       CPU warm-up and buffer touch
├── CpuDatabase.h/cpp       CPUID brand string detection
├── TeapotWindow.h/cpp      OpenGL teapot demo
├── Bench2DWindow.h/cpp     24 BView rendering tests + 2D accel detection
├── GpuBenchWindow.h/cpp    6 OpenGL tests + teapot preview
├── VulkanBenchWindow.h/cpp 4 Vulkan compute tests
├── TempOverlay.h/cpp       ACPI temperature overlay
├── Version.h               Version constant
└── tests/                  Unit and integration tests
```

## Requirements

- Haiku OS (x86_64), R1/beta5 or newer
- mesa_devel, glu_devel (build)
- vulkan_devel (build, optional)
- mesa_lavapipe, shaderc_devel (runtime, optional — for Vulkan tests)

## Author

Andrea Bernardi

## License

MIT License. See [LICENSE](LICENSE).
