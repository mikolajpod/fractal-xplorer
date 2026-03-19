#pragma once

#include <cmath>
#include <cstdio>

// Top-level fractal family discriminator
enum class FractalMode { EscapeTime = 0, Newton = 1 };

enum class FormulaType {
    Standard    = 0,  // z^2 + c  (always degree 2, no exponent slider)
    BurningShip = 1,  // (|Re z| + i|Im z|)^2 + c
    Celtic      = 2,  // |Re(z^2)| + i Im(z^2) + c
    Buffalo     = 3,  // |Re(z^2)| + i|Im(z^2)| + c
    Mandelbar   = 4,  // conj(z)^n + c  (integer exp 2-8)
    MultiFast   = 5,  // z^n + c  (integer exp 2-8, AVX)
    MultiSlow   = 6,  // z^n + c  (real exp, scalar)
    Collatz     = 7,  // (2+7z-(2+5z)cos(pi*z))/4  (complex Collatz map)
};
constexpr int FORMULA_COUNT = 8;

enum ColorMode {
    COLOR_SMOOTH            = 0,
    COLOR_LYAPUNOV_INTERIOR = 1,
    COLOR_LYAPUNOV_FULL     = 2,
};
constexpr int COLOR_MODE_COUNT = 3;

struct ViewState {
    double      center_x        =  0.0;
    double      center_y        =  0.0;
    double      view_width      =  4.0;  // width of viewport in complex-plane units
    int         max_iter        =  256;
    FormulaType formula         =  FormulaType::Standard;
    bool        julia_mode      =  false;
    double      julia_re        = -0.7;
    double      julia_im        =  0.27015;
    int         palette         =  7;    // Classic Ultra
    int         pal_offset      =  0;
    int         multibrot_exp   =  2;    // integer exponent for Mandelbar/MultiFast (2-8)
    double      multibrot_exp_f =  3.0;  // float exponent for MultiSlow
    int         color_mode      =  0;    // ColorMode: 0=smooth, 1=lyap interior, 2=lyap full

    // Top-level mode
    FractalMode mode            =  FractalMode::EscapeTime;

    // Newton-specific fields (only meaningful when mode == Newton)
    int         newton_degree        = 3;       // 2-8
    double      newton_roots_re[8]   = {};      // root positions (complex)
    double      newton_roots_im[8]   = {};
    double      newton_coeffs_re[9]  = {};      // expanded polynomial coefficients (cached)
    double      newton_coeffs_im[9]  = {};      // coeffs[k] = coeff of z^k; leading z^n = 1 implicit
    bool        newton_coeffs_dirty  = true;
};

inline double zoom_display(const ViewState& vs)
{
    return 4.0 / vs.view_width;
}

// Returns a human-readable name combining formula and Julia mode.
inline const char* fractal_name(const ViewState& vs)
{
    if (vs.mode == FractalMode::Newton) {
        static char newton_buf[32];
        std::snprintf(newton_buf, sizeof(newton_buf), "Newton (deg %d)", vs.newton_degree);
        return newton_buf;
    }
    switch (vs.formula) {
        case FormulaType::Standard:
            return vs.julia_mode ? "Julia"              : "Mandelbrot";
        case FormulaType::BurningShip:
            return vs.julia_mode ? "Burning Ship Julia" : "Burning Ship";
        case FormulaType::Mandelbar:
            return vs.julia_mode ? "Mandelbar Julia"    : "Mandelbar";
        case FormulaType::MultiFast:
            return vs.julia_mode ? "Multijulia"         : "Multibrot";
        case FormulaType::MultiSlow:
            return vs.julia_mode ? "Multijulia (slow)"  : "Multibrot (slow)";
        case FormulaType::Celtic:
            return vs.julia_mode ? "Celtic Julia"       : "Celtic";
        case FormulaType::Buffalo:
            return vs.julia_mode ? "Buffalo Julia"      : "Buffalo";
        case FormulaType::Collatz:
            return "Collatz";
    }
    return "Unknown";
}

inline ViewState default_view_for(FormulaType)
{
    return ViewState{};  // center (0,0), width 4.0 — same for all formula types
}

// Reset navigation (center, zoom) to the default while preserving all
// user-controlled parameters: Julia params, palette, offset, exponents, iter limit.
inline void reset_view_keep_params(ViewState& vs, FormulaType new_formula, bool new_julia_mode)
{
    const double      jre   = vs.julia_re;
    const double      jim   = vs.julia_im;
    const int         pal   = vs.palette;
    const int         poff  = vs.pal_offset;
    const int         mexp  = vs.multibrot_exp;
    const double      mexpf = vs.multibrot_exp_f;
    const int         iter  = vs.max_iter;
    const int         cmode = vs.color_mode;
    const FractalMode mode  = vs.mode;

    // Preserve Newton state across resets
    const int    ndeg = vs.newton_degree;
    double       nrre[8], nrim[8], ncre[9], ncim[9];
    for (int i = 0; i < 8; ++i) { nrre[i] = vs.newton_roots_re[i]; nrim[i] = vs.newton_roots_im[i]; }
    for (int i = 0; i < 9; ++i) { ncre[i] = vs.newton_coeffs_re[i]; ncim[i] = vs.newton_coeffs_im[i]; }
    const bool   ncdirty = vs.newton_coeffs_dirty;

    vs                = default_view_for(new_formula);
    vs.formula        = new_formula;
    vs.julia_mode     = new_julia_mode;
    vs.julia_re       = jre;
    vs.julia_im       = jim;
    vs.palette        = pal;
    vs.pal_offset     = poff;
    vs.multibrot_exp  = mexp;
    vs.multibrot_exp_f = mexpf;
    vs.max_iter       = iter;
    vs.color_mode     = cmode;
    vs.mode           = mode;

    vs.newton_degree  = ndeg;
    for (int i = 0; i < 8; ++i) { vs.newton_roots_re[i] = nrre[i]; vs.newton_roots_im[i] = nrim[i]; }
    for (int i = 0; i < 9; ++i) { vs.newton_coeffs_re[i] = ncre[i]; vs.newton_coeffs_im[i] = ncim[i]; }
    vs.newton_coeffs_dirty = ncdirty;
}

// Place newton_degree roots evenly on the unit circle.
inline void newton_init_roots(ViewState& vs)
{
    const int n = vs.newton_degree;
    const double two_pi = 2.0 * 3.14159265358979323846;
    for (int k = 0; k < n; ++k) {
        const double angle = two_pi * k / n;
        vs.newton_roots_re[k] = std::cos(angle);
        vs.newton_roots_im[k] = std::sin(angle);
    }
    // Zero out unused root slots
    for (int k = n; k < 8; ++k) {
        vs.newton_roots_re[k] = 0.0;
        vs.newton_roots_im[k] = 0.0;
    }
    vs.newton_coeffs_dirty = true;
}

// Expand product(z - r_k) into polynomial coefficients using incremental algorithm.
// coeffs[k] = coefficient of z^k for k=0..degree-1; leading z^degree = 1 (implicit).
inline void newton_expand_roots(ViewState& vs)
{
    const int n = vs.newton_degree;
    // Start with (z - r_0): coeffs = [-r_0], leading z^1 = 1
    // We store coeffs[0..n-1]; coeffs[k] is the coefficient of z^k.
    // The leading z^n coefficient is always 1 (implicit).

    // Initialize: polynomial = 1 (constant)
    double pre[9] = {}, pim[9] = {};
    pre[0] = 1.0; pim[0] = 0.0;  // p(z) = 1
    int deg = 0;

    for (int k = 0; k < n; ++k) {
        // Multiply by (z - r_k):  new_p = p * z - p * r_k
        const double rr = vs.newton_roots_re[k];
        const double ri = vs.newton_roots_im[k];

        // Shift up (multiply by z) and subtract r_k * old
        double nre[9] = {}, nim[9] = {};
        // z * p: shift coefficients up by 1
        for (int j = deg; j >= 0; --j) {
            nre[j + 1] += pre[j];
            nim[j + 1] += pim[j];
        }
        // Subtract r_k * p
        for (int j = 0; j <= deg; ++j) {
            // (a+bi)(c+di) = (ac-bd) + (ad+bc)i
            nre[j] -= pre[j] * rr - pim[j] * ri;
            nim[j] -= pre[j] * ri + pim[j] * rr;
        }
        deg++;
        for (int j = 0; j <= deg; ++j) { pre[j] = nre[j]; pim[j] = nim[j]; }
    }

    // Now pre[0..n] has the full polynomial with pre[n] = 1.0 (leading).
    // Store coeffs[0..n-1] (everything except the leading z^n term).
    for (int k = 0; k < n; ++k) {
        vs.newton_coeffs_re[k] = pre[k];
        vs.newton_coeffs_im[k] = pim[k];
    }
    vs.newton_coeffs_dirty = false;
}
