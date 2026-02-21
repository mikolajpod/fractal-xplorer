# Product Requirements Document: Fractal Xplorer

## 1. Overview

**Product name:** Fractal Xplorer
**Type:** Graphical desktop application
**Primary platform:** Windows (Linux secondary, no immediate timeline)
**License:** MIT
**Distribution:** Portable ZIP
**Target user:** Math enthusiasts and geeks who want a fast, no-nonsense fractal explorer with quality image export.

---

## 2. Problem Statement

Existing fractal tools are either too bloated, visually dated, or too slow. There is a gap for a lean, modern, high-performance fractal explorer that feels good for technically-minded users: fast rendering, precise controls, quality export, and a clean tool-like UI — without requiring installation or a massive runtime.

---

## 3. Goals & Non-Goals

### Goals (MVP)
- Render Mandelbrot, Julia, and Burning Ship fractals at interactive speeds
- AVX/SIMD + multithreaded rendering as baseline performance
- Export to PNG and JPEG XL at up to 8K resolution
- Intuitive navigation: zoom, pan, zoom-box
- Julia parameter selection via interactive mini Mandelbrot map
- 8 predefined color palettes with palette offset/cycling
- Smooth coloring (escape-time interpolation) as default color mapping
- Configurable iteration count (64–8192, default 256)
- Status overlay: center coordinates, zoom level, iteration count
- MIT licensed, portable ZIP distribution

### Non-Goals (MVP — deferred)
- OpenCL / GPU acceleration (architecture must allow future addition)
- Real-time navigation (nice-to-have; acceptable if render triggers on mouse release)
- Session saving / bookmarks
- Custom palette editor
- Arbitrary precision / deep zoom beyond double
- Mandelbrot with adjustable exponent (post-MVP)
- Linux packaging/testing (code should compile on Linux, but not a tested target yet)

---

## 4. Tech Stack

| Concern | Choice | Rationale |
|---|---|---|
| Language | C++17 | GCC 15.2.0 available; std::thread, AVX intrinsics, RAII |
| UI | Dear ImGui + SDL2 | Tiny footprint, excellent pixel-buffer texture rendering, tool-like aesthetic |
| Build | CMake + FetchContent | Familiar Make-based workflow; auto-fetches ImGui and other deps |
| Env | MSYS2 / MinGW on Windows | SDL2, libpng installable via pacman |
| PNG export | libpng | Standard, stable |
| JXL export | libjxl | Modern, lossless-capable, 16-bit support |
| SIMD | `<immintrin.h>` AVX2 | GCC 15.2.0 supports; optional detection at runtime |
| Threading | `std::thread` + thread pool | Tile-based parallel rendering |

**Future-proofing for OpenCL:** The compute kernel must be isolated behind a renderer interface (e.g., `IRenderer`) from day one. AVX and OpenCL backends are separate implementations of this interface. This costs nothing upfront and avoids a rewrite later.

---

## 5. Functional Requirements

### 5.1 Fractal Types (MVP)

| Fractal | Parameters |
|---|---|
| Mandelbrot | none (classic z² + c) |
| Julia | c ∈ ℂ (selectable via mini map or text input) |
| Burning Ship | none |

Post-MVP: Mandelbrot with configurable exponent (z^n + c, n adjustable).

### 5.2 Rendering

- Tile-based multithreaded rendering using `std::thread` pool
- AVX2 vectorized inner loop (compute 4 or 8 doubles per cycle)
- Runtime AVX detection with scalar fallback
- Double precision arithmetic (pixelation at extreme zoom is intentional and acceptable)
- Smooth coloring: fractional escape time via `log(log(|z|))` formula — produces gradient bands instead of hard iteration steps
- Re-render triggered on: fractal change, parameter change, navigation, iteration/palette change

### 5.3 Navigation

| Action | Input |
|---|---|
| Zoom in/out | Mouse wheel (centered on cursor) |
| Pan | Left-click drag |
| Zoom to region | Right-click drag (rubber band box) |
| Reset view | `R` key |
| Zoom +/- | `+` / `-` keys |

### 5.4 Julia Parameter Selection (MUST HAVE)

- Side panel always shows a small rendered Mandelbrot set
- User clicks or drags a point on the mini map to set c = (re, im)
- Julia view updates live as the point is dragged (if performance allows; fallback: update on release)
- c value also editable via two numeric text inputs (real, imaginary)

### 5.5 Color Palettes

- 8 predefined palettes shipped with v1:
  1. Grayscale
  2. Fire (black → red → yellow → white)
  3. Ice (black → blue → cyan → white)
  4. Electric (black → purple → blue → white)
  5. Sunset (deep red → orange → yellow)
  6. Forest (dark green → lime → white)
  7. Zebra (alternating black/white bands)
  8. Classic Ultra (blue-black-white gradient, inspired by classic fractal renders)
- Palette offset slider: shifts which color maps to iteration 0 (wraps around)
- No custom palette editor in MVP

### 5.6 Iteration Count

- Configurable via slider and/or numeric input
- Range: 64 – 8192
- Default: 256
- Changing value triggers immediate re-render

### 5.7 Image Export

- Formats: **PNG** (required), **JPEG XL** (optional/HQ)
- JPEG is strictly forbidden
- Resolution: up to 8K (7680×4320); export resolution independent of window size
- Export dialog: choose format, resolution multiplier (1x, 2x, 4x screen size, or custom)
- Exported filename includes fractal type, timestamp (e.g., `mandelbrot_20260220_143012.png`)

### 5.8 UI Layout (Single Window)

```
+----------------------------------------------------------+
| Menu: File | View | Help                                  |
+------------------+---------------------------------------+
|                  |                                       |
|  Side Panel      |   Main Fractal Render View            |
|                  |                                       |
|  [Fractal type]  |                                       |
|  [Iter count ]   |                                       |
|  [Palette    ]   |                                       |
|  [Pal offset ]   |                                       |
|                  |                                       |
|  Mini Mandelbrot |                                       |
|  map (Julia c)   |                                       |
|  [re:   ] [im: ] |                                       |
|                  |                                       |
+------------------+---------------------------------------+
| Status: x: -0.5000  y: 0.0000  zoom: 1.00x  iter: 256   |
+----------------------------------------------------------+
```

- Dark theme (ImGui default dark or custom)
- Side panel always visible (not collapsible in MVP)

### 5.9 Window Title

Format: `Fractal Xplorer — Mandelbrot [zoom: 3.14x]`
Update on navigation. Implemented only if the dynamic update is trivially cheap (it is — one `SDL_SetWindowTitle` call per render).

### 5.10 Startup Behavior

- Launch directly into fully rendered Mandelbrot at standard view:
  center `(-0.5, 0)`, range `[-2.5, 1.0]` × `[-1.25, 1.25]`
- No splash screen, no dialog, no setup wizard

### 5.11 Keyboard Shortcuts

| Key | Action |
|---|---|
| `R` | Reset to default view |
| `+` / `-` | Zoom in / out (step) |
| `Ctrl+S` | Export current view (PNG) |
| `F1` | Open About dialog |

### 5.12 About Dialog

- Single menu item: `Help → About`
- Shows: app name, version, author, MIT license notice, brief description
- One "Close" button

---

## 6. Non-Functional Requirements

| Requirement | Target |
|---|---|
| Render time (1080p Mandelbrot, 256 iter) | < 500ms on mid-range CPU |
| Render time (1080p, AVX2 path) | < 150ms target |
| Export time (4K PNG) | < 5s |
| Startup time | < 1s to first render |
| Binary size (portable ZIP) | < 20MB including all DLLs |
| Memory usage | < 200MB at 1080p |

---

## 7. Architecture Notes

### Compute Abstraction (future-proofing for OpenCL)

```
IFractalRenderer (interface)
  +-- CpuRenderer (AVX2 + std::thread, tile-based)
  +-- (future) OpenClRenderer
```

`CpuRenderer` owns a thread pool. Each tile is a `std::function` pushed to a work queue. The main thread waits for completion, then uploads pixel buffer to GPU texture via SDL2/ImGui.

### Data flow

```
User input -> ViewState (center, zoom, fractal type, params, iter, palette)
           -> IFractalRenderer::render(ViewState) -> PixelBuffer
           -> Upload to SDL_Texture -> ImGui::Image()
```

---

## 8. Milestones

| Milestone | Scope |
|---|---|
| M1: Skeleton | CMake build, SDL2+ImGui window, blank render area |
| M2: Mandelbrot | Basic scalar Mandelbrot render, navigation, reset |
| M3: Performance | AVX2 path + thread pool, benchmark vs scalar |
| M4: Julia | Mini map, live Julia updates, text inputs |
| M5: Burning Ship | Add third fractal type |
| M6: Palettes | 8 palettes, offset slider, smooth coloring |
| M7: Export | PNG export, JPEG XL export, resolution options |
| M8: Polish | Status bar, window title, keyboard shortcuts, About dialog |
| M9: Release | Portable ZIP, README, MIT license file |

---

## 9. Open Questions / Deferred Decisions

- **JPEG XL dependency**: libjxl build on MSYS2/MinGW needs verification. May be deferred to post-MVP if build friction is high.
- **Live Julia update performance**: If AVX render is fast enough at small mini-map resolution, live drag is trivial. Otherwise, update on mouse release.
- **Tile size**: 64x64 or 128x128 tiles for thread pool — tune empirically.
- **Linux**: Code should be portable (avoid Win32 API calls, use SDL2 abstractions), but Linux build is not a tested deliverable for MVP.

---

## 10. Success Criteria

- Mandelbrot renders at 1080p in under 500ms on a typical developer machine
- All three fractal types render correctly
- Julia c parameter correctly updates from mini map
- PNG export at 4K produces a correct, viewable image
- App launches, runs, and exits cleanly from a portable ZIP with no installation
- MIT LICENSE file present in repository root
