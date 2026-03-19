#pragma once

#include "cpu_renderer.hpp"
#include "palette.hpp"
#include "view_state.hpp"
#include <cstdio>
#include <algorithm>
#include <vector>

inline int run_cli_benchmark()
{
    init_palettes();

    CpuRenderer renderer;
    renderer.set_thread_count(1);

    constexpr int W = 1920, H = 1080, RUNS = 4, BEST_N = 2;
    PixelBuffer buf;
    buf.resize(W, H);

    struct TestCase {
        const char* label;
        FormulaType formula;
        bool        julia_mode;
        int         exp_i;
        double      exp_f;
        bool        force_scalar;
        FractalMode mode;
        int         newton_deg;
    };

    const TestCase tests[] = {
        // AVX path
        {"Mandelbrot",              FormulaType::Standard,    false, 2, 2.0, false, FractalMode::EscapeTime, 0},
        {"Julia",                   FormulaType::Standard,    true,  2, 2.0, false, FractalMode::EscapeTime, 0},
        {"Burning Ship",            FormulaType::BurningShip, false, 2, 2.0, false, FractalMode::EscapeTime, 0},
        {"Celtic",                  FormulaType::Celtic,      false, 2, 2.0, false, FractalMode::EscapeTime, 0},
        {"Buffalo",                 FormulaType::Buffalo,     false, 2, 2.0, false, FractalMode::EscapeTime, 0},
        {"Mandelbar (n=2)",         FormulaType::Mandelbar,   false, 2, 2.0, false, FractalMode::EscapeTime, 0},
        {"Multibrot (n=3)",         FormulaType::MultiFast,   false, 3, 3.0, false, FractalMode::EscapeTime, 0},
        {"Multibrot (r=3.5, slow)", FormulaType::MultiSlow,   false, 2, 3.5, false, FractalMode::EscapeTime, 0},
        {"Collatz",                 FormulaType::Collatz,     false, 2, 2.0, false, FractalMode::EscapeTime, 0},
        {"Newton (deg 3)",          FormulaType::Standard,    false, 2, 2.0, false, FractalMode::Newton, 3},
        {"Newton (deg 5)",          FormulaType::Standard,    false, 2, 2.0, false, FractalMode::Newton, 5},
        // Scalar path
        {"Mandelbrot",              FormulaType::Standard,    false, 2, 2.0, true,  FractalMode::EscapeTime, 0},
        {"Julia",                   FormulaType::Standard,    true,  2, 2.0, true,  FractalMode::EscapeTime, 0},
        {"Burning Ship",            FormulaType::BurningShip, false, 2, 2.0, true,  FractalMode::EscapeTime, 0},
        {"Celtic",                  FormulaType::Celtic,      false, 2, 2.0, true,  FractalMode::EscapeTime, 0},
        {"Buffalo",                 FormulaType::Buffalo,     false, 2, 2.0, true,  FractalMode::EscapeTime, 0},
        {"Mandelbar (n=2)",         FormulaType::Mandelbar,   false, 2, 2.0, true,  FractalMode::EscapeTime, 0},
        {"Multibrot (n=3)",         FormulaType::MultiFast,   false, 3, 3.0, true,  FractalMode::EscapeTime, 0},
        {"Multibrot (r=3.5, slow)", FormulaType::MultiSlow,   false, 2, 3.5, true,  FractalMode::EscapeTime, 0},
        {"Collatz",                 FormulaType::Collatz,     false, 2, 2.0, true,  FractalMode::EscapeTime, 0},
        {"Newton (deg 3)",          FormulaType::Standard,    false, 2, 2.0, true,  FractalMode::Newton, 3},
        {"Newton (deg 5)",          FormulaType::Standard,    false, 2, 2.0, true,  FractalMode::Newton, 5},
    };

    printf("Fractal Xplorer CLI Benchmark\n");
    printf("%dx%d, 256 iter, 1 thread, %d runs (avg best %d)\n", W, H, RUNS, BEST_N);
    printf("AVX supported: %s\n\n", renderer.avx_active ? "yes" : "no");
    printf("%-30s %-10s %s\n", "Label", "Path", "Mpix/s");
    printf("------------------------------------------------\n");

    const bool has_avx = renderer.avx_active;

    for (const auto& t : tests) {
        ViewState vs;
        vs.center_x        = -0.5;
        vs.center_y        =  0.0;
        vs.view_width      =  3.5;
        vs.max_iter        =  256;
        vs.formula         =  t.formula;
        vs.julia_mode      =  t.julia_mode;
        vs.julia_re        = -0.7;
        vs.julia_im        =  0.27015;
        vs.multibrot_exp   =  t.exp_i;
        vs.multibrot_exp_f =  t.exp_f;
        vs.mode            =  t.mode;
        if (t.mode == FractalMode::Newton) {
            vs.newton_degree = t.newton_deg;
            vs.center_x = 0.0;
            vs.view_width = 4.0;
            newton_init_roots(vs);
            newton_expand_roots(vs);
        }

        if (t.force_scalar)
            renderer.set_avx(false);
        else
            renderer.set_avx(has_avx);

        // Warm-up
        renderer.render(vs, buf);

        std::vector<double> times(RUNS);
        for (int r = 0; r < RUNS; ++r) {
            renderer.render(vs, buf);
            times[r] = renderer.last_render_ms;
        }
        std::sort(times.begin(), times.end());
        double avg_ms = 0.0;
        for (int i = 0; i < BEST_N; ++i) avg_ms += times[i];
        avg_ms /= BEST_N;
        double mpixs = (W * H) / (avg_ms * 1000.0);

        const char* path_label = "scalar";
        if (!t.force_scalar && has_avx)
            path_label = "AVX";

        printf("%-30s %-10s %6.2f\n", t.label, path_label, mpixs);
    }

    renderer.set_avx(has_avx);  // restore
    return 0;
}
