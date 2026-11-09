#include "view/ui/dashboard_view.h"

#include "model/alarms.h"
#include "model/display_limits.h"
#include "model/telemetry.h"
#include "view/ui/theme.h"
#include "view/ui/widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

#define DEG "\xC2\xB0"

using namespace theme;
using namespace ui;

// ---------------------------------------------------------------------------
// Car-specific gauges
// ---------------------------------------------------------------------------
void DriverInputBar(const Ctx& c, const ImRect& r, const char* label, float value, float max, const char* fmt,
                    ImU32 color, bool ok)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Card(dl, r, kCard, c.s);
    float cx = (r.Min.x + r.Max.x) * 0.5f;
    TextCentered(dl, c.f->label, ImVec2(cx, r.Min.y + 14 * c.s), WithAlpha(kText, 0.85f), label);
    char buf[32];
    if (ok) std::snprintf(buf, sizeof buf, fmt, value);
    TextCentered(dl, c.f->value, ImVec2(cx, r.Max.y - 17 * c.s), ok ? kText : kTextFaint, ok ? buf : kNoData);
    float bw = std::min(r.GetWidth() * 0.42f, 34 * c.s);
    ImRect bar(ImVec2(cx - bw * 0.5f, r.Min.y + 28 * c.s), ImVec2(cx + bw * 0.5f, r.Max.y - 34 * c.s));
    SegmentBar(dl, bar, ok ? value / max : 0.0f, color, c.s);
}

void SteeringGauge(const Ctx& c, const ImRect& r, float deg, bool ok)
{
    if (!ok) deg = 0.0f;
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
    TextCentered(dl, c.f->value, ImVec2(ctr.x, r.Max.y - 17 * s), ok ? kText : kTextFaint, ok ? buf : kNoData);
}

// Where a temperature bar ends and where its warning / critical marks are drawn.
struct TempScale { float warn, crit, max; };

// Horizontal temperature bar with warning / critical markers.
void TemperatureBar(const Ctx& c, const char* label, float value, TempScale th, AlarmLevel level, bool ok)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    ImRect r = Reserve(36 * s);
    if (!ok) value = 0.0f;
    if (!ok) level = AlarmLevel::Ok;
    Card(dl, r, AlarmColor(c, level), s);

    const float pad = 12 * s;
    Text(dl, c.f->label, ImVec2(r.Min.x + pad, r.Min.y + 4 * s), WithAlpha(kText, 0.9f), label);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.0f " DEG "C", value);
    TextRight(dl, c.f->body, ImVec2(r.Max.x - pad, r.Min.y + 2 * s), ok ? kText : kTextFaint, ok ? buf : kNoData);

    ImRect bar(ImVec2(r.Min.x + pad, r.Max.y - 12 * s), ImVec2(r.Max.x - pad, r.Max.y - 7 * s));
    dl->AddRectFilled(bar.Min, bar.Max, IM_COL32(255, 255, 255, 20), 3 * s);
    float t = std::clamp(value / th.max, 0.0f, 1.0f);
    // On an amber/red tile the fill is drawn white so it stays readable.
    ImU32 col = level == AlarmLevel::Ok ? Lerp(kGreen, kAmber, value / th.warn * 0.4f) : kText;
    if (t > 0.0f)
        dl->AddRectFilled(bar.Min, ImVec2(bar.Min.x + bar.GetWidth() * t, bar.Max.y), col, 3 * s);
    if (level == AlarmLevel::Ok) for (float mark : {th.warn, th.crit}) {
        float x = bar.Min.x + bar.GetWidth() * (mark / th.max);
        dl->AddLine(ImVec2(x, bar.Min.y - 3 * s), ImVec2(x, bar.Max.y + 2 * s),
                    mark == th.crit ? WithAlpha(kRed, 0.9f) : WithAlpha(kAmber, 0.9f), 1.5f * s);
    }
}

// ---------------------------------------------------------------------------
// Panels
// ---------------------------------------------------------------------------
ImU32 CarStateColor(CarState st)
{
    switch (st) {
    case CarState::Startup: return kAmber;
    case CarState::Standby: return IM_COL32(120, 160, 210, 255);
    case CarState::TSOn:    return kOrange;
    case CarState::R2D:     return kGreen;
    case CarState::Error:   return kRed;
    }
    return kText;
}

// STARTUP -> STANDBY -> TS ON -> R2D. With ok == false nothing is highlighted;
// ERROR outlines every step in pulsing red.
void CarStateStepper(const Ctx& c, CarState state, bool ok)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    ImRect r = Reserve(40 * s);
    const CarState states[4] = {CarState::Startup, CarState::Standby, CarState::TSOn, CarState::R2D};
    const float gap = 12 * s;
    ImRect cols[4];
    Columns(r, gap, {1.2f, 1.2f, 1.0f, 0.8f}, cols);
    const bool error = ok && state == CarState::Error;
    const int current = ok && !error ? static_cast<int>(state) : -1;

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
            const ImU32 border = error         ? WithAlpha(kRed, 0.4f + 0.6f * c.pulse())
                                 : i < current ? WithAlpha(col, 0.6f)
                                               : WithAlpha(kBorder, 0.7f);
            dl->AddRect(cr.Min, cr.Max, border, 9 * s, 0, (error ? 2.0f : 1.2f) * s);
            TextCentered(dl, c.f->label, cr.GetCenter(), i < current ? WithAlpha(col, 0.9f) : kTextFaint,
                         ToString(states[i]));
        }
    }
}

void LowVoltageBar(const Ctx& c, float volts, bool ok)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    ImRect r = Reserve(54 * s);
    const float vmin = limits::lv_voltage_min, vmax = limits::lv_voltage_max;
    const AlarmLevel level = ok ? LvVoltageAlarm(volts) : AlarmLevel::Ok;
    Card(dl, r, AlarmColor(c, level), s);

    const float pad = 12 * s;
    Text(dl, c.f->label, ImVec2(r.Min.x + pad, r.Min.y + 8 * s), WithAlpha(kText, 0.85f), "LV BATTERY");
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f V", volts);
    TextRight(dl, c.f->value, ImVec2(r.Max.x - pad, r.Min.y + 4 * s), ok ? kText : kTextFaint, ok ? buf : kNoData);

    ImRect bar(ImVec2(r.Min.x + pad, r.Max.y - 18 * s), ImVec2(r.Max.x - pad, r.Max.y - 12 * s));
    auto xOf = [&](float v) { return bar.Min.x + (std::clamp(v, vmin, vmax) - vmin) / (vmax - vmin) * bar.GetWidth(); };
    struct Zone { float a, b; ImU32 col; };
    const Zone zones[] = {{vmin, limits::lv_voltage_crit, kRed},
                          {limits::lv_voltage_crit, limits::lv_voltage_warn, kAmber},
                          {limits::lv_voltage_warn, vmax, kGreen}};
    // On an alarm-coloured tile the zones would clash, so they are drawn neutral.
    for (const Zone& z : zones)
        dl->AddRectFilled(ImVec2(xOf(z.a) + 1, bar.Min.y), ImVec2(xOf(z.b) - 1, bar.Max.y),
                          level != AlarmLevel::Ok ? IM_COL32(255, 255, 255, 40) : WithAlpha(z.col, 0.45f), 2 * s);
    for (int v = static_cast<int>(vmin); v <= static_cast<int>(vmax); ++v) {
        float x = xOf(static_cast<float>(v));
        std::snprintf(buf, sizeof buf, "%d", v);
        ImVec2 sz = TextSize(c.f->label, buf);
        float tx = std::clamp(x - sz.x * 0.5f, bar.Min.x, bar.Max.x - sz.x);
        dl->AddText(c.f->label, c.f->label->FontSize * 0.8f, ImVec2(tx, bar.Max.y + 1 * s), kTextFaint, buf);
    }
    if (!ok) return;
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
void MotorCurrentBar(const Ctx& c, float amps, bool ok)
{
    if (!ok) amps = 0.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    ImRect r = Reserve(44 * s);
    Card(dl, r, kCard, s);

    const float pad = 12 * s;
    Text(dl, c.f->label, ImVec2(r.Min.x + pad, r.Min.y + 5 * s), WithAlpha(kText, 0.85f), "MOTOR CURRENT");
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.0f A", amps);
    TextRight(dl, c.f->body, ImVec2(r.Max.x - pad, r.Min.y + 3 * s), ok ? kText : kTextFaint, ok ? buf : kNoData);

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
    CarStateStepper(c, f.carState, f.has(FrameField::CarState));
    ImGui::Dummy(ImVec2(0, 2 * s));
    LowVoltageBar(c, f.lvVoltage, f.has(FrameField::LvVoltage));

    ImGui::Dummy(ImVec2(0, 4 * s));
    SectionHeader(c, "DRIVER INPUTS");
    {
        ImRect row = Reserve(138 * s), col[3];
        Columns(row, gap, {1.0f, 1.0f, 1.55f}, col);
        DriverInputBar(c, col[0], "APPS %", f.appsPct, 100.0f, "%.0f", kGreen, f.has(FrameField::Apps));
        DriverInputBar(c, col[1], "BPS bar", f.bpsBar, 100.0f, "%.0f", kRed, f.has(FrameField::BpsBar));
        SteeringGauge(c, col[2], f.steeringDeg, f.has(FrameField::SteeringDeg));
    }

    ImGui::Dummy(ImVec2(0, 4 * s));
    SectionHeader(c, "TEMPERATURES " DEG "C");
    const TempScale motorTh{limits::mot_temp_warn, limits::mot_temp_crit, 130};
    const TempScale cellTh{limits::bat_temp_warn, limits::bat_temp_crit, 70};
    const TempScale invTh{limits::inv_temp_warn, limits::inv_temp_crit, 100};
    TemperatureBar(c, "MOTOR", f.motorTemp, motorTh, MotorTempAlarm(f.motorTemp), f.has(FrameField::MotorTemp));
    ImGui::Dummy(ImVec2(0, 1 * s));
    TemperatureBar(c, "MAX CELL", f.maxCellTemp, cellTh, CellTempAlarm(f.maxCellTemp), f.has(FrameField::MaxCellTemp));
    ImGui::Dummy(ImVec2(0, 1 * s));
    TemperatureBar(c, "INVERTER", f.inverterTemp, invTh, InverterTempAlarm(f.inverterTemp), f.has(FrameField::InverterTemp));
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
        case TSState::Off:       tsBg = kCard; break;
        case TSState::Precharge: tsBg = Lerp(kAmber, kCard, c.pulse() * 0.6f); break;
        case TSState::On:        tsBg = Lerp(kOrange, IM_COL32(170, 70, 20, 255), 0.35f); break;
        case TSState::Error:     tsBg = AlarmColor(c, AlarmLevel::Critical); break;
        }
        const bool tsOk = f.has(FrameField::TsState), benderOk = f.has(FrameField::Bender);
        ValueTile(c, col[0], "TS STATE", tsOk ? ToString(f.tsState) : kNoData, tsOk ? tsBg : kCard);
        const AlarmLevel benderLevel = benderOk ? BenderAlarm(f.bender) : AlarmLevel::Ok;
        ValueTile(c, col[1], "BENDER", benderOk ? ToString(f.bender) : kNoData,
                  AlarmColor(c, benderLevel));
    }
    ImGui::Dummy(ImVec2(0, 2 * s));
    {
        ImRect row = Reserve(64 * s), col[3];
        Columns(row, gap, {1.0f, 1.0f, 1.2f}, col);
        const bool packOk = f.has(FrameField::PackV), socOk = f.has(FrameField::SocPct),
                   ubOk = f.has(FrameField::UnbalancePct);
        std::snprintf(buf, sizeof buf, "%.0f", f.packV);
        MeterTile(c, col[0], "PACK V", packOk ? buf : kNoData, packOk ? (f.packV - 400.0f) / 200.0f : 0.0f, kAccent);
        std::snprintf(buf, sizeof buf, "%.0f", f.socPct);
        const AlarmLevel socLevel = socOk ? SocAlarm(f.socPct) : AlarmLevel::Ok;
        MeterTile(c, col[1], "SOC %", socOk ? buf : kNoData, socOk ? f.socPct / 100.0f : 0.0f,
                  socLevel != AlarmLevel::Ok ? kText : kGreen, AlarmColor(c, socLevel));
        std::snprintf(buf, sizeof buf, "%.1f", f.unbalancePct);
        const AlarmLevel ubLevel = ubOk ? UnbalanceAlarm(f.unbalancePct) : AlarmLevel::Ok;
        MeterTile(c, col[2], "UNBALANCE %", ubOk ? buf : kNoData, ubOk ? f.unbalancePct / 10.0f : 0.0f,
                  ubLevel != AlarmLevel::Ok ? kText : kGreen, AlarmColor(c, ubLevel));
    }
    ImGui::Dummy(ImVec2(0, 2 * s));
    MotorCurrentBar(c, f.motorCurrentA, f.has(FrameField::MotorCurrentA));
    (void)h;
}

void SettingsGroup(const Ctx& c, const TelemetryFrame& f)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = c.s;
    const CarSettings& st = f.settings;
    const FrameField fields[CarSettings::kCount] = {FrameField::PowerLimitKw, FrameField::RpmLimit,
                                                    FrameField::MotorCooling, FrameField::InverterCooling,
                                                    FrameField::WheelSizeMm,  FrameField::MaxRecuperationPct,
                                                    FrameField::FrequencyProfile};
    char values[CarSettings::kCount][32];
    std::snprintf(values[0], 32, "%.0f kW", st.powerLimitKw);
    std::snprintf(values[1], 32, "%d rpm", st.rpmLimit);
    // Cooling = pump override from MPCU_Settings: -1 means automatic control.
    auto cooling = [](char* out, int pct) {
        if (pct < 0) std::snprintf(out, 32, "AUTO");
        else std::snprintf(out, 32, "%d %%", pct);
    };
    cooling(values[2], st.motorCooling);
    cooling(values[3], st.inverterCooling);
    std::snprintf(values[4], 32, "%d mm", st.wheelSizeMm);
    std::snprintf(values[5], 32, "%d %%", st.maxRecuperationPct);
    // LOW switching frequency gives more torque, HIGH less (MPCU firmware comment).
    switch (st.frequencyProfile) {
    case FrequencyProfile::Low:  std::snprintf(values[6], 32, "LOW (HIGH TQ)"); break;
    case FrequencyProfile::High: std::snprintf(values[6], 32, "HIGH (LOW TQ)"); break;
    default:                     std::snprintf(values[6], 32, "%s", ToString(st.frequencyProfile)); break;
    }
    for (int i = 0; i < CarSettings::kCount; ++i)
        if (!f.has(fields[i])) std::snprintf(values[i], 32, "%s", kNoData);
    const char* labels[CarSettings::kCount] = {"POWER LIMIT", "RPM LIMIT", "MOTOR COOLING", "INVERTER COOLING",
                                               "WHEEL SIZE", "MAX RECUPERATION", "FREQUENCY PROFILE"};

    // Flash a row briefly when its value changes.
    static std::string previous[CarSettings::kCount];
    static double changedAt[CarSettings::kCount] = {-10, -10, -10, -10, -10, -10, -10};

    for (int i = 0; i < CarSettings::kCount; ++i) {
        if (!previous[i].empty() && previous[i] != values[i]) changedAt[i] = c.time;
        previous[i] = values[i];

        ImRect r = Reserve(44 * s);
        float flash = std::clamp(1.0f - static_cast<float>(c.time - changedAt[i]) / 1.5f, 0.0f, 1.0f);
        Card(dl, r, Lerp(kCard, kAccent, flash * 0.45f), s);
        float midY = (r.Min.y + r.Max.y) * 0.5f;
        Text(dl, c.f->body, ImVec2(r.Min.x + 16 * s, std::floor(midY - c.f->body->FontSize * 0.5f)),
             WithAlpha(kText, 0.8f), labels[i]);
        TextRight(dl, c.f->value, ImVec2(r.Max.x - 14 * s, std::floor(midY - c.f->value->FontSize * 0.5f)),
                  f.has(fields[i]) ? kText : kTextFaint, values[i]);
        ImGui::Dummy(ImVec2(0, 1 * s));
    }
}

void RightPanel(const Ctx& c, const TelemetryFrame& f, const TelemetryHistory& h)
{
    const float s = c.s;
    char buf[64];
    SectionHeader(c, "SETTINGS");
    SettingsGroup(c, f);

    ImGui::Dummy(ImVec2(0, 4 * s));
    SectionHeader(c, "VEHICLE", "last 30 s");
    std::snprintf(buf, sizeof buf, "%.0f km/h", f.speedKmh);
    if (!f.has(FrameField::SpeedKmh)) std::snprintf(buf, sizeof buf, "%s", kNoData);
    Chart(c, 104 * s, "SPEED", buf, {{&h.speed, kAccent, true}}, 0, 130);
    ImGui::Dummy(ImVec2(0, 2 * s));
    std::snprintf(buf, sizeof buf, "%.0f rpm", f.motorRpm);
    if (!f.has(FrameField::MotorRpm)) std::snprintf(buf, sizeof buf, "%s", kNoData);
    Chart(c, 104 * s, "MOTOR RPM", buf, {{&h.rpm, kViolet, true}}, 0, 6500,
          f.has(FrameField::RpmLimit) ? static_cast<float>(f.settings.rpmLimit) : NAN,
          f.has(FrameField::RpmLimit) ? "- - limit" : nullptr);

    ImGui::Dummy(ImVec2(0, 4 * s));
    SectionHeader(c, "PEDALS", "last 30 s");
    char apps[16], bps[16];
    std::snprintf(apps, sizeof apps, "%.0f%%", f.appsPct);
    std::snprintf(bps, sizeof bps, "%.0f bar", f.bpsBar);
    std::snprintf(buf, sizeof buf, "%s  /  %s", f.has(FrameField::Apps) ? apps : kNoData,
                  f.has(FrameField::BpsBar) ? bps : kNoData);
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

// Returns true when the data-source pill was clicked.
bool TopBar(const Ctx& c, const Telemetry& t, float width, float height, bool interactive)
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

    // Data-source pill; clicking it opens the data-source window.
    const bool ok = t.source().connected();
    const char* src = t.source().name();
    ImVec2 sz = TextSize(c.f->label, src);
    ImRect pill(ImVec2(x - sz.x - 50 * s, midY - 13 * s), ImVec2(x, midY + 13 * s));
    // clip=false: outside any window the default clip rect is ImGui's hidden fallback window.
    const bool hovered = interactive && ImGui::IsMouseHoveringRect(pill.Min, pill.Max, false) &&
                         !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImU32 col = ok ? kGreen : kRed;
    dl->AddRectFilled(pill.Min, pill.Max, WithAlpha(col, hovered ? 0.26f : 0.12f), 13 * s);
    dl->AddRect(pill.Min, pill.Max, WithAlpha(col, 0.5f), 13 * s, 0, 1.2f * s);
    ImVec2 dot(pill.Min.x + 14 * s, midY);
    dl->AddCircleFilled(dot, (4.0f + 3.0f * c.pulse()) * s, WithAlpha(col, 0.25f));
    dl->AddCircleFilled(dot, 4 * s, col);
    Text(dl, c.f->label, ImVec2(pill.Min.x + 26 * s, midY - sz.y * 0.5f), kText, src);
    // Chevron hinting that the pill opens a menu.
    const float cx = pill.Max.x - 15 * s;
    dl->AddTriangleFilled(ImVec2(cx - 4 * s, midY - 2 * s), ImVec2(cx + 4 * s, midY - 2 * s), ImVec2(cx, midY + 3 * s),
                          hovered ? kText : kTextDim);
    if (!ok) {
        const char* hint = "NO DATA";
        TextRight(dl, c.f->label, ImVec2(pill.Min.x - 10 * s, midY - c.f->label->FontSize * 0.5f), kRed, hint);
    }
    return hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

void SceneOverlay(const Ctx& c, const TelemetryFrame& f, const SceneRect& r, bool placeholder, const char* modelNote)
{
    const float s = c.s;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float cx = (r.min.x + r.max.x) * 0.5f;
    char buf[32];

    // Speed read-out and RPM bar at the bottom of the 3D view.
    std::snprintf(buf, sizeof buf, "%.0f", f.speedKmh);
    if (!f.has(FrameField::SpeedKmh)) std::snprintf(buf, sizeof buf, "%s", kNoData);
    const float speedY = r.max.y - 118 * s;
    ImVec2 sz = TextSize(c.f->huge, buf);
    dl->AddText(c.f->huge, c.f->huge->FontSize, ImVec2(cx - sz.x * 0.5f + 2 * s, speedY + 3 * s),
                IM_COL32(0, 0, 0, 120), buf);
    Text(dl, c.f->huge, ImVec2(cx - sz.x * 0.5f, speedY), f.has(FrameField::SpeedKmh) ? kText : kTextFaint, buf);
    TextCentered(dl, c.f->label, ImVec2(cx, speedY + sz.y + 4 * s), kTextDim, "KM/H");

    const float bw = std::min(360 * s, (r.max.x - r.min.x) * 0.7f);
    ImRect bar(ImVec2(cx - bw * 0.5f, r.max.y - 22 * s), ImVec2(cx + bw * 0.5f, r.max.y - 16 * s));
    const float limit = static_cast<float>(f.settings.rpmLimit);
    const bool rpmOk = f.has(FrameField::MotorRpm);
    const float t = rpmOk ? std::clamp(f.motorRpm / 6500.0f, 0.0f, 1.0f) : 0.0f;
    const float lt = std::clamp(limit / 6500.0f, 0.0f, 1.0f);
    dl->AddRectFilled(bar.Min, bar.Max, IM_COL32(255, 255, 255, 20), 3 * s);
    if (t > 0.0f) {
        const AlarmLevel level = RpmAlarm(f.motorRpm, limit);
        ImU32 col = level == AlarmLevel::Critical ? kRed : level == AlarmLevel::Warning ? kAmber : kAccent;
        dl->AddRectFilled(bar.Min, ImVec2(bar.Min.x + bw * t, bar.Max.y), col, 3 * s);
    }
    if (f.has(FrameField::RpmLimit)) {   // limit marker only when the car reports it
        float lx = bar.Min.x + bw * lt;
        dl->AddLine(ImVec2(lx, bar.Min.y - 4 * s), ImVec2(lx, bar.Max.y + 4 * s), kRed, 2 * s);
    }
    std::snprintf(buf, sizeof buf, "%.0f RPM", f.motorRpm);
    if (!rpmOk) std::snprintf(buf, sizeof buf, "%s RPM", kNoData);
    Text(dl, c.f->label, ImVec2(bar.Min.x, bar.Min.y - c.f->label->FontSize - 4 * s), kTextDim, buf);

    // Hints / badges.
    const char* hint = "DRAG TO ROTATE";
    TextRight(dl, c.f->label, ImVec2(r.max.x - 8 * s, r.min.y + 12 * s), kTextFaint, hint);
    float badgeY = r.min.y + 12 * s;
    if (placeholder) {
        Text(dl, c.f->label, ImVec2(r.min.x + 8 * s, badgeY), kTextFaint, "PLACEHOLDER MODEL");
        badgeY += c.f->label->FontSize + 4 * s;
    }
    if (modelNote[0]) {
        dl->AddText(c.f->label, c.f->label->FontSize, ImVec2(r.min.x + 8 * s, badgeY), kAmber, modelNote, nullptr,
                    r.max.x - r.min.x - 16 * s);
    }

    // Big R2D / state banner when not ready to drive.
    if (f.has(FrameField::CarState) && f.carState != CarState::R2D) {
        const char* msg = f.carState == CarState::Startup   ? "STARTING UP"
                          : f.carState == CarState::Standby ? "STANDBY"
                          : f.carState == CarState::TSOn    ? "TRACTIVE SYSTEM ON"
                                                            : "VEHICLE ERROR";
        ImU32 col = CarStateColor(f.carState);
        ImVec2 ts = TextSize(c.f->value, msg);
        ImRect b(ImVec2(cx - ts.x * 0.5f - 18 * s, r.min.y + 40 * s),
                 ImVec2(cx + ts.x * 0.5f + 18 * s, r.min.y + 40 * s + ts.y + 14 * s));
        dl->AddRectFilled(b.Min, b.Max, WithAlpha(col, 0.15f + 0.1f * c.pulse()), 8 * s);
        dl->AddRect(b.Min, b.Max, WithAlpha(col, 0.8f), 8 * s, 0, 1.5f * s);
        TextCentered(dl, c.f->value, b.GetCenter(), col, msg);
    }
}

} // namespace

DashboardResult DrawDashboard(const Telemetry& t, const DashboardOptions& o)
{
    const float s = o.scale;
    const Ctx c{o.fonts, s, o.time};
    DashboardResult result;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float topH = 56 * s;
    const float margin = 12 * s;
    const float sideW = std::clamp(display.x * 0.26f, 330 * s, 460 * s);
    const float panelH = display.y - topH - margin * 2;

    result.sourcePillClicked = TopBar(c, t, display.x, topH, o.sourcePillEnabled);

    BeginSidePanel("##left", ImVec2(margin, topH + margin), ImVec2(sideW, panelH), s);
    LeftPanel(c, t.frame(), t.history());
    EndSidePanel();

    BeginSidePanel("##right", ImVec2(display.x - margin - sideW, topH + margin), ImVec2(sideW, panelH), s);
    RightPanel(c, t.frame(), t.history());
    EndSidePanel();

    result.scene = {ImVec2(margin * 2 + sideW, topH), ImVec2(display.x - margin * 2 - sideW, display.y - margin)};
    SceneOverlay(c, t.frame(), result.scene, o.placeholderModel, o.modelNote);
    return result;
}
