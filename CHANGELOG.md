# Changelog

## v2.0 — 2026-07-10

- **Perpendicular fractal family** — four new formulas: Perpendicular
  Mandelbrot, Perpendicular Burning Ship, Perpendicular Celtic, and
  Perpendicular Buffalo (abs applied to a single factor of the imaginary
  product); full Julia-mode, Lyapunov, orbit, and AVX support
- **Lambda fractal** — complex logistic map z → λ·z·(1−z); pixel is λ,
  iteration starts at the critical point z₀ = ½; Julia mode + AVX support;
  Lyapunov coloring uses the exact logistic derivative λ(1−2z)
- **Collatz fractal** — smooth complex extension of the Collatz map,
  z → (2+7z−(2+5z)·cos(πz))/4; no *c* parameter, bailout radius 100
- **Re/Im/Width navigation inputs** — type exact view coordinates in the side panel
- **`--selftest` CLI mode** — renders every formula/mode on both the AVX and
  scalar paths and reports per-pixel divergence; regression gate for kernel changes
- **Bit-exact scalar/AVX parity** — AVX lane coordinates and FP associations now
  match the scalar path exactly; all non-SLEEF formulas render identically on
  both paths (previously up to 1.4% of pixels differed on Burning Ship)
- **Fix:** MultiSlow with a negative exponent rendered solid black on the AVX path
- **Fix:** Newton pixels at degenerate derivatives (p'(z)≈0) colored differently
  on AVX vs scalar
- **Fix:** benchmark dialog crashed with an illegal instruction on pre-AVX CPUs
- **Fix:** Collatz orbit visualization was stuck at the origin
- **Fix:** smooth coloring produced NaN/garbage for exponents ≤ 1; late-escaping
  Multibrot (n≥3) pixels could be misclassified as interior
- **Version single-sourced** from CMakeLists.txt (About dialog previously showed
  a stale version)

## v1.9 — 2026-03-05

- **Newton fractal mode** — Newton-Raphson basins for polynomials of degree 2–8
  with draggable roots on the minimap; AVX kernel + scalar fallback
- **Newton smooth coloring** — optional palette-band mode alongside the default
  flat root-hue coloring
- File renames: `fractal.hpp` → `escape_time.hpp`, `cpu_renderer_avx.*` →
  `escape_time_avx.*`

## v1.8 — 2026-03-02

- **AVX2 → AVX** — dropped FMA intrinsics and compile with `-mavx` only, so the
  fast path now runs on any AVX-capable CPU (Sandy Bridge 2011+)
- `--no-avx` flag (replaces `--no-avx2`); fixed Lyapunov coloring on non-AVX CPUs
- Scalar kernels refactored into 3 templates mirroring the AVX structure

## v1.7 — 2026-02-24

- **Lyapunov exponent coloring** — two new color modes: interior-only and full
- **SLEEF integration** — vectorized smooth coloring; AVX kernel for
  real-exponent Multibrot (polar form via SLEEF log/exp/atan2/sincos)
- Fixed status bar showing AVX for scalar MultiSlow renders
- Internal: side panel and dialogs extracted from main.cpp into ui_panels/app_state

## v1.6 — 2026-02-23

- **Orbit visualization** — enable "Show orbit" checkbox, then Ctrl+click any
  point in the main render area to trace up to 20 iteration steps; seed shown
  in red, subsequent points in yellow
- **Zoomable/pannable mini map** — the Julia parameter map now supports
  right-drag to pan, scroll-wheel to zoom, and a Reset button
- **Threads menu** — new menu bar entry to select thread count (Auto or 1…N)
- **CLI benchmark** (`--benchmark` flag) — single-threaded Mpix/s table covering
  all AVX2 and scalar render paths; useful for regression detection after kernel
  changes. On Windows redirect stdout: `fractal_xplorer.exe --benchmark > out.txt`
- **Fix idle CPU usage** — replaced `SDL_PollEvent` busy-loop with
  `SDL_WaitEventTimeout`, dropping CPU usage to ~0% when idle

## v1.5

- Celtic and Buffalo fractal formulas
- Mandelbar, Multibrot, Multijulia formulas (integer and real exponents)
- JXL (JPEG XL) lossless export
- Smooth coloring with AVX2 FMA kernels

## v1.0

- Initial release: Mandelbrot, Julia, Burning Ship
- AVX2-vectorised multithreaded renderer
- PNG export, palette system, interactive mini map
