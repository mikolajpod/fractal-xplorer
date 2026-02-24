#pragma once

struct AppState;
struct ImGuiIO;

void draw_side_panel(AppState& app, const ImGuiIO& io, float menu_h, float fh);
void draw_export_dialog(AppState& app);
void draw_benchmark_dialog(AppState& app);
void draw_about_dialog(AppState& app);
