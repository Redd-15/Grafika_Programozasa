// Dashboard UI drawn with Dear ImGui: side panels, top bar and scene overlay.
#pragma once
#include <imgui.h>

class Telemetry;

struct UiFonts {
    ImFont* label = nullptr;    // small caps labels
    ImFont* body = nullptr;
    ImFont* value = nullptr;    // bold tile values
    ImFont* large = nullptr;
    ImFont* huge = nullptr;     // speed read-out
};

UiFonts LoadFonts(float scale);
void ApplyStyle(float scale);

// Screen-space rectangle (ImGui coordinates, origin top-left) left free for the 3D view.
struct SceneRect { ImVec2 min, max; };

SceneRect DrawDashboard(const Telemetry& telemetry, const UiFonts& fonts, float scale, double time,
                        bool placeholderModel);
