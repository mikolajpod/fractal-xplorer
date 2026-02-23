#pragma once

#include <cmath>
#include <vector>
#include "view_state.hpp"

// Returns smooth iteration count for escaped points, or max_iter for interior.
// Smooth coloring uses the "normalized iteration count" (log-log) formula.

inline double mandelbrot_iter(double re, double im, int max_iter)
{
    double zr = 0.0, zi = 0.0;
    int i = 0;
    while (i < max_iter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            double log_zn = std::log(zr2 + zi2) * 0.5;
            double nu     = std::log(log_zn / std::log(2.0)) / std::log(2.0);
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr = zr2 - zi2 + re;
        zi = 2.0 * zr * zi + im;
        zr = new_zr;
        ++i;
    }
    return static_cast<double>(max_iter);
}

inline double julia_iter(double re, double im, double cr, double ci, int max_iter)
{
    double zr = re, zi = im;
    int i = 0;
    while (i < max_iter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            double log_zn = std::log(zr2 + zi2) * 0.5;
            double nu     = std::log(log_zn / std::log(2.0)) / std::log(2.0);
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr = zr2 - zi2 + cr;
        zi = 2.0 * zr * zi + ci;
        zr = new_zr;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = conj(z)^2 + c  (Tricorn / Mandelbar)
inline double mandelbar_iter(double re, double im, int max_iter)
{
    double zr = 0.0, zi = 0.0;
    int i = 0;
    while (i < max_iter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            double log_zn = std::log(zr2 + zi2) * 0.5;
            double nu     = std::log(log_zn / std::log(2.0)) / std::log(2.0);
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr =  zr2 - zi2 + re;
        zi            = -2.0 * zr * zi + im;   // conjugate: negate zi term
        zr = new_zr;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = z_n^exp + c, z_0 = 0  (Multibrot slow, real exponent)
// Uses polar form: z^n = |z|^n * (cos(n*arg(z)) + i*sin(n*arg(z)))
inline double multibrot_slow_iter(double re, double im, int max_iter, double n)
{
    double zr = 0.0, zi = 0.0;
    const double log_n = std::log(n);
    int i = 0;
    while (i < max_iter) {
        const double mag2 = zr * zr + zi * zi;
        if (mag2 > 4.0) {
            const double log_zn = std::log(mag2) * 0.5;
            const double nu     = std::log(log_zn / log_n) / log_n;
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        if (mag2 == 0.0) { zr = re; zi = im; }
        else {
            const double r_n   = std::exp(n * std::log(mag2) * 0.5);
            const double theta = std::atan2(zi, zr);
            zr = r_n * std::cos(n * theta) + re;
            zi = r_n * std::sin(n * theta) + im;
        }
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = z_n^exp + c, z_0 = pixel  (Multijulia slow, real exponent)
inline double multijulia_slow_iter(double re, double im, double cr, double ci,
                                    int max_iter, double n)
{
    double zr = re, zi = im;
    const double log_n = std::log(n);
    int i = 0;
    while (i < max_iter) {
        const double mag2 = zr * zr + zi * zi;
        if (mag2 > 4.0) {
            const double log_zn = std::log(mag2) * 0.5;
            const double nu     = std::log(log_zn / log_n) / log_n;
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        if (mag2 == 0.0) { zr = cr; zi = ci; }
        else {
            const double r_n   = std::exp(n * std::log(mag2) * 0.5);
            const double theta = std::atan2(zi, zr);
            zr = r_n * std::cos(n * theta) + cr;
            zi = r_n * std::sin(n * theta) + ci;
        }
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = z_n^exp + c, z_0 = 0  (Multibrot, integer exponent >= 2)
// Smooth coloring uses log(exp) instead of log(2).
inline double multibrot_iter(double re, double im, int max_iter, int n)
{
    double zr = 0.0, zi = 0.0;
    const double log_n = std::log(static_cast<double>(n));
    int i = 0;
    while (i < max_iter) {
        const double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            const double log_zn = std::log(zr2 + zi2) * 0.5;
            const double nu     = std::log(log_zn / log_n) / log_n;
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        // z^n via repeated complex multiplication
        double pr = zr, pi = zi;
        for (int k = 1; k < n; ++k) {
            const double new_pr = pr * zr - pi * zi;
            pi = pr * zi + pi * zr;
            pr = new_pr;
        }
        zr = pr + re;
        zi = pi + im;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = z_n^exp + c, z_0 = pixel  (Multijulia, integer exponent >= 2)
inline double multijulia_iter(double re, double im, double cr, double ci,
                               int max_iter, int n)
{
    double zr = re, zi = im;
    const double log_n = std::log(static_cast<double>(n));
    int i = 0;
    while (i < max_iter) {
        const double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            const double log_zn = std::log(zr2 + zi2) * 0.5;
            const double nu     = std::log(log_zn / log_n) / log_n;
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double pr = zr, pi = zi;
        for (int k = 1; k < n; ++k) {
            const double new_pr = pr * zr - pi * zi;
            pi = pr * zi + pi * zr;
            pr = new_pr;
        }
        zr = pr + cr;
        zi = pi + ci;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = conj(z)^n + c, z_0 = 0  (Mandelbar, integer exponent >= 3)
// conj(z)^n = conj(z^n), so: compute z^n via repeated multiply, then negate zi
inline double mandelbar_multi_iter(double re, double im, int max_iter, int n)
{
    double zr = 0.0, zi = 0.0;
    const double log_n = std::log(static_cast<double>(n));
    int i = 0;
    while (i < max_iter) {
        const double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            const double log_zn = std::log(zr2 + zi2) * 0.5;
            const double nu     = std::log(log_zn / log_n) / log_n;
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double pr = zr, pi = zi;
        for (int k = 1; k < n; ++k) {
            const double new_pr = pr * zr - pi * zi;
            pi = pr * zi + pi * zr;
            pr = new_pr;
        }
        zr =  pr + re;
        zi = -pi + im;   // conjugate: negate imaginary part of z^n
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = (|Re(z)| + i|Im(z)|)^2 + c
inline double burning_ship_iter(double re, double im, int max_iter)
{
    double zr = 0.0, zi = 0.0;
    int i = 0;
    while (i < max_iter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            double log_zn = std::log(zr2 + zi2) * 0.5;
            double nu     = std::log(log_zn / std::log(2.0)) / std::log(2.0);
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr = zr2 - zi2 + re;
        double new_zi = 2.0 * std::abs(zr) * std::abs(zi) + im;
        zr = new_zr;
        zi = new_zi;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = (|Re(z)| + i|Im(z)|)^2 + c, z_0 = pixel  (Burning Ship Julia)
inline double burning_ship_julia_iter(double re, double im, double cr, double ci, int max_iter)
{
    double zr = re, zi = im;
    int i = 0;
    while (i < max_iter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            double log_zn = std::log(zr2 + zi2) * 0.5;
            double nu     = std::log(log_zn / std::log(2.0)) / std::log(2.0);
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr = zr2 - zi2 + cr;
        double new_zi = 2.0 * std::abs(zr) * std::abs(zi) + ci;
        zr = new_zr;
        zi = new_zi;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = conj(z)^2 + c, z_0 = pixel  (Mandelbar Julia)
inline double mandelbar_julia_iter(double re, double im, double cr, double ci, int max_iter)
{
    double zr = re, zi = im;
    int i = 0;
    while (i < max_iter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            double log_zn = std::log(zr2 + zi2) * 0.5;
            double nu     = std::log(log_zn / std::log(2.0)) / std::log(2.0);
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr =  zr2 - zi2 + cr;
        zi            = -2.0 * zr * zi + ci;  // conjugate: negate zi term
        zr = new_zr;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = |Re(z^2)| + i Im(z^2) + c  (Celtic)
inline double celtic_iter(double re, double im, int max_iter)
{
    double zr = 0.0, zi = 0.0;
    int i = 0;
    while (i < max_iter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            double log_zn = std::log(zr2 + zi2) * 0.5;
            double nu     = std::log(log_zn / std::log(2.0)) / std::log(2.0);
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr = std::abs(zr2 - zi2) + re;
        zi = 2.0 * zr * zi + im;
        zr = new_zr;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = |Re(z^2)| + i Im(z^2) + c, z_0 = pixel  (Celtic Julia)
inline double celtic_julia_iter(double re, double im, double cr, double ci, int max_iter)
{
    double zr = re, zi = im;
    int i = 0;
    while (i < max_iter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            double log_zn = std::log(zr2 + zi2) * 0.5;
            double nu     = std::log(log_zn / std::log(2.0)) / std::log(2.0);
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr = std::abs(zr2 - zi2) + cr;
        zi = 2.0 * zr * zi + ci;
        zr = new_zr;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = |Re(z^2)| + i|Im(z^2)| + c  (Buffalo)
inline double buffalo_iter(double re, double im, int max_iter)
{
    double zr = 0.0, zi = 0.0;
    int i = 0;
    while (i < max_iter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            double log_zn = std::log(zr2 + zi2) * 0.5;
            double nu     = std::log(log_zn / std::log(2.0)) / std::log(2.0);
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr = std::abs(zr2 - zi2) + re;
        zi = std::abs(2.0 * zr * zi) + im;
        zr = new_zr;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = |Re(z^2)| + i|Im(z^2)| + c, z_0 = pixel  (Buffalo Julia)
inline double buffalo_julia_iter(double re, double im, double cr, double ci, int max_iter)
{
    double zr = re, zi = im;
    int i = 0;
    while (i < max_iter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            double log_zn = std::log(zr2 + zi2) * 0.5;
            double nu     = std::log(log_zn / std::log(2.0)) / std::log(2.0);
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double new_zr = std::abs(zr2 - zi2) + cr;
        zi = std::abs(2.0 * zr * zi) + ci;
        zr = new_zr;
        ++i;
    }
    return static_cast<double>(max_iter);
}

// z_{n+1} = conj(z)^n + c, z_0 = pixel  (Mandelbar Julia, integer exp >= 3)
inline double mandelbar_multi_julia_iter(double re, double im, double cr, double ci,
                                          int max_iter, int n)
{
    double zr = re, zi = im;
    const double log_n = std::log(static_cast<double>(n));
    int i = 0;
    while (i < max_iter) {
        const double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            const double log_zn = std::log(zr2 + zi2) * 0.5;
            const double nu     = std::log(log_zn / log_n) / log_n;
            return std::max(0.0, static_cast<double>(i) + 1.0 - nu);
        }
        double pr = zr, pi = zi;
        for (int k = 1; k < n; ++k) {
            const double new_pr = pr * zr - pi * zi;
            pi = pr * zi + pi * zr;
            pr = new_pr;
        }
        zr =  pr + cr;
        zi = -pi + ci;  // conjugate: negate imaginary part of z^n
        ++i;
    }
    return static_cast<double>(max_iter);
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
