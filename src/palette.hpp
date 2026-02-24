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

// Map a Lyapunov exponent to a 32-bit RGBA pixel.
static constexpr double LYAP_SCALE = 200.0;

inline uint32_t lyapunov_color(double lambda, int palette, int pal_offset)
{
    int idx = (static_cast<int>(lambda * LYAP_SCALE) + pal_offset) % LUT_SIZE;
    if (idx < 0) idx += LUT_SIZE;
    return g_palette_lut[palette][idx];
}
