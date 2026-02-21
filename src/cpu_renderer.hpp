#pragma once

#include "renderer.hpp"
#include "view_state.hpp"
#include "thread_pool.hpp"

#include <memory>

class CpuRenderer : public IFractalRenderer {
public:
    CpuRenderer();
    void render(const ViewState& state, PixelBuffer& buf) override;

    double last_render_ms = 0.0;
    bool   avx2_active    = false;   // true if AVX2 path is in use
    int    thread_count   = 0;

private:
    void render_tile(const ViewState& vs, PixelBuffer& buf,
                     int tx, int ty, int tw, int th);

    std::unique_ptr<ThreadPool> pool;
    bool use_avx2 = false;
};
