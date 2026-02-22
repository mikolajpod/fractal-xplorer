# Fractal Xplorer

A fast, no-nonsense fractal explorer for Windows.
Renders Mandelbrot, Julia, Burning Ship, Mandelbar, and Multibrot/Multijulia fractals
using AVX2-vectorised, multithreaded arithmetic — no GPU required.

![Fractal Xplorer screenshot](window.png)

---

## Requirements

- Windows 10 or later (x86-64)
- OpenGL 3.3-capable GPU (any GPU from the last 15 years)
- No installation needed — extract the ZIP and run

---

## Running

Extract the ZIP anywhere and double-click **fractal_xplorer.exe**.

The app launches directly into a fully-rendered Mandelbrot set.
Exports are saved to the folder the exe is in.

---

## Navigation

| Action | Input |
|---|---|
| Zoom in / out | Mouse wheel |
| Pan | Left-click drag or arrow keys |
| Zoom to region | Right-click drag (rubber-band box) |
| Reset view | `R` key or **View → Reset View** |
| Zoom step in / out | `+` / `-` keys |

---

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `R` | Reset view to default (centre/zoom only) |
| `+` / `-` | Zoom step in / out |
| Arrow keys | Pan (10% of current view width per press) |
| `Page Up` / `Page Down` | Double / halve iteration count |
| `P` / `Shift+P` | Cycle palette forward / backward |
| `Ctrl+S` | Open export dialog |
| `B` | Open benchmark dialog |
| `F1` | About |

---

## Side Panel

**Formula** — select the iteration formula (mouse wheel also cycles):

| Name | Formula | Notes |
|---|---|---|
| Mandelbrot (z²+c) | z² + c | Classic; always degree 2 |
| Burning Ship (\|z\|²+c) | (\|Re z\|+i\|Im z\|)²+c | Absolute value of each component |
| Mandelbar (conj(z)^n+c) | conj(z)^n + c | Tricorn at n=2; exponent slider 2–8, (n+1)-fold symmetry |
| Multibrot (z^n+c) | z^n + c | Integer exponent 2–8, fast AVX2 path |
| Multibrot (z^r+c, slow) | z^r + c | Real exponent r, any value; scalar polar-form arithmetic |

**Julia mode** — checkbox below the formula selector. When enabled, each pixel is
used as the starting point z₀ and *c* is fixed (set via the mini map or re/im inputs).
Available for every formula — giving 10 total combinations.

**Exponent (integer)** — slider 2–8, shown for Mandelbar and Multibrot (z^n+c).
At n=2: standard degree-2 formula. At n≥3: fast AVX2 path using repeated complex
multiplication (no trig). Mandelbar at n≥3 gives (n+1)-fold rotational symmetry.

**Exponent (float)** — shown for Multibrot (z^r+c, slow).
Slider covers −10 to 10; the numeric input below accepts any real value.
Ctrl+click on the slider to type a value directly.
When the exponent is an exact integer (e.g. 3.0), the fast AVX2 path is used
automatically. Non-integer exponents use scalar polar-form arithmetic.

**Iterations** — logarithmic slider, 64 – 8192 (default 256).
Higher values reveal more detail at deep zoom at the cost of speed.

**Palette** — 8 predefined colour palettes (mouse wheel cycles):

| # | Name | Character |
|---|---|---|
| 0 | Grayscale | black → white |
| 1 | Fire | black → red → orange → yellow → white |
| 2 | Ice | black → blue → cyan → white |
| 3 | Electric | black → purple → blue → cyan → white |
| 4 | Sunset | black → deep-red → orange → pale-yellow |
| 5 | Forest | black → dark-green → lime → pale-green |
| 6 | Zebra | alternating black / white bands |
| 7 | Classic Ultra | blue-gold gradient (default) |

**Offset** slider — shifts the palette along the iteration axis (0 – 1023).

**Julia parameter** — click or drag on the mini map to set the complex
parameter *c*. The mini map shows the current formula in Mandelbrot mode, so
interesting Julia parameters are easy to spot visually (e.g. Burning Ship Julia
shows the Burning Ship set). Fine-tune with the **re** / **im** numeric inputs.
Clicking the mini map updates *c* only — it does not change formula or Julia mode.

**Threads** — select thread count (Auto uses all logical CPUs).
Change takes effect on the next render.

---

## Export

Open with `Ctrl+S` or **File → Export Image**.

- **Format:** PNG (lossless) or JPEG XL (lossless, typically 2–3× smaller)
- **Resolution:** 1× / 2× / 4× current window size, or custom up to 7680 × 4320
- Filename is auto-generated: `mandelbrot_20260221_143012.png`

---

## Benchmark

Open with `B` or **Tools → Benchmark**.

Renders 1920×1080 Mandelbrot (center −0.5, width 3.5, 256 iter) for each thread
count from 1 to the number of logical CPUs, averaging 4 runs per setting.
Results are shown as two bar charts (AVX2 in blue, Scalar in orange), both on
the same Mpix/s scale. Hover over a bar to see the exact value.

---

## Performance

Typical render times at 1080p, 256 iterations on a mid-range CPU:

| Path | Time |
|---|---|
| AVX2 + 16 threads | ~35–50 ms |
| Scalar + 16 threads | ~300–500 ms |

The status bar shows the last render time, active path, and thread count.

---

## Building from Source

**Prerequisites (MSYS2 / MinGW-w64):**

```bash
pacman -S mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-SDL2 \
          mingw-w64-x86_64-libpng \
          mingw-w64-x86_64-libjxl \
          git
```

**Build:**

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="/c/msys64/mingw64"
cmake --build build -- -j$(nproc)
```

**Package (produces `fractal_xplorer-1.5-win64.zip`):**

```bash
bash package.sh
```

---

## License

MIT — see [LICENSE](LICENSE).
