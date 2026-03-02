#pragma once

enum class FormulaType {
    Standard    = 0,  // z^2 + c  (always degree 2, no exponent slider)
    BurningShip = 1,  // (|Re z| + i|Im z|)^2 + c
    Celtic      = 2,  // |Re(z^2)| + i Im(z^2) + c
    Buffalo     = 3,  // |Re(z^2)| + i|Im(z^2)| + c
    Mandelbar   = 4,  // conj(z)^n + c  (integer exp 2-8)
    MultiFast   = 5,  // z^n + c  (integer exp 2-8, AVX)
    MultiSlow   = 6,  // z^n + c  (real exp, scalar)
};
constexpr int FORMULA_COUNT = 7;

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
};

inline double zoom_display(const ViewState& vs)
{
    return 4.0 / vs.view_width;
}

// Returns a human-readable name combining formula and Julia mode.
inline const char* fractal_name(const ViewState& vs)
{
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
}
