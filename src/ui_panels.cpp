#include "ui_panels.hpp"
#include "app_state.hpp"
#include "fractal.hpp"
#include "palette.hpp"
#include "export.hpp"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

static const float PANEL_WIDTH = 280.0f;

// ---------------------------------------------------------------------------
// Side panel: formula, exponent, iterations, palette, minimap, orbit
// ---------------------------------------------------------------------------
void draw_side_panel(AppState& app, const ImGuiIO& io, float menu_h, float fh)
{
    static const float STATUS_HEIGHT = 24.0f;

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
        int f = static_cast<int>(app.vs.formula);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##formula", &f, names, FORMULA_COUNT)) {
            reset_view_keep_params(app.vs, static_cast<FormulaType>(f), app.vs.julia_mode);
            app.dirty = true;
        }
        if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f) {
            int nf = (f + (io.MouseWheel < 0.0f ? 1 : -1) + FORMULA_COUNT) % FORMULA_COUNT;
            reset_view_keep_params(app.vs, static_cast<FormulaType>(nf), app.vs.julia_mode);
            app.dirty = true;
        }
        ImGui::Spacing();
        if (ImGui::Checkbox("Julia mode", &app.vs.julia_mode))
            app.dirty = true;
    }

    // --- Exponent ---
    if (app.vs.formula == FormulaType::Mandelbar || app.vs.formula == FormulaType::MultiFast) {
        ImGui::Spacing();
        ImGui::TextDisabled("EXPONENT");
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##mexp", &app.vs.multibrot_exp, 2, 8))
            app.dirty = true;
    } else if (app.vs.formula == FormulaType::MultiSlow) {
        ImGui::Spacing();
        ImGui::TextDisabled("EXPONENT (float)");
        ImGui::Separator();
        static const double slow_min = -10.0, slow_max = 10.0;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderScalar("##mexpf_slider", ImGuiDataType_Double,
                                &app.vs.multibrot_exp_f, &slow_min, &slow_max, "%.4f"))
            app.dirty = true;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputDouble("##mexpf", &app.vs.multibrot_exp_f, 0.1, 0.5, "%.4f"))
            app.dirty = true;
    }

    // --- Iteration count ---
    ImGui::Spacing();
    ImGui::TextDisabled("ITERATIONS");
    ImGui::Separator();
    {
        int iter = app.vs.max_iter;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##iter", &iter, 64, 8192, "%d",
                             ImGuiSliderFlags_Logarithmic)) {
            app.vs.max_iter = iter;
            app.dirty = true;
        }
    }

    // --- Palette ---
    ImGui::Spacing();
    ImGui::TextDisabled("PALETTE");
    ImGui::Separator();
    {
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##palette", &app.vs.palette, g_palette_names, PALETTE_COUNT))
            app.dirty = true;
        if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f) {
            app.vs.palette = (app.vs.palette + (io.MouseWheel < 0.0f ? 1 : -1) + PALETTE_COUNT) % PALETTE_COUNT;
            app.dirty = true;
        }
        ImGui::Spacing();
        ImGui::Text("Offset");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##paloff", &app.vs.pal_offset, 0, LUT_SIZE - 1))
            app.dirty = true;
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
    const float map_scale = static_cast<float>(app.mini_vw) / map_w;

    // Re-render mini map when formula, exponent, or minimap view changes
    static FormulaType mini_last_formula  = FormulaType::Standard;
    static int         mini_last_exp      = 2;
    static double      mini_last_exp_f    = 3.0;
    static double      mini_last_cx       = 0.0;
    static double      mini_last_cy       = 0.0;
    static double      mini_last_vw       = 4.0;
    if (mini_last_formula != app.vs.formula         ||
        mini_last_exp     != app.vs.multibrot_exp   ||
        mini_last_exp_f   != app.vs.multibrot_exp_f ||
        mini_last_cx      != app.mini_cx            ||
        mini_last_cy      != app.mini_cy            ||
        mini_last_vw      != app.mini_vw) {
        app.mini_dirty        = true;
        mini_last_formula     = app.vs.formula;
        mini_last_exp         = app.vs.multibrot_exp;
        mini_last_exp_f       = app.vs.multibrot_exp_f;
        mini_last_cx          = app.mini_cx;
        mini_last_cy          = app.mini_cy;
        mini_last_vw          = app.mini_vw;
    }

    // Render mini map: Mandelbrot-mode of current formula, current mini view
    if (app.mini_dirty && map_iw > 0 && map_ih > 0) {
        ViewState mini_vs;
        mini_vs.center_x        = app.mini_cx;
        mini_vs.center_y        = app.mini_cy;
        mini_vs.view_width      = app.mini_vw;
        mini_vs.formula         = app.vs.formula;
        mini_vs.julia_mode      = false;   // always Mandelbrot-mode (parameter space)
        mini_vs.max_iter        = 128;
        mini_vs.palette         = 7;
        mini_vs.multibrot_exp   = app.vs.multibrot_exp;
        mini_vs.multibrot_exp_f = app.vs.multibrot_exp_f;
        app.mini_pbuf.resize(map_iw, map_ih);
        app.renderer.render(mini_vs, app.mini_pbuf);
        app.mini_tex.ensure(map_iw, map_ih);
        app.mini_tex.upload(app.mini_pbuf);
        app.mini_dirty = false;
    }

    if (app.mini_tex.id) {
        const ImVec2 map_tl = ImGui::GetCursorScreenPos();

        // Draw mini map
        ImGui::Image(app.mini_tex.imgui_id(), ImVec2(map_w, map_h));
        const bool map_hovered = ImGui::IsItemHovered();

        // Draw c-parameter indicator (bullseye)
        const float dot_x = map_tl.x
            + static_cast<float>((app.vs.julia_re - app.mini_cx) / map_scale)
            + map_w * 0.5f;
        const float dot_y = map_tl.y
            + static_cast<float>((app.vs.julia_im - app.mini_cy) / map_scale)
            + map_h * 0.5f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddCircleFilled(ImVec2(dot_x, dot_y), 3.5f,
                            IM_COL32(255,  50,  50, 230));
        dl->AddCircle      (ImVec2(dot_x, dot_y), 5.5f,
                            IM_COL32(255, 220,  50, 255), 0, 1.5f);

        // Left-click/drag: pick Julia c parameter
        if (map_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            app.mini_dragging = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            app.mini_dragging = false;

        if (app.mini_dragging) {
            const float mx = io.MousePos.x - map_tl.x;
            const float my = io.MousePos.y - map_tl.y;
            app.vs.julia_re = app.mini_cx + static_cast<double>((mx - map_w * 0.5f) * map_scale);
            app.vs.julia_im = app.mini_cy + static_cast<double>((my - map_h * 0.5f) * map_scale);
            app.dirty = true;
        }

        // Right-click drag: pan minimap
        if (map_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            app.mini_panning        = true;
            app.mini_pan_start_mouse = io.MousePos;
            app.mini_pan_start_cx   = app.mini_cx;
            app.mini_pan_start_cy   = app.mini_cy;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
            app.mini_panning = false;

        if (app.mini_panning) {
            app.mini_cx = app.mini_pan_start_cx - (io.MousePos.x - app.mini_pan_start_mouse.x) * map_scale;
            app.mini_cy = app.mini_pan_start_cy - (io.MousePos.y - app.mini_pan_start_mouse.y) * map_scale;
        }

        // Mouse wheel zoom on minimap (centered on cursor)
        if (map_hovered && io.MouseWheel != 0.0f) {
            const float  mx     = io.MousePos.x - map_tl.x;
            const float  my     = io.MousePos.y - map_tl.y;
            const double cur_re = app.mini_cx + (mx - map_w * 0.5f) * map_scale;
            const double cur_im = app.mini_cy + (my - map_h * 0.5f) * map_scale;
            const double factor = (io.MouseWheel > 0.0f) ? 1.25 : (1.0 / 1.25);
            app.mini_vw /= factor;
            const double ns = app.mini_vw / map_w;
            app.mini_cx = cur_re - (mx - map_w * 0.5f) * ns;
            app.mini_cy = cur_im - (my - map_h * 0.5f) * ns;
        }
    }

    // Reset minimap view
    if (ImGui::Button("Reset##minimap", ImVec2(-1.0f, 0.0f))) {
        app.mini_cx = 0.0;  app.mini_cy = 0.0;  app.mini_vw = 4.0;
        app.mini_dirty = true;
    }

    // re / im numeric inputs
    ImGui::Spacing();
    {
        double re = app.vs.julia_re;
        double im = app.vs.julia_im;
        ImGui::Text("re:"); ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputDouble("##jre", &re, 0.001, 0.01, "%.8f"))
            { app.vs.julia_re = re; app.dirty = true; }
        ImGui::Text("im:"); ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputDouble("##jim", &im, 0.001, 0.01, "%.8f"))
            { app.vs.julia_im = im; app.dirty = true; }
    }

    // --- Orbit ---
    ImGui::Spacing();
    ImGui::TextDisabled("ORBIT");
    ImGui::Separator();
    if (ImGui::Checkbox("Show orbit", &app.show_orbit)) {
        if (!app.show_orbit) app.orbit_active = false;
    }
    if (app.show_orbit)
        ImGui::TextDisabled("Ctrl+click to pick point");

    ImGui::End();  // ##panel
}

// ---------------------------------------------------------------------------
// Export dialog
// ---------------------------------------------------------------------------
void draw_export_dialog(AppState& app)
{
    if (app.show_export) {
        ImGui::OpenPopup("Export Image##dlg");
        app.show_export = false;
    }
    if (ImGui::BeginPopupModal("Export Image##dlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        // Format selector
        ImGui::TextDisabled("FORMAT");
        ImGui::Separator();
        if (!jxl_available()) {
            ImGui::RadioButton("PNG", &app.exp_fmt, 0);
            ImGui::SameLine();
            ImGui::TextDisabled("JXL (not available)");
        } else {
            ImGui::RadioButton("PNG", &app.exp_fmt, 0);
            ImGui::SameLine();
            ImGui::RadioButton("JPEG XL (lossless)", &app.exp_fmt, 1);
        }

        // Resolution selector
        ImGui::Spacing();
        ImGui::TextDisabled("RESOLUTION");
        ImGui::Separator();
        {
            char buf1[64], buf2[64], buf4[64];
            std::snprintf(buf1, sizeof(buf1), "1x   %d x %d", app.last_irw,     app.last_irh    );
            std::snprintf(buf2, sizeof(buf2), "2x   %d x %d", app.last_irw * 2, app.last_irh * 2);
            std::snprintf(buf4, sizeof(buf4), "4x   %d x %d", app.last_irw * 4, app.last_irh * 4);
            ImGui::RadioButton(buf1, &app.exp_scale, 0);
            ImGui::RadioButton(buf2, &app.exp_scale, 1);
            ImGui::RadioButton(buf4, &app.exp_scale, 2);
            ImGui::RadioButton("Custom", &app.exp_scale, 3);
            if (app.exp_scale == 3) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80.0f);
                ImGui::InputInt("##cw", &app.exp_custom_w, 0);
                app.exp_custom_w = std::max(16, std::min(app.exp_custom_w, 7680));
                ImGui::SameLine(); ImGui::TextUnformatted("x");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80.0f);
                ImGui::InputInt("##ch", &app.exp_custom_h, 0);
                app.exp_custom_h = std::max(16, std::min(app.exp_custom_h, 4320));
            }
        }

        // Filename preview
        ImGui::Spacing();
        ImGui::TextDisabled("OUTPUT");
        ImGui::Separator();
        {
            // Build filename from fractal name + timestamp
            const char* ext = (app.exp_fmt == 1 && jxl_available()) ? "jxl" : "png";
            std::time_t t = std::time(nullptr);
            std::tm* tm = std::localtime(&t);
            char ts[32];
            std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm);
            // lowercase fractal name, spaces -> underscore
            std::string fn_base;
            for (const char* p = fractal_name(app.vs); *p; ++p)
                fn_base += (*p == ' ') ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
            std::string filename = fn_base + "_" + ts + "." + ext;
            ImGui::Text("%s", filename.c_str());

            if (!app.exp_done) {
                ImGui::Spacing();
                if (ImGui::Button("Export", ImVec2(120.0f, 0.0f))) {
                    // Freeze filename at the moment Export is clicked
                    app.exp_saved_name = filename;
                    // Compute target resolution
                    int tw, th;
                    switch (app.exp_scale) {
                        case 0: tw = app.last_irw;     th = app.last_irh;     break;
                        case 1: tw = app.last_irw * 2; th = app.last_irh * 2; break;
                        case 2: tw = app.last_irw * 4; th = app.last_irh * 4; break;
                        default: tw = app.exp_custom_w; th = app.exp_custom_h; break;
                    }
                    PixelBuffer xbuf;
                    xbuf.resize(tw, th);
                    app.renderer.render(app.vs, xbuf);
                    if (app.exp_fmt == 1 && jxl_available()) {
#ifdef HAVE_JXL
                        app.exp_msg = export_jxl(app.exp_saved_name.c_str(), xbuf);
#endif
                    } else {
                        app.exp_msg = export_png(app.exp_saved_name.c_str(), xbuf);
                    }
                    app.exp_done = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
                    ImGui::CloseCurrentPopup();
            } else {
                ImGui::Spacing();
                if (app.exp_msg.empty()) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                                       "Saved: %s", app.exp_saved_name.c_str());
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                                       "Error: %s", app.exp_msg.c_str());
                }
                ImGui::Spacing();
                if (ImGui::Button("Close", ImVec2(80.0f, 0.0f)))
                    ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// Benchmark dialog
// ---------------------------------------------------------------------------
void draw_benchmark_dialog(AppState& app)
{
    if (app.show_benchmark) {
        ImGui::SetNextWindowSize(ImVec2(520, 620), ImGuiCond_Always);
        ImGui::OpenPopup("Benchmark##dlg");
        app.show_benchmark = false;
    }
    if (ImGui::BeginPopupModal("Benchmark##dlg", nullptr, ImGuiWindowFlags_NoResize)) {
        const int hw = app.renderer.hw_concurrency;

        // Per-run state (static -- persists across frames while modal is open)
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
            app.renderer.set_thread_count(bench_ti + 1);
            app.renderer.set_avx2(bench_phase == 0);

            ViewState bvs;   // Mandelbrot, center (-0.5,0), width 3.5, 256 iter
            bvs.center_x   = -0.5;
            bvs.view_width =  3.5;
            bench_buf.resize(1920, 1080);
            app.renderer.render(bvs, bench_buf);
            bench_sum += app.renderer.last_render_ms;
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
                        app.renderer.set_thread_count(bench_saved_tc);
                        app.renderer.set_avx2(bench_saved_a2);
                        app.dirty = true;  // restore main view
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
                bench_saved_tc = app.renderer.thread_count;
                bench_saved_a2 = app.renderer.avx2_active;
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

        // Chart -- overlay AVX2 (blue) and Scalar (orange) on same area
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

            // Scalar chart -- same Y scale so they are visually comparable
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
                app.renderer.set_thread_count(bench_saved_tc);
                app.renderer.set_avx2(bench_saved_a2);
                app.dirty = true;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// About dialog
// ---------------------------------------------------------------------------
void draw_about_dialog(AppState& app)
{
    if (app.show_about) {
        ImGui::OpenPopup("About##dlg");
        app.show_about = false;
    }
    if (ImGui::BeginPopupModal("About##dlg", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Fractal Xplorer  v1.6");
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
}
