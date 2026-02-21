#pragma once

#include <cmath>

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
