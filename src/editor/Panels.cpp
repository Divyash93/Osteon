// Tree, Properties and the smaller views.
#include "App.h"
#include "Geometry.h"
#include "imgui_internal.h"
#include <set>

namespace osteon {

static const ImVec4 kBoneCol(0.85f, 0.85f, 0.85f, 1), kSlotCol(0.55f, 0.80f, 1, 1), kAttCol(0.95f, 0.80f, 0.45f, 1);
static const ImVec4 kConCol(1, 0.55f, 0.35f, 1), kDimCol(0.55f, 0.55f, 0.55f, 1);

static const char* attIcon(AttachmentType t) {
    switch (t) {
    case AttachmentType::Region: return "[img]";
    case AttachmentType::Mesh: return "[mesh]";
    case AttachmentType::LinkedMesh: return "[link]";
    case AttachmentType::BoundingBox: return "[bbox]";
    case AttachmentType::Path: return "[path]";
    case AttachmentType::Point: return "[point]";
    case AttachmentType::Clipping: return "[clip]";
    }
    return "";
}

static bool matches(const std::string& name, const char* filter) {
    if (!filter[0]) return true;
    std::string a = name, b = filter;
    std::transform(a.begin(), a.end(), a.begin(), ::tolower);
    std::transform(b.begin(), b.end(), b.begin(), ::tolower);
    return a.find(b) != std::string::npos;
}

// ---- Tree ----

void Editor::treeView() {
    ImGui::Begin("Tree", &showTree);
    static char filter[128] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "Search", filter, sizeof filter);
    ImGui::Separator();
    ImGui::BeginChild("##tree");
    SkeletonData& d = doc.data;
    bool add = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
    const ImGuiTreeNodeFlags base = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    bool searching = filter[0] != 0;

    auto clickSelect = [&](const Sel& s) {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
            doc.select(s, add);
            if (s.type == SelType::Animation) { doc.animation = s.index; doc.setMode(Mode::Animate); }
            if (s.type == SelType::Skin) doc.skin = s.index;
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !doc.isSelected(s)) doc.select(s);
    };
    auto eye = [&](bool& visible, const char* id) {
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 22);
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, visible ? ImVec4(0.8f, 0.8f, 0.8f, 1) : ImVec4(0.4f, 0.4f, 0.4f, 1));
        if (ImGui::SmallButton(visible ? "o" : "-")) { doc.checkpoint("Visibility"); visible = !visible; }
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    };

    std::function<void(int)> boneNode;
    auto slotNode = [&](int si) {
        SlotData& s = d.slots[si];
        bool show = !searching || matches(s.name, filter);
        std::vector<std::pair<std::string, const Attachment*>> atts;
        std::set<std::string> seen;
        for (int sk : {doc.skin, 0}) {
            if (sk < 0 || sk >= (int)d.skins.size()) continue;
            auto it = d.skins[sk].attachments.find(si);
            if (it == d.skins[sk].attachments.end()) continue;
            for (auto& [n, a] : it->second) if (seen.insert(n).second) atts.push_back({n, &a});
        }
        for (auto& [n, a] : atts) show |= searching && matches(n, filter);
        if (!show) return;
        Sel ss{SelType::Slot, si, ""};
        ImGuiTreeNodeFlags fl = base | (doc.isSelected(ss) ? ImGuiTreeNodeFlags_Selected : 0) | (atts.empty() ? ImGuiTreeNodeFlags_Leaf : 0);
        if (searching) ImGui::SetNextItemOpen(true);
        ImGui::PushStyleColor(ImGuiCol_Text, kSlotCol);
        bool open = ImGui::TreeNodeEx((void*)(intptr_t)(0x100000 + si), fl, "%s", s.name.c_str());
        ImGui::PopStyleColor();
        clickSelect(ss);
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("OSTEON_SLOT", &si, sizeof si);
            ImGui::Text("Slot %s", s.name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("OSTEON_IMAGE")) {
                std::string img((const char*)p->Data, p->DataSize);
                doc.checkpoint("New attachment");
                const Bone& b = doc.sk.bones[s.bone];
                doc.addRegionAttachment(si, img, {b.worldX, b.worldY});
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename", "F2")) beginRename();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) doc.duplicateSelection();
            if (ImGui::MenuItem("Delete", "Del")) doc.deleteSelection();
            ImGui::Separator();
            if (ImGui::BeginMenu("New Attachment")) {
                if (ImGui::MenuItem("Bounding Box")) doc.addAttachment(si, AttachmentType::BoundingBox, "boundingbox");
                if (ImGui::MenuItem("Clipping")) doc.addAttachment(si, AttachmentType::Clipping, "clipping");
                if (ImGui::MenuItem("Path")) doc.addAttachment(si, AttachmentType::Path, "path");
                if (ImGui::MenuItem("Point")) doc.addAttachment(si, AttachmentType::Point, "point");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Add Image", !doc.imageNames.empty())) {
                for (auto& img : doc.imageNames)
                    if (ImGui::MenuItem(img.c_str())) {
                        doc.checkpoint("New attachment");
                        const Bone& b = doc.sk.bones[s.bone];
                        doc.addRegionAttachment(si, img, {b.worldX, b.worldY});
                    }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
        eye(s.visible, ("vs" + std::to_string(si)).c_str());
        if (!open) return;
        for (auto& [n, a] : atts) {
            Sel as{SelType::Attachment, si, n};
            bool active = doc.sk.slots.size() > (size_t)si && doc.sk.slots[si].attachment == n;
            ImGuiTreeNodeFlags af = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                    (doc.isSelected(as) ? ImGuiTreeNodeFlags_Selected : 0);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? kAttCol : kDimCol);
            ImGui::TreeNodeEx((void*)(intptr_t)(std::hash<std::string>()(n) ^ (si * 7919)), af, "%s %s", attIcon(a->type), n.c_str());
            ImGui::PopStyleColor();
            clickSelect(as);
            // The dot toggles which attachment the slot shows (keyed in animate mode).
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 22);
            ImGui::PushID(("a" + n + std::to_string(si)).c_str());
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            if (ImGui::SmallButton(active ? "*" : ".")) {
                doc.checkpoint(doc.mode == Mode::Setup ? "Set attachment" : "Key attachment");
                doc.setSlotAttachment(si, active ? "" : n);
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
            if (ImGui::BeginPopupContextItem(("ctx" + n + std::to_string(si)).c_str())) {
                if (ImGui::MenuItem("Rename", "F2")) beginRename();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D")) doc.duplicateSelection();
                if (ImGui::MenuItem("Delete", "Del")) doc.deleteSelection();
                if (a->type == AttachmentType::Region && ImGui::MenuItem("Convert to Mesh")) doc.convertToMesh(si, n);
                ImGui::EndPopup();
            }
        }
        ImGui::TreePop();
    };

    boneNode = [&](int bi) {
        BoneData& b = d.bones[bi];
        std::vector<int> kids = d.childrenOf(bi);
        std::vector<int> slots;
        for (size_t s = 0; s < d.slots.size(); s++) if (d.slots[s].bone == bi) slots.push_back((int)s);
        Sel bs{SelType::Bone, bi, ""};
        ImGuiTreeNodeFlags fl = base | (doc.isSelected(bs) ? ImGuiTreeNodeFlags_Selected : 0) | (kids.empty() && slots.empty() ? ImGuiTreeNodeFlags_Leaf : 0);
        if (bi == 0 || searching) ImGui::SetNextItemOpen(true, searching ? ImGuiCond_Always : ImGuiCond_Once);
        bool showSelf = !searching || matches(b.name, filter);
        bool open = true;
        if (showSelf) {
            bool pend = doc.pending.count(bi) > 0;
            ImGui::PushStyleColor(ImGuiCol_Text, pend ? ImVec4(1, 0.6f, 0.2f, 1) : kBoneCol);
            open = ImGui::TreeNodeEx((void*)(intptr_t)(bi + 1), fl, "%s", b.name.c_str());
            ImGui::PopStyleColor();
            clickSelect(bs);
            if (bi > 0 && ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("OSTEON_BONE", &bi, sizeof bi);
                ImGui::Text("Bone %s", b.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("OSTEON_BONE")) doc.reparentBones({*(const int*)p->Data}, bi);
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("OSTEON_SLOT")) doc.reparentSlot(*(const int*)p->Data, bi);
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("OSTEON_IMAGE")) {
                    std::string img((const char*)p->Data, p->DataSize);
                    doc.checkpoint("New slot");
                    int si = doc.createSlot(bi, fileName(img));
                    doc.updatePose();
                    const Bone& bb = doc.sk.bones[bi];
                    doc.addRegionAttachment(si, img, {bb.worldX, bb.worldY});
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("New Bone")) {
                    const Bone& rb = doc.sk.bones[bi];
                    doc.createBone(bi, {rb.worldX, rb.worldY}, {rb.worldX + 50, rb.worldY});
                }
                if (ImGui::MenuItem("New Slot")) { doc.checkpoint("New slot"); doc.selection = {{SelType::Slot, doc.createSlot(bi, "slot"), ""}}; }
                if (ImGui::BeginMenu("New Constraint")) {
                    std::vector<int> sel = doc.selectedBones();
                    if (ImGui::MenuItem("IK (selected bones -> this target)", nullptr, false, !sel.empty() && sel.size() <= 2))
                        doc.createIk("ik", sel, bi);
                    if (ImGui::MenuItem("Transform (selected bones -> this target)", nullptr, false, !sel.empty()))
                        doc.createTransform("transform", sel, bi);
                    if (ImGui::MenuItem("Physics (this bone)")) doc.createPhysics("physics", bi);
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Rename", "F2", false, bi > 0)) beginRename();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, bi > 0)) doc.duplicateSelection();
                if (ImGui::MenuItem("Delete", "Del", false, bi > 0)) doc.deleteSelection();
                ImGui::EndPopup();
            }
            eye(b.visible, ("vb" + std::to_string(bi)).c_str());
        }
        if (open) {
            for (int s : slots) slotNode(s);
            for (int k : kids) boneNode(k);
            if (showSelf) ImGui::TreePop();
        }
    };

    std::string skelName = doc.projectPath.empty() ? "skeleton" : stripExtension(fileName(doc.projectPath));
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNodeEx("##skel", base | (doc.isSelected({SelType::Skeleton, 0, ""}) ? ImGuiTreeNodeFlags_Selected : 0), "%s", skelName.c_str())) {
        clickSelect({SelType::Skeleton, 0, ""});
        if (!d.bones.empty()) boneNode(0);

        if (ImGui::TreeNodeEx("Draw Order", base)) {
            for (int i = (int)d.slots.size() - 1; i >= 0; i--) { // front-most first, like Spine
                if (searching && !matches(d.slots[i].name, filter)) continue;
                Sel s{SelType::Slot, i, ""};
                ImGui::PushStyleColor(ImGuiCol_Text, kSlotCol);
                ImGui::Selectable((d.slots[i].name + "##do" + std::to_string(i)).c_str(), doc.isSelected(s));
                ImGui::PopStyleColor();
                if (ImGui::IsItemClicked()) doc.select(s, add);
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("OSTEON_DRAWORDER", &i, sizeof i);
                    ImGui::Text("%s", d.slots[i].name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("OSTEON_DRAWORDER")) {
                        doc.checkpoint("Draw order");
                        doc.data.moveSlot(*(const int*)p->Data, i);
                        doc.selection = {{SelType::Slot, i, ""}};
                    }
                    ImGui::EndDragDropTarget();
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Constraints", base)) {
            auto conRow = [&](SelType t, int i, const std::string& name, const char* kind) {
                Sel s{t, i, ""};
                ImGui::PushStyleColor(ImGuiCol_Text, kConCol);
                ImGui::TreeNodeEx((void*)(intptr_t)(0x200000 + (int)t * 1000 + i), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                  ImGuiTreeNodeFlags_SpanAvailWidth | (doc.isSelected(s) ? ImGuiTreeNodeFlags_Selected : 0), "%s %s", kind, name.c_str());
                ImGui::PopStyleColor();
                clickSelect(s);
            };
            for (size_t i = 0; i < d.ik.size(); i++) conRow(SelType::Ik, (int)i, d.ik[i].name, "[ik]");
            for (size_t i = 0; i < d.transforms.size(); i++) conRow(SelType::Transform, (int)i, d.transforms[i].name, "[transform]");
            for (size_t i = 0; i < d.paths.size(); i++) conRow(SelType::Path, (int)i, d.paths[i].name, "[path]");
            for (size_t i = 0; i < d.physics.size(); i++) conRow(SelType::Physics, (int)i, d.physics[i].name, "[physics]");
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Skins", base)) {
            for (size_t i = 0; i < d.skins.size(); i++) {
                Sel s{SelType::Skin, (int)i, ""};
                std::string label = d.skins[i].name + ((int)i == doc.skin ? "  (active)" : "");
                ImGui::TreeNodeEx((void*)(intptr_t)(0x300000 + i), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                  (doc.isSelected(s) ? ImGuiTreeNodeFlags_Selected : 0), "%s", label.c_str());
                clickSelect(s);
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Events", base)) {
            for (size_t i = 0; i < d.events.size(); i++) {
                Sel s{SelType::Event, (int)i, ""};
                ImGui::TreeNodeEx((void*)(intptr_t)(0x400000 + i), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                  (doc.isSelected(s) ? ImGuiTreeNodeFlags_Selected : 0), "%s", d.events[i].name.c_str());
                clickSelect(s);
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Animations", base)) {
            for (size_t i = 0; i < d.animations.size(); i++) {
                Sel s{SelType::Animation, (int)i, ""};
                ImGui::PushStyleColor(ImGuiCol_Text, (int)i == doc.animation && doc.mode == Mode::Animate ? ImVec4(0.4f, 0.75f, 1, 1) : kBoneCol);
                ImGui::TreeNodeEx((void*)(intptr_t)(0x500000 + i), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                  (doc.isSelected(s) ? ImGuiTreeNodeFlags_Selected : 0), "%s", d.animations[i].name.c_str());
                ImGui::PopStyleColor();
                clickSelect(s);
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Images", base)) {
            if (doc.imagesDir.empty()) ImGui::TextDisabled("No images folder. File > Set Images Folder...");
            else ImGui::TextDisabled("%s", doc.imagesDir.c_str());
            if (ImGui::SmallButton("Refresh")) { doc.scanImages(); gfx.reloadImages(); }
            for (auto& img : doc.imageNames) {
                if (searching && !matches(img, filter)) continue;
                ImGui::Selectable(img.c_str(), doc.isSelected({SelType::Image, 0, img}));
                if (ImGui::IsItemClicked()) doc.select({SelType::Image, 0, img});
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("OSTEON_IMAGE", img.data(), img.size());
                    ImGui::Text("%s (drop on viewport, bone or slot)", img.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::IsItemHovered()) {
                    if (Image* im = gfx.image(doc.imageFileFor(img))) {
                        ImGui::BeginTooltip();
                        float s = std::min(1.0f, 160.0f / std::max(im->w, im->h));
                        ImGui::Image((ImTextureID)im->srv, ImVec2(im->w * s, im->h * s));
                        ImGui::Text("%d x %d", im->w, im->h);
                        ImGui::EndTooltip();
                    }
                }
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    if (ImGui::BeginPopupContextWindow("##treebg", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("New Animation")) doc.createAnimation("animation");
        if (ImGui::MenuItem("New Skin")) doc.createSkin("skin");
        if (ImGui::MenuItem("New Event")) doc.createEvent("event");
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::End();
}

// Sequence (frame-by-frame) settings shared by region and mesh attachments. Frames are images named
// path + zero-padded number (e.g. "walk01".."walk08"), as in Spine.
static void sequenceEditor(Editor& ed, int si, Attachment* a) {
    Document& doc = ed.doc;
    bool on = a->seqCount > 0;
    if (ImGui::Checkbox("Sequence", &on)) {
        doc.checkpoint("Sequence");
        if (!on) a->seqCount = 0;
        else {
            // Detect frames: longest run of numbered images starting with the path.
            std::string base = a->imagePath();
            while (!base.empty() && isdigit((unsigned char)base.back())) base.pop_back();
            int best = 0, digits = 2, start = 1;
            for (int d = 1; d <= 4; d++)
                for (int st : {0, 1}) {
                    int n = 0;
                    char buf[32];
                    for (;; n++) {
                        std::snprintf(buf, sizeof buf, "%0*d", d, st + n);
                        if (doc.imageFileFor(base + buf).empty()) break;
                    }
                    if (n > best) { best = n; digits = d; start = st; }
                }
            a->path = base;
            a->seqCount = std::max(1, best);
            a->seqDigits = digits;
            a->seqStart = start;
            a->seqSetup = 0;
            ed.setStatus(best ? "Sequence: " + std::to_string(best) + " frames found" : "No numbered frames found for '" + base + "'");
        }
    }
    if (a->seqCount <= 0) return;
    ImGui::BeginDisabled(doc.mode != Mode::Setup);
    int v[4] = {a->seqCount, a->seqStart, a->seqDigits, a->seqSetup};
    if (ImGui::DragInt4("Count/Start/Digits/Setup", v, 0.1f, 0, 9999)) {
        a->seqCount = std::max(1, v[0]); a->seqStart = std::max(0, v[1]); a->seqDigits = std::max(0, v[2]);
        a->seqSetup = std::max(0, std::min(a->seqCount - 1, v[3]));
        doc.dirty = true;
    }
    if (ImGui::IsItemActivated()) doc.beginEdit("Sequence");
    if (ImGui::IsItemDeactivatedAfterEdit()) doc.commitEdit();
    ImGui::EndDisabled();
    if (doc.mode == Mode::Animate && doc.currentAnimation()) {
        static int mode = 2;
        static float fps = 12;
        const char* modes[] = {"hold", "once", "loop", "pingpong", "onceReverse", "loopReverse", "pingpongReverse"};
        ImGui::SetNextItemWidth(120);
        ImGui::Combo("Mode", &mode, modes, 7);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        ImGui::DragFloat("FPS", &fps, 0.5f, 1, 120, "%.0f");
        if (ImGui::Button("Key sequence")) {
            doc.checkpoint("Key sequence");
            int skin = doc.data.skins[doc.skin].get(si, a->name) ? doc.skin : 0;
            Key& k = doc.currentAnimation()->get(TimelineType::Sequence, si, doc.data.skins[skin].name, a->name)
                         .setKey(std::round(doc.time * doc.data.fps) / doc.data.fps);
            k.str = modes[mode];
            k.intValue = std::max(0, doc.sk.slots[si].sequenceIndex);
            k.v[0] = 1 / fps;
        }
    }
}

// ---- Properties ----

static void inheritEditor(Document& doc, BoneData& b) {
    // Spine exposes three toggles; map them to the five inherit modes.
    bool rot = b.inherit != Inherit::OnlyTranslation && b.inherit != Inherit::NoRotationOrReflection;
    bool scale = b.inherit == Inherit::Normal || b.inherit == Inherit::NoRotationOrReflection;
    bool refl = b.inherit == Inherit::Normal || b.inherit == Inherit::NoScale;
    bool r2 = rot, s2 = scale, f2 = refl;
    ImGui::Checkbox("Rotation", &r2); ImGui::SameLine();
    ImGui::Checkbox("Scale", &s2); ImGui::SameLine();
    ImGui::Checkbox("Reflection", &f2);
    if (r2 != rot || s2 != scale || f2 != refl) {
        doc.checkpoint("Inherit");
        if (!r2) b.inherit = s2 ? Inherit::NoRotationOrReflection : Inherit::OnlyTranslation;
        else if (s2) b.inherit = Inherit::Normal;
        else b.inherit = f2 ? Inherit::NoScale : Inherit::NoScaleOrReflection;
    }
    ImGui::TextDisabled("Mode: %s", inheritName(b.inherit));
}

static bool boneCombo(Document& doc, const char* label, int* bone, bool allowNone = false) {
    const auto& bones = doc.data.bones;
    const char* preview = *bone >= 0 && *bone < (int)bones.size() ? bones[*bone].name.c_str() : "(none)";
    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
        if (allowNone && ImGui::Selectable("(none)", *bone < 0)) { *bone = -1; changed = true; }
        for (size_t i = 0; i < bones.size(); i++)
            if (ImGui::Selectable(bones[i].name.c_str(), (int)i == *bone)) { *bone = (int)i; changed = true; }
        ImGui::EndCombo();
    }
    return changed;
}

void Editor::propertiesView() {
    ImGui::Begin("Properties", &showProps);
    SkeletonData& d = doc.data;
    Sel p = doc.primary();
    ImGui::PushItemWidth(-90 * uiScale);
    auto header = [&](const char* kind, const std::string& name) {
        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1, 1), "%s:", kind);
        ImGui::SameLine();
        ImGui::TextUnformatted(name.c_str());
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 70 * uiScale);
        if (ImGui::SmallButton("Rename")) beginRename();
        ImGui::Separator();
    };
    switch (p.type) {
    case SelType::None:
    case SelType::Skeleton: {
        header("Skeleton", doc.projectPath.empty() ? "untitled" : fileName(doc.projectPath));
        dragFloatEdit(doc, "FPS", &d.fps, 0.1f, "%.1f", "FPS");
        if (d.fps < 1) d.fps = 1;
        dragFloatEdit(doc, "Ref. scale", &d.referenceScale, 1, "%.0f", "Reference scale");
        ImGui::TextWrapped("Images: %s", doc.imagesDir.empty() ? "(not set)" : doc.imagesDir.c_str());
        if (ImGui::Button("Set images folder...")) cmdImportImagesFolder();
        ImGui::SameLine();
        if (ImGui::Button("Open folder") && !doc.imagesDir.empty()) openInExplorer(doc.imagesDir);
        ImGui::Separator();
        ImGui::TextDisabled("%zu bones, %zu slots, %zu skins, %zu animations", d.bones.size(), d.slots.size(), d.skins.size(), d.animations.size());
        if (ImGui::Button("New animation")) doc.createAnimation("animation");
        ImGui::SameLine();
        if (ImGui::Button("New skin")) doc.createSkin("skin");
        ImGui::SameLine();
        if (ImGui::Button("New event")) doc.createEvent("event");
        break;
    }
    case SelType::Bone: {
        BoneData& b = d.bones[p.index];
        header("Bone", b.name);
        if (doc.mode == Mode::Setup) {
            dragFloatEdit(doc, "Length", &b.length, 0.5f, "%.2f", "Length");
            dragFloatEdit(doc, "Rotation", &b.rotation, 0.5f, "%.2f", "Rotate");
            float t[2] = {b.x, b.y};
            if (dragFloat2Edit(doc, "Translate", t, 0.5f, "%.2f", "Translate")) { b.x = t[0]; b.y = t[1]; }
            float s[2] = {b.scaleX, b.scaleY};
            if (dragFloat2Edit(doc, "Scale", s, 0.01f, "%.3f", "Scale")) { b.scaleX = s[0]; b.scaleY = s[1]; }
            float sh[2] = {b.shearX, b.shearY};
            if (dragFloat2Edit(doc, "Shear", sh, 0.5f, "%.2f", "Shear")) { b.shearX = sh[0]; b.shearY = sh[1]; }
        } else {
            Document::LocalPose lp = doc.bonePose(p.index);
            ImGui::TextDisabled("Animated pose at frame %d (edit with tools / toolbar)", (int)std::round(doc.time * d.fps));
            ImGui::Text("Rotation %.2f  Translate %.2f, %.2f", lp.rotation, lp.x, lp.y);
            ImGui::Text("Scale %.3f, %.3f  Shear %.2f, %.2f", lp.sx, lp.sy, lp.shx, lp.shy);
        }
        ImGui::Separator();
        ImGui::BeginDisabled(doc.mode != Mode::Setup);
        ImGui::TextUnformatted("Inherit");
        inheritEditor(doc, b);
        colorEdit(doc, "Color", b.color, true, "Bone color");
        checkboxEdit(doc, "Skin required", &b.skinRequired, "Skin required");
        int parent = b.parent;
        if (p.index > 0 && boneCombo(doc, "Parent", &parent) && parent >= 0) doc.reparentBones({p.index}, parent);
        ImGui::EndDisabled();
        ImGui::Separator();
        const Bone& rb = doc.sk.bones[p.index];
        ImGui::TextDisabled("World: x %.2f  y %.2f  rot %.2f  scale %.3f, %.3f", rb.worldX, rb.worldY, rb.worldRotationX(), rb.worldScaleX(), rb.worldScaleY());
        if (doc.mode == Mode::Setup) {
            if (ImGui::Button("New child bone")) doc.createBone(p.index, {rb.worldX, rb.worldY}, rb.localToWorld({d.bones[p.index].length + 50, 0}));
            ImGui::SameLine();
            if (ImGui::Button("New slot")) { doc.checkpoint("New slot"); doc.selection = {{SelType::Slot, doc.createSlot(p.index, b.name), ""}}; }
            ImGui::SameLine();
            if (ImGui::Button("Set parent (P)")) pickParent = true;
            std::vector<int> sel = doc.selectedBones();
            if (ImGui::Button("New IK...")) ImGui::OpenPopup("ikpick");
            ImGui::SameLine();
            if (ImGui::Button("New transform...")) ImGui::OpenPopup("tcpick");
            ImGui::SameLine();
            if (ImGui::Button("New physics")) doc.createPhysics("physics", p.index);
            for (int kind = 0; kind < 2; kind++) {
                if (ImGui::BeginPopup(kind == 0 ? "ikpick" : "tcpick")) {
                    ImGui::TextDisabled(kind == 0 ? "Constrain selected bone(s) to target:" : "Copy transform of target:");
                    for (size_t i = 0; i < d.bones.size(); i++) {
                        if (std::find(sel.begin(), sel.end(), (int)i) != sel.end()) continue;
                        if (ImGui::Selectable(d.bones[i].name.c_str())) {
                            if (kind == 0) { if (doc.createIk("ik", sel, (int)i) < 0) setStatus("Select 1 bone, or a parent and its child"); }
                            else doc.createTransform("transform", sel, (int)i);
                        }
                    }
                    ImGui::EndPopup();
                }
            }
        }
        break;
    }
    case SelType::Slot: {
        SlotData& s = d.slots[p.index];
        header("Slot", s.name);
        int bone = s.bone;
        ImGui::BeginDisabled(doc.mode != Mode::Setup);
        if (boneCombo(doc, "Bone", &bone)) doc.reparentSlot(p.index, bone);
        ImGui::EndDisabled();
        Color c = doc.mode == Mode::Setup ? s.color : doc.sk.slots[p.index].color;
        if (colorEdit(doc, "Color", c, true, doc.mode == Mode::Setup ? "Slot color" : "Key color")) doc.setSlotColor(p.index, c);
        if (doc.mode == Mode::Setup) {
            if (checkboxEdit(doc, "Tint black", &s.hasDark, "Tint black") && s.hasDark) s.dark = Color(0, 0, 0, 1);
            if (s.hasDark) colorEdit(doc, "Dark color", s.dark, false, "Dark color");
            const char* blends[] = {"Normal", "Additive", "Multiply", "Screen"};
            int bm = (int)s.blend;
            if (ImGui::Combo("Blending", &bm, blends, 4)) { doc.checkpoint("Blend mode"); s.blend = (BlendMode)bm; }
        }
        // Attachment chooser (keyed in animate mode).
        std::string cur = doc.sk.slots[p.index].attachment;
        if (ImGui::BeginCombo("Attachment", cur.empty() ? "(none)" : cur.c_str())) {
            if (ImGui::Selectable("(none)", cur.empty())) { doc.checkpoint("Attachment"); doc.setSlotAttachment(p.index, ""); }
            std::set<std::string> names;
            for (int sk : {doc.skin, 0})
                if (sk < (int)d.skins.size())
                    if (auto it = d.skins[sk].attachments.find(p.index); it != d.skins[sk].attachments.end())
                        for (auto& [n, a] : it->second) names.insert(n);
            for (auto& n : names)
                if (ImGui::Selectable(n.c_str(), n == cur)) { doc.checkpoint("Attachment"); doc.setSlotAttachment(p.index, n); }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Add image...")) ImGui::OpenPopup("addimg");
        if (ImGui::BeginPopup("addimg")) {
            for (auto& img : doc.imageNames)
                if (ImGui::Selectable(img.c_str())) {
                    doc.checkpoint("New attachment");
                    const Bone& b = doc.sk.bones[s.bone];
                    doc.addRegionAttachment(p.index, img, {b.worldX, b.worldY});
                }
            if (doc.imageNames.empty()) ImGui::TextDisabled("No images (set the images folder)");
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("New...")) ImGui::OpenPopup("newatt");
        if (ImGui::BeginPopup("newatt")) {
            if (ImGui::Selectable("Bounding box")) doc.addAttachment(p.index, AttachmentType::BoundingBox, "boundingbox");
            if (ImGui::Selectable("Clipping")) doc.addAttachment(p.index, AttachmentType::Clipping, "clipping");
            if (ImGui::Selectable("Path")) doc.addAttachment(p.index, AttachmentType::Path, "path");
            if (ImGui::Selectable("Point")) doc.addAttachment(p.index, AttachmentType::Point, "point");
            ImGui::EndPopup();
        }
        if (doc.mode == Mode::Animate) {
            ImGui::SameLine();
            if (ImGui::Button("Key color")) { doc.checkpoint("Key"); doc.keySlotColor(p.index); }
            ImGui::SameLine();
            if (ImGui::Button("Key attachment")) { doc.checkpoint("Key"); doc.keySlotAttachment(p.index); }
        }
        break;
    }
    case SelType::Attachment: {
        int si = p.index;
        Attachment* a = selectedAttachment(&si);
        if (!a) break;
        header(attachmentTypeName(a->type), a->name);
        bool setup = doc.mode == Mode::Setup;
        ImGui::BeginDisabled(!setup);
        switch (a->type) {
        case AttachmentType::Region: {
            const std::string& ip = a->imagePath();
            if (ImGui::BeginCombo("Image", ip.c_str())) {
                for (auto& img : doc.imageNames)
                    if (ImGui::Selectable(img.c_str(), img == ip)) {
                        doc.checkpoint("Image path");
                        a->path = img == a->name ? "" : img;
                        int w, h;
                        if (loadImageFileSize(doc.imageFileFor(img), w, h)) { a->width = (float)w; a->height = (float)h; }
                    }
                ImGui::EndCombo();
            }
            if (doc.imagePathFor(*a).empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Image not found in the images folder");
            float t[2] = {a->x, a->y};
            if (dragFloat2Edit(doc, "Translate", t, 0.5f, "%.2f", "Move attachment")) { a->x = t[0]; a->y = t[1]; }
            dragFloatEdit(doc, "Rotation", &a->rotation, 0.5f, "%.2f", "Rotate attachment");
            float s[2] = {a->scaleX, a->scaleY};
            if (dragFloat2Edit(doc, "Scale", s, 0.01f, "%.3f", "Scale attachment")) { a->scaleX = s[0]; a->scaleY = s[1]; }
            float wh[2] = {a->width, a->height};
            if (dragFloat2Edit(doc, "Size", wh, 1, "%.1f", "Size")) { a->width = wh[0]; a->height = wh[1]; }
            colorEdit(doc, "Color", a->color, true, "Attachment color");
            ImGui::EndDisabled();
            sequenceEditor(*this, si, a);
            ImGui::BeginDisabled(!setup);
            if (ImGui::Button("Convert to mesh")) doc.convertToMesh(si, a->name);
            ImGui::SameLine();
            if (ImGui::Button("Reset size")) {
                int w, h;
                if (loadImageFileSize(doc.imagePathFor(*a), w, h)) { doc.checkpoint("Reset size"); a->width = (float)w; a->height = (float)h; }
            }
            break;
        }
        case AttachmentType::Mesh:
        case AttachmentType::LinkedMesh:
            ImGui::Text("Image: %s", a->imagePath().c_str());
            ImGui::Text("Vertices: %d   Triangles: %zu   Hull: %d", a->vertexCount(), a->triangles.size() / 3, a->hullLength);
            ImGui::Text("Weighted: %s", a->isWeighted() ? "yes" : "no");
            colorEdit(doc, "Color", a->color, true, "Attachment color");
            ImGui::EndDisabled();
            if (a->type == AttachmentType::LinkedMesh) {
                ImGui::Text("Linked to: %s%s%s", a->parentMesh.c_str(), a->parentSkin.empty() ? "" : " in skin ", a->parentSkin.c_str());
                ImGui::BeginDisabled(!setup);
                checkboxEdit(doc, "Inherit deform timelines", &a->inheritTimelines, "Inherit timelines");
                if (ImGui::BeginCombo("Image", a->imagePath().c_str())) {
                    for (auto& img : doc.imageNames)
                        if (ImGui::Selectable(img.c_str(), img == a->imagePath())) { doc.checkpoint("Image path"); a->path = img; }
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
            } else if (setup && ImGui::Button("New linked mesh")) {
                doc.createLinkedMesh(si, a->name);
                ImGui::PopItemWidth();
                ImGui::End();
                return;
            }
            sequenceEditor(*this, si, a);
            if (a->type == AttachmentType::Mesh && ImGui::Button("Edit mesh (Mesh Tools)")) { showMeshTools = true; meshMode = MeshMode::Modify; }
            ImGui::SameLine();
            if (ImGui::Button("Weights...")) { showWeights = true; doc.tool = Tool::Weights; }
            ImGui::BeginDisabled(!setup);
            break;
        case AttachmentType::BoundingBox:
        case AttachmentType::Clipping:
            ImGui::Text("Vertices: %d", a->vertexCount());
            colorEdit(doc, "Color", a->color, true, "Attachment color");
            if (a->type == AttachmentType::Clipping) {
                if (ImGui::BeginCombo("End slot", a->endSlot.empty() ? "(end of draw order)" : a->endSlot.c_str())) {
                    if (ImGui::Selectable("(end of draw order)")) { doc.checkpoint("End slot"); a->endSlot.clear(); }
                    for (auto& s : d.slots) if (ImGui::Selectable(s.name.c_str())) { doc.checkpoint("End slot"); a->endSlot = s.name; }
                    ImGui::EndCombo();
                }
            }
            ImGui::TextDisabled("Edit the polygon with Mesh Tools > Modify.");
            if (ImGui::Button("Edit vertices")) { showMeshTools = true; meshMode = MeshMode::Modify; }
            break;
        case AttachmentType::Path:
            checkboxEdit(doc, "Closed", &a->closed, "Path closed");
            checkboxEdit(doc, "Constant speed", &a->constantSpeed, "Constant speed");
            ImGui::Text("Vertices: %d", a->vertexCount());
            if (ImGui::Button("Edit vertices")) { showMeshTools = true; meshMode = MeshMode::Modify; }
            break;
        case AttachmentType::Point: {
            float t[2] = {a->x, a->y};
            if (dragFloat2Edit(doc, "Translate", t, 0.5f, "%.2f", "Move point")) { a->x = t[0]; a->y = t[1]; }
            dragFloatEdit(doc, "Rotation", &a->rotation, 0.5f, "%.2f", "Rotate point");
            break;
        }
        }
        ImGui::EndDisabled();
        break;
    }
    case SelType::Ik: {
        IkConstraintData& c = d.ik[p.index];
        header("IK constraint", c.name);
        std::string bones;
        for (int b : c.bones) bones += d.bones[b].name + " ";
        ImGui::Text("Bones: %s", bones.c_str());
        int t = c.target;
        if (boneCombo(doc, "Target", &t) && t >= 0) { doc.checkpoint("IK target"); c.target = t; }
        if (doc.mode == Mode::Setup) {
            ImGui::SliderFloat("Mix", &c.mix, 0, 1);
            if (ImGui::IsItemActivated()) doc.beginEdit("IK mix");
            if (ImGui::IsItemDeactivatedAfterEdit()) doc.commitEdit();
            dragFloatEdit(doc, "Softness", &c.softness, 0.5f, "%.1f", "IK softness");
            bool bend = c.bendDirection > 0;
            if (checkboxEdit(doc, "Bend positive", &bend, "Bend direction")) c.bendDirection = bend ? 1 : -1;
            checkboxEdit(doc, "Compress", &c.compress, "IK compress");
            checkboxEdit(doc, "Stretch", &c.stretch, "IK stretch");
            checkboxEdit(doc, "Uniform", &c.uniform, "IK uniform");
        } else {
            IkConstraint& rc = doc.sk.ik[p.index];
            ImGui::Text("Mix %.2f  Softness %.1f  Bend %s", rc.mix, rc.softness, rc.bendDirection > 0 ? "+" : "-");
            if (ImGui::Button("Key mix/bend") && doc.currentAnimation()) {
                doc.checkpoint("Key IK");
                Key& k = doc.currentAnimation()->get(TimelineType::Ik, p.index).setKey(std::round(doc.time * d.fps) / d.fps);
                k.v[0] = rc.mix; k.v[1] = rc.softness; k.flag1 = rc.bendDirection > 0; k.flag2 = rc.compress; k.flag3 = rc.stretch;
            }
        }
        if (ImGui::InputInt("Order", &c.order)) doc.dirty = true;
        checkboxEdit(doc, "Skin required", &c.skinRequired, "Skin required");
        break;
    }
    case SelType::Transform: {
        TransformConstraintData& c = d.transforms[p.index];
        header("Transform constraint", c.name);
        int t = c.target;
        if (boneCombo(doc, "Target", &t) && t >= 0) { doc.checkpoint("Target"); c.target = t; }
        ImGui::TextUnformatted("Mix");
        float m1[2] = {c.mixRotate, c.mixShearY};
        if (dragFloat2Edit(doc, "Rotate / ShearY", m1, 0.01f, "%.2f", "Mix")) { c.mixRotate = m1[0]; c.mixShearY = m1[1]; }
        float m2[2] = {c.mixX, c.mixY};
        if (dragFloat2Edit(doc, "Translate X/Y", m2, 0.01f, "%.2f", "Mix")) { c.mixX = m2[0]; c.mixY = m2[1]; }
        float m3[2] = {c.mixScaleX, c.mixScaleY};
        if (dragFloat2Edit(doc, "Scale X/Y", m3, 0.01f, "%.2f", "Mix")) { c.mixScaleX = m3[0]; c.mixScaleY = m3[1]; }
        ImGui::TextUnformatted("Offsets");
        dragFloatEdit(doc, "Rotation", &c.offsetRotation, 0.5f, "%.2f", "Offset");
        float o[2] = {c.offsetX, c.offsetY};
        if (dragFloat2Edit(doc, "Translate", o, 0.5f, "%.2f", "Offset")) { c.offsetX = o[0]; c.offsetY = o[1]; }
        float os[2] = {c.offsetScaleX, c.offsetScaleY};
        if (dragFloat2Edit(doc, "Scale", os, 0.01f, "%.3f", "Offset")) { c.offsetScaleX = os[0]; c.offsetScaleY = os[1]; }
        dragFloatEdit(doc, "Shear Y", &c.offsetShearY, 0.5f, "%.2f", "Offset");
        checkboxEdit(doc, "Local", &c.local, "Local");
        ImGui::SameLine();
        checkboxEdit(doc, "Relative", &c.relative, "Relative");
        if (ImGui::InputInt("Order", &c.order)) doc.dirty = true;
        break;
    }
    case SelType::Path: {
        PathConstraintData& c = d.paths[p.index];
        header("Path constraint", c.name);
        ImGui::Text("Target slot: %s", d.slots[c.target].name.c_str());
        const char* pm[] = {"Fixed", "Percent"};
        const char* sm[] = {"Length", "Fixed", "Percent", "Proportional"};
        const char* rm[] = {"Tangent", "Chain", "Chain scale"};
        int a = (int)c.positionMode, b = (int)c.spacingMode, r = (int)c.rotateMode;
        if (ImGui::Combo("Position mode", &a, pm, 2)) { doc.checkpoint("Path"); c.positionMode = (PathConstraintData::Position)a; }
        if (ImGui::Combo("Spacing mode", &b, sm, 4)) { doc.checkpoint("Path"); c.spacingMode = (PathConstraintData::Spacing)b; }
        if (ImGui::Combo("Rotate mode", &r, rm, 3)) { doc.checkpoint("Path"); c.rotateMode = (PathConstraintData::Rotate)r; }
        dragFloatEdit(doc, "Position", &c.position, 0.01f, "%.3f", "Path position");
        dragFloatEdit(doc, "Spacing", &c.spacing, 0.1f, "%.2f", "Path spacing");
        dragFloatEdit(doc, "Rotation offset", &c.offsetRotation, 0.5f, "%.1f", "Path rotation");
        float m[3] = {c.mixRotate, c.mixX, c.mixY};
        if (ImGui::DragFloat3("Mix R/X/Y", m, 0.01f)) { c.mixRotate = m[0]; c.mixX = m[1]; c.mixY = m[2]; doc.dirty = true; }
        if (ImGui::IsItemActivated()) doc.beginEdit("Path mix");
        if (ImGui::IsItemDeactivatedAfterEdit()) doc.commitEdit();
        break;
    }
    case SelType::Physics: {
        PhysicsConstraintData& c = d.physics[p.index];
        header("Physics constraint", c.name);
        int b = c.bone;
        if (boneCombo(doc, "Bone", &b) && b >= 0) { doc.checkpoint("Physics bone"); c.bone = b; }
        ImGui::TextUnformatted("Affects (0 = off)");
        float t[2] = {c.x, c.y};
        if (dragFloat2Edit(doc, "Translate X/Y", t, 0.01f, "%.2f", "Physics")) { c.x = t[0]; c.y = t[1]; }
        dragFloatEdit(doc, "Rotate", &c.rotate, 0.01f, "%.2f", "Physics");
        dragFloatEdit(doc, "Scale X", &c.scaleX, 0.01f, "%.2f", "Physics");
        dragFloatEdit(doc, "Shear X", &c.shearX, 0.01f, "%.2f", "Physics");
        ImGui::Separator();
        dragFloatEdit(doc, "Inertia", &c.inertia, 0.01f, "%.2f", "Physics");
        dragFloatEdit(doc, "Strength", &c.strength, 1, "%.1f", "Physics");
        dragFloatEdit(doc, "Damping", &c.damping, 0.01f, "%.2f", "Physics");
        float mass = c.massInverse > 0 ? 1 / c.massInverse : 1;
        if (dragFloatEdit(doc, "Mass", &mass, 0.01f, "%.2f", "Physics") && mass > 0.001f) c.massInverse = 1 / mass;
        dragFloatEdit(doc, "Wind", &c.wind, 0.5f, "%.1f", "Physics");
        dragFloatEdit(doc, "Gravity", &c.gravity, 0.5f, "%.1f", "Physics");
        dragFloatEdit(doc, "Mix", &c.mix, 0.01f, "%.2f", "Physics");
        ImGui::TextDisabled("Physics simulates during playback in Animate mode.");
        break;
    }
    case SelType::Skin: {
        Skin& s = d.skins[p.index];
        header("Skin", s.name);
        if ((int)p.index != doc.skin) { if (ImGui::Button("Set active skin")) doc.skin = p.index; }
        else ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "Active skin");
        size_t n = 0;
        for (auto& [si, m] : s.attachments) n += m.size();
        ImGui::Text("%zu attachments", n);
        if (p.index > 0) {
            ImGui::TextUnformatted("Skin bones (active only with this skin):");
            for (int b : s.bones) ImGui::BulletText("%s", d.bones[b].name.c_str());
            std::vector<int> sel = doc.selectedBones();
            if (ImGui::Button("Copy selected default-skin attachment here")) {
                for (auto& sel2 : doc.selection)
                    if (sel2.type == SelType::Attachment)
                        if (const Attachment* a = d.skins[0].get(sel2.index, sel2.name)) { doc.checkpoint("Add to skin"); s.set(sel2.index, *a); }
            }
        }
        break;
    }
    case SelType::Animation: {
        Animation& a = d.animations[p.index];
        header("Animation", a.name);
        ImGui::Text("Duration: %.3f s (%d frames)", a.duration(), (int)std::round(a.duration() * d.fps));
        ImGui::Text("Timelines: %zu", a.timelines.size());
        if (ImGui::Button("Edit (Animate mode)")) { doc.animation = p.index; doc.setMode(Mode::Animate); }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) doc.duplicateSelection();
        break;
    }
    case SelType::Event: {
        EventData& e = d.events[p.index];
        header("Event", e.name);
        if (ImGui::InputInt("Int", &e.intValue)) doc.dirty = true;
        dragFloatEdit(doc, "Float", &e.floatValue, 0.01f, "%.3f", "Event");
        static char buf[256];
        strncpy_s(buf, e.stringValue.c_str(), _TRUNCATE);
        if (ImGui::InputText("String", buf, sizeof buf)) { e.stringValue = buf; doc.dirty = true; }
        static char abuf[256];
        strncpy_s(abuf, e.audio.c_str(), _TRUNCATE);
        if (ImGui::InputText("Audio path", abuf, sizeof abuf)) { e.audio = abuf; doc.dirty = true; }
        dragFloatEdit(doc, "Volume", &e.volume, 0.01f, "%.2f", "Event");
        dragFloatEdit(doc, "Balance", &e.balance, 0.01f, "%.2f", "Event");
        if (doc.mode == Mode::Animate && doc.currentAnimation() && ImGui::Button("Key event at current frame")) {
            doc.checkpoint("Key event");
            doc.currentAnimation()->get(TimelineType::Event, -1).setKey(std::round(doc.time * d.fps) / d.fps).str = e.name;
        }
        break;
    }
    case SelType::Image: {
        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1, 1), "Image:");
        ImGui::SameLine();
        ImGui::TextUnformatted(p.name.c_str());
        if (Image* im = gfx.image(doc.imageFileFor(p.name))) {
            ImGui::Text("%d x %d", im->w, im->h);
            float s = std::min(1.0f, ImGui::GetContentRegionAvail().x / std::max(1, im->w));
            ImGui::Image((ImTextureID)im->srv, ImVec2(im->w * s, im->h * s));
        }
        ImGui::TextDisabled("Drag the image onto the viewport, a bone or a slot.");
        break;
    }
    }
    ImGui::PopItemWidth();
    ImGui::End();
}

// ---- small views ----

void Editor::animationsView() {
    ImGui::Begin("Animations", &showAnimations);
    if (ImGui::Button("New")) { doc.createAnimation("animation"); doc.setMode(Mode::Animate); }
    ImGui::SameLine();
    ImGui::BeginDisabled(doc.animation < 0);
    if (ImGui::Button("Duplicate")) { doc.selection = {{SelType::Animation, doc.animation, ""}}; doc.duplicateSelection(); }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) { doc.selection = {{SelType::Animation, doc.animation, ""}}; doc.deleteSelection(); }
    ImGui::EndDisabled();
    ImGui::Separator();
    for (size_t i = 0; i < doc.data.animations.size(); i++) {
        const Animation& a = doc.data.animations[i];
        char label[256];
        std::snprintf(label, sizeof label, "%s##an%zu", a.name.c_str(), i);
        if (ImGui::Selectable(label, (int)i == doc.animation)) {
            doc.animation = (int)i;
            doc.setTime(0);
            doc.setMode(Mode::Animate);
            doc.selection = {{SelType::Animation, (int)i, ""}};
        }
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 90);
        ImGui::TextDisabled("%5.2fs", a.duration());
    }
    ImGui::End();
}

void Editor::skinsView() {
    ImGui::Begin("Skins", &showSkins);
    if (ImGui::Button("New skin")) doc.skin = doc.createSkin("skin");
    ImGui::Separator();
    for (size_t i = 0; i < doc.data.skins.size(); i++) {
        bool active = (int)i == doc.skin;
        if (ImGui::RadioButton((doc.data.skins[i].name + "##sk" + std::to_string(i)).c_str(), active)) doc.skin = (int)i;
        ImGui::SameLine();
        if (ImGui::SmallButton(("Select##s" + std::to_string(i)).c_str())) doc.selection = {{SelType::Skin, (int)i, ""}};
    }
    ImGui::TextDisabled("Comma / Period cycle skins.");
    ImGui::End();
}

void Editor::eventsView() {
    ImGui::Begin("Events", &showEvents);
    if (ImGui::Button("New event")) doc.selection = {{SelType::Event, doc.createEvent("event"), ""}};
    ImGui::Separator();
    for (size_t i = 0; i < doc.data.events.size(); i++)
        if (ImGui::Selectable(doc.data.events[i].name.c_str(), doc.isSelected({SelType::Event, (int)i, ""})))
            doc.selection = {{SelType::Event, (int)i, ""}};
    ImGui::End();
}

void Editor::metricsView() {
    ImGui::Begin("Metrics", &showMetrics);
    const SkeletonData& d = doc.data;
    int atts = 0, verts = 0, tris = 0, weighted = 0, active = 0;
    for (auto& s : d.skins)
        for (auto& [si, m] : s.attachments)
            for (auto& [n, a] : m) {
                atts++;
                verts += a.vertexCount();
                tris += (int)a.triangles.size() / 3;
                if (a.isWeighted()) weighted++;
            }
    for (auto& s : doc.sk.slots) if (doc.sk.attachmentFor(s)) active++;
    int keys = 0, tls = 0;
    for (auto& a : d.animations) { tls += (int)a.timelines.size(); for (auto& t : a.timelines) keys += (int)t.keys.size(); }
    ImGui::Text("Bones: %zu", d.bones.size());
    ImGui::Text("Slots: %zu (%d with visible attachment)", d.slots.size(), active);
    ImGui::Text("Attachments: %d (%d weighted meshes)", atts, weighted);
    ImGui::Text("Mesh vertices: %d  triangles: %d", verts, tris);
    ImGui::Text("Constraints: %zu IK, %zu transform, %zu path, %zu physics", d.ik.size(), d.transforms.size(), d.paths.size(), d.physics.size());
    ImGui::Text("Skins: %zu  Events: %zu", d.skins.size(), d.events.size());
    ImGui::Text("Animations: %zu  Timelines: %d  Keys: %d", d.animations.size(), tls, keys);
    ImGui::Text("Undo history: %zu", doc.undoStack.size());
    ImGui::Text("Frame: %.1f ms (%.0f fps)", 1000.0f / std::max(1.0f, ImGui::GetIO().Framerate), ImGui::GetIO().Framerate);
    ImGui::End();
}

void Editor::outlineView() {
    ImGui::Begin("Outline", &showOutline);
    ImGui::TextDisabled("Selection");
    for (auto& s : doc.selection) {
        switch (s.type) {
        case SelType::Bone: ImGui::BulletText("Bone %s", doc.data.bones[s.index].name.c_str()); break;
        case SelType::Slot: ImGui::BulletText("Slot %s", doc.data.slots[s.index].name.c_str()); break;
        case SelType::Attachment: ImGui::BulletText("Attachment %s (slot %s)", s.name.c_str(), doc.data.slots[s.index].name.c_str()); break;
        default: ImGui::BulletText("%s", s.name.c_str()); break;
        }
    }
    ImGui::Separator();
    ImGui::TextDisabled("Setup draw order (back to front)");
    for (size_t i = 0; i < doc.data.slots.size(); i++) ImGui::Text("%3zu  %s", i, doc.data.slots[i].name.c_str());
    ImGui::End();
}

void Editor::problemsView() {
    ImGui::Begin("Problems", &showProblems);
    std::vector<std::string> problems = doc.warnings;
    for (size_t si = 0; si < doc.data.slots.size(); si++)
        for (auto& s : doc.data.skins)
            if (auto it = s.attachments.find((int)si); it != s.attachments.end())
                for (auto& [n, a] : it->second)
                    if ((a.type == AttachmentType::Region || a.type == AttachmentType::Mesh) && doc.imagePathFor(a).empty())
                        problems.push_back("Missing image '" + a.imagePath() + "' for " + doc.data.slots[si].name + "/" + n + " (skin " + s.name + ")");
    for (auto& s : doc.data.slots)
        if (!s.attachment.empty() && doc.data.findAttachment(doc.skin, doc.data.findSlot(s.name), s.attachment) == nullptr)
            problems.push_back("Slot '" + s.name + "' setup attachment '" + s.attachment + "' does not exist in the active skin");
    if (problems.empty()) ImGui::TextColored(ImVec4(0.5f, 0.85f, 0.5f, 1), "No problems found.");
    for (auto& p : problems) ImGui::BulletText("%s", p.c_str());
    if (!doc.warnings.empty() && ImGui::Button("Clear import warnings")) doc.warnings.clear();
    ImGui::End();
}

void Editor::meshToolsView() {
    ImGui::Begin("Mesh Tools", &showMeshTools);
    int si = -1;
    Attachment* a = selectedAttachment(&si);
    if (!a || !a->isVertex()) {
        ImGui::TextWrapped("Select a mesh, bounding box, clipping or path attachment. Regions can be converted to meshes from the Properties view.");
        meshMode = MeshMode::Off;
        ImGui::End();
        return;
    }
    ImGui::Text("%s  %s  (%d vertices)", attachmentTypeName(a->type), a->name.c_str(), a->vertexCount());
    auto modeButton = [&](const char* l, MeshMode m) {
        bool on = meshMode == m;
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.50f, 0.78f, 1));
        if (ImGui::Button(l, ImVec2(80 * uiScale, 0))) meshMode = on ? MeshMode::Off : m;
        if (on) ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    modeButton("Modify", MeshMode::Modify);
    bool isMesh = a->type == AttachmentType::Mesh;
    ImGui::BeginDisabled(!isMesh || doc.mode != Mode::Setup);
    modeButton("Create", MeshMode::Create);
    modeButton("Delete", MeshMode::Delete);
    ImGui::EndDisabled();
    ImGui::NewLine();
    ImGui::TextDisabled(doc.mode == Mode::Setup ? "Modify drags vertices (setup). Create/Delete edit topology." : "Animate mode: dragging vertices keys deform.");
    if (isMesh && doc.mode == Mode::Setup) {
        ImGui::Separator();
        static float spacing = 40;
        static int alpha = 10, maxVerts = 24;
        static float padding = 2;
        ImGui::SetNextItemWidth(120);
        ImGui::DragInt("Alpha threshold", &alpha, 1, 0, 254);
        ImGui::SetNextItemWidth(120);
        ImGui::DragInt("Max hull vertices", &maxVerts, 1, 3, 200);
        ImGui::SetNextItemWidth(120);
        ImGui::DragFloat("Padding", &padding, 0.5f, 0, 50);
        bool canEdit = !a->isWeighted();
        ImGui::BeginDisabled(!canEdit);
        if (ImGui::Button("Trace (from image alpha)")) {
            Image* im = gfx.image(doc.imagePathFor(*a));
            if (!im) setStatus("Image not found");
            else {
                std::vector<float> hull = traceHull(im->rgba.data(), im->w, im->h, alpha, maxVerts, padding);
                if (hull.size() < 6) setStatus("Trace failed (image fully transparent?)");
                else {
                    // Fit an affine map uv -> local from the existing vertices (exact for unedited meshes).
                    int n = a->vertexCount();
                    double sx[3][3] = {}, rx[3] = {}, ry[3] = {};
                    for (int k = 0; k < n; k++) {
                        double u = a->uvs[k * 2], v = a->uvs[k * 2 + 1], row[3] = {u, v, 1};
                        for (int r = 0; r < 3; r++) {
                            for (int c = 0; c < 3; c++) sx[r][c] += row[r] * row[c];
                            rx[r] += row[r] * a->vertices[k * 2];
                            ry[r] += row[r] * a->vertices[k * 2 + 1];
                        }
                    }
                    auto solve = [&](double b[3], double out[3]) {
                        double m[3][4];
                        for (int r = 0; r < 3; r++) { for (int c = 0; c < 3; c++) m[r][c] = sx[r][c]; m[r][3] = b[r]; }
                        for (int c = 0; c < 3; c++) {
                            int piv = c;
                            for (int r = c + 1; r < 3; r++) if (std::fabs(m[r][c]) > std::fabs(m[piv][c])) piv = r;
                            for (int k = 0; k < 4; k++) std::swap(m[c][k], m[piv][k]);
                            if (std::fabs(m[c][c]) < 1e-12) return false;
                            for (int r = 0; r < 3; r++) if (r != c) { double f = m[r][c] / m[c][c]; for (int k = 0; k < 4; k++) m[r][k] -= f * m[c][k]; }
                        }
                        for (int r = 0; r < 3; r++) out[r] = m[r][3] / m[r][r];
                        return true;
                    };
                    double cx[3], cy[3];
                    if (!solve(rx, cx) || !solve(ry, cy)) setStatus("Cannot map image to mesh");
                    else {
                        doc.checkpoint("Trace mesh");
                        a = selectedAttachment(&si);
                        a->vertices.clear(); a->uvs.clear();
                        for (size_t k = 0; k + 1 < hull.size(); k += 2) {
                            float u = clampf(hull[k] / im->w, 0, 1), v = clampf(hull[k + 1] / im->h, 0, 1);
                            a->uvs.push_back(u); a->uvs.push_back(v);
                            a->vertices.push_back((float)(cx[0] * u + cx[1] * v + cx[2]));
                            a->vertices.push_back((float)(cy[0] * u + cy[1] * v + cy[2]));
                        }
                        a->worldVerticesLength = (int)a->vertices.size();
                        a->hullLength = a->vertexCount();
                        a->triangles = triangulateMesh(a->vertices, a->hullLength);
                        a->edges.clear();
                        for (int h = 0; h < a->hullLength; h++) { a->edges.push_back(h); a->edges.push_back((h + 1) % a->hullLength); }
                        meshSelection.clear();
                        setStatus("Traced hull: " + std::to_string(a->hullLength) + " vertices");
                    }
                }
            }
        }
        ImGui::SetNextItemWidth(120);
        ImGui::DragFloat("Interior spacing", &spacing, 1, 4, 500);
        if (ImGui::Button("Generate interior vertices")) {
            std::vector<float> hull(a->vertices.begin(), a->vertices.begin() + a->hullLength * 2);
            std::vector<float> pts = generateInteriorPoints(hull, spacing);
            doc.checkpoint("Generate vertices");
            a = selectedAttachment(&si);
            // Interior UVs via barycentric lookup in the current triangulation.
            std::vector<float> nv(a->vertices.begin(), a->vertices.begin() + a->hullLength * 2), nuv(a->uvs.begin(), a->uvs.begin() + a->hullLength * 2);
            for (size_t k = 0; k + 1 < pts.size(); k += 2) {
                float lx = pts[k], ly = pts[k + 1];
                for (size_t t = 0; t + 2 < a->triangles.size(); t += 3) {
                    int i0 = a->triangles[t], i1 = a->triangles[t + 1], i2 = a->triangles[t + 2];
                    float x0 = a->vertices[i0 * 2], y0 = a->vertices[i0 * 2 + 1], x1 = a->vertices[i1 * 2], y1 = a->vertices[i1 * 2 + 1], x2 = a->vertices[i2 * 2], y2 = a->vertices[i2 * 2 + 1];
                    float den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
                    if (std::fabs(den) < 1e-9f) continue;
                    float w0 = ((y1 - y2) * (lx - x2) + (x2 - x1) * (ly - y2)) / den, w1 = ((y2 - y0) * (lx - x2) + (x0 - x2) * (ly - y2)) / den, w2 = 1 - w0 - w1;
                    if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
                    nv.push_back(lx); nv.push_back(ly);
                    nuv.push_back(w0 * a->uvs[i0 * 2] + w1 * a->uvs[i1 * 2] + w2 * a->uvs[i2 * 2]);
                    nuv.push_back(w0 * a->uvs[i0 * 2 + 1] + w1 * a->uvs[i1 * 2 + 1] + w2 * a->uvs[i2 * 2 + 1]);
                    break;
                }
            }
            a->vertices = nv; a->uvs = nuv;
            a->worldVerticesLength = (int)nv.size();
            a->triangles = triangulateMesh(a->vertices, a->hullLength);
            setStatus(std::to_string(a->vertexCount() - a->hullLength) + " interior vertices");
        }
        if (ImGui::Button("Reset to image quad")) {
            doc.checkpoint("Reset mesh");
            a = selectedAttachment(&si);
            float w = a->width / 2, h = a->height / 2;
            a->vertices = {-w, -h, -w, h, w, h, w, -h};
            a->uvs = {0, 1, 0, 0, 1, 0, 1, 1};
            a->worldVerticesLength = 8;
            a->triangles = {0, 1, 2, 2, 3, 0};
            a->hullLength = 4;
            a->edges = {0, 1, 1, 2, 2, 3, 3, 0};
            meshSelection.clear();
        }
        ImGui::EndDisabled();
        if (!canEdit) ImGui::TextDisabled("Unbind weights (Weights view) to change topology.");
    }
    ImGui::End();
}

void Editor::weightsView() {
    ImGui::Begin("Weights", &showWeights);
    int si = -1;
    Attachment* a = selectedAttachment(&si);
    if (!a || a->type != AttachmentType::Mesh) {
        ImGui::TextWrapped("Select a mesh attachment. Weights bind mesh vertices to one or more bones.");
        ImGui::End();
        return;
    }
    ImGui::Text("Mesh %s: %s", a->name.c_str(), a->isWeighted() ? "weighted" : "not weighted");
    if (doc.mode != Mode::Setup) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "Weights are edited in Setup mode.");
    ImGui::BeginDisabled(doc.mode != Mode::Setup);
    // Bones bound to this mesh.
    std::vector<int> bound;
    if (a->isWeighted())
        for (size_t i = 0; i < a->bones.size();) {
            int n = a->bones[i++];
            for (int k = 0; k < n; k++, i++) if (std::find(bound.begin(), bound.end(), a->bones[i]) == bound.end()) bound.push_back(a->bones[i]);
        }
    std::sort(bound.begin(), bound.end());
    std::vector<int> sel = doc.selectedBones();
    if (ImGui::Button(a->isWeighted() ? "Rebind to selected bones" : "Bind to selected bones")) {
        if (sel.empty()) setStatus("Ctrl+click bones in the viewport or tree to choose them, keeping the mesh selected");
        else {
            doc.checkpoint("Bind weights");
            a = selectedAttachment(&si);
            if (a->isWeighted()) unbindWeights(*a, doc.sk, doc.data.slots[si].bone);
            bindWeights(*a, doc.sk, doc.data.slots[si].bone, sel);
            weightBone = sel[0];
            setStatus("Bound to " + std::to_string(sel.size()) + " bones with automatic weights");
        }
    }
    if (a->isWeighted()) {
        ImGui::SameLine();
        if (ImGui::Button("Unbind")) {
            doc.checkpoint("Unbind weights");
            a = selectedAttachment(&si);
            unbindWeights(*a, doc.sk, doc.data.slots[si].bone);
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Paint bone (click), then drag in the viewport with the Weights tool:");
        for (int b : bound) {
            bool on = weightBone == b;
            if (ImGui::Selectable(doc.data.bones[b].name.c_str(), on)) { weightBone = b; doc.tool = Tool::Weights; }
        }
        const char* modes[] = {"Add", "Remove", "Replace", "Smooth"};
        int m = (int)weightMode;
        if (ImGui::Combo("Mode", &m, modes, 4)) weightMode = (WeightMode)m;
        ImGui::SliderFloat("Strength", &brushStrength, 0.01f, 1);
        ImGui::SliderFloat("Size (px)", &brushSize, 4, 200);
        if (ImGui::Button("Prune < 1%")) { doc.checkpoint("Prune weights"); a = selectedAttachment(&si); pruneWeights(*a, 0.01f); }
        ImGui::SameLine();
        if (ImGui::Button("Normalize")) { doc.checkpoint("Normalize weights"); a = selectedAttachment(&si); normalizeWeights(*a); }
        if (!meshSelection.empty() && weightBone >= 0) {
            ImGui::Separator();
            static float w = 1;
            ImGui::SliderFloat("Weight", &w, 0, 1);
            ImGui::SameLine();
            if (ImGui::Button("Set on selected vertices")) {
                doc.checkpoint("Set weight");
                a = selectedAttachment(&si);
                for (int v : meshSelection) setVertexWeight(*a, doc.sk, v, weightBone, w);
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::End();
}

} // namespace osteon
