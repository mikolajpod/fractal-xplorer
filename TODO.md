# TODO

Backlog from the 2026-07-10 full project review. Items already fixed are not
listed (see CHANGELOG "Unreleased").

## 1. Async / progressive rendering (largest item)

All rendering is synchronous on the UI thread (`main.cpp` render on dirty,
`ui_panels.cpp` export render, benchmark render). A 4x export of a MultiSlow
view or any high-iteration render freezes the window ("Not Responding") with
no progress bar and no cancel. At deep zoom + high iterations every pan/zoom
step stalls the UI for the full render time.

Options, in increasing effort:
- progressive render: quick low-res pass first, then refine (cheap, biggest UX win)
- render on a worker thread with progress + cancel for exports
- fully async tile streaming into the texture

## 2. Gate global keyboard shortcuts (small)

`main.cpp:196-239`: `Ctrl+S`, `R`, `F1` fire even while typing in a numeric
field (typing `r` in the Re input resets the view mid-edit, and the field then
commits a stale value back). Arrows/`+`/`-`/`PageUp`/`P`/`B` fire behind open
modals — the view changes behind the export dialog, so the export silently
targets a different framing; `B` can stack a second modal.

Fix: wrap the whole shortcut block in one condition —
`!io.WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)`
— and move `Ctrl+S`/`R`/`F1` inside it (the inner `!io.WantTextInput` check
then disappears). Net code impact: about +3 lines (the diff looks bigger only
because of reindentation). Note: `io.WantCaptureKeyboard` is NOT usable here —
the whole app is ImGui windows, so it is effectively always true.

## 3. IFractalRenderer: remove it

The interface (`renderer.hpp`) has one method, one implementor, and nothing
holds an `IFractalRenderer*` — `AppState` holds a concrete `CpuRenderer` and
UI/benchmark code uses six concrete members (`set_avx`, `last_render_ms`,
`thread_count`, ...). An OpenCL prototype was built, showed no speedup, and
was discarded — so the "future OpenClRenderer slots in" justification is gone.
Delete the interface and drop the claim from PRD.md/CLAUDE.md ("Architecture
Constraints" section). Pure simplification, no behavior change.

## Minor

- **GlTex lifetime** (`app_state.hpp`, `main.cpp`): `AppState` is destroyed at
  end of `main()`, after `SDL_GL_DeleteContext` — `glDeleteTextures` runs with
  no current context (silent no-op on most drivers, but UB). Destroy textures
  before the context, and delete GlTex's copy ctor (accidental copy would
  double-free the texture id).
- **ThreadPool exception safety** (`thread_pool.hpp:57`): a task that throws
  escapes the worker thread → `std::terminate`. Currently latent (render_tile
  is pure math) but the pool API accepts arbitrary `std::function`s. Also
  `++pending` before `push_back` over-counts if `push_back` throws → `wait()`
  deadlock.
- **Benchmark dialog** (`ui_panels.cpp:637`): recreates the ThreadPool every
  frame while running (8 teardowns per thread-count step instead of 1); a
  window resize mid-run renders the main view with the benchmark's transient
  thread/AVX settings.
- **Minimap state leak** (`app_state.hpp:90`): Julia parameter map and Newton
  root map share one viewport (`mini_cx/cy/vw`) — zooming the Newton minimap
  leaves the Julia map in a strange place after a tab switch.
- **int overflow in size math** (`renderer.hpp:18`, `cpu_renderer.cpp`,
  `export.cpp`): `w * h` multiplies in int before widening — unreachable at
  current sizes, but the casts should be `static_cast<size_t>(w) * h`.
- **package.sh fragility**: hardcoded `/c/msys64` prefix; with `pipefail`, a
  statically-linked exe (grep matches nothing) aborts packaging with no
  diagnostic.
