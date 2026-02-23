# Changelog

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
