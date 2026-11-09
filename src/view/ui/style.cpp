#include "view/ui/style.h"

#include "view/ui/theme.h"

#include <cstdio>

using namespace theme;

namespace {

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
    auto col = [](ImU32 c, float a = 1.0f) { ImVec4 v = ImGui::ColorConvertU32ToFloat4(c); v.w *= a; return v; };
    st.Colors[ImGuiCol_Text] = col(kText);
    st.Colors[ImGuiCol_TextDisabled] = col(kTextFaint);
    st.Colors[ImGuiCol_PopupBg] = ImVec4(0.035f, 0.055f, 0.095f, 0.98f);
    st.Colors[ImGuiCol_TitleBg] = st.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.08f, 0.14f, 1.0f);
    st.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
    st.Colors[ImGuiCol_FrameBg] = col(kCard);
    st.Colors[ImGuiCol_FrameBgHovered] = col(kCardHi);
    st.Colors[ImGuiCol_FrameBgActive] = col(kCardHi);
    st.Colors[ImGuiCol_Button] = col(kCardHi);
    st.Colors[ImGuiCol_ButtonHovered] = col(kAccent, 0.55f);
    st.Colors[ImGuiCol_ButtonActive] = col(kAccent, 0.8f);
    st.Colors[ImGuiCol_Header] = col(kAccent, 0.22f);
    st.Colors[ImGuiCol_HeaderHovered] = col(kAccent, 0.32f);
    st.Colors[ImGuiCol_HeaderActive] = col(kAccent, 0.45f);
    st.Colors[ImGuiCol_TableHeaderBg] = col(kCardHi);
    st.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.025f);
    st.Colors[ImGuiCol_TableBorderLight] = col(kBorder, 0.5f);
    st.Colors[ImGuiCol_TableBorderStrong] = col(kBorder);
    st.Colors[ImGuiCol_Separator] = col(kBorder, 0.7f);
    st.Colors[ImGuiCol_CheckMark] = col(kAccent);
    st.Colors[ImGuiCol_TextSelectedBg] = col(kAccent, 0.35f);
    st.Colors[ImGuiCol_NavHighlight] = col(kAccent);
    st.FramePadding = ImVec2(8.0f, 5.0f);
    st.PopupRounding = 10.0f;
    st.CellPadding = ImVec2(6.0f, 4.0f);
    st.ScaleAllSizes(scale);
}

