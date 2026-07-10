#pragma once

#include "cpu_renderer.hpp"
#include "palette.hpp"
#include "view_state.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

// Renders every formula/mode combination on both the AVX and scalar paths
// and reports per-pixel divergence. W is a multiple of 4 (and of the 64-px
// tile width) so the AVX render has no scalar remainder columns — each
// buffer is produced by exactly one code path.
inline int run_cli_selftest()
{
    init_palettes();

    CpuRenderer renderer;
    if (!renderer.avx_active) {
        printf("AVX not supported on this CPU — nothing to compare.\n");
        return 0;
    }

    constexpr int W = 320, H = 240;
    PixelBuffer buf_avx, buf_sca;
    buf_avx.resize(W, H);
    buf_sca.resize(W, H);

    struct TestCase {
        const char* label;
        FormulaType formula;
        bool        julia_mode;
        int         exp_i;
        double      exp_f;
        int         color_mode;
        FractalMode mode;
        int         newton_deg;
    };

    const TestCase tests[] = {
        // Escape-time, smooth coloring
        {"Mandelbrot",                 FormulaType::Standard,    false, 2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"Julia",                      FormulaType::Standard,    true,  2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"Burning Ship",               FormulaType::BurningShip, false, 2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"Burning Ship Julia",         FormulaType::BurningShip, true,  2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"Celtic",                     FormulaType::Celtic,      false, 2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"Celtic Julia",               FormulaType::Celtic,      true,  2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"Buffalo",                    FormulaType::Buffalo,     false, 2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"Buffalo Julia",              FormulaType::Buffalo,     true,  2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"Mandelbar (n=2)",            FormulaType::Mandelbar,   false, 2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"Mandelbar Julia (n=2)",      FormulaType::Mandelbar,   true,  2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"Mandelbar (n=3)",            FormulaType::Mandelbar,   false, 3,  3.0, 0, FractalMode::EscapeTime, 0},
        {"Multibrot (n=3)",            FormulaType::MultiFast,   false, 3,  3.0, 0, FractalMode::EscapeTime, 0},
        {"Multijulia (n=3)",           FormulaType::MultiFast,   true,  3,  3.0, 0, FractalMode::EscapeTime, 0},
        {"MultiSlow (r=3.5)",          FormulaType::MultiSlow,   false, 2,  3.5, 0, FractalMode::EscapeTime, 0},
        {"MultiSlow Julia (r=3.5)",    FormulaType::MultiSlow,   true,  2,  3.5, 0, FractalMode::EscapeTime, 0},
        {"MultiSlow (r=2.0, int)",     FormulaType::MultiSlow,   false, 2,  2.0, 0, FractalMode::EscapeTime, 0},
        {"MultiSlow (r=-3.0)",         FormulaType::MultiSlow,   false, 2, -3.0, 0, FractalMode::EscapeTime, 0},
        {"MultiSlow (r=0.5)",          FormulaType::MultiSlow,   false, 2,  0.5, 0, FractalMode::EscapeTime, 0},
        {"Collatz",                    FormulaType::Collatz,     false, 2,  2.0, 0, FractalMode::EscapeTime, 0},
        // Lyapunov coloring
        {"Mandelbrot lyap-interior",   FormulaType::Standard,    false, 2,  2.0, 1, FractalMode::EscapeTime, 0},
        {"Mandelbrot lyap-full",       FormulaType::Standard,    false, 2,  2.0, 2, FractalMode::EscapeTime, 0},
        {"Multibrot (n=3) lyap-full",  FormulaType::MultiFast,   false, 3,  3.0, 2, FractalMode::EscapeTime, 0},
        {"MultiSlow (r=3.5) lyap-full",FormulaType::MultiSlow,   false, 2,  3.5, 2, FractalMode::EscapeTime, 0},
        // Newton
        {"Newton deg3 flat",           FormulaType::Standard,    false, 2,  2.0, 0, FractalMode::Newton, 3},
        {"Newton deg3 smooth",         FormulaType::Standard,    false, 2,  2.0, 1, FractalMode::Newton, 3},
        {"Newton deg5 flat",           FormulaType::Standard,    false, 2,  2.0, 0, FractalMode::Newton, 5},
        {"Newton deg5 smooth",         FormulaType::Standard,    false, 2,  2.0, 1, FractalMode::Newton, 5},
    };

    printf("Fractal Xplorer scalar/AVX self-test\n");
    printf("%dx%d, 256 iter — identical view rendered on both paths, pixels compared\n\n", W, H);
    printf("%-28s %9s %8s %7s   %s\n", "Label", "diff px", "diff %", "maxDch", "status");
    printf("----------------------------------------------------------------------\n");

    int failures = 0;
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
        vs.color_mode      =  t.color_mode;
        vs.mode            =  t.mode;
        if (t.mode == FractalMode::Newton) {
            vs.newton_degree = t.newton_deg;
            vs.center_x = 0.0;
            vs.view_width = 4.0;
            newton_init_roots(vs);
            newton_expand_roots(vs);
        }

        renderer.set_avx(true);
        renderer.render(vs, buf_avx);
        renderer.set_avx(false);
        renderer.render(vs, buf_sca);

        long ndiff = 0;
        int  max_dch = 0;
        int  worst_x = -1, worst_y = -1;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const uint32_t a = buf_avx.pixels[static_cast<size_t>(y) * W + x];
                const uint32_t s = buf_sca.pixels[static_cast<size_t>(y) * W + x];
                if (a == s) continue;
                ++ndiff;
                for (int c = 0; c < 24; c += 8) {
                    const int d = std::abs(static_cast<int>((a >> c) & 0xFF) -
                                           static_cast<int>((s >> c) & 0xFF));
                    if (d > max_dch) { max_dch = d; worst_x = x; worst_y = y; }
                }
            }
        }

        const double pct = 100.0 * ndiff / (static_cast<double>(W) * H);
        // Non-SLEEF formulas are expected bit-identical (0.000%); only the
        // SLEEF-transcendental kernels (MultiSlow polar form, Collatz) may
        // legitimately differ by a few pixels (< 0.02%).
        const char* status = "OK";
        if (pct >= 5.0)        { status = "FAIL"; ++failures; }
        else if (pct >= 0.05)    status = "WARN";

        printf("%-28s %9ld %7.3f%% %7d   %s", t.label, ndiff, pct, max_dch, status);
        if (ndiff > 0 && worst_x >= 0)
            printf("  (worst at %d,%d)", worst_x, worst_y);
        printf("\n");
    }

    printf("----------------------------------------------------------------------\n");
    printf("%d case(s) FAILed (>=5%% pixels differ)\n", failures);
    return failures > 0 ? 1 : 0;
}
