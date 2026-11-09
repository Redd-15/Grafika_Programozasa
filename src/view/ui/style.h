// View: fonts and the ImGui colour theme.
#pragma once
#include <imgui.h>

struct UiFonts {
    ImFont* label = nullptr;    // small caps labels
    ImFont* body = nullptr;
    ImFont* value = nullptr;    // bold tile values
    ImFont* large = nullptr;
    ImFont* huge = nullptr;     // speed read-out
};

UiFonts LoadFonts(float scale);
void ApplyStyle(float scale);
