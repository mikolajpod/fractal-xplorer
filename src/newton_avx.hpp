#pragma once

// AVX Newton fractal kernel — processes 4 pixels at a time.
// x0:        real coordinate of pixel column 0 (viewport left edge)
// px:        pixel column of the leftmost of the 4 pixels
// scale:     complex units per pixel
// im:        imaginary coordinate (same for all 4)
// max_iter:  iteration limit
// degree:    polynomial degree (2-8)
// coeffs_re/im: polynomial coefficients [0..degree-1] (leading z^n = 1 implicit)
// roots_re/im:  root positions [0..degree-1]
// root4:     output — which root each pixel converged to (-1 = none)
// smooth4:   output — smooth iteration count at convergence
// Flat coloring: returns integer iteration count in smooth4 (no log computation)
void avx_newton_4(double x0, int px, double scale, double im,
                  int max_iter, int degree,
                  const double* coeffs_re, const double* coeffs_im,
                  const double* roots_re, const double* roots_im,
                  int* root4, double* smooth4);

// Smooth coloring: returns smooth iteration count with fractional log-based correction
void avx_newton_smooth_4(double x0, int px, double scale, double im,
                          int max_iter, int degree,
                          const double* coeffs_re, const double* coeffs_im,
                          const double* roots_re, const double* roots_im,
                          int* root4, double* smooth4);
