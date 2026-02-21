#pragma once

enum class FractalType { Mandelbrot = 0, Julia = 1, BurningShip = 2 };

struct ViewState {
    double      center_x   = -0.5;
    double      center_y   =  0.0;
    double      view_width =  3.5;  // width of viewport in complex-plane units
    int         max_iter   =  256;
    FractalType fractal    =  FractalType::Mandelbrot;
    double      julia_re   = -0.7;
    double      julia_im   =  0.27015;
    int         palette    =  7;   // Classic Ultra
    int         pal_offset =  0;
};

inline double zoom_display(const ViewState& vs)
{
    return 3.5 / vs.view_width;
}

inline const char* fractal_name(const ViewState& vs)
{
    switch (vs.fractal) {
        case FractalType::Mandelbrot:  return "Mandelbrot";
        case FractalType::Julia:       return "Julia";
        case FractalType::BurningShip: return "Burning Ship";
    }
    return "Unknown";
}
