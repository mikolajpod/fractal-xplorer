#include "palette.hpp"

#include <algorithm>
#include <cmath>

const char* g_palette_names[PALETTE_COUNT] = {
    "Grayscale",
    "Fire",
    "Ice",
    "Electric",
    "Sunset",
    "Forest",
    "Zebra",
    "Classic Ultra",
};

uint32_t g_palette_lut[PALETTE_COUNT][LUT_SIZE];

// ---------------------------------------------------------------------------
// Color-stop interpolation helper
// ---------------------------------------------------------------------------
struct ColorStop { float t; uint8_t r, g, b; };

static void build_lut(int pal, const ColorStop* stops, int n)
{
    uint32_t* lut = g_palette_lut[pal];
    for (int i = 0; i < LUT_SIZE; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(LUT_SIZE - 1);

        // Find the segment [stops[seg], stops[seg+1]] that contains t.
        int seg = n - 2;
        for (int s = 0; s < n - 1; ++s) {
            if (t <= stops[s + 1].t) { seg = s; break; }
        }
        const ColorStop& a = stops[seg];
        const ColorStop& b = stops[seg + 1];
        const float span = b.t - a.t;
        const float f    = (span > 0.0f) ? (t - a.t) / span : 0.0f;
        const float cf   = std::max(0.0f, std::min(1.0f, f));

        const uint8_t r  = static_cast<uint8_t>(a.r + cf * (static_cast<float>(b.r) - a.r));
        const uint8_t g  = static_cast<uint8_t>(a.g + cf * (static_cast<float>(b.g) - a.g));
        const uint8_t bv = static_cast<uint8_t>(a.b + cf * (static_cast<float>(b.b) - a.b));
        lut[i] = 0xFF000000u
               | (static_cast<uint32_t>(bv) << 16)
               | (static_cast<uint32_t>(g)  <<  8)
               |  static_cast<uint32_t>(r);
    }
}

// ---------------------------------------------------------------------------
// Palette definitions
// ---------------------------------------------------------------------------
void init_palettes()
{
    // 0: Grayscale
    {
        static const ColorStop s[] = {
            {0.0f,   0,   0,   0},
            {1.0f, 255, 255, 255},
        };
        build_lut(0, s, 2);
    }

    // 1: Fire  (black → dark-red → red → orange → yellow → white)
    {
        static const ColorStop s[] = {
            {0.000f,   0,   0,   0},
            {0.250f, 128,   0,   0},
            {0.500f, 255,   0,   0},
            {0.750f, 255, 128,   0},
            {0.875f, 255, 255,   0},
            {1.000f, 255, 255, 255},
        };
        build_lut(1, s, 6);
    }

    // 2: Ice  (black → dark-blue → blue → cyan → white)
    {
        static const ColorStop s[] = {
            {0.000f,   0,   0,   0},
            {0.250f,   0,   0, 128},
            {0.500f,   0,  64, 255},
            {0.750f,   0, 200, 255},
            {1.000f, 255, 255, 255},
        };
        build_lut(2, s, 5);
    }

    // 3: Electric  (black → dark-purple → blue → cyan → white)
    {
        static const ColorStop s[] = {
            {0.000f,   0,   0,   0},
            {0.250f,  64,   0, 128},
            {0.500f,   0,  64, 255},
            {0.750f,   0, 200, 255},
            {1.000f, 255, 255, 255},
        };
        build_lut(3, s, 5);
    }

    // 4: Sunset  (black → deep-red → orange → yellow → pale-yellow)
    {
        static const ColorStop s[] = {
            {0.000f,   0,   0,   0},
            {0.300f, 128,   0,  32},
            {0.550f, 255,  64,   0},
            {0.800f, 255, 200,   0},
            {1.000f, 255, 255, 180},
        };
        build_lut(4, s, 5);
    }

    // 5: Forest  (black → dark-green → green → lime → pale-green)
    {
        static const ColorStop s[] = {
            {0.000f,   0,   0,   0},
            {0.250f,   0,  64,   0},
            {0.500f,   0, 160,   0},
            {0.750f, 100, 220,   0},
            {1.000f, 200, 255, 180},
        };
        build_lut(5, s, 5);
    }

    // 6: Zebra  — 8 alternating black/white bands
    {
        const int band = LUT_SIZE / 8;
        for (int i = 0; i < LUT_SIZE; ++i)
            g_palette_lut[6][i] = ((i / band) % 2 == 0) ? 0xFF000000u : 0xFFFFFFFFu;
    }

    // 7: Classic Ultra  (blue-gold gradient, UltraFractal-inspired)
    {
        static const ColorStop s[] = {
            {0.0000f,   0,   7, 100},
            {0.1600f,  32, 107, 203},
            {0.4200f, 237, 255, 255},
            {0.6425f, 255, 170,   0},
            {0.8575f,   0,   2,   0},
            {1.0000f,   0,   7, 100},
        };
        build_lut(7, s, 6);
    }
}
