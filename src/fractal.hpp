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
