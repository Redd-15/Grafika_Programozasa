// View: generic ImGui drawing helpers used by the dashboard: text placement, layout,
// cards, tiles, LED bars and line charts. Nothing car-specific.
#pragma once
#include <cmath>
#include <initializer_list>

#include <imgui.h>
#include <imgui_internal.h>   // ImRect

#include "model/alarms.h"
#include "model/telemetry_history.h"
#include "view/ui/style.h"
#include "view/ui/theme.h"

namespace ui {

constexpr float kPi = 3.14159265358979f;
constexpr const char* kNoData = "--";   // shown for values the source does not provide

ImU32 WithAlpha(ImU32 c, float a);
ImU32 Lerp(ImU32 a, ImU32 b, float t);

// Shared per-frame drawing context.
struct Ctx {
    const UiFonts* f;
    float s;         // UI scale
    double time;
    float pulse() const { return 0.5f + 0.5f * std::sin(static_cast<float>(time) * 6.0f); }
};

// Text
ImVec2 TextSize(ImFont* font, const char* text);
void Text(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, const char* text);
void TextCentered(ImDrawList* dl, ImFont* font, ImVec2 center, ImU32 col, const char* text);
void TextRight(ImDrawList* dl, ImFont* font, ImVec2 rightTop, ImU32 col, const char* text);

// Layout: reserves a full-width block of `height` in the current window and returns its rect.
ImRect Reserve(float height);

// Splits a rect horizontally into N columns with the given weights.
template <int N>
void Columns(const ImRect& r, float gap, const float (&weights)[N], ImRect (&out)[N])
{
    float total = 0;
    for (float w : weights) total += w;
    float avail = r.GetWidth() - gap * (N - 1), x = r.Min.x;
    for (int i = 0; i < N; ++i) {
        float w = avail * weights[i] / total;
        out[i] = ImRect(ImVec2(x, r.Min.y), ImVec2(x + w, r.Max.y));
        x += w + gap;
    }
}

// Building blocks
void SectionHeader(const Ctx& c, const char* title, const char* right = nullptr);
void Card(ImDrawList* dl, const ImRect& r, ImU32 bg, float s, ImU32 border = 0);
// Tile as on the car's dash: label on top, big value below.
void ValueTile(const Ctx& c, const ImRect& r, const char* label, const char* value, ImU32 bg);
// Background colour for an alarm level (critical pulses).
ImU32 AlarmColor(const Ctx& c, AlarmLevel level, ImU32 okColor = theme::kCard);
// LED-style vertical bar (APPS / BPS).
void SegmentBar(ImDrawList* dl, const ImRect& r, float v01, ImU32 color, float s);

// Line chart over the last 30 s; NaN samples leave a gap.
struct Series {
    const Trace* data;
    ImU32 color;
    bool fill;
};
void Chart(const Ctx& c, float height, const char* title, const char* value, std::initializer_list<Series> series,
           float vmin, float vmax, float refLine = NAN, const char* legend = nullptr);

} // namespace ui
