// Vector icons drawn with ImDrawList (no image assets), used by the toolbar, tree and properties.
#pragma once
#include "imgui.h"

namespace osteon {

enum class Icon {
    Pose, Weights, Create, Rotate, Translate, Scale, Shear,
    AxisLocal, AxisParent, AxisWorld, ShowBones, ShowImages, ShowPixels,
    Bone, Slot, Region, Mesh, LinkedMesh, BoundingBox, Path, Point, Clipping,
    Ik, TransformCon, PathCon, PhysicsCon, Constraints, DrawOrder, Skins, Events, Animations, ImagesFolder, Audio, Skeleton,
    Eye, EyeOff, Dot, DotOff, Parent, New, Split, Delete, Key, Folder, Search, Fit, Zoom100, Image, Chevron,
    FirstKey, PrevKey, PrevFrame, PlayBack, Play, Pause, NextFrame, NextKey, LastKey, Loop, Sync, Filter, Lock, Shift, Adjust,
    GroupOpen, GroupClosed, Color, Deform
};

/** Draws `icon` centred at `c`, fitting a box of `size` pixels. `col` is the main color. */
void drawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float size, ImU32 col);

/** Spine-style toolbar button: icon at the left, label next to it. Returns true when clicked. */
bool iconButton(const char* id, Icon icon, const char* label, bool active, ImVec2 size, const char* tooltip = nullptr, ImU32 iconColor = 0);
/** Small square button showing only an icon. */
bool iconOnlyButton(const char* id, Icon icon, bool active, float size, const char* tooltip = nullptr, ImU32 iconColor = 0);

} // namespace osteon
