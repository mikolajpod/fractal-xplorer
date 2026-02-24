#pragma once

// AVX2 accelerated fractal kernels — implementations in cpu_renderer_avx.cpp
// Each function computes 4 consecutive horizontal pixels at once.
// re0:   real coordinate of the leftmost of the 4 pixels
// scale: complex units per pixel
// im:    imaginary coordinate (same for all 4 pixels in a row)
// out4:  receives 4 smooth iteration values

void avx2_mandelbrot_4(double re0, double scale, double im,
                        int max_iter, double* out4);

void avx2_julia_4(double re0, double scale, double im,
                  int max_iter, double julia_re, double julia_im, double* out4);

void avx2_burning_ship_4(double re0, double scale, double im,
                          int max_iter, double* out4);

void avx2_mandelbar_4(double re0, double scale, double im,
                       int max_iter, double* out4);

// Integer exponent >= 3 (n=2 uses the standard mandelbrot/julia functions)
void avx2_multibrot_4(double re0, double scale, double im,
                       int max_iter, int exp_n, double* out4);

void avx2_multijulia_4(double re0, double scale, double im,
                        int max_iter, int exp_n,
                        double julia_re, double julia_im, double* out4);

// Mandelbar with integer exponent >= 3 (n=2 uses avx2_mandelbar_4)
void avx2_mandelbar_multi_4(double re0, double scale, double im,
                              int max_iter, int exp_n, double* out4);

// Julia variants for Burning Ship and Mandelbar
void avx2_burning_ship_julia_4(double re0, double scale, double im,
                                int max_iter, double julia_re, double julia_im,
                                double* out4);

void avx2_mandelbar_julia_4(double re0, double scale, double im,
                              int max_iter, double julia_re, double julia_im,
                              double* out4);

void avx2_mandelbar_multi_julia_4(double re0, double scale, double im,
                                   int max_iter, int exp_n,
                                   double julia_re, double julia_im, double* out4);

// Celtic: |Re(z^2)| + i Im(z^2) + c
void avx2_celtic_4(double re0, double scale, double im,
                   int max_iter, double* out4);
void avx2_celtic_julia_4(double re0, double scale, double im,
                          int max_iter, double julia_re, double julia_im, double* out4);

// Buffalo: |Re(z^2)| + i|Im(z^2)| + c
void avx2_buffalo_4(double re0, double scale, double im,
                    int max_iter, double* out4);
void avx2_buffalo_julia_4(double re0, double scale, double im,
                           int max_iter, double julia_re, double julia_im, double* out4);

// MultiSlow: real-exponent z^n+c via polar form (SLEEF trig/exp)
void avx2_multibrot_slow_4(double re0, double scale, double im,
                            int max_iter, double exp_n, double* out4);
void avx2_multijulia_slow_4(double re0, double scale, double im,
                              int max_iter, double exp_n,
                              double julia_re, double julia_im, double* out4);
