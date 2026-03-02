#pragma once

#include "view_state.hpp"   // FormulaType

// AVX accelerated fractal kernels — implementations in cpu_renderer_avx.cpp
// Each function computes 4 consecutive horizontal pixels at once.
// re0:   real coordinate of the leftmost of the 4 pixels
// scale: complex units per pixel
// im:    imaginary coordinate (same for all 4 pixels in a row)
// out4:  receives 4 smooth iteration values

void avx_mandelbrot_4(double re0, double scale, double im,
                      int max_iter, double* out4);

void avx_julia_4(double re0, double scale, double im,
                 int max_iter, double julia_re, double julia_im, double* out4);

void avx_burning_ship_4(double re0, double scale, double im,
                        int max_iter, double* out4);

void avx_mandelbar_4(double re0, double scale, double im,
                     int max_iter, double* out4);

// Integer exponent >= 3 (n=2 uses the standard mandelbrot/julia functions)
void avx_multibrot_4(double re0, double scale, double im,
                     int max_iter, int exp_n, double* out4);

void avx_multijulia_4(double re0, double scale, double im,
                      int max_iter, int exp_n,
                      double julia_re, double julia_im, double* out4);

// Mandelbar with integer exponent >= 3 (n=2 uses avx_mandelbar_4)
void avx_mandelbar_multi_4(double re0, double scale, double im,
                           int max_iter, int exp_n, double* out4);

// Julia variants for Burning Ship and Mandelbar
void avx_burning_ship_julia_4(double re0, double scale, double im,
                              int max_iter, double julia_re, double julia_im,
                              double* out4);

void avx_mandelbar_julia_4(double re0, double scale, double im,
                           int max_iter, double julia_re, double julia_im,
                           double* out4);

void avx_mandelbar_multi_julia_4(double re0, double scale, double im,
                                 int max_iter, int exp_n,
                                 double julia_re, double julia_im, double* out4);

// Celtic: |Re(z^2)| + i Im(z^2) + c
void avx_celtic_4(double re0, double scale, double im,
                  int max_iter, double* out4);
void avx_celtic_julia_4(double re0, double scale, double im,
                        int max_iter, double julia_re, double julia_im, double* out4);

// Buffalo: |Re(z^2)| + i|Im(z^2)| + c
void avx_buffalo_4(double re0, double scale, double im,
                   int max_iter, double* out4);
void avx_buffalo_julia_4(double re0, double scale, double im,
                         int max_iter, double julia_re, double julia_im, double* out4);

// MultiSlow: real-exponent z^n+c via polar form (SLEEF trig/exp)
void avx_multibrot_slow_4(double re0, double scale, double im,
                          int max_iter, double exp_n, double* out4);
void avx_multijulia_slow_4(double re0, double scale, double im,
                            int max_iter, double exp_n,
                            double julia_re, double julia_im, double* out4);

// Lyapunov dispatch — computes both smooth and lambda for 4 pixels.
// Covers all formula x julia_mode combinations internally.
void avx_lyapunov_4(FormulaType formula, bool julia_mode,
                    double re0, double scale, double im,
                    int max_iter, int exp_i, double exp_f,
                    double julia_re, double julia_im,
                    double* smooth4, double* lyap4);
