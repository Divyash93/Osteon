// The dark theme's colour system (Photoshop-inspired greys). Every dark surface, border, text and control state in the
// editor is meant to come from here, so a new panel or control only picks a role and stays consistent.
// The Light theme is separate (applyUiTheme in main.cpp); for custom-drawn colours wrap a value in uiG() (Icons.h) to flip it.
#pragma once
#include "imgui.h"

namespace osteon::theme {

constexpr ImU32 hex(unsigned rgb, unsigned a = 255) { return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, a); }
inline ImVec4 vec(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

// Surfaces
constexpr unsigned kAppBg = 0x262626;     // main application background, canvases behind panels (docking gaps, timeline grid)
constexpr unsigned kPanel = 0x323232;     // every panel, toolbar, menu bar, popup and button
constexpr unsigned kField = 0x292929;     // input fields, text boxes, sliders' tracks
// Lines
constexpr unsigned kBorder = 0x48484B;    // panel borders and component strokes
constexpr unsigned kStroke = 0x77777D;    // button outlines
constexpr unsigned kDivider = 0x262626;   // subtle dividers between rows
// Text
constexpr unsigned kText = 0xE6E6E6;      // primary
constexpr unsigned kTextSecondary = 0xB5B5B5;
constexpr unsigned kTextDisabled = 0x77777D;
// Control states (a step lighter / darker than kPanel)
constexpr unsigned kFieldHover = 0x2E2E2E; // an input field under the pointer
constexpr unsigned kHover = 0x3E3E3E;
constexpr unsigned kPressed = 0x292929;
constexpr unsigned kSelectedBg = 0x124DAD; // active / selected: the accent blue
constexpr unsigned kSelectedHi = 0x1F66D9;
constexpr unsigned kTabHover = 0x3A3A3A;
constexpr unsigned kRowHover = 0x3A3A3A;   // list / tree row under the pointer

} // namespace osteon::theme
