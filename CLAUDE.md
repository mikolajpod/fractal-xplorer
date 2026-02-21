# Fractal Xplorer — Project Instructions

## Project Overview

Fractal Xplorer is a graphical desktop fractal explorer targeting math enthusiasts.
See PRD.md for full requirements. All 9 milestones are complete (v1.0).

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
  → ViewState (center_x/y, view_width, max_iter, fractal, julia_re/im, palette, pal_offset)
  → CpuRenderer::render(vs, PixelBuffer)          [tile pool + AVX2 or scalar kernel]
  → palette_color(smooth, max_iter, palette, pal_offset)  [1024-entry LUT lookup]
  → PixelBuffer (uint32_t RGBA pixels)
  → glTexSubImage2D → GLuint texture
  → ImGui::Image()                                [displayed in ##render window]
```

Re-render is triggered by setting `dirty = true`. The mini Mandelbrot map is
rendered once at startup into its own `PixelBuffer` + `GlTex` and never again.

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
| `view_state.hpp` | `ViewState` struct + `zoom_display()` + `fractal_name()` |
| `renderer.hpp` | `IFractalRenderer` interface, `PixelBuffer` |
| `fractal.hpp` | Scalar iteration kernels (Mandelbrot, Julia, Burning Ship) |
| `fractal_avx.hpp` | Declarations for AVX2 entry points |
| `cpu_renderer_avx.cpp` | AVX2+FMA kernel — compiled with `-O2 -mavx2 -mfma` |
| `cpu_renderer.hpp/.cpp` | Thread pool tile dispatch, AVX2 runtime detection |
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

**Smooth coloring formula** — both scalar and AVX2 paths use:
```
smooth = iters + 1 - log2(log2(sqrt(|z|^2)))
       = iters + 1 - log(log(|z|^2) * 0.5) / log(2)
```
Interior points return `max_iter` exactly. The AVX2 path accumulates
`iters_d` by adding 1.0 per active lane per iteration (not `set1_pd(i+1)`),
so `iters_d == i` at escape, matching the scalar formula.

**Reset preserves palette** — `R` / View→Reset resets navigation
(center, zoom) only; fractal type, Julia params, palette, and pal_offset survive.

---

## How to Add a New Fractal Type

6 places, 4 files:

1. `view_state.hpp` — add enum value to `FractalType`
2. `fractal.hpp` — add scalar `foo_iter(re, im, max_iter)` inline function
3. `fractal_avx.hpp` — declare `avx2_foo_4(re0, scale, im, max_iter, out4)`
4. `cpu_renderer_avx.cpp` — add `template<bool IsJulia, bool IsBurningShip, bool IsFoo>`
   branch (or a new bool template parameter) inside `avx2_kernel`, add public wrapper
5. `cpu_renderer.cpp` — add `case FractalType::Foo:` to both the AVX2 switch and
   the scalar switch inside `render_tile()`
6. `main.cpp` — add the name string to the `names[]` array in the fractal Combo

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

## Known Gotchas

- **PATH conflict:** If `cc1.exe` loads DLLs from `C:/Program Files/Git/mingw64`
  instead of MSYS2, builds silently break. Fix: MSYS2 must precede Git in PATH.
- **Linker permission denied:** The exe is still running. Close it before rebuilding.
- **Mini map palette:** The mini map `ViewState` uses defaults — palette 7
  (Classic Ultra), max_iter 128. It ignores the user's current palette, which is
  intentional (it's a fixed reference, not a preview).
- **Export filename race:** The filename shown in the dialog is regenerated each
  frame. `exp_saved_name` captures it at the moment Export is clicked — use that
  in the success message, not the live-generated string.
