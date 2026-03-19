# Fractal Xplorer — Project Instructions

## Project Overview

Fractal Xplorer is a graphical desktop fractal explorer targeting math enthusiasts.
See PRD.md for full requirements. v1.0 shipped all 9 milestones; ongoing development
adds new fractal types and UX improvements.

## Tech Stack

- **Language:** C++17, GCC 15.2.0 (MSYS2/MinGW on Windows)
- **UI:** Dear ImGui v1.91.6 + SDL2 + OpenGL 3.3 core
- **Build:** CMake + FetchContent (ImGui fetched automatically; no vcpkg/Conan)
- **PNG export:** libpng (`pacman -S mingw-w64-x86_64-libpng`)
- **JXL export:** libjxl 0.11.1 (`pacman -S mingw-w64-x86_64-libjxl`)
- **SLEEF:** SLEEF 3.9.0 (`pacman -S mingw-w64-x86_64-sleef`) — vectorized math for AVX kernels

## Hard Rules

- **JPEG is strictly forbidden** — no encoding, no dependency, not even optional
- **No Win32 API calls** — use SDL2 abstractions to keep Linux portability
- **No arbitrary precision** — double only; pixelation at deep zoom is intentional
- **No installer** — distribution is portable ZIP only (`bash package.sh`)

## Build

```bash
# MSYS2 must precede Git MinGW in PATH — permanent fix: reorder in Windows PATH
# Temporary fix each shell session:
export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"

cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="/c/msys64/mingw64"
cmake --build build -- -j$(nproc)

# Portable ZIP:
bash package.sh   # → fractal_xplorer-1.0-win64.zip
```

---

## Data Flow

```
User input
  → ViewState (mode, center_x/y, view_width, max_iter, formula, julia_mode, julia_re/im,
               palette, pal_offset, multibrot_exp, multibrot_exp_f, color_mode,
               newton_degree, newton_roots_re/im, newton_coeffs_re/im)
  → CpuRenderer::render(vs, PixelBuffer)          [tile pool + AVX or scalar kernel]
  → palette_color() / lyapunov_color()             [escape-time: 1024-entry LUT lookup]
  → newton_color() / palette_color()               [Newton: flat=root hue, smooth=palette bands]
  → PixelBuffer (uint32_t RGBA pixels)
  → glTexSubImage2D → GLuint texture
  → ImGui::Image()                                [displayed in ##render window]
```

Re-render is triggered by setting `dirty = true`. The mini map is re-rendered
whenever relevant parameters change; in Escape-Time mode it always renders in
Mandelbrot mode (`julia_mode=false`) of the current formula; in Newton mode it
renders the Newton fractal with current roots. The mini map has its own navigable
viewport (`mini_cx/cy/vw`) with right-drag to pan, scroll-wheel to zoom, and a
Reset button. In Escape-Time mode, left-drag picks *c*; in Newton mode, left-drag
near a root dot drags that root.

---

## Pixel Format Invariant

Every pixel throughout the pipeline is a `uint32_t` with layout:

```
bits 31-24: A = 0xFF (always fully opaque)
bits 23-16: B
bits 15-8 : G
bits  7-0 : R
```

On a little-endian machine, the four bytes in memory are **[R, G, B, A]**.
This matches OpenGL `GL_RGBA / GL_UNSIGNED_BYTE`, PNG `PNG_COLOR_TYPE_RGBA`,
and JXL `JxlPixelFormat {4, JXL_TYPE_UINT8}` — **no conversion is ever needed**.
Do not change this layout without updating all three output paths.

---

## Source Files

| File | Role |
|---|---|
| `view_state.hpp` | `FractalMode` + `FormulaType` + `ColorMode` enums, `ViewState` struct, `zoom_display()`, `fractal_name()`, `reset_view_keep_params()`, `newton_init_roots()`, `newton_expand_roots()` |
| `renderer.hpp` | `IFractalRenderer` interface, `PixelBuffer` |
| `escape_time.hpp` | Scalar iteration kernels — 3 templates (`scalar_kernel`, `scalar_multibrot_kernel`, `scalar_multibrot_slow_kernel`) + thin named wrappers; `scalar_lyapunov_iter`; `compute_orbit` |
| `newton.hpp` | `NewtonResult` (root + smooth), scalar Newton kernel: `horner_eval()`, `newton_iter()` |
| `escape_time_avx.hpp` | Declarations for AVX escape-time entry points |
| `escape_time_avx.cpp` | AVX+SLEEF escape-time kernels — compiled with `-O2 -mavx` |
| `newton_avx.hpp` | Declaration for `avx_newton_4()` |
| `newton_avx.cpp` | AVX Newton kernel with smooth output — compiled with `-O2 -mavx` (no SLEEF needed) |
| `cpu_renderer.hpp/.cpp` | Thread pool tile dispatch, AVX runtime detection, `set_thread_count()` |
| `thread_pool.hpp` | `std::thread` pool with condition-variable task queue |
| `palette.hpp` | LUT declaration, `palette_color()` + `lyapunov_color()` + `newton_color()` inlines, `NEWTON_ROOT_COLORS[8]` |
| `palette.cpp` | `init_palettes()` — 8 palettes built from color stops at startup |
| `export.hpp/.cpp` | `export_png()`, `export_jxl()` (guarded by `HAVE_JXL`) |
| `app_state.hpp` | `GlTex` GL texture helper + `AppState` struct (all mutable application state) |
| `ui_panels.hpp/.cpp` | Side panel (TabBar: Escape-Time / Newton), export/benchmark/about dialogs |
| `main.cpp` | App entry point: SDL/GL init, render loop, navigation input |
| `cli_benchmark.hpp` | `run_cli_benchmark()` — CLI perf test, invoked via `--benchmark` flag |

---

## Key Invariants

**AVX separate translation units** — `escape_time_avx.cpp` and `newton_avx.cpp`
are compiled with `-mavx` while everything else is not. This allows runtime
detection via `__builtin_cpu_supports("avx")` with a scalar fallback. Never
`#include` AVX intrinsic code from another file or move it into a header.

**`ImTextureID` cast** — must go through `uintptr_t`, not `intptr_t`:
```cpp
reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(gl_id))  // correct
```

**Smooth coloring formula** — scalar and AVX use:
```
smooth = iters + 1 - log(log(|z|) / log(n)) / log(n)
```
where `n` is the fractal exponent (2 for Standard/BurningShip/Mandelbar at n=2,
or `multibrot_exp` / `multibrot_exp_f` for higher degrees). For n=2 this reduces
to the classic `log2(log2(|z|))` formula. Interior points return `max_iter` exactly.

The AVX path accumulates `iters_d` by adding 1.0 per active lane per iteration
(not `set1_pd(i+1)`), so `iters_d == i` at escape, matching the scalar formula.

**AVX smooth coloring** uses SLEEF `Sleef_logd4_u35` (vectorized log) instead of
extracting to scalar, computing 4 logs, and reinserting. This applies to all three
kernel templates.

**Scalar kernel structure** — `escape_time.hpp` mirrors the AVX structure with three templates:
- `scalar_kernel<IsJulia, IsBurningShip, IsMandelbar, AbsRe, AbsIm>` — degree-2 formulas
- `scalar_multibrot_kernel<IsJulia, IsMandelbar>` — integer exponent ≥ 2
- `scalar_multibrot_slow_kernel<IsJulia>` — real exponent (polar form)

All 16 named `*_iter()` functions are one-liner wrappers around these templates.
`scalar_lyapunov_iter()` and `compute_orbit()` remain independent (use their own switch).

**AVX kernel structure** — `escape_time_avx.cpp` contains three templates:
- `avx_kernel<IsJulia, IsBurningShip, IsMandelbar, AbsRe, AbsIm, ComputeLyapunov>`
  — for degree-2 formulas; 12 public wrappers cover all (formula × julia_mode)
  combinations; uses FMA squaring for Standard/Mandelbar/BurningShip; Celtic uses
  `AbsRe=true`, Buffalo uses `AbsRe=true, AbsIm=true`
- `avx_multibrot_kernel<IsJulia, IsMandelbar, ComputeLyapunov>` — for integer
  exponent ≥ 2; uses repeated complex multiplication (no trig), smooth coloring
  with `log(exp_n)`; covers MultiFast and Mandelbar with n≥3
- `avx_multibrot_slow_kernel<IsJulia, ComputeLyapunov>` — for real exponent
  (MultiSlow); uses polar-form z^n via SLEEF vectorized log, exp, atan2, sincos

All three templates have a `ComputeLyapunov` bool (default `false`). When `true`,
they accumulate `log|f'(z)| = log(n) + (n-1)/2 * log(|z|²)` per iteration using
SLEEF log, and output `lambda = sum/count` alongside the smooth value. Existing
public wrappers are unchanged (`ComputeLyapunov=false`); Lyapunov instantiations
are called only through `avx_lyapunov_4()`.

`avx_lyapunov_4(formula, julia_mode, ...)` is a single dispatch function that
covers all 15 formula × julia_mode combinations, routing to `<..., true>`
template instantiations. It replicates the `slow_int_n` integer promotion logic
for MultiSlow. Collatz has a Lyapunov template instantiation but `render_tile()`
bypasses it — Collatz always uses smooth coloring (see ColorMode section).

`n=2` for Standard dispatches to `avx_mandelbrot_4` / `avx_julia_4`;
`n≥3` (integer) dispatches to `avx_multibrot_4` / `avx_multijulia_4`;
real exponent dispatches to `avx_multibrot_slow_4` / `avx_multijulia_slow_4`.

When `multibrot_exp_f` is an exact integer (detected by `slow_int_n` in `render_tile()`),
MultiSlow routes to the fast integer kernel instead of the polar-form kernel.

**Formula + Julia mode** — `ViewState` has two orthogonal dimensions:
- `FormulaType formula` — which iteration rule to apply (8 values)
- `bool julia_mode` — Mandelbrot mode (z₀=0, c=pixel) vs Julia mode (z₀=pixel, c=fixed)

This gives 15 combinations without any enum explosion. `julia_re`/`julia_im` hold
the fixed *c* parameter used when `julia_mode=true`. Collatz ignores `julia_mode`
(always z₀=pixel, no *c* parameter).

**Default viewport** — `ViewState{}` defaults to center (0, 0), view_width 4.0.
`default_view_for(ft)` returns this for all formula types — no per-formula special cases.

**Reset preserves user params** — `R` / View→Reset resets navigation
(center, zoom) to the universal default; formula, julia_mode, Julia params, palette,
pal_offset, multibrot exponents, color_mode, mode, and Newton root positions survive.

**FractalMode** — `ViewState::mode` (`EscapeTime` or `Newton`) is the top-level
discriminator. `render_tile()` checks mode first: Newton mode skips the entire
escape-time formula dispatch and uses `newton_iter()` / `avx_newton_4()` instead.

**Newton polynomial representation** — roots are stored as `newton_roots_re/im[8]`;
the expanded polynomial coefficients (`coeffs[0..n-1]` for z^0 through z^(n-1),
leading z^n = 1 implicit) are cached in `newton_coeffs_re/im[9]`. The
`newton_coeffs_dirty` flag triggers recomputation via `newton_expand_roots()`.
Always call `newton_expand_roots()` before rendering if dirty.

**Newton coloring** — two modes controlled by `color_mode`:
- `color_mode == 0` (Flat): `newton_color(root, iters, max_iter)` maps root index
  to one of 8 fixed hues (`NEWTON_ROOT_COLORS`) and dims by integer iteration count.
- `color_mode >= 1` (Smooth): uses band encoding — `smooth = root * band_width +
  min(smooth_iters, band_width - 1)` where `band_width = max_iter / degree` — then
  feeds through `palette_color()`, giving full palette + offset cycling support.
  Smooth iteration uses `frac = log(threshold) / log(step_mag2)`.
Non-converging pixels are black in both modes.

**Collatz fractal** — `z → (2 + 7z - (2+5z)·cos(πz)) / 4` with z₀ = pixel.
This is a smooth complex extension of the integer Collatz map (n/2 if even,
3n+1 if odd). No *c* parameter — `julia_mode` is ignored. Uses `cos(πz)` for
complex z, which involves cosh/sinh and causes exponential escape away from the
real axis. The fractal structure is a thin strip along Re(z); most of the complex
plane escapes in 1–2 iterations. Bailout is `|z|² > 10000` (vs 4 for other formulas).
Lyapunov coloring is disabled (forced to smooth) because the standard derivative
formula doesn't apply.

**Newton minimap** — renders the Newton fractal (not parameter space). Root
positions are drawn as colored dots. Left-drag near a dot drags that root;
right-drag pans; scroll zooms. The Reset button in Newton mode restores roots
to the unit circle *and* resets minimap navigation.

---

## FractalMode Enum

```cpp
enum class FractalMode { EscapeTime = 0, Newton = 1 };
```

`EscapeTime` uses the escape-time formula dispatch (8 formula types × Julia mode).
`Newton` uses the Newton-Raphson method on a user-defined polynomial (degree 2–8
with draggable roots).

---

## FormulaType Enum

```cpp
enum class FormulaType {
    Standard    = 0,  // z^2 + c  (always degree 2, no exponent slider)
    BurningShip = 1,  // (|Re z| + i|Im z|)^2 + c
    Celtic      = 2,  // |Re(z^2)| + i Im(z^2) + c
    Buffalo     = 3,  // |Re(z^2)| + i|Im(z^2)| + c
    Mandelbar   = 4,  // conj(z)^n + c  (integer exp 2-8)
    MultiFast   = 5,  // z^n + c  (integer exp 2-8, AVX)
    MultiSlow   = 6,  // z^n + c  (real exp, AVX polar form via SLEEF)
    Collatz     = 7,  // (2+7z-(2+5z)cos(pi*z))/4  (complex Collatz map)
};
constexpr int FORMULA_COUNT = 8;
```

Combined with `bool julia_mode` in `ViewState`, this gives 15 render combinations
(14 for formulas 0–6 × julia_mode, plus Collatz which ignores julia_mode).

`multibrot_exp` (int, 2–8) is the exponent for Mandelbar and MultiFast.
`multibrot_exp_f` (double) is the exponent for MultiSlow; any real value is accepted.
When `multibrot_exp_f` is an exact integer (e.g. 3.0), `render_tile()` detects this
(`slow_int_n`) and routes to the fast AVX repeated-multiply kernel instead.

---

## ColorMode Enum

```cpp
enum ColorMode {
    COLOR_SMOOTH            = 0,  // escape-time smooth coloring (default)
    COLOR_LYAPUNOV_INTERIOR = 1,  // interior by λ, exterior by escape-time
    COLOR_LYAPUNOV_FULL     = 2,  // all pixels by λ
};
constexpr int COLOR_MODE_COUNT = 3;
```

`ViewState::color_mode` (int, default 0) selects the coloring mode.
In Escape-Time mode: 0 = smooth, 1 = Lyapunov interior, 2 = Lyapunov full.
In Newton mode: 0 = flat (root hues), ≥1 = smooth (palette bands). The Newton
tab UI clamps the value to 0–1; Lyapunov values don't apply to Newton.
**Collatz exception:** Lyapunov modes are not applicable — the Lyapunov derivative
formula assumes z^n growth which doesn't match Collatz. `render_tile()` forces
smooth coloring for Collatz regardless of `color_mode`.

**Lyapunov exponent** λ = (1/N) Σ log|f'(z_k)|, where log|f'(z)| = log(n) +
(n-1)/2 · log(|z|²). Mapped to palette via `lyapunov_color()` in `palette.hpp`
with `LYAP_SCALE = 200.0` (one palette cycle per λ range of ~5.1).

In `render_tile()`, `COLOR_SMOOTH` takes the fast path (existing per-formula
dispatch); Lyapunov modes call `avx_lyapunov_4()` which returns both smooth
and lambda arrays. `COLOR_LYAPUNOV_INTERIOR` uses lambda only for interior
points (smooth ≥ max_iter), exterior keeps escape-time coloring.
`COLOR_LYAPUNOV_FULL` colors everything by lambda.

The scalar path (remainder pixels and full rows on non-AVX CPUs) uses
`scalar_lyapunov_iter()` in `escape_time.hpp` for Lyapunov modes — a single
generic function covering all formulas that returns `{smooth, lambda}`.

Mini-map always uses `COLOR_SMOOTH` (ViewState{} defaults `color_mode=0`).

---

## How to Add a New Fixed-Formula Fractal (degree 2)

7 places, 4 files — follow the Burning Ship + Julia pattern:

1. `view_state.hpp` — add enum value to `FormulaType`, update `fractal_name()`, bump `FORMULA_COUNT`
2. `escape_time.hpp` — add thin wrapper functions `foo_iter` / `foo_julia_iter` delegating
   to the appropriate scalar template (`scalar_kernel`, `scalar_multibrot_kernel`, or
   `scalar_multibrot_slow_kernel`) with the correct template parameters
3. `escape_time_avx.hpp` — declare `avx_foo_4()` and `avx_foo_julia_4()`
4. `escape_time_avx.cpp` — choose one of two approaches:
   - **Abs-after-squaring variant** (Celtic/Buffalo style): reuse existing `AbsRe`/`AbsIm`
     template params — just add wrappers with the right `<IsJulia,...,AbsRe,AbsIm>` values
   - **New z-update rule**: add `bool IsFoo` template parameter to `avx_kernel`,
     add `if constexpr (IsFoo)` branch in the z-update block, add two public wrappers:
     `avx_foo_4()` → `avx_kernel<false, ..., true>(...)` and
     `avx_foo_julia_4()` → `avx_kernel<true, ..., true>(...)`
5. `cpu_renderer.cpp` — add `case FormulaType::Foo:` to both the AVX switch and
   the scalar switch inside `render_tile()`, dispatch on `vs.julia_mode`
6. `escape_time_avx.cpp` — add `case FormulaType::Foo:` to `avx_lyapunov_4()`
   dispatch, calling the `<..., true>` Lyapunov template instantiation
7. `main.cpp` — add the name string to the `names[]` array in the formula Combo

---

## How to Add a New Palette

2 places, 1 file (`palette.cpp`):

1. Add the name to `g_palette_names[]`
2. Add a `build_lut(N, stops, n)` call inside `init_palettes()` with color stops

Also increment `PALETTE_COUNT` in `palette.hpp`.

---

## How to Add a New Export Format

1. Add a write function in `export.cpp` (guard with `#ifdef HAVE_FOO` if optional)
2. Declare it in `export.hpp`
3. Add the CMake `find_package` / `pkg_check_modules` + conditional
   `target_compile_definitions(fractal_xplorer PRIVATE HAVE_FOO)` to `CMakeLists.txt`
4. Add a radio button in the export modal in `main.cpp` and a branch in the Export
   button handler

---

## Architecture Constraints

- Fractal compute always behind `IFractalRenderer`; future `OpenClRenderer` must
  be addable without touching rendering logic
- AVX path must always have a scalar fallback (runtime `__builtin_cpu_supports`)
- `CpuRenderer` owns the `ThreadPool` — do not share it
- `set_thread_count(n)` destroys and recreates the pool; only call between renders

## Performance Benchmark

After any changes to iteration kernels or AVX code, run the CLI benchmark and
compare against `benchmark_baseline.txt`:

```bash
./build/fractal_xplorer.exe --benchmark
```

Pass `--no-avx` to force the scalar path at runtime (useful for testing on
AVX-capable machines without needing old pre-AVX hardware):

```bash
./build/fractal_xplorer.exe --no-avx
```

Single-threaded, 1920×1080, 256 iter, 20 test cases — all 8 escape-time formulas
plus Newton degree 3 and 5, on both AVX and scalar paths. Reports Mpix/s — higher
is better. Baseline is stored in `scalar_baseline.txt` (local, not committed).

---

## Known Gotchas

- **PATH conflict:** If `cc1.exe` loads DLLs from `C:/Program Files/Git/mingw64`
  instead of MSYS2, builds silently break. Fix: MSYS2 must precede Git in PATH.
- **Linker permission denied:** The exe is still running. Close it before rebuilding.
- **Mini map (Escape-Time):** Re-renders whenever `formula`, `multibrot_exp`,
  `multibrot_exp_f`, `mini_cx`, `mini_cy`, or `mini_vw` changes. Always renders
  in Mandelbrot mode (`julia_mode=false`) of the current formula — so Burning Ship
  Julia shows the Burning Ship parameter space, not Mandelbrot. Always max_iter 128,
  palette 7. Left-drag updates `julia_re`/`julia_im` only. Right-drag pans
  (`mini_cx/cy`). Scroll wheel zooms (`mini_vw`). Reset button restores
  `mini_cx=0, mini_cy=0, mini_vw=4`. Panel has `NoScrollbar` to prevent oscillation
  from `map_w` changing.
- **Mini map (Newton):** Re-renders whenever `newton_coeffs_dirty` or minimap view
  changes. Renders Newton fractal with current roots. Colored dots overlay root
  positions. Left-drag near a root dot (8px hit radius) drags it, updating
  `newton_roots_re/im` and setting `newton_coeffs_dirty`. Right-drag pans, scroll
  zooms. Reset button restores roots to unit circle and minimap to default view.
- **Orbit:** `compute_orbit()` in `escape_time.hpp` returns up to 20 z-trajectory
  points for any formula+julia_mode. Enabled by "Show orbit" checkbox; Ctrl+click
  in the render area picks the seed; drawn as dots (red seed, yellow rest) using
  `ImDrawList` inside `##render`. The orbit Ctrl+click suppresses the pan handler
  via `!io.KeyCtrl` guard.
- **Collatz bailout:** Uses `COLLATZ_BAILOUT2 = 10000` (bailout radius 100) instead
  of 4 for other formulas. Both `collatz_iter()`, `scalar_lyapunov_iter()`, and
  `compute_orbit()` use the larger bailout. The AVX kernel `avx_collatz_4()` has its
  own bailout constant. Collatz uses SLEEF `sincos` + `exp` (for cosh/sinh) in the
  AVX path — similar perf profile to MultiSlow.
- **Export filename race:** The filename shown in the dialog is regenerated each
  frame. `exp_saved_name` captures it at the moment Export is clicked — use that
  in the success message, not the live-generated string.
