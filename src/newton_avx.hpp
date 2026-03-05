#pragma once

// AVX Newton fractal kernel — processes 4 pixels at a time.
// re0:       real coordinate of leftmost pixel
// scale:     complex units per pixel
// im:        imaginary coordinate (same for all 4)
// max_iter:  iteration limit
// degree:    polynomial degree (2-8)
// coeffs_re/im: polynomial coefficients [0..degree-1] (leading z^n = 1 implicit)
// roots_re/im:  root positions [0..degree-1]
// root4:     output — which root each pixel converged to (-1 = none)
// smooth4:   output — smooth iteration count at convergence
void avx_newton_4(double re0, double scale, double im,
                  int max_iter, int degree,
                  const double* coeffs_re, const double* coeffs_im,
                  const double* roots_re, const double* roots_im,
                  int* root4, double* smooth4);
