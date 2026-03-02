#pragma once

#include <cmath>
#include <vector>
#include "view_state.hpp"

// Returns smooth iteration count for escaped points, or max_iter for interior.
// Smooth coloring uses the "normalized iteration count" (log-log) formula.

// Template 1: degree-2 formulas (Standard, BurningShip, Mandelbar n=2, Celtic, Buffalo)
template<bool IsJulia, bool IsBurningShip, bool IsMandelbar,
         bool AbsRe = false, bool AbsIm = false>
inline double scalar_kernel(double re, double im, double cr, double ci, int max_iter)
{
    double zr = IsJulia ? re : 0.0;
    double zi = IsJulia ? im : 0.0;
    const double c_re = IsJulia ? cr : re;
    const double c_im = IsJulia ? ci : im;
    const double log2 = std::log(2.0);
    int i = 0;
    while (i < max_iter) {
        const double zr2 = zr*zr, zi2 = zi*zi;
        if (zr2 + zi2 > 4.0) {
            const double log_zn = std::log(zr2 + zi2) * 0.5;
            const double nu     = std::log(log_zn / log2) / log2;
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr, new_zi;
        if constexpr (IsBurningShip) {
            new_zr = zr2 - zi2 + c_re;
            new_zi = std::abs(2.0 * zr * zi) + c_im;
        } else if constexpr (AbsRe || AbsIm) {
            new_zr = (AbsRe ? std::abs(zr2 - zi2) : zr2 - zi2) + c_re;
            new_zi = (AbsIm ? std::abs(2.0*zr*zi) : 2.0*zr*zi) + c_im;
        } else {
            new_zr = zr2 - zi2 + c_re;
            new_zi = (IsMandelbar ? -2.0*zr*zi : 2.0*zr*zi) + c_im;
        }
        zr = new_zr;
        zi = new_zi;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// Template 2: integer exponent >= 2 (MultiFast, Mandelbar n>=3)
template<bool IsJulia, bool IsMandelbar = false>
inline double scalar_multibrot_kernel(double re, double im, double cr, double ci,
                                       int max_iter, int n)
{
    double zr = IsJulia ? re : 0.0;
    double zi = IsJulia ? im : 0.0;
    const double c_re = IsJulia ? cr : re;
    const double c_im = IsJulia ? ci : im;
    const double log_n = std::log(static_cast<double>(n));
    int i = 0;
    while (i < max_iter) {
        const double zr2 = zr*zr, zi2 = zi*zi;
        if (zr2 + zi2 > 4.0) {
            const double log_zn = std::log(zr2 + zi2) * 0.5;
            const double nu     = std::log(log_zn / log_n) / log_n;
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double pr = zr, pi = zi;
        for (int k = 1; k < n; ++k) {
            const double new_pr = pr*zr - pi*zi;
            pi = pr*zi + pi*zr;
            pr = new_pr;
        }
        zr =              pr + c_re;
        zi = (IsMandelbar ? -pi : pi) + c_im;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// Template 3: real exponent (MultiSlow) via polar form
template<bool IsJulia>
inline double scalar_multibrot_slow_kernel(double re, double im, double cr, double ci,
                                            int max_iter, double n)
{
    double zr = IsJulia ? re : 0.0;
    double zi = IsJulia ? im : 0.0;
    const double c_re = IsJulia ? cr : re;
    const double c_im = IsJulia ? ci : im;
    const double log_n = std::log(n);
    int i = 0;
    while (i < max_iter) {
        const double mag2 = zr*zr + zi*zi;
        if (mag2 > 4.0) {
            const double log_zn = std::log(mag2) * 0.5;
            const double nu     = std::log(log_zn / log_n) / log_n;
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        if (mag2 == 0.0) { zr = c_re; zi = c_im; }
        else {
            const double r_n   = std::exp(n * std::log(mag2) * 0.5);
            const double theta = std::atan2(zi, zr);
            zr = r_n * std::cos(n * theta) + c_re;
            zi = r_n * std::sin(n * theta) + c_im;
        }
        ++i;
    }
    return static_cast<double>(max_iter);
}

// Named wrappers — thin one-liners; all call sites unchanged.
inline double mandelbrot_iter(double re, double im, int max_iter)
    { return scalar_kernel<false,false,false>(re, im, 0, 0, max_iter); }

inline double julia_iter(double re, double im, double cr, double ci, int max_iter)
    { return scalar_kernel<true,false,false>(re, im, cr, ci, max_iter); }

inline double mandelbar_iter(double re, double im, int max_iter)
    { return scalar_kernel<false,false,true>(re, im, 0, 0, max_iter); }

inline double mandelbar_julia_iter(double re, double im, double cr, double ci, int max_iter)
    { return scalar_kernel<true,false,true>(re, im, cr, ci, max_iter); }

inline double burning_ship_iter(double re, double im, int max_iter)
    { return scalar_kernel<false,true,false>(re, im, 0, 0, max_iter); }

inline double burning_ship_julia_iter(double re, double im, double cr, double ci, int max_iter)
    { return scalar_kernel<true,true,false>(re, im, cr, ci, max_iter); }

inline double celtic_iter(double re, double im, int max_iter)
    { return scalar_kernel<false,false,false,true,false>(re, im, 0, 0, max_iter); }

inline double celtic_julia_iter(double re, double im, double cr, double ci, int max_iter)
    { return scalar_kernel<true,false,false,true,false>(re, im, cr, ci, max_iter); }

inline double buffalo_iter(double re, double im, int max_iter)
    { return scalar_kernel<false,false,false,true,true>(re, im, 0, 0, max_iter); }

inline double buffalo_julia_iter(double re, double im, double cr, double ci, int max_iter)
    { return scalar_kernel<true,false,false,true,true>(re, im, cr, ci, max_iter); }

inline double multibrot_iter(double re, double im, int max_iter, int n)
    { return scalar_multibrot_kernel<false>(re, im, 0, 0, max_iter, n); }

inline double multijulia_iter(double re, double im, double cr, double ci, int max_iter, int n)
    { return scalar_multibrot_kernel<true>(re, im, cr, ci, max_iter, n); }

inline double mandelbar_multi_iter(double re, double im, int max_iter, int n)
    { return scalar_multibrot_kernel<false,true>(re, im, 0, 0, max_iter, n); }

inline double mandelbar_multi_julia_iter(double re, double im, double cr, double ci,
                                          int max_iter, int n)
    { return scalar_multibrot_kernel<true,true>(re, im, cr, ci, max_iter, n); }

inline double multibrot_slow_iter(double re, double im, int max_iter, double n)
    { return scalar_multibrot_slow_kernel<false>(re, im, 0, 0, max_iter, n); }

inline double multijulia_slow_iter(double re, double im, double cr, double ci,
                                    int max_iter, double n)
    { return scalar_multibrot_slow_kernel<true>(re, im, cr, ci, max_iter, n); }

// Generic scalar Lyapunov iteration: returns {smooth, lambda} for any formula.
// lambda = (1/N) * sum(log|f'(z_k)|), where log|f'(z)| = log(n) + (n-1)/2 * log(|z|^2).
struct SmoothLyapunov { double smooth; double lambda; };

inline SmoothLyapunov scalar_lyapunov_iter(double re, double im, const ViewState& vs)
{
    double zr, zi, cr, ci;
    if (vs.julia_mode) {
        zr = re; zi = im;
        cr = vs.julia_re; ci = vs.julia_im;
    } else {
        zr = 0.0; zi = 0.0;
        cr = re; ci = im;
    }

    // Determine exponent for smooth coloring and Lyapunov derivative
    const double exp_n = [&]() -> double {
        switch (vs.formula) {
            case FormulaType::Mandelbar:
            case FormulaType::MultiFast:
                return static_cast<double>(vs.multibrot_exp);
            case FormulaType::MultiSlow:
                return vs.multibrot_exp_f;
            default:  // Standard, BurningShip, Celtic, Buffalo
                return 2.0;
        }
    }();
    const double log_n     = std::log(exp_n);
    const double half_nm1  = (exp_n - 1.0) * 0.5;

    double lyap_sum = 0.0;
    int    count    = 0;

    for (int i = 0; i < vs.max_iter; ++i) {
        const double mag2 = zr * zr + zi * zi;

        // Accumulate Lyapunov: log|f'(z)| = log(n) + (n-1)/2 * log(|z|^2)
        if (mag2 > 0.0) {
            lyap_sum += log_n + half_nm1 * std::log(mag2);
            ++count;
        }

        if (mag2 > 4.0) {
            // Smooth escape-time
            const double log_zn = std::log(mag2) * 0.5;
            const double nu     = std::log(log_zn / log_n) / log_n;
            const double smooth = std::max(0.0, static_cast<double>(i) + 1.0 - nu);
            const double lambda = (count > 0) ? lyap_sum / count : 0.0;
            return {smooth, lambda};
        }

        // z-update per formula
        double new_zr, new_zi;
        switch (vs.formula) {
            case FormulaType::Standard:
                new_zr = zr*zr - zi*zi + cr;
                new_zi = 2.0*zr*zi + ci;
                break;
            case FormulaType::BurningShip: {
                const double azr = std::abs(zr), azi = std::abs(zi);
                new_zr = azr*azr - azi*azi + cr;
                new_zi = 2.0*azr*azi + ci;
                break;
            }
            case FormulaType::Celtic: {
                const double zr2 = zr*zr, zi2 = zi*zi;
                new_zr = std::abs(zr2 - zi2) + cr;
                new_zi = 2.0*zr*zi + ci;
                break;
            }
            case FormulaType::Buffalo: {
                const double zr2 = zr*zr, zi2 = zi*zi;
                new_zr = std::abs(zr2 - zi2) + cr;
                new_zi = std::abs(2.0*zr*zi) + ci;
                break;
            }
            case FormulaType::Mandelbar: {
                const int n = vs.multibrot_exp;
                if (n == 2) {
                    new_zr =  zr*zr - zi*zi + cr;
                    new_zi = -2.0*zr*zi + ci;
                } else {
                    double pr = zr, pi = zi;
                    for (int k = 1; k < n; ++k) {
                        const double np = pr*zr - pi*zi;
                        pi = pr*zi + pi*zr;
                        pr = np;
                    }
                    new_zr =  pr + cr;
                    new_zi = -pi + ci;
                }
                break;
            }
            case FormulaType::MultiFast: {
                const int n = vs.multibrot_exp;
                double pr = zr, pi = zi;
                for (int k = 1; k < n; ++k) {
                    const double np = pr*zr - pi*zi;
                    pi = pr*zi + pi*zr;
                    pr = np;
                }
                new_zr = pr + cr;
                new_zi = pi + ci;
                break;
            }
            case FormulaType::MultiSlow: {
                if (mag2 == 0.0) {
                    new_zr = cr;
                    new_zi = ci;
                } else {
                    const double r_n   = std::exp(exp_n * std::log(mag2) * 0.5);
                    const double theta = std::atan2(zi, zr);
                    new_zr = r_n * std::cos(exp_n * theta) + cr;
                    new_zi = r_n * std::sin(exp_n * theta) + ci;
                }
                break;
            }
            default:
                new_zr = cr;
                new_zi = ci;
                break;
        }
        zr = new_zr;
        zi = new_zi;
    }

    // Interior point
    const double lambda = (count > 0) ? lyap_sum / count : 0.0;
    return {static_cast<double>(vs.max_iter), lambda};
}

// Returns up to max_n intermediate z values (stops early on escape).
// Works for any formula via the ViewState formula + julia_mode fields.
// Interior points (never escaping) still return all max_n+1 points.
inline std::vector<std::pair<double,double>>
compute_orbit(double re, double im, const ViewState& vs, int max_n = 20)
{
    std::vector<std::pair<double,double>> pts;
    pts.reserve(static_cast<size_t>(max_n) + 1);

    double zr, zi, cr, ci;
    if (vs.julia_mode) {
        zr = re; zi = im;
        cr = vs.julia_re; ci = vs.julia_im;
    } else {
        zr = 0.0; zi = 0.0;
        cr = re; ci = im;
    }

    pts.push_back({zr, zi});

    for (int i = 0; i < max_n; ++i) {
        double new_zr, new_zi;

        switch (vs.formula) {
            case FormulaType::Standard:
                new_zr = zr*zr - zi*zi + cr;
                new_zi = 2.0*zr*zi + ci;
                break;
            case FormulaType::BurningShip: {
                const double azr = std::abs(zr), azi = std::abs(zi);
                new_zr = azr*azr - azi*azi + cr;
                new_zi = 2.0*azr*azi + ci;
                break;
            }
            case FormulaType::Celtic: {
                const double zr2 = zr*zr, zi2 = zi*zi;
                new_zr = std::abs(zr2 - zi2) + cr;
                new_zi = 2.0*zr*zi + ci;
                break;
            }
            case FormulaType::Buffalo: {
                const double zr2 = zr*zr, zi2 = zi*zi;
                new_zr = std::abs(zr2 - zi2) + cr;
                new_zi = std::abs(2.0*zr*zi) + ci;
                break;
            }
            case FormulaType::Mandelbar: {
                const int n = vs.multibrot_exp;
                if (n == 2) {
                    new_zr =  zr*zr - zi*zi + cr;
                    new_zi = -2.0*zr*zi + ci;
                } else {
                    double pr = zr, pi = zi;
                    for (int k = 1; k < n; ++k) {
                        const double np = pr*zr - pi*zi;
                        pi = pr*zi + pi*zr;
                        pr = np;
                    }
                    new_zr =  pr + cr;
                    new_zi = -pi + ci;
                }
                break;
            }
            case FormulaType::MultiFast: {
                const int n = vs.multibrot_exp;
                double pr = zr, pi = zi;
                for (int k = 1; k < n; ++k) {
                    const double np = pr*zr - pi*zi;
                    pi = pr*zi + pi*zr;
                    pr = np;
                }
                new_zr = pr + cr;
                new_zi = pi + ci;
                break;
            }
            case FormulaType::MultiSlow: {
                const double n    = vs.multibrot_exp_f;
                const double mag2 = zr*zr + zi*zi;
                if (mag2 == 0.0) {
                    new_zr = cr;
                    new_zi = ci;
                } else {
                    const double r_n   = std::exp(n * std::log(mag2) * 0.5);
                    const double theta = std::atan2(zi, zr);
                    new_zr = r_n * std::cos(n * theta) + cr;
                    new_zi = r_n * std::sin(n * theta) + ci;
                }
                break;
            }
            default:
                new_zr = cr;
                new_zi = ci;
                break;
        }

        zr = new_zr;
        zi = new_zi;
        pts.push_back({zr, zi});
        if (zr*zr + zi*zi > 4.0) break;
    }

    return pts;
}
