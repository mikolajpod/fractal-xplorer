#pragma once

#include <cstdint>

static constexpr int PALETTE_COUNT = 8;
static constexpr int LUT_SIZE      = 1024;

extern const char* g_palette_names[PALETTE_COUNT];
extern uint32_t    g_palette_lut[PALETTE_COUNT][LUT_SIZE];

// Must be called once at startup before any rendering.
void init_palettes();

// Map a smooth escape-time value to a 32-bit RGBA pixel.
// palette : 0-7
// pal_offset : 0 – LUT_SIZE-1  (shifts which color lands at smooth=0)
inline uint32_t palette_color(double smooth, int max_iter, int palette, int pal_offset)
{
    if (smooth >= static_cast<double>(max_iter))
        return 0xFF000000u;   // interior: black

    // One full palette cycle every 25.6 smooth units (matches old sine period).
    int idx = (static_cast<int>(smooth * 40.0) + pal_offset) % LUT_SIZE;
    if (idx < 0) idx += LUT_SIZE;
    return g_palette_lut[palette][idx];
}

// Newton fractal root colors — 8 distinct hues, fully opaque.
// Layout: 0xAABBGGRR (little-endian memory: R, G, B, A)
static constexpr uint32_t NEWTON_ROOT_COLORS[8] = {
    0xFF0000FF,  // red
    0xFF00CC00,  // green
    0xFFFF6600,  // blue
    0xFF00FFFF,  // yellow
    0xFFFF00FF,  // magenta
    0xFFFFFF00,  // cyan
    0xFF0088FF,  // orange
    0xFF00FF88,  // lime
};

// Map a Newton result to a 32-bit RGBA pixel.
// root < 0 -> black (no convergence). Otherwise: base color dimmed by iteration count.
inline uint32_t newton_color(int root, int iters, int max_iter)
{
    if (root < 0) return 0xFF000000u;
    const uint32_t base = NEWTON_ROOT_COLORS[root & 7];
    const double brightness = 1.0 - 0.6 * (static_cast<double>(iters) / max_iter);
    const uint8_t r = static_cast<uint8_t>((base & 0xFF)       * brightness);
    const uint8_t g = static_cast<uint8_t>(((base >> 8) & 0xFF) * brightness);
    const uint8_t b = static_cast<uint8_t>(((base >> 16) & 0xFF) * brightness);
    return 0xFF000000u | (static_cast<uint32_t>(b) << 16)
                       | (static_cast<uint32_t>(g) << 8)
                       | static_cast<uint32_t>(r);
}

// Map a Lyapunov exponent to a 32-bit RGBA pixel.
static constexpr double LYAP_SCALE = 200.0;

inline uint32_t lyapunov_color(double lambda, int palette, int pal_offset)
{
    int idx = (static_cast<int>(lambda * LYAP_SCALE) + pal_offset) % LUT_SIZE;
    if (idx < 0) idx += LUT_SIZE;
    return g_palette_lut[palette][idx];
}
