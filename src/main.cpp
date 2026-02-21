#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include "view_state.hpp"
#include "renderer.hpp"
#include "cpu_renderer.hpp"
#include "palette.hpp"
#include "export.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

static const float PANEL_WIDTH   = 280.0f;
static const float STATUS_HEIGHT = 24.0f;

// ---------------------------------------------------------------------------
// GL texture helpers — one for the main render, one for the mini map
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

static GlTex g_render_tex;
static GlTex g_mini_tex;

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Fractal Xplorer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        fprintf(stderr, "SDL_GL_CreateContext error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style      = ImGui::GetStyle();
    style.WindowBorderSize = 0.0f;
    style.WindowPadding    = ImVec2(8.0f, 6.0f);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    init_palettes();

    // -----------------------------------------------------------------------
    // App state
    // -----------------------------------------------------------------------
    ViewState   vs;
    CpuRenderer renderer;
    PixelBuffer pbuf;
    bool        dirty      = true;
    bool        show_about = false;

    // Export dialog
    bool        show_export  = false;
    int         exp_scale    = 1;      // 0=1x, 1=2x, 2=4x, 3=custom
    int         exp_custom_w = 3840;
    int         exp_custom_h = 2160;
    int         exp_fmt      = 0;      // 0=PNG, 1=JXL
    bool        exp_done     = false;  // true after an export attempt
    std::string exp_msg;               // error string (empty = success)
    std::string exp_saved_name;        // actual filename written on success
    int         last_irw     = 0;
    int         last_irh     = 0;

    // Mini Mandelbrot map
    PixelBuffer mini_pbuf;
    bool        mini_dirty    = true;   // render once on startup
    bool        mini_dragging = false;

    // Navigation
    bool      panning         = false;
    ImVec2    pan_start_mouse = {};
    ViewState pan_start_vs    = {};

    bool   zoom_boxing = false;
    ImVec2 zbox_start  = {};
    ImVec2 zbox_end    = {};

    auto update_title = [&]() {
        char tbuf[128];
        std::snprintf(tbuf, sizeof(tbuf), "Fractal Xplorer  —  %s  [zoom: %.2fx]",
                      fractal_name(vs), zoom_display(vs));
        SDL_SetWindowTitle(window, tbuf);
    };
    update_title();

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        const float fw       = static_cast<float>(win_w);
        const float fh       = static_cast<float>(win_h);
        const float menu_h   = ImGui::GetFrameHeight();
        const float render_x = PANEL_WIDTH;
        const float render_y = menu_h;
        const float render_w = fw - PANEL_WIDTH;
        const float render_h = fh - menu_h - STATUS_HEIGHT;
        const int   irw      = static_cast<int>(render_w);
        const int   irh      = static_cast<int>(render_h);

        last_irw = irw;
        last_irh = irh;

        // Main fractal render
        if (dirty || irw != pbuf.width || irh != pbuf.height) {
            if (irw > 0 && irh > 0) {
                pbuf.resize(irw, irh);
                renderer.render(vs, pbuf);
                g_render_tex.ensure(irw, irh);
                g_render_tex.upload(pbuf);
                update_title();
            }
            dirty = false;
        }

        // -------------------------------------------------------------------
        // Menu bar
        // -------------------------------------------------------------------
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Export Image", "Ctrl+S")) {
                    show_export = true;
                    exp_done    = false;
                    exp_msg.clear();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) running = false;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Reset View", "R")) {
                    FractalType ft  = vs.fractal;
                    double jre = vs.julia_re, jim = vs.julia_im;
                    int    pal = vs.palette,  poff = vs.pal_offset;
                    vs = ViewState{};
                    vs.fractal    = ft;
                    vs.julia_re   = jre;
                    vs.julia_im   = jim;
                    vs.palette    = pal;
                    vs.pal_offset = poff;
                    dirty = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About", "F1")) show_about = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // -------------------------------------------------------------------
        // Global keyboard shortcuts
        // -------------------------------------------------------------------
        if (ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl) {
            show_export = true;
            exp_done    = false;
            exp_msg.clear();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) {
            FractalType ft  = vs.fractal;
            double jre = vs.julia_re, jim = vs.julia_im;
            int    pal = vs.palette,  poff = vs.pal_offset;
            vs = ViewState{};
            vs.fractal    = ft;
            vs.julia_re   = jre;
            vs.julia_im   = jim;
            vs.palette    = pal;
            vs.pal_offset = poff;
            dirty = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F1))
            show_about = true;
        if (ImGui::IsKeyPressed(ImGuiKey_Equal) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
            vs.view_width /= 1.5;  dirty = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Minus) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
            vs.view_width *= 1.5;  dirty = true;
        }

        // -------------------------------------------------------------------
        // Side panel
        // -------------------------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(0.0f, menu_h));
        ImGui::SetNextWindowSize(ImVec2(PANEL_WIDTH, fh - menu_h - STATUS_HEIGHT));
        ImGui::Begin("##panel", nullptr,
            ImGuiWindowFlags_NoTitleBar            |
            ImGuiWindowFlags_NoResize              |
            ImGuiWindowFlags_NoMove                |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        // --- Fractal selector ---
        ImGui::TextDisabled("FRACTAL");
        ImGui::Separator();
        {
            static const char* names[] = { "Mandelbrot", "Julia", "Burning Ship" };
            int ft = static_cast<int>(vs.fractal);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##fractal", &ft, names, 3)) {
                vs.fractal = static_cast<FractalType>(ft);
                dirty = true;
            }
        }

        // --- Iteration count ---
        ImGui::Spacing();
        ImGui::TextDisabled("ITERATIONS");
        ImGui::Separator();
        {
            int iter = vs.max_iter;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderInt("##iter", &iter, 64, 8192, "%d",
                                 ImGuiSliderFlags_Logarithmic)) {
                vs.max_iter = iter;
                dirty = true;
            }
        }

        // --- Palette ---
        ImGui::Spacing();
        ImGui::TextDisabled("PALETTE");
        ImGui::Separator();
        {
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##palette", &vs.palette, g_palette_names, PALETTE_COUNT))
                dirty = true;
            ImGui::Spacing();
            ImGui::Text("Offset");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderInt("##paloff", &vs.pal_offset, 0, LUT_SIZE - 1))
                dirty = true;
        }

        // --- Julia parameter + mini map ---
        ImGui::Spacing();
        ImGui::TextDisabled("JULIA PARAMETER");
        ImGui::Separator();

        // Mini map dimensions: fill panel width, maintain Mandelbrot aspect
        const float map_w     = ImGui::GetContentRegionAvail().x;
        const float map_h     = map_w * (2.5f / 3.5f);
        const int   map_iw    = static_cast<int>(map_w);
        const int   map_ih    = static_cast<int>(map_h);
        // Complex units per display pixel in the mini map
        const float map_scale = 3.5f / map_w;

        // Render mini map once (fixed standard Mandelbrot view)
        if (mini_dirty && map_iw > 0 && map_ih > 0) {
            ViewState mini_vs;          // default: Mandelbrot, center (-0.5,0)
            mini_vs.max_iter = 128;
            mini_pbuf.resize(map_iw, map_ih);
            renderer.render(mini_vs, mini_pbuf);
            g_mini_tex.ensure(map_iw, map_ih);
            g_mini_tex.upload(mini_pbuf);
            mini_dirty = false;
        }

        if (g_mini_tex.id) {
            const ImVec2 map_tl = ImGui::GetCursorScreenPos();

            // Draw mini map
            ImGui::Image(g_mini_tex.imgui_id(), ImVec2(map_w, map_h));
            const bool map_hovered = ImGui::IsItemHovered();

            // Draw c-parameter indicator (bullseye)
            const float dot_x = map_tl.x
                + (static_cast<float>(vs.julia_re) + 0.5f) / map_scale
                + map_w * 0.5f;
            const float dot_y = map_tl.y
                + static_cast<float>(vs.julia_im) / map_scale
                + map_h * 0.5f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddCircleFilled(ImVec2(dot_x, dot_y), 3.5f,
                                IM_COL32(255,  50,  50, 230));
            dl->AddCircle      (ImVec2(dot_x, dot_y), 5.5f,
                                IM_COL32(255, 220,  50, 255), 0, 1.5f);

            // Start drag on click inside mini map
            if (map_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                mini_dragging = true;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                mini_dragging = false;

            if (mini_dragging) {
                const float mx = io.MousePos.x - map_tl.x;
                const float my = io.MousePos.y - map_tl.y;
                vs.julia_re = static_cast<double>(-0.5f + (mx - map_w * 0.5f) * map_scale);
                vs.julia_im = static_cast<double>((my - map_h * 0.5f) * map_scale);
                // Auto-switch to Julia when user picks a point
                if (vs.fractal != FractalType::Julia) {
                    vs.fractal = FractalType::Julia;
                }
                dirty = true;
            }
        }

        // re / im numeric inputs
        ImGui::Spacing();
        {
            double re = vs.julia_re;
            double im = vs.julia_im;
            ImGui::Text("re:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputDouble("##jre", &re, 0.001, 0.01, "%.8f"))
                { vs.julia_re = re; dirty = true; }
            ImGui::Text("im:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputDouble("##jim", &im, 0.001, 0.01, "%.8f"))
                { vs.julia_im = im; dirty = true; }
        }

        ImGui::End();  // ##panel

        // -------------------------------------------------------------------
        // Render area
        // -------------------------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(render_x, render_y));
        ImGui::SetNextWindowSize(ImVec2(render_w, render_h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##render", nullptr,
            ImGuiWindowFlags_NoTitleBar            |
            ImGuiWindowFlags_NoResize              |
            ImGuiWindowFlags_NoMove                |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();

        if (g_render_tex.id)
            ImGui::Image(g_render_tex.imgui_id(),
                         ImVec2(static_cast<float>(g_render_tex.w),
                                static_cast<float>(g_render_tex.h)));

        const bool render_hovered = ImGui::IsWindowHovered();

        // Mouse wheel zoom (centered on cursor)
        if (render_hovered && io.MouseWheel != 0.0f) {
            const double mx     = io.MousePos.x - render_x;
            const double my     = io.MousePos.y - render_y;
            const double scale  = vs.view_width / irw;
            const double cur_re = vs.center_x + (mx - irw * 0.5) * scale;
            const double cur_im = vs.center_y + (my - irh * 0.5) * scale;
            const double factor = (io.MouseWheel > 0.0f) ? 1.25 : (1.0 / 1.25);
            vs.view_width      /= factor;
            const double ns     = vs.view_width / irw;
            vs.center_x = cur_re - (mx - irw * 0.5) * ns;
            vs.center_y = cur_im - (my - irh * 0.5) * ns;
            dirty = true;
        }

        // Left-click drag: pan
        if (render_hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !zoom_boxing) {
            panning         = true;
            pan_start_mouse = io.MousePos;
            pan_start_vs    = vs;
        }
        if (panning) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                const double scale = pan_start_vs.view_width / irw;
                vs.center_x   = pan_start_vs.center_x
                                 - (io.MousePos.x - pan_start_mouse.x) * scale;
                vs.center_y   = pan_start_vs.center_y
                                 - (io.MousePos.y - pan_start_mouse.y) * scale;
                vs.view_width = pan_start_vs.view_width;
                dirty = true;
            } else {
                panning = false;
            }
        }

        // Right-click drag: zoom box
        if (render_hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !panning) {
            zoom_boxing = true;
            zbox_start  = io.MousePos;
            zbox_end    = io.MousePos;
        }
        if (zoom_boxing) {
            zbox_end = io.MousePos;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(zbox_start, zbox_end, IM_COL32(255, 255, 255, 20));
            dl->AddRect(zbox_start, zbox_end, IM_COL32(255, 255, 255, 200),
                        0.0f, 0, 1.5f);

            if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                const float x0 = std::min(zbox_start.x, zbox_end.x) - render_x;
                const float y0 = std::min(zbox_start.y, zbox_end.y) - render_y;
                const float x1 = std::max(zbox_start.x, zbox_end.x) - render_x;
                const float y1 = std::max(zbox_start.y, zbox_end.y) - render_y;
                const float bw = x1 - x0;
                const float bh = y1 - y0;
                if (bw > 4.0f && bh > 4.0f) {
                    const double scale = vs.view_width / irw;
                    vs.center_x   = vs.center_x + (x0 + bw * 0.5f - irw * 0.5) * scale;
                    vs.center_y   = vs.center_y + (y0 + bh * 0.5f - irh * 0.5) * scale;
                    vs.view_width = bw * scale;
                    dirty = true;
                }
                zoom_boxing = false;
            }
        }

        ImGui::End();  // ##render

        // -------------------------------------------------------------------
        // Status bar
        // -------------------------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(0.0f, fh - STATUS_HEIGHT));
        ImGui::SetNextWindowSize(ImVec2(fw, STATUS_HEIGHT));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
        ImGui::Begin("##status", nullptr,
            ImGuiWindowFlags_NoTitleBar            |
            ImGuiWindowFlags_NoResize              |
            ImGuiWindowFlags_NoMove                |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        ImGui::Text("x: %.8f   y: %.8f   zoom: %.4fx   iter: %d   %.0f ms  [%s  %dt]",
                    vs.center_x, vs.center_y, zoom_display(vs), vs.max_iter,
                    renderer.last_render_ms,
                    renderer.avx2_active ? "AVX2" : "scalar",
                    renderer.thread_count);
        ImGui::End();

        // -------------------------------------------------------------------
        // Export dialog
        // -------------------------------------------------------------------
        if (show_export) {
            ImGui::OpenPopup("Export Image##dlg");
            show_export = false;
        }
        if (ImGui::BeginPopupModal("Export Image##dlg", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            // Format selector
            ImGui::TextDisabled("FORMAT");
            ImGui::Separator();
            if (!jxl_available()) {
                ImGui::RadioButton("PNG", &exp_fmt, 0);
                ImGui::SameLine();
                ImGui::TextDisabled("JXL (not available)");
            } else {
                ImGui::RadioButton("PNG", &exp_fmt, 0);
                ImGui::SameLine();
                ImGui::RadioButton("JPEG XL (lossless)", &exp_fmt, 1);
            }

            // Resolution selector
            ImGui::Spacing();
            ImGui::TextDisabled("RESOLUTION");
            ImGui::Separator();
            {
                char buf1[64], buf2[64], buf4[64];
                std::snprintf(buf1, sizeof(buf1), "1x   %d x %d", last_irw,     last_irh    );
                std::snprintf(buf2, sizeof(buf2), "2x   %d x %d", last_irw * 2, last_irh * 2);
                std::snprintf(buf4, sizeof(buf4), "4x   %d x %d", last_irw * 4, last_irh * 4);
                ImGui::RadioButton(buf1, &exp_scale, 0);
                ImGui::RadioButton(buf2, &exp_scale, 1);
                ImGui::RadioButton(buf4, &exp_scale, 2);
                ImGui::RadioButton("Custom", &exp_scale, 3);
                if (exp_scale == 3) {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(80.0f);
                    ImGui::InputInt("##cw", &exp_custom_w, 0);
                    exp_custom_w = std::max(16, std::min(exp_custom_w, 7680));
                    ImGui::SameLine(); ImGui::TextUnformatted("x");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(80.0f);
                    ImGui::InputInt("##ch", &exp_custom_h, 0);
                    exp_custom_h = std::max(16, std::min(exp_custom_h, 4320));
                }
            }

            // Filename preview
            ImGui::Spacing();
            ImGui::TextDisabled("OUTPUT");
            ImGui::Separator();
            {
                // Build filename from fractal name + timestamp
                const char* ext = (exp_fmt == 1 && jxl_available()) ? "jxl" : "png";
                std::time_t t = std::time(nullptr);
                std::tm* tm = std::localtime(&t);
                char ts[32];
                std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm);
                // lowercase fractal name, spaces → underscore
                std::string fn_base;
                for (const char* p = fractal_name(vs); *p; ++p)
                    fn_base += (*p == ' ') ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
                std::string filename = fn_base + "_" + ts + "." + ext;
                ImGui::Text("%s", filename.c_str());

                if (!exp_done) {
                    ImGui::Spacing();
                    if (ImGui::Button("Export", ImVec2(120.0f, 0.0f))) {
                        // Freeze filename at the moment Export is clicked
                        exp_saved_name = filename;
                        // Compute target resolution
                        int tw, th;
                        switch (exp_scale) {
                            case 0: tw = last_irw;     th = last_irh;     break;
                            case 1: tw = last_irw * 2; th = last_irh * 2; break;
                            case 2: tw = last_irw * 4; th = last_irh * 4; break;
                            default: tw = exp_custom_w; th = exp_custom_h; break;
                        }
                        PixelBuffer xbuf;
                        xbuf.resize(tw, th);
                        renderer.render(vs, xbuf);
                        if (exp_fmt == 1 && jxl_available()) {
#ifdef HAVE_JXL
                            exp_msg = export_jxl(exp_saved_name.c_str(), xbuf);
#endif
                        } else {
                            exp_msg = export_png(exp_saved_name.c_str(), xbuf);
                        }
                        exp_done = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
                        ImGui::CloseCurrentPopup();
                } else {
                    ImGui::Spacing();
                    if (exp_msg.empty()) {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                                           "Saved: %s", exp_saved_name.c_str());
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                                           "Error: %s", exp_msg.c_str());
                    }
                    ImGui::Spacing();
                    if (ImGui::Button("Close", ImVec2(80.0f, 0.0f)))
                        ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }

        // -------------------------------------------------------------------
        // About dialog
        // -------------------------------------------------------------------
        if (show_about) {
            ImGui::OpenPopup("About##dlg");
            show_about = false;
        }
        if (ImGui::BeginPopupModal("About##dlg", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Fractal Xplorer  v1.0");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("A fast, no-nonsense fractal explorer.");
            ImGui::Text("Mandelbrot  |  Julia  |  Burning Ship");
            ImGui::Spacing();
            ImGui::TextDisabled("AVX2 + multithreaded tile rendering");
            ImGui::TextDisabled("8 color palettes with offset cycling");
            ImGui::TextDisabled("PNG and JPEG XL lossless export up to 8K");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("MIT License  (c) 2026 Fractal Xplorer Contributors");
            ImGui::Spacing();
            ImGui::TextDisabled("Built with Dear ImGui, SDL2, libpng, libjxl");
            ImGui::Spacing();
            ImGui::SetCursorPosX(
                (ImGui::GetContentRegionAvail().x - 120.0f) * 0.5f
                + ImGui::GetCursorPosX());
            if (ImGui::Button("Close", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        // -------------------------------------------------------------------
        // Render
        // -------------------------------------------------------------------
        ImGui::Render();
        glViewport(0, 0, win_w, win_h);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
