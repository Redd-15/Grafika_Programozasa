#include "source_panel.h"

#include "data_source.h"
#include "telemetry.h"
#include "udp_source.h"
#include "ui.h"
#include "ui_theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <unordered_map>

using namespace theme;

namespace {

constexpr const char* kWindowId = "Data source##sources";

ImVec4 Col(ImU32 c, float a = 1.0f)
{
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
    v.w *= a;
    return v;
}

std::string Lower(std::string s)
{
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

std::string FormatValue(const SignalValue& v)
{
    if (!v.isNumber) return v.text;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", v.number);
    return buf;
}

void SectionTitle(const UiFonts& f, const char* text)
{
    ImGui::PushFont(f.label);
    ImGui::TextColored(Col(kTextDim), "%s", text);
    ImGui::PopFont();
}

// Large selectable card for a source type. Returns true when clicked.
bool SourceCard(const char* id, const char* title, const char* desc, bool selected, ImVec2 size, const UiFonts& f,
                float s)
{
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 q(p.x + size.x, p.y + size.y);
    dl->AddRectFilled(p, q, selected ? kCardHi : hovered ? IM_COL32(17, 45, 76, 255) : kCard, 10 * s);
    dl->AddRect(p, q, selected ? kAccent : IM_COL32(38, 74, 115, 160), 10 * s, 0, (selected ? 2.0f : 1.0f) * s);

    // Radio indicator
    const ImVec2 dot(p.x + 22 * s, p.y + 24 * s);
    dl->AddCircle(dot, 8 * s, selected ? kAccent : kTextDim, 24, 1.8f * s);
    if (selected) dl->AddCircleFilled(dot, 4.5f * s, kAccent);

    dl->AddText(f.value, f.value->FontSize, ImVec2(p.x + 40 * s, p.y + 24 * s - f.value->FontSize * 0.5f), kText,
                title);
    dl->AddText(f.label, f.label->FontSize, ImVec2(p.x + 40 * s, p.y + 40 * s), kTextDim, desc, nullptr);
    return clicked;
}

void StatusDot(ImU32 color, const char* tooltip)
{
    const float r = ImGui::GetTextLineHeight() * 0.22f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetFrameHeight();
    ImGui::Dummy(ImVec2(r * 2 + 2, h));
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + r + 1, p.y + h * 0.5f), r, color);
    if (tooltip && *tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
}

void ConnectionStatus(const UdpSource* udp, double timeout)
{
    if (!udp) return;
    const UdpSource::Stats st = udp->stats();
    ImU32 color = kRed;
    char line[256];
    if (!st.listening) {
        std::snprintf(line, sizeof line, "Not listening: %s", st.error.c_str());
    } else if (st.packets == 0) {
        color = kAmber;
        std::snprintf(line, sizeof line, "Listening on %s:%d, waiting for packets...", udp->config().bindAddress.c_str(),
                      udp->config().port);
    } else {
        color = st.lastPacketAge < timeout ? kGreen : kAmber;
        std::snprintf(line, sizeof line,
                      "Listening on %s:%d   |   %.0f packets/s   |   last %.2f s ago from %s   |   %llu packets, "
                      "%llu parse errors",
                      udp->config().bindAddress.c_str(), udp->config().port, st.packetsPerSec, st.lastPacketAge,
                      st.lastSender.c_str(), static_cast<unsigned long long>(st.packets),
                      static_cast<unsigned long long>(st.parseErrors));
    }
    StatusDot(color, nullptr);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(Col(color == kRed ? kRed : kText), "%s", line);
}

void MappingTable(SourceConfig& config, const Telemetry& telemetry, const UdpSource* udp, int& selected, ImVec2 size)
{
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    if (!ImGui::BeginTable("##mapping", 7, flags, size)) return;
    const float em = ImGui::GetFontSize();
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, em * 0.8f);
    ImGui::TableSetupColumn("Dashboard value", ImGuiTableColumnFlags_WidthStretch, 1.15f);
    ImGui::TableSetupColumn("CAN signal", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Scale", ImGuiTableColumnFlags_WidthFixed, em * 4.0f);
    ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, em * 4.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, em * 5.0f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
    ImGui::TableHeadersRow();

    const char* group = nullptr;
    for (int i = 0; i < kFieldCount; ++i) {
        const FrameField field = static_cast<FrameField>(i);
        const FieldInfo& info = GetFieldInfo(field);
        SignalBinding& b = config.bindings[i];
        ImGui::PushID(i);

        if (!group || std::string(group) != info.group) {
            group = info.group;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(Col(kAccent), "%s", group);
        }
        ImGui::TableNextRow();
        if (i == selected) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(63, 169, 245, 40));

        // Binding status
        ImGui::TableSetColumnIndex(0);
        UdpSource::FieldStatus st;
        if (udp) st = udp->fieldStatus(field);
        std::string tip;
        ImU32 dot = IM_COL32(255, 255, 255, 40);
        if (!b.signal.empty()) {
            switch (st.state) {
            case UdpSource::BindState::Ok:       dot = kGreen; tip = "Receiving " + st.path; break;
            case UdpSource::BindState::Missing:  dot = kAmber; tip = "Signal not received yet"; break;
            case UdpSource::BindState::BadValue: dot = kRed; tip = "Value of " + st.path + " could not be converted"; break;
            case UdpSource::BindState::Unbound:  dot = kAmber; tip = "Not connected"; break;
            }
        }
        StatusDot(dot, tip.c_str());

        ImGui::TableSetColumnIndex(1);
        if (ImGui::Selectable(info.label, i == selected, ImGuiSelectableFlags_None,
                              ImVec2(0, ImGui::GetFrameHeight())))
            selected = i;
        if (info.isEnum && ImGui::IsItemHovered())
            ImGui::SetTooltip("Accepts the state name as text (e.g. \"R2D\") or its index as a number.");

        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##signal", "message.signal", &b.signal);
        if (ImGui::IsItemActivated()) selected = i;
        if (ImGui::IsItemDeactivatedAfterEdit()) AssignSignal(config, field, b.signal);   // keep signals unique

        ImGui::TableSetColumnIndex(3);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputFloat("##scale", &b.scale, 0, 0, "%g");
        ImGui::TableSetColumnIndex(4);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputFloat("##offset", &b.offset, 0, 0, "%g");

        ImGui::TableSetColumnIndex(5);
        ImGui::AlignTextToFramePadding();
        const std::string value = FormatField(field, telemetry.frame());
        if (st.state == UdpSource::BindState::Ok) ImGui::TextUnformatted(value.c_str());
        else ImGui::TextDisabled("%s", value.c_str());

        ImGui::TableSetColumnIndex(6);
        if (!b.signal.empty()) {
            if (ImGui::Button("x", ImVec2(-FLT_MIN, 0))) ClearSignal(config, field);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unbind");
        }

        ImGui::PopID();
    }
    ImGui::EndTable();
}

void ReceivedSignals(SourceConfig& config, const UdpSource* udp, int& selected, ImVec2 size, double timeout)
{
    static std::string filter;
    const std::vector<UdpSource::SignalRow> rows = udp ? udp->signals() : std::vector<UdpSource::SignalRow>{};

    // Which dashboard value each received path is bound to.
    std::unordered_map<std::string, std::string> boundTo;
    if (udp) {
        for (int i = 0; i < kFieldCount; ++i) {
            const UdpSource::FieldStatus st = udp->fieldStatus(static_cast<FrameField>(i));
            if (!st.path.empty() && !config.bindings[i].signal.empty()) {
                std::string& s = boundTo[st.path];
                s += s.empty() ? "" : ", ";
                s += GetFieldInfo(static_cast<FrameField>(i)).label;
            }
        }
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##filter", "Filter signals...", &filter);
    const std::string needle = Lower(filter);

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    const ImVec2 tableSize(size.x, size.y - ImGui::GetFrameHeightWithSpacing());
    if (!ImGui::BeginTable("##received", 4, flags, tableSize)) return;
    const float em = ImGui::GetFontSize();
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthStretch, 3.2f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.8f);
    ImGui::TableSetupColumn("Age", ImGuiTableColumnFlags_WidthFixed, em * 3.2f);
    ImGui::TableSetupColumn("Bound to", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableHeadersRow();

    if (rows.empty()) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled(udp ? "No signals received yet." : "Not connected.");
    }
    for (const auto& r : rows) {
        if (!needle.empty() && Lower(r.path).find(needle) == std::string::npos) continue;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto bound = boundTo.find(r.path);
        if (bound != boundTo.end()) ImGui::PushStyleColor(ImGuiCol_Text, Col(kAccent));
        // One click binds one value; the selection is then cleared so stray clicks do nothing.
        if (ImGui::Selectable(r.path.c_str(), false, ImGuiSelectableFlags_SpanAllColumns) && selected >= 0) {
            AssignSignal(config, static_cast<FrameField>(selected), r.path);
            selected = -1;
        }
        if (bound != boundTo.end()) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            if (selected >= 0)
                ImGui::SetTooltip("Click to bind to \"%s\"", GetFieldInfo(static_cast<FrameField>(selected)).label);
            else
                ImGui::SetTooltip("Select a dashboard value on the left first");
        }

        const bool stale = r.age > timeout;
        ImGui::TableSetColumnIndex(1);
        const std::string v = FormatValue(r.value);
        if (stale) ImGui::TextDisabled("%s", v.c_str());
        else ImGui::TextUnformatted(v.c_str());
        ImGui::TableSetColumnIndex(2);
        if (stale) ImGui::TextDisabled("%.1fs", r.age);
        else ImGui::Text("%.2fs", r.age);
        ImGui::TableSetColumnIndex(3);
        if (bound != boundTo.end()) ImGui::TextColored(Col(kAccent), "%s", bound->second.c_str());
    }
    ImGui::EndTable();
}

void JsonFormatHelp()
{
    ImGui::TextWrapped(
        "Each UDP datagram should contain one JSON object (or several, one per line). It is flattened into "
        "dotted signal paths, so any nesting works. Arrays use each element's \"name\" (or hex \"id\") as the "
        "path segment, and {\"name\": ..., \"value\": ...} pairs become a single signal. A binding may also be "
        "just the end of a path, e.g. \"BMS_Status.SOC\" or \"SOC\".");
    ImGui::Spacing();
    ImGui::TextDisabled("Examples (all give the signal BMS_Status.SOC):");
    ImGui::PushStyleColor(ImGuiCol_Text, Col(kTextDim));
    ImGui::TextUnformatted("  {\"BMS_Status\": {\"SOC\": 54.5, \"PackVoltage\": 562.1}}\n"
                           "  {\"messages\": [{\"name\": \"BMS_Status\", \"id\": 513, \"signals\": {\"SOC\": 54.5}}]}\n"
                           "  {\"messages\": [{\"name\": \"BMS_Status\", \"signals\": [{\"name\": \"SOC\", \"value\": 54.5}]}]}");
    ImGui::PopStyleColor();
}

} // namespace

void DrawSourceWindow(bool* open, SourceConfig& config, Telemetry& telemetry, const UiFonts& fonts, float s)
{
    static bool wasOpen = false;
    static UdpConfig edit;          // connection settings being edited
    static int selected = -1;       // mapping row that a click on a received signal binds to (-1 = none)

    if (*open && !wasOpen) {
        edit = config.udp;
        ImGui::OpenPopup(kWindowId);
    }
    if (!*open && wasOpen) config.save();
    wasOpen = *open;
    if (!*open) return;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(std::min(1180 * s, display.x * 0.94f), std::min(800 * s, display.y * 0.92f)),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18 * s, 14 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * s, 7 * s));
    if (!ImGui::BeginPopupModal(kWindowId, open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PopStyleVar(2);
        return;
    }

    auto* udp = dynamic_cast<UdpSource*>(&telemetry.source());
    if (udp) udp->setBindings(config.bindings);   // mapping edits apply live

    // --- Source type -----------------------------------------------------------
    SectionTitle(fonts, "SOURCE");
    const float cardW = (ImGui::GetContentRegionAvail().x - 12 * s) * 0.5f;
    const ImVec2 cardSize(cardW, 66 * s);
    const bool isUdp = config.kind == SourceConfig::Kind::Udp;
    if (SourceCard("##sim", "Simulation", "Built-in simulated car: start-up sequence, laps and shutdown.", !isUdp,
                   cardSize, fonts, s) && isUdp) {
        config.kind = SourceConfig::Kind::Simulated;
        telemetry.setSource(CreateSource(config));
        udp = nullptr;
        config.save();
    }
    ImGui::SameLine(0, 12 * s);
    if (SourceCard("##udp", "UDP  /  JSON", "CAN messages and signals from the car's telemetry unit.", isUdp, cardSize,
                   fonts, s) && !isUdp) {
        config.kind = SourceConfig::Kind::Udp;
        config.udp = edit;
        telemetry.setSource(CreateSource(config));
        udp = dynamic_cast<UdpSource*>(&telemetry.source());
        config.save();
    }
    ImGui::Spacing();

    const float footerH = ImGui::GetFrameHeightWithSpacing() + 10 * s;
    if (config.kind == SourceConfig::Kind::Simulated) {
        ImGui::BeginChild("##simbody", ImVec2(0, -footerH));
        ImGui::TextWrapped("The dashboard is driven by the built-in simulator. Switch to UDP / JSON to receive live "
                           "data from the car; your connection settings and signal mapping are kept.");
        ImGui::EndChild();
    } else {
        // --- Connection ----------------------------------------------------------
        SectionTitle(fonts, "CONNECTION");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Bind address");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150 * s);
        ImGui::InputTextWithHint("##addr", "0.0.0.0", &edit.bindAddress);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("0.0.0.0 listens on every network interface.");
        ImGui::SameLine(0, 18 * s);
        ImGui::TextUnformatted("Port");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90 * s);
        ImGui::InputInt("##port", &edit.port, 0);
        edit.port = std::clamp(edit.port, 1, 65535);
        ImGui::SameLine(0, 18 * s);
        ImGui::TextUnformatted("No-data timeout");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70 * s);
        ImGui::InputDouble("##timeout", &edit.timeoutSec, 0, 0, "%.1f s");
        edit.timeoutSec = std::clamp(edit.timeoutSec, 0.1, 60.0);
        ImGui::SameLine(0, 18 * s);

        const bool dirty = !udp || edit.bindAddress != udp->config().bindAddress || edit.port != udp->config().port ||
                           edit.timeoutSec != udp->config().timeoutSec;
        const bool failed = udp && !udp->stats().listening;
        if (!dirty && !failed) ImGui::BeginDisabled();
        if (ImGui::Button(failed && !dirty ? "Retry" : "Apply & reconnect")) {
            config.udp = edit;
            telemetry.setSource(CreateSource(config));
            udp = dynamic_cast<UdpSource*>(&telemetry.source());
            config.save();
        }
        if (!dirty && !failed) ImGui::EndDisabled();

        ConnectionStatus(udp, config.udp.timeoutSec);
        ImGui::Spacing();

        // --- Mapping + received signals -------------------------------------------
        const float helpH = ImGui::GetFrameHeightWithSpacing() * 2.0f;
        const float paneH = std::max(160 * s, ImGui::GetContentRegionAvail().y - footerH - helpH);
        const float leftW = ImGui::GetContentRegionAvail().x * 0.54f;

        ImGui::BeginGroup();
        SectionTitle(fonts, "SIGNAL MAPPING");
        MappingTable(config, telemetry, udp, selected, ImVec2(leftW, paneH - ImGui::GetTextLineHeightWithSpacing()));
        ImGui::EndGroup();
        ImGui::SameLine(0, 14 * s);
        ImGui::BeginGroup();
        SectionTitle(fonts, "RECEIVED SIGNALS  -  click one to bind it to the selected value");
        ReceivedSignals(config, udp, selected,
                        ImVec2(ImGui::GetContentRegionAvail().x, paneH - ImGui::GetTextLineHeightWithSpacing()),
                        config.udp.timeoutSec);
        ImGui::EndGroup();

        // --- Help + raw packet (collapsible) ------------------------------------------
        ImGui::BeginChild("##extras", ImVec2(0, -footerH));
        if (ImGui::CollapsingHeader("Last packet")) {
            std::string raw = udp ? udp->lastPacket() : std::string();
            ImGui::InputTextMultiline("##raw", &raw, ImVec2(-FLT_MIN, 150 * s), ImGuiInputTextFlags_ReadOnly);
        }
        if (ImGui::CollapsingHeader("Expected JSON format")) JsonFormatHelp();
        ImGui::EndChild();
    }

    // --- Footer --------------------------------------------------------------------
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Saved to %s", config.path.c_str());
    const char* closeLabel = "Close";
    const float bw = ImGui::CalcTextSize(closeLabel).x + ImGui::GetStyle().FramePadding.x * 2 + 30 * s;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - bw);
    const bool escape = ImGui::IsKeyPressed(ImGuiKey_Escape) && !ImGui::IsAnyItemActive();
    if (ImGui::Button(closeLabel, ImVec2(bw, 0)) || escape) {
        *open = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    ImGui::PopStyleVar(2);
}
