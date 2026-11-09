// View: the dashboard drawn with Dear ImGui: side panels, top bar and the overlay on
// the 3D view. It only reads the model and reports user actions back to the caller.
#pragma once
#include <imgui.h>

#include "view/ui/style.h"

class Telemetry;

// Screen-space rectangle (ImGui coordinates, origin top-left) left free for the 3D view.
struct SceneRect { ImVec2 min, max; };

struct DashboardOptions {
    const UiFonts* fonts = nullptr;
    float scale = 1.0f;
    double time = 0.0;
    bool placeholderModel = false;        // shows a "PLACEHOLDER MODEL" badge
    const char* modelNote = "";           // problems with car.glb, shown in the 3D view
    bool sourcePillEnabled = true;        // false while the data-source window is open
};

struct DashboardResult {
    SceneRect scene;                      // where the 3D view goes
    bool sourcePillClicked = false;       // user asked to open the data-source window
};

DashboardResult DrawDashboard(const Telemetry& telemetry, const DashboardOptions& options);
