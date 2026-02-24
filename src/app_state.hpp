#pragma once

#include <SDL2/SDL_opengl.h>
#include "imgui.h"
#include "view_state.hpp"
#include "renderer.hpp"
#include "cpu_renderer.hpp"

#include <string>

// ---------------------------------------------------------------------------
// GL texture helper
// ---------------------------------------------------------------------------
struct GlTex {
    GLuint id = 0;
    int    w  = 0;
    int    h  = 0;

    void ensure(int nw, int nh) {
        if (nw == w && nh == h && id != 0) return;
        if (id) glDeleteTextures(1, &id);
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, nw, nh, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        w = nw; h = nh;
    }

    void upload(const PixelBuffer& buf) {
        glBindTexture(GL_TEXTURE_2D, id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, buf.width, buf.height,
                        GL_RGBA, GL_UNSIGNED_BYTE, buf.pixels.data());
    }

    ImTextureID imgui_id() const {
        return reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(id));
    }

    ~GlTex() { if (id) glDeleteTextures(1, &id); }
};

// ---------------------------------------------------------------------------
// All mutable application state
// ---------------------------------------------------------------------------
struct AppState {
    ViewState   vs;
    CpuRenderer renderer;
    PixelBuffer pbuf;
    bool        dirty          = true;
    double      main_render_ms = 0.0;

    // Dialog flags
    bool        show_about     = false;
    bool        show_benchmark = false;
    bool        show_export    = false;

    // Export dialog state
    int         exp_scale    = 1;      // 0=1x, 1=2x, 2=4x, 3=custom
    int         exp_custom_w = 3840;
    int         exp_custom_h = 2160;
    int         exp_fmt      = 0;      // 0=PNG, 1=JXL
    bool        exp_done     = false;
    std::string exp_msg;
    std::string exp_saved_name;
    int         last_irw     = 0;
    int         last_irh     = 0;

    // Thread count selector (0 = Auto)
    int thread_sel = 0;

    // Orbit visualization
    bool   show_orbit    = false;
    bool   orbit_active  = false;
    double orbit_re      = 0.0;
    double orbit_im      = 0.0;

    // Mini Mandelbrot map
    PixelBuffer mini_pbuf;
    bool        mini_dirty    = true;
    bool        mini_dragging = false;
    bool        mini_panning  = false;
    ImVec2      mini_pan_start_mouse = {};
    double      mini_pan_start_cx = 0.0;
    double      mini_pan_start_cy = 0.0;
    double      mini_cx       = 0.0;
    double      mini_cy       = 0.0;
    double      mini_vw       = 4.0;

    // Navigation
    bool      panning         = false;
    ImVec2    pan_start_mouse = {};
    ViewState pan_start_vs    = {};

    bool   zoom_boxing = false;
    ImVec2 zbox_start  = {};
    ImVec2 zbox_end    = {};

    // GL textures
    GlTex render_tex;
    GlTex mini_tex;
};
