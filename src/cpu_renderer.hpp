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
    bool   avx_active     = false;   // true if AVX path is in use
    int    thread_count   = 0;
    int    hw_concurrency = 0;       // logical CPU count detected at startup

    // n=0 restores hw_concurrency
    void set_thread_count(int n);

    // Override AVX flag (e.g. for benchmarking scalar path)
    void set_avx(bool b) { use_avx = b; avx_active = b; }

private:
    void render_tile(const ViewState& vs, PixelBuffer& buf,
                     int tx, int ty, int tw, int th);

    std::unique_ptr<ThreadPool> pool;
    bool use_avx = false;
};
