#pragma once

enum class FractalType {
    Mandelbrot     = 0,  // fast; multibrot_exp>2 => fast Multibrot (AVX2)
    Julia          = 1,  // fast; multibrot_exp>2 => fast Multijulia (AVX2)
    BurningShip    = 2,
    Mandelbar      = 3,
    MultibroSlow   = 4,  // float exponent, scalar only
    MultijuliaSlow = 5,  // float exponent + fixed c, scalar only
};
constexpr int FRACTAL_COUNT = 6;

struct ViewState {
    double      center_x        =  0.0;
    double      center_y        =  0.0;
    double      view_width      =  4.0;  // width of viewport in complex-plane units
    int         max_iter        =  256;
    FractalType fractal         =  FractalType::Mandelbrot;
    double      julia_re        = -0.7;
    double      julia_im        =  0.27015;
    int         palette         =  7;    // Classic Ultra
    int         pal_offset      =  0;
    int         multibrot_exp   =  2;    // integer exponent for Mandelbrot/Julia fast path (2-8)
    double      multibrot_exp_f =  3.0;  // float exponent for slow path
};

inline double zoom_display(const ViewState& vs)
{
    return 4.0 / vs.view_width;
}

inline const char* fractal_name(const ViewState& vs)
{
    switch (vs.fractal) {
        case FractalType::Mandelbrot:     return "Mandelbrot";
        case FractalType::Julia:          return "Julia";
        case FractalType::BurningShip:    return "Burning Ship";
        case FractalType::Mandelbar:      return "Mandelbar";
        case FractalType::MultibroSlow:   return "Multibrot (slow)";
        case FractalType::MultijuliaSlow: return "Multijulia (slow)";
    }
    return "Unknown";
}

inline ViewState default_view_for(FractalType)
{
    return ViewState{};  // center (0,0), width 4.0 — same for all fractal types
}

// Reset navigation (center, zoom) to the default for `new_ft` while preserving
// all user-controlled parameters: fractal type, Julia params, palette, palette
// offset, exponents, and iteration limit.
inline void reset_view_keep_params(ViewState& vs, FractalType new_ft)
{
    const double      jre   = vs.julia_re;
    const double      jim   = vs.julia_im;
    const int         pal   = vs.palette;
    const int         poff  = vs.pal_offset;
    const int         mexp  = vs.multibrot_exp;
    const double      mexpf = vs.multibrot_exp_f;
    const int         iter  = vs.max_iter;
    vs               = default_view_for(new_ft);
    vs.fractal        = new_ft;
    vs.julia_re       = jre;
    vs.julia_im       = jim;
    vs.palette        = pal;
    vs.pal_offset     = poff;
    vs.multibrot_exp  = mexp;
    vs.multibrot_exp_f = mexpf;
    vs.max_iter       = iter;
}
