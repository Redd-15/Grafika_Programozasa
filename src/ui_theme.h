// Colour palette shared by the dashboard and the data-source window.
#pragma once
#include <imgui.h>

namespace theme {
constexpr ImU32 kPanel     = IM_COL32(7, 11, 19, 225);
constexpr ImU32 kCard      = IM_COL32(13, 37, 63, 255);
constexpr ImU32 kCardHi    = IM_COL32(20, 54, 90, 255);
constexpr ImU32 kBorder    = IM_COL32(38, 74, 115, 255);
constexpr ImU32 kText      = IM_COL32(236, 242, 250, 255);
constexpr ImU32 kTextDim   = IM_COL32(138, 162, 188, 255);
constexpr ImU32 kTextFaint = IM_COL32(90, 110, 135, 255);
constexpr ImU32 kAccent    = IM_COL32(63, 169, 245, 255);
constexpr ImU32 kGreen     = IM_COL32(46, 204, 113, 255);
constexpr ImU32 kAmber     = IM_COL32(245, 166, 35, 255);
constexpr ImU32 kOrange    = IM_COL32(255, 118, 40, 255);
constexpr ImU32 kRed       = IM_COL32(240, 71, 71, 255);
constexpr ImU32 kViolet    = IM_COL32(155, 110, 255, 255);
} // namespace theme
