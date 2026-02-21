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
