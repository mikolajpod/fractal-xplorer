#pragma once

#include <vector>
#include <cstdint>

struct ViewState;

// Pixel buffer: RGBA, little-endian packed as 0xAABBGGRR
struct PixelBuffer {
    std::vector<uint32_t> pixels;
    int width  = 0;
    int height = 0;

    void resize(int w, int h)
    {
        width  = w;
        height = h;
        pixels.assign(static_cast<size_t>(w * h), 0xFF000000u);
    }
};

class IFractalRenderer {
public:
    virtual ~IFractalRenderer() = default;
    virtual void render(const ViewState& state, PixelBuffer& buf) = 0;
};
