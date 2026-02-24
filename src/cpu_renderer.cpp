#include "cpu_renderer.hpp"
#include "fractal.hpp"
#include "fractal_avx.hpp"
#include "palette.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

// -----------------------------------------------------------------------
// Constructor — detect AVX2, build thread pool
// -----------------------------------------------------------------------
CpuRenderer::CpuRenderer()
{
    use_avx2   = __builtin_cpu_supports("avx2");
    avx2_active = use_avx2;

    int n = static_cast<int>(std::thread::hardware_concurrency());
    if (n < 1) n = 4;
    hw_concurrency = n;
    thread_count   = n;
    pool = std::make_unique<ThreadPool>(n);
}

void CpuRenderer::set_thread_count(int n)
{
    if (n < 1) n = hw_concurrency;
    pool = std::make_unique<ThreadPool>(n);
    thread_count = n;
}

// -----------------------------------------------------------------------
// Tile renderer — called from thread pool workers
// -----------------------------------------------------------------------
void CpuRenderer::render_tile(const ViewState& vs, PixelBuffer& buf,
                               int tx, int ty, int tw, int th)
{
    const int    W     = buf.width;
    const int    H     = buf.height;
    const double scale = vs.view_width / W;
    const double x0    = vs.center_x - W * 0.5 * scale;
    const double y0    = vs.center_y - H * 0.5 * scale;

    // For MultiSlow: if float exponent is effectively an integer, promote to
    // the fast integer path (AVX2 repeated-multiply, no trig).
    const int slow_int_n = [&]() -> int {
        if (vs.formula != FormulaType::MultiSlow)
            return 0;
        const int n = static_cast<int>(std::round(vs.multibrot_exp_f));
        return (n >= 2 && std::abs(vs.multibrot_exp_f - n) < 1e-9) ? n : 0;
    }();

    for (int py = ty; py < ty + th && py < H; ++py) {
        const double im  = y0 + py * scale;
        uint32_t*    row = buf.pixels.data() + py * W;
        int          px  = tx;
        const int    end = std::min(tx + tw, W);

        // AVX2 path: 4 pixels per iteration.
        const bool use_avx2_here = use_avx2;

        if (use_avx2_here) {
            for (; px + 4 <= end; px += 4) {
                const double re0 = x0 + px * scale;

                if (vs.color_mode == COLOR_SMOOTH) {
                    double smooth4[4];
                    switch (vs.formula) {
                        case FormulaType::Standard:
                            if (vs.julia_mode)
                                avx2_julia_4(re0, scale, im, vs.max_iter,
                                             vs.julia_re, vs.julia_im, smooth4);
                            else
                                avx2_mandelbrot_4(re0, scale, im, vs.max_iter, smooth4);
                            break;
                        case FormulaType::BurningShip:
                            if (vs.julia_mode)
                                avx2_burning_ship_julia_4(re0, scale, im, vs.max_iter,
                                                          vs.julia_re, vs.julia_im, smooth4);
                            else
                                avx2_burning_ship_4(re0, scale, im, vs.max_iter, smooth4);
                            break;
                        case FormulaType::Mandelbar:
                            if (vs.julia_mode) {
                                if (vs.multibrot_exp == 2)
                                    avx2_mandelbar_julia_4(re0, scale, im, vs.max_iter,
                                                           vs.julia_re, vs.julia_im, smooth4);
                                else
                                    avx2_mandelbar_multi_julia_4(re0, scale, im, vs.max_iter,
                                                                  vs.multibrot_exp,
                                                                  vs.julia_re, vs.julia_im, smooth4);
                            } else {
                                if (vs.multibrot_exp == 2)
                                    avx2_mandelbar_4(re0, scale, im, vs.max_iter, smooth4);
                                else
                                    avx2_mandelbar_multi_4(re0, scale, im, vs.max_iter,
                                                           vs.multibrot_exp, smooth4);
                            }
                            break;
                        case FormulaType::MultiFast:
                            if (vs.julia_mode) {
                                if (vs.multibrot_exp == 2)
                                    avx2_julia_4(re0, scale, im, vs.max_iter,
                                                 vs.julia_re, vs.julia_im, smooth4);
                                else
                                    avx2_multijulia_4(re0, scale, im, vs.max_iter,
                                                      vs.multibrot_exp,
                                                      vs.julia_re, vs.julia_im, smooth4);
                            } else {
                                if (vs.multibrot_exp == 2)
                                    avx2_mandelbrot_4(re0, scale, im, vs.max_iter, smooth4);
                                else
                                    avx2_multibrot_4(re0, scale, im, vs.max_iter,
                                                     vs.multibrot_exp, smooth4);
                            }
                            break;
                        case FormulaType::MultiSlow:
                            if (slow_int_n > 0) {
                                if (vs.julia_mode) {
                                    if (slow_int_n == 2)
                                        avx2_julia_4(re0, scale, im, vs.max_iter,
                                                     vs.julia_re, vs.julia_im, smooth4);
                                    else
                                        avx2_multijulia_4(re0, scale, im, vs.max_iter,
                                                          slow_int_n,
                                                          vs.julia_re, vs.julia_im, smooth4);
                                } else {
                                    if (slow_int_n == 2)
                                        avx2_mandelbrot_4(re0, scale, im, vs.max_iter, smooth4);
                                    else
                                        avx2_multibrot_4(re0, scale, im, vs.max_iter,
                                                         slow_int_n, smooth4);
                                }
                            } else {
                                if (vs.julia_mode)
                                    avx2_multijulia_slow_4(re0, scale, im, vs.max_iter,
                                                            vs.multibrot_exp_f,
                                                            vs.julia_re, vs.julia_im, smooth4);
                                else
                                    avx2_multibrot_slow_4(re0, scale, im, vs.max_iter,
                                                           vs.multibrot_exp_f, smooth4);
                            }
                            break;
                        case FormulaType::Celtic:
                            if (vs.julia_mode)
                                avx2_celtic_julia_4(re0, scale, im, vs.max_iter,
                                                    vs.julia_re, vs.julia_im, smooth4);
                            else
                                avx2_celtic_4(re0, scale, im, vs.max_iter, smooth4);
                            break;
                        case FormulaType::Buffalo:
                            if (vs.julia_mode)
                                avx2_buffalo_julia_4(re0, scale, im, vs.max_iter,
                                                     vs.julia_re, vs.julia_im, smooth4);
                            else
                                avx2_buffalo_4(re0, scale, im, vs.max_iter, smooth4);
                            break;
                        default:
                            avx2_mandelbrot_4(re0, scale, im, vs.max_iter, smooth4);
                            break;
                    }
                    for (int k = 0; k < 4; ++k)
                        row[px + k] = palette_color(smooth4[k], vs.max_iter,
                                                    vs.palette, vs.pal_offset);
                } else {
                    // Lyapunov mode: compute both smooth and lambda
                    double smooth4[4], lyap4[4];
                    avx2_lyapunov_4(vs.formula, vs.julia_mode, re0, scale, im,
                                     vs.max_iter, vs.multibrot_exp, vs.multibrot_exp_f,
                                     vs.julia_re, vs.julia_im, smooth4, lyap4);
                    for (int k = 0; k < 4; ++k) {
                        if (vs.color_mode == COLOR_LYAPUNOV_FULL)
                            row[px + k] = lyapunov_color(lyap4[k], vs.palette, vs.pal_offset);
                        else  // COLOR_LYAPUNOV_INTERIOR
                            row[px + k] = (smooth4[k] >= static_cast<double>(vs.max_iter))
                                ? lyapunov_color(lyap4[k], vs.palette, vs.pal_offset)
                                : palette_color(smooth4[k], vs.max_iter, vs.palette, vs.pal_offset);
                    }
                }
            }
        }

        // Scalar path: remainder pixels (or full row if no AVX2)
        for (; px < end; ++px) {
            const double re = x0 + px * scale;
            double smooth;
            switch (vs.formula) {
                case FormulaType::Standard:
                    smooth = vs.julia_mode
                        ? julia_iter(re, im, vs.julia_re, vs.julia_im, vs.max_iter)
                        : mandelbrot_iter(re, im, vs.max_iter);
                    break;
                case FormulaType::BurningShip:
                    smooth = vs.julia_mode
                        ? burning_ship_julia_iter(re, im, vs.julia_re, vs.julia_im, vs.max_iter)
                        : burning_ship_iter(re, im, vs.max_iter);
                    break;
                case FormulaType::Mandelbar:
                    if (vs.julia_mode)
                        smooth = (vs.multibrot_exp == 2)
                            ? mandelbar_julia_iter(re, im, vs.julia_re, vs.julia_im, vs.max_iter)
                            : mandelbar_multi_julia_iter(re, im, vs.julia_re, vs.julia_im,
                                                         vs.max_iter, vs.multibrot_exp);
                    else
                        smooth = (vs.multibrot_exp == 2)
                            ? mandelbar_iter(re, im, vs.max_iter)
                            : mandelbar_multi_iter(re, im, vs.max_iter, vs.multibrot_exp);
                    break;
                case FormulaType::MultiFast:
                    if (vs.julia_mode)
                        smooth = (vs.multibrot_exp == 2)
                            ? julia_iter(re, im, vs.julia_re, vs.julia_im, vs.max_iter)
                            : multijulia_iter(re, im, vs.julia_re, vs.julia_im,
                                              vs.max_iter, vs.multibrot_exp);
                    else
                        smooth = (vs.multibrot_exp == 2)
                            ? mandelbrot_iter(re, im, vs.max_iter)
                            : multibrot_iter(re, im, vs.max_iter, vs.multibrot_exp);
                    break;
                case FormulaType::MultiSlow:
                    if (slow_int_n > 0) {
                        if (vs.julia_mode)
                            smooth = (slow_int_n == 2)
                                ? julia_iter(re, im, vs.julia_re, vs.julia_im, vs.max_iter)
                                : multijulia_iter(re, im, vs.julia_re, vs.julia_im,
                                                  vs.max_iter, slow_int_n);
                        else
                            smooth = (slow_int_n == 2)
                                ? mandelbrot_iter(re, im, vs.max_iter)
                                : multibrot_iter(re, im, vs.max_iter, slow_int_n);
                    } else {
                        smooth = vs.julia_mode
                            ? multijulia_slow_iter(re, im, vs.julia_re, vs.julia_im,
                                                   vs.max_iter, vs.multibrot_exp_f)
                            : multibrot_slow_iter(re, im, vs.max_iter, vs.multibrot_exp_f);
                    }
                    break;
                case FormulaType::Celtic:
                    smooth = vs.julia_mode
                        ? celtic_julia_iter(re, im, vs.julia_re, vs.julia_im, vs.max_iter)
                        : celtic_iter(re, im, vs.max_iter);
                    break;
                case FormulaType::Buffalo:
                    smooth = vs.julia_mode
                        ? buffalo_julia_iter(re, im, vs.julia_re, vs.julia_im, vs.max_iter)
                        : buffalo_iter(re, im, vs.max_iter);
                    break;
                default:
                    smooth = mandelbrot_iter(re, im, vs.max_iter);
                    break;
            }
            row[px] = palette_color(smooth, vs.max_iter,
                                    vs.palette, vs.pal_offset);
        }
    }
}

// -----------------------------------------------------------------------
// Top-level render — splits image into tiles and dispatches to thread pool
// -----------------------------------------------------------------------
void CpuRenderer::render(const ViewState& vs, PixelBuffer& buf)
{
    avx2_active = use_avx2;

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();

    const int W = buf.width, H = buf.height;
    if (W <= 0 || H <= 0) return;

    constexpr int TILE_W = 64;
    constexpr int TILE_H = 64;

    for (int ty = 0; ty < H; ty += TILE_H) {
        for (int tx = 0; tx < W; tx += TILE_W) {
            const int tw = std::min(TILE_W, W - tx);
            const int th = std::min(TILE_H, H - ty);
            pool->submit([this, vs, &buf, tx, ty, tw, th] {
                render_tile(vs, buf, tx, ty, tw, th);
            });
        }
    }
    pool->wait();

    last_render_ms = std::chrono::duration<double, std::milli>(
                         clock::now() - t0).count();
}
