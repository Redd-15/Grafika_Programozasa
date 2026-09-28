#include "ui.h"

#include "display_limits.h"
#include "telemetry.h"

#include <imgui_internal.h>   // ImRect

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

constexpr float kPi = 3.14159265358979f;
#define DEG "\xC2\xB0"

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
const ImU32 kPanel    = IM_COL32(7, 11, 19, 225);
const ImU32 kCard     = IM_COL32(13, 37, 63, 255);
const ImU32 kCardHi   = IM_COL32(20, 54, 90, 255);
const ImU32 kBorder   = IM_COL32(38, 74, 115, 255);
const ImU32 kText     = IM_COL32(236, 242, 250, 255);
const ImU32 kTextDim  = IM_COL32(138, 162, 188, 255);
const ImU32 kTextFaint= IM_COL32(90, 110, 135, 255);
const ImU32 kAccent   = IM_COL32(63, 169, 245, 255);
const ImU32 kGreen    = IM_COL32(46, 204, 113, 255);
const ImU32 kAmber    = IM_COL32(245, 166, 35, 255);
const ImU32 kOrange   = IM_COL32(255, 118, 40, 255);
const ImU32 kRed      = IM_COL32(240, 71, 71, 255);
const ImU32 kViolet   = IM_COL32(155, 110, 255, 255);

ImU32 WithAlpha(ImU32 c, float a)
{
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
    v.w *= a;
    return ImGui::ColorConvertFloat4ToU32(v);
}

ImU32 Lerp(ImU32 a, ImU32 b, float t)
{
    ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    t = std::clamp(t, 0.0f, 1.0f);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t,
                                                 x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t));
}

// Shared per-frame drawing context.
struct Ctx {
    const UiFonts* f;
    float s;         // UI scale
    double time;
    float pulse() const { return 0.5f + 0.5f * std::sin(static_cast<float>(time) * 6.0f); }
};

// ---------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------
ImVec2 TextSize(ImFont* font, const char* text)
{
    return font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text);
}

void Text(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, const char* text)
{
    dl->AddText(font, font->FontSize, pos, col, text);
}

void TextCentered(ImDrawList* dl, ImFont* font, ImVec2 center, ImU32 col, const char* text)
{
    ImVec2 sz = TextSize(font, text);
    Text(dl, font, ImVec2(std::floor(center.x - sz.x * 0.5f), std::floor(center.y - sz.y * 0.5f)), col, text);
}

void TextRight(ImDrawList* dl, ImFont* font, ImVec2 rightTop, ImU32 col, const char* text)
{
    ImVec2 sz = TextSize(font, text);
    Text(dl, font, ImVec2(std::floor(rightTop.x - sz.x), rightTop.y), col, text);
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------
// Reserves a full-width block of `height` in the current window, returns its rect.
ImRect Reserve(float height)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(w, height));
    return ImRect(p, ImVec2(p.x + w, p.y + height));
}

// Splits a rect horizontally into `n` columns with the given weights.
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

void SectionHeader(const Ctx& c, const char* title, const char* right = nullptr)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImRect r = Reserve(21 * c.s);
    Text(dl, c.f->label, ImVec2(r.Min.x + 2 * c.s, r.Min.y + 2 * c.s), kTextDim, title);
    float tx = r.Min.x + TextSize(c.f->label, title).x + 12 * c.s;
    float rx = r.Max.x;
    if (right) {
        TextRight(dl, c.f->label, ImVec2(r.Max.x, r.Min.y + 2 * c.s), kTextFaint, right);
        rx -= TextSize(c.f->label, right).x + 10 * c.s;
    }
    float ly = std::floor(r.Min.y + 2 * c.s + c.f->label->FontSize * 0.55f);
    if (rx > tx)
        dl->AddRectFilledMultiColor(ImVec2(tx, ly), ImVec2(rx, ly + 1), WithAlpha(kAccent, 0.45f), 0, 0,
                                    WithAlpha(kAccent, 0.45f));
}

void Card(ImDrawList* dl, const ImRect& r, ImU32 bg, float s, ImU32 border = 0)
{
    dl->AddRectFilled(r.Min, r.Max, bg, 9 * s);
    // Subtle top highlight for depth.
    dl->AddRectFilledMultiColor(ImVec2(r.Min.x + 6 * s, r.Min.y + 1), ImVec2(r.Max.x - 6 * s, r.Min.y + 2),
                                IM_COL32(255, 255, 255, 18), IM_COL32(255, 255, 255, 18),
                                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    if (border) dl->AddRect(r.Min, r.Max, border, 9 * s, 0, 1.5f * s);
}

// Tile as on the car's dash: label on top, big value below.
void ValueTile(const Ctx& c, const ImRect& r, const char* label, const char* value, ImU32 bg)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Card(dl, r, bg, c.s);
    float cx = (r.Min.x + r.Max.x) * 0.5f, h = r.GetHeight();
    TextCentered(dl, c.f->label, ImVec2(cx, r.Min.y + h * 0.32f), WithAlpha(kText, 0.85f), label);
    TextCentered(dl, c.f->value, ImVec2(cx, r.Min.y + h * 0.66f), kText, value);
}

// Background colour for an alarm level: 0 ok, 1 warning, 2 critical (pulsing).
ImU32 AlarmColor(const Ctx& c, int level, ImU32 okColor = kCard)
{
    if (level >= 2) return Lerp(IM_COL32(150, 30, 34, 255), kRed, c.pulse());
    if (level == 1) return Lerp(kAmber, IM_COL32(200, 120, 20, 255), 0.35f);
    return okColor;
}

// ---------------------------------------------------------------------------
// Gauges
// ---------------------------------------------------------------------------
// LED-style vertical bar (APPS / BPS).
void SegmentBar(ImDrawList* dl, const ImRect& r, float v01, ImU32 color, float s)
{
    const int n = 18;
    const float gap = 2.5f * s;
    const float segH = (r.GetHeight() - gap * (n - 1)) / n;
    v01 = std::clamp(v01, 0.0f, 1.0f);
    for (int i = 0; i < n; ++i) {
        float y1 = r.Max.y - i * (segH + gap);
        ImRect seg(ImVec2(r.Min.x, y1 - segH), ImVec2(r.Max.x, y1));
        float fill = std::clamp(v01 * n - i, 0.0f, 1.0f);
        dl->AddRectFilled(seg.Min, seg.Max, IM_COL32(255, 255, 255, 14), 2 * s);
        if (fill > 0.0f) {
            ImU32 lit = Lerp(WithAlpha(color, 0.55f), color, static_cast<float>(i) / n + 0.3f);
            dl->AddRectFilled(seg.Min, seg.Max, WithAlpha(lit, 0.25f + 0.75f * fill), 2 * s);
        }
    }
}

void DriverInputBar(const Ctx& c, const ImRect& r, const char* label, float value, float max, const char* fmt,
                    ImU32 color)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Card(dl, r, kCard, c.s);
    float cx = (r.Min.x + r.Max.x) * 0.5f;
    TextCentered(dl, c.f->label, ImVec2(cx, r.Min.y + 14 * c.s), WithAlpha(kText, 0.85f), label);
    char buf[32];
    std::snprintf(buf, sizeof buf, fmt, value);
    TextCentered(dl, c.f->value, ImVec2(cx, r.Max.y - 17 * c.s), kText, buf);
    float bw = std::min(r.GetWidth() * 0.42f, 34 * c.s);
    ImRect bar(ImVec2(cx - bw * 0.5f, r.Min.y + 28 * c.s), ImVec2(cx + bw * 0.5f, r.Max.y - 34 * c.s));
    SegmentBar(dl, bar, value / max, color, c.s);
}

void SteeringGauge(const Ctx& c, const ImRect& r, float deg)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Card(dl, r, kCard, c.s);
    const float s = c.s;
    TextCentered(dl, c.f->label, ImVec2((r.Min.x + r.Max.x) * 0.5f, r.Min.y + 14 * s), WithAlpha(kText, 0.85f),
                 "STEERING " DEG);

    const ImVec2 ctr((r.Min.x + r.Max.x) * 0.5f, r.Min.y + 28 * s + (r.GetHeight() - 62 * s) * 0.5f + 4 * s);
    const float rad = std::min(r.GetWidth() * 0.36f, (r.GetHeight() - 56 * s) * 0.5f);
    const float range = 135.0f;
    const float top = -kPi * 0.5f;
    const float span = range / 180.0f * kPi;

    // Scale arc and ticks.
    dl->PathArcTo(ctr, rad, top - span, top + span, 48);
    dl->PathStroke(IM_COL32(255, 255, 255, 22), 0, 5 * s);
    for (int t = -3; t <= 3; ++t) {
        float a = top + span * t / 3.0f;
        ImVec2 d(std::cos(a), std::sin(a));
        float inner = rad + 5 * s, outer = rad + (t == 0 ? 11 : 8) * s;
        dl->AddLine(ImVec2(ctr.x + d.x * inner, ctr.y + d.y * inner), ImVec2(ctr.x + d.x * outer, ctr.y + d.y * outer),
                    t == 0 ? kTextDim : kTextFaint, 1.5f * s);
    }

    // Active arc from centre to current angle.
    const float clamped = std::clamp(deg, -range, range);
    const float a = top + clamped / 180.0f * kPi;
    if (std::abs(clamped) > 0.5f) {
        dl->PathArcTo(ctr, rad, std::min(top, a), std::max(top, a), 32);
        dl->PathStroke(WithAlpha(kAccent, 0.25f), 0, 11 * s);
        dl->PathArcTo(ctr, rad, std::min(top, a), std::max(top, a), 32);
        dl->PathStroke(kAccent, 0, 5 * s);
    }

    // Miniature steering wheel rotating with the input (+ = clockwise).
    const float wr = rad * 0.62f;
    const float rot = deg / 180.0f * kPi;
    dl->AddCircle(ctr, wr, kText, 40, 4 * s);
    for (float side : {-1.0f, 1.0f}) {
        float sa = rot + (side > 0 ? 0.0f : kPi);
        dl->AddLine(ImVec2(ctr.x + std::cos(sa) * wr * 0.35f, ctr.y + std::sin(sa) * wr * 0.35f),
                    ImVec2(ctr.x + std::cos(sa) * wr, ctr.y + std::sin(sa) * wr), kText, 3.5f * s);
    }
    dl->AddCircleFilled(ctr, wr * 0.35f, kCardHi);
    dl->AddCircle(ctr, wr * 0.35f, kText, 20, 2.5f * s);
    ImVec2 mark(ctr.x + std::cos(top + rot) * wr, ctr.y + std::sin(top + rot) * wr);
    dl->AddCircleFilled(mark, 4.5f * s, kOrange);

    char buf[32];
    std::snprintf(buf, sizeof buf, "%+.0f", deg);
    if (std::abs(deg) < 0.5f) std::snprintf(buf, sizeof buf, "0");
    TextCentered(dl, c.f->value, ImVec2(ctr.x, r.Max.y - 17 * s), kText, buf);
}

struct Thresholds { float warn, crit, max; };

int Level(float v, Thresholds t) { return v >= t.crit ? 2 : v >= t.warn ? 1 : 0; }

// Horizontal temperature bar with warning / critical markers.
void TemperatureBar(const Ctx& c, const char* label, float value, Thresholds th)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    ImRect r = Reserve(36 * s);
    const int level = Level(value, th);
    Card(dl, r, AlarmColor(c, level), s);

    const float pad = 12 * s;
    Text(dl, c.f->label, ImVec2(r.Min.x + pad, r.Min.y + 4 * s), WithAlpha(kText, 0.9f), label);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.0f " DEG "C", value);
    TextRight(dl, c.f->body, ImVec2(r.Max.x - pad, r.Min.y + 2 * s), kText, buf);

    ImRect bar(ImVec2(r.Min.x + pad, r.Max.y - 12 * s), ImVec2(r.Max.x - pad, r.Max.y - 7 * s));
    dl->AddRectFilled(bar.Min, bar.Max, IM_COL32(255, 255, 255, 20), 3 * s);
    float t = std::clamp(value / th.max, 0.0f, 1.0f);
    // On an amber/red tile the fill is drawn white so it stays readable.
    ImU32 col = level == 0 ? Lerp(kGreen, kAmber, value / th.warn * 0.4f) : kText;
    if (t > 0.0f)
        dl->AddRectFilled(bar.Min, ImVec2(bar.Min.x + bar.GetWidth() * t, bar.Max.y), col, 3 * s);
    if (level == 0) for (float mark : {th.warn, th.crit}) {
        float x = bar.Min.x + bar.GetWidth() * (mark / th.max);
        dl->AddLine(ImVec2(x, bar.Min.y - 3 * s), ImVec2(x, bar.Max.y + 2 * s),
                    mark == th.crit ? WithAlpha(kRed, 0.9f) : WithAlpha(kAmber, 0.9f), 1.5f * s);
    }
}

// ---------------------------------------------------------------------------
// Charts
// ---------------------------------------------------------------------------
struct Series {
    const Trace* data;
    ImU32 color;
    bool fill;
};

void Chart(const Ctx& c, float height, const char* title, const char* value, std::initializer_list<Series> series,
           float vmin, float vmax, float refLine = NAN, const char* legend = nullptr)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    ImRect r = Reserve(height);
    Card(dl, r, kCard, s);
    Text(dl, c.f->label, ImVec2(r.Min.x + 12 * s, r.Min.y + 8 * s), WithAlpha(kText, 0.85f), title);
    if (value) TextRight(dl, c.f->value, ImVec2(r.Max.x - 12 * s, r.Min.y + 4 * s), kText, value);
    if (legend) Text(dl, c.f->label, ImVec2(r.Min.x + 12 * s, r.Min.y + 8 * s + c.f->label->FontSize), kTextFaint, legend);

    ImRect plot(ImVec2(r.Min.x + 10 * s, r.Min.y + 34 * s), ImVec2(r.Max.x - 10 * s, r.Max.y - 8 * s));
    for (int i = 0; i <= 3; ++i) {
        float y = std::floor(plot.Min.y + plot.GetHeight() * i / 3.0f) + 0.5f;
        dl->AddLine(ImVec2(plot.Min.x, y), ImVec2(plot.Max.x, y), IM_COL32(255, 255, 255, i == 3 ? 30 : 12), 1.0f);
    }
    auto yOf = [&](float v) {
        return plot.Max.y - std::clamp((v - vmin) / (vmax - vmin), 0.0f, 1.0f) * plot.GetHeight();
    };
    if (!std::isnan(refLine)) {
        float y = yOf(refLine);
        for (float x = plot.Min.x; x < plot.Max.x; x += 10 * s)
            dl->AddLine(ImVec2(x, y), ImVec2(std::min(x + 5 * s, plot.Max.x), y), WithAlpha(kRed, 0.8f), 1.5f * s);
    }

    dl->PushClipRect(plot.Min, ImVec2(plot.Max.x + 6 * s, plot.Max.y + 1), true);
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    static ImVector<ImVec2> pts;
    for (const Series& se : series) {
        const Trace& d = *se.data;
        const size_t n = d.size();
        if (n < 2) continue;
        const float dx = plot.GetWidth() / (Trace::capacity() - 1);
        const float x0 = plot.Max.x - dx * (n - 1);
        pts.resize(static_cast<int>(n));
        for (size_t i = 0; i < n; ++i) pts[static_cast<int>(i)] = ImVec2(x0 + dx * i, yOf(d[i]));

        if (se.fill) {
            // Gradient area under the curve.
            const ImU32 topCol = WithAlpha(se.color, 0.32f), botCol = WithAlpha(se.color, 0.0f);
            dl->PrimReserve(static_cast<int>(n - 1) * 6, static_cast<int>(n - 1) * 4);
            for (size_t i = 0; i + 1 < n; ++i) {
                ImVec2 a = pts[static_cast<int>(i)], b = pts[static_cast<int>(i + 1)];
                auto idx = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
                dl->PrimWriteVtx(a, uv, topCol);
                dl->PrimWriteVtx(b, uv, topCol);
                dl->PrimWriteVtx(ImVec2(b.x, plot.Max.y), uv, botCol);
                dl->PrimWriteVtx(ImVec2(a.x, plot.Max.y), uv, botCol);
                dl->PrimWriteIdx(idx);
                dl->PrimWriteIdx(static_cast<ImDrawIdx>(idx + 1));
                dl->PrimWriteIdx(static_cast<ImDrawIdx>(idx + 2));
                dl->PrimWriteIdx(idx);
                dl->PrimWriteIdx(static_cast<ImDrawIdx>(idx + 2));
                dl->PrimWriteIdx(static_cast<ImDrawIdx>(idx + 3));
            }
        }
        dl->AddPolyline(pts.Data, pts.Size, WithAlpha(se.color, 0.18f), 0, 6 * s);   // glow
        dl->AddPolyline(pts.Data, pts.Size, se.color, 0, 2 * s);
        ImVec2 last = pts[pts.Size - 1];
        dl->AddCircleFilled(last, 5 * s, WithAlpha(se.color, 0.3f));
        dl->AddCircleFilled(last, 3 * s, se.color);
    }
    dl->PopClipRect();
}

// ---------------------------------------------------------------------------
// Panels
// ---------------------------------------------------------------------------
ImU32 CarStateColor(CarState st)
{
    switch (st) {
    case CarState::Off:     return IM_COL32(120, 132, 150, 255);
    case CarState::Startup: return kAmber;
    case CarState::TSOn:    return kOrange;
    case CarState::R2D:     return kGreen;
    }
    return kText;
}

void CarStateStepper(const Ctx& c, CarState state)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    ImRect r = Reserve(40 * s);
    const CarState states[4] = {CarState::Off, CarState::Startup, CarState::TSOn, CarState::R2D};
    const float gap = 14 * s;
    ImRect cols[4];
    Columns(r, gap, {1.0f, 1.25f, 1.0f, 1.0f}, cols);
    const int current = static_cast<int>(state);

    for (int i = 0; i < 4; ++i) {
        const ImRect& cr = cols[i];
        if (i < 3) {   // connector
            float y = (cr.Min.y + cr.Max.y) * 0.5f;
            ImU32 lc = i < current ? WithAlpha(CarStateColor(states[i + 1]), 0.8f) : IM_COL32(255, 255, 255, 30);
            dl->AddLine(ImVec2(cr.Max.x + 2 * s, y), ImVec2(cols[i + 1].Min.x - 2 * s, y), lc, 2 * s);
        }
        const ImU32 col = CarStateColor(states[i]);
        if (i == current) {
            for (int g = 3; g >= 1; --g) {   // glow
                float e = g * 3.0f * s;
                dl->AddRectFilled(ImVec2(cr.Min.x - e, cr.Min.y - e), ImVec2(cr.Max.x + e, cr.Max.y + e),
                                  WithAlpha(col, 0.07f), 10 * s + e);
            }
            dl->AddRectFilled(cr.Min, cr.Max, col, 9 * s);
            TextCentered(dl, c.f->value, cr.GetCenter(), IM_COL32(8, 14, 22, 255), ToString(states[i]));
        } else {
            dl->AddRectFilled(cr.Min, cr.Max, kCard, 9 * s);
            dl->AddRect(cr.Min, cr.Max, i < current ? WithAlpha(col, 0.6f) : WithAlpha(kBorder, 0.7f), 9 * s, 0,
                        1.2f * s);
            TextCentered(dl, c.f->label, cr.GetCenter(), i < current ? WithAlpha(col, 0.9f) : kTextFaint,
                         ToString(states[i]));
        }
    }
}

void LowVoltageBar(const Ctx& c, float volts)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    ImRect r = Reserve(54 * s);
    const float vmin = limits::lv_voltage_min, vmax = limits::lv_voltage_max;
    const int level = volts < limits::lv_voltage_crit ? 2 : volts < limits::lv_voltage_warn ? 1 : 0;
    Card(dl, r, AlarmColor(c, level), s);

    const float pad = 12 * s;
    Text(dl, c.f->label, ImVec2(r.Min.x + pad, r.Min.y + 8 * s), WithAlpha(kText, 0.85f), "LV BATTERY");
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f V", volts);
    TextRight(dl, c.f->value, ImVec2(r.Max.x - pad, r.Min.y + 4 * s), kText, buf);

    ImRect bar(ImVec2(r.Min.x + pad, r.Max.y - 18 * s), ImVec2(r.Max.x - pad, r.Max.y - 12 * s));
    auto xOf = [&](float v) { return bar.Min.x + (std::clamp(v, vmin, vmax) - vmin) / (vmax - vmin) * bar.GetWidth(); };
    struct Zone { float a, b; ImU32 col; };
    const Zone zones[] = {{vmin, limits::lv_voltage_crit, kRed},
                          {limits::lv_voltage_crit, limits::lv_voltage_warn, kAmber},
                          {limits::lv_voltage_warn, vmax, kGreen}};
    // On an alarm-coloured tile the zones would clash, so they are drawn neutral.
    for (const Zone& z : zones)
        dl->AddRectFilled(ImVec2(xOf(z.a) + 1, bar.Min.y), ImVec2(xOf(z.b) - 1, bar.Max.y),
                          level ? IM_COL32(255, 255, 255, 40) : WithAlpha(z.col, 0.45f), 2 * s);
    for (int v = static_cast<int>(vmin); v <= static_cast<int>(vmax); ++v) {
        float x = xOf(static_cast<float>(v));
        std::snprintf(buf, sizeof buf, "%d", v);
        ImVec2 sz = TextSize(c.f->label, buf);
        float tx = std::clamp(x - sz.x * 0.5f, bar.Min.x, bar.Max.x - sz.x);
        dl->AddText(c.f->label, c.f->label->FontSize * 0.8f, ImVec2(tx, bar.Max.y + 1 * s), kTextFaint, buf);
    }
    // Needle
    float x = xOf(volts);
    dl->AddRectFilled(ImVec2(x - 1.5f * s, bar.Min.y - 5 * s), ImVec2(x + 1.5f * s, bar.Max.y + 3 * s), kText, 1 * s);
    dl->AddTriangleFilled(ImVec2(x - 5 * s, bar.Min.y - 10 * s), ImVec2(x + 5 * s, bar.Min.y - 10 * s),
                          ImVec2(x, bar.Min.y - 4 * s), kText);
}

// Tile with a thin progress bar under the value (pack voltage, unbalance).
void MeterTile(const Ctx& c, const ImRect& r, const char* label, const char* value, float t01, ImU32 barColor,
               ImU32 bg = kCard)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ValueTile(c, r, label, value, bg);
    ImRect bar(ImVec2(r.Min.x + 14 * c.s, r.Max.y - 13 * c.s), ImVec2(r.Max.x - 14 * c.s, r.Max.y - 9 * c.s));
    dl->AddRectFilled(bar.Min, bar.Max, IM_COL32(255, 255, 255, 22), 2 * c.s);
    t01 = std::clamp(t01, 0.0f, 1.0f);
    if (t01 > 0.0f)
        dl->AddRectFilled(bar.Min, ImVec2(bar.Min.x + bar.GetWidth() * t01, bar.Max.y), barColor, 2 * c.s);
}

// Motor current bar, symmetrical around 0 A: blue fill to the right while driving,
// green fill to the left for regen (negative current).
void MotorCurrentBar(const Ctx& c, float amps)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    ImRect r = Reserve(44 * s);
    Card(dl, r, kCard, s);

    const float pad = 12 * s;
    Text(dl, c.f->label, ImVec2(r.Min.x + pad, r.Min.y + 5 * s), WithAlpha(kText, 0.85f), "MOTOR CURRENT");
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.0f A", amps);
    TextRight(dl, c.f->body, ImVec2(r.Max.x - pad, r.Min.y + 3 * s), kText, buf);

    ImRect bar(ImVec2(r.Min.x + pad, r.Max.y - 14 * s), ImVec2(r.Max.x - pad, r.Max.y - 8 * s));
    dl->AddRectFilled(bar.Min, bar.Max, IM_COL32(255, 255, 255, 20), 3 * s);
    const float mid = (bar.Min.x + bar.Max.x) * 0.5f;
    const float t = amps >= 0 ? std::clamp(amps / limits::hv_current_max, 0.0f, 1.0f)
                              : -std::clamp(amps / limits::hv_current_min, 0.0f, 1.0f);
    const float x = mid + t * bar.GetWidth() * 0.5f;
    if (std::abs(x - mid) > 0.5f)
        dl->AddRectFilled(ImVec2(std::min(mid, x), bar.Min.y), ImVec2(std::max(mid, x), bar.Max.y),
                          amps >= 0 ? kAccent : kGreen, 3 * s);
    dl->AddLine(ImVec2(mid, bar.Min.y - 3 * s), ImVec2(mid, bar.Max.y + 3 * s), kTextDim, 1.5f * s);
    Text(dl, c.f->label, ImVec2(mid - TextSize(c.f->label, "REGEN").x - 6 * s, r.Min.y + 5 * s), kTextFaint, "REGEN");
    Text(dl, c.f->label, ImVec2(mid + 6 * s, r.Min.y + 5 * s), kTextFaint, "DRIVE");
}

void LeftPanel(const Ctx& c, const TelemetryFrame& f, const TelemetryHistory& h)
{
    const float s = c.s;
    const float gap = 8 * s;
    char buf[64];

    SectionHeader(c, "CAR STATE");
    CarStateStepper(c, f.carState);
    ImGui::Dummy(ImVec2(0, 2 * s));
    LowVoltageBar(c, f.lvVoltage);

    ImGui::Dummy(ImVec2(0, 4 * s));
    SectionHeader(c, "DRIVER INPUTS");
    {
        ImRect row = Reserve(138 * s), col[3];
        Columns(row, gap, {1.0f, 1.0f, 1.55f}, col);
        DriverInputBar(c, col[0], "APPS %", f.appsPct, 100.0f, "%.0f", kGreen);
        DriverInputBar(c, col[1], "BPS bar", f.bpsBar, 100.0f, "%.0f", kRed);
        SteeringGauge(c, col[2], f.steeringDeg);
    }

    ImGui::Dummy(ImVec2(0, 4 * s));
    SectionHeader(c, "TEMPERATURES " DEG "C");
    const Thresholds motorTh{limits::mot_temp_warn, limits::mot_temp_crit, 130};
    const Thresholds cellTh{limits::bat_temp_warn, limits::bat_temp_crit, 70};
    const Thresholds invTh{limits::inv_temp_warn, limits::inv_temp_crit, 100};
    TemperatureBar(c, "MOTOR", f.motorTemp, motorTh);
    ImGui::Dummy(ImVec2(0, 1 * s));
    TemperatureBar(c, "MAX CELL", f.maxCellTemp, cellTh);
    ImGui::Dummy(ImVec2(0, 1 * s));
    TemperatureBar(c, "INVERTER", f.inverterTemp, invTh);
    ImGui::Dummy(ImVec2(0, 1 * s));
    // The trend chart absorbs spare height; the accumulator block below needs ~220 px.
    const float trendH = std::max(56 * s, ImGui::GetContentRegionAvail().y - 222 * s);
    Chart(c, trendH, "TREND 30 s", nullptr,
          {{&h.motorTemp, kViolet, false}, {&h.inverterTemp, kAccent, false}, {&h.cellTemp, kAmber, false}}, 0, 130,
          NAN, nullptr);
    {
        // Legend in the chart header.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetItemRectMin();
        float x = ImGui::GetItemRectMax().x - 12 * s;
        const std::pair<const char*, ImU32> items[] = {{"CELL", kAmber}, {"INV", kAccent}, {"MOTOR", kViolet}};
        for (const auto& it : items) {
            ImVec2 sz = TextSize(c.f->label, it.first);
            x -= sz.x;
            Text(dl, c.f->label, ImVec2(x, p.y + 8 * s), kTextDim, it.first);
            x -= 12 * s;
            dl->AddCircleFilled(ImVec2(x + 5 * s, p.y + 8 * s + sz.y * 0.5f), 3.5f * s, it.second);
            x -= 10 * s;
        }
    }

    ImGui::Dummy(ImVec2(0, 4 * s));
    SectionHeader(c, "ACCUMULATOR");
    {
        ImRect row = Reserve(56 * s), col[2];
        Columns(row, gap, {1.0f, 1.0f}, col);
        ImU32 tsBg = kCard;
        switch (f.tsState) {
        case TSState::Idle:      tsBg = kCard; break;
        case TSState::Precharge: tsBg = Lerp(kAmber, kCard, c.pulse() * 0.6f); break;
        case TSState::Active:    tsBg = Lerp(kOrange, IM_COL32(170, 70, 20, 255), 0.35f); break;
        case TSState::Error:     tsBg = AlarmColor(c, 2); break;
        }
        ValueTile(c, col[0], "TS STATE", ToString(f.tsState), tsBg);
        int benderLevel = f.bender == BenderState::Fault ? 2 : f.bender == BenderState::Warning ? 1 : 0;
        ValueTile(c, col[1], "BENDER", ToString(f.bender), AlarmColor(c, benderLevel));
    }
    ImGui::Dummy(ImVec2(0, 2 * s));
    {
        ImRect row = Reserve(64 * s), col[3];
        Columns(row, gap, {1.0f, 1.0f, 1.2f}, col);
        std::snprintf(buf, sizeof buf, "%.0f", f.packV);
        MeterTile(c, col[0], "PACK V", buf, (f.packV - 400.0f) / 200.0f, kAccent);
        std::snprintf(buf, sizeof buf, "%.0f", f.socPct);
        const int socLevel = f.socPct < limits::hv_soc_crit ? 2 : f.socPct < limits::hv_soc_warn ? 1 : 0;
        MeterTile(c, col[1], "SOC %", buf, f.socPct / 100.0f, socLevel ? kText : kGreen, AlarmColor(c, socLevel));
        std::snprintf(buf, sizeof buf, "%.1f", f.unbalancePct);
        const int ubLevel = f.unbalancePct > 5.0f ? 2 : f.unbalancePct > 3.0f ? 1 : 0;
        MeterTile(c, col[2], "UNBALANCE %", buf, f.unbalancePct / 10.0f, ubLevel ? kText : kGreen,
                  AlarmColor(c, ubLevel));
    }
    ImGui::Dummy(ImVec2(0, 2 * s));
    MotorCurrentBar(c, f.motorCurrentA);
    (void)h;
}

void SettingsGroup(const Ctx& c, const CarSettings& st)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    char values[CarSettings::kCount][32];
    std::snprintf(values[0], 32, "%.0f kW", st.powerLimitKw);
    std::snprintf(values[1], 32, "%d rpm", st.rpmLimit);
    std::snprintf(values[2], 32, "%s", ToString(st.motorCooling));
    std::snprintf(values[3], 32, "%s", ToString(st.inverterCooling));
    std::snprintf(values[4], 32, "%d mm", st.wheelSizeMm);
    std::snprintf(values[5], 32, "%d A", st.regenMaxA);
    const char* labels[CarSettings::kCount] = {"POWER LIMIT", "RPM LIMIT", "MOTOR COOLING", "INVERTER COOLING",
                                               "WHEEL SIZE", "REGEN MAX"};

    // Flash a row briefly when its value changes.
    static std::string previous[CarSettings::kCount];
    static double changedAt[CarSettings::kCount] = {-10, -10, -10, -10, -10, -10};

    for (int i = 0; i < CarSettings::kCount; ++i) {
        if (!previous[i].empty() && previous[i] != values[i]) changedAt[i] = c.time;
        previous[i] = values[i];

        ImRect r = Reserve(44 * s);
        const bool sel = i == st.selected;
        float flash = std::clamp(1.0f - static_cast<float>(c.time - changedAt[i]) / 1.5f, 0.0f, 1.0f);
        ImU32 bg = Lerp(sel ? kCardHi : kCard, WithAlpha(kAccent, 1.0f), flash * 0.45f);
        if (sel) {
            for (int g = 2; g >= 1; --g) {
                float e = g * 2.5f * s;
                dl->AddRectFilled(ImVec2(r.Min.x - e, r.Min.y - e), ImVec2(r.Max.x + e, r.Max.y + e),
                                  WithAlpha(kAccent, 0.07f), 10 * s + e);
            }
        }
        Card(dl, r, bg, s, sel ? kAccent : 0);
        if (sel)
            dl->AddRectFilled(ImVec2(r.Min.x + 5 * s, r.Min.y + 10 * s), ImVec2(r.Min.x + 8 * s, r.Max.y - 10 * s),
                              kAccent, 2 * s);
        float midY = (r.Min.y + r.Max.y) * 0.5f;
        Text(dl, c.f->body, ImVec2(r.Min.x + 16 * s, std::floor(midY - c.f->body->FontSize * 0.5f)),
             sel ? kText : WithAlpha(kText, 0.8f), labels[i]);
        TextRight(dl, c.f->value, ImVec2(r.Max.x - 14 * s, std::floor(midY - c.f->value->FontSize * 0.5f)), kText,
                  values[i]);
        ImGui::Dummy(ImVec2(0, 1 * s));
    }
    ImRect foot = Reserve(20 * s);
    TextRight(dl, c.f->label, ImVec2(foot.Max.x - 4 * s, foot.Min.y + 2 * s), kTextDim,
              "L knob: select     R knob: change");
}

void RightPanel(const Ctx& c, const TelemetryFrame& f, const TelemetryHistory& h)
{
    const float s = c.s;
    char buf[64];
    SectionHeader(c, "SETTINGS");
    SettingsGroup(c, f.settings);

    ImGui::Dummy(ImVec2(0, 4 * s));
    SectionHeader(c, "VEHICLE", "last 30 s");
    std::snprintf(buf, sizeof buf, "%.0f km/h", f.speedKmh);
    Chart(c, 104 * s, "SPEED", buf, {{&h.speed, kAccent, true}}, 0, 130);
    ImGui::Dummy(ImVec2(0, 2 * s));
    std::snprintf(buf, sizeof buf, "%.0f rpm", f.motorRpm);
    Chart(c, 104 * s, "MOTOR RPM", buf, {{&h.rpm, kViolet, true}}, 0, 6500,
          static_cast<float>(f.settings.rpmLimit), "- - limit");

    ImGui::Dummy(ImVec2(0, 4 * s));
    SectionHeader(c, "PEDALS", "last 30 s");
    std::snprintf(buf, sizeof buf, "%.0f%%  /  %.0f bar", f.appsPct, f.bpsBar);
    Chart(c, std::max(104 * s, ImGui::GetContentRegionAvail().y - 2 * s), "APPS / BPS", buf,
          {{&h.apps, kGreen, true}, {&h.bps, kRed, true}}, 0, 100);
}

void BeginSidePanel(const char* id, ImVec2 pos, ImVec2 size, float s)
{
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(kPanel));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14 * s, 12 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * s, 4 * s));
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    // Keep a bit of spacing between the stacked widgets.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * s, 5 * s));
}

void EndSidePanel()
{
    ImGui::PopStyleVar();
    ImGui::End();
}

void TopBar(const Ctx& c, const Telemetry& t, float width, float height)
{
    const float s = c.s;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(width, height), IM_COL32(6, 10, 18, 235));
    dl->AddRectFilledMultiColor(ImVec2(0, height - 1), ImVec2(width, height + 1), WithAlpha(kAccent, 0.0f),
                                WithAlpha(kAccent, 0.8f), WithAlpha(kAccent, 0.8f), WithAlpha(kAccent, 0.0f));
    dl->AddRectFilledMultiColor(ImVec2(width * 0.5f, height - 1), ImVec2(width, height + 1),
                                WithAlpha(kAccent, 0.8f), WithAlpha(kAccent, 0.0f), WithAlpha(kAccent, 0.0f),
                                WithAlpha(kAccent, 0.8f));

    // Logo
    const float midY = height * 0.5f;
    ImRect logo(ImVec2(16 * s, midY - 15 * s), ImVec2(16 * s + 30 * s, midY + 15 * s));
    dl->AddRectFilled(logo.Min, logo.Max, kAccent, 7 * s);
    TextCentered(dl, c.f->value, logo.GetCenter(), IM_COL32(6, 12, 22, 255), "FS");
    Text(dl, c.f->large, ImVec2(logo.Max.x + 12 * s, midY - c.f->large->FontSize * 0.5f), kText, "TELEMETRY");
    float tx = logo.Max.x + 12 * s + TextSize(c.f->large, "TELEMETRY").x + 12 * s;
    Text(dl, c.f->label, ImVec2(tx, midY - c.f->label->FontSize * 0.5f + 2 * s), kTextFaint, "FORMULA STUDENT");

    // Right: connection + session clock + fps
    char buf[64];
    float x = width - 18 * s;
    std::snprintf(buf, sizeof buf, "%.0f FPS", ImGui::GetIO().Framerate);
    TextRight(dl, c.f->label, ImVec2(x, midY - c.f->label->FontSize * 0.5f), kTextFaint, buf);
    x -= TextSize(c.f->label, buf).x + 24 * s;

    int secs = static_cast<int>(c.time);
    std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60);
    TextRight(dl, c.f->body, ImVec2(x, midY - c.f->body->FontSize * 0.5f), kText, buf);
    x -= TextSize(c.f->body, buf).x + 8 * s;
    TextRight(dl, c.f->label, ImVec2(x, midY - c.f->label->FontSize * 0.5f), kTextFaint, "SESSION");
    x -= TextSize(c.f->label, "SESSION").x + 24 * s;

    const bool ok = t.source().connected();
    const char* src = t.source().name();
    ImVec2 sz = TextSize(c.f->label, src);
    ImRect pill(ImVec2(x - sz.x - 34 * s, midY - 13 * s), ImVec2(x, midY + 13 * s));
    ImU32 col = ok ? kGreen : kRed;
    dl->AddRectFilled(pill.Min, pill.Max, WithAlpha(col, 0.12f), 13 * s);
    dl->AddRect(pill.Min, pill.Max, WithAlpha(col, 0.5f), 13 * s, 0, 1.2f * s);
    ImVec2 dot(pill.Min.x + 14 * s, midY);
    dl->AddCircleFilled(dot, (4.0f + 3.0f * c.pulse()) * s, WithAlpha(col, 0.25f));
    dl->AddCircleFilled(dot, 4 * s, col);
    Text(dl, c.f->label, ImVec2(pill.Min.x + 26 * s, midY - sz.y * 0.5f), kText, src);
}

void SceneOverlay(const Ctx& c, const TelemetryFrame& f, const SceneRect& r, bool placeholder)
{
    const float s = c.s;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float cx = (r.min.x + r.max.x) * 0.5f;
    char buf[32];

    // Speed read-out and RPM bar at the bottom of the 3D view.
    std::snprintf(buf, sizeof buf, "%.0f", f.speedKmh);
    const float speedY = r.max.y - 118 * s;
    ImVec2 sz = TextSize(c.f->huge, buf);
    dl->AddText(c.f->huge, c.f->huge->FontSize, ImVec2(cx - sz.x * 0.5f + 2 * s, speedY + 3 * s),
                IM_COL32(0, 0, 0, 120), buf);
    Text(dl, c.f->huge, ImVec2(cx - sz.x * 0.5f, speedY), kText, buf);
    TextCentered(dl, c.f->label, ImVec2(cx, speedY + sz.y + 4 * s), kTextDim, "KM/H");

    const float bw = std::min(360 * s, (r.max.x - r.min.x) * 0.7f);
    ImRect bar(ImVec2(cx - bw * 0.5f, r.max.y - 22 * s), ImVec2(cx + bw * 0.5f, r.max.y - 16 * s));
    const float limit = static_cast<float>(f.settings.rpmLimit);
    const float t = std::clamp(f.motorRpm / 6500.0f, 0.0f, 1.0f);
    const float lt = std::clamp(limit / 6500.0f, 0.0f, 1.0f);
    dl->AddRectFilled(bar.Min, bar.Max, IM_COL32(255, 255, 255, 20), 3 * s);
    if (t > 0.0f) {
        ImU32 col = f.motorRpm >= limit * 0.97f ? kRed : f.motorRpm > limit * 0.85f ? kAmber : kAccent;
        dl->AddRectFilled(bar.Min, ImVec2(bar.Min.x + bw * t, bar.Max.y), col, 3 * s);
    }
    float lx = bar.Min.x + bw * lt;
    dl->AddLine(ImVec2(lx, bar.Min.y - 4 * s), ImVec2(lx, bar.Max.y + 4 * s), kRed, 2 * s);
    std::snprintf(buf, sizeof buf, "%.0f RPM", f.motorRpm);
    Text(dl, c.f->label, ImVec2(bar.Min.x, bar.Min.y - c.f->label->FontSize - 4 * s), kTextDim, buf);

    // Hints / badges.
    const char* hint = "DRAG TO ROTATE";
    TextRight(dl, c.f->label, ImVec2(r.max.x - 8 * s, r.min.y + 12 * s), kTextFaint, hint);
    if (placeholder) Text(dl, c.f->label, ImVec2(r.min.x + 8 * s, r.min.y + 12 * s), kTextFaint, "PLACEHOLDER MODEL");

    // Big R2D / state banner when not ready to drive.
    if (f.carState != CarState::R2D) {
        const char* msg = f.carState == CarState::Off       ? "VEHICLE OFF"
                          : f.carState == CarState::Startup ? "STARTING UP"
                                                            : "TRACTIVE SYSTEM ON";
        ImU32 col = CarStateColor(f.carState);
        ImVec2 ts = TextSize(c.f->value, msg);
        ImRect b(ImVec2(cx - ts.x * 0.5f - 18 * s, r.min.y + 40 * s),
                 ImVec2(cx + ts.x * 0.5f + 18 * s, r.min.y + 40 * s + ts.y + 14 * s));
        dl->AddRectFilled(b.Min, b.Max, WithAlpha(col, 0.15f + 0.1f * c.pulse()), 8 * s);
        dl->AddRect(b.Min, b.Max, WithAlpha(col, 0.8f), 8 * s, 0, 1.5f * s);
        TextCentered(dl, c.f->value, b.GetCenter(), col, msg);
    }
}

ImFont* AddFont(const char* const* candidates, float size)
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    for (const char* const* p = candidates; *p; ++p) {
        if (FILE* file = std::fopen(*p, "rb")) {
            std::fclose(file);
            if (ImFont* font = io.Fonts->AddFontFromFileTTF(*p, size, &cfg)) return font;
        }
    }
    ImFontConfig def;
    def.SizePixels = size;
    return io.Fonts->AddFontDefault(&def);
}

} // namespace

UiFonts LoadFonts(float scale)
{
    static const char* const semibold[] = {"C:/Windows/Fonts/seguisb.ttf", "C:/Windows/Fonts/segoeui.ttf",
                                           "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", nullptr};
    static const char* const bold[] = {"C:/Windows/Fonts/segoeuib.ttf",
                                       "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", nullptr};
    static const char* const display[] = {"C:/Windows/Fonts/bahnschrift.ttf", "C:/Windows/Fonts/segoeuib.ttf",
                                          "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", nullptr};
    UiFonts f;
    f.body = AddFont(semibold, 16.0f * scale);     // first font = ImGui default
    f.label = AddFont(semibold, 13.5f * scale);
    f.value = AddFont(bold, 19.0f * scale);
    f.large = AddFont(bold, 24.0f * scale);
    f.huge = AddFont(display, 76.0f * scale);
    return f;
}

void ApplyStyle(float scale)
{
    ImGuiStyle& st = ImGui::GetStyle();
    ImGui::StyleColorsDark(&st);
    st.WindowRounding = 14.0f;
    st.WindowBorderSize = 1.0f;
    st.FrameRounding = 6.0f;
    st.ScrollbarSize = 8.0f;
    st.Colors[ImGuiCol_Border] = ImVec4(0.15f, 0.28f, 0.44f, 0.55f);
    st.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(kText);
    st.ScaleAllSizes(scale);
}

SceneRect DrawDashboard(const Telemetry& t, const UiFonts& fonts, float s, double time, bool placeholderModel)
{
    const Ctx c{&fonts, s, time};
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float topH = 56 * s;
    const float margin = 12 * s;
    const float sideW = std::clamp(display.x * 0.26f, 330 * s, 460 * s);
    const float panelH = display.y - topH - margin * 2;

    TopBar(c, t, display.x, topH);

    BeginSidePanel("##left", ImVec2(margin, topH + margin), ImVec2(sideW, panelH), s);
    LeftPanel(c, t.frame(), t.history());
    EndSidePanel();

    BeginSidePanel("##right", ImVec2(display.x - margin - sideW, topH + margin), ImVec2(sideW, panelH), s);
    RightPanel(c, t.frame(), t.history());
    EndSidePanel();

    SceneRect scene{ImVec2(margin * 2 + sideW, topH), ImVec2(display.x - margin * 2 - sideW, display.y - margin)};
    SceneOverlay(c, t.frame(), scene, placeholderModel);
    return scene;
}
