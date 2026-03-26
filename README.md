# HaikuBench

**System Benchmark Suite for Haiku OS**

HaikuBench is a native Haiku application that measures the performance of CPU,
memory, cache, kernel primitives, messaging, 2D graphics, OpenGL and Vulkan.
It produces reproducible results with statistical analysis (mean and standard
deviation over multiple runs) and exports them in Markdown and JSON format for
easy comparison and leaderboard aggregation.

Each machine gets a unique hardware fingerprint (Machine ID) so results from
different systems can be collected and ranked.

## Screenshots

The main window shows system information, benchmark results with standard
deviation, and buttons to launch each benchmark individually.

## Quick Start

```sh
# install dependencies
pkgman install mesa_devel glu_devel glut_devel

# optional: Vulkan support
pkgman install vulkan_devel mesa_lavapipe shaderc_devel

# build and run
make
./HaikuBench
```

## Benchmarks

### System Bench — 20 tests, 3 runs each

Every test runs 3 times. The UI and exports report mean +/- standard deviation.

| Section | Tests | What it measures |
|---------|-------|------------------|
| **CPU** (3) | Integer, Float, Multi-thread | Arithmetic throughput, transcendentals, HT/multi-core scaling |
| **Memory** (4) | Seq Read, Seq Write, Copy, Latency | DDR bandwidth and random-access latency (pointer-chasing) |
| **Cache** (3) | L1, L2, L3 | Per-level bandwidth with 8 independent accumulators |
| **Kernel** (8) | Semaphores, Threads, Ports, Areas, Atomics, Syscalls | Haiku kernel primitive throughput and overhead |
| **Messaging** (2) | BMessage Flatten, BLooper | Haiku IPC serialization and looper round-trip |

### 2D Benchmark — 24 tests, 6 levels

All rendering through BView, no GPU required.

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

## Export

Clicking **Export .md** generates two files on the Desktop:

- `HaikuBench_YYYY-MM-DD_HH-MM-SS.md` — human-readable report
- `HaikuBench_YYYY-MM-DD_HH-MM-SS.json` — machine-readable data

Both files include:

- App version, date, Machine ID
- Full system info (CPU, cores, RAM, OS version)
- All benchmark results with mean and standard deviation
- Overall composite score broken down by subsystem

### JSON structure

```json
{
  "format_version": 1,
  "app_version": "1.0.0",
  "machine_id": "HB-A3F7B21C",
  "hardware": "Intel Core i3 M 370 | 4 cores | 3757 MB RAM",
  "os": "Haiku hrev59543",
  "system_bench": {
    "valid": true,
    "runs": 3,
    "results": {
      "cpu_integer_mops": {"mean": 59.10, "stddev": 0.30},
      "cpu_float_mflops": {"mean": 15.20, "stddev": 0.10},
      ...
    }
  },
  "graphics": {
    "teapot": "Teapot 3D: 25 FPS (4 teapots)",
    "gpu_opengl": "GPU Bench: 24.1 FPS (...)",
    "vulkan": null
  }
}
```

## Temperature Monitoring

ACPI thermal zones are read in real-time and displayed:

- In the main window (System panel)
- As an overlay on all OpenGL benchmark windows

Colors: green (< 65 C), yellow (65-80 C), red (> 80 C).

## Architecture

```
HaikuBench/
├── HaikuBench              binary (with HVIF icon)
├── icon.hvif               application icon
├── Makefile                src/ → obj/ build
├── LICENSE                 MIT
├── README.md
├── CHANGELOG.md
└── src/
    ├── WattHaiku.cpp           BApplication entry point
    ├── MainWindow.h/cpp        Main window, export, score
    ├── SysBenchmark.h/cpp      20 system tests, multi-run stats
    ├── CpuDatabase.h/cpp       CPUID brand string detection
    ├── TeapotWindow.h/cpp      OpenGL teapot demo
    ├── Bench2DWindow.h/cpp     24 BView rendering tests
    ├── GpuBenchWindow.h/cpp    6 OpenGL tests + teapot preview
    ├── VulkanBenchWindow.h/cpp 4 Vulkan compute tests
    ├── TempOverlay.h/cpp       ACPI temperature overlay
    └── Version.h               Version constant
```

## Requirements

- Haiku OS (x86_64)
- mesa_devel, glu_devel, glut_devel
- Optional: vulkan_devel, mesa_lavapipe, shaderc_devel

## Author

Andrea Bernardi

## License

MIT License. See [LICENSE](LICENSE).
