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
  → ViewState (center_x/y, view_width, max_iter, formula, julia_mode, julia_re/im,
               palette, pal_offset, multibrot_exp, multibrot_exp_f)
  → CpuRenderer::render(vs, PixelBuffer)          [tile pool + AVX2 or scalar kernel]
  → palette_color(smooth, max_iter, palette, pal_offset)  [1024-entry LUT lookup]
  → PixelBuffer (uint32_t RGBA pixels)
  → glTexSubImage2D → GLuint texture
  → ImGui::Image()                                [displayed in ##render window]
```

Re-render is triggered by setting `dirty = true`. The mini map is re-rendered
whenever `formula`, `multibrot_exp`, or `multibrot_exp_f` changes; it always renders
in Mandelbrot mode (`julia_mode=false`) of the current formula, showing the
parameter space for *c* at a fixed reference view.

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
| `view_state.hpp` | `FormulaType` enum + `ViewState` struct + `zoom_display()` + `fractal_name()` + `reset_view_keep_params()` |
| `renderer.hpp` | `IFractalRenderer` interface, `PixelBuffer` |
| `fractal.hpp` | Scalar iteration kernels (Mandelbrot, Julia, Burning Ship, Mandelbar, Multibrot, Multijulia, Celtic, Buffalo) |
| `fractal_avx.hpp` | Declarations for AVX2 entry points |
| `cpu_renderer_avx.cpp` | AVX2+FMA kernels — compiled with `-O2 -mavx2 -mfma` |
| `cpu_renderer.hpp/.cpp` | Thread pool tile dispatch, AVX2 runtime detection, `set_thread_count()` |
| `thread_pool.hpp` | `std::thread` pool with condition-variable task queue |
| `palette.hpp` | LUT declaration, `palette_color()` inline, constants |
| `palette.cpp` | `init_palettes()` — 8 palettes built from color stops at startup |
| `export.hpp/.cpp` | `export_png()`, `export_jxl()` (guarded by `HAVE_JXL`) |
| `main.cpp` | App entry point: render loop, ImGui UI, navigation, dialogs |

---

## Key Invariants

**AVX2 separate translation unit** — `cpu_renderer_avx.cpp` is compiled with
`-mavx2 -mfma` while everything else is not. This allows runtime detection via
`__builtin_cpu_supports("avx2")` with a scalar fallback. Never `#include` AVX2
intrinsic code from another file or move it into a header.

**`ImTextureID` cast** — must go through `uintptr_t`, not `intptr_t`:
```cpp
reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(gl_id))  // correct
```

**Smooth coloring formula** — scalar and AVX2 use:
```
smooth = iters + 1 - log(log(|z|) / log(n)) / log(n)
```
where `n` is the fractal exponent (2 for Standard/BurningShip/Mandelbar at n=2,
or `multibrot_exp` / `multibrot_exp_f` for higher degrees). For n=2 this reduces
to the classic `log2(log2(|z|))` formula. Interior points return `max_iter` exactly.

The AVX2 path accumulates `iters_d` by adding 1.0 per active lane per iteration
(not `set1_pd(i+1)`), so `iters_d == i` at escape, matching the scalar formula.

**AVX2 kernel structure** — `cpu_renderer_avx.cpp` contains two templates:
- `avx2_kernel<IsJulia, IsBurningShip, IsMandelbar, AbsRe, AbsIm>` — for degree-2
  formulas; 12 public wrappers cover all (formula × julia_mode) combinations;
  uses FMA squaring for Standard/Mandelbar/BurningShip; Celtic uses `AbsRe=true`,
  Buffalo uses `AbsRe=true, AbsIm=true` (abs applied to components of z² after squaring)
- `avx2_multibrot_kernel<IsJulia, IsMandelbar>` — for integer exponent ≥ 2; uses
  repeated complex multiplication (no trig), smooth coloring with `log(exp_n)`;
  covers MultiFast and Mandelbar with n≥3, both in Mandelbrot and Julia mode

`n=2` for Standard dispatches to `avx2_mandelbrot_4` / `avx2_julia_4`;
`n≥3` dispatches to `avx2_multibrot_4` / `avx2_multijulia_4`.

**Formula + Julia mode** — `ViewState` has two orthogonal dimensions:
- `FormulaType formula` — which iteration rule to apply (7 values)
- `bool julia_mode` — Mandelbrot mode (z₀=0, c=pixel) vs Julia mode (z₀=pixel, c=fixed)

This gives 14 combinations without any enum explosion. `julia_re`/`julia_im` hold
the fixed *c* parameter used when `julia_mode=true`.

**Default viewport** — `ViewState{}` defaults to center (0, 0), view_width 4.0.
`default_view_for(ft)` returns this for all formula types — no per-formula special cases.

**Reset preserves user params** — `R` / View→Reset resets navigation
(center, zoom) to the universal default; formula, julia_mode, Julia params, palette,
pal_offset, and multibrot exponents survive.

---

## FormulaType Enum

```cpp
enum class FormulaType {
    Standard    = 0,  // z^2 + c  (always degree 2, no exponent slider)
    BurningShip = 1,  // (|Re z| + i|Im z|)^2 + c
    Mandelbar   = 2,  // conj(z)^n + c  (integer exp 2-8)
    MultiFast   = 3,  // z^n + c  (integer exp 2-8, AVX2)
    MultiSlow   = 4,  // z^n + c  (real exp, scalar)
    Celtic      = 5,  // |Re(z^2)| + i Im(z^2) + c
    Buffalo     = 6,  // |Re(z^2)| + i|Im(z^2)| + c
};
constexpr int FORMULA_COUNT = 7;
```

Combined with `bool julia_mode` in `ViewState`, this gives 14 render combinations.

`multibrot_exp` (int, 2–8) is the exponent for Mandelbar and MultiFast.
`multibrot_exp_f` (double) is the exponent for MultiSlow; any real value is accepted.
When `multibrot_exp_f` is an exact integer (e.g. 3.0), `render_tile()` detects this
(`slow_int_n`) and routes to the fast AVX2 repeated-multiply kernel instead.

---

## How to Add a New Fixed-Formula Fractal (degree 2)

6 places, 4 files — follow the Burning Ship + Julia pattern:

1. `view_state.hpp` — add enum value to `FormulaType`, update `fractal_name()`, bump `FORMULA_COUNT`
2. `fractal.hpp` — add scalar `foo_iter(re, im, max_iter)` and
   `foo_julia_iter(re, im, cr, ci, max_iter)` inline functions
3. `fractal_avx.hpp` — declare `avx2_foo_4()` and `avx2_foo_julia_4()`
4. `cpu_renderer_avx.cpp` — choose one of two approaches:
   - **Abs-after-squaring variant** (Celtic/Buffalo style): reuse existing `AbsRe`/`AbsIm`
     template params — just add wrappers with the right `<IsJulia,...,AbsRe,AbsIm>` values
   - **New z-update rule**: add `bool IsFoo` template parameter to `avx2_kernel`,
     add `if constexpr (IsFoo)` branch in the z-update block, add two public wrappers:
     `avx2_foo_4()` → `avx2_kernel<false, ..., true>(...)` and
     `avx2_foo_julia_4()` → `avx2_kernel<true, ..., true>(...)`
5. `cpu_renderer.cpp` — add `case FormulaType::Foo:` to both the AVX2 switch and
   the scalar switch inside `render_tile()`, dispatch on `vs.julia_mode`
6. `main.cpp` — add the name string to the `names[]` array in the formula Combo

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
- AVX2 path must always have a scalar fallback (runtime `__builtin_cpu_supports`)
- `CpuRenderer` owns the `ThreadPool` — do not share it
- `set_thread_count(n)` destroys and recreates the pool; only call between renders

## Known Gotchas

- **PATH conflict:** If `cc1.exe` loads DLLs from `C:/Program Files/Git/mingw64`
  instead of MSYS2, builds silently break. Fix: MSYS2 must precede Git in PATH.
- **Linker permission denied:** The exe is still running. Close it before rebuilding.
- **Mini map:** Re-renders whenever `formula`, `multibrot_exp`, or `multibrot_exp_f`
  changes. Always renders in Mandelbrot mode (`julia_mode=false`) of the current
  formula — so Burning Ship Julia shows the Burning Ship parameter space, not
  Mandelbrot. Always max_iter 128, palette 7. Clicking/dragging updates
  `julia_re`/`julia_im` only — does not change formula or julia_mode.
- **Export filename race:** The filename shown in the dialog is regenerated each
  frame. `exp_saved_name` captures it at the moment Export is clicked — use that
  in the success message, not the live-generated string.
