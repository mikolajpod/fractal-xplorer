#pragma once

#include "view_state.hpp"   // FormulaType

// AVX accelerated escape-time kernels — implementations in escape_time_avx.cpp
// Each function computes 4 consecutive horizontal pixels at once.
// x0:    real coordinate of pixel column 0 (viewport left edge)
// px:    pixel column of the leftmost of the 4 pixels
// scale: complex units per pixel
// im:    imaginary coordinate (same for all 4 pixels in a row)
// out4:  receives 4 smooth iteration values
//
// Lane k's coordinate is computed as x0 + (px+k)*scale — the exact same
// double-rounding as the scalar path — so both paths iterate bit-identical
// trajectories. Do not "optimize" this back to re0 + k*scale.

void avx_mandelbrot_4(double x0, int px, double scale, double im,
                      int max_iter, double* out4);

void avx_julia_4(double x0, int px, double scale, double im,
                 int max_iter, double julia_re, double julia_im, double* out4);

void avx_burning_ship_4(double x0, int px, double scale, double im,
                        int max_iter, double* out4);

void avx_mandelbar_4(double x0, int px, double scale, double im,
                     int max_iter, double* out4);

// Integer exponent >= 3 (n=2 uses the standard mandelbrot/julia functions)
void avx_multibrot_4(double x0, int px, double scale, double im,
                     int max_iter, int exp_n, double* out4);

void avx_multijulia_4(double x0, int px, double scale, double im,
                      int max_iter, int exp_n,
                      double julia_re, double julia_im, double* out4);

// Mandelbar with integer exponent >= 3 (n=2 uses avx_mandelbar_4)
void avx_mandelbar_multi_4(double x0, int px, double scale, double im,
                           int max_iter, int exp_n, double* out4);

// Julia variants for Burning Ship and Mandelbar
void avx_burning_ship_julia_4(double x0, int px, double scale, double im,
                              int max_iter, double julia_re, double julia_im,
                              double* out4);

void avx_mandelbar_julia_4(double x0, int px, double scale, double im,
                           int max_iter, double julia_re, double julia_im,
                           double* out4);

void avx_mandelbar_multi_julia_4(double x0, int px, double scale, double im,
                                 int max_iter, int exp_n,
                                 double julia_re, double julia_im, double* out4);

// Celtic: |Re(z^2)| + i Im(z^2) + c
void avx_celtic_4(double x0, int px, double scale, double im,
                  int max_iter, double* out4);
void avx_celtic_julia_4(double x0, int px, double scale, double im,
                        int max_iter, double julia_re, double julia_im, double* out4);

// Buffalo: |Re(z^2)| + i|Im(z^2)| + c
void avx_buffalo_4(double x0, int px, double scale, double im,
                   int max_iter, double* out4);
void avx_buffalo_julia_4(double x0, int px, double scale, double im,
                         int max_iter, double julia_re, double julia_im, double* out4);

// MultiSlow: real-exponent z^n+c via polar form (SLEEF trig/exp)
void avx_multibrot_slow_4(double x0, int px, double scale, double im,
                          int max_iter, double exp_n, double* out4);
void avx_multijulia_slow_4(double x0, int px, double scale, double im,
                            int max_iter, double exp_n,
                            double julia_re, double julia_im, double* out4);

// Collatz: (2+7z-(2+5z)*cos(pi*z))/4, z0=pixel, no c parameter
void avx_collatz_4(double x0, int px, double scale, double im,
                   int max_iter, double* out4);

// Perpendicular family: abs on a single factor of the imaginary product
//   Perp. Mandelbrot:   re = zr^2-zi^2,   im = -2|zr|zi
//   Perp. Burning Ship: re = zr^2-zi^2,   im =  2 zr|zi|
//   Perp. Celtic:       re = |zr^2-zi^2|, im = -2|zr|zi
//   Perp. Buffalo:      re = |zr^2-zi^2|, im = -2 zr|zi|
void avx_perp_mandelbrot_4(double x0, int px, double scale, double im,
                           int max_iter, double* out4);
void avx_perp_mandelbrot_julia_4(double x0, int px, double scale, double im,
                                 int max_iter, double julia_re, double julia_im, double* out4);
void avx_perp_burning_ship_4(double x0, int px, double scale, double im,
                             int max_iter, double* out4);
void avx_perp_burning_ship_julia_4(double x0, int px, double scale, double im,
                                   int max_iter, double julia_re, double julia_im, double* out4);
void avx_perp_celtic_4(double x0, int px, double scale, double im,
                       int max_iter, double* out4);
void avx_perp_celtic_julia_4(double x0, int px, double scale, double im,
                             int max_iter, double julia_re, double julia_im, double* out4);
void avx_perp_buffalo_4(double x0, int px, double scale, double im,
                        int max_iter, double* out4);
void avx_perp_buffalo_julia_4(double x0, int px, double scale, double im,
                              int max_iter, double julia_re, double julia_im, double* out4);

// Lambda: z -> c*z*(1-z) (complex logistic map; z0 = 1/2 in Mandelbrot mode)
void avx_lambda_4(double x0, int px, double scale, double im,
                  int max_iter, double* out4);
void avx_lambda_julia_4(double x0, int px, double scale, double im,
                        int max_iter, double julia_re, double julia_im, double* out4);

// Lyapunov dispatch — computes both smooth and lambda for 4 pixels.
// Covers all formula x julia_mode combinations internally.
void avx_lyapunov_4(FormulaType formula, bool julia_mode,
                    double x0, int px, double scale, double im,
                    int max_iter, int exp_i, double exp_f,
                    double julia_re, double julia_im,
                    double* smooth4, double* lyap4);
