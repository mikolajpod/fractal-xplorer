#pragma once

#include <cmath>
#include "view_state.hpp"

struct NewtonResult { int root; double smooth; };

// Evaluate p(z) and p'(z) via Horner's method.
// Polynomial: z^n + coeffs[n-1]*z^(n-1) + ... + coeffs[1]*z + coeffs[0]
// (leading z^n coefficient is implicit 1)
inline void horner_eval(double zr, double zi, int degree,
                        const double* coeffs_re, const double* coeffs_im,
                        double& pr, double& pi, double& dr, double& di)
{
    // p starts as z^n's leading coeff = 1, d starts as 0
    pr = 1.0; pi = 0.0;
    dr = 0.0; di = 0.0;

    for (int k = degree - 1; k >= 0; --k) {
        // d = d * z + p
        const double ndr = dr * zr - di * zi + pr;
        const double ndi = dr * zi + di * zr + pi;
        dr = ndr; di = ndi;

        // p = p * z + coeffs[k]
        const double npr = pr * zr - pi * zi + coeffs_re[k];
        const double npi = pr * zi + pi * zr + coeffs_im[k];
        pr = npr; pi = npi;
    }
}

// Newton iteration for a single pixel.
// Returns which root the pixel converged to and smooth iteration count.
// When compute_smooth=false, returns integer iteration count (no log).
template <bool ComputeSmooth = true>
inline NewtonResult newton_iter(double re, double im, const ViewState& vs)
{
    const int degree = vs.newton_degree;
    double zr = re, zi = im;

    for (int i = 0; i < vs.max_iter; ++i) {
        double pr, pi, dr, di;
        horner_eval(zr, zi, degree, vs.newton_coeffs_re, vs.newton_coeffs_im, pr, pi, dr, di);

        // Complex division: f(z) / f'(z)
        const double denom = dr * dr + di * di;
        if (denom < 1e-30) break;  // degenerate derivative

        const double step_re = (pr * dr + pi * di) / denom;
        const double step_im = (pi * dr - pr * di) / denom;

        zr -= step_re;
        zi -= step_im;

        // Check convergence
        const double step_mag2 = step_re * step_re + step_im * step_im;
        if (step_mag2 < 1e-20) {
            // Find nearest root
            int best = 0;
            double best_dist = 1e30;
            for (int r = 0; r < degree; ++r) {
                const double dx = zr - vs.newton_roots_re[r];
                const double dy = zi - vs.newton_roots_im[r];
                const double d2 = dx * dx + dy * dy;
                if (d2 < best_dist) { best_dist = d2; best = r; }
            }
            if constexpr (ComputeSmooth) {
                const double frac = std::log(1e-20) / std::log(step_mag2);
                return {best, static_cast<double>(i) + frac};
            } else {
                return {best, static_cast<double>(i)};
            }
        }
    }
    return {-1, static_cast<double>(vs.max_iter)};
}
