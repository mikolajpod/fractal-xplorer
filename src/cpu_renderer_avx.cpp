// Compiled with -mavx only — do NOT include from other translation units.

#include "cpu_renderer_avx.hpp"

#include <immintrin.h>
#include <sleef.h>
#include <algorithm>
#include <cmath>

// -----------------------------------------------------------------------
// Generic AVX kernel — 4 consecutive horizontal pixels per call.
//
// Changes vs initial version:
//  - iters tracked via counter accumulation (no set1_pd(i+1) per iteration)
//  - off-by-one fixed: iters_d counts completed iterations before escape,
//    matching the scalar formula: smooth = iter + 1 - nu
//  - z update uses mul+add/sub (no FMA)
// -----------------------------------------------------------------------
template<bool IsJulia, bool IsBurningShip, bool IsMandelbar,
         bool AbsRe = false, bool AbsIm = false, bool ComputeLyapunov = false>
static void avx_kernel(double re0, double scale, double im, int max_iter,
                        double c_re, double c_im, double* out4,
                        double* lyap_out4 = nullptr)
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

    // Lyapunov accumulators (degree 2: log|f'| = log(2) + 0.5*log(|z|^2))
    __m256d log_deriv_sum, lyap_n_iters;
    __m256d log_n_v, nm1_half_v;
    if constexpr (ComputeLyapunov) {
        log_deriv_sum = _mm256_setzero_pd();
        lyap_n_iters  = _mm256_setzero_pd();
        log_n_v       = _mm256_set1_pd(std::log(2.0));
        nm1_half_v    = _mm256_set1_pd(0.5);
    }

    for (int i = 0; i < max_iter; ++i) {
        const __m256d zr2  = _mm256_mul_pd(zr, zr);
        const __m256d zi2  = _mm256_mul_pd(zi, zi);
        const __m256d mag2 = _mm256_add_pd(zr2, zi2);

        // Lyapunov: accumulate log|f'(z)| for active lanes with mag2 > eps
        if constexpr (ComputeLyapunov) {
            __m256d safe_mag2 = _mm256_max_pd(mag2, _mm256_set1_pd(1e-300));
            __m256d log_mag2  = Sleef_logd4_u35(safe_mag2);
            __m256d log_deriv = _mm256_add_pd(_mm256_mul_pd(nm1_half_v, log_mag2), log_n_v);
            __m256d accum_mask = _mm256_and_pd(active,
                _mm256_cmp_pd(mag2, _mm256_set1_pd(1e-200), _CMP_GT_OQ));
            log_deriv_sum = _mm256_add_pd(log_deriv_sum, _mm256_and_pd(accum_mask, log_deriv));
            lyap_n_iters  = _mm256_add_pd(lyap_n_iters,  _mm256_and_pd(accum_mask, one));
        }

        // Lanes escaping this iteration (mag2 > 4 AND still active)
        const __m256d just_esc = _mm256_and_pd(
            _mm256_cmp_pd(mag2, four, _CMP_GT_OQ), active);

        // Record |z|^2 at escape for smooth coloring
        final_r2 = _mm256_blendv_pd(final_r2, mag2, just_esc);

        // Remove newly escaped lanes from active set
        active = _mm256_andnot_pd(just_esc, active);

        if (_mm256_movemask_pd(active) == 0) break;

        // Update z
        __m256d new_zr, new_zi;
        if constexpr (IsBurningShip) {
            const __m256d azr = _mm256_andnot_pd(sign_bit, zr);  // |zr|
            const __m256d azi = _mm256_andnot_pd(sign_bit, zi);  // |zi|
            new_zr = _mm256_add_pd(_mm256_mul_pd(zr, zr),
                         _mm256_sub_pd(cr, _mm256_mul_pd(zi, zi)));  // zr^2 - zi^2 + cr
            new_zi = _mm256_add_pd(_mm256_mul_pd(_mm256_add_pd(azr, azr), azi), ci);  // 2|zr||zi| + ci
        } else if constexpr (AbsRe || AbsIm) {
            // Celtic (AbsRe only) / Buffalo (AbsRe+AbsIm): abs applied after squaring
            const __m256d re_raw = _mm256_sub_pd(zr2, zi2);                       // zr^2 - zi^2
            const __m256d im_raw = _mm256_mul_pd(_mm256_add_pd(zr, zr), zi);      // 2*zr*zi
            new_zr = _mm256_add_pd(
                AbsRe ? _mm256_andnot_pd(sign_bit, re_raw) : re_raw, cr);
            new_zi = _mm256_add_pd(
                AbsIm ? _mm256_andnot_pd(sign_bit, im_raw) : im_raw, ci);
        } else {
            new_zr = _mm256_add_pd(_mm256_mul_pd(zr, zr),
                         _mm256_sub_pd(cr, _mm256_mul_pd(zi, zi)));  // zr^2 - zi^2 + cr
            if constexpr (IsMandelbar)
                new_zi = _mm256_sub_pd(ci, _mm256_mul_pd(_mm256_add_pd(zr, zr), zi)); // -2*zr*zi + ci
            else
                new_zi = _mm256_add_pd(_mm256_mul_pd(_mm256_add_pd(zr, zr), zi), ci); //  2*zr*zi + ci
        }

        // Freeze escaped lanes (blendv only needed when some lanes have escaped)
        zr = _mm256_blendv_pd(zr, new_zr, active);
        zi = _mm256_blendv_pd(zi, new_zi, active);

        // Increment counter for still-active lanes
        // and(active, one): adds 1.0 for active lanes, 0.0 for escaped ones
        iters_d = _mm256_add_pd(iters_d, _mm256_and_pd(active, one));
    }

    // Vectorized smooth coloring using SLEEF
    const __m256d max_d_v = _mm256_set1_pd(static_cast<double>(max_iter));
    const __m256d inv_log2 = _mm256_set1_pd(1.0 / std::log(2.0));
    const __m256d half     = _mm256_set1_pd(0.5);
    const __m256d one_v    = _mm256_set1_pd(1.0);
    const __m256d zero_v   = _mm256_setzero_pd();

    // smooth = iters + 1 - log2(log2(|z|))
    __m256d log_zn = _mm256_mul_pd(Sleef_logd4_u35(final_r2), half);       // log(|z|)
    __m256d nu     = _mm256_mul_pd(Sleef_logd4_u35(_mm256_mul_pd(log_zn, inv_log2)), inv_log2);
    __m256d smooth = _mm256_max_pd(zero_v, _mm256_sub_pd(_mm256_add_pd(iters_d, one_v), nu));
    // Interior points (still active) get max_iter; escaped points get smooth value
    __m256d result = _mm256_blendv_pd(smooth, max_d_v, active);
    _mm256_storeu_pd(out4, result);

    if constexpr (ComputeLyapunov) {
        __m256d safe_n = _mm256_max_pd(lyap_n_iters, one_v);
        __m256d lambda = _mm256_div_pd(log_deriv_sum, safe_n);
        _mm256_storeu_pd(lyap_out4, lambda);
    }
}

// -----------------------------------------------------------------------
// Public entry points
// -----------------------------------------------------------------------

void avx_mandelbrot_4(double re0, double scale, double im,
                      int max_iter, double* out4)
{
    avx_kernel<false, false, false>(re0, scale, im, max_iter, 0.0, 0.0, out4);
}

void avx_julia_4(double re0, double scale, double im,
                 int max_iter, double julia_re, double julia_im, double* out4)
{
    avx_kernel<true, false, false>(re0, scale, im, max_iter, julia_re, julia_im, out4);
}

void avx_burning_ship_4(double re0, double scale, double im,
                        int max_iter, double* out4)
{
    avx_kernel<false, true, false>(re0, scale, im, max_iter, 0.0, 0.0, out4);
}

void avx_mandelbar_4(double re0, double scale, double im,
                     int max_iter, double* out4)
{
    avx_kernel<false, false, true>(re0, scale, im, max_iter, 0.0, 0.0, out4);
}

// -----------------------------------------------------------------------
// AVX kernel for integer-exponent Multibrot/Multijulia (exp_n >= 3).
// Uses repeated complex multiplication to compute z^n without trig.
// Smooth coloring uses log(exp_n) as the base instead of log(2).
// -----------------------------------------------------------------------
template<bool IsJulia, bool IsMandelbar = false, bool ComputeLyapunov = false>
static void avx_multibrot_kernel(double re0, double scale, double im, int max_iter,
                                   int exp_n, double c_re, double c_im, double* out4,
                                   double* lyap_out4 = nullptr)
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

    // Lyapunov accumulators
    __m256d log_deriv_sum, lyap_n_iters;
    __m256d log_n_v, nm1_half_v;
    if constexpr (ComputeLyapunov) {
        log_deriv_sum = _mm256_setzero_pd();
        lyap_n_iters  = _mm256_setzero_pd();
        log_n_v       = _mm256_set1_pd(std::log(static_cast<double>(exp_n)));
        nm1_half_v    = _mm256_set1_pd((exp_n - 1) / 2.0);
    }

    for (int i = 0; i < max_iter; ++i) {
        const __m256d zr2  = _mm256_mul_pd(zr, zr);
        const __m256d zi2  = _mm256_mul_pd(zi, zi);
        const __m256d mag2 = _mm256_add_pd(zr2, zi2);

        if constexpr (ComputeLyapunov) {
            __m256d safe_mag2 = _mm256_max_pd(mag2, _mm256_set1_pd(1e-300));
            __m256d log_mag2  = Sleef_logd4_u35(safe_mag2);
            __m256d log_deriv = _mm256_add_pd(_mm256_mul_pd(nm1_half_v, log_mag2), log_n_v);
            __m256d accum_mask = _mm256_and_pd(active,
                _mm256_cmp_pd(mag2, _mm256_set1_pd(1e-200), _CMP_GT_OQ));
            log_deriv_sum = _mm256_add_pd(log_deriv_sum, _mm256_and_pd(accum_mask, log_deriv));
            lyap_n_iters  = _mm256_add_pd(lyap_n_iters,  _mm256_and_pd(accum_mask, one));
        }

        const __m256d just_esc = _mm256_and_pd(
            _mm256_cmp_pd(mag2, four, _CMP_GT_OQ), active);
        final_r2 = _mm256_blendv_pd(final_r2, mag2, just_esc);
        active   = _mm256_andnot_pd(just_esc, active);

        if (_mm256_movemask_pd(active) == 0) break;

        // z^exp_n via repeated complex multiplication: pw = pw * z
        __m256d pw_r = zr, pw_i = zi;
        for (int p = 1; p < exp_n; ++p) {
            const __m256d new_pr = _mm256_sub_pd(_mm256_mul_pd(pw_r, zr),
                                       _mm256_mul_pd(pw_i, zi));   // pw_r*zr - pw_i*zi
            pw_i = _mm256_add_pd(_mm256_mul_pd(pw_r, zi),
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

    // Vectorized smooth coloring using SLEEF
    const __m256d max_d_v = _mm256_set1_pd(static_cast<double>(max_iter));
    const __m256d inv_logn = _mm256_set1_pd(1.0 / std::log(static_cast<double>(exp_n)));
    const __m256d half     = _mm256_set1_pd(0.5);
    const __m256d one_v    = _mm256_set1_pd(1.0);
    const __m256d zero_v   = _mm256_setzero_pd();

    // smooth = iters + 1 - log_n(log_n(|z|))
    __m256d log_zn = _mm256_mul_pd(Sleef_logd4_u35(final_r2), half);       // log(|z|)
    __m256d nu     = _mm256_mul_pd(Sleef_logd4_u35(_mm256_mul_pd(log_zn, inv_logn)), inv_logn);
    __m256d smooth = _mm256_max_pd(zero_v, _mm256_sub_pd(_mm256_add_pd(iters_d, one_v), nu));
    // Interior points (still active) get max_iter; escaped points get smooth value
    __m256d result = _mm256_blendv_pd(smooth, max_d_v, active);
    _mm256_storeu_pd(out4, result);

    if constexpr (ComputeLyapunov) {
        __m256d safe_n = _mm256_max_pd(lyap_n_iters, one_v);
        __m256d lambda = _mm256_div_pd(log_deriv_sum, safe_n);
        _mm256_storeu_pd(lyap_out4, lambda);
    }
}

void avx_multibrot_4(double re0, double scale, double im,
                     int max_iter, int exp_n, double* out4)
{
    avx_multibrot_kernel<false>(re0, scale, im, max_iter, exp_n, 0.0, 0.0, out4);
}

void avx_multijulia_4(double re0, double scale, double im,
                      int max_iter, int exp_n,
                      double julia_re, double julia_im, double* out4)
{
    avx_multibrot_kernel<true>(re0, scale, im, max_iter, exp_n, julia_re, julia_im, out4);
}

void avx_mandelbar_multi_4(double re0, double scale, double im,
                           int max_iter, int exp_n, double* out4)
{
    avx_multibrot_kernel<false, true>(re0, scale, im, max_iter, exp_n, 0.0, 0.0, out4);
}

// -----------------------------------------------------------------------
// AVX kernel for real-exponent Multibrot/Multijulia (MultiSlow).
// Uses polar form: z^n = |z|^n * e^(i*n*theta), vectorized with SLEEF.
// -----------------------------------------------------------------------
template<bool IsJulia, bool ComputeLyapunov = false>
static void avx_multibrot_slow_kernel(double re0, double scale, double im,
                                        int max_iter, double exp_n,
                                        double c_re, double c_im, double* out4,
                                        double* lyap_out4 = nullptr)
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

    const __m256d four  = _mm256_set1_pd(4.0);
    const __m256d one   = _mm256_set1_pd(1.0);
    const __m256d exp_v = _mm256_set1_pd(exp_n);
    const __m256d half  = _mm256_set1_pd(0.5);

    __m256d active   = _mm256_castsi256_pd(_mm256_set1_epi64x(-1LL));
    __m256d iters_d  = _mm256_setzero_pd();
    __m256d final_r2 = _mm256_set1_pd(4.0);

    // Lyapunov accumulators
    __m256d log_deriv_sum, lyap_n_iters;
    __m256d log_n_v, nm1_half_v;
    if constexpr (ComputeLyapunov) {
        log_deriv_sum = _mm256_setzero_pd();
        lyap_n_iters  = _mm256_setzero_pd();
        log_n_v       = _mm256_set1_pd(std::log(exp_n));
        nm1_half_v    = _mm256_set1_pd((exp_n - 1.0) / 2.0);
    }

    for (int i = 0; i < max_iter; ++i) {
        const __m256d zr2  = _mm256_mul_pd(zr, zr);
        const __m256d zi2  = _mm256_mul_pd(zi, zi);
        const __m256d mag2 = _mm256_add_pd(zr2, zi2);

        if constexpr (ComputeLyapunov) {
            __m256d safe_mag2 = _mm256_max_pd(mag2, _mm256_set1_pd(1e-300));
            __m256d log_mag2  = Sleef_logd4_u35(safe_mag2);
            __m256d log_deriv = _mm256_add_pd(_mm256_mul_pd(nm1_half_v, log_mag2), log_n_v);
            __m256d accum_mask = _mm256_and_pd(active,
                _mm256_cmp_pd(mag2, _mm256_set1_pd(1e-200), _CMP_GT_OQ));
            log_deriv_sum = _mm256_add_pd(log_deriv_sum, _mm256_and_pd(accum_mask, log_deriv));
            lyap_n_iters  = _mm256_add_pd(lyap_n_iters,  _mm256_and_pd(accum_mask, one));
        }

        const __m256d just_esc = _mm256_and_pd(
            _mm256_cmp_pd(mag2, four, _CMP_GT_OQ), active);
        final_r2 = _mm256_blendv_pd(final_r2, mag2, just_esc);
        active   = _mm256_andnot_pd(just_esc, active);

        if (_mm256_movemask_pd(active) == 0) break;

        // z^n via polar form: r_n = |z|^n, theta = arg(z)
        __m256d log_mag = _mm256_mul_pd(Sleef_logd4_u35(mag2), half);
        __m256d r_n     = Sleef_expd4_u10(_mm256_mul_pd(exp_v, log_mag));
        __m256d theta   = Sleef_atan2d4_u10(zi, zr);
        __m256d n_theta = _mm256_mul_pd(exp_v, theta);
        Sleef___m256d_2 sc = Sleef_sincosd4_u10(n_theta);
        __m256d new_zr = _mm256_add_pd(_mm256_mul_pd(r_n, sc.y), cr);
        __m256d new_zi = _mm256_add_pd(_mm256_mul_pd(r_n, sc.x), ci);

        zr = _mm256_blendv_pd(zr, new_zr, active);
        zi = _mm256_blendv_pd(zi, new_zi, active);
        iters_d = _mm256_add_pd(iters_d, _mm256_and_pd(active, one));
    }

    // Vectorized smooth coloring using SLEEF
    const __m256d max_d_v  = _mm256_set1_pd(static_cast<double>(max_iter));
    const __m256d inv_logn = _mm256_set1_pd(1.0 / std::log(exp_n));
    const __m256d one_v    = _mm256_set1_pd(1.0);
    const __m256d zero_v   = _mm256_setzero_pd();

    __m256d log_zn = _mm256_mul_pd(Sleef_logd4_u35(final_r2), half);
    __m256d nu     = _mm256_mul_pd(Sleef_logd4_u35(_mm256_mul_pd(log_zn, inv_logn)), inv_logn);
    __m256d smooth = _mm256_max_pd(zero_v, _mm256_sub_pd(_mm256_add_pd(iters_d, one_v), nu));
    __m256d result = _mm256_blendv_pd(smooth, max_d_v, active);
    _mm256_storeu_pd(out4, result);

    if constexpr (ComputeLyapunov) {
        __m256d safe_n = _mm256_max_pd(lyap_n_iters, one_v);
        __m256d lambda = _mm256_div_pd(log_deriv_sum, safe_n);
        _mm256_storeu_pd(lyap_out4, lambda);
    }
}

void avx_multibrot_slow_4(double re0, double scale, double im,
                          int max_iter, double exp_n, double* out4)
{
    avx_multibrot_slow_kernel<false>(re0, scale, im, max_iter, exp_n, 0.0, 0.0, out4);
}

void avx_multijulia_slow_4(double re0, double scale, double im,
                            int max_iter, double exp_n,
                            double julia_re, double julia_im, double* out4)
{
    avx_multibrot_slow_kernel<true>(re0, scale, im, max_iter, exp_n, julia_re, julia_im, out4);
}

void avx_burning_ship_julia_4(double re0, double scale, double im,
                              int max_iter, double julia_re, double julia_im,
                              double* out4)
{
    avx_kernel<true, true, false>(re0, scale, im, max_iter, julia_re, julia_im, out4);
}

void avx_mandelbar_julia_4(double re0, double scale, double im,
                           int max_iter, double julia_re, double julia_im,
                           double* out4)
{
    avx_kernel<true, false, true>(re0, scale, im, max_iter, julia_re, julia_im, out4);
}

void avx_mandelbar_multi_julia_4(double re0, double scale, double im,
                                 int max_iter, int exp_n,
                                 double julia_re, double julia_im, double* out4)
{
    avx_multibrot_kernel<true, true>(re0, scale, im, max_iter, exp_n, julia_re, julia_im, out4);
}

// -----------------------------------------------------------------------
// Celtic and Buffalo entry points
// -----------------------------------------------------------------------

void avx_celtic_4(double re0, double scale, double im,
                  int max_iter, double* out4)
{
    avx_kernel<false, false, false, true, false>(re0, scale, im, max_iter, 0.0, 0.0, out4);
}

void avx_celtic_julia_4(double re0, double scale, double im,
                        int max_iter, double julia_re, double julia_im, double* out4)
{
    avx_kernel<true, false, false, true, false>(re0, scale, im, max_iter, julia_re, julia_im, out4);
}

void avx_buffalo_4(double re0, double scale, double im,
                   int max_iter, double* out4)
{
    avx_kernel<false, false, false, true, true>(re0, scale, im, max_iter, 0.0, 0.0, out4);
}

void avx_buffalo_julia_4(double re0, double scale, double im,
                         int max_iter, double julia_re, double julia_im, double* out4)
{
    avx_kernel<true, false, false, true, true>(re0, scale, im, max_iter, julia_re, julia_im, out4);
}

// -----------------------------------------------------------------------
// Lyapunov dispatch — computes both smooth and lambda for 4 pixels.
// -----------------------------------------------------------------------
void avx_lyapunov_4(FormulaType formula, bool julia_mode,
                    double re0, double scale, double im,
                    int max_iter, int exp_i, double exp_f,
                    double julia_re, double julia_im,
                    double* smooth4, double* lyap4)
{
    // For MultiSlow: if float exponent is effectively an integer, promote
    const int slow_int_n = [&]() -> int {
        if (formula != FormulaType::MultiSlow) return 0;
        const int n = static_cast<int>(std::round(exp_f));
        return (n >= 2 && std::abs(exp_f - n) < 1e-9) ? n : 0;
    }();

    switch (formula) {
        case FormulaType::Standard:
            if (julia_mode)
                avx_kernel<true,false,false,false,false,true>(re0,scale,im,max_iter,julia_re,julia_im,smooth4,lyap4);
            else
                avx_kernel<false,false,false,false,false,true>(re0,scale,im,max_iter,0.0,0.0,smooth4,lyap4);
            break;
        case FormulaType::BurningShip:
            if (julia_mode)
                avx_kernel<true,true,false,false,false,true>(re0,scale,im,max_iter,julia_re,julia_im,smooth4,lyap4);
            else
                avx_kernel<false,true,false,false,false,true>(re0,scale,im,max_iter,0.0,0.0,smooth4,lyap4);
            break;
        case FormulaType::Celtic:
            if (julia_mode)
                avx_kernel<true,false,false,true,false,true>(re0,scale,im,max_iter,julia_re,julia_im,smooth4,lyap4);
            else
                avx_kernel<false,false,false,true,false,true>(re0,scale,im,max_iter,0.0,0.0,smooth4,lyap4);
            break;
        case FormulaType::Buffalo:
            if (julia_mode)
                avx_kernel<true,false,false,true,true,true>(re0,scale,im,max_iter,julia_re,julia_im,smooth4,lyap4);
            else
                avx_kernel<false,false,false,true,true,true>(re0,scale,im,max_iter,0.0,0.0,smooth4,lyap4);
            break;
        case FormulaType::Mandelbar:
            if (julia_mode) {
                if (exp_i == 2)
                    avx_kernel<true,false,true,false,false,true>(re0,scale,im,max_iter,julia_re,julia_im,smooth4,lyap4);
                else
                    avx_multibrot_kernel<true,true,true>(re0,scale,im,max_iter,exp_i,julia_re,julia_im,smooth4,lyap4);
            } else {
                if (exp_i == 2)
                    avx_kernel<false,false,true,false,false,true>(re0,scale,im,max_iter,0.0,0.0,smooth4,lyap4);
                else
                    avx_multibrot_kernel<false,true,true>(re0,scale,im,max_iter,exp_i,0.0,0.0,smooth4,lyap4);
            }
            break;
        case FormulaType::MultiFast:
            if (julia_mode) {
                if (exp_i == 2)
                    avx_kernel<true,false,false,false,false,true>(re0,scale,im,max_iter,julia_re,julia_im,smooth4,lyap4);
                else
                    avx_multibrot_kernel<true,false,true>(re0,scale,im,max_iter,exp_i,julia_re,julia_im,smooth4,lyap4);
            } else {
                if (exp_i == 2)
                    avx_kernel<false,false,false,false,false,true>(re0,scale,im,max_iter,0.0,0.0,smooth4,lyap4);
                else
                    avx_multibrot_kernel<false,false,true>(re0,scale,im,max_iter,exp_i,0.0,0.0,smooth4,lyap4);
            }
            break;
        case FormulaType::MultiSlow:
            if (slow_int_n > 0) {
                if (julia_mode) {
                    if (slow_int_n == 2)
                        avx_kernel<true,false,false,false,false,true>(re0,scale,im,max_iter,julia_re,julia_im,smooth4,lyap4);
                    else
                        avx_multibrot_kernel<true,false,true>(re0,scale,im,max_iter,slow_int_n,julia_re,julia_im,smooth4,lyap4);
                } else {
                    if (slow_int_n == 2)
                        avx_kernel<false,false,false,false,false,true>(re0,scale,im,max_iter,0.0,0.0,smooth4,lyap4);
                    else
                        avx_multibrot_kernel<false,false,true>(re0,scale,im,max_iter,slow_int_n,0.0,0.0,smooth4,lyap4);
                }
            } else {
                if (julia_mode)
                    avx_multibrot_slow_kernel<true,true>(re0,scale,im,max_iter,exp_f,julia_re,julia_im,smooth4,lyap4);
                else
                    avx_multibrot_slow_kernel<false,true>(re0,scale,im,max_iter,exp_f,0.0,0.0,smooth4,lyap4);
            }
            break;
        default:
            avx_kernel<false,false,false,false,false,true>(re0,scale,im,max_iter,0.0,0.0,smooth4,lyap4);
            break;
    }
}
