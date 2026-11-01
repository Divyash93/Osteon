// Vector icons drawn with ImDrawList (no image assets), used by the toolbar, tree and properties.
#pragma once
#include "imgui.h"
#include "Theme.h"
#include <cstdlib>
#include <string>
#include <vector>

namespace osteon {

enum class Icon {
    Pose, Weights, Create, Rotate, Translate, Scale, Shear,
    AxisLocal, AxisParent, AxisWorld, ShowBones, ShowImages, ShowPixels,
    Bone, Slot, Region, Mesh, LinkedMesh, BoundingBox, Path, Point, Clipping,
    Ik, TransformCon, PathCon, PhysicsCon, Constraints, DrawOrder, Skins, Events, Animations, ImagesFolder, Audio, Skeleton,
    Eye, EyeOff, Dot, DotOff, Parent, New, Split, Delete, Key, Folder, Search, Fit, Zoom100, Image, Chevron,
    FirstKey, PrevKey, PrevFrame, PlayBack, Play, Pause, NextFrame, NextKey, LastKey, Loop, Sync, Filter, Lock, Shift, Adjust,
    GroupOpen, GroupClosed, Color, Deform, Copy, Cut, Paste, CurveLinear, CurveBezier, CurveStepped, Globe,
    DocNew, Save, SaveAs, Import, Export, Psd, Atlas, Gear, Exit, Undo, Redo, Question, Pencil, ZoomIn, ZoomOut
};

/** The user interface theme: false = Dark (Photoshop-style greys), true = Light. Set from Settings. */
extern bool gLightUi;
inline ImU32 uiTextColor() { return gLightUi ? IM_COL32(40, 40, 40, 255) : theme::hex(theme::kText); }
/** A neutral grey drawn into the UI: kept in Dark, flipped for Light (dark surfaces become light, light text becomes dark).
    Translucent white or black overlays become dark overlays. Colours with a hue are left alone. */
inline ImU32 uiG(ImU32 c) {
    if (!gLightUi) return c;
    int r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF, a = (c >> 24) & 0xFF;
    if (std::abs(r - g) > 14 || std::abs(g - b) > 14) return c;
    int v = (r + g + b) / 3;
    int nv;
    if (a < 200) nv = v > 128 ? 0 : v;            // translucent overlays darken instead of lighten
    else if (v <= 70) nv = 212 - (v * 55) / 100;   // dark surfaces (canvases, rulers) become mid greys
    else nv = 160 - ((v - 70) * 80) / 100;         // light text, ticks and handles become dark
    if (nv < 10) nv = 10;
    return IM_COL32(nv, nv, nv, a);
}
inline ImVec4 uiRaised() { return gLightUi ? ImVec4(0.87f, 0.87f, 0.87f, 1) : theme::vec(theme::hex(theme::kPanel)); }
inline ImVec4 uiRaisedHi() { return gLightUi ? ImVec4(0.79f, 0.79f, 0.79f, 1) : theme::vec(theme::hex(theme::kHover)); }
inline ImU32 uiDimTextColor() { return gLightUi ? IM_COL32(70, 70, 70, 255) : theme::hex(theme::kTextSecondary); }

/** Draws `icon` centred at `c`, fitting a box of `size` pixels. `col` is the main color. */
void drawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float size, ImU32 col);
/** While on, icons are drawn in one colour (their shapes keep their alpha): the header's white glyphs. */
void setIconMono(bool on, ImU32 color = 0);

/** The icons a bone can be shown as (its "Icon" property). The first, "default", is the ordinary bone shape. */
const std::vector<std::string>& boneIconNames();
/** Draws the bone icon `name` centred at `c` in a box of `size` pixels; "default", "" or an unknown name draws the bone shape. */
void drawBoneIcon(ImDrawList* dl, const std::string& name, ImVec2 c, float size, ImU32 col);
// Two opposite horizontal arrows (a "resize" hint) with its top-left corner at o; the artwork is 40 wide x 64 tall units times s, arrows pointing up and down.
void drawDoubleArrowIcon(ImDrawList* dl, ImVec2 o, float s, ImU32 fill, ImU32 line);
inline bool isDefaultBoneIcon(const std::string& name) { return name.empty() || name == "default"; }

/** Spine-style toolbar button: icon at the left, label next to it. Returns true when clicked. */
bool iconButton(const char* id, Icon icon, const char* label, bool active, ImVec2 size, const char* tooltip = nullptr, ImU32 iconColor = 0);
/** Small square button showing only an icon. */
bool iconOnlyButton(const char* id, Icon icon, bool active, float size, const char* tooltip = nullptr, ImU32 iconColor = 0);

/**
 * Modern slider: a thin track with a round knob, and a box beside it to type a value. The format's text around the number
 * (like "Weight %.0f%%") becomes the label. Use the helpers below in place of IsItemActivated() etc. after calling it.
 */
/** Soft dark glow around the current window (call right after Begin): layered rings that fade out over `spread` pixels. */
void windowShadow(float spread = 30);
bool sliderFloat(const char* label, float* v, float lo, float hi, const char* format = "%.2f");
bool sliderInt(const char* label, int* v, int lo, int hi, const char* format = "%d");
bool sliderActivated();              // the track or the box was just grabbed
bool sliderDeactivatedAfterEdit();   // it was let go after the value changed
bool sliderDeactivated();            // it was let go, whether or not the value changed
bool sliderHovered();                // the mouse is over the track or the box

/** Menu entries with an icon at the left (the label is padded to make room). Same arguments as ImGui::MenuItem / BeginMenu. */
bool iconMenuItem(Icon icon, const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true);
bool iconBeginMenu(Icon icon, const char* label, bool enabled = true);

/** Round filled button (the play button). `fill` is its colour. */
bool roundIconButton(const char* id, Icon icon, bool active, float size, const char* tooltip, ImU32 fill);

/** A soft rounded plate behind a run of buttons: begin(), emit the items with SameLine, end(). */
struct ButtonGroup {
    ImDrawListSplitter split;
    ImVec2 min;
    void begin();
    void end(float pad = 3);
};

} // namespace osteon
