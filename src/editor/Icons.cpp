#include "Icons.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace osteon {

static ImVec2 P(ImVec2 c, float s, float x, float y) { return ImVec2(c.x + x * s, c.y + y * s); }

// Triangle pointing towards `tip` (x) from a base at `base` (x), half height `hh`.
static void tri(ImDrawList* dl, ImVec2 c, float s, float tip, float base, float hh, ImU32 col) {
    dl->AddTriangleFilled(P(c, s, tip, 0), P(c, s, base, -hh), P(c, s, base, hh), col);
}

static void arrowHead(ImDrawList* dl, ImVec2 tip, ImVec2 dir, float len, float half, ImU32 col) {
    ImVec2 n(-dir.y, dir.x);
    dl->AddTriangleFilled(tip, ImVec2(tip.x - dir.x * len + n.x * half, tip.y - dir.y * len + n.y * half),
                          ImVec2(tip.x - dir.x * len - n.x * half, tip.y - dir.y * len - n.y * half), col);
}

void drawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float size, ImU32 col) {
    const float s = size;
    const float th = std::max(1.0f, s / 12.0f);
    const ImU32 white = IM_COL32(230, 230, 230, 255);
    switch (icon) {
    case Icon::Pose: { // green diamond with a dot
        ImU32 g = IM_COL32(90, 200, 90, 255);
        ImVec2 pts[4] = {P(c, s, 0, -0.42f), P(c, s, 0.42f, 0), P(c, s, 0, 0.42f), P(c, s, -0.42f, 0)};
        dl->AddPolyline(pts, 4, g, ImDrawFlags_Closed, th);
        dl->AddCircleFilled(c, s * 0.12f, g);
        break;
    }
    case Icon::Weights: { // red grid
        ImU32 r = IM_COL32(220, 70, 70, 255);
        dl->AddRect(P(c, s, -0.4f, -0.4f), P(c, s, 0.4f, 0.4f), r, 0, 0, th);
        dl->AddLine(P(c, s, -0.4f, 0), P(c, s, 0.4f, 0), r, th);
        dl->AddLine(P(c, s, 0, -0.4f), P(c, s, 0, 0.4f), r, th);
        dl->AddCircleFilled(P(c, s, -0.2f, -0.2f), s * 0.09f, white);
        dl->AddCircleFilled(P(c, s, 0.2f, 0.2f), s * 0.09f, white);
        break;
    }
    case Icon::Create: { // bone being drawn, with a plus
        ImU32 b = IM_COL32(110, 190, 240, 255);
        dl->AddLine(P(c, s, -0.38f, 0.38f), P(c, s, 0.2f, -0.2f), b, th * 1.4f);
        arrowHead(dl, P(c, s, 0.34f, -0.34f), ImVec2(0.7071f, -0.7071f), s * 0.3f, s * 0.16f, b);
        dl->AddLine(P(c, s, -0.45f, -0.3f), P(c, s, -0.15f, -0.3f), white, th);
        dl->AddLine(P(c, s, -0.3f, -0.45f), P(c, s, -0.3f, -0.15f), white, th);
        break;
    }
    case Icon::Rotate: { // 3/4 circular arrow
        ImU32 g = IM_COL32(100, 210, 110, 255);
        dl->PathArcTo(c, s * 0.34f, -0.3f, 4.4f, 20);
        dl->PathStroke(g, 0, th * 1.4f);
        ImVec2 tip = P(c, s, std::cos(-0.3f) * 0.34f, std::sin(-0.3f) * 0.34f);
        arrowHead(dl, ImVec2(tip.x + s * 0.12f, tip.y + s * 0.1f), ImVec2(0.5f, 0.85f), s * 0.28f, s * 0.15f, g);
        break;
    }
    case Icon::Translate: { // four-way arrow
        ImU32 b = IM_COL32(90, 170, 240, 255);
        dl->AddLine(P(c, s, -0.4f, 0), P(c, s, 0.4f, 0), b, th);
        dl->AddLine(P(c, s, 0, -0.4f), P(c, s, 0, 0.4f), b, th);
        arrowHead(dl, P(c, s, 0.46f, 0), ImVec2(1, 0), s * 0.22f, s * 0.13f, b);
        arrowHead(dl, P(c, s, -0.46f, 0), ImVec2(-1, 0), s * 0.22f, s * 0.13f, b);
        arrowHead(dl, P(c, s, 0, 0.46f), ImVec2(0, 1), s * 0.22f, s * 0.13f, b);
        arrowHead(dl, P(c, s, 0, -0.46f), ImVec2(0, -1), s * 0.22f, s * 0.13f, b);
        break;
    }
    case Icon::Scale: { // box with outward arrow
        ImU32 r = IM_COL32(230, 90, 90, 255);
        dl->AddRect(P(c, s, -0.42f, -0.05f), P(c, s, 0.05f, 0.42f), r, 0, 0, th);
        dl->AddLine(P(c, s, -0.1f, 0.1f), P(c, s, 0.34f, -0.34f), r, th);
        arrowHead(dl, P(c, s, 0.44f, -0.44f), ImVec2(0.7071f, -0.7071f), s * 0.28f, s * 0.15f, r);
        break;
    }
    case Icon::Shear: { // parallelogram
        ImU32 y = IM_COL32(240, 200, 70, 255);
        ImVec2 pts[4] = {P(c, s, -0.2f, -0.3f), P(c, s, 0.45f, -0.3f), P(c, s, 0.2f, 0.3f), P(c, s, -0.45f, 0.3f)};
        dl->AddPolyline(pts, 4, y, ImDrawFlags_Closed, th);
        break;
    }
    case Icon::AxisLocal: case Icon::AxisParent: case Icon::AxisWorld: {
        ImU32 g = IM_COL32(110, 210, 140, 255);
        dl->AddLine(P(c, s, -0.3f, 0.35f), P(c, s, -0.3f, -0.3f), g, th * 1.3f);
        dl->AddLine(P(c, s, -0.3f, 0.35f), P(c, s, 0.35f, 0.35f), g, th * 1.3f);
        arrowHead(dl, P(c, s, -0.3f, -0.42f), ImVec2(0, -1), s * 0.22f, s * 0.12f, g);
        arrowHead(dl, P(c, s, 0.45f, 0.35f), ImVec2(1, 0), s * 0.22f, s * 0.12f, g);
        if (icon == Icon::AxisParent) dl->AddCircle(P(c, s, 0.2f, -0.15f), s * 0.14f, IM_COL32(240, 170, 60, 255), 12, th);
        if (icon == Icon::AxisWorld) dl->AddCircle(P(c, s, 0.2f, -0.15f), s * 0.18f, IM_COL32(110, 170, 240, 255), 12, th);
        break;
    }
    case Icon::Bone: {
        ImVec2 pts[4] = {P(c, s, -0.38f, 0.38f), P(c, s, -0.12f, -0.02f), P(c, s, 0.4f, -0.4f), P(c, s, 0.02f, 0.12f)};
        dl->AddConvexPolyFilled(pts, 4, col);
        dl->AddCircleFilled(P(c, s, -0.34f, 0.34f), s * 0.1f, white);
        break;
    }
    case Icon::ShowBones: {
        ImVec2 pts[4] = {P(c, s, -0.38f, 0.38f), P(c, s, -0.12f, -0.02f), P(c, s, 0.4f, -0.4f), P(c, s, 0.02f, 0.12f)};
        dl->AddConvexPolyFilled(pts, 4, IM_COL32(110, 200, 230, 255));
        break;
    }
    case Icon::Slot: {
        dl->AddCircle(c, s * 0.34f, col, 16, th * 1.4f);
        dl->AddCircleFilled(c, s * 0.14f, col);
        break;
    }
    case Icon::Image: case Icon::Region: case Icon::ShowImages: case Icon::ImagesFolder: {
        ImU32 cc = icon == Icon::ImagesFolder ? IM_COL32(240, 220, 90, 255) : IM_COL32(90, 200, 210, 255);
        dl->AddRect(P(c, s, -0.4f, -0.34f), P(c, s, 0.4f, 0.34f), cc, 0, 0, th * 1.2f);
        ImVec2 m[3] = {P(c, s, -0.32f, 0.26f), P(c, s, -0.05f, -0.08f), P(c, s, 0.14f, 0.26f)};
        dl->AddConvexPolyFilled(m, 3, cc);
        dl->AddCircleFilled(P(c, s, 0.22f, -0.15f), s * 0.07f, cc);
        break;
    }
    case Icon::Mesh: case Icon::LinkedMesh: {
        ImU32 cc = IM_COL32(90, 200, 210, 255);
        dl->AddRect(P(c, s, -0.4f, -0.4f), P(c, s, 0.4f, 0.4f), cc, 0, 0, th);
        dl->AddLine(P(c, s, -0.4f, -0.4f), P(c, s, 0.4f, 0.4f), cc, th);
        dl->AddLine(P(c, s, -0.4f, 0.05f), P(c, s, 0.05f, -0.4f), cc, th);
        dl->AddLine(P(c, s, -0.05f, 0.4f), P(c, s, 0.4f, -0.05f), cc, th);
        if (icon == Icon::LinkedMesh) dl->AddCircleFilled(P(c, s, 0.3f, 0.3f), s * 0.15f, IM_COL32(240, 170, 60, 255));
        break;
    }
    case Icon::BoundingBox: {
        ImU32 g = IM_COL32(110, 220, 70, 255);
        ImVec2 pts[5] = {P(c, s, -0.38f, -0.1f), P(c, s, -0.1f, -0.4f), P(c, s, 0.38f, -0.2f), P(c, s, 0.3f, 0.36f), P(c, s, -0.3f, 0.3f)};
        dl->AddPolyline(pts, 5, g, ImDrawFlags_Closed, th * 1.2f);
        break;
    }
    case Icon::Path: {
        ImU32 o = IM_COL32(250, 150, 50, 255);
        dl->AddBezierCubic(P(c, s, -0.42f, 0.3f), P(c, s, -0.1f, -0.6f), P(c, s, 0.1f, 0.6f), P(c, s, 0.42f, -0.3f), o, th * 1.3f);
        break;
    }
    case Icon::Point: {
        ImU32 y = IM_COL32(240, 230, 80, 255);
        dl->AddCircle(c, s * 0.2f, y, 12, th * 1.2f);
        dl->AddLine(P(c, s, -0.42f, 0), P(c, s, 0.42f, 0), y, th);
        dl->AddLine(P(c, s, 0, -0.42f), P(c, s, 0, 0.42f), y, th);
        break;
    }
    case Icon::Clipping: {
        ImU32 r = IM_COL32(230, 90, 90, 255);
        dl->AddRect(P(c, s, -0.4f, -0.4f), P(c, s, 0.2f, 0.2f), r, 0, 0, th);
        dl->AddRectFilled(P(c, s, -0.1f, -0.1f), P(c, s, 0.4f, 0.4f), r);
        break;
    }
    case Icon::Ik: case Icon::Constraints: {
        ImU32 o = IM_COL32(250, 140, 60, 255);
        dl->AddLine(P(c, s, -0.38f, 0.4f), P(c, s, 0.3f, -0.3f), o, th * 1.6f);
        arrowHead(dl, P(c, s, 0.42f, -0.42f), ImVec2(0.7071f, -0.7071f), s * 0.3f, s * 0.17f, o);
        break;
    }
    case Icon::TransformCon: {
        ImU32 y = IM_COL32(220, 220, 90, 255);
        dl->AddLine(P(c, s, -0.38f, 0.38f), P(c, s, 0.38f, -0.38f), y, th * 1.4f);
        dl->AddCircle(P(c, s, -0.3f, 0.3f), s * 0.12f, y, 10, th);
        dl->AddCircle(P(c, s, 0.3f, -0.3f), s * 0.12f, y, 10, th);
        break;
    }
    case Icon::PathCon: dl->AddBezierCubic(P(c, s, -0.42f, 0.3f), P(c, s, -0.1f, -0.6f), P(c, s, 0.1f, 0.6f), P(c, s, 0.42f, -0.3f), IM_COL32(250, 150, 50, 255), th * 1.3f); break;
    case Icon::PhysicsCon: {
        ImU32 b = IM_COL32(120, 190, 250, 255);
        dl->AddCircle(P(c, s, 0.1f, 0.15f), s * 0.2f, b, 12, th * 1.2f);
        dl->AddLine(P(c, s, -0.3f, -0.4f), P(c, s, -0.02f, 0.02f), b, th);
        break;
    }
    case Icon::DrawOrder: {
        ImU32 b = IM_COL32(90, 150, 240, 255);
        dl->AddRectFilled(P(c, s, -0.42f, -0.1f), P(c, s, 0.22f, 0.4f), IM_COL32(60, 100, 170, 255));
        dl->AddRect(P(c, s, -0.2f, -0.4f), P(c, s, 0.42f, 0.1f), b, 0, 0, th * 1.2f);
        break;
    }
    case Icon::Skins: {
        ImU32 o = IM_COL32(250, 150, 60, 255);
        ImVec2 pts[8] = {P(c, s, -0.15f, -0.4f), P(c, s, 0.15f, -0.4f), P(c, s, 0.42f, -0.2f), P(c, s, 0.3f, 0.0f),
                         P(c, s, 0.2f, -0.06f), P(c, s, 0.2f, 0.4f), P(c, s, -0.2f, 0.4f), P(c, s, -0.2f, -0.06f)};
        dl->AddConvexPolyFilled(pts, 8, o);
        break;
    }
    case Icon::Events: {
        ImU32 p = IM_COL32(170, 110, 240, 255);
        dl->AddRectFilled(P(c, s, -0.4f, -0.4f), P(c, s, 0.4f, 0.4f), p, s * 0.1f);
        dl->AddRectFilled(P(c, s, -0.05f, -0.28f), P(c, s, 0.05f, 0.08f), white);
        dl->AddCircleFilled(P(c, s, 0, 0.24f), s * 0.07f, white);
        break;
    }
    case Icon::Key: { // a key: round bow, shaft, two teeth
        dl->AddCircle(P(c, s, -0.22f, 0), s * 0.17f, col, 14, th * 1.3f);
        dl->AddLine(P(c, s, -0.05f, 0), P(c, s, 0.42f, 0), col, th * 1.4f);
        dl->AddLine(P(c, s, 0.26f, 0), P(c, s, 0.26f, 0.16f), col, th * 1.4f);
        dl->AddLine(P(c, s, 0.39f, 0), P(c, s, 0.39f, 0.16f), col, th * 1.4f);
        break;
    }
    case Icon::Animations: {
        ImU32 w = col;
        dl->AddCircleFilled(P(c, s, 0.05f, -0.32f), s * 0.1f, w);
        dl->AddLine(P(c, s, 0.05f, -0.2f), P(c, s, -0.05f, 0.1f), w, th * 1.4f);
        dl->AddLine(P(c, s, -0.05f, 0.1f), P(c, s, -0.3f, 0.4f), w, th * 1.4f);
        dl->AddLine(P(c, s, -0.05f, 0.1f), P(c, s, 0.25f, 0.4f), w, th * 1.4f);
        dl->AddLine(P(c, s, 0.0f, -0.12f), P(c, s, 0.35f, -0.2f), w, th * 1.4f);
        dl->AddLine(P(c, s, 0.0f, -0.12f), P(c, s, -0.35f, -0.02f), w, th * 1.4f);
        break;
    }
    case Icon::Audio: {
        ImU32 g = IM_COL32(110, 210, 110, 255);
        ImVec2 pts[4] = {P(c, s, -0.4f, -0.12f), P(c, s, -0.15f, -0.12f), P(c, s, 0.15f, -0.38f), P(c, s, 0.15f, 0.38f)};
        dl->AddConvexPolyFilled(pts, 4, g);
        dl->AddRectFilled(P(c, s, -0.4f, -0.12f), P(c, s, -0.12f, 0.12f), g);
        dl->AddLine(P(c, s, 0.28f, -0.15f), P(c, s, 0.28f, 0.15f), g, th);
        break;
    }
    case Icon::Skeleton: {
        dl->AddCircleFilled(P(c, s, 0, -0.32f), s * 0.11f, col);
        dl->AddLine(P(c, s, 0, -0.2f), P(c, s, 0, 0.12f), col, th * 1.4f);
        dl->AddLine(P(c, s, -0.3f, -0.08f), P(c, s, 0.3f, -0.08f), col, th * 1.4f);
        dl->AddLine(P(c, s, 0, 0.12f), P(c, s, -0.22f, 0.42f), col, th * 1.4f);
        dl->AddLine(P(c, s, 0, 0.12f), P(c, s, 0.22f, 0.42f), col, th * 1.4f);
        break;
    }
    case Icon::Eye: {
        dl->PathArcTo(P(c, s, 0, 0.3f), s * 0.52f, -2.3f, -0.84f, 12);
        dl->PathStroke(col, 0, th);
        dl->PathArcTo(P(c, s, 0, -0.3f), s * 0.52f, 0.84f, 2.3f, 12);
        dl->PathStroke(col, 0, th);
        dl->AddCircleFilled(c, s * 0.14f, col);
        break;
    }
    case Icon::EyeOff: { // a closed eye: the lower lid with three lashes, dimmed
        ImU32 e = IM_COL32(120, 122, 130, 255);
        dl->PathArcTo(P(c, s, 0, -0.3f), s * 0.52f, 0.84f, 2.3f, 12);
        dl->PathStroke(e, 0, th);
        dl->AddLine(P(c, s, 0, 0.22f), P(c, s, 0, 0.38f), e, th);
        dl->AddLine(P(c, s, -0.25f, 0.16f), P(c, s, -0.33f, 0.31f), e, th);
        dl->AddLine(P(c, s, 0.25f, 0.16f), P(c, s, 0.33f, 0.31f), e, th);
        break;
    }
    case Icon::Dot: dl->AddCircleFilled(c, s * 0.2f, col); break;
    case Icon::DotOff: dl->AddCircle(c, s * 0.2f, IM_COL32(110, 110, 110, 255), 10, th); break;
    case Icon::Parent: { // arrow pointing at a bone
        ImU32 b = IM_COL32(110, 190, 240, 255);
        dl->AddLine(P(c, s, -0.4f, 0.4f), P(c, s, 0.1f, -0.1f), b, th * 1.4f);
        arrowHead(dl, P(c, s, 0.34f, -0.34f), ImVec2(0.7071f, -0.7071f), s * 0.3f, s * 0.17f, b);
        dl->AddCircleFilled(P(c, s, -0.38f, 0.38f), s * 0.1f, white);
        break;
    }
    case Icon::New: {
        ImU32 g = IM_COL32(110, 210, 120, 255);
        dl->AddLine(P(c, s, -0.35f, 0), P(c, s, 0.35f, 0), g, th * 1.6f);
        dl->AddLine(P(c, s, 0, -0.35f), P(c, s, 0, 0.35f), g, th * 1.6f);
        break;
    }
    case Icon::Split: {
        ImU32 o = IM_COL32(250, 140, 60, 255);
        dl->AddLine(P(c, s, -0.4f, 0.4f), P(c, s, 0.4f, -0.4f), o, th * 1.4f);
        dl->AddLine(P(c, s, -0.1f, -0.3f), P(c, s, 0.3f, 0.1f), o, th);
        break;
    }
    case Icon::Delete: {
        ImU32 r = IM_COL32(230, 70, 70, 255);
        dl->AddCircleFilled(c, s * 0.42f, r);
        dl->AddLine(P(c, s, -0.18f, -0.18f), P(c, s, 0.18f, 0.18f), white, th * 1.4f);
        dl->AddLine(P(c, s, -0.18f, 0.18f), P(c, s, 0.18f, -0.18f), white, th * 1.4f);
        break;
    }
    case Icon::Folder: {
        ImU32 y = IM_COL32(240, 200, 80, 255);
        dl->AddRectFilled(P(c, s, -0.42f, -0.2f), P(c, s, 0.42f, 0.36f), y, s * 0.06f);
        dl->AddRectFilled(P(c, s, -0.42f, -0.34f), P(c, s, -0.05f, -0.15f), y);
        break;
    }
    case Icon::Search: {
        dl->AddCircle(P(c, s, -0.08f, -0.08f), s * 0.26f, col, 14, th * 1.2f);
        dl->AddLine(P(c, s, 0.12f, 0.12f), P(c, s, 0.4f, 0.4f), col, th * 1.6f);
        break;
    }
    case Icon::Fit: {
        float a = 0.42f, b = 0.18f;
        for (int sx : {-1, 1})
            for (int sy : {-1, 1}) {
                dl->AddLine(P(c, s, sx * a, sy * a), P(c, s, sx * (a - b), sy * a), col, th * 1.3f);
                dl->AddLine(P(c, s, sx * a, sy * a), P(c, s, sx * a, sy * (a - b)), col, th * 1.3f);
            }
        break;
    }
    case Icon::Zoom100: {
        dl->AddCircle(P(c, s, -0.08f, -0.08f), s * 0.26f, col, 14, th * 1.2f);
        dl->AddLine(P(c, s, 0.12f, 0.12f), P(c, s, 0.4f, 0.4f), col, th * 1.6f);
        dl->AddLine(P(c, s, -0.08f, -0.2f), P(c, s, -0.08f, 0.04f), col, th);
        break;
    }
    case Icon::FirstKey: tri(dl, c, s, -0.32f, 0.4f, 0.36f, col); dl->AddLine(P(c, s, -0.42f, -0.36f), P(c, s, -0.42f, 0.36f), col, th * 1.6f); break;
    case Icon::LastKey: tri(dl, c, s, 0.32f, -0.4f, 0.36f, col); dl->AddLine(P(c, s, 0.42f, -0.36f), P(c, s, 0.42f, 0.36f), col, th * 1.6f); break;
    case Icon::PrevKey: tri(dl, c, s, -0.42f, 0.0f, 0.36f, col); tri(dl, c, s, -0.02f, 0.42f, 0.36f, col); break;
    case Icon::NextKey: tri(dl, c, s, 0.42f, 0.0f, 0.36f, col); tri(dl, c, s, 0.02f, -0.42f, 0.36f, col); break;
    case Icon::PrevFrame: tri(dl, c, s, -0.38f, 0.36f, 0.36f, col); break;
    case Icon::NextFrame: tri(dl, c, s, 0.38f, -0.36f, 0.36f, col); break;
    case Icon::PlayBack: tri(dl, c, s, -0.4f, 0.4f, 0.4f, col); break;
    case Icon::Play: tri(dl, c, s, 0.4f, -0.4f, 0.4f, col); break;
    case Icon::Pause:
        dl->AddRectFilled(P(c, s, -0.3f, -0.4f), P(c, s, -0.08f, 0.4f), col);
        dl->AddRectFilled(P(c, s, 0.08f, -0.4f), P(c, s, 0.3f, 0.4f), col);
        break;
    case Icon::Loop: case Icon::Sync: {
        ImU32 b = icon == Icon::Loop ? IM_COL32(110, 190, 240, 255) : col;
        dl->PathArcTo(c, s * 0.34f, -0.3f, 4.4f, 20);
        dl->PathStroke(b, 0, th * 1.4f);
        ImVec2 tip = P(c, s, std::cos(-0.3f) * 0.34f, std::sin(-0.3f) * 0.34f);
        arrowHead(dl, ImVec2(tip.x + s * 0.12f, tip.y + s * 0.1f), ImVec2(0.5f, 0.85f), s * 0.28f, s * 0.15f, b);
        if (icon == Icon::Sync) {
            dl->PathArcTo(c, s * 0.34f, 2.8f, 7.6f, 20);
            dl->PathStroke(b, 0, th * 1.4f);
        }
        break;
    }
    case Icon::Filter:
        dl->AddTriangleFilled(P(c, s, -0.42f, -0.36f), P(c, s, 0.42f, -0.36f), P(c, s, 0.0f, 0.06f), col);
        dl->AddRectFilled(P(c, s, -0.06f, 0.04f), P(c, s, 0.06f, 0.42f), col);
        break;
    case Icon::Lock:
        dl->AddRect(P(c, s, -0.32f, -0.04f), P(c, s, 0.32f, 0.42f), col, 0, 0, th * 1.2f);
        dl->PathArcTo(P(c, s, 0.0f, -0.04f), s * 0.2f, -3.1416f, 0.0f, 12);
        dl->PathStroke(col, 0, th * 1.5f);
        break;
    case Icon::Shift:
        dl->AddLine(P(c, s, -0.3f, 0.0f), P(c, s, 0.3f, 0.0f), col, th * 1.4f);
        arrowHead(dl, P(c, s, 0.42f, 0.0f), ImVec2(1, 0), s * 0.3f, s * 0.17f, col);
        dl->AddLine(P(c, s, -0.42f, -0.36f), P(c, s, -0.42f, 0.36f), col, th * 1.4f);
        break;
    case Icon::Adjust:
        dl->AddLine(P(c, s, -0.34f, 0.34f), P(c, s, 0.3f, -0.3f), col, th * 2.0f);
        dl->AddTriangleFilled(P(c, s, -0.46f, 0.46f), P(c, s, -0.22f, 0.36f), P(c, s, -0.36f, 0.22f), col);
        break;
    case Icon::GroupOpen: case Icon::GroupClosed:
        dl->AddRect(P(c, s, -0.4f, -0.4f), P(c, s, 0.4f, 0.4f), col, 0, 0, th);
        dl->AddLine(P(c, s, -0.22f, 0.0f), P(c, s, 0.22f, 0.0f), col, th * 1.3f);
        if (icon == Icon::GroupClosed) dl->AddLine(P(c, s, 0.0f, -0.22f), P(c, s, 0.0f, 0.22f), col, th * 1.3f);
        break;
    case Icon::Color:
        dl->AddCircleFilled(c, s * 0.36f, IM_COL32(240, 150, 240, 255));
        dl->AddCircleFilled(P(c, s, -0.1f, -0.1f), s * 0.14f, IM_COL32(255, 255, 255, 230));
        break;
    case Icon::Deform: {
        ImVec2 pts[11];
        for (int i = 0; i < 11; i++) pts[i] = P(c, s, -0.42f + 0.084f * i, 0.22f * std::sin(i * 0.9f));
        dl->AddPolyline(pts, 11, col, 0, th * 1.4f);
        break;
    }
    case Icon::Copy:
        dl->AddRect(P(c, s, -0.38f, -0.38f), P(c, s, 0.12f, 0.12f), col, 0, 0, th * 1.2f);
        dl->AddRectFilled(P(c, s, -0.12f, -0.12f), P(c, s, 0.38f, 0.38f), IM_COL32(60, 60, 60, 255));
        dl->AddRect(P(c, s, -0.12f, -0.12f), P(c, s, 0.38f, 0.38f), col, 0, 0, th * 1.2f);
        break;
    case Icon::Cut:
        dl->AddLine(P(c, s, -0.3f, -0.4f), P(c, s, 0.35f, 0.3f), col, th * 1.3f);
        dl->AddLine(P(c, s, -0.3f, 0.4f), P(c, s, 0.35f, -0.3f), col, th * 1.3f);
        dl->AddCircle(P(c, s, -0.3f, -0.32f), s * 0.1f, col, 10, th);
        dl->AddCircle(P(c, s, -0.3f, 0.32f), s * 0.1f, col, 10, th);
        break;
    case Icon::Paste:
        dl->AddRect(P(c, s, -0.36f, -0.3f), P(c, s, 0.36f, 0.42f), col, 0, 0, th * 1.2f);
        dl->AddRectFilled(P(c, s, -0.16f, -0.42f), P(c, s, 0.16f, -0.18f), col);
        break;
    case Icon::CurveLinear:
        dl->AddLine(P(c, s, -0.38f, 0.34f), P(c, s, 0.38f, -0.34f), col, th * 1.4f);
        dl->AddCircleFilled(P(c, s, -0.38f, 0.34f), s * 0.08f, col);
        dl->AddCircleFilled(P(c, s, 0.38f, -0.34f), s * 0.08f, col);
        break;
    case Icon::CurveBezier:
        dl->AddBezierCubic(P(c, s, -0.4f, 0.34f), P(c, s, 0.1f, 0.34f), P(c, s, -0.1f, -0.34f), P(c, s, 0.4f, -0.34f), col, th * 1.4f);
        dl->AddCircleFilled(P(c, s, -0.4f, 0.34f), s * 0.08f, col);
        dl->AddCircleFilled(P(c, s, 0.4f, -0.34f), s * 0.08f, col);
        break;
    case Icon::CurveStepped: {
        ImVec2 pts[5] = {P(c, s, -0.42f, 0.34f), P(c, s, -0.02f, 0.34f), P(c, s, -0.02f, -0.34f), P(c, s, 0.42f, -0.34f), P(c, s, 0.42f, -0.34f)};
        dl->AddPolyline(pts, 5, col, 0, th * 1.4f);
        break;
    }
    case Icon::ShowPixels: dl->AddRect(P(c, s, -0.34f, -0.34f), P(c, s, 0.34f, 0.34f), IM_COL32(90, 200, 210, 255), 0, 0, th * 1.2f); break;
    case Icon::Chevron: dl->AddLine(P(c, s, -0.15f, -0.3f), P(c, s, 0.15f, 0), col, th); dl->AddLine(P(c, s, 0.15f, 0), P(c, s, -0.15f, 0.3f), col, th); break;
    }
}

const std::vector<std::string>& boneIconNames() {
    static const std::vector<std::string> names = {
        "default", "crosshair", "circle", "square", "triangle", "diamond", "hexagon", "ring",
        "ringDot", "target", "plus", "cross", "dash", "bar", "asterisk", "star",
        "heart", "spade", "club", "squareOutline", "triangleOutline", "diamondOutline", "hexagonOutline", "sun",
        "arrowUp", "arrowDown", "arrowLeft", "arrowRight", "arrowUpDown", "arrowLeftRight", "arrows4", "halfCircle",
        "chevronUp", "chevronDown", "chevronLeft", "chevronRight", "drop", "bolt", "flag", "pin"};
    return names;
}

void drawBoneIcon(ImDrawList* dl, const std::string& name, ImVec2 c, float s, ImU32 col) {
    const float th = std::max(1.4f, s * 0.09f);
    auto pt = [&](float x, float y) { return ImVec2(c.x + x * s, c.y + y * s); };
    auto ngon = [&](int n, float rot, float rad, bool fill) {
        std::vector<ImVec2> p;
        for (int i = 0; i < n; i++) {
            float a = rot + 6.2831853f * i / n;
            p.push_back(ImVec2(c.x + std::cos(a) * rad * s, c.y + std::sin(a) * rad * s));
        }
        if (fill) dl->AddConvexPolyFilled(p.data(), n, col);
        else dl->AddPolyline(p.data(), n, col, ImDrawFlags_Closed, th);
    };
    auto arrow = [&](float dx, float dy, bool tail = true) { // unit direction
        ImVec2 tip = pt(dx * 0.46f, dy * 0.46f), base = pt(dx * 0.1f, dy * 0.1f);
        if (tail) dl->AddLine(pt(-dx * 0.46f, -dy * 0.46f), base, col, th * 1.3f);
        else dl->AddLine(pt(-dx * 0.2f, -dy * 0.2f), base, col, th * 1.3f);
        ImVec2 n(-dy, dx);
        dl->AddTriangleFilled(tip, ImVec2(base.x + n.x * s * 0.22f, base.y + n.y * s * 0.22f), ImVec2(base.x - n.x * s * 0.22f, base.y - n.y * s * 0.22f), col);
    };
    auto chevron = [&](float dx, float dy) {
        ImVec2 n(-dy, dx), tip = pt(dx * 0.3f, dy * 0.3f);
        ImVec2 a(c.x - dx * 0.1f * s + n.x * 0.3f * s, c.y - dy * 0.1f * s + n.y * 0.3f * s);
        ImVec2 b(c.x - dx * 0.1f * s - n.x * 0.3f * s, c.y - dy * 0.1f * s - n.y * 0.3f * s);
        dl->AddLine(a, tip, col, th * 1.4f);
        dl->AddLine(tip, b, col, th * 1.4f);
    };
    if (name == "crosshair") {
        dl->AddCircle(c, s * 0.28f, col, 20, th);
        dl->AddLine(pt(-0.46f, 0), pt(-0.12f, 0), col, th); dl->AddLine(pt(0.12f, 0), pt(0.46f, 0), col, th);
        dl->AddLine(pt(0, -0.46f), pt(0, -0.12f), col, th); dl->AddLine(pt(0, 0.12f), pt(0, 0.46f), col, th);
    } else if (name == "circle") dl->AddCircleFilled(c, s * 0.38f, col, 24);
    else if (name == "square") dl->AddRectFilled(pt(-0.34f, -0.34f), pt(0.34f, 0.34f), col);
    else if (name == "triangle") ngon(3, -1.5708f, 0.46f, true);
    else if (name == "diamond") ngon(4, -1.5708f, 0.46f, true);
    else if (name == "hexagon") ngon(6, 0, 0.44f, true);
    else if (name == "ring") dl->AddCircle(c, s * 0.34f, col, 24, th * 1.5f);
    else if (name == "ringDot") { dl->AddCircle(c, s * 0.36f, col, 24, th * 1.3f); dl->AddCircleFilled(c, s * 0.13f, col, 16); }
    else if (name == "target") { dl->AddCircle(c, s * 0.42f, col, 24, th); dl->AddCircle(c, s * 0.26f, col, 20, th); dl->AddCircleFilled(c, s * 0.1f, col, 12); }
    else if (name == "plus") { dl->AddLine(pt(-0.4f, 0), pt(0.4f, 0), col, th * 1.6f); dl->AddLine(pt(0, -0.4f), pt(0, 0.4f), col, th * 1.6f); }
    else if (name == "cross") { dl->AddLine(pt(-0.34f, -0.34f), pt(0.34f, 0.34f), col, th * 1.6f); dl->AddLine(pt(-0.34f, 0.34f), pt(0.34f, -0.34f), col, th * 1.6f); }
    else if (name == "dash") dl->AddLine(pt(-0.4f, 0), pt(0.4f, 0), col, th * 1.8f);
    else if (name == "bar") dl->AddLine(pt(0, -0.42f), pt(0, 0.42f), col, th * 1.8f);
    else if (name == "asterisk") for (int i = 0; i < 3; i++) {
        float a = 3.14159265f * i / 3;
        dl->AddLine(pt(std::cos(a) * 0.4f, std::sin(a) * 0.4f), pt(-std::cos(a) * 0.4f, -std::sin(a) * 0.4f), col, th * 1.4f);
    }
    else if (name == "star") {
        ImVec2 prev;
        for (int i = 0; i <= 10; i++) {
            float a = -1.5708f + 3.14159265f * i / 5, r = (i % 2 == 0) ? 0.46f : 0.19f;
            ImVec2 p(c.x + std::cos(a) * r * s, c.y + std::sin(a) * r * s);
            if (i) dl->AddTriangleFilled(c, prev, p, col);
            prev = p;
        }
    }
    else if (name == "heart") {
        dl->AddCircleFilled(pt(-0.17f, -0.12f), s * 0.21f, col, 16); dl->AddCircleFilled(pt(0.17f, -0.12f), s * 0.21f, col, 16);
        dl->AddTriangleFilled(pt(-0.37f, -0.04f), pt(0.37f, -0.04f), pt(0, 0.42f), col);
    }
    else if (name == "spade") {
        dl->AddTriangleFilled(pt(0, -0.46f), pt(-0.37f, 0.08f), pt(0.37f, 0.08f), col);
        dl->AddCircleFilled(pt(-0.17f, 0.1f), s * 0.2f, col, 16); dl->AddCircleFilled(pt(0.17f, 0.1f), s * 0.2f, col, 16);
        dl->AddLine(pt(0, 0.05f), pt(0, 0.44f), col, th * 1.6f);
    }
    else if (name == "club") {
        dl->AddCircleFilled(pt(0, -0.2f), s * 0.2f, col, 16); dl->AddCircleFilled(pt(-0.2f, 0.1f), s * 0.2f, col, 16); dl->AddCircleFilled(pt(0.2f, 0.1f), s * 0.2f, col, 16);
        dl->AddLine(pt(0, 0), pt(0, 0.44f), col, th * 1.6f);
    }
    else if (name == "squareOutline") dl->AddRect(pt(-0.34f, -0.34f), pt(0.34f, 0.34f), col, 0, 0, th * 1.3f);
    else if (name == "triangleOutline") ngon(3, -1.5708f, 0.46f, false);
    else if (name == "diamondOutline") ngon(4, -1.5708f, 0.46f, false);
    else if (name == "hexagonOutline") ngon(6, 0, 0.44f, false);
    else if (name == "sun") {
        dl->AddCircleFilled(c, s * 0.17f, col, 16);
        for (int i = 0; i < 8; i++) {
            float a = 6.2831853f * i / 8;
            dl->AddLine(pt(std::cos(a) * 0.28f, std::sin(a) * 0.28f), pt(std::cos(a) * 0.45f, std::sin(a) * 0.45f), col, th);
        }
    }
    else if (name == "arrowUp") arrow(0, -1);
    else if (name == "arrowDown") arrow(0, 1);
    else if (name == "arrowLeft") arrow(-1, 0);
    else if (name == "arrowRight") arrow(1, 0);
    else if (name == "arrowUpDown") { arrow(0, -1, false); arrow(0, 1, false); }
    else if (name == "arrowLeftRight") { arrow(-1, 0, false); arrow(1, 0, false); }
    else if (name == "arrows4") { arrow(0, -1, false); arrow(0, 1, false); arrow(-1, 0, false); arrow(1, 0, false); }
    else if (name == "halfCircle") {
        dl->PathArcTo(c, s * 0.4f, 3.14159265f, 6.2831853f, 20);
        dl->PathFillConvex(col);
    }
    else if (name == "chevronUp") chevron(0, -1);
    else if (name == "chevronDown") chevron(0, 1);
    else if (name == "chevronLeft") chevron(-1, 0);
    else if (name == "chevronRight") chevron(1, 0);
    else if (name == "drop") {
        dl->AddCircleFilled(pt(0, 0.14f), s * 0.28f, col, 20);
        dl->AddTriangleFilled(pt(0, -0.46f), pt(-0.26f, 0.06f), pt(0.26f, 0.06f), col);
    }
    else if (name == "bolt") {
        dl->AddTriangleFilled(pt(0.12f, -0.46f), pt(-0.26f, 0.08f), pt(0.04f, 0.08f), col);
        dl->AddTriangleFilled(pt(-0.12f, 0.46f), pt(0.26f, -0.08f), pt(-0.04f, -0.08f), col);
    }
    else if (name == "flag") {
        dl->AddLine(pt(-0.22f, -0.44f), pt(-0.22f, 0.44f), col, th * 1.4f);
        dl->AddTriangleFilled(pt(-0.2f, -0.42f), pt(0.38f, -0.2f), pt(-0.2f, 0.02f), col);
    }
    else if (name == "pin") {
        dl->AddCircleFilled(pt(0, -0.16f), s * 0.22f, col, 18);
        dl->AddLine(pt(0, 0.02f), pt(0, 0.46f), col, th * 1.4f);
    }
    else drawIcon(dl, Icon::Bone, c, s, col); // "default" and unknown names: the ordinary bone
}

bool iconButton(const char* id, Icon icon, const char* label, bool active, ImVec2 size, const char* tooltip, ImU32 iconColor) {
    ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.157f, 0.522f, 0.745f, 1) : ImVec4(0.247f, 0.255f, 0.282f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? ImVec4(0.376f, 0.612f, 1.0f, 1) : ImVec4(0.302f, 0.314f, 0.345f, 1));
    bool clicked = ImGui::Button(id, size);
    ImGui::PopStyleColor(2);
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float is = (mx.y - mn.y) * 0.62f;
    drawIcon(dl, icon, ImVec2(mn.x + is * 0.8f, (mn.y + mx.y) / 2), is, iconColor ? iconColor : IM_COL32(230, 230, 230, 255));
    dl->AddText(ImVec2(mn.x + is * 1.5f, (mn.y + mx.y) / 2 - ImGui::GetTextLineHeight() / 2), IM_COL32(235, 235, 235, 255), label);
    if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

bool iconOnlyButton(const char* id, Icon icon, bool active, float size, const char* tooltip, ImU32 iconColor) {
    ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.157f, 0.522f, 0.745f, 1) : ImVec4(0.247f, 0.255f, 0.282f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? ImVec4(0.376f, 0.612f, 1.0f, 1) : ImVec4(0.302f, 0.314f, 0.345f, 1));
    bool clicked = ImGui::Button(id, ImVec2(size, size));
    ImGui::PopStyleColor(2);
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    drawIcon(ImGui::GetWindowDrawList(), icon, ImVec2((mn.x + mx.x) / 2, (mn.y + mx.y) / 2), size * 0.66f, iconColor ? iconColor : IM_COL32(230, 230, 230, 255));
    if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

} // namespace osteon
