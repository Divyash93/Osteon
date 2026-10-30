// The main editor viewport: rendering, overlays (bones, gizmos, meshes), picking and tools.
#include "App.h"
#include "Geometry.h"
#include "Icons.h"
#include "imgui_internal.h"

namespace osteon {

static ImU32 toU32(const Color& c, float a = 1) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a * a)); }
static const ImU32 kSelect = IM_COL32(90, 210, 255, 255);
static const ImU32 kSelectFill = IM_COL32(90, 210, 255, 90);

static float dist(ImVec2 a, ImVec2 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }

static void dottedPolygon(ImDrawList* dl, const std::vector<ImVec2>& p, ImU32 col, float dash, float gap, float th);

int Editor::pickBone(ImVec2 s) {
    if (!showBones || !selectBones) return -1;
    int best = -1;
    float bestD = 8;
    for (int i = (int)doc.sk.bones.size() - 1; i >= 0; i--) {
        const Bone& b = doc.sk.bones[i];
        if (!b.active || !boneVisible(i)) continue;
        ImVec2 o = toScreen({b.worldX, b.worldY});
        float len = doc.data.bones[i].length;
        bool iconBone = !isDefaultBoneIcon(doc.data.bones[i].icon);
        float d = dist(o, s) - (iconBone ? 6 * uiScale : 2); // origins win over segments; an icon is a larger target
        if (len > 0 && !iconBone) {
            ImVec2 t = toScreen(b.localToWorld({len, 0}));
            d = std::min(d, distToSegment({s.x, s.y}, {o.x, o.y}, {t.x, t.y}));
        }
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

int Editor::pickSlot(ImVec2 s) {
    if (!showImages || !selectImages || doc.tool == Tool::Pose) return -1; // Pose only picks bones
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
    // Wait until docking has settled the viewport size before fitting.
    static ImVec2 lastSize;
    bool sizeStable = std::fabs(lastSize.x - vpSize.x) < 1 && std::fabs(lastSize.y - vpSize.y) < 1;
    lastSize = vpSize;
    if (fitRequested && sizeStable && ImGui::GetFrameCount() > 3) { zoomToFit(); fitRequested = false; }

    gfx.ensureTarget(viewRT, (int)vpSize.x, (int)vpSize.y);
    RenderOptions opt = renderOptions();
    gfx.begin(viewRT, camX, camY, zoom, opt);
    auto imagePath = [this](const Attachment& a, int seq) { return doc.imagePathFor(a, seq); };
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
    // Mode label, top-left, as in Spine: a figure icon (standing for Setup, running for Animate) and the
    // mode name; a click toggles the mode (Ctrl+Tab too).
    {
        bool setup = doc.mode == Mode::Setup;
        ImVec4 tint = setup ? ImVec4(0.78f, 0.79f, 0.82f, 1) : ImVec4(0.38f, 0.66f, 1, 1);
        ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + 10, vpOrigin.y + 8));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_Text, tint);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
        ImGui::SetWindowFontScale(1.35f);
        float iconSz = ImGui::GetTextLineHeight() * 1.1f;
        std::string label = std::string("      ") + (setup ? "SETUP" : "ANIMATE"); // room for the icon
        if (ImGui::Button(label.c_str())) doc.setMode(setup ? Mode::Animate : Mode::Setup);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(setup ? "Setup mode: click (or Ctrl+Tab) to animate" : "Animate mode: click (or Ctrl+Tab) for setup");
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        drawIcon(dl, setup ? Icon::Skeleton : Icon::Animations, ImVec2(mn.x + 8 + iconSz / 2, (mn.y + mx.y) / 2), iconSz,
                 ImGui::ColorConvertFloat4ToU32(tint));
        ImGui::SetWindowFontScale(1);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }
    if (doc.mode == Mode::Animate && doc.currentAnimation()) {
        ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + 14, vpOrigin.y + 42));
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "%s   frame %d", doc.currentAnimation()->name.c_str(), (int)std::round(doc.time * doc.data.fps));
    }
    if (bindMode) {
        const char* msg = "Bind: click each bone to bind to the mesh (Space/Esc finishes)";
        ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + vpSize.x / 2 - ImGui::CalcTextSize(msg).x / 2, vpOrigin.y + 10));
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "%s", msg);
    }
    if (picking()) {
        const char* msg = pickParent ? "Click the new parent bone (Esc cancels)"
                        : pickConstraint == 1 ? "Click the IK target bone, or empty space for a new target (Esc cancels)" : "Click the transform target bone (Esc cancels)";
        ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + vpSize.x / 2 - ImGui::CalcTextSize(msg).x / 2, vpOrigin.y + 10));
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "%s", msg);
    }
    viewportToolbar();
    drawHoverChip(dl);
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
            // A selected mesh always shows its dots; the Edit Mesh modes only change what a click does.
            bool editing = selSlot == si && (meshMode != MeshMode::Off || slotSel);
            ImU32 wire = editing ? IM_COL32(255, 255, 255, 120) : IM_COL32(90, 210, 255, 90);
            // Inner triangle edges are dashed; the outer hull stays solid.
            auto isHullEdge = [&](int i0, int i1) {
                if (i0 >= g->hullLength || i1 >= g->hullLength) return false;
                int d = std::abs(i0 - i1);
                return d == 1 || d == g->hullLength - 1;
            };
            if (meshDim && editing && meshMode != MeshMode::Off) // Dim: darken the mesh image
                for (size_t t = 0; t + 2 < g->triangles.size(); t += 3)
                    dl->AddTriangleFilled(toScreen({v[g->triangles[t] * 2], v[g->triangles[t] * 2 + 1]}), toScreen({v[g->triangles[t + 1] * 2], v[g->triangles[t + 1] * 2 + 1]}),
                                          toScreen({v[g->triangles[t + 2] * 2], v[g->triangles[t + 2] * 2 + 1]}), IM_COL32(0, 0, 0, 130));
            for (size_t t = 0; meshTriangles && t + 2 < g->triangles.size(); t += 3)
                for (int e = 0; e < 3; e++) {
                    int i0 = g->triangles[t + e], i1 = g->triangles[t + (e + 1) % 3];
                    ImVec2 p0 = toScreen({v[i0 * 2], v[i0 * 2 + 1]}), p1 = toScreen({v[i1 * 2], v[i1 * 2 + 1]});
                    if (isHullEdge(i0, i1)) dl->AddLine(p0, p1, wire);
                    else dottedPolygon(dl, {p0, p1}, wire, 6 * uiScale, 4 * uiScale, 1.2f);
                }
            for (int h = 0; h < g->hullLength; h++) {
                int j = (h + 1) % g->hullLength;
                dl->AddLine(toScreen({v[h * 2], v[h * 2 + 1]}), toScreen({v[j * 2], v[j * 2 + 1]}), meshMode != MeshMode::Off ? IM_COL32(70, 210, 235, 255) : kSelect, 1.5f); // hull edges: cyan while editing
            }
            const bool weightsShown = doc.tool == Tool::Weights && g->isWeighted();
            if (weightsShown) {
                // ---- Weights view: overlay and pies, in each bone's colour (as in the Bones list). ----
                std::vector<int> bound = boundBones(*g);
                auto colorOf = [&](int bone) {
                    size_t idx = std::find(bound.begin(), bound.end(), bone) - bound.begin();
                    return weightPaletteColor(idx);
                };
                const bool hasBoneSel = !weightBones.empty();
                // Each vertex's influences: (bone, weight).
                std::vector<std::vector<std::pair<int, float>>> inf(g->vertexCount());
                {
                    size_t bi = 0, vi = 0;
                    for (int k = 0; k < g->vertexCount() && bi < g->bones.size(); k++) {
                        int n = g->bones[bi++];
                        for (int q = 0; q < n; q++, vi += 3) inf[k].push_back({g->bones[bi++], g->vertices[vi + 2]});
                    }
                }
                auto isSel = [&](int k) { return std::find(meshSelection.begin(), meshSelection.end(), k) != meshSelection.end(); };
                if (showOverlay) {
                    // Fill the mesh with the blend of its bones' colours; with Selected, unselected bones go black.
                    auto blend = [&](int k) {
                        float r = 0, gg = 0, b = 0;
                        for (auto& [bone, w] : inf[k]) {
                            ImU32 c = (showSelectedOnly && hasBoneSel && !weightBones.count(bone)) ? IM_COL32(0, 0, 0, 255) : colorOf(bone);
                            r += w * (float)(c & 0xFF); gg += w * (float)((c >> 8) & 0xFF); b += w * (float)((c >> 16) & 0xFF);
                        }
                        return IM_COL32((int)r, (int)gg, (int)b, 150);
                    };
                    const ImVec2 white = dl->_Data->TexUvWhitePixel;
                    for (size_t t = 0; t + 2 < g->triangles.size(); t += 3) {
                        int ids[3] = {g->triangles[t], g->triangles[t + 1], g->triangles[t + 2]};
                        if (ids[0] >= g->vertexCount() || ids[1] >= g->vertexCount() || ids[2] >= g->vertexCount()) continue;
                        dl->PrimReserve(3, 3);
                        unsigned base = dl->_VtxCurrentIdx;
                        for (int c = 0; c < 3; c++) dl->PrimWriteVtx(toScreen({v[ids[c] * 2], v[ids[c] * 2 + 1]}), white, blend(ids[c]));
                        dl->PrimWriteIdx((ImDrawIdx)base); dl->PrimWriteIdx((ImDrawIdx)(base + 1)); dl->PrimWriteIdx((ImDrawIdx)(base + 2));
                    }
                }
                for (int k = 0; k < g->vertexCount(); k++) {
                    ImVec2 p = toScreen({v[k * 2], v[k * 2 + 1]});
                    bool vs = isSel(k);
                    // Selected: only pies for the selected vertices and the vertices weighted to the selected bones.
                    bool pieShown = showPies;
                    if (pieShown && showSelectedOnly) {
                        bool onSelBone = false;
                        for (auto& [bone, w] : inf[k]) onSelBone |= weightBones.count(bone) && w > 0;
                        pieShown = vs || (hasBoneSel && onSelBone);
                    }
                    if (pieShown) {
                        float r = (vs ? 9.0f : 6.0f) * uiScale, a0 = -1.5707963f;
                        for (auto& [bone, w] : inf[k]) {
                            float span = 6.2831853f * w;
                            if (span < 1e-3f) continue;
                            // A slice wider than half a circle is drawn in two halves (the fill needs convex shapes).
                            for (float s = 0; s < span - 1e-4f; s += 3.1f) {
                                float s1 = std::min(span, s + 3.1f);
                                dl->PathClear();
                                dl->PathLineTo(p);
                                dl->PathArcTo(p, r, a0 + s, a0 + s1, 12);
                                dl->PathFillConvex(colorOf(bone));
                            }
                            a0 += span;
                        }
                        dl->AddCircle(p, r, IM_COL32(0, 0, 0, 200), 24);
                    } else dl->AddCircleFilled(p, 2.5f * uiScale, IM_COL32(255, 255, 255, 200));
                    if (vs) dl->AddCircle(p, 11 * uiScale, IM_COL32(255, 200, 40, 255), 24, 2.0f);
                }
            } else if (editing) {
                for (int k = 0; k < g->vertexCount(); k++) {
                    ImVec2 p = toScreen({v[k * 2], v[k * 2 + 1]});
                    bool vs = std::find(meshSelection.begin(), meshSelection.end(), k) != meshSelection.end();
                    ImU32 col = vs ? IM_COL32(255, 200, 40, 255) : IM_COL32(255, 255, 255, 230);
                    dl->AddCircleFilled(p, vs ? 4.5f : 3.5f, col);
                    dl->AddCircle(p, vs ? 4.5f : 3.5f, IM_COL32(0, 0, 0, 200));
                }
                if (meshMode == MeshMode::New && selSlot == si) { // the hull being placed
                    const Bone& sb = doc.sk.bones[d.slots[si].bone];
                    for (size_t k = 0; k < newHull.size(); k++) {
                        ImVec2 p = toScreen(sb.localToWorld(newHull[k]));
                        if (k > 0) dl->AddLine(toScreen(sb.localToWorld(newHull[k - 1])), p, IM_COL32(70, 210, 235, 255), 1.5f);
                        else if (newHull.size() >= 3) dl->AddCircle(p, 8 * uiScale, IM_COL32(255, 200, 40, 255), 16, 1.5f); // click it to close
                        dl->AddCircleFilled(p, 4.5f, k == 0 ? IM_COL32(255, 200, 40, 255) : IM_COL32(255, 255, 255, 240));
                    }
                    if (!newHull.empty() && vpHovered) dl->AddLine(toScreen(sb.localToWorld(newHull.back())), ImGui::GetIO().MousePos, IM_COL32(70, 210, 235, 120), 1.0f);
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
        if (showOtherNames && showOthers && !v.empty() &&
            (a->type == AttachmentType::BoundingBox || a->type == AttachmentType::Clipping || a->type == AttachmentType::Path || a->type == AttachmentType::Point))
            dl->AddText(toScreen({v[0], v[1]}), IM_COL32(255, 255, 255, 210), a->name.c_str());
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

    // With the Weights tool, the bones bound to the selected mesh are drawn in their colours from the Bones list.
    std::map<int, ImU32> weightBoneColors;
    if (doc.tool == Tool::Weights) {
        int wsi = -1;
        Attachment* wa = selectedAttachment(&wsi);
        if (wa && wa->type == AttachmentType::Mesh) {
            std::vector<int> bound = boundBones(*wa);
            for (size_t k = 0; k < bound.size(); k++) weightBoneColors[bound[k]] = weightPaletteColor(k);
            for (int b : bindPending) if (!weightBoneColors.count(b)) weightBoneColors[b] = IM_COL32(255, 200, 60, 255); // Bind mode: clicked so far
        }
    }

    // Bones.
    if (showBones) {
        for (size_t i = 0; i < sk.bones.size(); i++) {
            const Bone& b = sk.bones[i];
            if (!b.active || !boneVisible((int)i)) continue;
            const BoneData& bd = d.bones[i];
            bool sel = doc.isSelected({SelType::Bone, (int)i, ""});
            bool pend = doc.pending.count((int)i) > 0;
            // While picking a parent, the hovered bone is the candidate: amber, so it reads as a target.
            bool target = picking() && (int)i == hoverBone;
            bool hov = (int)i == hoverBone && (!sel || target);
            ImU32 col = target ? IM_COL32(255, 190, 60, 255) : sel ? kSelect : hov ? IM_COL32(255, 255, 255, 255) : pend ? IM_COL32(255, 150, 40, 255) : toU32(bd.color);
            ImU32 fill = target ? IM_COL32(255, 190, 60, 110) : sel ? kSelectFill : hov ? IM_COL32(255, 255, 255, 80) : pend ? IM_COL32(255, 150, 40, 70) : toU32(bd.color, 0.35f);
            if (auto wc = weightBoneColors.find((int)i); wc != weightBoneColors.end() && !target) {
                // Bound bones show their weight colour; the ones selected in the Weights view are drawn solid.
                ImU32 c = wc->second;
                col = c;
                fill = (c & 0x00FFFFFF) | (weightBones.count((int)i) ? 0xB4000000u : 0x55000000u);
            }
            ImVec2 p0 = toScreen({b.worldX, b.worldY});
            const bool iconBone = !isDefaultBoneIcon(bd.icon);
            const float thick = clampf(bd.thickness, 20, 300) / 100; // the bone's "Thickness" slider
            if (iconBone) {
                // A bone with an icon is shown as that icon at its origin, in the bone's selection/hover colour.
                float isz = 26 * uiScale * thick;
                if (sel || hov) dl->AddCircleFilled(p0, isz * 0.62f, fill);
                drawBoneIcon(dl, bd.icon, p0, isz, col);
            } else if (bd.length > 0) {
                ImVec2 p1 = toScreen(b.localToWorld({bd.length, 0}));
                float L = dist(p0, p1);
                if (L > 2 && (bd.line || lineBonesAll)) {
                    // Line bone: a plain line from the joint to the tip, in the usual selection/hover colour.
                    if (hov || target) dl->AddLine(p0, p1, fill, 7.0f * uiScale * thick);
                    dl->AddLine(p0, p1, col, (sel || hov ? 3.0f : 2.0f) * uiScale * thick);
                    dl->AddCircleFilled(p1, (sel || hov ? 3.2f : 2.4f) * uiScale * thick, col);
                } else if (L > 2) {
                    ImVec2 dir((p1.x - p0.x) / L, (p1.y - p0.y) / L), n(-dir.y, dir.x);
                    float w = clampf(L * 0.12f, 2.5f, 9) * thick;
                    ImVec2 m(p0.x + dir.x * L * 0.18f, p0.y + dir.y * L * 0.18f);
                    ImVec2 pts[4] = {p0, ImVec2(m.x + n.x * w, m.y + n.y * w), p1, ImVec2(m.x - n.x * w, m.y - n.y * w)};
                    // Pose tool: every bone shows its edges so it is clear what can be grabbed.
                    if (doc.tool == Tool::Pose && !sel && !hov) dl->AddPolyline(pts, 4, IM_COL32(255, 255, 255, 70), ImDrawFlags_Closed, 1.0f);
                    if (hov) dl->AddPolyline(pts, 4, IM_COL32(255, 255, 255, 90), ImDrawFlags_Closed, 5.0f); // glow
                    dl->AddConvexPolyFilled(pts, 4, fill);
                    dl->AddPolyline(pts, 4, col, ImDrawFlags_Closed, sel ? 2.0f : hov ? 2.4f : 1.2f);
                }
            } else {
                dl->AddCircle(p0, 7, col, 0, 1.5f);
                dl->AddLine(ImVec2(p0.x - 9, p0.y), ImVec2(p0.x + 9, p0.y), col);
                dl->AddLine(ImVec2(p0.x, p0.y - 9), ImVec2(p0.x, p0.y + 9), col);
            }
            if (!iconBone) dl->AddCircleFilled(p0, 3.2f, col);
            if (showBoneNames || sel) dl->AddText(ImVec2(p0.x + (iconBone ? 16 : 6) * uiScale, p0.y - 18), IM_COL32(230, 230, 230, 220), bd.name.c_str());
        }
    }
    drawGizmo(dl);
    drawHover(dl);

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
    if (doc.tool == Tool::Weights && vpHovered && weightBrush && !bindMode) {
        // The brush: the outer ring is its size, the inner ring where the feathering begins.
        dl->AddCircle(ImGui::GetMousePos(), brushSize, IM_COL32(255, 255, 255, 160), 48);
        float inner = brushSize * (1 - clampf(brushFeather, 0, 100) / 100);
        if (inner > 2) dl->AddCircle(ImGui::GetMousePos(), inner, IM_COL32(255, 255, 255, 70), 48);
    }
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
        drag.startScreen = mouse;
        drag.handle = ImGui::IsMouseClicked(ImGuiMouseButton_Right) ? 1 : 0; // a right press that does not move cycles the Edit Mesh tools
    }
    if (drag.active && drag.kind == 2) {
        camX -= io.MouseDelta.x / zoom;
        camY += io.MouseDelta.y / zoom;
        if (std::fabs(mouse.x - drag.startScreen.x) + std::fabs(mouse.y - drag.startScreen.y) > 4) drag.moved = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            drag.active = false;
            if (drag.handle == 1 && !drag.moved && meshMode != MeshMode::Off && meshMode != MeshMode::New) {
                int sl = -1;
                Attachment* sa = selectedAttachment(&sl);
                bool mesh = sa && sa->type == AttachmentType::Mesh && doc.mode == Mode::Setup;
                meshMode = meshMode == MeshMode::Modify ? (mesh ? MeshMode::Create : MeshMode::Modify)
                         : meshMode == MeshMode::Create ? MeshMode::Delete : MeshMode::Modify;
                meshSelection.clear();
            }
        }
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

        if (pickConstraint) {
            // The clicked bone becomes the target of a new IK / transform constraint on the selected bone(s).
            int b = pickBone(mouse);
            int kind = pickConstraint;
            std::vector<int> bones = doc.selectedBones();
            if (kind == 1) {
                // Spine: click a bone to target it; click empty space to create a new target bone there; click one
                // of the constrained bones to create the target at its tip.
                if (bones.empty() || bones.size() > 2) { pickConstraint = 0; setStatus("An IK constraint needs 1 bone, or a parent and its child, selected"); return; }
                int res = -1;
                if (b >= 0 && std::find(bones.begin(), bones.end(), b) == bones.end()) {
                    res = doc.createIk("ik", bones, b);
                } else {
                    Vec2 at = mw;
                    if (b >= 0) { int last = bones.back(); at = doc.sk.bones[last].localToWorld({doc.data.bones[last].length, 0}); }
                    res = doc.createIk("ik", bones, -1, &at);
                }
                pickConstraint = 0;
                setStatus(res >= 0 ? "IK constraint created"
                                   : "Cannot create the IK constraint: the target cannot be a constrained bone or one of its children, and two bones must be parent and child");
                return;
            }
            if (b < 0) { setStatus("No bone under the cursor: click the target bone (Esc cancels)"); return; }
            pickConstraint = 0;
            bones.erase(std::remove(bones.begin(), bones.end(), b), bones.end()); // a bone cannot target itself
            setStatus(doc.createTransform("transform", bones, b) >= 0 ? "Transform constraint created" : "Select the bone(s) to constrain");
            return;
        }

        if (pickParent) {
            // A miss keeps the pick active so the user can aim again; Esc cancels.
            int b = pickBone(mouse);
            if (b < 0) { setStatus("No bone under the cursor: click a bone (Esc cancels)"); return; }
            pickParent = false;
            std::vector<int> bones = doc.selectedBones();
            std::vector<int> slots = doc.selectedSlots();
            if (!bones.empty()) doc.reparentBones(bones, b);
            for (int s : slots) doc.reparentSlot(s, b);
            setStatus("Parent set to " + doc.data.bones[b].name);
            return;
        }

        if (doc.tool == Tool::Weights && selAtt && selAtt->type == AttachmentType::Mesh) {
            if (bindMode) { // Bind: each bone clicked is bound to the mesh when binding finishes
                int b = pickBone(mouse);
                if (b < 0) { setStatus("Click a bone to bind (Space/Esc finishes)"); return; }
                if (std::find(bindPending.begin(), bindPending.end(), b) == bindPending.end()) bindPending.push_back(b);
                setStatus(doc.data.bones[b].name + " will be bound (" + std::to_string(bindPending.size()) + " bones)");
                return;
            }
            if (!selAtt->isWeighted()) { setStatus("Bind the mesh to bones first: use Bind in the Weights view, then click the bones"); return; }
            std::vector<float> wv;
            meshWorld(doc, selSlot, *selAtt, wv);
            int hit = -1;
            float hitD = 9 * uiScale;
            for (int k = 0; k < selAtt->vertexCount(); k++) {
                float dd = dist(toScreen({wv[k * 2], wv[k * 2 + 1]}), mouse);
                if (dd < hitD) { hitD = dd; hit = k; }
            }
            auto inSel = [&](int k) { return std::find(meshSelection.begin(), meshSelection.end(), k) != meshSelection.end(); };
            const bool ctrl = io.KeyCtrl;
            if (!weightBrush) {
                // Direct: click a vertex to select it, drag up/down to change the selected bones' weights on the
                // selected vertices; drag across empty space to box-select.
                if (hit >= 0) {
                    if (ctrl) { auto it = std::find(meshSelection.begin(), meshSelection.end(), hit); if (it == meshSelection.end()) meshSelection.push_back(hit); else meshSelection.erase(it); }
                    else if (!inSel(hit)) meshSelection = {hit};
                    if (inSel(hit) && !weightBones.empty()) {
                        drag.active = true;
                        drag.kind = 7;
                        drag.startAtt = *selAtt;
                        drag.adjustVertices = meshSelection;
                        drag.adjustBones.assign(weightBones.begin(), weightBones.end());
                        drag.adjustBase.clear();
                        for (int v : drag.adjustVertices) for (int b : drag.adjustBones) drag.adjustBase.push_back(boneWeightAt(*selAtt, v, b));
                        doc.beginEdit("Adjust weights");
                    } else if (weightBones.empty()) setStatus("Select a bone in the Bones list to adjust its weights");
                    return;
                }
                if (!ctrl) meshSelection.clear();
                drag.active = true;
                drag.kind = 3;
                return;
            }
            // Brush: paints with the selected bones on the selected vertices (all of them when none are selected).
            // Ctrl+drag box-selects; a click without dragging selects one vertex.
            if (ctrl) { drag.active = true; drag.kind = 3; return; }
            if (weightBones.empty()) setStatus("Select a bone in the Bones list to paint with it");
            doc.beginEdit("Paint weights");
            drag.active = true;
            drag.kind = 6;
            drag.startTime = ImGui::GetTime();
            return;
        }

        // Mesh dots are handles, as in Spine: on the selected mesh, or on the mesh under the cursor (whose dots
        // are showing), dragging a dot moves that vertex. Setup mode edits the mesh; Animate mode keys a deform.
        if (!meshEditing && !add) {
            int ms = -1;
            const Attachment* ma = nullptr;
            if (selAtt && selAtt->type == AttachmentType::Mesh && selSlot >= 0) { ms = selSlot; ma = selAtt; }
            else if (hoverSlot >= 0 && hoverSlot < (int)doc.sk.slots.size()) {
                const Attachment* h = doc.sk.attachmentFor(doc.sk.slots[hoverSlot]);
                if (h && h->type == AttachmentType::Mesh) { ms = hoverSlot; ma = h; }
            }
            std::vector<float> wv;
            if (ma && meshWorld(doc, ms, *ma, wv)) {
                for (int k = 0; k < ma->vertexCount(); k++) {
                    if (dist(toScreen({wv[k * 2], wv[k * 2 + 1]}), mouse) >= 7) continue;
                    if (ma != selAtt) { // grab a hovered mesh: select it first
                        doc.select({SelType::Attachment, ms, ma->name});
                        selAtt = selectedAttachment(&selSlot);
                        if (!selAtt) break;
                    }
                    meshSelection = {k};
                    doc.beginEdit(doc.mode == Mode::Setup ? "Move vertices" : "Deform");
                    drag.active = true;
                    drag.kind = 5;
                    drag.startAtt = *selAtt;
                    drag.attachment = doc.primary();
                    return;
                }
            }
        }

        if (meshEditing && meshMode == MeshMode::New) {
            // New: click places hull vertices, drag moves one, double-click deletes one; the first closes the hull.
            const Bone& sb = doc.sk.bones[doc.data.slots[selSlot].bone];
            int ph = -1;
            for (size_t k = 0; k < newHull.size(); k++)
                if (dist(toScreen(sb.localToWorld(newHull[k])), mouse) < 8 * uiScale) ph = (int)k;
            if (ph >= 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { newHull.erase(newHull.begin() + ph); return; }
            if (ph == 0 && newHull.size() >= 3) { meshFinishNew(true); return; }
            if (ph >= 0) { drag.active = true; drag.kind = 8; drag.newPoint = ph; return; }
            newHull.push_back(sb.worldToLocal(mw));
            return;
        }

        if (meshEditing) {
            std::vector<float> wv;
            meshWorld(doc, selSlot, *selAtt, wv);
            int hit = -1;
            for (int k = 0; k < selAtt->vertexCount(); k++)
                if (dist(toScreen({wv[k * 2], wv[k * 2 + 1]}), mouse) < 7) hit = k;
            // Double-click a vertex (Modify or Create) deletes it.
            if (hit >= 0 && (meshMode == MeshMode::Modify || meshMode == MeshMode::Create) && doc.mode == Mode::Setup && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                std::string an = selAtt->name;
                bool weighted = selAtt->isWeighted();
                if (!doc.deleteMeshVertices(selSlot, an, {hit})) setStatus(weighted ? "Unbind weights before deleting vertices" : "A mesh needs at least 3 vertices");
                meshSelection.clear();
                return;
            }
            if (meshMode == MeshMode::Delete) {
                if (hit >= 0) {
                    std::string an = selAtt->name;
                    bool weighted = selAtt->isWeighted();
                    if (!doc.deleteMeshVertices(selSlot, an, {hit})) setStatus(weighted ? "Unbind weights before deleting vertices" : "A mesh needs at least 3 vertices");
                    meshSelection.clear();
                    return;
                }
                meshSelection.clear();
                drag.active = true;
                drag.kind = 3; // drag a box to delete several (done on release)
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
                doc.dropDeformKeys(selSlot, m.name);
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

        int gh = gizmoHit(mouse); // a gizmo handle wins over picking whatever is underneath
        int bone = gh ? -1 : pickBone(mouse);
        int slot = (bone < 0 && !gh) ? pickSlot(mouse) : -1;
        if (gh) {
            drag.handle = gh;
        } else if (bone >= 0) {
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
        if (GizmoTarget gt = gizmoTarget(); gt.valid) { drag.axX = gt.x; drag.axY = gt.y; }
        drag.active = true;
        drag.kind = 1;
        std::vector<int> bones = doc.selectedBones();
        // With several objects selected, the images among them move with their bones: add those bones too.
        if (doc.selection.size() > 1)
            for (const Sel& s : doc.selection)
                if ((s.type == SelType::Attachment || s.type == SelType::Slot) && s.index < (int)doc.data.slots.size()) {
                    int sb = doc.data.slots[s.index].bone;
                    if (std::find(bones.begin(), bones.end(), sb) == bones.end()) bones.push_back(sb);
                }
        // A group moves its top-most selected bones; a selected child follows its selected parent instead of
        // being moved twice (once by itself, once through the parent).
        for (int b : bones) {
            bool ancestorSelected = false;
            for (int q : bones) if (q != b && doc.data.isDescendant(b, q)) ancestorSelected = true;
            if (ancestorSelected) continue;
            drag.bones.push_back(b);
            drag.startPose.push_back(doc.bonePose(b));
            const Bone& bb = doc.sk.bones[b];
            drag.startWorldPos.push_back({bb.worldX, bb.worldY});
            drag.startAngles.push_back(bb.worldRotationX());
        }
        // A group's pivot is the world origin: Rotate and Scale turn all selected objects about it.
        drag.pivot = {0, 0};
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
        // While the cursor is on a dot of the mesh already hovered, keep that mesh hovered (not a bone beneath).
        int prevSlot = hoverSlot;
        bool onMeshDot = false;
        if (vpHovered && !picking() && prevSlot >= 0 && prevSlot < (int)doc.sk.slots.size()) {
            const Attachment* h = doc.sk.attachmentFor(doc.sk.slots[prevSlot]);
            std::vector<float> wv;
            if (h && h->type == AttachmentType::Mesh && meshWorld(doc, prevSlot, *h, wv))
                for (size_t k = 0; k + 1 < wv.size() && !onMeshDot; k += 2) onMeshDot = dist(toScreen({wv[k], wv[k + 1]}), mouse) < 7;
        }
        hoverBone = hoverSlot = -1;
        hoverHandle = 0;
        if (onMeshDot) { hoverSlot = prevSlot; ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); return; }
        if (vpHovered && !meshEditing && doc.tool != Tool::Weights) {
            hoverHandle = picking() ? 0 : gizmoHit(mouse);
            if (!hoverHandle) {
                hoverBone = pickBone(mouse);
                if (hoverBone < 0 && !picking()) hoverSlot = pickSlot(mouse);
            }
            if (hoverHandle || hoverBone >= 0 || hoverSlot >= 0) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        return;
    }

    // ---- dragging ----
    bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (dist(mouse, drag.startScreen) > 3) drag.moved = true;

    if (drag.kind == 7) { // direct weights: drag up/down to change the selected bones' weights on the selected vertices
        int si;
        Attachment* a = selectedAttachment(&si);
        if (a && a->isWeighted() && drag.moved) {
            float delta = -(mouse.y - drag.startScreen.y) / 150.0f; // up adds weight
            *a = drag.startAtt;                                     // always from the weights the drag began with
            std::vector<VertexWeightEdit> edits;
            for (size_t k = 0; k < drag.adjustVertices.size(); k++)
                for (size_t j = 0; j < drag.adjustBones.size(); j++)
                    edits.push_back({drag.adjustVertices[k], drag.adjustBones[j], clampf(drag.adjustBase[k * drag.adjustBones.size() + j] + delta, 0, 1)});
            setVertexWeights(*a, doc.sk, edits, lockedBonesFor(si, *a));
            doc.dirty = true;
        }
        if (!down) { drag.moved ? doc.commitEdit() : doc.cancelEdit(); drag.active = false; }
        return;
    }

    if (drag.kind == 6) { // weights brush
        int si;
        Attachment* a = selectedAttachment(&si);
        // The brush starts after a moment or a move, so a plain click can select a vertex instead.
        bool painting = drag.moved || ImGui::GetTime() - drag.startTime > 0.2;
        if (a && a->isWeighted() && !weightBones.empty() && painting && down) {
            std::vector<float> wv;
            meshWorld(doc, si, *a, wv);
            float dt = io.DeltaTime;
            float inner = brushSize * (1 - clampf(brushFeather, 0, 100) / 100); // full strength inside, falling to 0 at the edge
            std::vector<VertexWeightEdit> edits;
            for (int k = 0; k < a->vertexCount(); k++) {
                if (!meshSelection.empty() && std::find(meshSelection.begin(), meshSelection.end(), k) == meshSelection.end()) continue;
                float dd = dist(toScreen({wv[k * 2], wv[k * 2 + 1]}), mouse);
                if (dd > brushSize) continue;
                float fall = dd <= inner || brushSize - inner < 1e-3f ? 1.0f : 1 - (dd - inner) / (brushSize - inner);
                for (int bone : weightBones) {
                    float cur = boneWeightAt(*a, k, bone), amount = brushStrength * fall * dt * 8, nw = cur;
                    switch (weightMode) {
                    case WeightMode::Add: nw = cur + amount; break;
                    case WeightMode::Remove: nw = cur - amount; break;
                    case WeightMode::Replace: nw = cur + (brushStrength - cur) * std::min(1.0f, fall * dt * 12); break;
                    }
                    edits.push_back({k, bone, clampf(nw, 0, 1)});
                }
            }
            setVertexWeights(*a, doc.sk, edits, lockedBonesFor(si, *a));
            doc.dirty = true;
        }
        if (!down) {
            if (!painting && a && a->isWeighted()) { // a click: select the vertex under the cursor, or clear the selection
                std::vector<float> wv;
                meshWorld(doc, si, *a, wv);
                int hit = -1;
                float hitD = 12 * uiScale;
                for (int k = 0; k < a->vertexCount(); k++) {
                    float dd = dist(toScreen({wv[k * 2], wv[k * 2 + 1]}), mouse);
                    if (dd < hitD) { hitD = dd; hit = k; }
                }
                if (hit >= 0) meshSelection = {hit}; else meshSelection.clear();
                doc.cancelEdit();
            } else painting ? doc.commitEdit() : doc.cancelEdit();
            drag.active = false;
        }
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
            } else if (doc.currentAnimation()) {
                // Animate mode: the moved vertices become deform offsets at this frame, keyed with
                // Auto Key or left pending (orange) until keyed, as in Spine.
                const Slot& slot = doc.sk.slots[si];
                int len = a->deformLength();
                if (!drag.deformCaptured) { // the deform shown when the drag started
                    drag.deformBase = slot.deform.size() == (size_t)len && slot.deformFor == a->name ? slot.deform : std::vector<float>(len, 0.0f);
                    drag.deformCaptured = true;
                }
                std::vector<float> nd = drag.deformBase;
                if (!a->isWeighted()) {
                    const Bone& b = doc.sk.bones[slot.bone];
                    Vec2 o = b.worldToLocal({b.worldX, b.worldY}), d2 = b.worldToLocal(Vec2(b.worldX, b.worldY) + delta);
                    for (int v : meshSelection) { nd[v * 2] += d2.x - o.x; nd[v * 2 + 1] += d2.y - o.y; }
                } else {
                    size_t bi = 0, fi = 0;
                    for (int v = 0; v < a->vertexCount(); v++) {
                        int n = a->bones[bi++];
                        bool sel = std::find(meshSelection.begin(), meshSelection.end(), v) != meshSelection.end();
                        for (int q = 0; q < n; q++, bi++, fi += 2) {
                            if (!sel) continue;
                            const Bone& b = doc.sk.bones[a->bones[bi]];
                            Vec2 o = b.worldToLocal({b.worldX, b.worldY}), d2 = b.worldToLocal(Vec2(b.worldX, b.worldY) + delta);
                            nd[fi] += d2.x - o.x;
                            nd[fi + 1] += d2.y - o.y;
                        }
                    }
                }
                doc.setDeform(si, a->name, nd);
            }
            doc.dirty = true;
        }
        if (!down) { drag.moved ? doc.commitEdit() : doc.cancelEdit(); drag.active = false; }
        return;
    }

    if (drag.kind == 8) { // New: move a hull vertex being placed
        if (down && selSlot >= 0 && drag.newPoint >= 0 && drag.newPoint < (int)newHull.size())
            newHull[drag.newPoint] = doc.sk.bones[doc.data.slots[selSlot].bone].worldToLocal(mw);
        if (!down) drag.active = false;
        return;
    }

    if (drag.kind == 3) { // box select
        if (!down) {
            ImVec2 a = drag.startScreen;
            float x0 = std::min(a.x, mouse.x), x1 = std::max(a.x, mouse.x), y0 = std::min(a.y, mouse.y), y1 = std::max(a.y, mouse.y);
            auto inBox = [&](ImVec2 p) { return p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1; };
            // The Weights tool box-selects the selected mesh's vertices too (Direct mode, or Ctrl+drag in Brush mode).
            const bool weightVerts = doc.tool == Tool::Weights && selAtt && selAtt->type == AttachmentType::Mesh && selAtt->isWeighted();
            if (meshEditing || weightVerts) {
                std::vector<float> wv;
                meshWorld(doc, selSlot, *selAtt, wv);
                for (int k = 0; k < selAtt->vertexCount(); k++)
                    if (inBox(toScreen({wv[k * 2], wv[k * 2 + 1]})) && std::find(meshSelection.begin(), meshSelection.end(), k) == meshSelection.end())
                        meshSelection.push_back(k);
                if (meshEditing && meshMode == MeshMode::Delete && !meshSelection.empty()) { // Delete: the box removes what it holds
                    std::string an = selAtt->name;
                    bool weighted = selAtt->isWeighted();
                    if (!doc.deleteMeshVertices(selSlot, an, meshSelection)) setStatus(weighted ? "Unbind weights before deleting vertices" : "A mesh needs at least 3 vertices");
                    meshSelection.clear();
                }
            } else if (drag.moved) {
                // Marquee: bones whose origin is inside, and images whose outline touches the box.
                // Shift/Ctrl (the `add` the press saw) keeps the existing selection, otherwise it was cleared on press.
                auto addOnce = [&](const Sel& s) { if (!doc.isSelected(s)) doc.select(s, true); }; // a marquee adds, never toggles
                if (showBones && selectBones)
                    for (size_t i = 0; i < doc.sk.bones.size(); i++)
                        if (boneVisible((int)i) && inBox(toScreen({doc.sk.bones[i].worldX, doc.sk.bones[i].worldY})))
                            addOnce({SelType::Bone, (int)i, ""});
                if (showImages && selectImages && doc.tool != Tool::Pose) {
                    for (int si = 0; si < (int)doc.sk.slots.size(); si++) {
                        if (!slotVisible(si)) continue;
                        const Attachment* a = doc.sk.attachmentFor(doc.sk.slots[si]);
                        std::vector<float> wv;
                        if (!a || !doc.sk.worldVertices(doc.sk.slots[si], *a, wv)) continue;
                        float mx0 = 1e9f, my0 = 1e9f, mx1 = -1e9f, my1 = -1e9f;
                        for (size_t k = 0; k + 1 < wv.size(); k += 2) {
                            ImVec2 p = toScreen({wv[k], wv[k + 1]});
                            mx0 = std::min(mx0, p.x); my0 = std::min(my0, p.y);
                            mx1 = std::max(mx1, p.x); my1 = std::max(my1, p.y);
                        }
                        if (mx1 < x0 || mx0 > x1 || my1 < y0 || my0 > y1) continue;
                        // Same selection a click on the image makes: the attachment in Setup, its bone in Animate.
                        Sel s = doc.mode == Mode::Setup ? Sel{SelType::Attachment, si, a->name} : Sel{SelType::Bone, doc.data.slots[si].bone, ""};
                        addOnce(s);
                    }
                }
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
        if (!drag.bones.empty() && drag.bones.size() > 1 && (doc.tool == Tool::Rotate || doc.tool == Tool::Scale)) {
            // Group rotate / scale about the pivot: every bone keeps its place relative to the pivot.
            Vec2 pv = drag.pivot;
            float d = 0, r = 1;
            if (doc.tool == Tool::Rotate) {
                d = wrapDeg(atan2Deg(mw.y - pv.y, mw.x - pv.x) - atan2Deg(drag.startWorld.y - pv.y, drag.startWorld.x - pv.x));
            } else {
                float d0 = (drag.startWorld - pv).len(), d1 = (mw - pv).len();
                r = d0 > 4 / zoom ? d1 / d0 : 1;
            }
            float rad = d * degRad, cs = std::cos(rad), sn = std::sin(rad);
            for (size_t i = 0; i < drag.bones.size(); i++) {
                int bi = drag.bones[i];
                const Bone& b = doc.sk.bones[bi];
                Vec2 q = drag.startWorldPos[i] - pv;
                Vec2 np = doc.tool == Tool::Rotate ? pv + Vec2(cs * q.x - sn * q.y, sn * q.x + cs * q.y) : pv + q * r;
                Vec2 l = b.worldToParent(doc.sk, np);
                Document::LocalPose p = drag.startPose[i];
                p.x = l.x; p.y = l.y;
                const WorldMatrix& pm = doc.sk.parentMatrix(bi);
                bool reflect = pm.a * pm.d - pm.b * pm.c < 0;
                if (doc.tool == Tool::Rotate) {
                    p.rotation = drag.startPose[i].rotation + (reflect ? -d : d);
                    doc.setBonePose(bi, p, (int)Channel::Rotate | (int)Channel::Translate);
                } else {
                    p.sx = drag.startPose[i].sx * r; p.sy = drag.startPose[i].sy * r;
                    doc.setBonePose(bi, p, (int)Channel::Scale | (int)Channel::Translate);
                }
            }
        } else if (!drag.bones.empty()) {
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
                    if (drag.handle == 1) target = origin + drag.axX * delta.dot(drag.axX);
                    else if (drag.handle == 2) target = origin + drag.axY * delta.dot(drag.axY);
                    else if (io.KeyShift) { // constrain to the dominant axis
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
                    if (drag.handle != 2 && std::fabs(d0.dot(u)) > minD) p.sx = drag.startPose[i].sx * d1.dot(u) / d0.dot(u);
                    if (drag.handle != 1 && std::fabs(d0.dot(vv)) > minD) p.sy = drag.startPose[i].sy * d1.dot(vv) / d0.dot(vv);
                    if (drag.handle == 3 || (drag.handle == 0 && io.KeyShift)) { // uniform
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
                    if (drag.handle == 2 || (drag.handle != 1 && io.KeyShift)) p.shy = drag.startPose[i].shy + d;
                    else p.shx = drag.startPose[i].shx + d;
                    doc.setBonePose(bi, p, (int)Channel::Shear);
                    break;
                }
                case Tool::Pose:
                    // The dragged bone itself turns about its joint (the root moves); its image and children follow.
                    doc.poseDrag(bi, drag.startPose[i], origin, drag.startWorld, mw);
                    break;
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
                        Vec2 dd = delta;
                        if (drag.handle == 1) dd = drag.axX * delta.dot(drag.axX);
                        else if (drag.handle == 2) dd = drag.axY * delta.dot(drag.axY);
                        Vec2 l = b.worldToLocal(origin + dd);
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
                        Vec2 v0 = drag.startWorld - origin, v1 = mw - origin;
                        float d0 = v0.len(), d1 = v1.len();
                        if (drag.handle == 1 || drag.handle == 2) {
                            Vec2 ax = drag.handle == 1 ? drag.axX : drag.axY;
                            if (std::fabs(v0.dot(ax)) > 2 / zoom) {
                                float r = v1.dot(ax) / v0.dot(ax);
                                if (drag.handle == 1) a->scaleX = s.scaleX * r; else a->scaleY = s.scaleY * r;
                            }
                        } else if (d0 > 2 / zoom) { a->scaleX = s.scaleX * d1 / d0; a->scaleY = s.scaleY * d1 / d0; }
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

// ---- gizmos ----

Editor::GizmoTarget Editor::gizmoTarget() {
    GizmoTarget g;
    if (doc.tool == Tool::Pose || doc.tool == Tool::Create || doc.tool == Tool::Weights) return g;
    // Several bones selected: the gizmo sits on the world origin, the pivot for group rotate and scale.
    if (doc.selectedBones().size() > 1) {
        g.valid = true;
        g.origin = {0, 0};
        return g;
    }
    Sel p = doc.primary();
    float rot = 0;
    Vec2 origin;
    int parentBone = -1;
    if (p.type == SelType::Bone && p.index < (int)doc.sk.bones.size()) {
        const Bone& b = doc.sk.bones[p.index];
        origin = {b.worldX, b.worldY};
        rot = b.worldRotationX();
        parentBone = b.parent;
    } else if (p.type == SelType::Attachment && doc.mode == Mode::Setup && p.index < (int)doc.sk.slots.size()) {
        int si = -1;
        Attachment* a = selectedAttachment(&si);
        if (!a || si < 0) return g;
        const Bone& b = doc.sk.bones[doc.data.slots[si].bone];
        if (a->type == AttachmentType::Region || a->type == AttachmentType::Point) {
            origin = b.localToWorld({a->x, a->y});
            rot = b.worldRotationX() + a->rotation;
        } else if (a->isVertex() && !a->isWeighted()) {
            origin = {b.worldX, b.worldY};
            rot = b.worldRotationX();
        } else return g;
        parentBone = doc.data.slots[si].bone;
    } else return g;
    if (doc.tool == Tool::Translate) { // the Local / Parent / World toggle only affects translation
        if (doc.axes == Axes::World) rot = 0;
        else if (doc.axes == Axes::Parent) rot = parentBone >= 0 ? doc.sk.bones[parentBone].worldRotationX() : 0;
    }
    g.valid = true;
    g.origin = origin;
    g.x = {cosDeg(rot), sinDeg(rot)};
    g.y = {-sinDeg(rot), cosDeg(rot)};
    return g;
}

// Gizmo geometry in screen pixels (before DPI scale).
static const float kAxisLen = 28, kRingR = 15, kShearLen = 38, kHandleTol = 6;

int Editor::gizmoHit(ImVec2 m) {
    GizmoTarget g = gizmoTarget();
    if (!g.valid) return 0;
    const float u = uiScale;
    ImVec2 c = toScreen(g.origin);
    Vec2 cp(c.x, c.y), mp(m.x, m.y);
    Vec2 ax(g.x.x, -g.x.y), ay(g.y.x, -g.y.y); // screen-space directions
    float tol = kHandleTol * u;
    auto seg = [&](Vec2 a, Vec2 b) { return distToSegment(mp, cp + a, cp + b) < tol; };
    switch (doc.tool) {
    case Tool::Rotate: return std::fabs((mp - cp).len() - kRingR * u) < tol ? 3 : 0;
    case Tool::Translate:
        if ((mp - cp).len() < 5 * u) return 3;
        if (seg(ax * (7 * u), ax * ((kAxisLen + 6) * u))) return 1;
        if (seg(ay * (7 * u), ay * ((kAxisLen + 6) * u))) return 2;
        return 0;
    case Tool::Scale:
        if ((mp - cp).len() < 6 * u) return 3;
        if ((mp - (cp + ax * (kAxisLen * u))).len() < 8 * u || seg(ax * (7 * u), ax * (kAxisLen * u))) return 1;
        if ((mp - (cp + ay * (kAxisLen * u))).len() < 8 * u || seg(ay * (7 * u), ay * (kAxisLen * u))) return 2;
        return 0;
    case Tool::Shear:
        if ((mp - (cp + ax * (kShearLen * u))).len() < 8 * u || seg(ax * (7 * u), ax * (kShearLen * u))) return 1;
        if ((mp - (cp + ay * (kShearLen * u))).len() < 8 * u || (mp - (cp - ay * (kShearLen * u))).len() < 8 * u ||
            seg(ay * (-kShearLen * u), ay * (kShearLen * u))) return 2;
        return 0;
    default: return 0;
    }
}

void Editor::drawGizmo(ImDrawList* dl) {
    GizmoTarget g = gizmoTarget();
    if (!g.valid) return;
    const float u = uiScale;
    ImVec2 c = toScreen(g.origin);
    ImVec2 ax(g.x.x, -g.x.y), ay(g.y.x, -g.y.y);
    int active = drag.active && drag.kind == 1 ? drag.handle : hoverHandle;
    auto pick = [&](int handle, ImU32 normal, ImU32 lit) { return active == handle ? lit : normal; };
    ImU32 red = IM_COL32(225, 60, 60, 255), redLit = IM_COL32(255, 140, 130, 255);
    ImU32 green = IM_COL32(70, 200, 80, 255), greenLit = IM_COL32(150, 250, 150, 255);
    ImU32 white = IM_COL32(255, 255, 255, 255), yellow = IM_COL32(240, 200, 60, 255);
    auto at = [&](ImVec2 d, float len) { return ImVec2(c.x + d.x * len * u, c.y + d.y * len * u); };
    auto arrow = [&](ImVec2 d, ImU32 cc) {
        ImVec2 tip = at(d, kAxisLen + 7), base = at(d, kAxisLen - 3), n(-d.y, d.x);
        dl->AddTriangleFilled(tip, ImVec2(base.x + n.x * 5 * u, base.y + n.y * 5 * u), ImVec2(base.x - n.x * 5 * u, base.y - n.y * 5 * u), cc);
    };
    switch (doc.tool) {
    case Tool::Rotate: {
        ImU32 cc = pick(3, red, redLit);
        dl->AddCircle(c, kRingR * u, IM_COL32(0, 0, 0, 120), 40, 4.0f * u);
        dl->AddCircle(c, kRingR * u, cc, 40, (active == 3 ? 3.0f : 2.2f) * u);
        break;
    }
    case Tool::Translate: {
        ImU32 cx = pick(1, red, redLit), cy = pick(2, green, greenLit);
        dl->AddLine(at(ax, 4), at(ax, kAxisLen), cx, 2.4f * u);
        dl->AddLine(at(ay, 4), at(ay, kAxisLen), cy, 2.4f * u);
        arrow(ax, cx);
        arrow(ay, cy);
        dl->AddRectFilled(ImVec2(c.x - 3 * u, c.y - 3 * u), ImVec2(c.x + 3 * u, c.y + 3 * u), pick(3, yellow, white));
        break;
    }
    case Tool::Scale: {
        ImU32 cx = pick(1, red, redLit), cy = pick(2, green, greenLit);
        dl->AddLine(at(ax, 4), at(ax, kAxisLen), cx, 2.4f * u);
        dl->AddLine(at(ay, 4), at(ay, kAxisLen), cy, 2.4f * u);
        ImVec2 tx = at(ax, kAxisLen), ty = at(ay, kAxisLen);
        dl->AddRectFilled(ImVec2(tx.x - 5 * u, tx.y - 5 * u), ImVec2(tx.x + 5 * u, tx.y + 5 * u), cx);
        dl->AddRectFilled(ImVec2(ty.x - 5 * u, ty.y - 5 * u), ImVec2(ty.x + 5 * u, ty.y + 5 * u), cy);
        dl->AddRectFilled(ImVec2(c.x - 4 * u, c.y - 4 * u), ImVec2(c.x + 4 * u, c.y + 4 * u), pick(3, yellow, white));
        break;
    }
    case Tool::Shear: {
        ImU32 cx = pick(1, red, redLit), cy = pick(2, green, greenLit);
        dl->AddLine(c, at(ax, kShearLen), cx, 2.2f * u);
        dl->AddLine(at(ay, -kShearLen), at(ay, kShearLen), cy, 2.2f * u);
        dl->AddCircleFilled(at(ax, kShearLen), 4.5f * u, cx);
        dl->AddCircleFilled(at(ay, kShearLen), 4.5f * u, cy);
        dl->AddCircleFilled(at(ay, -kShearLen), 4.5f * u, cy);
        break;
    }
    default: break;
    }
}

// ---- hover feedback ----

const std::vector<float>& Editor::imageOutline(const std::string& file) {
    auto it = outlineCache.find(file);
    if (it != outlineCache.end()) return it->second;
    std::vector<float> hull;
    if (Image* im = gfx.image(file)) hull = traceHull(im->rgba.data(), im->w, im->h, 20, 48, 0);
    return outlineCache[file] = hull;
}

static void dottedPolygon(ImDrawList* dl, const std::vector<ImVec2>& p, ImU32 col, float dash, float gap, float th) {
    if (p.size() < 2) return;
    float acc = 0;
    bool on = true;
    for (size_t i = 0; i < p.size(); i++) {
        ImVec2 a = p[i], b = p[(i + 1) % p.size()];
        float len = dist(a, b);
        if (len < 0.001f) continue;
        ImVec2 d((b.x - a.x) / len, (b.y - a.y) / len);
        float t = 0;
        while (t < len) {
            float lim = on ? dash : gap;
            float step = std::min(lim - acc, len - t);
            if (on) dl->AddLine(ImVec2(a.x + d.x * t, a.y + d.y * t), ImVec2(a.x + d.x * (t + step), a.y + d.y * (t + step)), col, th);
            t += step;
            acc += step;
            if (acc >= lim - 1e-4f) { acc = 0; on = !on; }
        }
    }
}

void Editor::drawHover(ImDrawList* dl) {
    if (hoverSlot < 0 || hoverSlot >= (int)doc.sk.slots.size()) return;
    const Slot& slot = doc.sk.slots[hoverSlot];
    const Attachment* a = doc.sk.attachmentFor(slot);
    if (!a) return;
    std::vector<float> v;
    if (!doc.sk.worldVertices(slot, *a, v) || v.size() < 4) return;
    std::vector<ImVec2> pts;
    if (a->type == AttachmentType::Region) {
        std::string file = doc.imagePathFor(*a, slot.sequenceIndex);
        if (!file.empty()) {
            const std::vector<float>& hull = imageOutline(file);
            Image* im = hull.empty() ? nullptr : gfx.image(file);
            if (im && im->w > 0 && im->h > 0) {
                float c[8];
                a->regionCorners(c); // local BL, TL, TR, BR
                const Bone& b = doc.sk.bones[slot.bone];
                for (size_t i = 0; i + 1 < hull.size(); i += 2) {
                    float uu = hull[i] / im->w, vv = hull[i + 1] / im->h;
                    float lx = c[2] + uu * (c[4] - c[2]) + vv * (c[0] - c[2]);
                    float ly = c[3] + uu * (c[5] - c[3]) + vv * (c[1] - c[3]);
                    pts.push_back(toScreen(b.localToWorld({lx, ly})));
                }
            }
        }
    } else if (a->type == AttachmentType::Mesh || a->type == AttachmentType::LinkedMesh) {
        const Attachment* gm = a;
        if (a->type == AttachmentType::LinkedMesh) gm = doc.data.findAttachment(doc.data.findSkin(a->parentSkin), hoverSlot, a->parentMesh);
        int n = gm ? gm->hullLength : 0;
        for (int i = 0; i < n && i * 2 + 1 < (int)v.size(); i++) pts.push_back(toScreen({v[i * 2], v[i * 2 + 1]}));
    }
    if (pts.empty())
        for (size_t i = 0; i + 1 < v.size(); i += 2) pts.push_back(toScreen({v[i], v[i + 1]}));
    // White dashes over a dark under-stroke so they read on any art.
    dl->AddPolyline(pts.data(), (int)pts.size(), IM_COL32(0, 0, 0, 70), ImDrawFlags_Closed, 3.0f);
    dottedPolygon(dl, pts, IM_COL32(255, 255, 255, 240), 4 * uiScale, 3 * uiScale, 1.6f);
    // Vertex dots, as in Spine's hover: every mesh vertex is a small handle on the traced shape.
    bool isMesh = a->type == AttachmentType::Mesh || a->type == AttachmentType::LinkedMesh;
    for (size_t i = 0; isMesh && i + 1 < v.size(); i += 2) {
        ImVec2 p = toScreen({v[i], v[i + 1]});
        dl->AddCircleFilled(p, 4.0f * uiScale, IM_COL32(0, 0, 0, 150));
        dl->AddCircleFilled(p, 2.6f * uiScale, IM_COL32(255, 255, 255, 255));
    }
}

// Name chip above the toolbar: the hovered bone/image, otherwise the selection's path through the hierarchy.
void Editor::drawHoverChip(ImDrawList* dl) {
    struct Item { Icon icon; ImU32 color; std::string text; };
    std::vector<Item> items;
    const SkeletonData& d = doc.data;
    auto boneItem = [&](int b) { return Item{Icon::Bone, toU32(d.bones[b].color), d.bones[b].name}; };
    auto attIcon = [&](const Attachment* a) -> Icon {
        if (!a) return Icon::Region;
        switch (a->type) {
        case AttachmentType::Mesh: return Icon::Mesh;
        case AttachmentType::LinkedMesh: return Icon::LinkedMesh;
        case AttachmentType::BoundingBox: return Icon::BoundingBox;
        case AttachmentType::Path: return Icon::Path;
        case AttachmentType::Point: return Icon::Point;
        case AttachmentType::Clipping: return Icon::Clipping;
        default: return Icon::Region;
        }
    };
    auto chain = [&](int bone) {
        std::vector<int> c;
        for (int b = bone; b >= 0; b = d.bones[b].parent) c.push_back(b);
        std::reverse(c.begin(), c.end());
        for (int b : c) items.push_back(boneItem(b));
    };
    bool hovering = hoverBone >= 0 || hoverSlot >= 0;
    if (hoverBone >= 0) items.push_back(boneItem(hoverBone));
    else if (hoverSlot >= 0) {
        const Attachment* a = doc.sk.attachmentFor(doc.sk.slots[hoverSlot]);
        items.push_back({attIcon(a), IM_COL32(120, 210, 220, 255), a ? a->name : d.slots[hoverSlot].name});
    } else {
        Sel p = doc.primary();
        if (p.type == SelType::Bone && p.index < (int)d.bones.size()) chain(p.index);
        else if ((p.type == SelType::Slot || p.type == SelType::Attachment) && p.index < (int)d.slots.size()) {
            chain(d.slots[p.index].bone);
            items.push_back({Icon::Slot, IM_COL32(230, 230, 230, 255), d.slots[p.index].name});
            if (p.type == SelType::Attachment)
                items.push_back({attIcon(doc.data.findAttachment(doc.skin, p.index, p.name)), IM_COL32(120, 210, 220, 255), p.name});
        }
    }
    if (items.empty()) return;
    const float u = uiScale, isz = 15 * u, pad = 8 * u, sep = 14 * u;
    float w = pad * 2;
    for (size_t i = 0; i < items.size(); i++) w += isz + 4 * u + ImGui::CalcTextSize(items[i].text.c_str()).x + (i ? sep : 0);
    float h = 24 * u;
    ImVec2 mn(vpOrigin.x + (vpSize.x - w) / 2, toolbarTop - h - 6 * u);
    ImVec2 mx(mn.x + w, mn.y + h);
    dl->AddRectFilled(mn, mx, hovering ? IM_COL32(30, 30, 30, 235) : IM_COL32(24, 24, 24, 215), 4);
    float x = mn.x + pad;
    for (size_t i = 0; i < items.size(); i++) {
        if (i) { drawIcon(dl, Icon::Chevron, ImVec2(x + sep / 2, (mn.y + mx.y) / 2), 10 * u, IM_COL32(150, 150, 150, 255)); x += sep; }
        drawIcon(dl, items[i].icon, ImVec2(x + isz / 2, (mn.y + mx.y) / 2), isz, items[i].color);
        x += isz + 4 * u;
        dl->AddText(ImVec2(x, (mn.y + mx.y) / 2 - ImGui::GetTextLineHeight() / 2), IM_COL32(235, 235, 235, 255), items[i].text.c_str());
        x += ImGui::CalcTextSize(items[i].text.c_str()).x;
    }
}

// ---- toolbar ----

void Editor::viewportToolbar() {
    const float u = uiScale;
    ImGuiStyle& st = ImGui::GetStyle();
    const float rowH = 24 * u;
    ImVec2 size(710 * u, 4 * rowH + 3 * st.ItemSpacing.y + 2 * st.WindowPadding.y);
    if (size.x > vpSize.x - 8) size.x = vpSize.x - 8;
    // As in Spine: the bar sits at the bottom of the viewport. Unlocked, it slides left and right only,
    // and it is clamped so it never leaves the canvas.
    float centreX = vpOrigin.x + (vpSize.x - size.x) / 2;
    float halfRoom = std::max(0.0f, (vpSize.x - size.x) / 2);
    toolbarOffset = clampf(toolbarOffset, -halfRoom, halfRoom);
    ImVec2 pos(centreX + toolbarOffset, vpOrigin.y + vpSize.y - size.y - 8);
    toolbarTop = pos.y;
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.188f, 0.192f, 0.204f, 0.94f));
    ImGui::Begin("##toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove);
    ImDrawList* cdl = ImGui::GetWindowDrawList();
    auto toolBtn = [&](const char* id, Icon ic, const char* label, Tool t, float w, bool enabled, const char* tip) {
        ImGui::BeginDisabled(!enabled);
        if (iconButton(id, ic, label, doc.tool == t, ImVec2(w, rowH), tip)) doc.tool = t;
        ImGui::EndDisabled();
    };
    // Tools column.
    ImGui::BeginGroup();
    toolBtn("##pose", Icon::Pose, "Pose", Tool::Pose, 92 * u, true, "Pose (B): pose the skeleton by dragging bones");
    toolBtn("##weights", Icon::Weights, "Weights", Tool::Weights, 92 * u, true, "Weights (G): paint mesh weights");
    toolBtn("##create", Icon::Create, "Create", Tool::Create, 92 * u, doc.mode == Mode::Setup, "Create (N): drag to create bones");
    if (iconOnlyButton("##tbLock", Icon::Lock, !toolbarLocked, rowH,
                       toolbarLocked ? "Unlock the toolbar to drag it left or right" : "Lock the toolbar in place"))
        toolbarLocked = !toolbarLocked;
    ImGui::EndGroup();
    ImGui::SameLine(0, 10 * u);

    // Transform tools with numeric fields for the primary selected bone (or attachment, in Setup mode).
    Sel p = doc.primary();
    int bone = p.type == SelType::Bone ? p.index : -1;
    Attachment* att = bone < 0 ? selectedAttachment() : nullptr;
    Document::LocalPose pose{};
    if (bone >= 0) pose = doc.bonePose(bone);
    const float fw = 62 * u, tw = 100 * u;
    auto keyButton = [&](const char* id, TimelineType t, int ch) {
        ImGui::SameLine(0, 4 * u);
        if (doc.mode != Mode::Animate || bone < 0) { ImGui::Dummy(ImVec2(20 * u, rowH)); return; }
        bool keyed = doc.hasKey(bone, t);
        bool pend = doc.pending.count(bone) && (doc.pending[bone].channels & ch);
        ImVec4 bg = pend ? ImVec4(0.95f, 0.55f, 0.15f, 1) : keyed ? ImVec4(0.85f, 0.25f, 0.25f, 1) : ImVec4(0.3f, 0.3f, 0.3f, 1);
        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        if (ImGui::Button(id, ImVec2(20 * u, rowH))) { doc.checkpoint("Key"); doc.keyBone(bone, ch); }
        ImGui::PopStyleColor();
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax(), cc((mn.x + mx.x) / 2, (mn.y + mx.y) / 2);
        ImVec2 dia[4] = {ImVec2(cc.x, cc.y - 5 * u), ImVec2(cc.x + 5 * u, cc.y), ImVec2(cc.x, cc.y + 5 * u), ImVec2(cc.x - 5 * u, cc.y)};
        cdl->AddConvexPolyFilled(dia, 4, IM_COL32(245, 245, 245, 255));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(keyed ? "Keyed at this frame (click to re-key)" : "Set key");
    };
    auto applyPose = [&](int ch) { if (bone >= 0) { doc.setBonePose(bone, pose, ch); doc.updatePose(); } };
    int emptyRow = 0;
    auto disabledFields = [&](int n) {
        float z[2] = {};
        ImGui::BeginDisabled();
        ImGui::PushID(emptyRow++); // each empty row needs its own ID
        // No selection: an empty, disabled bar with no numbers, as in Spine.
        if (n == 1) ImGui::DragFloat("##d", z, 0.1f, 0, 0, " "); else ImGui::DragFloat2("##d", z, 0.1f, 0, 0, " ");
        ImGui::PopID();
        ImGui::EndDisabled();
    };

    ImGui::BeginGroup();
    toolBtn("##tRot", Icon::Rotate, "Rotate", Tool::Rotate, tw, true, "Rotate (C)");
    ImGui::SameLine(0, 4 * u);
    ImGui::SetNextItemWidth(fw * 2 + st.ItemSpacing.x);
    if (bone >= 0) { if (dragFloatEdit(doc, "##rot", &pose.rotation, 0.5f, "%.2f", "Rotate")) applyPose((int)Channel::Rotate); }
    else if (att) dragFloatEdit(doc, "##rot", &att->rotation, 0.5f, "%.2f", "Rotate");
    else disabledFields(1);
    keyButton("##kr", TimelineType::Rotate, (int)Channel::Rotate);

    toolBtn("##tTr", Icon::Translate, "Translate", Tool::Translate, tw, true, "Translate (V)");
    ImGui::SameLine(0, 4 * u);
    ImGui::SetNextItemWidth(fw * 2 + st.ItemSpacing.x);
    if (bone >= 0) {
        float v[2] = {pose.x, pose.y};
        if (doc.axes == Axes::World) { const Bone& b = doc.sk.bones[bone]; v[0] = b.worldX; v[1] = b.worldY; }
        if (dragFloat2Edit(doc, "##tr", v, 0.5f, "%.2f", "Translate")) {
            if (doc.axes == Axes::World) { Vec2 l = doc.sk.bones[bone].worldToParent(doc.sk, {v[0], v[1]}); pose.x = l.x; pose.y = l.y; }
            else { pose.x = v[0]; pose.y = v[1]; }
            applyPose((int)Channel::Translate);
        }
    } else if (att) { float v[2] = {att->x, att->y}; if (dragFloat2Edit(doc, "##tr", v, 0.5f, "%.2f", "Move attachment")) { att->x = v[0]; att->y = v[1]; } }
    else disabledFields(2);
    keyButton("##kt", TimelineType::Translate, (int)Channel::Translate);

    toolBtn("##tSc", Icon::Scale, "Scale", Tool::Scale, tw, true, "Scale (X)");
    ImGui::SameLine(0, 4 * u);
    ImGui::SetNextItemWidth(fw * 2 + st.ItemSpacing.x);
    if (bone >= 0) { float v[2] = {pose.sx, pose.sy}; if (dragFloat2Edit(doc, "##sc", v, 0.01f, "%.3f", "Scale")) { pose.sx = v[0]; pose.sy = v[1]; applyPose((int)Channel::Scale); } }
    else if (att) { float v[2] = {att->scaleX, att->scaleY}; if (dragFloat2Edit(doc, "##sc", v, 0.01f, "%.3f", "Scale attachment")) { att->scaleX = v[0]; att->scaleY = v[1]; } }
    else disabledFields(2);
    keyButton("##ks", TimelineType::Scale, (int)Channel::Scale);

    toolBtn("##tSh", Icon::Shear, "Shear", Tool::Shear, tw, true, "Shear (Z)");
    ImGui::SameLine(0, 4 * u);
    ImGui::SetNextItemWidth(fw * 2 + st.ItemSpacing.x);
    if (bone >= 0) { float v[2] = {pose.shx, pose.shy}; if (dragFloat2Edit(doc, "##sh", v, 0.5f, "%.2f", "Shear")) { pose.shx = v[0]; pose.shy = v[1]; applyPose((int)Channel::Shear); } }
    else disabledFields(2);
    keyButton("##kh", TimelineType::Shear, (int)Channel::Shear);
    ImGui::EndGroup();
    ImGui::SameLine(0, 10 * u);

    // Axes (and Auto Key in Animate mode).
    ImGui::BeginGroup();
    auto axisBtn = [&](const char* id, Icon ic, const char* label, Axes a, const char* tip) {
        if (iconButton(id, ic, label, doc.axes == a, ImVec2(82 * u, rowH), tip)) doc.axes = a;
    };
    axisBtn("##aL", Icon::AxisLocal, "Local", Axes::Local, "Local axes: gizmo follows the bone");
    axisBtn("##aP", Icon::AxisParent, "Parent", Axes::Parent, "Parent axes: translate along the parent's axes");
    axisBtn("##aW", Icon::AxisWorld, "World", Axes::World, "World axes: translate along the screen axes");
    if (doc.mode == Mode::Animate) {
        ImGui::PushStyleColor(ImGuiCol_Button, doc.autoKey ? ImVec4(0.75f, 0.22f, 0.22f, 1) : ImVec4(0.25f, 0.25f, 0.25f, 1));
        if (ImGui::Button("Auto Key", ImVec2(82 * u, rowH))) doc.autoKey = !doc.autoKey;
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Auto Key (Ctrl+Shift+A): record changes as keys");
    } else ImGui::Dummy(ImVec2(82 * u, rowH));
    ImGui::EndGroup();
    ImGui::SameLine(0, 10 * u);

    // Visibility grid: select / visible / names for bones, images and others.
    ImGui::BeginGroup();
    const float labelW = 52 * u, cellW = 24 * u;
    auto gridLabel = [&](const char* text) {
        ImGui::InvisibleButton(text, ImVec2(labelW, rowH));
        ImVec2 mn = ImGui::GetItemRectMin();
        cdl->AddText(ImVec2(mn.x, mn.y + rowH / 2 - ImGui::GetTextLineHeight() / 2), IM_COL32(170, 170, 170, 255), text);
    };
    auto cell = [&](const char* id, bool& flag, const char* tip) {
        ImGui::SameLine(0, 2 * u);
        ImGui::InvisibleButton(id, ImVec2(cellW, rowH));
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax(), cc((mn.x + mx.x) / 2, (mn.y + mx.y) / 2);
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) flag = !flag;
        if (hovered) cdl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, 20), 3);
        cdl->AddCircleFilled(cc, 3.5f * u, flag ? IM_COL32(235, 235, 235, 255) : IM_COL32(85, 85, 85, 255));
        if (hovered) ImGui::SetTooltip("%s: %s", tip, flag ? "on" : "off");
    };
    ImGui::Dummy(ImVec2(labelW, rowH));
    for (Icon ic : {Icon::Search, Icon::Eye, Icon::Key}) {
        ImGui::SameLine(0, 2 * u);
        ImGui::Dummy(ImVec2(cellW, rowH));
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax(), cc((mn.x + mx.x) / 2, (mn.y + mx.y) / 2);
        if (ic == Icon::Search) { // selectable: cursor arrow
            ImVec2 tri[3] = {ImVec2(cc.x - 4 * u, cc.y - 5 * u), ImVec2(cc.x - 4 * u, cc.y + 5 * u), ImVec2(cc.x + 4 * u, cc.y + 2 * u)};
            cdl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(200, 200, 200, 255));
        } else if (ic == Icon::Eye) drawIcon(cdl, Icon::Eye, cc, 14 * u, IM_COL32(200, 200, 200, 255));
        else { // tag icon
            ImVec2 d[4] = {ImVec2(cc.x - 5 * u, cc.y), ImVec2(cc.x, cc.y - 5 * u), ImVec2(cc.x + 5 * u, cc.y), ImVec2(cc.x, cc.y + 5 * u)};
            cdl->AddConvexPolyFilled(d, 4, IM_COL32(200, 200, 200, 255));
            cdl->AddCircleFilled(ImVec2(cc.x - 2 * u, cc.y), 1.2f * u, IM_COL32(40, 40, 40, 255));
        }
    }
    gridLabel("Bones");
    cell("##bs", selectBones, "Select bones"); cell("##bv", showBones, "Show bones"); cell("##bn", showBoneNames, "Bone names");
    gridLabel("Images");
    cell("##is", selectImages, "Select images"); cell("##iv", showImages, "Show images"); cell("##in", showImageNames, "Image names");
    gridLabel("Others");
    cell("##os", selectOthers, "Select others"); cell("##ov", showOthers, "Show others"); cell("##on", showOtherNames, "Other names");
    ImGui::EndGroup();
    // Unlocked: drag the bar's background to slide it horizontally.
    static bool slidingBar = false;
    if (!toolbarLocked) {
        bool overBackground = ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered();
        if (overBackground) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (overBackground && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) slidingBar = true;
    }
    if (slidingBar) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !toolbarLocked) {
            toolbarOffset += ImGui::GetIO().MouseDelta.x;
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        } else slidingBar = false;
    }
    ImGui::End();
    ImGui::PopStyleColor();

    // Zoom controls, bottom-left (fit and 100%), with the zoom readout.
    ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + 8, vpOrigin.y + vpSize.y - 78 * u));
    if (iconOnlyButton("##fit", Icon::Fit, false, 24 * u, "Zoom to fit (Ctrl+Shift+F)")) zoomToFit();
    ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + 8, vpOrigin.y + vpSize.y - 50 * u));
    if (iconOnlyButton("##z100", Icon::Zoom100, false, 24 * u, "Zoom to 100% (Ctrl+F)")) zoom100();
    ImGui::SetCursorScreenPos(ImVec2(vpOrigin.x + 38 * u, vpOrigin.y + vpSize.y - 46 * u));
    ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1), "%.0f%%", zoom * 100);
}

} // namespace osteon
