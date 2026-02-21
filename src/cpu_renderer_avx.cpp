// Compiled with -mavx2 -mfma — do NOT include from other translation units.

#include "fractal_avx.hpp"

#include <immintrin.h>
#include <algorithm>
#include <cmath>

// -----------------------------------------------------------------------
// Generic AVX2+FMA kernel — 4 consecutive horizontal pixels per call.
//
// Changes vs initial version:
//  - iters tracked via counter accumulation (no set1_pd(i+1) per iteration)
//  - off-by-one fixed: iters_d counts completed iterations before escape,
//    matching the scalar formula: smooth = iter + 1 - nu
//  - z update uses FMA (_mm256_fmadd_pd / _mm256_fnmadd_pd)
// -----------------------------------------------------------------------
template<bool IsJulia, bool IsBurningShip, bool IsMandelbar>
static void avx2_kernel(double re0, double scale, double im, int max_iter,
                        double c_re, double c_im, double* out4)
{
    __m256d re4 = _mm256_set_pd(re0 + 3.0*scale, re0 + 2.0*scale,
                                 re0 +     scale,  re0);
    __m256d cr, ci, zr, zi;
    if constexpr (IsJulia) {
        cr = _mm256_set1_pd(c_re);
        ci = _mm256_set1_pd(c_im);
        zr = re4;
        zi = _mm256_set1_pd(im);
    } else {
        cr = re4;
        ci = _mm256_set1_pd(im);
        zr = _mm256_setzero_pd();
        zi = _mm256_setzero_pd();
    }

    const __m256d four     = _mm256_set1_pd(4.0);
    const __m256d one      = _mm256_set1_pd(1.0);
    const __m256d sign_bit = _mm256_set1_pd(-0.0);  // 0x8000000000000000

    // active: all bits set for lanes that have not yet escaped
    __m256d active   = _mm256_castsi256_pd(_mm256_set1_epi64x(-1LL));
    // iters_d counts completed iterations (incremented AFTER z update, for
    // still-active lanes). At escape step i: iters_d[k] == i, giving
    // smooth = i + 1 - nu, matching the scalar formula.
    __m256d iters_d  = _mm256_setzero_pd();
    __m256d final_r2 = _mm256_set1_pd(4.0);

    for (int i = 0; i < max_iter; ++i) {
        const __m256d zr2  = _mm256_mul_pd(zr, zr);
        const __m256d zi2  = _mm256_mul_pd(zi, zi);
        const __m256d mag2 = _mm256_add_pd(zr2, zi2);

        // Lanes escaping this iteration (mag2 > 4 AND still active)
        const __m256d just_esc = _mm256_and_pd(
            _mm256_cmp_pd(mag2, four, _CMP_GT_OQ), active);

        // Record |z|^2 at escape for smooth coloring
        final_r2 = _mm256_blendv_pd(final_r2, mag2, just_esc);

        // Remove newly escaped lanes from active set
        active = _mm256_andnot_pd(just_esc, active);

        if (_mm256_movemask_pd(active) == 0) break;

        // Update z using FMA (2 instructions instead of 4)
        __m256d new_zr, new_zi;
        if constexpr (IsBurningShip) {
            const __m256d azr = _mm256_andnot_pd(sign_bit, zr);  // |zr|
            const __m256d azi = _mm256_andnot_pd(sign_bit, zi);  // |zi|
            new_zr = _mm256_fmadd_pd (zr, zr,
                         _mm256_fnmadd_pd(zi, zi, cr));     // zr^2 - zi^2 + cr
            new_zi = _mm256_fmadd_pd (_mm256_add_pd(azr, azr), azi, ci);  // 2|zr||zi| + ci
        } else {
            new_zr = _mm256_fmadd_pd (zr, zr,
                         _mm256_fnmadd_pd(zi, zi, cr));     // zr^2 - zi^2 + cr
            if constexpr (IsMandelbar)
                new_zi = _mm256_fnmadd_pd(_mm256_add_pd(zr, zr), zi, ci); // -2*zr*zi + ci
            else
                new_zi = _mm256_fmadd_pd (_mm256_add_pd(zr, zr), zi, ci); //  2*zr*zi + ci
        }

        // Freeze escaped lanes (blendv only needed when some lanes have escaped)
        zr = _mm256_blendv_pd(zr, new_zr, active);
        zi = _mm256_blendv_pd(zi, new_zi, active);

        // Increment counter for still-active lanes
        // and(active, one): adds 1.0 for active lanes, 0.0 for escaped ones
        iters_d = _mm256_add_pd(iters_d, _mm256_and_pd(active, one));
    }

    // Extract results and apply smooth coloring
    double iters_arr[4], r2_arr[4], active_arr[4];
    _mm256_storeu_pd(iters_arr,  iters_d);
    _mm256_storeu_pd(r2_arr,     final_r2);
    _mm256_storeu_pd(active_arr, active);

    const double max_d    = static_cast<double>(max_iter);
    const double inv_log2 = 1.0 / std::log(2.0);

    for (int k = 0; k < 4; ++k) {
        if (active_arr[k] != 0.0) {
            out4[k] = max_d;  // interior point
        } else {
            // smooth = iters + 1 - log2(log2(|z|))
            const double log_zn = std::log(r2_arr[k]) * 0.5;
            const double nu     = std::log(log_zn * inv_log2) * inv_log2;
            out4[k] = std::max(0.0, iters_arr[k] + 1.0 - nu);
        }
    }
}

// -----------------------------------------------------------------------
// Public entry points
// -----------------------------------------------------------------------

void avx2_mandelbrot_4(double re0, double scale, double im,
                        int max_iter, double* out4)
{
    avx2_kernel<false, false, false>(re0, scale, im, max_iter, 0.0, 0.0, out4);
}

void avx2_julia_4(double re0, double scale, double im,
                  int max_iter, double julia_re, double julia_im, double* out4)
{
    avx2_kernel<true, false, false>(re0, scale, im, max_iter, julia_re, julia_im, out4);
}

void avx2_burning_ship_4(double re0, double scale, double im,
                          int max_iter, double* out4)
{
    avx2_kernel<false, true, false>(re0, scale, im, max_iter, 0.0, 0.0, out4);
}

void avx2_mandelbar_4(double re0, double scale, double im,
                       int max_iter, double* out4)
{
    avx2_kernel<false, false, true>(re0, scale, im, max_iter, 0.0, 0.0, out4);
}

// -----------------------------------------------------------------------
// AVX2 kernel for integer-exponent Multibrot/Multijulia (exp_n >= 3).
// Uses repeated complex multiplication to compute z^n without trig.
// Smooth coloring uses log(exp_n) as the base instead of log(2).
// -----------------------------------------------------------------------
template<bool IsJulia, bool IsMandelbar = false>
static void avx2_multibrot_kernel(double re0, double scale, double im, int max_iter,
                                   int exp_n, double c_re, double c_im, double* out4)
{
    __m256d re4 = _mm256_set_pd(re0 + 3.0*scale, re0 + 2.0*scale,
                                 re0 +     scale,  re0);
    __m256d cr, ci, zr, zi;
    if constexpr (IsJulia) {
        cr = _mm256_set1_pd(c_re);
        ci = _mm256_set1_pd(c_im);
        zr = re4;
        zi = _mm256_set1_pd(im);
    } else {
        cr = re4;
        ci = _mm256_set1_pd(im);
        zr = _mm256_setzero_pd();
        zi = _mm256_setzero_pd();
    }

    const __m256d four     = _mm256_set1_pd(4.0);
    const __m256d one      = _mm256_set1_pd(1.0);
    const __m256d sign_bit = _mm256_set1_pd(-0.0);  // used by IsMandelbar

    __m256d active   = _mm256_castsi256_pd(_mm256_set1_epi64x(-1LL));
    __m256d iters_d  = _mm256_setzero_pd();
    __m256d final_r2 = _mm256_set1_pd(4.0);

    for (int i = 0; i < max_iter; ++i) {
        const __m256d zr2  = _mm256_mul_pd(zr, zr);
        const __m256d zi2  = _mm256_mul_pd(zi, zi);
        const __m256d mag2 = _mm256_add_pd(zr2, zi2);

        const __m256d just_esc = _mm256_and_pd(
            _mm256_cmp_pd(mag2, four, _CMP_GT_OQ), active);
        final_r2 = _mm256_blendv_pd(final_r2, mag2, just_esc);
        active   = _mm256_andnot_pd(just_esc, active);

        if (_mm256_movemask_pd(active) == 0) break;

        // z^exp_n via repeated complex multiplication: pw = pw * z
        __m256d pw_r = zr, pw_i = zi;
        for (int p = 1; p < exp_n; ++p) {
            const __m256d new_pr = _mm256_fmsub_pd(pw_r, zr,
                                       _mm256_mul_pd(pw_i, zi));   // pw_r*zr - pw_i*zi
            pw_i = _mm256_fmadd_pd(pw_r, zi,
                       _mm256_mul_pd(pw_i, zr));                   // pw_r*zi + pw_i*zr
            pw_r = new_pr;
        }

        if constexpr (IsMandelbar)
            pw_i = _mm256_xor_pd(pw_i, sign_bit);  // conj(z^n): negate imag part

        __m256d new_zr = _mm256_add_pd(pw_r, cr);
        __m256d new_zi = _mm256_add_pd(pw_i, ci);

        zr = _mm256_blendv_pd(zr, new_zr, active);
        zi = _mm256_blendv_pd(zi, new_zi, active);
        iters_d = _mm256_add_pd(iters_d, _mm256_and_pd(active, one));
    }

    double iters_arr[4], r2_arr[4], active_arr[4];
    _mm256_storeu_pd(iters_arr,  iters_d);
    _mm256_storeu_pd(r2_arr,     final_r2);
    _mm256_storeu_pd(active_arr, active);

    const double max_d    = static_cast<double>(max_iter);
    const double inv_logn = 1.0 / std::log(static_cast<double>(exp_n));

    for (int k = 0; k < 4; ++k) {
        if (active_arr[k] != 0.0) {
            out4[k] = max_d;
        } else {
            const double log_zn = std::log(r2_arr[k]) * 0.5;
            const double nu     = std::log(log_zn * inv_logn) * inv_logn;
            out4[k] = std::max(0.0, iters_arr[k] + 1.0 - nu);
        }
    }
}

void avx2_multibrot_4(double re0, double scale, double im,
                       int max_iter, int exp_n, double* out4)
{
    avx2_multibrot_kernel<false>(re0, scale, im, max_iter, exp_n, 0.0, 0.0, out4);
}

void avx2_multijulia_4(double re0, double scale, double im,
                        int max_iter, int exp_n,
                        double julia_re, double julia_im, double* out4)
{
    avx2_multibrot_kernel<true>(re0, scale, im, max_iter, exp_n, julia_re, julia_im, out4);
}

void avx2_mandelbar_multi_4(double re0, double scale, double im,
                              int max_iter, int exp_n, double* out4)
{
    avx2_multibrot_kernel<false, true>(re0, scale, im, max_iter, exp_n, 0.0, 0.0, out4);
}
