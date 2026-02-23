#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include "view_state.hpp"
#include "fractal.hpp"
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
    bool        dirty         = true;
    double      main_render_ms = 0.0;   // last main-render time (not overwritten by mini map)
    bool        show_about = false;

    // Benchmark dialog
    bool        show_benchmark = false;

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

    // Thread count selector (0 = Auto)
    int thread_sel = 0;

    // Orbit visualization
    bool   show_orbit    = false;
    bool   orbit_active  = false;
    double orbit_re      = 0.0;
    double orbit_im      = 0.0;

    // Mini Mandelbrot map
    PixelBuffer mini_pbuf;
    bool        mini_dirty    = true;   // render once on startup
    bool        mini_dragging = false;
    bool        mini_panning  = false;
    ImVec2      mini_pan_start_mouse = {};
    double      mini_pan_start_cx = 0.0;
    double      mini_pan_start_cy = 0.0;
    double      mini_cx       = 0.0;   // minimap center (fractal coords)
    double      mini_cy       = 0.0;
    double      mini_vw       = 4.0;   // minimap view width

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
        // Block until an SDL event arrives or 50 ms elapses.
        // This eliminates busy-spinning when the app is idle.
        // Mouse/keyboard input still wakes the loop immediately.
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 50)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
        }
        // Drain any additional events that queued up.
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
                main_render_ms = renderer.last_render_ms;
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
                    reset_view_keep_params(vs, vs.formula, vs.julia_mode);
                    dirty = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Threads")) {
                const int hw = renderer.hw_concurrency;
                char buf[32];
                snprintf(buf, sizeof(buf), "Auto (%d)", hw);
                if (ImGui::MenuItem(buf, nullptr, thread_sel == 0)) {
                    thread_sel = 0;
                    renderer.set_thread_count(0);
                    dirty = true;
                }
                ImGui::Separator();
                for (int i = 1; i <= hw; ++i) {
                    snprintf(buf, sizeof(buf), "%d", i);
                    if (ImGui::MenuItem(buf, nullptr, thread_sel == i)) {
                        thread_sel = i;
                        renderer.set_thread_count(i);
                        dirty = true;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("Benchmark", "B")) show_benchmark = true;
                ImGui::Separator();
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
            reset_view_keep_params(vs, vs.formula, vs.julia_mode);
            dirty = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F1))
            show_about = true;
        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_Equal) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
                vs.view_width /= 1.5;  dirty = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Minus) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
                vs.view_width *= 1.5;  dirty = true;
            }
            // Arrow keys: pan by 10% of view width
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow,  true))
                { vs.center_x -= vs.view_width * 0.1;  dirty = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
                { vs.center_x += vs.view_width * 0.1;  dirty = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow,    true))
                { vs.center_y -= vs.view_width * 0.1;  dirty = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow,  true))
                { vs.center_y += vs.view_width * 0.1;  dirty = true; }
            // PageUp/Down: double or halve iteration count
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
                { vs.max_iter = std::min(vs.max_iter * 2, 8192);  dirty = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
                { vs.max_iter = std::max(vs.max_iter / 2, 64);    dirty = true; }
            // P / Shift+P: cycle palette forward / backward
            if (ImGui::IsKeyPressed(ImGuiKey_P)) {
                int dir = io.KeyShift ? -1 : 1;
                vs.palette = (vs.palette + dir + PALETTE_COUNT) % PALETTE_COUNT;
                dirty = true;
            }
            // B: benchmark
            if (ImGui::IsKeyPressed(ImGuiKey_B))
                show_benchmark = true;
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
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoScrollbar           |
            ImGuiWindowFlags_NoScrollWithMouse);

        // --- Formula selector ---
        ImGui::TextDisabled("FORMULA");
        ImGui::Separator();
        {
            static const char* names[] = {
                "Mandelbrot  (z^2 + c)",
                "Burning Ship  (|z|^2 + c)",
                "Celtic  (|Re(z^2)| + c)",
                "Buffalo  (|Re(z^2)| + i|Im(z^2)| + c)",
                "Mandelbar  (conj(z)^n + c)",
                "Multibrot  (z^n + c)",
                "Multibrot  (z^r + c, slow)",
            };
            int f = static_cast<int>(vs.formula);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##formula", &f, names, FORMULA_COUNT)) {
                reset_view_keep_params(vs, static_cast<FormulaType>(f), vs.julia_mode);
                dirty = true;
            }
            if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f) {
                int nf = (f + (io.MouseWheel < 0.0f ? 1 : -1) + FORMULA_COUNT) % FORMULA_COUNT;
                reset_view_keep_params(vs, static_cast<FormulaType>(nf), vs.julia_mode);
                dirty = true;
            }
            ImGui::Spacing();
            if (ImGui::Checkbox("Julia mode", &vs.julia_mode))
                dirty = true;
        }

        // --- Exponent ---
        if (vs.formula == FormulaType::Mandelbar || vs.formula == FormulaType::MultiFast) {
            ImGui::Spacing();
            ImGui::TextDisabled("EXPONENT");
            ImGui::Separator();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderInt("##mexp", &vs.multibrot_exp, 2, 8))
                dirty = true;
        } else if (vs.formula == FormulaType::MultiSlow) {
            ImGui::Spacing();
            ImGui::TextDisabled("EXPONENT (float)");
            ImGui::Separator();
            static const double slow_min = -10.0, slow_max = 10.0;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderScalar("##mexpf_slider", ImGuiDataType_Double,
                                    &vs.multibrot_exp_f, &slow_min, &slow_max, "%.4f"))
                dirty = true;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputDouble("##mexpf", &vs.multibrot_exp_f, 0.1, 0.5, "%.4f"))
                dirty = true;
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
            if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f) {
                vs.palette = (vs.palette + (io.MouseWheel < 0.0f ? 1 : -1) + PALETTE_COUNT) % PALETTE_COUNT;
                dirty = true;
            }
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

        // Mini map dimensions: square, covers -2..2 on both axes
        const float map_w     = ImGui::GetContentRegionAvail().x;
        const float map_h     = map_w;   // square: same range on both axes
        const int   map_iw    = static_cast<int>(map_w);
        const int   map_ih    = static_cast<int>(map_h);
        // Complex units per display pixel in the mini map
        const float map_scale = static_cast<float>(mini_vw) / map_w;

        // Re-render mini map when formula, exponent, or minimap view changes
        static FormulaType mini_last_formula  = FormulaType::Standard;
        static int         mini_last_exp      = 2;
        static double      mini_last_exp_f    = 3.0;
        static double      mini_last_cx       = 0.0;
        static double      mini_last_cy       = 0.0;
        static double      mini_last_vw       = 4.0;
        if (mini_last_formula != vs.formula         ||
            mini_last_exp     != vs.multibrot_exp   ||
            mini_last_exp_f   != vs.multibrot_exp_f ||
            mini_last_cx      != mini_cx            ||
            mini_last_cy      != mini_cy            ||
            mini_last_vw      != mini_vw) {
            mini_dirty          = true;
            mini_last_formula   = vs.formula;
            mini_last_exp       = vs.multibrot_exp;
            mini_last_exp_f     = vs.multibrot_exp_f;
            mini_last_cx        = mini_cx;
            mini_last_cy        = mini_cy;
            mini_last_vw        = mini_vw;
        }

        // Render mini map: Mandelbrot-mode of current formula, current mini view
        if (mini_dirty && map_iw > 0 && map_ih > 0) {
            ViewState mini_vs;
            mini_vs.center_x        = mini_cx;
            mini_vs.center_y        = mini_cy;
            mini_vs.view_width      = mini_vw;
            mini_vs.formula         = vs.formula;
            mini_vs.julia_mode      = false;   // always Mandelbrot-mode (parameter space)
            mini_vs.max_iter        = 128;
            mini_vs.palette         = 7;
            mini_vs.multibrot_exp   = vs.multibrot_exp;
            mini_vs.multibrot_exp_f = vs.multibrot_exp_f;
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
                + static_cast<float>((vs.julia_re - mini_cx) / map_scale)
                + map_w * 0.5f;
            const float dot_y = map_tl.y
                + static_cast<float>((vs.julia_im - mini_cy) / map_scale)
                + map_h * 0.5f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddCircleFilled(ImVec2(dot_x, dot_y), 3.5f,
                                IM_COL32(255,  50,  50, 230));
            dl->AddCircle      (ImVec2(dot_x, dot_y), 5.5f,
                                IM_COL32(255, 220,  50, 255), 0, 1.5f);

            // Left-click/drag: pick Julia c parameter
            if (map_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                mini_dragging = true;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                mini_dragging = false;

            if (mini_dragging) {
                const float mx = io.MousePos.x - map_tl.x;
                const float my = io.MousePos.y - map_tl.y;
                vs.julia_re = mini_cx + static_cast<double>((mx - map_w * 0.5f) * map_scale);
                vs.julia_im = mini_cy + static_cast<double>((my - map_h * 0.5f) * map_scale);
                dirty = true;
            }

            // Right-click drag: pan minimap
            if (map_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                mini_panning        = true;
                mini_pan_start_mouse = io.MousePos;
                mini_pan_start_cx   = mini_cx;
                mini_pan_start_cy   = mini_cy;
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
                mini_panning = false;

            if (mini_panning) {
                mini_cx = mini_pan_start_cx - (io.MousePos.x - mini_pan_start_mouse.x) * map_scale;
                mini_cy = mini_pan_start_cy - (io.MousePos.y - mini_pan_start_mouse.y) * map_scale;
            }

            // Mouse wheel zoom on minimap (centered on cursor)
            if (map_hovered && io.MouseWheel != 0.0f) {
                const float  mx     = io.MousePos.x - map_tl.x;
                const float  my     = io.MousePos.y - map_tl.y;
                const double cur_re = mini_cx + (mx - map_w * 0.5f) * map_scale;
                const double cur_im = mini_cy + (my - map_h * 0.5f) * map_scale;
                const double factor = (io.MouseWheel > 0.0f) ? 1.25 : (1.0 / 1.25);
                mini_vw /= factor;
                const double ns = mini_vw / map_w;
                mini_cx = cur_re - (mx - map_w * 0.5f) * ns;
                mini_cy = cur_im - (my - map_h * 0.5f) * ns;
            }
        }

        // Reset minimap view
        if (ImGui::Button("Reset##minimap", ImVec2(-1.0f, 0.0f))) {
            mini_cx = 0.0;  mini_cy = 0.0;  mini_vw = 4.0;
            mini_dirty = true;
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

        // --- Orbit ---
        ImGui::Spacing();
        ImGui::TextDisabled("ORBIT");
        ImGui::Separator();
        if (ImGui::Checkbox("Show orbit", &show_orbit)) {
            if (!show_orbit) orbit_active = false;
        }
        if (show_orbit)
            ImGui::TextDisabled("Ctrl+click to pick point");

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

        // Ctrl+click: pick orbit seed (checked before pan so Ctrl suppresses pan)
        if (show_orbit && render_hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyCtrl) {
            const double scale = vs.view_width / irw;
            orbit_re     = vs.center_x + (io.MousePos.x - render_x - irw * 0.5) * scale;
            orbit_im     = vs.center_y + (io.MousePos.y - render_y - irh * 0.5) * scale;
            orbit_active = true;
        }

        // Left-click drag: pan (skip when Ctrl is held for orbit pick)
        if (render_hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !zoom_boxing && !io.KeyCtrl) {
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

        // Orbit overlay
        if (show_orbit && orbit_active) {
            auto pts = compute_orbit(orbit_re, orbit_im, vs, 20);
            const double scale = vs.view_width / irw;
            auto to_screen = [&](double r, double i) -> ImVec2 {
                return { render_x + static_cast<float>((r - vs.center_x) / scale + irw * 0.5f),
                         render_y + static_cast<float>((i - vs.center_y) / scale + irh * 0.5f) };
            };
            ImDrawList* odl = ImGui::GetWindowDrawList();
            for (size_t k = 0; k < pts.size(); ++k)
                odl->AddCircleFilled(to_screen(pts[k].first, pts[k].second),
                                     k == 0 ? 4.0f : 2.5f,
                                     k == 0 ? IM_COL32(255, 80, 80, 230)
                                            : IM_COL32(255, 220, 50, 230));
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
                    main_render_ms,
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
        // Benchmark dialog
        // -------------------------------------------------------------------
        if (show_benchmark) {
            ImGui::SetNextWindowSize(ImVec2(520, 620), ImGuiCond_Always);
            ImGui::OpenPopup("Benchmark##dlg");
            show_benchmark = false;
        }
        if (ImGui::BeginPopupModal("Benchmark##dlg", nullptr, ImGuiWindowFlags_NoResize)) {
            const int hw = renderer.hw_concurrency;

            // Per-run state (static — persists across frames while modal is open)
            static bool   bench_running  = false;
            static bool   bench_done     = false;
            static int    bench_phase    = 0;   // 0=AVX2, 1=scalar
            static int    bench_ti       = 0;   // thread index 0-based
            static int    bench_rep      = 0;   // repetition 0-3
            static double bench_sum      = 0.0;
            static int    bench_saved_tc = 0;   // saved thread count
            static bool   bench_saved_a2 = false;
            static std::vector<float> bench_avx2;
            static std::vector<float> bench_scalar;
            static PixelBuffer bench_buf;

            // One render step per frame while running
            if (bench_running) {
                renderer.set_thread_count(bench_ti + 1);
                renderer.set_avx2(bench_phase == 0);

                ViewState bvs;   // Mandelbrot, center (-0.5,0), width 3.5, 256 iter
                bvs.center_x   = -0.5;
                bvs.view_width =  3.5;
                bench_buf.resize(1920, 1080);
                renderer.render(bvs, bench_buf);
                bench_sum += renderer.last_render_ms;
                bench_rep++;

                if (bench_rep == 4) {
                    const double avg_ms = bench_sum / 4.0;
                    const float  mpixs  = static_cast<float>(
                        1920.0 * 1080.0 / (avg_ms * 1000.0));
                    if (bench_phase == 0) bench_avx2[bench_ti]   = mpixs;
                    else                  bench_scalar[bench_ti]  = mpixs;
                    bench_sum = 0.0;
                    bench_rep = 0;
                    bench_ti++;

                    if (bench_ti == hw) {
                        bench_ti = 0;
                        bench_phase++;
                        if (bench_phase == 2) {
                            bench_running = false;
                            bench_done    = true;
                            renderer.set_thread_count(bench_saved_tc);
                            renderer.set_avx2(bench_saved_a2);
                            dirty = true;  // restore main view
                        }
                    }
                }
            }

            // Run button
            if (!bench_running) {
                if (ImGui::Button(bench_done ? "Run again" : "Run")) {
                    bench_avx2.assign(hw, 0.0f);
                    bench_scalar.assign(hw, 0.0f);
                    bench_phase   = 0;
                    bench_ti      = 0;
                    bench_rep     = 0;
                    bench_sum     = 0.0;
                    bench_done    = false;
                    bench_saved_tc = renderer.thread_count;
                    bench_saved_a2 = renderer.avx2_active;
                    bench_running  = true;
                }
            } else {
                ImGui::BeginDisabled();
                ImGui::Button("Running...");
                ImGui::EndDisabled();
            }

            // Progress
            if (bench_running || bench_done) {
                const int total = hw * 2 * 4;
                const int done  = bench_phase * hw * 4 + bench_ti * 4 + bench_rep;
                ImGui::SameLine();
                char prog[64];
                if (bench_running)
                    snprintf(prog, sizeof(prog), "%s  %d/%d threads  rep %d/4",
                             bench_phase == 0 ? "AVX2" : "Scalar",
                             bench_ti + 1, hw, bench_rep + 1);
                else
                    snprintf(prog, sizeof(prog), "Done");
                ImGui::TextDisabled("%s", prog);
                ImGui::ProgressBar(static_cast<float>(done) / total,
                                   ImVec2(-1.0f, 0.0f));
            }

            // Chart — overlay AVX2 (blue) and Scalar (orange) on same area
            if ((bench_running && (bench_phase > 0 || bench_ti > 0)) || bench_done) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                const float avail = ImGui::GetContentRegionAvail().x;
                const ImVec2 plot_sz(avail, 110.0f);

                // Compute common Y scale from whichever data is ready
                float y_max = 1.0f;
                for (int i = 0; i < hw; ++i) {
                    if (bench_avx2[i]   > y_max) y_max = bench_avx2[i];
                    if (bench_scalar[i] > y_max) y_max = bench_scalar[i];
                }
                y_max *= 1.1f;  // 10% headroom

                // AVX2 chart
                char avx2_lbl[48], scalar_lbl[48];
                snprintf(avx2_lbl,   sizeof(avx2_lbl),
                         "AVX2  (Mpix/s, 1..%d threads)", hw);
                snprintf(scalar_lbl, sizeof(scalar_lbl),
                         "Scalar(Mpix/s, 1..%d threads)", hw);

                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
                ImGui::PlotHistogram("##avx2", bench_avx2.data(), hw, 0,
                                     avx2_lbl, 0.0f, y_max, plot_sz);
                ImGui::PopStyleColor();

                // Scalar chart — same Y scale so they are visually comparable
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
                ImGui::PlotHistogram("##scalar", bench_scalar.data(), hw, 0,
                                     scalar_lbl, 0.0f, y_max, plot_sz);
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::TextDisabled("1920x1080  Mandelbrot  256 iter  avg 4 runs"
                                    "  hover for exact value");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                if (bench_running) {
                    bench_running = false;
                    renderer.set_thread_count(bench_saved_tc);
                    renderer.set_avx2(bench_saved_a2);
                    dirty = true;
                }
                ImGui::CloseCurrentPopup();
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
            ImGui::Text("Fractal Xplorer  v1.5");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("A fast, no-nonsense fractal explorer.");
            ImGui::Text("z^2  |  Burning Ship  |  Mandelbar  |  z^n  |  Julia mode for all");
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
