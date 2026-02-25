#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include "app_state.hpp"
#include "ui_panels.hpp"
#include "fractal.hpp"
#include "palette.hpp"
#include "cli_benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

static const float PANEL_WIDTH   = 280.0f;
static const float STATUS_HEIGHT = 24.0f;

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    // CLI benchmark mode — no GUI needed
    if (argc > 1 && std::string(argv[1]) == "--benchmark") {
        run_cli_benchmark();
        return 0;
    }

    // Check for --no-avx2 flag anywhere in argv
    bool force_no_avx2 = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--no-avx2") {
            force_no_avx2 = true;
            break;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "Fractal Xplorer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    init_palettes();

    // -----------------------------------------------------------------------
    // App state
    // -----------------------------------------------------------------------
    AppState app;
    if (force_no_avx2)
        app.renderer.set_avx2(false);

    auto update_title = [&]() {
        char tbuf[128];
        std::snprintf(tbuf, sizeof(tbuf), "Fractal Xplorer  —  %s  [zoom: %.2fx]",
                      fractal_name(app.vs), zoom_display(app.vs));
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

        app.last_irw = irw;
        app.last_irh = irh;

        // Main fractal render
        if (app.dirty || irw != app.pbuf.width || irh != app.pbuf.height) {
            if (irw > 0 && irh > 0) {
                app.pbuf.resize(irw, irh);
                app.renderer.render(app.vs, app.pbuf);
                app.main_render_ms = app.renderer.last_render_ms;
                app.render_tex.ensure(irw, irh);
                app.render_tex.upload(app.pbuf);
                update_title();
            }
            app.dirty = false;
        }

        // -------------------------------------------------------------------
        // Menu bar
        // -------------------------------------------------------------------
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Export Image", "Ctrl+S")) {
                    app.show_export = true;
                    app.exp_done    = false;
                    app.exp_msg.clear();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) running = false;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Reset View", "R")) {
                    reset_view_keep_params(app.vs, app.vs.formula, app.vs.julia_mode);
                    app.dirty = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Threads")) {
                const int hw = app.renderer.hw_concurrency;
                char buf[32];
                snprintf(buf, sizeof(buf), "Auto (%d)", hw);
                if (ImGui::MenuItem(buf, nullptr, app.thread_sel == 0)) {
                    app.thread_sel = 0;
                    app.renderer.set_thread_count(0);
                    app.dirty = true;
                }
                ImGui::Separator();
                for (int i = 1; i <= hw; ++i) {
                    snprintf(buf, sizeof(buf), "%d", i);
                    if (ImGui::MenuItem(buf, nullptr, app.thread_sel == i)) {
                        app.thread_sel = i;
                        app.renderer.set_thread_count(i);
                        app.dirty = true;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("Benchmark", "B")) app.show_benchmark = true;
                ImGui::Separator();
                if (ImGui::MenuItem("About", "F1")) app.show_about = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // -------------------------------------------------------------------
        // Global keyboard shortcuts
        // -------------------------------------------------------------------
        if (ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl) {
            app.show_export = true;
            app.exp_done    = false;
            app.exp_msg.clear();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) {
            reset_view_keep_params(app.vs, app.vs.formula, app.vs.julia_mode);
            app.dirty = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F1))
            app.show_about = true;
        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_Equal) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
                app.vs.view_width /= 1.5;  app.dirty = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Minus) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
                app.vs.view_width *= 1.5;  app.dirty = true;
            }
            // Arrow keys: pan by 10% of view width
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow,  true))
                { app.vs.center_x -= app.vs.view_width * 0.1;  app.dirty = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
                { app.vs.center_x += app.vs.view_width * 0.1;  app.dirty = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow,    true))
                { app.vs.center_y -= app.vs.view_width * 0.1;  app.dirty = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow,  true))
                { app.vs.center_y += app.vs.view_width * 0.1;  app.dirty = true; }
            // PageUp/Down: double or halve iteration count
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
                { app.vs.max_iter = std::min(app.vs.max_iter * 2, 8192);  app.dirty = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
                { app.vs.max_iter = std::max(app.vs.max_iter / 2, 64);    app.dirty = true; }
            // P / Shift+P: cycle palette forward / backward
            if (ImGui::IsKeyPressed(ImGuiKey_P)) {
                int dir = io.KeyShift ? -1 : 1;
                app.vs.palette = (app.vs.palette + dir + PALETTE_COUNT) % PALETTE_COUNT;
                app.dirty = true;
            }
            // B: benchmark
            if (ImGui::IsKeyPressed(ImGuiKey_B))
                app.show_benchmark = true;
        }

        // -------------------------------------------------------------------
        // Side panel
        // -------------------------------------------------------------------
        draw_side_panel(app, io, menu_h, fh);

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

        if (app.render_tex.id)
            ImGui::Image(app.render_tex.imgui_id(),
                         ImVec2(static_cast<float>(app.render_tex.w),
                                static_cast<float>(app.render_tex.h)));

        const bool render_hovered = ImGui::IsWindowHovered();

        // Mouse wheel zoom (centered on cursor)
        if (render_hovered && io.MouseWheel != 0.0f) {
            const double mx     = io.MousePos.x - render_x;
            const double my     = io.MousePos.y - render_y;
            const double scale  = app.vs.view_width / irw;
            const double cur_re = app.vs.center_x + (mx - irw * 0.5) * scale;
            const double cur_im = app.vs.center_y + (my - irh * 0.5) * scale;
            const double factor = (io.MouseWheel > 0.0f) ? 1.25 : (1.0 / 1.25);
            app.vs.view_width  /= factor;
            const double ns     = app.vs.view_width / irw;
            app.vs.center_x = cur_re - (mx - irw * 0.5) * ns;
            app.vs.center_y = cur_im - (my - irh * 0.5) * ns;
            app.dirty = true;
        }

        // Ctrl+click: pick orbit seed (checked before pan so Ctrl suppresses pan)
        if (app.show_orbit && render_hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyCtrl) {
            const double scale = app.vs.view_width / irw;
            app.orbit_re     = app.vs.center_x + (io.MousePos.x - render_x - irw * 0.5) * scale;
            app.orbit_im     = app.vs.center_y + (io.MousePos.y - render_y - irh * 0.5) * scale;
            app.orbit_active = true;
        }

        // Left-click drag: pan (skip when Ctrl is held for orbit pick)
        if (render_hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !app.zoom_boxing && !io.KeyCtrl) {
            app.panning         = true;
            app.pan_start_mouse = io.MousePos;
            app.pan_start_vs    = app.vs;
        }
        if (app.panning) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                const double scale = app.pan_start_vs.view_width / irw;
                app.vs.center_x   = app.pan_start_vs.center_x
                                     - (io.MousePos.x - app.pan_start_mouse.x) * scale;
                app.vs.center_y   = app.pan_start_vs.center_y
                                     - (io.MousePos.y - app.pan_start_mouse.y) * scale;
                app.vs.view_width = app.pan_start_vs.view_width;
                app.dirty = true;
            } else {
                app.panning = false;
            }
        }

        // Right-click drag: zoom box
        if (render_hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !app.panning) {
            app.zoom_boxing = true;
            app.zbox_start  = io.MousePos;
            app.zbox_end    = io.MousePos;
        }
        if (app.zoom_boxing) {
            app.zbox_end = io.MousePos;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(app.zbox_start, app.zbox_end, IM_COL32(255, 255, 255, 20));
            dl->AddRect(app.zbox_start, app.zbox_end, IM_COL32(255, 255, 255, 200),
                        0.0f, 0, 1.5f);

            if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                const float x0 = std::min(app.zbox_start.x, app.zbox_end.x) - render_x;
                const float y0 = std::min(app.zbox_start.y, app.zbox_end.y) - render_y;
                const float x1 = std::max(app.zbox_start.x, app.zbox_end.x) - render_x;
                const float y1 = std::max(app.zbox_start.y, app.zbox_end.y) - render_y;
                const float bw = x1 - x0;
                const float bh = y1 - y0;
                if (bw > 4.0f && bh > 4.0f) {
                    const double scale = app.vs.view_width / irw;
                    app.vs.center_x   = app.vs.center_x + (x0 + bw * 0.5f - irw * 0.5) * scale;
                    app.vs.center_y   = app.vs.center_y + (y0 + bh * 0.5f - irh * 0.5) * scale;
                    app.vs.view_width = bw * scale;
                    app.dirty = true;
                }
                app.zoom_boxing = false;
            }
        }

        // Orbit overlay
        if (app.show_orbit && app.orbit_active) {
            auto pts = compute_orbit(app.orbit_re, app.orbit_im, app.vs, 20);
            const double scale = app.vs.view_width / irw;
            auto to_screen = [&](double r, double i) -> ImVec2 {
                return { render_x + static_cast<float>((r - app.vs.center_x) / scale + irw * 0.5f),
                         render_y + static_cast<float>((i - app.vs.center_y) / scale + irh * 0.5f) };
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
                    app.vs.center_x, app.vs.center_y, zoom_display(app.vs), app.vs.max_iter,
                    app.main_render_ms,
                    app.renderer.avx2_active ? "AVX2" : "scalar",
                    app.renderer.thread_count);
        ImGui::End();

        // -------------------------------------------------------------------
        // Dialogs
        // -------------------------------------------------------------------
        draw_export_dialog(app);
        draw_benchmark_dialog(app);
        draw_about_dialog(app);

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
