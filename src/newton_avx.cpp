#include "newton_avx.hpp"
#include <cmath>
#include <immintrin.h>

// AVX Newton fractal kernel — 4 pixels at a time using 256-bit double SIMD.
// No SLEEF needed — only basic arithmetic (mul, add, sub, div).

void avx_newton_4(double re0, double scale, double im,
                  int max_iter, int degree,
                  const double* coeffs_re, const double* coeffs_im,
                  const double* roots_re, const double* roots_im,
                  int* root4, double* smooth4)
{
    // Initial z values: 4 consecutive pixels
    __m256d zr = _mm256_add_pd(_mm256_set1_pd(re0),
                               _mm256_mul_pd(_mm256_set1_pd(scale),
                                             _mm256_set_pd(3.0, 2.0, 1.0, 0.0)));
    __m256d zi = _mm256_set1_pd(im);

    __m256d iters_d    = _mm256_setzero_pd();
    __m256d frozen_step_mag2 = _mm256_set1_pd(1.0); // step_mag2 at convergence
    __m256d active     = _mm256_castsi256_pd(_mm256_set1_epi64x(-1LL)); // all active
    const __m256d one  = _mm256_set1_pd(1.0);
    const __m256d conv_thresh = _mm256_set1_pd(1e-20);
    const __m256d degen_thresh = _mm256_set1_pd(1e-30);

    for (int i = 0; i < max_iter; ++i) {
        // Horner evaluation of p(z) and p'(z)
        // p = 1, d = 0 (leading coefficient is implicit 1)
        __m256d pr = one, pi = _mm256_setzero_pd();
        __m256d dr = _mm256_setzero_pd(), di = _mm256_setzero_pd();

        for (int k = degree - 1; k >= 0; --k) {
            __m256d ck_re = _mm256_set1_pd(coeffs_re[k]);
            __m256d ck_im = _mm256_set1_pd(coeffs_im[k]);

            // d = d * z + p
            __m256d ndr = _mm256_add_pd(_mm256_sub_pd(_mm256_mul_pd(dr, zr),
                                                       _mm256_mul_pd(di, zi)), pr);
            __m256d ndi = _mm256_add_pd(_mm256_add_pd(_mm256_mul_pd(dr, zi),
                                                       _mm256_mul_pd(di, zr)), pi);
            dr = ndr; di = ndi;

            // p = p * z + coeffs[k]
            __m256d npr = _mm256_add_pd(_mm256_sub_pd(_mm256_mul_pd(pr, zr),
                                                       _mm256_mul_pd(pi, zi)), ck_re);
            __m256d npi = _mm256_add_pd(_mm256_add_pd(_mm256_mul_pd(pr, zi),
                                                       _mm256_mul_pd(pi, zr)), ck_im);
            pr = npr; pi = npi;
        }

        // Complex division: step = f(z) / f'(z)
        // denom = dr*dr + di*di
        __m256d denom = _mm256_add_pd(_mm256_mul_pd(dr, dr), _mm256_mul_pd(di, di));

        // Protect against degenerate denominator — set step to 0 for those lanes
        __m256d denom_ok = _mm256_cmp_pd(denom, degen_thresh, _CMP_GE_OQ);
        denom = _mm256_blendv_pd(one, denom, denom_ok);  // avoid division by zero

        __m256d inv_denom = _mm256_div_pd(one, denom);
        __m256d step_re = _mm256_mul_pd(_mm256_add_pd(_mm256_mul_pd(pr, dr),
                                                       _mm256_mul_pd(pi, di)), inv_denom);
        __m256d step_im = _mm256_mul_pd(_mm256_sub_pd(_mm256_mul_pd(pi, dr),
                                                       _mm256_mul_pd(pr, di)), inv_denom);

        // Zero out step for degenerate lanes
        step_re = _mm256_and_pd(step_re, denom_ok);
        step_im = _mm256_and_pd(step_im, denom_ok);

        // Newton update: z -= step (only for active lanes)
        __m256d new_zr = _mm256_sub_pd(zr, _mm256_and_pd(step_re, active));
        __m256d new_zi = _mm256_sub_pd(zi, _mm256_and_pd(step_im, active));

        // Check convergence: |step|^2 < threshold
        __m256d step_mag2 = _mm256_add_pd(_mm256_mul_pd(step_re, step_re),
                                           _mm256_mul_pd(step_im, step_im));
        __m256d converged = _mm256_cmp_pd(step_mag2, conv_thresh, _CMP_LT_OQ);

        // Deactivate converged lanes; freeze step_mag2 for smooth computation
        __m256d newly_done = _mm256_and_pd(converged, active);
        frozen_step_mag2 = _mm256_blendv_pd(frozen_step_mag2, step_mag2, newly_done);
        active = _mm256_andnot_pd(converged, active);

        // Also deactivate degenerate lanes
        __m256d degen_and_active = _mm256_andnot_pd(denom_ok,
                                    _mm256_castsi256_pd(_mm256_set1_epi64x(-1LL)));
        active = _mm256_andnot_pd(degen_and_active, active);

        zr = new_zr;
        zi = new_zi;

        // Increment iteration counter for still-active lanes
        iters_d = _mm256_add_pd(iters_d, _mm256_and_pd(one, active));

        // Also add 1 for lanes that just converged this iteration (they count this iter)
        // (no — iters_d already represents iterations *before* this one was added for active)
        // Actually: iters_d was incremented only for active lanes, and newly converged
        // lanes were removed from active before the increment. So iters_d for converged
        // lanes holds the correct value (i iterations completed).

        // Early exit: all lanes done
        if (_mm256_testz_pd(active, active)) break;
    }

    // Extract final z, iteration counts, and frozen step_mag2
    double final_zr[4], final_zi[4], final_iters[4], final_smag2[4];
    _mm256_storeu_pd(final_zr, zr);
    _mm256_storeu_pd(final_zi, zi);
    _mm256_storeu_pd(final_iters, iters_d);
    _mm256_storeu_pd(final_smag2, frozen_step_mag2);

    // Find nearest root for each pixel and compute smooth value
    for (int p = 0; p < 4; ++p) {
        const int it = static_cast<int>(final_iters[p]);
        if (it >= max_iter) {
            root4[p] = -1;
            smooth4[p] = static_cast<double>(max_iter);
            continue;
        }
        int best = 0;
        double best_dist = 1e30;
        for (int r = 0; r < degree; ++r) {
            const double dx = final_zr[p] - roots_re[r];
            const double dy = final_zi[p] - roots_im[r];
            const double d2 = dx * dx + dy * dy;
            if (d2 < best_dist) { best_dist = d2; best = r; }
        }
        root4[p] = (best_dist < 1.0) ? best : -1;
        const double log_smag2 = std::log(final_smag2[p]);
        const double frac = (log_smag2 < 0.0) ? std::log(1e-20) / log_smag2 : 0.0;
        smooth4[p] = final_iters[p] + frac;
    }
}
