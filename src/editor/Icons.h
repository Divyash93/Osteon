// Vector icons drawn with ImDrawList (no image assets), used by the toolbar, tree and properties.
#pragma once
#include "imgui.h"
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
    GroupOpen, GroupClosed, Color, Deform, Copy, Cut, Paste, CurveLinear, CurveBezier, CurveStepped
};

/** Draws `icon` centred at `c`, fitting a box of `size` pixels. `col` is the main color. */
void drawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float size, ImU32 col);

/** The icons a bone can be shown as (its "Icon" property). The first, "default", is the ordinary bone shape. */
const std::vector<std::string>& boneIconNames();
/** Draws the bone icon `name` centred at `c` in a box of `size` pixels; "default", "" or an unknown name draws the bone shape. */
void drawBoneIcon(ImDrawList* dl, const std::string& name, ImVec2 c, float size, ImU32 col);
inline bool isDefaultBoneIcon(const std::string& name) { return name.empty() || name == "default"; }

/** Spine-style toolbar button: icon at the left, label next to it. Returns true when clicked. */
bool iconButton(const char* id, Icon icon, const char* label, bool active, ImVec2 size, const char* tooltip = nullptr, ImU32 iconColor = 0);
/** Small square button showing only an icon. */
bool iconOnlyButton(const char* id, Icon icon, bool active, float size, const char* tooltip = nullptr, ImU32 iconColor = 0);

} // namespace osteon
