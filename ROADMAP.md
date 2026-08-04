# HaikuBench Roadmap

Living document. Tracks where the project is, what is inconsistent, and what
ships next. Dates are absolute.

## Where we are — 1.3.0 (released 2026-08-04, tagged `v1.3.0`)

- System benchmark: 20 tests, adaptive sampling, full statistics (p50/p95/CoV).
- 2D benchmark: 24 tests, accelerant detection.
- GPU benchmark: OpenGL, hardware-vs-software A/B proof.
- Vulkan compute benchmark (optional, dynamically loaded).
- Teapot 3D stress test.
- SPEC-style score across 8 categories (baseline = 1000).
- Markdown + JSON export.
- Tabbed **benchmark deck** (single window, four `BView` panels) — replaces the
  former four standalone windows.
- Slate `HeaderView` banner on the main window and the deck.
- **Demoscene splash screen** — pure app_server 2D, no GL dependency.
- **Localized** with the Haiku Locale Kit (English + Italian, 77 keys).
- **Scriptable via `hey`** (Score / Results / RunAll / SystemBench).
- **Settings menu** with a persistent store (splash toggle); `make` embeds the
  signature/version/icon resources into the binary.

## Coherence audit (2026-08-03)

Findings that motivated the 1.3.0 work:

1. **Version drift.** `Version.h` said 1.2.0 but `HaikuBench.rdef` still declared
   `app_version { major=1, middle=1, minor=0 }`. The `make` build only attaches
   the icon attribute — it never applies the rdef — so a locally built binary
   carried no signature/version resource at all; only the recipe applied them.
   → Single source of truth for the version, and `make` now applies the rdef.
2. **No Haiku localization.** Every user-facing string was a hardcoded literal;
   the Locale Kit (`liblocalestub`, `BCatalog`, `B_TRANSLATE`) was not used.
   → Adopt the Haiku Locale Kit. Wrap UI *chrome* only — window titles, buttons,
   section headers, status lines, alerts, About box. **Test names and export
   field keys stay ASCII and stable**, because they are also JSON/Markdown keys
   consumed by leaderboards and regression tooling; translating them would break
   parsers. This boundary is intentional and documented here.
3. **Not scriptable (`hey`).** No `GetSupportedSuites` / `ResolveSpecifier`, so
   the app could not be driven or queried from the command line.
   → Add a scripting suite so `hey HaikuBench` can run benchmarks and read the
   score/results for automation and CI.
4. **Naming mismatch (accepted).** The four panel classes (`Bench2DPanel`,
   `GpuBenchPanel`, `VulkanBenchPanel`, `TeapotPanel`) still live in files named
   `*Window.cpp/.h`. Left as-is to avoid churn; noted so it is not mistaken for a
   bug. May be renamed in a later cleanup pass.
5. **Stray working notes** in the tree (`pluto.md`, `graph.json`,
   `PROMPT_HAIKU_GPU_BENCH.md`, `smp-fix-ticket.md`) — author scratch, not part
   of the product. Left in place (they may be in active use); added to
   `.gitignore` candidates / flagged here for the author to prune. Not shipped
   by the recipe regardless.
6. **Hard GL link.** The binary links `libGL/libGLU/libglut` directly, so it
   will not launch at all on an install with no Mesa — even though only the GPU
   and Teapot tabs actually need GL. The new splash screen is deliberately
   GL-free (pure app_server 2D) so it is ready for a future where GL is loaded
   lazily. → Backlog: dynamically load GL and disable the GL-only tabs when it
   is absent, so system/2D benchmarks + splash run everywhere.

## 1.3.0 — shipped (2026-08-04)

Theme: **first impression + integration**. Make the app feel finished and make
it a first-class Haiku citizen (localized, scriptable, coherently packaged).

- [x] **Demoscene splash screen.** A `SplashWindow` shown on launch: a rotating
  red Utah teapot with the word `HaikuBench` orbiting it, over a 64k-intro
  backdrop (starfield + drifting blobs), with a sine-wave scroller. Auto-
  dismisses; click/key skips; then the main window appears. Rendered entirely
  with the app_server 2D API (a tiny software 3D teapot renderer) — **no GL
  dependency**, so it runs even where Mesa/OpenGL is absent.
- [x] **Settings menu.** A menu bar with a persistent **Settings** store
  (`Settings` module → flattened `BMessage` under `B_USER_SETTINGS_DIRECTORY`).
  First item: "Show splash screen at startup", which `SplashWindow::Launch`
  consults alongside the `HAIKUBENCH_NOSPLASH` env override.
- [x] **Haiku Locale Kit localization.** `B_TRANSLATE` across the UI chrome,
  `en` + `it` catalogs (77 keys, 7 contexts), `make catkeys`/`make catalogs`
  targets, `liblocalestub` linked, catalog installed by the recipe.
- [x] **`hey` scripting suite.** `Score` (GET), `Results` (GET, JSON string),
  and `RunAll` / `SystemBench` (EXECUTE) on the main window.
- [x] **Coherence + packaging.** rdef/Version synced to 1.3.0, `make` embeds the
  rdef resources, recipe → 1.3.0 with catalog install.
- [x] **Docs.** CHANGELOG 1.3.0, README feature bullets + Scripting/Localization
  sections, ROADMAP.

## Next — 1.4.0 candidates

Not yet started. Rough priority order:

1. **Decouple libGL/glut** (audit finding #6). Load GL dynamically so the app —
   splash, system and 2D benchmarks — launches on installs without Mesa; the
   GPU and Teapot tabs disable themselves gracefully when GL is absent. This is
   the biggest remaining coherence gap and unblocks "runs anywhere".
2. **Headless benchmark mode** (`--run-all --json out.json`) complementing
   `hey`, for CI and scripted runs with no window at all.
3. **Grow the Settings store**: persist last results and window geometry, and
   surface a couple more preferences now that the menu + store exist.

## Later (backlog)

- Rename `*Window` panel files to `*Panel` for naming coherence (audit #4).
- Additional catalog languages (de, fr, es) — the `it` pipeline is proven.
- Leaderboard submission from the export dialog.
- Refresh the README screenshot to show the 1.3.0 UI (menu bar + splash).
- Fill the recipe `CHECKSUM_SHA256` once the `v1.3.0` source tarball is
  published on GitHub.
