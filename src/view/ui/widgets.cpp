#include "view/ui/widgets.h"

#include <algorithm>
#include <cmath>

namespace ui {

using namespace theme;

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

void SectionHeader(const Ctx& c, const char* title, const char* right)
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

void Card(ImDrawList* dl, const ImRect& r, ImU32 bg, float s, ImU32 border)
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

// Background colour for an alarm level (critical pulses).
ImU32 AlarmColor(const Ctx& c, AlarmLevel level, ImU32 okColor)
{
    if (level == AlarmLevel::Critical) return Lerp(IM_COL32(150, 30, 34, 255), kRed, c.pulse());
    if (level == AlarmLevel::Warning) return Lerp(kAmber, IM_COL32(200, 120, 20, 255), 0.35f);
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

// ---------------------------------------------------------------------------
// Charts
// ---------------------------------------------------------------------------
void Chart(const Ctx& c, float height, const char* title, const char* value, std::initializer_list<Series> series,
           float vmin, float vmax, float refLine, const char* legend)
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

        // Draw each run of valid samples separately; NaN ("no data") leaves a gap.
        size_t i = 0;
        while (i < n) {
            while (i < n && std::isnan(d[i])) ++i;
            pts.resize(0);
            for (; i < n && !std::isnan(d[i]); ++i) pts.push_back(ImVec2(x0 + dx * i, yOf(d[i])));
            if (pts.Size < 2) continue;

            if (se.fill) {
                // Gradient area under the curve.
                const ImU32 topCol = WithAlpha(se.color, 0.32f), botCol = WithAlpha(se.color, 0.0f);
                dl->PrimReserve((pts.Size - 1) * 6, (pts.Size - 1) * 4);
                for (int k = 0; k + 1 < pts.Size; ++k) {
                    ImVec2 a = pts[k], b = pts[k + 1];
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
        }
        if (!std::isnan(d[n - 1])) {   // dot on the newest sample
            ImVec2 last(plot.Max.x, yOf(d[n - 1]));
            dl->AddCircleFilled(last, 5 * s, WithAlpha(se.color, 0.3f));
            dl->AddCircleFilled(last, 3 * s, se.color);
        }
    }
    dl->PopClipRect();
}

} // namespace ui
