# Changelog

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
