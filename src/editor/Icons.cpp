#include "Icons.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
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
    case Icon::Globe: { // a globe: circle, equator and a meridian
        ImU32 b = IM_COL32(90, 170, 240, 255);
        dl->AddCircle(c, s * 0.4f, b, 20, th * 1.2f);
        dl->AddLine(P(c, s, -0.4f, 0), P(c, s, 0.4f, 0), b, th);
        dl->AddEllipse(c, ImVec2(s * 0.17f, s * 0.4f), b, 0, 14, th);
        break;
    }
    case Icon::DocNew: { // a page with a green plus
        ImVec2 pts[5] = {P(c, s, -0.26f, -0.42f), P(c, s, 0.08f, -0.42f), P(c, s, 0.26f, -0.24f), P(c, s, 0.26f, 0.42f), P(c, s, -0.26f, 0.42f)};
        dl->AddPolyline(pts, 5, IM_COL32(210, 214, 224, 255), ImDrawFlags_Closed, th);
        dl->AddLine(P(c, s, 0.08f, -0.42f), P(c, s, 0.08f, -0.24f), IM_COL32(210, 214, 224, 255), th);
        dl->AddLine(P(c, s, 0.08f, -0.24f), P(c, s, 0.26f, -0.24f), IM_COL32(210, 214, 224, 255), th);
        ImU32 g = IM_COL32(90, 210, 110, 255);
        dl->AddLine(P(c, s, 0.04f, 0.12f), P(c, s, 0.04f, 0.38f), g, th * 1.5f);
        dl->AddLine(P(c, s, -0.09f, 0.25f), P(c, s, 0.17f, 0.25f), g, th * 1.5f);
        break;
    }
    case Icon::Save: case Icon::SaveAs: { // a floppy disk
        ImU32 b = IM_COL32(110, 170, 245, 255);
        dl->AddRect(P(c, s, -0.38f, -0.38f), P(c, s, 0.38f, 0.38f), b, 2.0f, 0, th * 1.1f);
        dl->AddRectFilled(P(c, s, -0.2f, -0.38f), P(c, s, 0.16f, -0.1f), b);
        dl->AddRect(P(c, s, -0.2f, 0.06f), P(c, s, 0.2f, 0.38f), b, 0, 0, th);
        if (icon == Icon::SaveAs) {
            ImU32 o = IM_COL32(250, 170, 70, 255);
            dl->AddLine(P(c, s, 0.18f, 0.46f), P(c, s, 0.44f, 0.2f), o, th * 2.0f);
        }
        break;
    }
    case Icon::Import: case Icon::Export: { // a tray with an arrow into or out of it
        ImVec2 tray[4] = {P(c, s, -0.4f, 0.05f), P(c, s, -0.4f, 0.4f), P(c, s, 0.4f, 0.4f), P(c, s, 0.4f, 0.05f)};
        dl->AddPolyline(tray, 4, IM_COL32(210, 214, 224, 255), 0, th);
        ImU32 a = icon == Icon::Import ? IM_COL32(90, 200, 120, 255) : IM_COL32(240, 160, 70, 255);
        float dir = icon == Icon::Import ? 1.0f : -1.0f;
        dl->AddLine(P(c, s, 0, -0.4f * dir + (dir > 0 ? 0.0f : 0.0f)), P(c, s, 0, 0.18f * dir), a, th * 1.4f);
        float tipY = 0.28f * dir;
        dl->AddTriangleFilled(P(c, s, 0, tipY), P(c, s, -0.15f, tipY - 0.17f * dir), P(c, s, 0.15f, tipY - 0.17f * dir), a);
        break;
    }
    case Icon::Psd: { // Photoshop-style badge
        ImU32 b = IM_COL32(24, 28, 52, 255), t = IM_COL32(49, 168, 255, 255);
        dl->AddRectFilled(P(c, s, -0.42f, -0.4f), P(c, s, 0.42f, 0.4f), b, s * 0.12f);
        dl->AddRect(P(c, s, -0.42f, -0.4f), P(c, s, 0.42f, 0.4f), t, s * 0.12f, 0, th * 1.1f);
        float fs = s * 0.5f;
        ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, 1e9f, 0, "Ps");
        dl->AddText(ImGui::GetFont(), fs, ImVec2(c.x - ts.x / 2, c.y - ts.y / 2), t, "Ps");
        break;
    }
    case Icon::Atlas: { // packed rectangles
        ImU32 b = IM_COL32(200, 205, 220, 255);
        dl->AddRect(P(c, s, -0.4f, -0.4f), P(c, s, 0.4f, 0.4f), b, 0, 0, th);
        dl->AddRectFilled(P(c, s, -0.32f, -0.32f), P(c, s, 0.05f, 0.05f), IM_COL32(110, 170, 245, 255));
        dl->AddRectFilled(P(c, s, 0.12f, -0.32f), P(c, s, 0.32f, -0.05f), IM_COL32(240, 170, 80, 255));
        dl->AddRectFilled(P(c, s, -0.32f, 0.12f), P(c, s, -0.08f, 0.32f), IM_COL32(110, 210, 140, 255));
        dl->AddRectFilled(P(c, s, 0.0f, 0.12f), P(c, s, 0.32f, 0.32f), IM_COL32(200, 130, 230, 255));
        break;
    }
    case Icon::Gear: {
        ImU32 b = IM_COL32(200, 205, 220, 255);
        dl->AddCircle(c, s * 0.2f, b, 16, th * 1.2f);
        for (int i = 0; i < 8; i++) {
            float a = i * 0.7853982f;
            dl->AddLine(ImVec2(c.x + std::cos(a) * s * 0.28f, c.y + std::sin(a) * s * 0.28f), ImVec2(c.x + std::cos(a) * s * 0.42f, c.y + std::sin(a) * s * 0.42f), b, th * 1.6f);
        }
        dl->AddCircle(c, s * 0.3f, b, 20, th * 0.9f);
        break;
    }
    case Icon::Exit: { // a door with an arrow leaving it
        ImU32 b = IM_COL32(210, 214, 224, 255), r = IM_COL32(235, 100, 100, 255);
        ImVec2 door[4] = {P(c, s, 0.0f, -0.4f), P(c, s, -0.4f, -0.4f), P(c, s, -0.4f, 0.4f), P(c, s, 0.0f, 0.4f)};
        dl->AddPolyline(door, 4, b, 0, th);
        dl->AddLine(P(c, s, -0.12f, 0), P(c, s, 0.34f, 0), r, th * 1.4f);
        dl->AddTriangleFilled(P(c, s, 0.46f, 0), P(c, s, 0.28f, -0.14f), P(c, s, 0.28f, 0.14f), r);
        break;
    }
    case Icon::Undo: case Icon::Redo: { // a curved arrow
        ImU32 b = IM_COL32(210, 214, 224, 255);
        float dir = icon == Icon::Undo ? -1.0f : 1.0f; // the head is at the left for Undo, the right for Redo
        ImVec2 ctr = P(c, s, 0, 0.1f);
        dl->PathClear();
        if (dir < 0) dl->PathArcTo(ctr, s * 0.3f, -3.0f, 0.4f, 14); else dl->PathArcTo(ctr, s * 0.3f, 3.1416f - 0.4f, 6.1f, 14);
        dl->PathStroke(b, 0, th * 1.4f);
        float hx = dir * 0.3f; // the head sits at the arc's end and points down
        dl->AddTriangleFilled(P(c, s, hx, 0.26f), P(c, s, hx - 0.15f, 0.04f), P(c, s, hx + 0.15f, 0.04f), b);
        break;
    }
    case Icon::Question: {
        ImU32 b = IM_COL32(110, 170, 245, 255);
        dl->AddCircle(c, s * 0.4f, b, 20, th * 1.2f);
        float fs = s * 0.55f;
        ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, 1e9f, 0, "?");
        dl->AddText(ImGui::GetFont(), fs, ImVec2(c.x - ts.x / 2, c.y - ts.y / 2), b, "?");
        break;
    }
    case Icon::Pencil: {
        ImU32 o = IM_COL32(250, 170, 70, 255);
        dl->AddLine(P(c, s, -0.34f, 0.34f), P(c, s, 0.3f, -0.3f), o, th * 2.4f);
        dl->AddTriangleFilled(P(c, s, -0.42f, 0.42f), P(c, s, -0.34f, 0.2f), P(c, s, -0.2f, 0.34f), IM_COL32(220, 224, 235, 255));
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

static const ImVec4 kAccent(0.231f, 0.510f, 0.965f, 1), kAccentHi(0.376f, 0.612f, 1.0f, 1);

bool iconButton(const char* id, Icon icon, const char* label, bool active, ImVec2 size, const char* tooltip, ImU32 iconColor) {
    ImGui::PushStyleColor(ImGuiCol_Button, active ? kAccent : ImVec4(0.235f, 0.243f, 0.270f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? kAccentHi : ImVec4(0.290f, 0.302f, 0.337f, 1));
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
    // Ghost style: no fill until hovered, accent fill when on.
    ImGui::PushStyleColor(ImGuiCol_Button, active ? kAccent : ImVec4(1, 1, 1, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? kAccentHi : ImVec4(1, 1, 1, 0.10f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active ? kAccentHi : ImVec4(1, 1, 1, 0.18f));
    bool clicked = ImGui::Button(id, ImVec2(size, size));
    ImGui::PopStyleColor(3);
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    bool off = ImGui::GetStyle().Alpha < 0.99f; // inside BeginDisabled
    ImU32 col = iconColor ? iconColor : (active ? IM_COL32(255, 255, 255, 255) : IM_COL32(214, 218, 228, 255));
    if (off) col = (col & 0x00FFFFFF) | (90u << 24);
    drawIcon(ImGui::GetWindowDrawList(), icon, ImVec2((mn.x + mx.x) / 2, (mn.y + mx.y) / 2), size * 0.62f, col);
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

bool roundIconButton(const char* id, Icon icon, bool active, float size, const char* tooltip, ImU32 fill) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, size * 0.5f);
    ImVec4 f = ImGui::ColorConvertU32ToFloat4(fill);
    auto scaled = [&](float k) { return ImVec4(std::min(1.0f, f.x * k), std::min(1.0f, f.y * k), std::min(1.0f, f.z * k), f.w); };
    ImGui::PushStyleColor(ImGuiCol_Button, f);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, scaled(1.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, scaled(0.85f));
    bool clicked = ImGui::Button(id, ImVec2(size, size));
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    drawIcon(ImGui::GetWindowDrawList(), icon, ImVec2((mn.x + mx.x) / 2, (mn.y + mx.y) / 2), size * 0.5f, IM_COL32(255, 255, 255, 255));
    if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    (void)active;
    return clicked;
}

void ButtonGroup::begin() {
    min = ImGui::GetCursorScreenPos();
    split.Split(ImGui::GetWindowDrawList(), 2);
    split.SetCurrentChannel(ImGui::GetWindowDrawList(), 1);
    ImGui::BeginGroup();
}

void ButtonGroup::end(float pad) {
    ImGui::EndGroup();
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    split.SetCurrentChannel(ImGui::GetWindowDrawList(), 0);
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(mn.x - pad, mn.y - pad), ImVec2(mx.x + pad, mx.y + pad), IM_COL32(255, 255, 255, 14), 7.0f * ImGui::GetStyle().FrameRounding / 5.0f);
    split.Merge(ImGui::GetWindowDrawList());
    ImGui::SameLine(0, pad * 2 + 4);
}


// ---- Modern slider ----------------------------------------------------------------------------

static bool gSliderAct = false, gSliderDeact = false, gSliderHover = false, gSliderDeactAny = false;
bool sliderDeactivated() { return gSliderDeactAny; }
bool sliderActivated() { return gSliderAct; }
bool sliderDeactivatedAfterEdit() { return gSliderDeact; }
bool sliderHovered() { return gSliderHover; }

// Splits "Weight %.0f%%" into "Weight ", "%.0f", "%".
static void splitFormat(const char* fmt, std::string& prefix, std::string& spec, std::string& suffix) {
    std::string f = fmt ? fmt : "";
    size_t i = 0;
    while (i < f.size()) {
        if (f[i] == '%' && i + 1 < f.size() && f[i + 1] == '%') { prefix += '%'; i += 2; continue; }
        if (f[i] == '%') break;
        prefix += f[i++];
    }
    size_t j = i;
    while (j < f.size() && !std::strchr("fdgi", f[j])) j++;
    if (j < f.size()) j++;
    spec = f.substr(i, j - i);
    for (size_t k = j; k < f.size(); k++) {
        if (f[k] == '%' && k + 1 < f.size() && f[k + 1] == '%') { suffix += '%'; k++; }
        else suffix += f[k];
    }
}

static std::string trimmed(const std::string& s) {
    size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static bool sliderCore(const char* label, float* v, float lo, float hi, const char* fmt, bool isInt) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStorage* st = ImGui::GetStateStorage();
    const float u = ImGui::GetFrameHeight() / 22.0f;
    std::string prefix, spec, suffix;
    splitFormat(fmt, prefix, spec, suffix);
    const char* hash = std::strstr(label, "##");
    std::string text = hash ? std::string(label, hash) : std::string(label);
    if (text.empty()) text = trimmed(prefix);
    if (!text.empty() && !trimmed(suffix).empty()) text += " " + trimmed(suffix);
    if (spec.empty()) spec = isInt ? "%d" : "%.2f";

    const float total = ImGui::CalcItemWidth();
    const float boxW = 58 * u, gap = 8 * u;
    const float trackW = std::max(30 * u, total - boxW - gap);
    ImGui::PushID(label);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetFrameHeight();
    ImGui::InvisibleButton("##track", ImVec2(trackW, h));
    const ImGuiID tid = ImGui::GetItemID();
    const bool tAct = ImGui::IsItemActivated(), tHover = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    bool changed = false;
    const float knobR = 6.5f * u, x0 = p.x + knobR, x1 = p.x + trackW - knobR;
    if (active && hi > lo) {
        float t = std::min(1.0f, std::max(0.0f, (io.MousePos.x - x0) / std::max(1.0f, x1 - x0)));
        float nv = lo + t * (hi - lo);
        if (isInt) nv = std::round(nv);
        if (nv != *v) { *v = nv; changed = true; st->SetBool(tid, true); }
    }
    const bool tDeact = ImGui::IsItemDeactivated();
    const bool tEdited = st->GetBool(tid, false);
    if (tDeact) st->SetBool(tid, false);

    // Draw: a thin track, the filled part in the accent colour, and a round knob.
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float t = hi > lo ? std::min(1.0f, std::max(0.0f, (*v - lo) / (hi - lo))) : 0;
        float cy = p.y + h * 0.5f, kx = x0 + t * (x1 - x0), th = 3 * u;
        dl->AddRectFilled(ImVec2(p.x + 2 * u, cy - th / 2), ImVec2(p.x + trackW - 2 * u, cy + th / 2), IM_COL32(255, 255, 255, 40), th);
        dl->AddRectFilled(ImVec2(p.x + 2 * u, cy - th / 2), ImVec2(kx, cy + th / 2), IM_COL32(59, 130, 246, 255), th);
        float r = knobR * (active ? 1.12f : tHover ? 1.06f : 1.0f);
        dl->AddCircleFilled(ImVec2(kx, cy + 0.5f * u), r + 1.5f * u, IM_COL32(0, 0, 0, 70), 24); // soft shadow
        dl->AddCircleFilled(ImVec2(kx, cy), r, active ? IM_COL32(255, 255, 255, 255) : IM_COL32(226, 230, 240, 255), 24);
        dl->AddCircle(ImVec2(kx, cy), r, IM_COL32(59, 130, 246, active ? 255 : 150), 24, 1.5f * u);
    }

    ImGui::SameLine(0, gap);
    ImGui::SetNextItemWidth(boxW);
    float tmp = *v;
    bool boxChanged;
    if (isInt) { int iv = (int)std::lround(*v); boxChanged = ImGui::InputInt("##box", &iv, 0, 0); tmp = (float)iv; }
    else boxChanged = ImGui::InputFloat("##box", &tmp, 0, 0, spec.c_str());
    const bool bAct = ImGui::IsItemActivated(), bHover = ImGui::IsItemHovered(), bDeactEdit = ImGui::IsItemDeactivatedAfterEdit(), bDeact = ImGui::IsItemDeactivated();
    if (boxChanged && tmp != *v) { *v = tmp; changed = true; } // live while typing, range-checked when the box is left
    if (bDeact && (*v < lo || *v > hi)) { *v = std::min(hi, std::max(lo, *v)); changed = true; }
    if (!text.empty()) {
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text.c_str());
    }
    ImGui::PopID();
    gSliderAct = tAct || bAct;
    gSliderDeact = (tDeact && tEdited) || bDeactEdit;
    gSliderHover = tHover || bHover;
    gSliderDeactAny = tDeact || bDeact;
    return changed;
}

bool sliderFloat(const char* label, float* v, float lo, float hi, const char* format) { return sliderCore(label, v, lo, hi, format, false); }

bool sliderInt(const char* label, int* v, int lo, int hi, const char* format) {
    float f = (float)*v;
    bool changed = sliderCore(label, &f, (float)lo, (float)hi, format, true);
    if (changed) *v = (int)std::lround(f);
    return changed;
}


// ---- Menu entries with icons --------------------------------------------------------------------

static std::string paddedForIcon(const char* label) {
    float w = ImGui::GetFontSize() + 10;
    float sp = std::max(1.0f, ImGui::CalcTextSize(" ").x);
    return std::string((size_t)std::ceil(w / sp), ' ') + label;
}

static void drawMenuIcon(Icon icon, bool enabled) {
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    float fs = ImGui::GetFontSize();
    drawIcon(ImGui::GetWindowDrawList(), icon, ImVec2(mn.x + fs * 0.95f, (mn.y + mx.y) / 2), fs * 1.1f, enabled ? IM_COL32(220, 224, 235, 255) : IM_COL32(120, 124, 135, 255));
}

bool iconMenuItem(Icon icon, const char* label, const char* shortcut, bool selected, bool enabled) {
    std::string padded = paddedForIcon(label);
    bool r = ImGui::MenuItem(padded.c_str(), shortcut, selected, enabled);
    drawMenuIcon(icon, enabled);
    return r;
}

bool iconBeginMenu(Icon icon, const char* label, bool enabled) {
    std::string padded = paddedForIcon(label);
    bool r = ImGui::BeginMenu(padded.c_str(), enabled);
    drawMenuIcon(icon, enabled);
    return r;
}

} // namespace osteon
