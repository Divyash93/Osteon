// The main editor viewport: rendering, overlays (bones, gizmos, meshes), picking and tools.
#include "App.h"
#include "Geometry.h"
#include "imgui_internal.h"

namespace osteon {

static ImU32 toU32(const Color& c, float a = 1) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a * a)); }
static const ImU32 kSelect = IM_COL32(90, 210, 255, 255);
static const ImU32 kSelectFill = IM_COL32(90, 210, 255, 90);

static float dist(ImVec2 a, ImVec2 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }

int Editor::pickBone(ImVec2 s) {
    if (!showBones || !selectBones) return -1;
    int best = -1;
    float bestD = 8;
    for (int i = (int)doc.sk.bones.size() - 1; i >= 0; i--) {
        const Bone& b = doc.sk.bones[i];
        if (!b.active || !boneVisible(i)) continue;
        ImVec2 o = toScreen({b.worldX, b.worldY});
        float len = doc.data.bones[i].length;
        float d = dist(o, s) - 2; // origins win over segments
        if (len > 0) {
            ImVec2 t = toScreen(b.localToWorld({len, 0}));
            d = std::min(d, distToSegment({s.x, s.y}, {o.x, o.y}, {t.x, t.y}));
        }
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

int Editor::pickSlot(ImVec2 s) {
    if (!showImages || !selectImages) return -1;
    Vec2 w = toWorld(s);
    std::vector<float> v;
    for (int k = (int)doc.sk.drawOrder.size() - 1; k >= 0; k--) {
        int si = doc.sk.drawOrder[k];
        if (!slotVisible(si)) continue;
        const Slot& slot = doc.sk.slots[si];
        const Attachment* a = doc.sk.attachmentFor(slot);
        if (!a) continue;
        if (!doc.sk.worldVertices(slot, *a, v)) continue;
        if (a->type == AttachmentType::Region) {
            if (pointInPolygon(w.x, w.y, v)) return si;
        } else if (a->type == AttachmentType::Mesh || a->type == AttachmentType::LinkedMesh) {
            const Attachment* g = a;
            if (a->type == AttachmentType::LinkedMesh) g = doc.data.findAttachment(doc.data.findSkin(a->parentSkin), si, a->parentMesh);
            if (!g) continue;
            for (size_t t = 0; t + 2 < g->triangles.size(); t += 3) {
                std::vector<float> tri = {v[g->triangles[t] * 2], v[g->triangles[t] * 2 + 1], v[g->triangles[t + 1] * 2],
                                          v[g->triangles[t + 1] * 2 + 1], v[g->triangles[t + 2] * 2], v[g->triangles[t + 2] * 2 + 1]};
                if (pointInPolygon(w.x, w.y, tri)) return si;
            }
        } else if (showOthers && selectOthers && (a->type == AttachmentType::BoundingBox || a->type == AttachmentType::Clipping)) {
            if (pointInPolygon(w.x, w.y, v)) return si;
        }
    }
    return -1;
}

void Editor::viewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    vpOrigin = ImGui::GetCursorScreenPos();
    vpSize = ImGui::GetContentRegionAvail();
    if (vpSize.x < 4 || vpSize.y < 4) { ImGui::End(); return; }
    if (fitRequested) { zoomToFit(); fitRequested = false; }

    gfx.ensureTarget(viewRT, (int)vpSize.x, (int)vpSize.y);
    RenderOptions opt = renderOptions();
    gfx.begin(viewRT, camX, camY, zoom, opt);
    auto imagePath = [this](const Attachment& a) { return doc.imagePathFor(a); };
    if (ghosting && doc.mode == Mode::Animate && doc.currentAnimation()) {
        Skeleton ghost(doc.data);
        ghost.skin = doc.skin;
        ghost.updateCacheOrder();
        float fps = doc.data.fps;
        for (int i = ghostFrames; i >= 1; i--) {
            for (int dir : {-1, 1}) {
                float t = doc.time + dir * i * ghostStep / fps;
                if (t < 0) continue;
                ghost.setToSetupPose();
                applyAnimation(*doc.currentAnimation(), ghost, t, t, false, nullptr, 1, MixBlend::Setup);
                ghost.updateWorldTransform(Physics::Pose);
                RenderOptions g = opt;
                g.alphaScale = ghostAlpha * (1 - (i - 1) / (float)ghostFrames);
                g.tint = dir < 0 ? Color(0.55f, 0.75f, 1, 1) : Color(1, 0.65f, 0.55f, 1);
                gfx.drawSkeleton(ghost, imagePath, g);
            }
        }
    }
    if (showImages) gfx.drawSkeleton(doc.sk, imagePath, opt);
    gfx.end();
    ImGui::Image((ImTextureID)viewRT.srv, vpSize);
    vpHovered = ImGui::IsItemHovered();

    // Accept images dragged from the tree's Images node.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("OSTEON_IMAGE")) {
            std::string name((const char*)p->Data, p->DataSize);
            Vec2 w = toWorld(ImGui::GetMousePos());
            doc.checkpoint("New attachment");
            int bone = doc.selectedBones().empty() ? 0 : doc.selectedBones().back();
            int si = doc.createSlot(bone, fileName(name));
            doc.updatePose();
            doc.addRegionAttachment(si, name, w);
            doc.selection = {{SelType::Attachment, si, name}};
        }
        ImGui::EndDragDropTarget();
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(vpOrigin, ImVec2(vpOrigin.x + vpSize.x, vpOrigin.y + vpSize.y), true);
    drawOverlay(dl);
    viewportInput();
    // Mode label, top-left (click toggles like Spine).
    ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + 10, vpOrigin.y + 8));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, doc.mode == Mode::Setup ? ImVec4(0.75f, 0.75f, 0.75f, 1) : ImVec4(0.35f, 0.75f, 1, 1));
    ImGui::SetWindowFontScale(1.35f);
    if (ImGui::Button(doc.mode == Mode::Setup ? "SETUP" : "ANIMATE")) doc.setMode(doc.mode == Mode::Setup ? Mode::Animate : Mode::Setup);
    ImGui::SetWindowFontScale(1);
    ImGui::PopStyleColor(2);
    if (doc.mode == Mode::Animate && doc.currentAnimation()) {
        ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + 14, vpOrigin.y + 42));
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "%s   frame %d", doc.currentAnimation()->name.c_str(), (int)std::round(doc.time * doc.data.fps));
    }
    if (pickParent) {
        ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + vpSize.x / 2 - 120, vpOrigin.y + 10));
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Click the new parent bone (Esc cancels)");
    }
    viewportToolbar();
    dl->PopClipRect();
    ImGui::End();
}

void Editor::drawOverlay(ImDrawList* dl) {
    const SkeletonData& d = doc.data;
    const Skeleton& sk = doc.sk;
    ImVec2 o = toScreen({0, 0});
    if (showAxes) {
        dl->AddLine(ImVec2(vpOrigin.x, o.y), ImVec2(vpOrigin.x + vpSize.x, o.y), IM_COL32(255, 255, 255, 40));
        dl->AddLine(ImVec2(o.x, vpOrigin.y), ImVec2(o.x, vpOrigin.y + vpSize.y), IM_COL32(255, 255, 255, 40));
    }
    if (showRulers) {
        float step = 10;
        while (step * zoom < 8) step *= 10;
        while (step * zoom > 80) step /= 10;
        Vec2 a = toWorld(vpOrigin), b = toWorld(ImVec2(vpOrigin.x + vpSize.x, vpOrigin.y + vpSize.y));
        dl->AddRectFilled(vpOrigin, ImVec2(vpOrigin.x + vpSize.x, vpOrigin.y + 16), IM_COL32(30, 30, 30, 220));
        dl->AddRectFilled(vpOrigin, ImVec2(vpOrigin.x + 16, vpOrigin.y + vpSize.y), IM_COL32(30, 30, 30, 220));
        for (float x = std::floor(a.x / step) * step; x < b.x; x += step) {
            float sx = toScreen({x, 0}).x;
            bool major = std::fmod(std::fabs(std::round(x / step)), 10) == 0;
            dl->AddLine(ImVec2(sx, vpOrigin.y + (major ? 0 : 10)), ImVec2(sx, vpOrigin.y + 16), IM_COL32(160, 160, 160, 255));
            if (major) dl->AddText(ImVec2(sx + 2, vpOrigin.y), IM_COL32(160, 160, 160, 255), std::to_string((int)x).c_str());
        }
        for (float y = std::floor(b.y / step) * step; y < a.y; y += step) {
            float sy = toScreen({0, y}).y;
            bool major = std::fmod(std::fabs(std::round(y / step)), 10) == 0;
            dl->AddLine(ImVec2(vpOrigin.x + (major ? 0 : 10), sy), ImVec2(vpOrigin.x + 16, sy), IM_COL32(160, 160, 160, 255));
        }
    }

    std::vector<float> v;
    int selSlot = -1;
    Attachment* selAtt = selectedAttachment(&selSlot);
    // Other attachments and the selected attachment's outline.
    for (int si : sk.drawOrder) {
        const Slot& slot = sk.slots[si];
        if (!sk.bones[slot.bone].active || !boneVisible(slot.bone) || !d.slots[si].visible) continue;
        const Attachment* a = sk.attachmentFor(slot);
        if (!a || !sk.worldVertices(slot, *a, v)) continue;
        bool slotSel = doc.isSelected({SelType::Slot, si, ""}) || (selSlot == si && selAtt && selAtt->name == a->name);
        auto poly = [&](ImU32 col, float th) {
            for (size_t i = 0; i + 1 < v.size(); i += 2) {
                size_t j = (i + 2) % v.size();
                dl->AddLine(toScreen({v[i], v[i + 1]}), toScreen({v[j], v[j + 1]}), col, th);
            }
        };
        switch (a->type) {
        case AttachmentType::Region:
            if (slotSel) poly(kSelect, 1.5f);
            break;
        case AttachmentType::Mesh:
        case AttachmentType::LinkedMesh: {
            const Attachment* g = a->type == AttachmentType::LinkedMesh ? d.findAttachment(d.findSkin(a->parentSkin), si, a->parentMesh) : a;
            if (!g || !slotSel) break;
            bool editing = meshMode != MeshMode::Off && selSlot == si;
            ImU32 wire = editing ? IM_COL32(255, 255, 255, 120) : IM_COL32(90, 210, 255, 90);
            for (size_t t = 0; t + 2 < g->triangles.size(); t += 3)
                for (int e = 0; e < 3; e++) {
                    int i0 = g->triangles[t + e], i1 = g->triangles[t + (e + 1) % 3];
                    dl->AddLine(toScreen({v[i0 * 2], v[i0 * 2 + 1]}), toScreen({v[i1 * 2], v[i1 * 2 + 1]}), wire);
                }
            for (int h = 0; h < g->hullLength; h++) {
                int j = (h + 1) % g->hullLength;
                dl->AddLine(toScreen({v[h * 2], v[h * 2 + 1]}), toScreen({v[j * 2], v[j * 2 + 1]}), kSelect, 1.5f);
            }
            if (editing || doc.tool == Tool::Weights) {
                // Weight heat map for the weights tool, plain dots otherwise.
                std::vector<float> wv(g->vertexCount(), 0.0f);
                if (doc.tool == Tool::Weights && weightBone >= 0 && g->isWeighted()) {
                    size_t bi = 0, vi = 0;
                    for (int k = 0; k < g->vertexCount(); k++) {
                        int n = g->bones[bi++];
                        for (int q = 0; q < n; q++, vi += 3) if (g->bones[bi++] == weightBone) wv[k] = g->vertices[vi + 2];
                    }
                }
                for (int k = 0; k < g->vertexCount(); k++) {
                    ImVec2 p = toScreen({v[k * 2], v[k * 2 + 1]});
                    bool vs = std::find(meshSelection.begin(), meshSelection.end(), k) != meshSelection.end();
                    ImU32 col = vs ? IM_COL32(255, 200, 40, 255) : IM_COL32(255, 255, 255, 230);
                    if (doc.tool == Tool::Weights && weightBone >= 0) {
                        float w = wv[k];
                        col = ImGui::ColorConvertFloat4ToU32(ImVec4(w, 0.2f + 0.3f * (1 - std::fabs(w - 0.5f) * 2), 1 - w, 1));
                    }
                    dl->AddCircleFilled(p, vs ? 4.5f : 3.5f, col);
                    dl->AddCircle(p, vs ? 4.5f : 3.5f, IM_COL32(0, 0, 0, 200));
                }
            }
            break;
        }
        case AttachmentType::BoundingBox:
        case AttachmentType::Clipping:
            if (showOthers) poly(slotSel ? kSelect : toU32(a->color), slotSel ? 2.0f : 1.0f);
            break;
        case AttachmentType::Path:
            if (showOthers) {
                ImU32 col = slotSel ? kSelect : toU32(a->color);
                size_t n = v.size() / 2;
                // Vertices are [in, point, out] triples; draw each cubic between consecutive points.
                size_t curves = a->closed ? n / 3 : n / 3 - 1;
                for (size_t c = 0; c < curves; c++) {
                    size_t p0 = c * 3 + 1, h0 = c * 3 + 2, h1 = (c * 3 + 3) % n, p1 = (c * 3 + 4) % n;
                    dl->AddBezierCubic(toScreen({v[p0 * 2], v[p0 * 2 + 1]}), toScreen({v[h0 * 2], v[h0 * 2 + 1]}),
                                       toScreen({v[h1 * 2], v[h1 * 2 + 1]}), toScreen({v[p1 * 2], v[p1 * 2 + 1]}), col, 2);
                }
                if (slotSel)
                    for (size_t k = 0; k < n; k++) dl->AddCircleFilled(toScreen({v[k * 2], v[k * 2 + 1]}), k % 3 == 1 ? 4.0f : 2.5f, col);
            }
            break;
        case AttachmentType::Point:
            if (showOthers) {
                ImVec2 p = toScreen({v[0], v[1]});
                ImU32 col = slotSel ? kSelect : toU32(a->color);
                dl->AddCircle(p, 6, col, 0, 2);
                float r = a->rotation;
                const Bone& b = sk.bones[slot.bone];
                Vec2 dir = b.localToWorld({a->x + cosDeg(r) * 20 / zoom, a->y + sinDeg(r) * 20 / zoom});
                dl->AddLine(p, toScreen(dir), col, 2);
            }
            break;
        }
        if (showImageNames && (a->type == AttachmentType::Region || a->type == AttachmentType::Mesh)) {
            float cx = 0, cy = 0;
            for (size_t i = 0; i + 1 < v.size(); i += 2) { cx += v[i]; cy += v[i + 1]; }
            int n = (int)v.size() / 2;
            if (n) dl->AddText(toScreen({cx / n, cy / n}), IM_COL32(255, 255, 255, 200), a->name.c_str());
        }
    }

    // IK constraint guides.
    for (size_t i = 0; i < d.ik.size(); i++) {
        if (!showBones) break;
        const auto& c = d.ik[i];
        const Bone& end = sk.bones[c.bones.back()];
        Vec2 tip = end.localToWorld({d.bones[c.bones.back()].length, 0});
        const Bone& t = sk.bones[c.target];
        bool sel = doc.isSelected({SelType::Ik, (int)i, ""});
        dl->AddLine(toScreen(tip), toScreen({t.worldX, t.worldY}), sel ? kSelect : IM_COL32(255, 90, 90, 160), 1);
        ImVec2 tp = toScreen({t.worldX, t.worldY});
        dl->AddCircle(tp, 9, sel ? kSelect : IM_COL32(255, 90, 90, 220), 0, 2);
        dl->AddLine(ImVec2(tp.x - 12, tp.y), ImVec2(tp.x + 12, tp.y), IM_COL32(255, 90, 90, 220));
        dl->AddLine(ImVec2(tp.x, tp.y - 12), ImVec2(tp.x, tp.y + 12), IM_COL32(255, 90, 90, 220));
    }

    // Bones.
    if (showBones) {
        for (size_t i = 0; i < sk.bones.size(); i++) {
            const Bone& b = sk.bones[i];
            if (!b.active || !boneVisible((int)i)) continue;
            const BoneData& bd = d.bones[i];
            bool sel = doc.isSelected({SelType::Bone, (int)i, ""});
            bool pend = doc.pending.count((int)i) > 0;
            ImU32 col = sel ? kSelect : pend ? IM_COL32(255, 150, 40, 255) : toU32(bd.color);
            ImU32 fill = sel ? kSelectFill : pend ? IM_COL32(255, 150, 40, 70) : toU32(bd.color, 0.35f);
            ImVec2 p0 = toScreen({b.worldX, b.worldY});
            if (bd.length > 0) {
                ImVec2 p1 = toScreen(b.localToWorld({bd.length, 0}));
                float L = dist(p0, p1);
                if (L > 2) {
                    ImVec2 dir((p1.x - p0.x) / L, (p1.y - p0.y) / L), n(-dir.y, dir.x);
                    float w = clampf(L * 0.12f, 2.5f, 9);
                    ImVec2 m(p0.x + dir.x * L * 0.18f, p0.y + dir.y * L * 0.18f);
                    ImVec2 pts[4] = {p0, ImVec2(m.x + n.x * w, m.y + n.y * w), p1, ImVec2(m.x - n.x * w, m.y - n.y * w)};
                    dl->AddConvexPolyFilled(pts, 4, fill);
                    dl->AddPolyline(pts, 4, col, ImDrawFlags_Closed, sel ? 2.0f : 1.2f);
                }
            } else {
                dl->AddCircle(p0, 7, col, 0, 1.5f);
                dl->AddLine(ImVec2(p0.x - 9, p0.y), ImVec2(p0.x + 9, p0.y), col);
                dl->AddLine(ImVec2(p0.x, p0.y - 9), ImVec2(p0.x, p0.y + 9), col);
            }
            dl->AddCircleFilled(p0, 3.2f, col);
            if (showBoneNames || sel) dl->AddText(ImVec2(p0.x + 6, p0.y - 18), IM_COL32(230, 230, 230, 220), bd.name.c_str());
        }
        // Tool gizmo on the primary selected bone.
        Sel p = doc.primary();
        if (p.type == SelType::Bone && p.index < (int)sk.bones.size()) {
            const Bone& b = sk.bones[p.index];
            ImVec2 c = toScreen({b.worldX, b.worldY});
            float ang = -b.worldRotationX() * degRad; // screen space y down
            if (doc.axes == Axes::World) ang = 0;
            else if (doc.axes == Axes::Parent) ang = -(b.parent >= 0 ? sk.bones[b.parent].worldRotationX() : 0) * degRad;
            ImVec2 ax(std::cos(ang), std::sin(ang)), ay(std::sin(ang), -std::cos(ang));
            ImU32 red = IM_COL32(230, 70, 70, 255), green = IM_COL32(90, 210, 90, 255);
            switch (doc.tool) {
            case Tool::Rotate: dl->AddCircle(c, 34, IM_COL32(90, 210, 255, 160), 48, 1.5f); break;
            case Tool::Translate:
                dl->AddLine(c, ImVec2(c.x + ax.x * 40, c.y + ax.y * 40), red, 2);
                dl->AddLine(c, ImVec2(c.x + ay.x * 40, c.y + ay.y * 40), green, 2);
                dl->AddTriangleFilled(ImVec2(c.x + ax.x * 46, c.y + ax.y * 46), ImVec2(c.x + ax.x * 38 + ay.x * 4, c.y + ax.y * 38 + ay.y * 4),
                                      ImVec2(c.x + ax.x * 38 - ay.x * 4, c.y + ax.y * 38 - ay.y * 4), red);
                dl->AddTriangleFilled(ImVec2(c.x + ay.x * 46, c.y + ay.y * 46), ImVec2(c.x + ay.x * 38 + ax.x * 4, c.y + ay.y * 38 + ax.y * 4),
                                      ImVec2(c.x + ay.x * 38 - ax.x * 4, c.y + ay.y * 38 - ax.y * 4), green);
                break;
            case Tool::Scale:
                dl->AddLine(c, ImVec2(c.x + ax.x * 40, c.y + ax.y * 40), red, 2);
                dl->AddLine(c, ImVec2(c.x + ay.x * 40, c.y + ay.y * 40), green, 2);
                dl->AddRectFilled(ImVec2(c.x + ax.x * 40 - 4, c.y + ax.y * 40 - 4), ImVec2(c.x + ax.x * 40 + 4, c.y + ax.y * 40 + 4), red);
                dl->AddRectFilled(ImVec2(c.x + ay.x * 40 - 4, c.y + ay.y * 40 - 4), ImVec2(c.x + ay.x * 40 + 4, c.y + ay.y * 40 + 4), green);
                break;
            case Tool::Shear:
                dl->AddLine(c, ImVec2(c.x + ax.x * 40, c.y + ax.y * 40), red, 2);
                dl->AddLine(c, ImVec2(c.x + ay.x * 40, c.y + ay.y * 40), green, 2);
                dl->AddCircle(ImVec2(c.x + ax.x * 40, c.y + ax.y * 40), 5, red, 0, 2);
                dl->AddCircle(ImVec2(c.x + ay.x * 40, c.y + ay.y * 40), 5, green, 0, 2);
                break;
            default: break;
            }
        }
    }

    // Create-tool preview and box selection.
    if (drag.active && drag.kind == 4) {
        ImVec2 a = toScreen(drag.startWorld), b = ImGui::GetMousePos();
        dl->AddLine(a, b, kSelect, 2);
        dl->AddCircleFilled(a, 3.5f, kSelect);
    }
    if (drag.active && drag.kind == 3) {
        ImVec2 a = drag.startScreen, b = ImGui::GetMousePos();
        dl->AddRectFilled(ImVec2(std::min(a.x, b.x), std::min(a.y, b.y)), ImVec2(std::max(a.x, b.x), std::max(a.y, b.y)), IM_COL32(90, 210, 255, 30));
        dl->AddRect(ImVec2(std::min(a.x, b.x), std::min(a.y, b.y)), ImVec2(std::max(a.x, b.x), std::max(a.y, b.y)), kSelect);
    }
    if (doc.tool == Tool::Weights && vpHovered) dl->AddCircle(ImGui::GetMousePos(), brushSize, IM_COL32(255, 255, 255, 140), 40);
}

// Returns the world-space positions of mesh vertices for the selected mesh (for editing).
static bool meshWorld(Document& doc, int slot, const Attachment& a, std::vector<float>& out) {
    return doc.sk.worldVertices(doc.sk.slots[slot], a, out);
}

void Editor::viewportInput() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;
    Vec2 mw = toWorld(mouse);

    // Pan (right / middle drag, or J + left drag) and zoom (wheel, or U + drag).
    if (vpHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                      (ImGui::IsKeyDown(ImGuiKey_J) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))) {
        drag = Drag();
        drag.active = true;
        drag.kind = 2;
    }
    if (drag.active && drag.kind == 2) {
        camX -= io.MouseDelta.x / zoom;
        camY += io.MouseDelta.y / zoom;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            drag.active = false;
        return;
    }
    if (vpHovered && io.MouseWheel != 0) {
        float f = std::pow(1.15f, io.MouseWheel);
        float nz = clampf(zoom * f, 0.01f, 64);
        // Keep the point under the cursor fixed.
        camX = mw.x - (mouse.x - vpOrigin.x - vpSize.x / 2) / nz;
        camY = mw.y + (mouse.y - vpOrigin.y - vpSize.y / 2) / nz;
        zoom = nz;
        mw = toWorld(mouse);
    }

    int selSlot = -1;
    Attachment* selAtt = selectedAttachment(&selSlot);
    bool meshEditing = meshMode != MeshMode::Off && selAtt && selAtt->type == AttachmentType::Mesh;

    // ---- mouse down ----
    if (vpHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsKeyDown(ImGuiKey_J)) {
        bool add = io.KeyCtrl || io.KeyShift;
        drag = Drag();
        drag.startScreen = mouse;
        drag.startWorld = drag.lastWorld = mw;

        if (pickParent) {
            int b = pickBone(mouse);
            pickParent = false;
            if (b >= 0) {
                std::vector<int> bones = doc.selectedBones();
                std::vector<int> slots = doc.selectedSlots();
                if (!bones.empty()) doc.reparentBones(bones, b);
                for (int s : slots) doc.reparentSlot(s, b);
                setStatus("Parent set to " + doc.data.bones[doc.data.findBone(doc.data.bones[std::min(b, (int)doc.data.bones.size() - 1)].name)].name);
            }
            return;
        }

        if (doc.tool == Tool::Weights && selAtt && selAtt->type == AttachmentType::Mesh) {
            if (weightBone >= 0 && selAtt->isWeighted()) {
                doc.beginEdit("Paint weights");
                drag.active = true;
                drag.kind = 6;
            } else setStatus(selAtt->isWeighted() ? "Choose a bone in the Weights view" : "Bind the mesh to bones in the Weights view first");
            return;
        }

        if (meshEditing) {
            std::vector<float> wv;
            meshWorld(doc, selSlot, *selAtt, wv);
            int hit = -1;
            for (int k = 0; k < selAtt->vertexCount(); k++)
                if (dist(toScreen({wv[k * 2], wv[k * 2 + 1]}), mouse) < 7) hit = k;
            if (meshMode == MeshMode::Delete) {
                if (hit >= 0 && selAtt->vertexCount() > 3) {
                    if (selAtt->isWeighted()) { setStatus("Unbind weights before deleting vertices"); return; }
                    doc.checkpoint("Delete vertex");
                    selAtt = selectedAttachment(&selSlot);
                    Attachment& a = *selAtt;
                    a.vertices.erase(a.vertices.begin() + hit * 2, a.vertices.begin() + hit * 2 + 2);
                    a.uvs.erase(a.uvs.begin() + hit * 2, a.uvs.begin() + hit * 2 + 2);
                    a.worldVerticesLength -= 2;
                    if (hit < a.hullLength) a.hullLength--;
                    a.triangles = triangulateMesh(a.vertices, a.hullLength);
                    a.edges.clear();
                    for (int h = 0; h < a.hullLength; h++) { a.edges.push_back(h); a.edges.push_back((h + 1) % a.hullLength); }
                    meshSelection.clear();
                }
                return;
            }
            if (meshMode == MeshMode::Create && hit < 0) {
                if (selAtt->isWeighted()) { setStatus("Unbind weights before adding vertices"); return; }
                Vec2 l = doc.sk.bones[doc.data.slots[selSlot].bone].worldToLocal(mw);
                // UV from the containing triangle (barycentric), so the texture stays put.
                const Attachment& a = *selAtt;
                int tri = -1;
                float u = 0, vv = 0;
                for (size_t t = 0; t + 2 < a.triangles.size(); t += 3) {
                    int i0 = a.triangles[t], i1 = a.triangles[t + 1], i2 = a.triangles[t + 2];
                    float x0 = a.vertices[i0 * 2], y0 = a.vertices[i0 * 2 + 1], x1 = a.vertices[i1 * 2], y1 = a.vertices[i1 * 2 + 1],
                          x2 = a.vertices[i2 * 2], y2 = a.vertices[i2 * 2 + 1];
                    float den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
                    if (std::fabs(den) < 1e-9f) continue;
                    float w0 = ((y1 - y2) * (l.x - x2) + (x2 - x1) * (l.y - y2)) / den;
                    float w1 = ((y2 - y0) * (l.x - x2) + (x0 - x2) * (l.y - y2)) / den;
                    float w2 = 1 - w0 - w1;
                    if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
                    tri = (int)t;
                    u = w0 * a.uvs[i0 * 2] + w1 * a.uvs[i1 * 2] + w2 * a.uvs[i2 * 2];
                    vv = w0 * a.uvs[i0 * 2 + 1] + w1 * a.uvs[i1 * 2 + 1] + w2 * a.uvs[i2 * 2 + 1];
                    break;
                }
                if (tri < 0) { setStatus("New vertices must be inside the mesh hull"); return; }
                doc.checkpoint("Add vertex");
                selAtt = selectedAttachment(&selSlot);
                Attachment& m = *selAtt;
                m.vertices.push_back(l.x); m.vertices.push_back(l.y);
                m.uvs.push_back(u); m.uvs.push_back(vv);
                m.worldVerticesLength += 2;
                m.triangles = triangulateMesh(m.vertices, m.hullLength);
                meshSelection = {m.vertexCount() - 1};
                // Deform keys no longer match the vertex count.
                for (auto& an : doc.data.animations)
                    an.timelines.erase(std::remove_if(an.timelines.begin(), an.timelines.end(), [&](const Timeline& t) {
                        return t.type == TimelineType::Deform && t.target == selSlot && t.attachment == m.name; }), an.timelines.end());
                return;
            }
            if (hit >= 0) {
                if (add) {
                    auto it = std::find(meshSelection.begin(), meshSelection.end(), hit);
                    if (it == meshSelection.end()) meshSelection.push_back(hit); else meshSelection.erase(it);
                } else if (std::find(meshSelection.begin(), meshSelection.end(), hit) == meshSelection.end()) {
                    meshSelection = {hit};
                }
                doc.beginEdit(doc.mode == Mode::Setup ? "Move vertices" : "Deform");
                drag.active = true;
                drag.kind = 5;
                drag.startAtt = *selAtt;
                drag.attachment = doc.primary();
                return;
            }
            if (!add) meshSelection.clear();
            drag.active = true;
            drag.kind = 3; // box select vertices
            return;
        }

        if (doc.tool == Tool::Create && doc.mode == Mode::Setup) {
            int hitBone = pickBone(mouse);
            if (hitBone >= 0 && !add) { doc.select({SelType::Bone, hitBone, ""}); }
            drag.active = true;
            drag.kind = 4;
            std::vector<int> sb = doc.selectedBones();
            drag.createParent = sb.empty() ? 0 : sb.back();
            return;
        }

        int bone = pickBone(mouse);
        int slot = bone < 0 ? pickSlot(mouse) : -1;
        if (bone >= 0) {
            Sel s{SelType::Bone, bone, ""};
            if (add) doc.select(s, true);
            else if (!doc.isSelected(s)) doc.select(s);
        } else if (slot >= 0) {
            const Attachment* a = doc.sk.attachmentFor(doc.sk.slots[slot]);
            // In setup mode clicking an image selects the attachment (so it can be moved); in animate mode its bone.
            Sel s = doc.mode == Mode::Setup && a ? Sel{SelType::Attachment, slot, a->name} : Sel{SelType::Bone, doc.data.slots[slot].bone, ""};
            if (add) doc.select(s, true);
            else if (!doc.isSelected(s)) doc.select(s);
        } else {
            if (!add) doc.clearSelection();
            drag.active = true;
            drag.kind = 3;
            return;
        }
        // Start a tool drag on the selection.
        drag.active = true;
        drag.kind = 1;
        std::vector<int> bones = doc.selectedBones();
        // Translating a parent and its child together would double-move the child.
        for (int b : bones) {
            bool ancestorSelected = false;
            for (int q : bones) if (q != b && doc.data.isDescendant(b, q)) ancestorSelected = true;
            if (doc.tool == Tool::Translate && ancestorSelected) continue;
            drag.bones.push_back(b);
            drag.startPose.push_back(doc.bonePose(b));
            const Bone& bb = doc.sk.bones[b];
            drag.startWorldPos.push_back({bb.worldX, bb.worldY});
            drag.startAngles.push_back(bb.worldRotationX());
        }
        if (drag.bones.empty()) {
            Attachment* a = selectedAttachment(&selSlot);
            if (a && doc.mode == Mode::Setup && (a->type == AttachmentType::Region || a->type == AttachmentType::Point)) {
                drag.attachment = doc.primary();
                drag.startAtt = *a;
            } else if (a && doc.mode == Mode::Setup && a->isVertex() && !a->isWeighted()) {
                drag.attachment = doc.primary();
                drag.startAtt = *a;
            } else drag.active = false;
        }
        if (drag.active) {
            static const char* names[] = {"Pose", "Weights", "Create", "Rotate", "Translate", "Scale", "Shear"};
            doc.beginEdit(names[(int)doc.tool]);
        }
        return;
    }

    if (!drag.active) {
        if (vpHovered) {
            int b = pickBone(mouse);
            if (b >= 0) ImGui::SetTooltip("%s", doc.data.bones[b].name.c_str());
        }
        return;
    }

    // ---- dragging ----
    bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (dist(mouse, drag.startScreen) > 3) drag.moved = true;

    if (drag.kind == 6) { // weights brush
        int si;
        Attachment* a = selectedAttachment(&si);
        if (a && a->isWeighted() && weightBone >= 0) {
            std::vector<float> wv;
            meshWorld(doc, si, *a, wv);
            float dt = io.DeltaTime;
            for (int k = 0; k < a->vertexCount(); k++) {
                float dd = dist(toScreen({wv[k * 2], wv[k * 2 + 1]}), mouse);
                if (dd > brushSize) continue;
                float fall = 1 - dd / brushSize;
                // current weight of weightBone on vertex k
                float cur = 0;
                size_t bi = 0, vi = 0;
                for (int q = 0; q < k; q++) { int n = a->bones[bi++]; bi += n; vi += n * 3; }
                int n = a->bones[bi++];
                for (int q = 0; q < n; q++) if (a->bones[bi + q] == weightBone) cur = a->vertices[vi + q * 3 + 2];
                float amount = brushStrength * fall * dt * 10;
                float nw = cur;
                switch (weightMode) {
                case WeightMode::Add: nw = cur + amount; break;
                case WeightMode::Remove: nw = cur - amount; break;
                case WeightMode::Replace: nw = brushStrength; break;
                case WeightMode::Smooth: nw = cur + (0.5f - cur) * amount; break;
                }
                setVertexWeight(*a, doc.sk, k, weightBone, clampf(nw, 0, 1));
            }
            doc.dirty = true;
        }
        if (!down) { doc.commitEdit(); drag.active = false; }
        return;
    }

    if (drag.kind == 5) { // move mesh vertices
        int si;
        Attachment* a = selectedAttachment(&si);
        if (a && drag.moved) {
            Vec2 delta = mw - drag.startWorld;
            const Attachment& s = drag.startAtt;
            if (doc.mode == Mode::Setup) {
                *a = s;
                if (!a->isWeighted()) {
                    const Bone& b = doc.sk.bones[doc.data.slots[si].bone];
                    for (int k : meshSelection) {
                        Vec2 w = b.localToWorld({s.vertices[k * 2], s.vertices[k * 2 + 1]});
                        Vec2 l = b.worldToLocal(w + delta);
                        a->vertices[k * 2] = l.x; a->vertices[k * 2 + 1] = l.y;
                    }
                } else {
                    size_t bi = 0, vi = 0;
                    for (int k = 0; k < a->vertexCount(); k++) {
                        int n = a->bones[bi++];
                        bool sel = std::find(meshSelection.begin(), meshSelection.end(), k) != meshSelection.end();
                        for (int q = 0; q < n; q++, bi++, vi += 3) {
                            if (!sel) continue;
                            const Bone& b = doc.sk.bones[a->bones[bi]];
                            Vec2 o = b.worldToLocal({b.worldX, b.worldY}), d2 = b.worldToLocal(Vec2(b.worldX, b.worldY) + delta);
                            a->vertices[vi] = s.vertices[vi] + d2.x - o.x;
                            a->vertices[vi + 1] = s.vertices[vi + 1] + d2.y - o.y;
                        }
                    }
                }
            } else if (Animation* an = doc.currentAnimation()) {
                // Animate mode: key a deform offset for the attachment at the current frame.
                const Slot& slot = doc.sk.slots[si];
                std::string skinName = doc.data.skins[doc.data.skins[doc.skin].get(si, a->name) ? doc.skin : 0].name;
                int len = a->deformLength();
                Timeline& tl = an->get(TimelineType::Deform, si, skinName, a->name);
                float t = std::round(doc.time * doc.data.fps) / doc.data.fps;
                if (!drag.deformCaptured) { // the deform shown when the drag started
                    drag.deformBase = slot.deform.size() == (size_t)len && slot.deformFor == a->name ? slot.deform : std::vector<float>(len, 0.0f);
                    drag.deformCaptured = true;
                }
                Key& k = tl.setKey(t);
                k.deform = drag.deformBase;
                if (!a->isWeighted()) {
                    const Bone& b = doc.sk.bones[slot.bone];
                    Vec2 o = b.worldToLocal({b.worldX, b.worldY}), d2 = b.worldToLocal(Vec2(b.worldX, b.worldY) + delta);
                    for (int v : meshSelection) { k.deform[v * 2] += d2.x - o.x; k.deform[v * 2 + 1] += d2.y - o.y; }
                } else {
                    size_t bi = 0, fi = 0;
                    for (int v = 0; v < a->vertexCount(); v++) {
                        int n = a->bones[bi++];
                        bool sel = std::find(meshSelection.begin(), meshSelection.end(), v) != meshSelection.end();
                        for (int q = 0; q < n; q++, bi++, fi += 2) {
                            if (!sel) continue;
                            const Bone& b = doc.sk.bones[a->bones[bi]];
                            Vec2 o = b.worldToLocal({b.worldX, b.worldY}), d2 = b.worldToLocal(Vec2(b.worldX, b.worldY) + delta);
                            k.deform[fi] += d2.x - o.x;
                            k.deform[fi + 1] += d2.y - o.y;
                        }
                    }
                }
            }
            doc.dirty = true;
        }
        if (!down) { drag.moved ? doc.commitEdit() : doc.cancelEdit(); drag.active = false; }
        return;
    }

    if (drag.kind == 3) { // box select
        if (!down) {
            ImVec2 a = drag.startScreen;
            float x0 = std::min(a.x, mouse.x), x1 = std::max(a.x, mouse.x), y0 = std::min(a.y, mouse.y), y1 = std::max(a.y, mouse.y);
            auto inBox = [&](ImVec2 p) { return p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1; };
            if (meshEditing) {
                std::vector<float> wv;
                meshWorld(doc, selSlot, *selAtt, wv);
                for (int k = 0; k < selAtt->vertexCount(); k++)
                    if (inBox(toScreen({wv[k * 2], wv[k * 2 + 1]})) && std::find(meshSelection.begin(), meshSelection.end(), k) == meshSelection.end())
                        meshSelection.push_back(k);
            } else if (drag.moved && showBones && selectBones) {
                for (size_t i = 0; i < doc.sk.bones.size(); i++)
                    if (boneVisible((int)i) && inBox(toScreen({doc.sk.bones[i].worldX, doc.sk.bones[i].worldY})))
                        doc.select({SelType::Bone, (int)i, ""}, true);
            }
            drag.active = false;
        }
        return;
    }

    if (drag.kind == 4) { // create bone
        if (!down) {
            if (drag.moved) {
                int parent = drag.createParent;
                doc.updatePose();
                doc.createBone(parent, drag.startWorld, mw);
                // Selected images become children of the new bone, as in Spine.
            } else {
                int b = pickBone(mouse);
                if (b < 0) doc.clearSelection();
            }
            drag.active = false;
        }
        return;
    }

    // Tool drag (kind 1).
    if (drag.moved) {
        Vec2 delta = mw - drag.startWorld;
        if (!drag.bones.empty()) {
            for (size_t i = 0; i < drag.bones.size(); i++) {
                int bi = drag.bones[i];
                Document::LocalPose p = drag.startPose[i];
                const Bone& b = doc.sk.bones[bi];
                Vec2 origin = drag.startWorldPos[i];
                const WorldMatrix& pm = doc.sk.parentMatrix(bi);
                bool reflect = pm.a * pm.d - pm.b * pm.c < 0;
                switch (doc.tool) {
                case Tool::Rotate: {
                    float a0 = atan2Deg(drag.startWorld.y - origin.y, drag.startWorld.x - origin.x);
                    float a1 = atan2Deg(mw.y - origin.y, mw.x - origin.x);
                    float d = wrapDeg(a1 - a0);
                    p.rotation = drag.startPose[i].rotation + (reflect ? -d : d);
                    doc.setBonePose(bi, p, (int)Channel::Rotate);
                    break;
                }
                case Tool::Translate: {
                    Vec2 target = origin + delta;
                    if (io.KeyShift) { // constrain to the dominant axis
                        if (std::fabs(delta.x) > std::fabs(delta.y)) target.y = origin.y; else target.x = origin.x;
                    }
                    Vec2 l = b.worldToParent(doc.sk, target);
                    p.x = l.x; p.y = l.y;
                    doc.setBonePose(bi, p, (int)Channel::Translate);
                    break;
                }
                case Tool::Scale: {
                    float ang = drag.startAngles[i] * degRad;
                    Vec2 u(std::cos(ang), std::sin(ang)), vv(-std::sin(ang), std::cos(ang));
                    Vec2 d0 = drag.startWorld - origin, d1 = mw - origin;
                    float minD = 4 / zoom;
                    if (std::fabs(d0.dot(u)) > minD) p.sx = drag.startPose[i].sx * d1.dot(u) / d0.dot(u);
                    if (std::fabs(d0.dot(vv)) > minD) p.sy = drag.startPose[i].sy * d1.dot(vv) / d0.dot(vv);
                    if (io.KeyShift) { // uniform
                        float r = d1.len() / std::max(minD, d0.len());
                        p.sx = drag.startPose[i].sx * r;
                        p.sy = drag.startPose[i].sy * r;
                    }
                    doc.setBonePose(bi, p, (int)Channel::Scale);
                    break;
                }
                case Tool::Shear: {
                    float a0 = atan2Deg(drag.startWorld.y - origin.y, drag.startWorld.x - origin.x);
                    float a1 = atan2Deg(mw.y - origin.y, mw.x - origin.x);
                    float d = wrapDeg(a1 - a0);
                    if (io.KeyShift) p.shy = drag.startPose[i].shy + d;
                    else p.shx = drag.startPose[i].shx + d;
                    doc.setBonePose(bi, p, (int)Channel::Shear);
                    break;
                }
                case Tool::Pose: {
                    // Rotates the parent so the bone follows the mouse; root/parentless bones translate.
                    int parent = doc.data.bones[bi].parent;
                    if (parent <= 0) {
                        Vec2 l = b.worldToParent(doc.sk, origin + delta);
                        p.x = l.x; p.y = l.y;
                        doc.setBonePose(bi, p, (int)Channel::Translate);
                    } else {
                        const Bone& pb = doc.sk.bones[parent];
                        Vec2 po(pb.worldX, pb.worldY);
                        float a0 = atan2Deg(origin.y - po.y, origin.x - po.x);
                        float a1 = atan2Deg(mw.y - po.y, mw.x - po.x);
                        if (i == 0) {
                            if (!drag.parentCaptured) { drag.parentStart = doc.bonePose(parent); drag.parentCaptured = true; }
                            Document::LocalPose pp = drag.parentStart;
                            const WorldMatrix& gm = doc.sk.parentMatrix(parent);
                            float d = wrapDeg(a1 - a0);
                            pp.rotation = drag.parentStart.rotation + ((gm.a * gm.d - gm.b * gm.c) < 0 ? -d : d);
                            doc.setBonePose(parent, pp, (int)Channel::Rotate);
                        }
                    }
                    break;
                }
                default: break;
                }
            }
        } else if (drag.attachment.type == SelType::Attachment) {
            int si = drag.attachment.index;
            Attachment* a = selectedAttachment();
            const Bone& b = doc.sk.bones[doc.data.slots[si].bone];
            if (a) {
                const Attachment& s = drag.startAtt;
                if (a->type == AttachmentType::Region || a->type == AttachmentType::Point) {
                    Vec2 origin = b.localToWorld({s.x, s.y});
                    switch (doc.tool) {
                    case Tool::Translate: case Tool::Pose: {
                        Vec2 l = b.worldToLocal(origin + delta);
                        a->x = l.x; a->y = l.y;
                        break;
                    }
                    case Tool::Rotate: {
                        float a0 = atan2Deg(drag.startWorld.y - origin.y, drag.startWorld.x - origin.x);
                        float a1 = atan2Deg(mw.y - origin.y, mw.x - origin.x);
                        a->rotation = s.rotation + wrapDeg(a1 - a0);
                        break;
                    }
                    case Tool::Scale: {
                        float d0 = (drag.startWorld - origin).len(), d1 = (mw - origin).len();
                        if (d0 > 2 / zoom) { a->scaleX = s.scaleX * d1 / d0; a->scaleY = s.scaleY * d1 / d0; }
                        break;
                    }
                    default: break;
                    }
                } else if (a->isVertex()) { // translate whole unweighted vertex attachment
                    for (size_t k = 0; k + 1 < s.vertices.size(); k += 2) {
                        Vec2 l = b.worldToLocal(b.localToWorld({s.vertices[k], s.vertices[k + 1]}) + delta);
                        a->vertices[k] = l.x; a->vertices[k + 1] = l.y;
                    }
                }
                doc.dirty = true;
            }
        }
        doc.updatePose();
    }
    if (!down) {
        if (drag.moved) doc.commitEdit(); else doc.cancelEdit();
        drag.active = false;
    }
}

void Editor::viewportToolbar() {
    ImVec2 size(840 * uiScale, 92 * uiScale);
    ImVec2 pos(vpOrigin.x + (vpSize.x - size.x) / 2, vpOrigin.y + vpSize.y - size.y - 8);
    if (pos.x < vpOrigin.x + 4) pos.x = vpOrigin.x + 4;
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.14f, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4);
    ImGui::BeginChild("##toolbar", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    auto toolButton = [&](const char* label, Tool t, float w, bool enabled = true) {
        bool on = doc.tool == t;
        ImGui::BeginDisabled(!enabled);
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.50f, 0.78f, 1));
        if (ImGui::Button(label, ImVec2(w, 0))) doc.tool = t;
        if (on) ImGui::PopStyleColor();
        ImGui::EndDisabled();
    };
    float bw = 74 * uiScale;
    ImGui::BeginGroup();
    toolButton("Pose", Tool::Pose, bw);
    toolButton("Weights", Tool::Weights, bw);
    toolButton("Create", Tool::Create, bw, doc.mode == Mode::Setup);
    ImGui::EndGroup();
    ImGui::SameLine();

    // Transform tools with numeric fields for the primary selected bone (or attachment in setup).
    Sel p = doc.primary();
    int bone = p.type == SelType::Bone ? p.index : -1;
    Attachment* att = bone < 0 ? selectedAttachment() : nullptr;
    Document::LocalPose pose{};
    if (bone >= 0) pose = doc.bonePose(bone);
    ImGui::BeginGroup();
    float fw = 70 * uiScale;
    auto keyButton = [&](const char* id, TimelineType t, int ch) {
        if (doc.mode != Mode::Animate || bone < 0) return;
        ImGui::SameLine();
        bool keyed = doc.hasKey(bone, t);
        bool pend = doc.pending.count(bone) && (doc.pending[bone].channels & ch);
        ImVec4 col = pend ? ImVec4(0.95f, 0.55f, 0.15f, 1) : keyed ? ImVec4(0.85f, 0.25f, 0.25f, 1) : ImVec4(0.35f, 0.35f, 0.35f, 1);
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        if (ImGui::Button(id, ImVec2(18 * uiScale, 0))) {
            doc.checkpoint("Key");
            doc.keyBone(bone, ch);
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(keyed ? "Keyed at this frame (click to re-key)" : "Set key");
    };
    auto applyPose = [&](int ch) { if (bone >= 0) { doc.setBonePose(bone, pose, ch); doc.updatePose(); } };
    toolButton("Rotate", Tool::Rotate, bw);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(fw * 2 + 4);
    if (bone >= 0) { if (dragFloatEdit(doc, "##rot", &pose.rotation, 0.5f, "%.2f", "Rotate")) applyPose((int)Channel::Rotate); }
    else if (att) dragFloatEdit(doc, "##rot", &att->rotation, 0.5f, "%.2f", "Rotate");
    else { float z = 0; ImGui::BeginDisabled(); ImGui::DragFloat("##rot", &z); ImGui::EndDisabled(); }
    keyButton("##kr", TimelineType::Rotate, (int)Channel::Rotate);
    toolButton("Translate", Tool::Translate, bw);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(fw * 2 + 4);
    if (bone >= 0) {
        float v[2] = {pose.x, pose.y};
        if (doc.axes == Axes::World) { const Bone& b = doc.sk.bones[bone]; v[0] = b.worldX; v[1] = b.worldY; }
        if (dragFloat2Edit(doc, "##tr", v, 0.5f, "%.2f", "Translate")) {
            if (doc.axes == Axes::World) { Vec2 l = doc.sk.bones[bone].worldToParent(doc.sk, {v[0], v[1]}); pose.x = l.x; pose.y = l.y; }
            else { pose.x = v[0]; pose.y = v[1]; }
            applyPose((int)Channel::Translate);
        }
    } else if (att) { float v[2] = {att->x, att->y}; if (dragFloat2Edit(doc, "##tr", v, 0.5f, "%.2f", "Translate")) { att->x = v[0]; att->y = v[1]; } }
    else { float z[2] = {}; ImGui::BeginDisabled(); ImGui::DragFloat2("##tr", z); ImGui::EndDisabled(); }
    keyButton("##kt", TimelineType::Translate, (int)Channel::Translate);
    toolButton("Scale", Tool::Scale, bw);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(fw * 2 + 4);
    if (bone >= 0) { float v[2] = {pose.sx, pose.sy}; if (dragFloat2Edit(doc, "##sc", v, 0.01f, "%.3f", "Scale")) { pose.sx = v[0]; pose.sy = v[1]; applyPose((int)Channel::Scale); } }
    else if (att) { float v[2] = {att->scaleX, att->scaleY}; if (dragFloat2Edit(doc, "##sc", v, 0.01f, "%.3f", "Scale")) { att->scaleX = v[0]; att->scaleY = v[1]; } }
    else { float z[2] = {}; ImGui::BeginDisabled(); ImGui::DragFloat2("##sc", z); ImGui::EndDisabled(); }
    keyButton("##ks", TimelineType::Scale, (int)Channel::Scale);
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    toolButton("Shear", Tool::Shear, bw);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(fw * 2 + 4);
    if (bone >= 0) { float v[2] = {pose.shx, pose.shy}; if (dragFloat2Edit(doc, "##sh", v, 0.5f, "%.2f", "Shear")) { pose.shx = v[0]; pose.shy = v[1]; applyPose((int)Channel::Shear); } }
    else { float z[2] = {}; ImGui::BeginDisabled(); ImGui::DragFloat2("##sh", z); ImGui::EndDisabled(); }
    keyButton("##kh", TimelineType::Shear, (int)Channel::Shear);
    // Axes.
    auto axisButton = [&](const char* l, Axes a) {
        bool on = doc.axes == a;
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.50f, 0.78f, 1));
        if (ImGui::Button(l, ImVec2(60 * uiScale, 0))) doc.axes = a;
        if (on) ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    axisButton("Local", Axes::Local);
    axisButton("Parent", Axes::Parent);
    axisButton("World", Axes::World);
    ImGui::NewLine();
    if (doc.mode == Mode::Animate) {
        if (doc.autoKey) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.22f, 0.22f, 1));
        if (ImGui::Button("Auto Key", ImVec2(90 * uiScale, 0))) doc.autoKey = !doc.autoKey;
        if (doc.autoKey) ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Key (K)", ImVec2(70 * uiScale, 0))) {
            doc.checkpoint("Key");
            auto pend = doc.pending;
            for (auto& [b, pd] : pend) doc.keyBone(b, pd.channels);
        }
        ImGui::SameLine();
    }
    ImGui::Checkbox("Bones", &showBones); ImGui::SameLine();
    ImGui::Checkbox("Images", &showImages); ImGui::SameLine();
    ImGui::Checkbox("Others", &showOthers); ImGui::SameLine();
    ImGui::Checkbox("Names", &showBoneNames);
    ImGui::EndGroup();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    // Zoom readout, bottom-left.
    ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + 10, vpOrigin.y + vpSize.y - 26));
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1), "%.0f%%", zoom * 100);
}

} // namespace osteon
