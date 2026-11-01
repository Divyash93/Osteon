// Tree, Properties and the smaller views.
#include "App.h"
#include "Geometry.h"
#include "Icons.h"
#include "imgui_internal.h"
#include <set>

namespace osteon {

// Tree text colours; the Light theme darkens them so they read on light panels.
static ImVec4 themed(ImVec4 c) { return gLightUi ? ImVec4(c.x * 0.42f, c.y * 0.42f, c.z * 0.42f, c.w) : c; }
static const ImVec4 kBoneColD(0.85f, 0.85f, 0.85f, 1), kSlotColD(0.55f, 0.80f, 1, 1), kAttColD(0.95f, 0.80f, 0.45f, 1);
static const ImVec4 kConColD(1, 0.55f, 0.35f, 1), kDimColD(0.710f, 0.710f, 0.710f, 1); // #b5b5b5, the secondary text
#define kBoneCol themed(kBoneColD)
#define kSlotCol themed(kSlotColD)
#define kAttCol themed(kAttColD)
#define kConCol themed(kConColD)
#define kDimCol (gLightUi ? ImVec4(0.45f, 0.45f, 0.45f, 1) : kDimColD)

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

// Tree row with a type icon: the label is padded so the icon fits in front of the name.
static float g_treeRowY = 0; // top of the tree row being drawn, used to place its eye in the left gutter

// `boneIcon`: for a bone row, the bone's chosen icon replaces the ordinary bone symbol.
static bool treeRow(const void* id, ImGuiTreeNodeFlags fl, Icon ic, const std::string& name, const std::string& boneIcon = {}) {
    g_treeRowY = ImGui::GetCursorScreenPos().y;
    float isz = ImGui::GetTextLineHeight() * 0.85f;
    int pad = (int)std::ceil((isz + 6) / ImGui::CalcTextSize(" ").x);
    std::string label(pad, ' ');
    label += name;
    bool open = ImGui::TreeNodeEx(id, fl, "%s", label.c_str());
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    float x = mn.x + ImGui::GetTreeNodeToLabelSpacing();
    ImVec2 c(x + isz / 2, (mn.y + mx.y) / 2);
    if (ic == Icon::Bone && !isDefaultBoneIcon(boneIcon)) drawBoneIcon(ImGui::GetWindowDrawList(), boneIcon, c, isz, ImGui::GetColorU32(ImGuiCol_Text));
    else drawIcon(ImGui::GetWindowDrawList(), ic, c, isz, ImGui::GetColorU32(ImGuiCol_Text));
    return open;
}

// One row of the New... menu: icon, label, a rounded accent highlight on hover. The popup it sits in must make the
// ImGui selectable colours transparent (see the "newbone" popup) so only this highlight shows.
static bool newMenuItem(Icon ic, const char* label, bool enabled = true, const char* tip = nullptr) {
    const float h = ImGui::GetTextLineHeight() + 12;
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::Selectable((std::string("##nm") + label).c_str(), false, 0, ImVec2(0, h));
    ImGui::EndDisabled();
    bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // The theme accent (main.cpp); the style colours are overridden to transparent inside the menu, so not read from there.
    if (enabled && (hovered || ImGui::IsItemActive()))
        dl->AddRectFilled(mn, mx, ImGui::IsItemActive() ? IM_COL32(96, 156, 255, 255) : IM_COL32(18, 77, 173, 255), 6);
    ImU32 text = ImGui::GetColorU32(enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    float isz = h * 0.6f;
    drawIcon(dl, ic, ImVec2(mn.x + 10 + isz / 2, (mn.y + mx.y) / 2), isz, text);
    dl->AddText(ImVec2(mn.x + 10 + isz + 10, mn.y + (h - ImGui::GetTextLineHeight()) / 2), text, label);
    if (tip && hovered) ImGui::SetTooltip("%s", tip);
    return clicked && enabled;
}

static Icon attachmentIcon(AttachmentType t) {
    switch (t) {
    case AttachmentType::Mesh: return Icon::Mesh;
    case AttachmentType::LinkedMesh: return Icon::LinkedMesh;
    case AttachmentType::BoundingBox: return Icon::BoundingBox;
    case AttachmentType::Path: return Icon::Path;
    case AttachmentType::Point: return Icon::Point;
    case AttachmentType::Clipping: return Icon::Clipping;
    default: return Icon::Region;
    }
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
    // Visibility eyes sit in a left gutter, as in Spine; the tree is indented to make room for them.
    const float gutterX = ImGui::GetCursorScreenPos().x;
    ImGui::Indent(36 * uiScale); // room for the eye and key columns in the gutter
    SkeletonData& d = doc.data;
    bool add = ImGui::GetIO().KeyCtrl; // Ctrl toggles one item; Shift selects a range (below)
    const ImGuiTreeNodeFlags base = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    bool searching = filter[0] != 0;
    // Rows in the order they were drawn: last frame's order is used for Shift-range clicks (this frame's
    // rows below the click are not known yet).
    static std::vector<Sel> rowsLast, rowsNow;
    static Sel rangeAnchor;
    rowsLast = rowsNow;
    rowsNow.clear();

    // The tree follows the selection: when it changes anywhere but in the tree itself (canvas click, marquee,
    // Ctrl+A), open the rows that lead to the selected items and scroll to the primary one.
    static std::vector<Sel> prevSelection;
    static bool selectionFromTree = false;
    bool reveal = false;
    if (doc.selection != prevSelection) {
        reveal = !selectionFromTree && !doc.selection.empty();
        prevSelection = doc.selection;
    }
    selectionFromTree = false;
    std::set<int> revealBones, revealSlots;
    if (reveal) {
        auto addChain = [&](int bone) { for (; bone >= 0 && bone < (int)d.bones.size(); bone = d.bones[bone].parent) revealBones.insert(bone); };
        for (const Sel& s : doc.selection) {
            if (s.type == SelType::Bone && s.index >= 0 && s.index < (int)d.bones.size()) addChain(d.bones[s.index].parent);
            else if ((s.type == SelType::Slot || s.type == SelType::Attachment) && s.index >= 0 && s.index < (int)d.slots.size()) {
                revealSlots.insert(s.index);
                addChain(d.slots[s.index].bone);
            }
        }
    }
    // Drag-and-drop in the tree: dropping on the top/bottom edge of a row places the item before/after it, on the
    // middle reparents. Changes run after the tree is drawn, since they renumber bones and slots.
    std::function<void()> deferredTreeAction;
    const ImGuiDragDropFlags previewDrop = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
    auto dropZone = [&]() { // -1 = above the row, 0 = onto it, +1 = below it
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        float y = ImGui::GetIO().MousePos.y, h = mx.y - mn.y;
        return y < mn.y + h * 0.28f ? -1 : (y > mx.y - h * 0.28f ? 1 : 0);
    };
    auto dropMark = [&](int zone) {
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (zone == 0) dl->AddRect(mn, mx, col, 2, 0, 1.5f);
        else { float y = zone < 0 ? mn.y : mx.y; dl->AddLine(ImVec2(mn.x, y), ImVec2(mx.x, y), col, 2.5f); }
    };

    auto clickSelect = [&](const Sel& s) {
        rowsNow.push_back(s);
        if (reveal && s == doc.primary()) ImGui::SetScrollHereY(0.5f);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
            selectionFromTree = true;
            if (ImGui::GetIO().KeyShift && rangeAnchor.type != SelType::None) doc.selectRange(rangeAnchor, s, rowsLast, add);
            else doc.select(s, add);
            rangeAnchor = s;
            if (s.type == SelType::Animation) { doc.animation = s.index; doc.setMode(Mode::Animate); }
            if (s.type == SelType::Skin) doc.skin = s.index;
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !doc.isSelected(s)) doc.select(s);
    };
    auto eye = [&](bool& visible, const char* id, int boneIndex = -1) {
        ImVec2 saved = ImGui::GetCursorScreenPos(); // the tree layout continues from here
        float sz = 16 * uiScale;
        ImGui::SetCursorScreenPos(ImVec2(gutterX, g_treeRowY + (ImGui::GetTextLineHeight() - sz) / 2));
        ImGui::PushID(id);
        if (iconOnlyButton("##eye", visible ? Icon::Eye : Icon::EyeOff, false, sz, visible ? "Hide (right click: with child bones and slots)" : "Show (right click: with child bones and slots)")) { doc.checkpoint("Visibility"); visible = !visible; }
        if (boneIndex >= 0 && ImGui::IsItemClicked(ImGuiMouseButton_Right)) doc.setSubtreeVisible(boneIndex, !visible);
        ImGui::PopID();
        ImGui::SetCursorScreenPos(saved);
    };
    // Keyframe marker in the gutter, right of the eye (Animate mode only): red = keyed here, green = keyed elsewhere.
    auto keyMarkState = [&](int state) {
        if (doc.mode != Mode::Animate || state == 0) return;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float sz = 14 * uiScale;
        ImVec2 c(gutterX + 26 * uiScale, g_treeRowY + ImGui::GetTextLineHeight() / 2);
        ImU32 col = state == 3 ? IM_COL32(255, 150, 40, 255)    // pending: changed but not keyed
                  : state == 2 ? IM_COL32(235, 80, 80, 255)     // keyed at this frame
                               : IM_COL32(110, 210, 90, 255);   // keyed at other frames
        drawIcon(dl, Icon::Key, c, sz, col);
    };
    auto keyMark = [&](int target, bool slot) { if (doc.mode == Mode::Animate) keyMarkState(doc.keyState(target, slot)); };

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
        std::vector<std::string> emptyPlaceholders; // placeholders no skin fills yet
        for (auto& n : s.placeholders) if (!seen.count(n)) { emptyPlaceholders.push_back(n); show |= searching && matches(n, filter); }
        if (!show) return;
        Sel ss{SelType::Slot, si, ""};
        ImGuiTreeNodeFlags fl = base | (doc.isSelected(ss) ? ImGuiTreeNodeFlags_Selected : 0) | (atts.empty() && emptyPlaceholders.empty() ? ImGuiTreeNodeFlags_Leaf : 0);
        if (searching || revealSlots.count(si)) ImGui::SetNextItemOpen(true);
        ImGui::PushStyleColor(ImGuiCol_Text, kSlotCol);
        bool open = treeRow((void*)(intptr_t)(0x100000 + si), fl, Icon::Slot, s.name);
        ImGui::PopStyleColor();
        clickSelect(ss);
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("OSTEON_SLOT", &si, sizeof si);
            ImGui::Text("Slot %s", s.name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            // Another slot dropped here takes this slot's place in the draw order (before it, or after it).
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("OSTEON_SLOT", previewDrop)) {
                int from = *(const int*)p->Data;
                if (from != si) {
                    int zone = dropZone();
                    if (zone == 0) zone = 1;
                    dropMark(zone);
                    if (p->IsDelivery()) deferredTreeAction = [this, from, si, zone]() { doc.moveSlotNextTo(from, si, zone > 0); };
                }
            }
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
            if (ImGui::MenuItem("Delete", "Del")) requestDelete();
            ImGui::Separator();
            if (ImGui::MenuItem("Set Parent")) {
                doc.select({SelType::Slot, si, ""});
                pickParent = true;
                setStatus("Click the new parent bone (Esc cancels)");
            }
            if (ImGui::MenuItem("New Skin Placeholder")) doc.createPlaceholder(si);
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
        keyMark(si, true);
        if (!open) return;
        for (auto& [n, a] : atts) {
            Sel as{SelType::Attachment, si, n};
            bool active = doc.sk.slots.size() > (size_t)si && doc.sk.slots[si].attachment == n;
            ImGuiTreeNodeFlags af = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                    (doc.isSelected(as) ? ImGuiTreeNodeFlags_Selected : 0);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? kAttCol : kDimCol);
            treeRow((void*)(intptr_t)(std::hash<std::string>()(n) ^ (si * 7919)), af, attachmentIcon(a->type), n);
            ImGui::PopStyleColor();
            if (doc.mode == Mode::Animate) keyMarkState(doc.deformKeyState(si, n)); // mesh vertices animated by deform keys
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
                if (ImGui::MenuItem("Set Parent")) {
                    doc.select({SelType::Attachment, si, n});
                    pickParent = true;
                    setStatus("Click the new parent bone (Esc cancels)");
                }
                if (ImGui::MenuItem("Rename", "F2")) beginRename();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D")) doc.duplicateSelection();
                if (ImGui::MenuItem("Delete", "Del")) requestDelete();
                if (a->type == AttachmentType::Region && ImGui::MenuItem("Convert to Mesh")) doc.convertToMesh(si, n);
                ImGui::EndPopup();
            }
        }
        for (auto& n : emptyPlaceholders) {
            Sel as{SelType::Attachment, si, n};
            ImGuiTreeNodeFlags af = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                    (doc.isSelected(as) ? ImGuiTreeNodeFlags_Selected : 0);
            ImGui::PushStyleColor(ImGuiCol_Text, kDimCol);
            treeRow((void*)(intptr_t)(std::hash<std::string>()("ph" + n) ^ (si * 7919)), af, Icon::Skins, n);
            ImGui::PopStyleColor();
            clickSelect(as);
            if (ImGui::BeginPopupContextItem(("phctx" + n + std::to_string(si)).c_str())) {
                if (ImGui::MenuItem("Rename", "F2")) beginRename();
                if (ImGui::MenuItem("Delete", "Del")) requestDelete();
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
        if (revealBones.count(bi)) ImGui::SetNextItemOpen(true);
        bool showSelf = !searching || matches(b.name, filter);
        bool open = true;
        if (showSelf) {
            bool pend = doc.pending.count(bi) > 0;
            ImGui::PushStyleColor(ImGuiCol_Text, pend ? themed(ImVec4(1, 0.6f, 0.2f, 1)) : kBoneCol);
            open = treeRow((void*)(intptr_t)(bi + 1), fl, Icon::Bone, b.name, (isDefaultBoneIcon(b.icon) && !(b.length > 0) && bi > 0) ? std::string("crosshair") : b.icon);
            ImGui::PopStyleColor();
            clickSelect(bs);
            if (bi > 0 && ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("OSTEON_BONE", &bi, sizeof bi);
                ImGui::Text("Bone %s", b.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                // A bone dropped on the top/bottom edge of a sibling is reordered; on the middle of any bone, reparented.
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("OSTEON_BONE", previewDrop)) {
                    int from = *(const int*)p->Data, zone = dropZone();
                    bool sibling = bi > 0 && from != bi && d.bones[from].parent == d.bones[bi].parent;
                    if (zone != 0 && sibling) {
                        dropMark(zone);
                        if (p->IsDelivery()) deferredTreeAction = [this, from, bi, zone]() { doc.moveBone(from, bi, zone > 0); };
                    } else if (from != bi) {
                        dropMark(0);
                        if (p->IsDelivery()) deferredTreeAction = [this, from, bi]() { doc.reparentBones({from}, bi); };
                    }
                }
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
                if (ImGui::MenuItem("Rename", "F2")) beginRename();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, bi > 0)) doc.duplicateSelection();
                if (ImGui::MenuItem("Delete", "Del", false, bi > 0)) requestDelete();
                ImGui::EndPopup();
            }
            eye(b.visible, ("vb" + std::to_string(bi)).c_str(), bi);
            keyMark(bi, false);
        }
        if (open) {
            for (int s : slots) slotNode(s);
            for (int k : kids) boneNode(k);
            if (showSelf) ImGui::TreePop();
        }
    };

    std::string skelName = doc.projectPath.empty() ? "skeleton" : stripExtension(fileName(doc.projectPath));
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (treeRow("##skel", base | (doc.isSelected({SelType::Skeleton, 0, ""}) ? ImGuiTreeNodeFlags_Selected : 0), Icon::Skeleton, skelName)) {
        clickSelect({SelType::Skeleton, 0, ""});
        if (!d.bones.empty()) boneNode(0);

        if (treeRow("##grpDrawOrder", base, Icon::DrawOrder, "Draw order")) {
            for (int i = (int)d.slots.size() - 1; i >= 0; i--) { // front-most first, like Spine
                if (searching && !matches(d.slots[i].name, filter)) continue;
                Sel s{SelType::Slot, i, ""};
                ImGui::PushStyleColor(ImGuiCol_Text, kSlotCol);
                ImGui::Selectable((d.slots[i].name + "##do" + std::to_string(i)).c_str(), doc.isSelected(s));
                ImGui::PopStyleColor();
                clickSelect(s);
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
        const ImGuiTreeNodeFlags leaf = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (treeRow("##grpConstraints", base, Icon::Constraints, "Constraints")) {
            auto conRow = [&](SelType t, int i, const std::string& name, Icon ic) {
                Sel s{t, i, ""};
                ImGui::PushStyleColor(ImGuiCol_Text, kConCol);
                treeRow((void*)(intptr_t)(0x200000 + (int)t * 1000 + i), leaf | (doc.isSelected(s) ? ImGuiTreeNodeFlags_Selected : 0), ic, name);
                ImGui::PopStyleColor();
                clickSelect(s);
            };
            for (size_t i = 0; i < d.ik.size(); i++) conRow(SelType::Ik, (int)i, d.ik[i].name, Icon::Ik);
            for (size_t i = 0; i < d.transforms.size(); i++) conRow(SelType::Transform, (int)i, d.transforms[i].name, Icon::TransformCon);
            for (size_t i = 0; i < d.paths.size(); i++) conRow(SelType::Path, (int)i, d.paths[i].name, Icon::PathCon);
            for (size_t i = 0; i < d.physics.size(); i++) conRow(SelType::Physics, (int)i, d.physics[i].name, Icon::PhysicsCon);
            for (size_t i = 0; i < d.sliders.size(); i++) conRow(SelType::Slider, (int)i, d.sliders[i].name, Icon::Adjust);
            ImGui::TreePop();
        }
        if (treeRow("##grpSkins", base, Icon::Skins, "Skins")) {
            for (size_t i = 0; i < d.skins.size(); i++) {
                Sel s{SelType::Skin, (int)i, ""};
                std::string label = d.skins[i].name + ((int)i == doc.skin ? "  (active)" : "");
                treeRow((void*)(intptr_t)(0x300000 + i), leaf | (doc.isSelected(s) ? ImGuiTreeNodeFlags_Selected : 0), Icon::Skins, label);
                clickSelect(s);
            }
            ImGui::TreePop();
        }
        if (treeRow("##grpEvents", base, Icon::Events, "Events")) {
            for (size_t i = 0; i < d.events.size(); i++) {
                Sel s{SelType::Event, (int)i, ""};
                treeRow((void*)(intptr_t)(0x400000 + i), leaf | (doc.isSelected(s) ? ImGuiTreeNodeFlags_Selected : 0), Icon::Events, d.events[i].name);
                clickSelect(s);
            }
            ImGui::TreePop();
        }
        if (treeRow("##grpAnimations", base, Icon::Animations, "Animations")) {
            for (size_t i = 0; i < d.animations.size(); i++) {
                Sel s{SelType::Animation, (int)i, ""};
                ImGui::PushStyleColor(ImGuiCol_Text, (int)i == doc.animation && doc.mode == Mode::Animate ? themed(ImVec4(0.4f, 0.75f, 1, 1)) : kBoneCol);
                treeRow((void*)(intptr_t)(0x500000 + i), leaf | (doc.isSelected(s) ? ImGuiTreeNodeFlags_Selected : 0), Icon::Animations, d.animations[i].name);
                ImGui::PopStyleColor();
                clickSelect(s);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { // double-click: rename
                    doc.selection = {s};
                    beginRename();
                }
            }
            ImGui::TreePop();
        }
        if (treeRow("##grpImages", base, Icon::ImagesFolder, "Images")) {
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
    ImGui::Unindent(36 * uiScale);
    if (deferredTreeAction) { deferredTreeAction(); selectionFromTree = true; } // drag-and-drop edits, after drawing
    if (ImGui::BeginPopupContextWindow("##treebg", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("New Animation")) newAnimation();
        if (ImGui::MenuItem("New Skin")) doc.createSkin("skin");
        if (ImGui::MenuItem("New Event")) doc.createEvent("event");
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::End();
}

// Sequence (frame-by-frame) settings shared by region and mesh attachments. Frames are images named
// path + zero-padded number (e.g. "walk01".."walk08"), as in Spine.
static void meshImageCorners(const Attachment& a, float out[8]);

// The Sequence checkbox (frame-by-frame images); switching it on looks for numbered frames.
static void sequenceCheckbox(Editor& ed, Attachment* a) {
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
}

static void sequenceEditor(Editor& ed, int si, Attachment* a) {
    Document& doc = ed.doc;
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

// "Skin..." popup: ticks the skins that list this bone (bone >= 0) or constraint (name) as theirs.
static void skinMembership(Document& doc, const char* id, int bone, const std::string& constraint) {
    const bool open = ImGui::SmallButton((std::string("Skin...##") + id).c_str());
    if (open) ImGui::OpenPopup(id);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose the skins that include this %s (used with Skin required)", bone >= 0 ? "bone" : "constraint");
    if (ImGui::BeginPopup(id)) {
        if (doc.data.skins.size() <= 1) ImGui::TextDisabled("No skins yet: create one in the Skins list.");
        for (size_t i = 1; i < doc.data.skins.size(); i++) {
            const Skin& s = doc.data.skins[i];
            bool in = bone >= 0 ? std::find(s.bones.begin(), s.bones.end(), bone) != s.bones.end()
                                : std::find(s.constraints.begin(), s.constraints.end(), constraint) != s.constraints.end();
            if (ImGui::Checkbox((s.name + "##sk" + std::to_string(i)).c_str(), &in)) {
                if (bone >= 0) doc.setSkinBone((int)i, bone, in); else doc.setSkinConstraint((int)i, constraint, in);
                doc.updatePose();
            }
        }
        ImGui::EndPopup();
    }
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

// ---- Spine-style properties table: a label cell with an icon on the left, value cells on the right ----

static const ImU32 kLabelBg = theme::hex(theme::kBorder);
static const ImVec4 kHeaderText(0.53f, 0.82f, 0.86f, 1);

static bool propBegin(const char* id, float labelW) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6, 4));
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::PopStyleVar();
        return false;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, labelW);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

static void propEnd() {
    ImGui::EndTable();
    ImGui::PopStyleVar();
}

static void propLabel(Icon ic, const char* text) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, gLightUi ? IM_COL32(190, 192, 196, 255) : kLabelBg);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight();
    drawIcon(ImGui::GetWindowDrawList(), ic, ImVec2(p.x + h * 0.55f, p.y + h * 0.5f), h * 0.95f, uiG(theme::hex(theme::kText)));
    ImGui::SetCursorScreenPos(ImVec2(p.x + h * 1.2f + 4, p.y));
    ImGui::TextUnformatted(text);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

// ---- Transform constraint (Spine 4.3 layout): source, targets, offsets, mixes, the property link graph and its curve editor ----

static const char* kTcName[6] = {"Rotate", "Translate X", "Translate Y", "Scale X", "Scale Y", "Shear Y"};
static const Icon kTcIcon[6] = {Icon::Rotate, Icon::Translate, Icon::Translate, Icon::Scale, Icon::Scale, Icon::Shear};

// The transform gizmo colours, as used for the dopesheet strips.
static ImU32 tcColor(int p) {
    switch (p) {
    case 0: case 1: case 3: return IM_COL32(225, 60, 60, 255);
    case 2: case 4: return IM_COL32(70, 200, 80, 255);
    default: return IM_COL32(240, 200, 60, 255);
    }
}

static ImVec2 bezierPt(ImVec2 a, ImVec2 b, ImVec2 c, ImVec2 d, float t) {
    float s = 1 - t, w0 = s * s * s, w1 = 3 * s * s * t, w2 = 3 * s * t * t, w3 = t * t * t;
    return ImVec2(w0 * a.x + w1 * b.x + w2 * c.x + w3 * d.x, w0 * a.y + w1 * b.y + w2 * c.y + w3 * d.y);
}

// Match: offsets (on the source properties) that make the first target keep its current pose through the links.
static void matchTransformOffsets(Document& doc, TransformConstraintData& c) {
    if (c.bones.empty() || c.target >= (int)doc.sk.bones.size()) return;
    bool ls = c.mapped ? c.localSource : c.local, lt = c.mapped ? c.localTarget : c.local;
    bool done[6] = {};
    for (auto& l : c.links) {
        if (done[l.from] || l.toMax == l.toMin || l.fromMax == l.fromMin) continue;
        float s = transformBoneProp(doc.sk.bones[c.target], l.from, ls);
        float t = transformBoneProp(doc.sk.bones[c.bones[0]], l.to, lt);
        float wanted = l.fromMin + (t - l.toMin) / (l.toMax - l.toMin) * (l.fromMax - l.fromMin);
        c.offsetOf(l.from) = wanted - s;
        done[l.from] = true;
    }
}

// The property link graph, as in Spine 4.3: ports on both edges that start unconnected. Drag from a left (source) port to a
// right (target) port to connect them, click a connected port to disconnect it. The Min / Max boxes of a row appear once it
// is connected, and a link shows its scale percentage while it is hovered or selected.
static void transformLinkGraph(Document& doc, TransformConstraintData& c, float u) {
    static const void* owner = nullptr;
    static int sel = -1, pressPort = -1;
    static bool pressRight = false;
    static ImVec2 pressPos;
    if (owner != &c) { owner = &c; sel = -1; pressPort = -1; }
    if (sel >= (int)c.links.size()) sel = -1;

    std::vector<TransformLink> legacy;
    if (!c.mapped) { TransformConstraintData tmp; tmp.local = c.local; tmp.materialiseLinks(); legacy = tmp.links; }
    std::vector<TransformLink>& L = c.mapped ? c.links : legacy;
    bool edit = c.mapped;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float rowH = 24 * u, fw = 44 * u, dotR = 5 * u;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float totalW = ImGui::GetContentRegionAvail().x;
    float labelW = totalW > 380 * u ? 100 * u : 26 * u;
    float cx0 = origin.x + labelW + 2 * fw, cx1 = origin.x + totalW - 2 * fw;
    if (cx1 - cx0 < 40 * u) cx1 = cx0 + 40 * u;
    ImU32 cText = uiG(theme::hex(theme::kText)), cDim = uiG(theme::hex(theme::kTextDisabled));
    dl->AddRectFilled(ImVec2(cx0, origin.y), ImVec2(cx1, origin.y + 6 * rowH), uiG(theme::hex(theme::kAppBg)));
    for (int i = 0; i < 6; i++) {
        float y = origin.y + i * rowH;
        dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + labelW, y + rowH - 1), uiG(theme::hex(theme::kBorder)));
        drawIcon(dl, kTcIcon[i], ImVec2(origin.x + 13 * u, y + rowH * 0.5f), 15 * u, cText);
        if (labelW > 40 * u) dl->AddText(ImVec2(origin.x + 26 * u, y + (rowH - ImGui::GetTextLineHeight()) * 0.5f), cText, kTcName[i]);
    }
    auto dotL = [&](int i) { return ImVec2(cx0 + dotR * 1.6f, origin.y + i * rowH + rowH * 0.5f); };
    auto dotRt = [&](int i) { return ImVec2(cx1 - dotR * 1.6f, origin.y + i * rowH + rowH * 0.5f); };
    auto linkPts = [&](const TransformLink& l, ImVec2& p0, ImVec2& p1, ImVec2& p2, ImVec2& p3) {
        p0 = dotL(l.from); p3 = dotRt(l.to);
        float dx = (p3.x - p0.x) * 0.5f;
        p1 = ImVec2(p0.x + dx, p0.y); p2 = ImVec2(p3.x - dx, p3.y);
    };

    int srcLink[6], dstLink[6];
    for (int i = 0; i < 6; i++) srcLink[i] = dstLink[i] = -1;
    for (int k = 0; k < (int)L.size(); k++) {
        if (srcLink[L[k].from] < 0) srcLink[L[k].from] = k;
        if (dstLink[L[k].to] < 0) dstLink[L[k].to] = k;
    }
    // Min / Max boxes: the left side belongs to the source property, the right side to the target property.
    ImGui::PushID("tcg");
    for (int i = 0; i < 6; i++) {
        float y = origin.y + i * rowH;
        for (int side = 0; side < 2; side++) {
            int k = side == 0 ? srcLink[i] : dstLink[i];
            for (int m = 0; m < 2; m++) {
                float x = side == 0 ? origin.x + labelW + m * fw : cx1 + m * fw;
                if (k < 0 || !edit) {
                    float v = k < 0 ? 0 : (side == 0 ? (m == 0 ? L[k].fromMin : L[k].fromMax) : (m == 0 ? L[k].toMin : L[k].toMax));
                    char buf[32];
                    if (k < 0) std::snprintf(buf, sizeof buf, "%s", m == 0 ? "Min" : "Max"); else std::snprintf(buf, sizeof buf, "%g", v);
                    dl->AddText(ImVec2(x + 4 * u, y + (rowH - ImGui::GetTextLineHeight()) * 0.5f), cDim, buf);
                    continue;
                }
                float* v = side == 0 ? (m == 0 ? &L[k].fromMin : &L[k].fromMax) : (m == 0 ? &L[k].toMin : &L[k].toMax);
                ImGui::SetCursorScreenPos(ImVec2(x, y));
                ImGui::SetNextItemWidth(fw - 2);
                ImGui::PushID(i * 4 + side * 2 + m);
                if (dragFloatEdit(doc, "##f", v, 0, "%g", "Edit link range")) {
                    for (auto& l : L) { // every link of this row shares its range
                        if (side == 0 && l.from == i) { if (m == 0) l.fromMin = *v; else l.fromMax = *v; }
                        if (side == 1 && l.to == i) { if (m == 0) l.toMin = *v; else l.toMax = *v; }
                    }
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(cx0, origin.y));
    ImGui::InvisibleButton("##tcgraph", ImVec2(cx1 - cx0, 6 * rowH));
    bool hov = ImGui::IsItemHovered();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    auto linkAt = [&](ImVec2 m) {
        int best = -1; float bd = 6 * u;
        for (int k = 0; k < (int)L.size(); k++) {
            ImVec2 p0, p1, p2, p3; linkPts(L[k], p0, p1, p2, p3);
            for (int s = 0; s <= 24; s++) {
                ImVec2 q = bezierPt(p0, p1, p2, p3, s / 24.0f);
                float dd = std::sqrt((q.x - m.x) * (q.x - m.x) + (q.y - m.y) * (q.y - m.y));
                if (dd < bd) { bd = dd; best = k; }
            }
        }
        return best;
    };
    auto rowAt = [&](float y) { return std::min(5, std::max(0, (int)((y - origin.y) / rowH))); };
    auto onPort = [&](ImVec2 m, bool right, int& row) {
        row = rowAt(m.y);
        ImVec2 d = right ? dotRt(row) : dotL(row);
        return std::fabs(m.x - d.x) < dotR * 2.2f && std::fabs(m.y - d.y) < dotR * 2.2f;
    };
    int hoverLink = hov && pressPort < 0 ? linkAt(mouse) : -1;
    if (edit && hov && ImGui::IsMouseClicked(0)) {
        int row;
        if (onPort(mouse, false, row)) { pressPort = row; pressRight = false; pressPos = mouse; }
        else if (onPort(mouse, true, row)) { pressPort = row; pressRight = true; pressPos = mouse; }
        else sel = linkAt(mouse);
    }
    if (pressPort >= 0 && ImGui::IsMouseReleased(0)) {
        float moved = std::fabs(mouse.x - pressPos.x) + std::fabs(mouse.y - pressPos.y);
        int row = pressPort; bool right = pressRight;
        pressPort = -1;
        if (moved < 4 * u) { // a click on a connected port disconnects it
            bool any = false;
            for (auto& l : c.links) if ((right ? l.to : l.from) == row) any = true;
            if (any) {
                doc.checkpoint("Disconnect");
                c.links.erase(std::remove_if(c.links.begin(), c.links.end(), [&](const TransformLink& l) { return (right ? l.to : l.from) == row; }), c.links.end());
                sel = -1;
            }
            return;
        }
        int other;
        if (onPort(mouse, !right, other)) {
            int from = right ? other : row, to = right ? row : other;
            int found = -1;
            for (int k = 0; k < (int)c.links.size(); k++) if (c.links[k].from == from && c.links[k].to == to) found = k;
            if (found < 0) {
                doc.checkpoint("Connect");
                TransformLink l; l.from = from; l.to = to;
                if (srcLink[from] >= 0) { l.fromMin = c.links[srcLink[from]].fromMin; l.fromMax = c.links[srcLink[from]].fromMax; }
                else tcDefaultRange(from, l.fromMin, l.fromMax);
                if (dstLink[to] >= 0) { l.toMin = c.links[dstLink[to]].toMin; l.toMax = c.links[dstLink[to]].toMax; }
                else tcDefaultRange(to, l.toMin, l.toMax);
                c.links.push_back(l);
                found = (int)c.links.size() - 1;
            }
            sel = found;
        }
        return;
    }

    for (int k = 0; k < (int)L.size(); k++) {
        ImVec2 p0, p1, p2, p3; linkPts(L[k], p0, p1, p2, p3);
        bool on = k == sel || k == hoverLink;
        dl->AddBezierCubic(p0, p1, p2, p3, on ? IM_COL32(70, 220, 90, 255) : IM_COL32(50, 140, 220, 255), (on ? 3 : 2) * u);
    }
    for (int k = 0; k < (int)L.size(); k++) { // the percentage of the link under the pointer or selected
        if (k != sel && k != hoverLink) continue;
        ImVec2 p0, p1, p2, p3; linkPts(L[k], p0, p1, p2, p3);
        float span = L[k].fromMax - L[k].fromMin;
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.0f%%", span != 0 ? (L[k].toMax - L[k].toMin) / span * 100 : 0);
        ImVec2 mid = bezierPt(p0, p1, p2, p3, 0.5f), ts = ImGui::CalcTextSize(buf);
        dl->AddRectFilled(ImVec2(mid.x - ts.x * 0.5f - 4 * u, mid.y - ts.y * 0.5f - 1), ImVec2(mid.x + ts.x * 0.5f + 4 * u, mid.y + ts.y * 0.5f + 1),
                          IM_COL32(40, 170, 60, 255), 4 * u);
        dl->AddText(ImVec2(mid.x - ts.x * 0.5f, mid.y - ts.y * 0.5f), IM_COL32(255, 255, 255, 255), buf);
    }
    for (int i = 0; i < 6; i++) {
        bool usedL = srcLink[i] >= 0, usedR = dstLink[i] >= 0;
        ImU32 col = tcColor(i), dim = (col & 0x00FFFFFF) | 0x70000000;
        dl->AddCircleFilled(dotL(i), dotR, usedL ? col : dim);
        dl->AddCircleFilled(dotRt(i), dotR, usedR ? col : dim);
    }
    if (pressPort >= 0 && ImGui::IsMouseDown(0)) { // rubber band while connecting
        ImVec2 a = pressRight ? dotRt(pressPort) : dotL(pressPort);
        dl->AddBezierCubic(a, ImVec2(a.x + (mouse.x - a.x) * 0.5f, a.y), ImVec2(mouse.x - (mouse.x - a.x) * 0.5f, mouse.y), mouse, IM_COL32(70, 220, 90, 255), 2 * u);
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 6 * rowH + 4 * u));
    ImGui::Dummy(ImVec2(totalW, 1));
}

// One collapsible section of the constraint panel (Offset, Mix, Properties): a header with an optional right-hand button.
static bool tcSection(const char* title) {
    ImGui::PushStyleColor(ImGuiCol_Text, kHeaderText);
    ImGui::PushStyleColor(ImGuiCol_Header, gLightUi ? IM_COL32(205, 207, 212, 255) : theme::hex(theme::kPanel));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, gLightUi ? IM_COL32(195, 197, 202, 255) : theme::hex(theme::kHover));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, gLightUi ? IM_COL32(185, 187, 192, 255) : theme::hex(theme::kHover));
    bool open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleColor(4);
    return open;
}

// Spine groups properties in rows: Rotate, Translate (X and Y), Scale (X and Y), Shear.
static const int kTcGroupFirst[4] = {0, 1, 3, 5}, kTcGroupCount[4] = {1, 2, 2, 1};
static const char* kTcGroupName[4] = {"Rotate", "Translate", "Scale", "Shear"};
static const Icon kTcGroupIcon[4] = {Icon::Rotate, Icon::Translate, Icon::Scale, Icon::Shear};

static void transformConstraintView(Document& doc, TransformConstraintData& c, float u) {
    SkeletonData& d = doc.data;
    float* mixp[6] = {&c.mixRotate, &c.mixX, &c.mixY, &c.mixScaleX, &c.mixScaleY, &c.mixShearY};
    bool srcUsed[6], dstUsed[6];
    for (int i = 0; i < 6; i++) srcUsed[i] = dstUsed[i] = !c.mapped;
    for (auto& l : c.links) if (l.to >= 0 && l.to < 6 && l.from >= 0 && l.from < 6) { dstUsed[l.to] = true; srcUsed[l.from] = true; }

    if (propBegin("##tcsrc", 100 * u)) {
        propLabel(Icon::Point, "Source");
        int t = c.target;
        if (boneCombo(doc, "##tctarget", &t) && t >= 0) { doc.checkpoint("Source"); c.target = t; }
        for (size_t i = 0; i < c.bones.size(); i++) {
            propLabel(Icon::Bone, i == 0 ? "Targets" : "");
            ImGui::PushID((int)i);
            ImGui::TextUnformatted(d.bones[c.bones[i]].name.c_str());
            if (c.bones.size() > 1) {
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) { doc.checkpoint("Remove target"); c.bones.erase(c.bones.begin() + i); ImGui::PopID(); break; }
            }
            ImGui::PopID();
        }
        propLabel(Icon::Bone, "");
        if (ImGui::BeginCombo("##tcadd", "Add target...")) {
            for (size_t i = 0; i < d.bones.size(); i++) {
                if ((int)i == c.target || std::find(c.bones.begin(), c.bones.end(), (int)i) != c.bones.end()) continue;
                if (ImGui::Selectable(d.bones[i].name.c_str())) { doc.checkpoint("Add target"); c.bones.push_back((int)i); }
            }
            ImGui::EndCombo();
        }
        propEnd();
    }

    // Offset belongs to the source properties and Mix to the target properties; a row shows only when it is connected.
    bool anyOffset = false, anyMix = false;
    for (int i = 0; i < 6; i++) { anyOffset |= srcUsed[i]; anyMix |= dstUsed[i]; }
    if (tcSection("Offset")) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40 * u);
        if (ImGui::SmallButton("Match##tcoff")) { doc.checkpoint("Match offsets"); matchTransformOffsets(doc, c); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the offsets so the first target keeps its current pose");
        if (anyOffset && propBegin("##tcoff", 100 * u)) {
            for (int g = 0; g < 4; g++) {
                int n = 0, ids[2];
                for (int k = 0; k < kTcGroupCount[g]; k++) if (srcUsed[kTcGroupFirst[g] + k]) ids[n++] = kTcGroupFirst[g] + k;
                if (!n) continue;
                propLabel(kTcGroupIcon[g], kTcGroupName[g]);
                float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * (n - 1)) / n;
                for (int k = 0; k < n; k++) {
                    ImGui::PushID(ids[k]);
                    if (k) ImGui::SameLine();
                    ImGui::SetNextItemWidth(w);
                    dragFloatEdit(doc, "##o", &c.offsetOf(ids[k]), 0.5f, "%.2f", "Offset");
                    ImGui::PopID();
                }
            }
            propEnd();
        }
    }
    if (tcSection("Mix")) {
        static bool linkSliders = false;
        bool pair = dstUsed[1] && dstUsed[2] || dstUsed[3] && dstUsed[4];
        if (pair) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100 * u);
            ImGui::Checkbox("Link sliders", &linkSliders);
        }
        if (anyMix && propBegin("##tcmix", 100 * u)) {
            for (int g = 0; g < 4; g++) {
                int n = 0, ids[2];
                for (int k = 0; k < kTcGroupCount[g]; k++) if (dstUsed[kTcGroupFirst[g] + k]) ids[n++] = kTcGroupFirst[g] + k;
                if (!n) continue;
                propLabel(kTcGroupIcon[g], kTcGroupName[g]);
                float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * (n - 1)) / n;
                for (int k = 0; k < n; k++) {
                    ImGui::PushID(ids[k]);
                    if (k) ImGui::SameLine();
                    ImGui::SetNextItemWidth(w);
                    if (sliderFloatEdit(doc, "##m", mixp[ids[k]], 0, 1, "%.2f", "Mix") && linkSliders)
                        for (int q = 0; q < n; q++) *mixp[ids[q]] = *mixp[ids[k]];
                    ImGui::PopID();
                }
            }
            propEnd();
        }
    }
    if (tcSection("Properties")) {
        if (!c.mapped) {
            if (ImGui::Button("Edit property links")) { doc.checkpoint("Edit transform links"); c.materialiseLinks(); }
            ImGui::SameLine();
            ImGui::TextDisabled("Classic same-property copy");
        }
        transformLinkGraph(doc, c, u);
    }
    if (propBegin("##tcopt", 100 * u)) {
        propLabel(Icon::Dot, "Options");
        if (c.mapped) {
            checkboxEdit(doc, "Local##src", &c.localSource, "Local source");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("The source's local values are read (otherwise world values)");
            ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
            checkboxEdit(doc, "Local##dst", &c.localTarget, "Local target");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("The targets' local values are changed (otherwise world values)");
            ImGui::SameLine();
            checkboxEdit(doc, "Additive", &c.relative, "Additive");
            ImGui::SameLine();
            checkboxEdit(doc, "Clamp", &c.clamp, "Clamp");
        } else {
            checkboxEdit(doc, "Local", &c.local, "Local");
            ImGui::SameLine();
            checkboxEdit(doc, "Relative", &c.relative, "Relative");
        }
        propEnd();
    }
}

static Icon attachmentTypeIcon(AttachmentType t) { return attachmentIcon(t); }

void Editor::propertiesView() {
    ImGui::Begin("Properties", &showProps);
    SkeletonData& d = doc.data;
    Sel p = doc.primary();
    const float u = uiScale;
    ImGui::PushItemWidth(-90 * u);
    bool pendingDelete = false, pendingDuplicate = false;
    // Header: type icon, "Kind: name" in teal, and duplicate / rename / delete buttons on the right (as in Spine).
    auto header = [&](const char* kind, const std::string& name, Icon ic = Icon::Dot, ImU32 icol = 0) {
        if (!icol) icol = uiTextColor();
        ImVec2 hp = ImGui::GetCursorScreenPos();
        float h = ImGui::GetTextLineHeight();
        drawIcon(ImGui::GetWindowDrawList(), ic, ImVec2(hp.x + h * 0.55f, hp.y + h * 0.55f), h, icol);
        ImGui::SetCursorScreenPos(ImVec2(hp.x + h * 1.3f + 4, hp.y));
        ImGui::TextColored(themed(kHeaderText), "%s: %s", kind, name.c_str());
        float bs = 22 * u;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 3 * bs - 2 * ImGui::GetStyle().ItemSpacing.x);
        if (iconOnlyButton("##dup", Icon::Copy, false, bs, "Duplicate (Ctrl+D)")) pendingDuplicate = true;
        ImGui::SameLine();
        if (iconOnlyButton("##ren", Icon::Adjust, false, bs, "Rename (F2)")) beginRename();
        ImGui::SameLine();
        if (iconOnlyButton("##del", Icon::Delete, false, bs, "Delete (Del)")) pendingDelete = true;
        ImGui::Spacing();
    };
    // Row of Spine-style buttons under the table.
    auto actionButton = [&](const char* id, Icon ic, const char* label, bool enabled = true, const char* tip = nullptr) {
        float w = ImGui::CalcTextSize(label).x + 40 * u;
        // Responsive: a button that does not fit in what is left of the line wraps to the next one.
        if (ImGui::GetContentRegionAvail().x < w && ImGui::GetCursorPosX() > ImGui::GetStyle().WindowPadding.x + 1) ImGui::NewLine();
        ImGui::BeginDisabled(!enabled);
        bool r = iconButton(id, ic, label, false, ImVec2(w, 24 * u), tip);
        ImGui::EndDisabled();
        ImGui::SameLine();
        return r;
    };
    switch (p.type) {
    case SelType::None:
    case SelType::Skeleton: {
        header("Skeleton", doc.projectPath.empty() ? "untitled" : fileName(doc.projectPath), Icon::Skeleton);
        if (propBegin("##skel", 92 * u)) {
            propLabel(Icon::Animations, "FPS");
            dragFloatEdit(doc, "##fps", &d.fps, 0.1f, "%.1f", "FPS");
            if (d.fps < 1) d.fps = 1;
            propLabel(Icon::Fit, "Ref. scale");
            dragFloatEdit(doc, "##refscale", &d.referenceScale, 1, "%.0f", "Reference scale");
            propLabel(Icon::Folder, "Images");
            ImGui::TextWrapped("%s", doc.imagesDir.empty() ? "(not set)" : doc.imagesDir.c_str());
            propEnd();
        }
        if (actionButton("##skimg", Icon::Folder, "Images folder...")) cmdImportImagesFolder();
        if (actionButton("##skopen", Icon::Folder, "Open folder", !doc.imagesDir.empty()) && !doc.imagesDir.empty()) openInExplorer(doc.imagesDir);
        ImGui::NewLine();
        ImGui::TextDisabled("%zu bones, %zu slots, %zu skins, %zu animations", d.bones.size(), d.slots.size(), d.skins.size(), d.animations.size());
        if (actionButton("##sanim", Icon::Animations, "Animation")) newAnimation();
        if (actionButton("##sskin", Icon::Skins, "Skin")) doc.createSkin("skin");
        if (actionButton("##sevent", Icon::Events, "Event")) doc.createEvent("event");
        ImGui::NewLine();
        break;
    }
    case SelType::Bone: {
        BoneData& b = d.bones[p.index];
        ImU32 boneCol = ImGui::ColorConvertFloat4ToU32(ImVec4(b.color.r, b.color.g, b.color.b, 1));
        header("Bone", b.name, Icon::Bone, boneCol);
        bool setup = doc.mode == Mode::Setup;
        if (!setup) ImGui::TextDisabled("Animated values at frame %d are shown in the toolbar.", (int)std::round(doc.time * d.fps));
        ImGui::BeginDisabled(!setup);
        if (propBegin("##bone", 92 * u)) {
            propLabel(Icon::Parent, "Inherit");
            inheritEditor(doc, b);
            propLabel(Icon::ShowBones, "Icon");
            {
                // The bone's icon: shown for the bone in the viewport and the tree. Applies to every selected bone.
                if (ImGui::Button("##boneicon", ImVec2(34 * u, 24 * u))) ImGui::OpenPopup("boneiconpicker");
                ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                drawBoneIcon(ImGui::GetWindowDrawList(), b.icon, ImVec2((mn.x + mx.x) / 2, (mn.y + mx.y) / 2), 16 * u, uiTextColor());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose the icon this bone is shown with");
                if (ImGui::BeginPopup("boneiconpicker")) {
                    const std::vector<std::string>& names = boneIconNames();
                    const float cell = 30 * u;
                    for (size_t i = 0; i < names.size(); i++) {
                        if (i % 8) ImGui::SameLine(0, 2 * u);
                        ImGui::PushID((int)i);
                        bool current = (isDefaultBoneIcon(b.icon) && i == 0) || b.icon == names[i];
                        ImGui::PushStyleColor(ImGuiCol_Button, current ? ImGui::GetStyle().Colors[ImGuiCol_ButtonActive] : ImVec4(0, 0, 0, 0));
                        bool pick = ImGui::Button("##ic", ImVec2(cell, cell));
                        ImGui::PopStyleColor();
                        ImVec2 imn = ImGui::GetItemRectMin(), imx = ImGui::GetItemRectMax();
                        drawBoneIcon(ImGui::GetWindowDrawList(), names[i], ImVec2((imn.x + imx.x) / 2, (imn.y + imx.y) / 2), 18 * u, uiTextColor());
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", names[i].c_str());
                        if (pick) {
                            doc.checkpoint("Bone icon");
                            for (int sb : doc.selectedBones()) doc.data.bones[sb].icon = i == 0 ? "" : names[i];
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndPopup();
                }
                // The length sits beside the icon, and a switch shows the bone as a plain line instead of its shape.
                ImGui::SameLine(0, 8 * u);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Length");
                ImGui::SameLine(0, 6 * u);
                ImGui::SetNextItemWidth(78 * u);
                dragFloatEdit(doc, "##len", &b.length, 0.5f, "%.5g", "Length");
                ImGui::SameLine(0, 8 * u);
                ImGui::PushStyleColor(ImGuiCol_Button, b.line ? ImVec4(0.071f, 0.302f, 0.678f, 1) : ImGui::GetStyle().Colors[ImGuiCol_Button]);
                bool toggleLine = ImGui::Button("##linebone", ImVec2(34 * u, 24 * u));
                ImGui::PopStyleColor();
                ImVec2 lmn = ImGui::GetItemRectMin(), lmx = ImGui::GetItemRectMax();
                ImDrawList* ldl = ImGui::GetWindowDrawList();
                ImVec2 la(lmn.x + 9 * u, lmx.y - 6 * u), lb(lmx.x - 9 * u, lmn.y + 6 * u);
                ldl->AddLine(la, lb, uiTextColor(), 2 * u);
                ldl->AddCircleFilled(la, 2.6f * u, uiTextColor());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(b.line ? "Line bone: on (shown as a plain line)" : "Line bone: off (shown as the bone shape)");
                if (toggleLine) {
                    doc.checkpoint("Line bone");
                    bool nv = !b.line;
                    for (int sb : doc.selectedBones()) doc.data.bones[sb].line = nv;
                }
                // Thickness: how thick the selected bones (or their icon) are drawn.
                ImGui::SetNextItemWidth(-FLT_MIN);
                float thick = b.thickness;
                bool slid = sliderFloat("##bthick", &thick, 20, 300, "Thickness %.0f%%");
                if (sliderActivated()) doc.checkpoint("Bone thickness");
                if (slid) {
                    for (int sb : doc.selectedBones()) doc.data.bones[sb].thickness = thick;
                    doc.dirty = true;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("How thick the bone is drawn (display only)");
            }
            propLabel(Icon::Color, "Color");
            colorEdit(doc, "##bcol", b.color, true, "Bone color");
            propLabel(Icon::Slot, "Options");
            checkboxEdit(doc, "Select", &b.selectable, "Bone selectable");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unchecked: this bone cannot be picked in the viewport (it can still be selected in the tree)");
            ImGui::SameLine();
            checkboxEdit(doc, "Name", &b.showName, "Bone name");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Always draw this bone's name in the viewport");
            propLabel(Icon::Skins, "Skin");
            checkboxEdit(doc, "Skin required", &b.skinRequired, "Skin required");
            ImGui::SameLine();
            skinMembership(doc, "##bskins", p.index, {});
            propEnd();
        }
        ImGui::EndDisabled();
        const Bone& rb = doc.sk.bones[p.index];
        ImGui::TextDisabled("World: x %.2f  y %.2f  rot %.2f  scale %.3f, %.3f", rb.worldX, rb.worldY, rb.worldRotationX(), rb.worldScaleX(), rb.worldScaleY());
        if (setup) {
            std::vector<int> sel = doc.selectedBones();
            if (actionButton("##bnew", Icon::New, "New...", true, "New child bone, slot or constraint")) ImGui::OpenPopup("newbone");
            if (actionButton("##bparent", Icon::Parent, "Parent", p.index > 0, "Pick a new parent bone (P)")) { pickParent = true; setStatus("Click the new parent bone (Esc cancels)"); }
            if (actionButton("##bsplit", Icon::Split, "Split", p.index >= 0 && doc.data.bones[p.index].length > 0, "Split the bone into smaller bones")) ImGui::OpenPopup("##splitpop");
            if (ImGui::BeginPopup("##splitpop")) {
                static int parts = 2;
                static bool nested = true, fib = false;
                ImGui::SetNextItemWidth(120 * u);
                ImGui::InputInt("Pieces", &parts);
                parts = std::max(2, std::min(parts, 32));
                ImGui::Checkbox("Nested", &nested);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Each new bone is a child of the previous one. Unchecked: they all share the original's parent");
                ImGui::Checkbox("Fibonacci", &fib);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lengths decrease along the Fibonacci sequence. Unchecked: equal lengths");
                if (ImGui::Button("Split")) { doc.splitBone(p.index, parts, nested, fib); ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }
            ImGui::NewLine();
            // The New... menu, as in Spine: bone, slot, attachments, then constraints. Rounded, roomy, accent hover.
            ImGui::SetNextWindowSizeConstraints(ImVec2(236 * u, 0), ImVec2(1e9f, 1e9f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6 * u, 6 * u));
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10 * u);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2 * u));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
            if (ImGui::BeginPopup("newbone")) {
                std::vector<int> target = sel.empty() ? std::vector<int>{p.index} : sel;
                auto newSlotWith = [&](AttachmentType t, const char* name) {
                    doc.checkpoint(std::string("New ") + name);
                    int si = doc.createSlot(p.index, name);
                    doc.addAttachment(si, t, name);
                };
                if (newMenuItem(Icon::Bone, "Bone", true, "An empty child bone at this bone's tip, shown as a crosshair")) {
                    Vec2 tip = rb.localToWorld({d.bones[p.index].length, 0});
                    doc.createBone(p.index, tip, tip); // zero length: no shape, drawn as its icon
                }
                if (newMenuItem(Icon::Slot, "Slot")) { doc.checkpoint("New slot"); doc.selection = {{SelType::Slot, doc.createSlot(p.index, b.name), ""}}; }
                if (newMenuItem(Icon::Skins, "Skin Placeholder", true, "A placeholder on a new slot that skins fill with their own attachment")) {
                    doc.checkpoint("New slot");
                    int ps = doc.createSlot(p.index, "slot");
                    doc.createPlaceholder(ps);
                }
                if (newMenuItem(Icon::BoundingBox, "Bounding Box", true, "A new slot on this bone with a bounding box")) newSlotWith(AttachmentType::BoundingBox, "boundingbox");
                if (newMenuItem(Icon::Clipping, "Clipping", true, "A new slot on this bone with a clipping polygon")) newSlotWith(AttachmentType::Clipping, "clipping");
                if (newMenuItem(Icon::Path, "Path", true, "A new slot on this bone with a path")) newSlotWith(AttachmentType::Path, "path");
                if (newMenuItem(Icon::Point, "Point", true, "A new slot on this bone with a point")) newSlotWith(AttachmentType::Point, "point");
                ImGui::Separator();
                if (newMenuItem(Icon::Ik, "IK Constraint", true, "Click the target bone on the canvas")) {
                    pickConstraint = 1;
                    setStatus("Click the IK target bone (Esc cancels)");
                }
                if (newMenuItem(Icon::PathCon, "Path Constraint", true, "Constrain the bone(s) to a path attachment")) {
                    std::vector<int> pathSlots; // slots that hold a path attachment in the active skin
                    if (doc.skin >= 0 && doc.skin < (int)d.skins.size())
                        for (auto& [slot, atts] : d.skins[doc.skin].attachments)
                            for (auto& [n, a] : atts) if (a.type == AttachmentType::Path) { pathSlots.push_back(slot); break; }
                    if (pathSlots.empty()) setStatus("Add a Path first (New > Path)");
                    else if (pathSlots.size() == 1) doc.createPath("path", target, pathSlots[0]);
                    else ImGui::OpenPopup("pathtarget");
                }
                if (newMenuItem(Icon::PhysicsCon, "Physics Constraint")) doc.createPhysics("physics", p.index);
                if (newMenuItem(Icon::Adjust, "Slider", true, "A slider constraint; choose its animation in the Properties panel")) doc.createSlider("slider", doc.animation);
                if (newMenuItem(Icon::TransformCon, "Transform Constraint", true, "Click the target bone on the canvas")) {
                    pickConstraint = 2;
                    setStatus("Click the transform target bone (Esc cancels)");
                }
                if (ImGui::BeginPopup("pathtarget")) { // more than one path: choose which slot's path
                    for (auto& [slot, atts] : d.skins[doc.skin].attachments) {
                        bool hasPath = false;
                        for (auto& [n, a] : atts) hasPath |= a.type == AttachmentType::Path;
                        if (hasPath && newMenuItem(Icon::Path, d.slots[slot].name.c_str())) doc.createPath("path", target, slot);
                    }
                    ImGui::EndPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(3);
        }
        break;
    }
    case SelType::Slot: {
        SlotData& s = d.slots[p.index];
        header("Slot", s.name, Icon::Slot);
        bool setup = doc.mode == Mode::Setup;
        if (propBegin("##slot", 92 * u)) {
            propLabel(Icon::Bone, "Bone");
            int bone = s.bone;
            ImGui::BeginDisabled(!setup);
            if (boneCombo(doc, "##slotbone", &bone)) doc.reparentSlot(p.index, bone);
            ImGui::EndDisabled();
            propLabel(Icon::Image, "Attachment");
            std::string cur = doc.sk.slots[p.index].attachment;
            if (ImGui::BeginCombo("##slotatt", cur.empty() ? "(none)" : cur.c_str())) {
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
            propLabel(Icon::Color, "Color");
            Color c = setup ? s.color : doc.sk.slots[p.index].color;
            if (colorEdit(doc, "##slotcol", c, true, setup ? "Slot color" : "Key color")) doc.setSlotColor(p.index, c);
            if (setup) {
                propLabel(Icon::Color, "Dark");
                if (checkboxEdit(doc, "Tint black", &s.hasDark, "Tint black") && s.hasDark) s.dark = Color(0, 0, 0, 1);
                if (s.hasDark) colorEdit(doc, "##slotdark", s.dark, false, "Dark color");
                propLabel(Icon::Fit, "Blending");
                const char* blends[] = {"Normal", "Additive", "Multiply", "Screen"};
                int bm = (int)s.blend;
                if (ImGui::Combo("##slotblend", &bm, blends, 4)) { doc.checkpoint("Blend mode"); s.blend = (BlendMode)bm; }
            }
            propEnd();
        }
        if (actionButton("##snew", Icon::New, "New...", setup, "New attachment")) ImGui::OpenPopup("newatt");
        if (actionButton("##sparent", Icon::Parent, "Parent", setup, "Pick a new parent bone (P)")) { pickParent = true; setStatus("Click the new parent bone (Esc cancels)"); }
        if (doc.mode == Mode::Animate) {
            if (actionButton("##skeyc", Icon::Key, "Key color")) { doc.checkpoint("Key"); doc.keySlotColor(p.index); }
            if (actionButton("##skeya", Icon::Key, "Key attachment")) { doc.checkpoint("Key"); doc.keySlotAttachment(p.index); }
        }
        ImGui::NewLine();
        if (ImGui::BeginPopup("newatt")) {
            if (ImGui::BeginMenu("Image", !doc.imageNames.empty())) {
                for (auto& img : doc.imageNames)
                    if (ImGui::MenuItem(img.c_str())) {
                        doc.checkpoint("New attachment");
                        const Bone& bb = doc.sk.bones[s.bone];
                        doc.addRegionAttachment(p.index, img, {bb.worldX, bb.worldY});
                    }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Bounding box")) doc.addAttachment(p.index, AttachmentType::BoundingBox, "boundingbox");
            if (ImGui::MenuItem("Clipping")) doc.addAttachment(p.index, AttachmentType::Clipping, "clipping");
            if (ImGui::MenuItem("Path")) doc.addAttachment(p.index, AttachmentType::Path, "path");
            if (ImGui::MenuItem("Point")) doc.addAttachment(p.index, AttachmentType::Point, "point");
            ImGui::EndPopup();
        }
        break;
    }
    case SelType::Attachment: {
        int si = p.index;
        Attachment* a = selectedAttachment(&si);
        if (!a && doc.isPlaceholder(p.index, p.name)) {
            header("Skin Placeholder", p.name, Icon::Skins, IM_COL32(250, 150, 60, 255));
            ImGui::TextWrapped("Skins fill this placeholder with their own attachment. Pick a skin, then add an image: it is stored in that skin under this name.");
            ImGui::BeginDisabled(doc.imageNames.empty() || doc.skin <= 0);
            if (actionButton("##phimg", Icon::Image, "Add Image", true, "Add an image to the active skin under this placeholder's name")) ImGui::OpenPopup("##phimgs");
            ImGui::EndDisabled();
            if (doc.skin <= 0) ImGui::TextDisabled("Select a skin other than default in the Skins list first.");
            if (ImGui::BeginPopup("##phimgs")) {
                for (auto& img : doc.imageNames)
                    if (ImGui::MenuItem(img.c_str())) {
                        doc.checkpoint("Fill placeholder");
                        const Bone& pb = doc.sk.bones[d.slots[p.index].bone];
                        doc.addRegionAttachment(p.index, img, {pb.worldX, pb.worldY}, p.name, true);
                    }
                ImGui::EndPopup();
            }
            break;
        }
        if (!a) break;
        std::string kind = attachmentTypeName(a->type);
        kind[0] = (char)std::toupper((unsigned char)kind[0]);
        header(kind.c_str(), a->name, attachmentTypeIcon(a->type), IM_COL32(120, 210, 220, 255));
        bool setup = doc.mode == Mode::Setup;
        bool imageType = a->type == AttachmentType::Region || a->type == AttachmentType::Mesh || a->type == AttachmentType::LinkedMesh;
        if (imageType) {
            ImGui::BeginDisabled(!setup);
            if (propBegin("##att", 92 * u)) {
                propLabel(Icon::Folder, "Image path");
                const std::string ip = a->imagePath();
                // A blank path means the attachment's name finds the image, shown as <name>.
                std::string preview = a->path.empty() ? "<" + a->name + ">" : a->path;
                if (ImGui::BeginCombo("##img", preview.c_str())) {
                    for (auto& img : doc.imageNames)
                        if (ImGui::Selectable(img.c_str(), img == ip)) {
                            doc.checkpoint("Image path");
                            a->path = img == a->name ? "" : img;
                            int w, h;
                            if (a->type == AttachmentType::Region && loadImageFileSize(doc.imageFileFor(img), w, h)) { a->width = (float)w; a->height = (float)h; }
                        }
                    ImGui::EndCombo();
                }
                // Options: Select / Export / Name, then Mesh / Wireframe / Sequence.
                propLabel(Icon::Slot, "Options");
                const bool isMesh = a->type != AttachmentType::Region;
                bool convertBack = false, convertTo = false;
                // Three columns when there is room, fewer as the panel gets narrow, so the labels stay readable.
                const float optW = ImGui::GetContentRegionAvail().x;
                const int optCols = optW >= 270 * u ? 3 : optW >= 170 * u ? 2 : 1;
                if (ImGui::BeginTable("##opts", optCols, ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextColumn(); checkboxEdit(doc, "Select", &a->selectable, "Selectable");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unchecked: this image cannot be picked by clicking it in the viewport");
                    ImGui::TableNextColumn(); checkboxEdit(doc, "Export", &a->exportable, "Export");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unchecked: left out of exported data");
                    ImGui::TableNextColumn(); checkboxEdit(doc, "Name", &a->showName, "Show name");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Draw the attachment's name in the viewport");
                    ImGui::TableNextColumn();
                    bool meshFlag = isMesh;
                    ImGui::BeginDisabled(a->type == AttachmentType::LinkedMesh);
                    if (ImGui::Checkbox("Mesh", &meshFlag)) { if (meshFlag) convertTo = true; else convertBack = true; }
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unchecked: the mesh turns back into a region attachment");
                    ImGui::TableNextColumn();
                    ImGui::BeginDisabled(!isMesh);
                    checkboxEdit(doc, "Wireframe", &a->wireframe, "Wireframe");
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Draw the mesh's vertices and edges even when it is not selected");
                    ImGui::TableNextColumn();
                    sequenceCheckbox(*this, a);
                    ImGui::EndTable();
                }
                if (convertTo) doc.convertToMesh(si, a->name);
                if (convertBack) { doc.convertToRegion(si, a->name); a = selectedAttachment(&si); if (!a) { propEnd(); ImGui::EndDisabled(); break; } }
                if (isMesh && a->type == AttachmentType::Mesh) { ImGui::TextDisabled("%d vertices, %s", a->vertexCount(), a->isWeighted() ? "weighted" : "unweighted"); }
                propLabel(Icon::Color, "Color");
                colorEdit(doc, "##acol", a->color, true, "Attachment color");
                if (a->type == AttachmentType::LinkedMesh) {
                    propLabel(Icon::LinkedMesh, "Linked to");
                    ImGui::Text("%s%s%s", a->parentMesh.c_str(), a->parentSkin.empty() ? "" : " in skin ", a->parentSkin.c_str());
                    propLabel(Icon::Deform, "Timelines");
                    checkboxEdit(doc, "Inherit deform", &a->inheritTimelines, "Inherit timelines");
                }
                propEnd();
            }
            ImGui::EndDisabled();
            if (a->type == AttachmentType::Region && doc.imagePathFor(*a).empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Image not found in the images folder");
            sequenceEditor(*this, si, a);
            // New... and Set Parent on a row of their own above the mesh buttons; every row wraps when the panel is narrow.
            if (actionButton("##anew", Icon::New, "New...", setup, "Convert to mesh, or create a linked mesh")) ImGui::OpenPopup("newmesh");
            if (actionButton("##aparent", Icon::Parent, "Set Parent", setup, "Pick a new parent bone (P)")) { pickParent = true; setStatus("Click the new parent bone (Esc cancels)"); }
            ImGui::NewLine();
            if (a->type == AttachmentType::Mesh) {
                if (actionButton("##aedit", Icon::Mesh, "Edit Mesh")) { showMeshTools = true; meshMode = MeshMode::Modify; }
                if (actionButton("##afreeze", Icon::Lock, "Freeze", setup, "Set the mesh's rotation to 0 and scale to 1 at the current vertex positions")) {
                    doc.checkpoint("Freeze mesh");
                    a->rotation = 0; a->scaleX = a->scaleY = 1; a->x = a->y = 0;
                    setStatus("Mesh frozen: rotation 0, scale 1");
                }
                if (doc.mode == Mode::Animate) {
                    // Deform keys: drag vertices to move them, then key (or let Auto Key key them).
                    bool pend = doc.pendingDeform.count({si, a->name}) > 0;
                    if (actionButton("##akeyd", Icon::Key, "Key deform", pend, "Key the moved vertices at this frame"))
                        { doc.checkpoint("Key deform"); doc.keyDeform(si, a->name); }
                    if (actionButton("##aresetd", Icon::Sync, "Reset", true, "Put the vertices back to the setup pose at this frame"))
                        { doc.checkpoint("Reset deform"); doc.setDeform(si, a->name, std::vector<float>(a->deformLength(), 0.0f)); }
                } else if (actionButton("##areset", Icon::Sync, "Reset", true, "Snap the vertices to the UVs, removing any deformation, the weights and the deform keys")) {
                    doc.checkpoint("Reset mesh");
                    a = selectedAttachment(&si);
                    doc.dropMeshBindings(si, *a);
                    float corners[8];
                    meshImageCorners(*a, corners);
                    double rx[3], ry[3];
                    if (fitAffine(std::vector<float>{0, 1, 0, 0, 1, 0, 1, 1}, std::vector<float>(corners, corners + 8), 4, rx, ry)) {
                        for (int k = 0; k < a->vertexCount(); k++) {
                            double uu = a->uvs[k * 2], vv = a->uvs[k * 2 + 1];
                            a->vertices[k * 2] = (float)(rx[0] * uu + rx[1] * vv + rx[2]);
                            a->vertices[k * 2 + 1] = (float)(ry[0] * uu + ry[1] * vv + ry[2]);
                        }
                    }
                    setStatus("Vertices snapped to the UVs");
                }
                if (actionButton("##aweights", Icon::Weights, "Weights")) { showWeights = true; doc.tool = Tool::Weights; ImGui::SetWindowFocus("Weights"); }
            }
            ImGui::NewLine();
            if (ImGui::BeginPopup("newmesh")) {
                if (a->type == AttachmentType::Region && ImGui::MenuItem("Convert to mesh")) doc.convertToMesh(si, a->name);
                if (a->type == AttachmentType::Mesh && ImGui::MenuItem("Linked mesh")) {
                    doc.createLinkedMesh(si, a->name);
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    break;
                }
                if (a->type == AttachmentType::Region && ImGui::MenuItem("Reset size")) {
                    int w, h;
                    if (loadImageFileSize(doc.imagePathFor(*a), w, h)) { doc.checkpoint("Reset size"); a->width = (float)w; a->height = (float)h; }
                }
                ImGui::EndPopup();
            }
            break;
        }
        ImGui::BeginDisabled(!setup);
        switch (a->type) {
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
            ImGui::Text("Points: %d", a->vertexCount() / 3);
            if (ImGui::Button("Edit path")) { showMeshTools = true; meshMode = MeshMode::Create; }
            ImGui::TextDisabled("Create: click to add points (drag for Bezier handles), click the curve to insert one, click the first point to close. Modify: drag points and handles.");
            break;
        case AttachmentType::Point: {
            float t[2] = {a->x, a->y};
            if (dragFloat2Edit(doc, "Translate", t, 0.5f, "%.2f", "Move point")) { a->x = t[0]; a->y = t[1]; }
            dragFloatEdit(doc, "Rotation", &a->rotation, 0.5f, "%.2f", "Rotate point");
            break;
        }
        default: break;
        }
        ImGui::EndDisabled();
        if (actionButton("##aparent2", Icon::Parent, "Parent", setup, "Pick a new parent bone (P)")) { pickParent = true; setStatus("Click the new parent bone (Esc cancels)"); }
        ImGui::NewLine();
        break;
    }
    case SelType::Ik: {
        IkConstraintData& c = d.ik[p.index];
        header("IK Constraint", c.name, Icon::Ik, IM_COL32(250, 140, 60, 255));
        bool setup = doc.mode == Mode::Setup;
        bool two = c.bones.size() == 2;
        if (setup) {
            if (propBegin("##ik", 100 * u)) {
                propLabel(Icon::Bone, two ? "Parent" : "Bone");
                ImGui::TextUnformatted(d.bones[c.bones[0]].name.c_str());
                if (two) {
                    propLabel(Icon::Bone, "Child");
                    ImGui::TextUnformatted(d.bones[c.bones[1]].name.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X##ikchild")) { // dropping the child makes it a one-bone constraint
                        doc.checkpoint("IK one bone");
                        c.bones.resize(1);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove the child: a one-bone IK constraint");
                }
                propLabel(Icon::Point, "Target");
                int t = c.target;
                if (boneCombo(doc, "##iktarget", &t) && t >= 0) {
                    bool bad = false; // the target may not be a constrained bone or below one
                    for (int a = t; a >= 0; a = d.bones[a].parent) bad |= std::find(c.bones.begin(), c.bones.end(), a) != c.bones.end();
                    if (bad) setStatus("The IK target cannot be a constrained bone or one of its children");
                    else { doc.checkpoint("IK target"); c.target = t; }
                }
                if (two) {
                    bool bend = c.bendDirection > 0;
                    if (checkboxEdit(doc, "Positive##ikbend", &bend, "Bend direction")) c.bendDirection = bend ? 1 : -1;
                } else {
                    checkboxEdit(doc, "Compress##ikc", &c.compress, "IK compress");
                }
                checkboxEdit(doc, "Stretch##iks", &c.stretch, "IK stretch");
                ImGui::SameLine();
                checkboxEdit(doc, "Uniform##iku", &c.uniform, "IK uniform");
                if (two) {
                    checkboxEdit(doc, "Compress##ikc", &c.compress, "IK compress");
                    propLabel(Icon::Dot, "Softness");
                    dragFloatEdit(doc, "##iksoft", &c.softness, 0.5f, "%.1f", "IK softness");
                }
                propLabel(Icon::Dot, "Mix");
                sliderFloat("##ikmix", &c.mix, 0, 1);
                if (sliderActivated()) doc.beginEdit("IK mix");
                if (sliderDeactivatedAfterEdit()) doc.commitEdit();
                propLabel(Icon::Dot, "Order");
                if (ImGui::InputInt("##ikorder", &c.order)) doc.dirty = true;
                propLabel(Icon::Skins, "Skin");
                checkboxEdit(doc, "Skin required", &c.skinRequired, "Skin required");
                ImGui::SameLine();
                skinMembership(doc, "##ikskins", -1, c.name);
                propEnd();
            }
        } else {
            // Positive, Compress, Stretch, Softness and Mix are animatable: one key stores all five, as in Spine.
            IkConstraint& rc = doc.sk.ik[p.index];
            Animation* an = doc.currentAnimation();
            auto keyIk = [&](const IkConstraint& v) {
                if (!an) return;
                doc.checkpoint("Key IK");
                Key& k = an->get(TimelineType::Ik, p.index).setKey(std::round(doc.time * d.fps) / d.fps);
                k.v[0] = v.mix; k.v[1] = v.softness; k.flag1 = v.bendDirection > 0; k.flag2 = v.compress; k.flag3 = v.stretch;
            };
            IkConstraint v = rc;
            bool edited = false;
            if (propBegin("##ikanim", 100 * u)) {
                propLabel(Icon::Point, "Target");
                ImGui::TextUnformatted(d.bones[c.target].name.c_str());
                if (two) {
                    bool bend = v.bendDirection > 0;
                    if (ImGui::Checkbox("Positive##ikbend", &bend)) { v.bendDirection = bend ? 1 : -1; edited = true; }
                } else {
                    edited |= ImGui::Checkbox("Compress##ikc", &v.compress);
                }
                edited |= ImGui::Checkbox("Stretch##iks", &v.stretch);
                if (two) {
                    edited |= ImGui::Checkbox("Compress##ikc", &v.compress);
                    propLabel(Icon::Dot, "Softness");
                    edited |= ImGui::DragFloat("##iksoft", &v.softness, 0.5f, 0, 10000, "%.1f");
                }
                propLabel(Icon::Dot, "Mix");
                edited |= sliderFloat("##ikmix", &v.mix, 0, 1);
                propEnd();
            }
            if (edited && an) keyIk(v); // edits in Animate mode key immediately
            if (actionButton("##ikkey", Icon::Key, "Key", an != nullptr, "Key Positive, Compress, Stretch, Softness and Mix at the current time")) keyIk(rc);
            ImGui::NewLine();
        }
        break;
    }
    case SelType::Transform: {
        TransformConstraintData& c = d.transforms[p.index];
        header("Transform Constraint", c.name, Icon::TransformCon, IM_COL32(220, 220, 90, 255));
        transformConstraintView(doc, c, u);
        if (propBegin("##tc", 100 * u)) {
            propLabel(Icon::Dot, "Order");
            if (ImGui::InputInt("##tcorder", &c.order)) doc.dirty = true;
            propLabel(Icon::Skins, "Skin");
            checkboxEdit(doc, "Skin required", &c.skinRequired, "Skin required");
            ImGui::SameLine();
            skinMembership(doc, "##tcskins", -1, c.name);
            propEnd();
        }
        break;
    }
    case SelType::Path: {
        PathConstraintData& c = d.paths[p.index];
        header("Path Constraint", c.name, Icon::PathCon, IM_COL32(250, 150, 50, 255));
        if (propBegin("##pc", 100 * u)) {
            propLabel(Icon::Slot, "Target slot");
            ImGui::TextUnformatted(d.slots[c.target].name.c_str());
            const char* pm[] = {"Fixed", "Percent"};
            const char* sm[] = {"Length", "Fixed", "Percent", "Proportional"};
            const char* rm[] = {"Tangent", "Chain", "Chain scale"};
            int a = (int)c.positionMode, b = (int)c.spacingMode, r = (int)c.rotateMode;
            propLabel(Icon::Dot, "Position mode");
            if (ImGui::Combo("##pcpm", &a, pm, 2)) { doc.checkpoint("Path"); c.positionMode = (PathConstraintData::Position)a; }
            propLabel(Icon::Dot, "Spacing mode");
            if (ImGui::Combo("##pcsm", &b, sm, 4)) { doc.checkpoint("Path"); c.spacingMode = (PathConstraintData::Spacing)b; }
            propLabel(Icon::Dot, "Rotate mode");
            if (ImGui::Combo("##pcrm", &r, rm, 3)) { doc.checkpoint("Path"); c.rotateMode = (PathConstraintData::Rotate)r; }
            propLabel(Icon::Dot, "Position");
            dragFloatEdit(doc, "##pcpos", &c.position, 0.01f, "%.3f", "Path position");
            propLabel(Icon::Dot, "Spacing");
            dragFloatEdit(doc, "##pcsp", &c.spacing, 0.1f, "%.2f", "Path spacing");
            propLabel(Icon::Rotate, "Rotation offset");
            dragFloatEdit(doc, "##pcrot", &c.offsetRotation, 0.5f, "%.1f", "Path rotation");
            propLabel(Icon::Dot, "Mix R / X / Y");
            float m[3] = {c.mixRotate, c.mixX, c.mixY};
            if (ImGui::DragFloat3("##pcmix", m, 0.01f)) { c.mixRotate = m[0]; c.mixX = m[1]; c.mixY = m[2]; doc.dirty = true; }
            if (ImGui::IsItemActivated()) doc.beginEdit("Path mix");
            if (ImGui::IsItemDeactivatedAfterEdit()) doc.commitEdit();
            propLabel(Icon::Dot, "Order");
            if (ImGui::InputInt("##pcorder", &c.order)) doc.dirty = true;
            propLabel(Icon::Skins, "Skin");
            checkboxEdit(doc, "Skin required", &c.skinRequired, "Skin required");
            ImGui::SameLine();
            skinMembership(doc, "##pcskins", -1, c.name);
            propEnd();
        }
        break;
    }
    case SelType::Physics: {
        PhysicsConstraintData& c = d.physics[p.index];
        header("Physics Constraint", c.name, Icon::PhysicsCon, IM_COL32(120, 190, 250, 255));
        const bool animate = doc.mode == Mode::Animate;
        Animation* an = doc.currentAnimation();
        const PhysicsConstraint* rc = p.index < (int)doc.sk.physics.size() ? &doc.sk.physics[p.index] : nullptr;
        // Warnings, as Spine shows on the constraint's tree icon.
        {
            float len = c.bone >= 0 && c.bone < (int)d.bones.size() ? d.bones[c.bone].length : 0;
            if (len <= 0 && (c.rotate > 0 || c.scaleX > 0 || c.shearX > 0))
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1), "Invalid bone length: Rotate, Scale X or Shear X needs a bone with length.");
            if (c.x <= 0 && c.y <= 0 && c.rotate <= 0 && c.scaleX <= 0 && c.shearX <= 0)
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1), "Nothing active: Translate X, Y, Rotate, Scale X and Shear X are all 0.");
        }
        if (propBegin("##phys", 100 * u)) {
            propLabel(Icon::Bone, "Bone");
            int b = c.bone;
            if (boneCombo(doc, "##physbone", &b) && b >= 0) { doc.checkpoint("Physics bone"); c.bone = b; doc.simInvalid = true; }
            // Setup values (not keyed): how much each property is affected, the speed limit and the update rate.
            ImGui::BeginDisabled(animate);
            propLabel(Icon::Translate, "Translate X");
            sliderFloatEdit(doc, "##phystx", &c.x, 0, 1, "%.2f", "Physics");
            propLabel(Icon::Translate, "Translate Y");
            sliderFloatEdit(doc, "##physty", &c.y, 0, 1, "%.2f", "Physics");
            propLabel(Icon::Rotate, "Rotate");
            sliderFloatEdit(doc, "##physr", &c.rotate, 0, 1, "%.2f", "Physics");
            propLabel(Icon::Scale, "Scale X");
            sliderFloatEdit(doc, "##physsx", &c.scaleX, 0, 1, "%.2f", "Physics");
            propLabel(Icon::Shear, "Shear X");
            sliderFloatEdit(doc, "##physsh", &c.shearX, 0, 1, "%.2f", "Physics");
            propLabel(Icon::Dot, "Limit");
            sliderFloatEdit(doc, "##physlim", &c.limit, 0, 10000, "%.0f", "Physics");
            propLabel(Icon::Dot, "FPS");
            float fps = c.step > 0 ? 1 / c.step : 60;
            if (sliderFloatEdit(doc, "##physfps", &fps, 1, 240, "%.0f", "Physics")) c.step = 1 / std::max(1.0f, fps);
            ImGui::EndDisabled();

            // Keyable properties: a slider with a typed-value box, a key button, and a Global checkbox.
            auto row = [&](const char* label, Icon ic, const char* id, float* field, float runtime, bool mass, TimelineType type, bool* global, float lo, float hi, const char* fmt) {
                propLabel(ic, label);
                ImGui::PushID(id);
                const float tail = 26 * u + 4 * u + (global ? 52 * u : 0);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - tail);
                auto inv = [](float x) { return x > 1e-6f ? 1 / x : 1e6f; };
                float v = animate ? (mass ? inv(runtime) : runtime) : (mass ? inv(*field) : *field);
                int target = global && *global ? -1 : p.index;
                bool changed = sliderFloat("##v", &v, lo, hi, fmt);
                if (sliderActivated()) doc.beginEdit(animate ? "Key physics" : "Physics");
                if (changed) {
                    if (animate) { if (an) doc.keyPhysics(type, target, v); }
                    else { *field = mass ? 1 / std::max(v, 1e-3f) : v; doc.dirty = true; doc.simInvalid = true; }
                }
                if (sliderDeactivated()) { if (sliderDeactivatedAfterEdit()) doc.commitEdit(); else doc.cancelEdit(); }
                ImGui::SameLine(0, 4 * u);
                bool keyed = animate && an && doc.hasPhysicsKey(type, target);
                ImGui::BeginDisabled(!animate || !an);
                if (iconOnlyButton("##key", Icon::Key, keyed, 26 * u, animate ? "Key this property at the current frame" : "Switch to Animate mode to key it")) {
                    doc.checkpoint("Key physics");
                    doc.keyPhysics(type, target, v);
                }
                ImGui::EndDisabled();
                if (global) {
                    ImGui::SameLine(0, 4 * u);
                    bool g = *global;
                    if (ImGui::Checkbox("##glob", &g)) { doc.checkpoint("Physics global"); *global = g; doc.simInvalid = true; }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Global: the property follows the skeleton-wide timeline row, so one row can drive several constraints");
                    ImGui::SameLine(0, 2 * u);
                    ImVec2 gp = ImGui::GetCursorScreenPos();
                    ImGui::Dummy(ImVec2(18 * u, 18 * u));
                    drawIcon(ImGui::GetWindowDrawList(), Icon::Globe, ImVec2(gp.x + 9 * u, gp.y + 11 * u), 18 * u, IM_COL32(90, 170, 240, 255));
                }
                ImGui::PopID();
            };
            row("Inertia", Icon::Dot, "##physin", &c.inertia, rc ? rc->inertia : c.inertia, false, TimelineType::PhysicsInertia, &c.inertiaGlobal, 0, 1, "%.2f");
            row("Strength", Icon::Dot, "##physst", &c.strength, rc ? rc->strength : c.strength, false, TimelineType::PhysicsStrength, &c.strengthGlobal, 0, 500, "%.1f");
            row("Damping", Icon::Dot, "##physda", &c.damping, rc ? rc->damping : c.damping, false, TimelineType::PhysicsDamping, &c.dampingGlobal, 0, 1, "%.2f");
            row("Mass", Icon::Dot, "##physma", &c.massInverse, rc ? rc->massInverse : c.massInverse, true, TimelineType::PhysicsMass, &c.massGlobal, 0.1f, 20, "%.2f");
            row("Wind", Icon::Dot, "##physwi", &c.wind, rc ? rc->wind : c.wind, false, TimelineType::PhysicsWind, &c.windGlobal, -100, 100, "%.1f");
            row("Gravity", Icon::Dot, "##physgr", &c.gravity, rc ? rc->gravity : c.gravity, false, TimelineType::PhysicsGravity, &c.gravityGlobal, -100, 100, "%.1f");
            row("Mix", Icon::Dot, "##physmx", &c.mix, rc ? rc->mix : c.mix, false, TimelineType::PhysicsMix, &c.mixGlobal, 0, 1, "%.2f");
            propLabel(Icon::Dot, "Order");
            if (ImGui::InputInt("##physorder", &c.order)) doc.dirty = true;
            propEnd();
        }
        // Simulate / Deterministic / New / Skin, then Reset and Reset All.
        const float bh = 26 * u;
        if (iconButton("##physsim", Icon::PhysicsCon, "Simulate", doc.simulate, ImVec2(96 * u, bh),
                       "On: physics runs live on playback. Off: changing the time re-runs the physics from frame 0")) { doc.simulate = !doc.simulate; doc.simInvalid = true; }
        ImGui::SameLine(0, 6 * u);
        if (ImGui::Checkbox("Deterministic", &doc.deterministic)) doc.simInvalid = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Animate mode: always re-run the physics from frame 0 up to the current frame (more CPU, a consistent pose). Editor only");
        ImGui::SameLine(0, 10 * u);
        if (iconButton("##physnew", Icon::New, "New...", false, ImVec2(80 * u, bh), "Create another physics constraint")) ImGui::OpenPopup("##physnewmenu");
        if (ImGui::BeginPopup("##physnewmenu")) {
            if (ImGui::MenuItem("Physics Constraint (same bone)")) doc.createPhysics("physics", c.bone);
            ImGui::EndPopup();
        }
        ImGui::SameLine(0, 4 * u);
        checkboxEdit(doc, "Skin required", &c.skinRequired, "Skin required");
        ImGui::SameLine();
        skinMembership(doc, "##physskins", -1, c.name);
        if (iconButton("##physreset", Icon::Sync, "Reset", false, ImVec2(80 * u, bh), "Restart this constraint's simulation (keyed too when Auto Key is on in Animate mode)")) doc.resetPhysics(p.index);
        ImGui::SameLine(0, 4 * u);
        if (iconButton("##physresetall", Icon::Sync, "Reset All", false, ImVec2(100 * u, bh), "Restart every physics constraint (keyed on the global row when Auto Key is on)")) doc.resetPhysics(-1);
        ImGui::TextDisabled("Physics runs during playback in Animate mode. Give it the highest order so later constraints do not overwrite it.");
        break;
    }
    case SelType::Slider: {
        SliderConstraintData& c = d.sliders[p.index];
        header("Slider Constraint", c.name, Icon::Adjust, IM_COL32(120, 190, 250, 255));
        const bool animate = doc.mode == Mode::Animate;
        Animation* an = doc.currentAnimation();
        const SliderConstraint* rc = p.index < (int)doc.sk.sliders.size() ? &doc.sk.sliders[p.index] : nullptr;
        if (d.findAnimation(c.animation) < 0)
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1), "No animation: choose one to constrain.");
        else if (animate && an && an->name == c.animation)
            ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1), "This slider constrains the animation being edited.");
        if (propBegin("##slider", 100 * u)) {
            propLabel(Icon::Animations, "Animation");
            if (ImGui::BeginCombo("##slanim", c.animation.empty() ? "(none)" : c.animation.c_str())) {
                for (auto& a : d.animations)
                    if (ImGui::Selectable(a.name.c_str(), a.name == c.animation)) {
                        doc.checkpoint("Slider animation");
                        c.animation = a.name;
                        c.frameTo = std::max(1.0f, std::round(a.duration() * d.fps));
                    }
                ImGui::EndCombo();
            }
            propLabel(Icon::Loop, "Loop");
            if (ImGui::Checkbox("##slloop", &c.loop)) { doc.checkpoint("Slider loop"); doc.dirty = true; }
            propLabel(Icon::Dot, "Additive");
            if (ImGui::Checkbox("##sladd", &c.additive)) { doc.checkpoint("Slider additive"); doc.dirty = true; }
            // Keyable values: a slider, a key button.
            auto keyed = [&](const char* label, Icon ic, const char* id, float* field, float runtime, TimelineType type, float lo, float hi, const char* fmt) {
                propLabel(ic, label);
                ImGui::PushID(id);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30 * u);
                float v = animate ? runtime : *field;
                bool changed = sliderFloat("##v", &v, lo, hi, fmt);
                if (sliderActivated()) doc.beginEdit(animate ? "Key slider" : "Slider");
                if (changed) { if (animate) { if (an) doc.keyPhysics(type, p.index, v); } else { *field = v; doc.dirty = true; } }
                if (sliderDeactivated()) { if (sliderDeactivatedAfterEdit()) doc.commitEdit(); else doc.cancelEdit(); }
                ImGui::SameLine(0, 4 * u);
                ImGui::BeginDisabled(!animate || !an);
                if (iconOnlyButton("##key", Icon::Key, animate && an && doc.hasPhysicsKey(type, p.index), 26 * u,
                                   animate ? "Key this value at the current frame" : "Switch to Animate mode to key it")) {
                    doc.checkpoint("Key slider");
                    doc.keyPhysics(type, p.index, v);
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            };
            const float maxFrame = std::max(1.0f, std::round((d.findAnimation(c.animation) >= 0 ? d.animations[d.findAnimation(c.animation)].duration() : 1.0f) * d.fps));
            if (c.bone < 0) keyed("Frame", Icon::Dot, "##slframe", &c.time, rc ? rc->time : c.time, TimelineType::SliderTime, 0, maxFrame, "%.1f");
            propLabel(Icon::Bone, "Bone");
            int b = c.bone;
            if (boneCombo(doc, "##slbone", &b, true)) { doc.checkpoint("Slider bone"); c.bone = b; }
            if (c.bone >= 0) {
                propLabel(Icon::Dot, "Local");
                if (ImGui::Checkbox("##sllocal", &c.local)) { doc.checkpoint("Slider local"); doc.dirty = true; }
                propLabel(Icon::Dot, "Property");
                static const char* names[] = {"Rotate", "X", "Y", "Scale X", "Scale Y", "Shear X", "Shear Y"};
                int pi = (int)c.property;
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::Combo("##slprop", &pi, names, 7)) { doc.checkpoint("Slider property"); c.property = (SliderConstraintData::Property)pi; }
                propLabel(Icon::Dot, "Range");
                ImGui::SetNextItemWidth(-FLT_MIN);
                float rng[2] = {c.rangeFrom, c.rangeTo};
                if (dragFloat2Edit(doc, "##slrange", rng, 0.5f, "%.2f", "Slider range")) { c.rangeFrom = rng[0]; c.rangeTo = rng[1]; }
                propLabel(Icon::Dot, "Frames");
                ImGui::SetNextItemWidth(-FLT_MIN);
                float fr[2] = {c.frameFrom, c.frameTo};
                if (dragFloat2Edit(doc, "##slframes", fr, 0.5f, "%.1f", "Slider frames")) { c.frameFrom = fr[0]; c.frameTo = fr[1]; }
            }
            keyed("Mix", Icon::Dot, "##slmix", &c.mix, rc ? rc->mix : c.mix, TimelineType::SliderMix, 0, 1, "%.2f");
            propLabel(Icon::Dot, "Order");
            if (ImGui::InputInt("##slorder", &c.order)) doc.dirty = true;
            propLabel(Icon::Skins, "Skin");
            checkboxEdit(doc, "Skin required", &c.skinRequired, "Skin required");
            ImGui::SameLine();
            skinMembership(doc, "##slskins", -1, c.name);
            propEnd();
        }
        ImGui::TextDisabled("Frame (or the source bone's value) picks the frame of the animation that is posed. Give it an order after the constraints it depends on.");
        break;
    }
    case SelType::Skin: {
        Skin& s = d.skins[p.index];
        header("Skin", s.name, Icon::Skins, IM_COL32(250, 150, 60, 255));
        size_t n = 0;
        for (auto& [si, m] : s.attachments) n += m.size();
        if (propBegin("##skin", 100 * u)) {
            propLabel(Icon::Skins, "Active");
            if ((int)p.index != doc.skin) { if (ImGui::Button("Set active skin")) doc.skin = p.index; }
            else ImGui::TextColored(ImVec4(0.55f, 0.9f, 0.55f, 1), "Active skin");
            propLabel(Icon::Image, "Attachments");
            ImGui::Text("%zu", n);
            if (p.index > 0) {
                propLabel(Icon::Bone, "Skin bones");
                int removeBone = -1;
                for (int b : s.bones) {
                    ImGui::PushID(b);
                    ImGui::TextUnformatted(d.bones[b].name.c_str());
                    ImGui::SameLine(ImGui::GetContentRegionMax().x - 22 * u);
                    if (ImGui::SmallButton("x")) removeBone = b;
                    ImGui::PopID();
                }
                if (s.bones.empty()) ImGui::TextDisabled("none");
                if (removeBone >= 0) doc.setSkinBone(p.index, removeBone, false);
                propLabel(Icon::Constraints, "Skin constraints");
                std::string removeCon;
                for (size_t ci = 0; ci < s.constraints.size(); ci++) {
                    ImGui::PushID((int)ci);
                    ImGui::TextUnformatted(s.constraints[ci].c_str());
                    ImGui::SameLine(ImGui::GetContentRegionMax().x - 22 * u);
                    if (ImGui::SmallButton("x")) removeCon = s.constraints[ci];
                    ImGui::PopID();
                }
                if (s.constraints.empty()) ImGui::TextDisabled("none");
                if (!removeCon.empty()) doc.setSkinConstraint(p.index, removeCon, false);
            }
            propEnd();
        }
        if (p.index > 0 && actionButton("##skbones", Icon::Bone, "Add selected bones", true, "Add the selected bones to this skin (they need Skin required to be hidden in other skins)")) {
            for (int b : doc.selectedBones()) doc.setSkinBone(p.index, b, true);
            doc.updatePose();
        }
        if (p.index > 0 && actionButton("##skcopy", Icon::Copy, "Add selected attachment")) {
            for (auto& sel2 : doc.selection)
                if (sel2.type == SelType::Attachment)
                    if (const Attachment* a = d.skins[0].get(sel2.index, sel2.name)) { doc.checkpoint("Add to skin"); s.set(sel2.index, *a); }
        }
        ImGui::NewLine();
        break;
    }
    case SelType::Animation: {
        Animation& a = d.animations[p.index];
        header("Animation", a.name, Icon::Animations);
        if (propBegin("##anim", 100 * u)) {
            propLabel(Icon::Animations, "Duration");
            ImGui::Text("%.3f s (%d frames)", a.duration(), (int)std::round(a.duration() * d.fps));
            propLabel(Icon::Key, "Timelines");
            ImGui::Text("%zu", a.timelines.size());
            propEnd();
        }
        if (actionButton("##animedit", Icon::Animations, "Edit")) { doc.animation = p.index; doc.setMode(Mode::Animate); }
        if (actionButton("##animdup", Icon::Copy, "Duplicate")) pendingDuplicate = true;
        if (actionButton("##animslider", Icon::Adjust, "Slider", true, "New slider constraint for this animation")) doc.createSlider("slider", p.index);
        ImGui::NewLine();
        break;
    }
    case SelType::Event: {
        EventData& e = d.events[p.index];
        header("Event", e.name, Icon::Events, IM_COL32(170, 110, 240, 255));
        if (propBegin("##event", 100 * u)) {
            propLabel(Icon::Dot, "Int");
            if (ImGui::InputInt("##evint", &e.intValue)) doc.dirty = true;
            propLabel(Icon::Dot, "Float");
            dragFloatEdit(doc, "##evfloat", &e.floatValue, 0.01f, "%.3f", "Event");
            propLabel(Icon::Dot, "String");
            char buf[256];
            strncpy_s(buf, e.stringValue.c_str(), _TRUNCATE);
            if (ImGui::InputText("##evstr", buf, sizeof buf)) { e.stringValue = buf; doc.dirty = true; }
            propLabel(Icon::Audio, "Audio path");
            char abuf[256];
            strncpy_s(abuf, e.audio.c_str(), _TRUNCATE);
            if (ImGui::InputText("##evaudio", abuf, sizeof abuf)) { e.audio = abuf; doc.dirty = true; }
            propLabel(Icon::Audio, "Volume");
            dragFloatEdit(doc, "##evvol", &e.volume, 0.01f, "%.2f", "Event");
            propLabel(Icon::Audio, "Balance");
            dragFloatEdit(doc, "##evbal", &e.balance, 0.01f, "%.2f", "Event");
            propEnd();
        }
        if (doc.mode == Mode::Animate && doc.currentAnimation() && actionButton("##evkey", Icon::Key, "Key at current frame")) {
            doc.checkpoint("Key event");
            doc.currentAnimation()->get(TimelineType::Event, -1).setKey(std::round(doc.time * d.fps) / d.fps).str = e.name;
        }
        ImGui::NewLine();
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
    if (pendingDuplicate) doc.duplicateSelection();
    if (pendingDelete) doc.deleteSelection();
}

// ---- small views ----

void Editor::animationsView() {
    ImGui::Begin("Animations", &showAnimations);
    {
        const float u = uiScale, bh = 26 * u;
        if (iconButton("##animnew", Icon::New, "New", false, ImVec2(70 * u, bh), "New animation")) newAnimation();
        ImGui::SameLine(0, 4 * u);
        ImGui::BeginDisabled(doc.animation < 0);
        if (iconButton("##animdup", Icon::Copy, "Duplicate", false, ImVec2(92 * u, bh), "Duplicate the selected animation")) { doc.selection = {{SelType::Animation, doc.animation, ""}}; doc.duplicateSelection(); }
        ImGui::SameLine(0, 4 * u);
        if (iconButton("##animdel", Icon::Delete, "Delete", false, ImVec2(74 * u, bh), "Delete the selected animation", IM_COL32(236, 100, 100, 255))) { doc.selection = {{SelType::Animation, doc.animation, ""}}; doc.deleteSelection(); }
        ImGui::EndDisabled();
    }
    ImGui::Separator();
    // Double-click an animation's name to rename it in place: Enter or clicking away keeps the new name, Esc cancels.
    static int renamingAnim = -1;
    static char animName[256];
    static bool animFocusNext = false, animWasActive = false;
    for (size_t i = 0; i < doc.data.animations.size(); i++) {
        const Animation& a = doc.data.animations[i];
        if ((int)i == renamingAnim) {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100);
            if (animFocusNext) { ImGui::SetKeyboardFocusHere(); animFocusNext = false; }
            bool enter = ImGui::InputText("##animrename", animName, sizeof animName, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            if (ImGui::IsItemActive()) animWasActive = true;
            bool left = animWasActive && !ImGui::IsItemActive(); // the field lost focus: clicked away
            if (enter || left) {
                // Enter or clicking away keeps the name; an empty or taken name is refused and the old one stays.
                doc.selection = {{SelType::Animation, (int)i, ""}};
                if (std::string(animName) != a.name && !doc.renameSelection(animName)) setStatus("Name is empty or already used");
                renamingAnim = -1;
            } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) renamingAnim = -1; // cancel
            continue;
        }
        char label[256];
        std::snprintf(label, sizeof label, "%s##an%zu", a.name.c_str(), i);
        if (ImGui::Selectable(label, (int)i == doc.animation)) {
            doc.animation = (int)i;
            doc.setTime(0);
            doc.setMode(Mode::Animate);
            doc.selection = {{SelType::Animation, (int)i, ""}};
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            renamingAnim = (int)i;
            animFocusNext = true;
            animWasActive = false;
            std::snprintf(animName, sizeof animName, "%s", a.name.c_str());
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

// The four corners of the image in the mesh's local space (BL, TL, TR, BR, matching UVs 0,1 / 0,0 / 1,0 / 1,1), found from where
// the mesh's own UVs put the image. The image sits at the attachment's offset, not at the bone's origin, so this must not
// assume it is centred. Falls back to the attachment's own placement when the vertices no longer follow the UVs.
static void meshImageCorners(const Attachment& a, float out[8]) {
    static const float uv[8] = {0, 1, 0, 0, 1, 0, 1, 1};
    double cx[3], cy[3];
    if (a.vertexCount() >= 3 && fitAffine(a.uvs, a.vertices, a.vertexCount(), cx, cy)) {
        // Accept the fit when the vertices sit on it (an unedited or traced mesh); dragged vertices spoil it.
        float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f, worst = 0;
        for (int k = 0; k < a.vertexCount(); k++) {
            double px = cx[0] * a.uvs[k * 2] + cx[1] * a.uvs[k * 2 + 1] + cx[2], py = cy[0] * a.uvs[k * 2] + cy[1] * a.uvs[k * 2 + 1] + cy[2];
            worst = std::max(worst, (float)std::hypot(px - a.vertices[k * 2], py - a.vertices[k * 2 + 1]));
            minX = std::min(minX, a.vertices[k * 2]); maxX = std::max(maxX, a.vertices[k * 2]);
            minY = std::min(minY, a.vertices[k * 2 + 1]); maxY = std::max(maxY, a.vertices[k * 2 + 1]);
        }
        float size = std::max(maxX - minX, maxY - minY);
        if (size > 1e-3f && worst <= 0.02f * size) {
            for (int c = 0; c < 4; c++) {
                out[c * 2] = (float)(cx[0] * uv[c * 2] + cx[1] * uv[c * 2 + 1] + cx[2]);
                out[c * 2 + 1] = (float)(cy[0] * uv[c * 2] + cy[1] * uv[c * 2 + 1] + cy[2]);
            }
            return;
        }
    }
    Attachment t = a;
    t.scaleX = t.scaleY = 1; // a mesh's width and height already include the scale it was converted with
    t.regionCorners(out);
}

// Replaces a mesh's vertices with a new hull (and its UVs) and re-triangulates; the hull is the whole mesh.
static void setMeshHull(Attachment& a, const std::vector<float>& local, const std::vector<float>& uvs) {
    a.vertices = local;
    a.uvs = uvs;
    a.worldVerticesLength = (int)a.vertices.size();
    a.hullLength = a.vertexCount();
    a.triangles = triangulateMesh(a.vertices, a.hullLength);
    a.edges.clear();
    for (int h = 0; h < a.hullLength; h++) { a.edges.push_back(h); a.edges.push_back((h + 1) % a.hullLength); }
}

void Editor::meshBeginNew() {
    newHull.clear();
    meshMode = MeshMode::New;
    meshSelection.clear();
    setStatus("New: click to place hull vertices, drag to move them. Click the first vertex or New again to finish (Esc cancels)");
}

void Editor::meshFinishNew(bool apply) {
    meshMode = apply ? MeshMode::Create : MeshMode::Modify; // Spine drops into Create once the new hull is closed
    std::vector<Vec2> pts = std::move(newHull);
    newHull.clear();
    if (!apply) return;
    int si = -1;
    Attachment* a = selectedAttachment(&si);
    if (!a || a->type != AttachmentType::Mesh) return;
    if (pts.size() < 3) { setStatus("A mesh hull needs at least 3 vertices"); return; }
    // Local -> UV from the current mesh, so the texture stays where it is.
    doc.checkpoint("New mesh");
    a = selectedAttachment(&si);
    doc.dropMeshBindings(si, *a); // a weighted mesh becomes plain vertices first
    double cx[3], cy[3];
    if (!fitAffine(a->vertices, a->uvs, a->vertexCount(), cx, cy)) {
        float corners[8];
        meshImageCorners(*a, corners);
        if (!fitAffine(std::vector<float>(corners, corners + 8), {0, 1, 0, 0, 1, 0, 1, 1}, 4, cx, cy)) { setStatus("Cannot map the image to the mesh"); return; }
    }
    std::vector<float> local, uvs;
    for (Vec2 p : pts) {
        local.push_back(p.x); local.push_back(p.y);
        uvs.push_back(clampf((float)(cx[0] * p.x + cx[1] * p.y + cx[2]), 0, 1));
        uvs.push_back(clampf((float)(cy[0] * p.x + cy[1] * p.y + cy[2]), 0, 1));
    }
    setMeshHull(*a, local, uvs);
    meshSelection.clear();
    traceLive = false;
    setStatus("New hull: " + std::to_string(pts.size()) + " vertices");
}

void Editor::meshToolsView() {
    ImGui::Begin("Mesh Tools", &showMeshTools);
    int si = -1;
    Attachment* a = selectedAttachment(&si);
    if (!a || !a->isVertex()) {
        ImGui::TextWrapped("Select a mesh, bounding box, clipping or path attachment. Regions can be converted to meshes from the Properties view.");
        meshMode = MeshMode::Off;
        newHull.clear();
        ImGui::End();
        return;
    }
    const float u = uiScale;
    bool isMesh = a->type == AttachmentType::Mesh;
    const bool isPath = a->type == AttachmentType::Path;
    bool setup = doc.mode == Mode::Setup;
    if (!isMesh && meshMode == MeshMode::New) { meshMode = MeshMode::Modify; newHull.clear(); }
    ImGui::Text("%s  %s", attachmentTypeName(a->type), a->name.c_str());

    // Layout follows Spine's Edit Mesh box: Modify/Create/Delete, then New/Reset/Generate/Trace, then the
    // view checkboxes, then the vertex count. (Right-click in the viewport cycles Modify, Create and Delete.)
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float applyW = 92 * u, bh = 26 * u;
    float colW = (ImGui::GetContentRegionAvail().x - spacing * 2) / 3;
    auto modeButton = [&](const char* id, const char* l, Icon ic, MeshMode m, bool enabled, const char* tip, bool last) {
        ImGui::BeginDisabled(!enabled);
        if (iconButton(id, ic, l, meshMode == m, ImVec2(colW, bh), tip)) {
            if (m == MeshMode::New) { if (meshMode == MeshMode::New) meshFinishNew(true); else meshBeginNew(); }
            else { if (meshMode == MeshMode::New) newHull.clear(); meshMode = meshMode == m ? MeshMode::Off : m; }
        }
        ImGui::EndDisabled();
        if (!last) ImGui::SameLine();
    };
    modeButton("##mModify", "Modify", Icon::Adjust, MeshMode::Modify, true, isPath ? "Drag points and their handles. Alt moves one handle on its own" : "Move vertices to adjust UVs", false);
    modeButton("##mCreate", "Create", Icon::Create, MeshMode::Create, (isMesh || isPath) && setup, isPath ? "Click to add a point (drag to pull its Bezier handles), click the curve to insert one, click the first point to close the path" : "Create vertices and edges", false);
    modeButton("##mDelete", "Delete", Icon::Delete, MeshMode::Delete, (isMesh || isPath) && setup, isPath ? "Click a point to delete it, or a handle to reset it" : "Delete vertices and edges", true);
    if (meshMode == MeshMode::New) ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.25f, 1), "New hull: %d vertices placed", (int)newHull.size());
    if (isMesh && setup) ImGui::Dummy(ImVec2(0, 0)); // the action row (New, Reset, Generate, Trace) follows below

    auto viewOptions = [&]() {
        ImGui::Checkbox("Triangles", &meshTriangles);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Triangle lines can be hidden to see internal edges more easily\nHotkey: <none>  Edit Mesh - Triangles");
        ImGui::SameLine();
        ImGui::Checkbox("Dim", &meshDim);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Darkens the image to make the mesh easier to see\nHotkey: <none>  Edit Mesh - Dim");
        ImGui::SameLine();
        ImGui::Checkbox("Isolate", &meshIsolate);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hides all other attachments\nHotkey: <none>  Edit Mesh - Isolate");
    };
    bool apply = false;
    auto footer = [&]() {
        if (isPath) ImGui::TextDisabled("Points: %d", a->vertexCount() / 3);
        else if (meshMode == MeshMode::New) ImGui::TextDisabled("Vertices: %d", (int)newHull.size());
        else ImGui::TextDisabled("Vertices: %d", a->vertexCount());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - applyW + ImGui::GetCursorPosX());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.55f, 0.22f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.17f, 0.66f, 0.28f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.45f, 0.18f, 1));
        apply = ImGui::Button("Apply", ImVec2(applyW, bh));
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Finish editing the mesh. Then drag its dots to deform it");
    };
    auto finishApply = [&]() {
        // Done with the mesh: leave Edit Mesh and the panel; the dots now deform the mesh.
        if (meshMode == MeshMode::New) meshFinishNew(true);
        meshMode = MeshMode::Off;
        newHull.clear();
        meshSelection.clear();
        traceLive = false;
        showMeshTools = false;
        meshTriangles = false; // after Apply only the dots show; Edit Mesh > Triangles brings the wireframe back
        setStatus("Mesh applied: drag its dots to deform it");
    };

    if (isMesh && setup) {
        ImGui::Separator();
        ImGui::TextDisabled(a->isWeighted() ? "Generate, Trace, Reset and New remove weights and deform keys." : "Generate, Trace, Reset and New remove deform keys.");
        ImGui::Spacing();

        // Runs the trace on the selected mesh; `fresh` starts a new undo step, otherwise it updates the last one.
        auto runTrace = [&](bool fresh) {
            Image* im = gfx.image(doc.imagePathFor(*a));
            if (!im) { setStatus("Image not found"); return; }
            std::vector<float> hull = traceHull(im->rgba.data(), im->w, im->h, trace);
            if (hull.size() < 6) { setStatus("Trace failed (image fully transparent?)"); return; }
            if (fresh) doc.checkpoint("Trace mesh");
            a = selectedAttachment(&si);
            doc.dropMeshBindings(si, *a); // a weighted mesh becomes plain vertices first
            // uv -> local from the vertices as they are now (exact for unedited meshes).
            double cx[3], cy[3];
            if (!fitAffine(a->uvs, a->vertices, a->vertexCount(), cx, cy)) {
                float corners[8];
                meshImageCorners(*a, corners);
                if (!fitAffine({0, 1, 0, 0, 1, 0, 1, 1}, std::vector<float>(corners, corners + 8), 4, cx, cy)) { setStatus("Cannot map the image to the mesh"); return; }
            }
            std::vector<float> local, uvs;
            for (size_t k = 0; k + 1 < hull.size(); k += 2) {
                float uu = clampf(hull[k] / im->w, 0, 1), vv = clampf(hull[k + 1] / im->h, 0, 1);
                uvs.push_back(uu); uvs.push_back(vv);
                local.push_back((float)(cx[0] * uu + cx[1] * vv + cx[2]));
                local.push_back((float)(cy[0] * uu + cy[1] * vv + cy[2]));
            }
            setMeshHull(*a, local, uvs);
            meshSelection.clear();
            traceLive = true;
            setStatus("Traced hull: " + std::to_string(a->hullLength) + " vertices");
        };

        // Spine asks before an operation that removes the attachment's weights; the gate returns true when the operation should run now.
        int confirmed = 0;
        if (ImGui::BeginPopupModal("Attachment weights", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("This attachment has weights.");
            ImGui::TextUnformatted("Performing this operation will remove all weights for this attachment.");
            ImGui::Spacing();
            if (ImGui::Button("Remove weights")) { confirmed = pendingWeightOp; pendingWeightOp = 0; ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) { pendingWeightOp = 0; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        auto weightGate = [&](int op, bool clicked) {
            if (confirmed == op) return true;
            if (!clicked) return false;
            if (!a->isWeighted()) return true;
            pendingWeightOp = op;
            ImGui::OpenPopup("Attachment weights");
            return false;
        };
        // New, Reset, Generate and Trace in one row, as in Spine.
        const float actW = (ImGui::GetContentRegionAvail().x - spacing * 3) / 4;
        {
            bool newClicked = iconButton("##mNew", Icon::New, "New", meshMode == MeshMode::New, ImVec2(actW, bh), "Clears existing vertices and allows new edge vertices to be created easily\nHotkey: <none>  Edit Mesh - New");
            if (newClicked && meshMode == MeshMode::New) meshFinishNew(true);
            else if (weightGate(4, newClicked)) meshBeginNew();
            ImGui::SameLine();
        }
        if (weightGate(2, iconButton("##mReset", Icon::Sync, "Reset", false, ImVec2(actW, bh), "Resets the vertices to the four corners\nHotkey: <none>  Edit Mesh - Reset"))) {
            doc.checkpoint("Reset mesh");
            a = selectedAttachment(&si);
            doc.dropMeshBindings(si, *a);
            float corners[8];
            meshImageCorners(*a, corners); // where the image really is, not a square around the bone
            setMeshHull(*a, std::vector<float>(corners, corners + 8), {0, 1, 0, 0, 1, 0, 1, 1});
            meshSelection.clear();
            traceLive = false;
            setStatus("Mesh reset to its four corners");
        }
        ImGui::SameLine();
        if (weightGate(1, iconButton("##mGenerate", Icon::Fit, "Generate", false, ImVec2(actW, bh), "Generate new vertices within the mesh\nHotkey: <none>  Edit Mesh - Generate"))) {
            doc.checkpoint("Generate vertices");
            a = selectedAttachment(&si);
            doc.dropMeshBindings(si, *a);
            std::vector<float> pts = generateVertices(a->vertices, a->hullLength, a->triangles);
            int added = 0;
            for (size_t k = 0; k + 1 < pts.size(); k += 2) {
                float uv[2];
                if (!interpolateUv(a->vertices, a->uvs, a->triangles, pts[k], pts[k + 1], uv)) continue;
                a->vertices.push_back(pts[k]); a->vertices.push_back(pts[k + 1]);
                a->uvs.push_back(uv[0]); a->uvs.push_back(uv[1]);
                added++;
            }
            a->worldVerticesLength = (int)a->vertices.size();
            a->triangles = triangulateMesh(a->vertices, a->hullLength);
            traceLive = false;
            setStatus("Generated " + std::to_string(added) + " vertices (" + std::to_string(a->vertexCount() - a->hullLength) + " inside the hull)");
        }
        ImGui::SameLine();
        if (weightGate(3, iconButton("##mTrace", Icon::Image, "Trace", false, ImVec2(actW, bh), "Automatically places vertices around the edges of the image\nHotkey: <none>  Edit Mesh - Trace"))) runTrace(true);
        bool refresh = false;
        viewOptions();
        ImGui::Spacing();

        // Trace settings; after a Trace they re-trace live.
        if (traceLive && (selectedAttachment(&si) == nullptr || !a)) traceLive = false;
        bool changed = false;
        ImGui::PushItemWidth(-90 * u);
        changed |= sliderFloat("Detail", &trace.detail, 0, 100, "%.0f");
        if (sliderHovered()) ImGui::SetTooltip("The number of vertices used to surround the shape");
        changed |= sliderFloat("Concavity", &trace.concavity, 0, 100, "%.0f");
        if (sliderHovered()) ImGui::SetTooltip("Prioritizes placing vertices into concave areas of the shape");
        changed |= sliderFloat("Refinement", &trace.refinement, 0, 100, "%.0f");
        if (sliderHovered()) ImGui::SetTooltip("The time and effort to spend finding an optimal solution");
        changed |= sliderFloat("Uniform", &trace.uniform, 0, 1, "%.1f");
        if (sliderHovered()) ImGui::SetTooltip("Subdivide long edges for more uniform spacing");
        changed |= sliderInt("Alpha threshold", &trace.alphaThreshold, 0, 254);
        if (sliderHovered()) ImGui::SetTooltip("Pixels with alpha values below this threshold are ignored");
        changed |= sliderFloat("Padding", &trace.padding, 0, 30, "%.1f px");
        if (sliderHovered()) ImGui::SetTooltip("Space between the image and the hull");
        ImGui::PopItemWidth();
        if (iconButton("##mRefresh", Icon::Sync, "Refresh", false, ImVec2(-1, bh), "Trace again with the same settings: the result differs slightly each time")) { trace.seed++; runTrace(!traceLive); refresh = true; }
        if (!refresh && traceLive && changed) runTrace(false);
    } else if (!setup) {
        ImGui::Separator();
        ImGui::TextDisabled("Animate mode: dragging a vertex keys a deform.");
    }
    if (!(isMesh && setup)) viewOptions();
    ImGui::Separator();
    footer();
    if (apply) finishApply();
    ImGui::End();
}

std::vector<int> Editor::lockedBonesFor(int slot, const Attachment& mesh) const {
    std::vector<int> out;
    if (slot < 0 || slot >= (int)doc.data.slots.size()) return out;
    for (int b : boundBones(mesh))
        if (weightLocks.count(doc.data.slots[slot].name + "/" + mesh.name + "/" + doc.data.bones[b].name)) out.push_back(b);
    return out;
}

// Applies the bones clicked in Bind mode: the first binding is automatic, later ones add bones to the binding.
void Editor::applyWeights() {
    if (bindMode) finishBind();
    bindMode = false; bindPending.clear();
    meshSelection.clear();
    if (doc.tool == Tool::Weights) doc.tool = lastTransformTool;
    ImGui::SetWindowFocus("Properties");
    setStatus("Weights applied");
}

void Editor::finishBind() {
    bindMode = false;
    std::vector<int> pending = bindPending;
    bindPending.clear();
    int si = -1;
    Attachment* a = selectedAttachment(&si);
    if (pending.empty() || !a || a->type != AttachmentType::Mesh) return;
    doc.checkpoint("Bind weights");
    a = selectedAttachment(&si);
    bool first = !a->isWeighted();
    addBonesToBinding(*a, doc.sk, doc.data.slots[si].bone, pending);
    weightBones = {pending.back()};
    weightBone = pending.back();
    setStatus(first ? "Bound to " + std::to_string(pending.size()) + " bones with automatic weights" : "Bones added to the binding");
}

void Editor::weightsView() {
    ImGui::Begin("Weights", &showWeights);
    int si = -1;
    Attachment* a = selectedAttachment(&si);
    if (!a || a->type != AttachmentType::Mesh) {
        ImGui::TextWrapped("Select a mesh attachment. Weights bind mesh vertices to bones, so the vertices move when the bones do.");
        if (doc.tool == Tool::Weights && iconButton("##wapply0", Icon::Save, "Apply", false, ImVec2(96 * uiScale, 0), "Leave the Weights view (or double-click empty space)")) applyWeights();
        ImGui::End();
        return;
    }
    const float u = uiScale;
    if (doc.mode != Mode::Setup) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "Weights are edited in Setup mode.");
    ImGui::BeginDisabled(doc.mode != Mode::Setup);
    const int slotBone = doc.data.slots[si].bone;
    std::vector<int> bound = boundBones(*a);
    for (auto it = weightBones.begin(); it != weightBones.end();) it = std::find(bound.begin(), bound.end(), *it) == bound.end() ? weightBones.erase(it) : std::next(it);
    std::vector<int> locked = lockedBonesFor(si, *a);
    std::vector<int> sel = doc.selectedBones();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float bw = (ImGui::GetContentRegionAvail().x - spacing * 2) / 3;

    // Layout follows Spine's Weights box: Influence + Prune, Mode + Update, the brush sliders, Smooth / Auto,
    // Pies / Overlay / Selected, the Bones list with Copy / Paste / Select Bones, then Bind / Swap / Remove.
    if (a->isWeighted()) {
        // ---- Influence (the bone limit that Prune enforces) and Prune ----
        const float rowW = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(rowW - 190 * u);
        sliderInt("##winf", &pruneMaxBones, 1, 8, "Influence %d");
        if (sliderHovered()) ImGui::SetTooltip("Shows a problem when a vertex is influenced by more than this many bones");
        ImGui::SameLine();
        if (iconButton("##wprune", Icon::Delete, "Prune", false, ImVec2(-1, 0), "Removes weights of the selected vertices below a threshold\nHotkey: <none>  Weights - Prune")) {
            doc.checkpoint("Prune weights");
            a = selectedAttachment(&si);
            pruneWeights(*a, pruneThreshold / 100, pruneMaxBones);
        }
        ImGui::SetNextItemWidth(rowW - 190 * u);
        sliderFloat("##pthresh", &pruneThreshold, 0, 50, "Threshold %.1f%%");
        if (sliderHovered()) ImGui::SetTooltip("Weights below this are removed by Prune; the removed weight goes to the bones that stay");
        ImGui::SameLine();
        ImGui::TextDisabled("%d", pruneCount(*a, pruneThreshold / 100, pruneMaxBones));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bone weights Prune would remove");

        // ---- Mode: Direct, Add, Remove or Replace; Update rebinds at the current pose ----
        static const char* kModes[] = {"Direct", "Add", "Remove", "Replace"};
        int modeIx = !weightBrush ? 0 : 1 + (int)weightMode;
        ImGui::SetNextItemWidth(rowW - 84 * u);
        if (ImGui::BeginCombo("##wmode", kModes[modeIx])) {
            for (int k = 0; k < 4; k++)
                if (ImGui::Selectable(kModes[k], k == modeIx)) {
                    weightBrush = k > 0;
                    if (k > 0) weightMode = (WeightMode)(k - 1);
                }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Controls how weights are manipulated");
        ImGui::SameLine();
        if (iconButton("##wupdate", Icon::Sync, "Update", false, ImVec2(-1, 0), "Store the current mesh vertices as the bind positions\nHotkey: <none>  Weights - Update Bindings")) {
            doc.checkpoint("Update bindings");
            a = selectedAttachment(&si);
            updateBindings(*a, doc.sk);
            setStatus("Bindings updated to the current pose");
        }

        // ---- Direct: Weight. Brush: Strength, Size, Feather ----
        if (!weightBrush) {
            ImGui::SetNextItemWidth(-110 * u);
            bool changed = sliderFloat("##wval", &weightValue, 0, 100, "Weight %.0f");
            if (sliderHovered()) ImGui::SetTooltip("Sets the weight for painting");
            if (sliderActivated()) doc.checkpoint("Set weight");
            if (changed && !meshSelection.empty() && !weightBones.empty()) {
                a = selectedAttachment(&si);
                std::vector<VertexWeightEdit> edits;
                for (int v : meshSelection) for (int b : weightBones) edits.push_back({v, b, weightValue / 100});
                setVertexWeights(*a, doc.sk, edits, locked);
                doc.dirty = true;
            }
        } else {
            ImGui::SetNextItemWidth(-110 * u);
            float strengthPct = brushStrength * 100;
            if (sliderFloat("##wstr", &strengthPct, 1, 100, "Strength %.0f")) brushStrength = strengthPct / 100;
            if (sliderHovered()) ImGui::SetTooltip("Sets the weight for painting");
            ImGui::SetNextItemWidth(-110 * u);
            sliderFloat("##wsize", &brushSize, 4, 200, "Size %.0f");
            if (sliderHovered()) ImGui::SetTooltip("The radius of the weight painting brush");
            ImGui::SetNextItemWidth(-110 * u);
            sliderFloat("##wfeather", &brushFeather, 0, 100, "Feather %.0f");
            if (sliderHovered()) ImGui::SetTooltip("The percentage of the weight painting brush size that fades to zero");
        }

        // ---- Smooth, Auto (Normalize is an Osteon extra) ----
        const float actW = (ImGui::GetContentRegionAvail().x - spacing * 2) / 3;
        if (iconButton("##wsmooth", Icon::Adjust, "Smooth", false, ImVec2(actW, 0), "Blends the weights of the selected vertices, or all vertices if none are selected\nHotkey: <none>  Weights - Smooth")) {
            doc.checkpoint("Smooth weights");
            a = selectedAttachment(&si);
            smoothWeights(*a, doc.sk, meshSelection);
        }
        ImGui::SameLine();
        if (iconButton("##wauto", Icon::Fit, "Auto", false, ImVec2(actW, 0), "Computes weights automatically for the selected vertices and bones\nHotkey: <none>  Weights - Auto")) {
            doc.checkpoint("Auto weights");
            a = selectedAttachment(&si);
            autoWeights(*a, doc.sk, slotBone, bound, meshSelection, std::vector<int>(weightBones.begin(), weightBones.end()));
        }
        ImGui::SameLine();
        if (iconButton("##wnorm", Icon::Sync, "Normalize", false, ImVec2(actW, 0), "Makes each vertex's weights add up to 100%")) { doc.checkpoint("Normalize weights"); a = selectedAttachment(&si); normalizeWeights(*a); }

        // ---- Display ----
        ImGui::Checkbox("Pies", &showPies);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shows colored pies for the Weights tool, indicating bone influences\nHotkey: <none>  Weights - Pies");
        ImGui::SameLine();
        ImGui::Checkbox("Overlay", &showOverlay);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shows a color overlay indicating the influence of the bones\nHotkey: <none>  Weights - Overlay");
        ImGui::SameLine();
        ImGui::Checkbox("Selected", &showSelectedOnly);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shows pies and overlay only for the selected vertices and bones\nHotkey: <none>  Weights - Selected");

        // ---- Bones list header: Copy / Paste / Select Bones ----
        ImGui::Separator();
        ImGui::TextUnformatted("Bones:");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150 * u + ImGui::GetCursorPosX());
        std::vector<int> ordered = meshSelection;
        std::sort(ordered.begin(), ordered.end());
        ImGui::BeginDisabled(ordered.empty());
        if (ImGui::SmallButton("Copy")) {
            weightClipboard = copyWeights(*a, ordered);
            setStatus("Copied the weights of " + std::to_string(ordered.size()) + " vertices");
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Copy selected vertex weights or bones");
        ImGui::SameLine();
        if (ImGui::SmallButton("Paste")) {
            doc.checkpoint("Paste weights");
            a = selectedAttachment(&si);
            if (pasteWeights(*a, doc.sk, ordered, weightClipboard)) setStatus("Pasted weights");
            else setStatus("Select the same number of vertices as were copied");
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Paste vertex weights or bones");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(weightBones.empty());
        if (ImGui::SmallButton("Select")) {
            meshSelection.clear();
            for (int v = 0; v < a->vertexCount(); v++)
                for (int b : weightBones) if (boneWeightAt(*a, v, b) > 0.0001f) { meshSelection.push_back(v); break; }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Selects the vertices influenced by the selected bones\nHotkey: <none>  Weights - Select Bones");
        ImGui::EndDisabled();

        // ---- Bones list: lock, colour, name and (with vertices selected) the weight of each bone. ----
        for (size_t bi = 0; bi < bound.size(); bi++) {
            int b = bound[bi];
            ImGui::PushID(b);
            std::string key = doc.data.slots[si].name + "/" + a->name + "/" + doc.data.bones[b].name;
            bool isLocked = weightLocks.count(key) > 0;
            bool on = weightBones.count(b) > 0;
            ImVec2 p = ImGui::GetCursorScreenPos();
            float rowH = ImGui::GetTextLineHeight() + 6 * u;
            if (ImGui::Selectable("##bone", on, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, rowH))) {
                if (!ImGui::GetIO().KeyCtrl) weightBones.clear();
                if (on && ImGui::GetIO().KeyCtrl) weightBones.erase(b); else weightBones.insert(b);
                weightBone = b;
                doc.tool = Tool::Weights;
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) doc.select({SelType::Bone, b, ""}); // shows it in the tree and viewport
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float cy = p.y + rowH / 2;
            // Lock: a locked bone's weights do not change when other bones are adjusted.
            ImVec2 lockMin(p.x + 2 * u, cy - 8 * u), lockMax(p.x + 18 * u, cy + 8 * u);
            if (ImGui::IsMouseHoveringRect(lockMin, lockMax)) {
                ImGui::SetTooltip("Lock: keeps this bone's weights unchanged while others are adjusted");
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { if (isLocked) weightLocks.erase(key); else weightLocks.insert(key); }
            }
            drawIcon(dl, Icon::Lock, ImVec2((lockMin.x + lockMax.x) / 2, cy), 13 * u, isLocked ? IM_COL32(255, 210, 90, 255) : (theme::hex(theme::kTextDisabled) & 0x00FFFFFF) | (150u << 24));
            // The bone's colour, the same one it has in the viewport.
            dl->AddRectFilled(ImVec2(p.x + 24 * u, cy - 6 * u), ImVec2(p.x + 36 * u, cy + 6 * u), weightPaletteColor(bi), 2);
            dl->AddText(ImVec2(p.x + 44 * u, cy - ImGui::GetTextLineHeight() / 2), ImGui::GetColorU32(ImGuiCol_Text), doc.data.bones[b].name.c_str());
            // The weight on the selected vertices; * when they differ.
            if (!meshSelection.empty()) {
                float w0 = boneWeightAt(*a, meshSelection[0], b);
                bool same = true;
                for (int v : meshSelection) same &= std::fabs(boneWeightAt(*a, v, b) - w0) < 0.005f;
                char txt[16];
                if (same) std::snprintf(txt, sizeof txt, "%.0f%%", w0 * 100); else std::snprintf(txt, sizeof txt, "*");
                float tw = ImGui::CalcTextSize(txt).x;
                dl->AddText(ImVec2(p.x + ImGui::GetContentRegionAvail().x - tw - 6 * u, cy - ImGui::GetTextLineHeight() / 2),
                            w0 > 0 || !same ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled), txt);
            }
            ImGui::PopID();
        }
        if (meshSelection.empty()) ImGui::TextDisabled("No vertices selected: the brush affects all vertices.");
        else ImGui::TextDisabled("%d vertex%s selected.", (int)meshSelection.size(), meshSelection.size() == 1 ? "" : "es");
        ImGui::TextDisabled("Ctrl+A selects all vertices; Ctrl+C / Ctrl+V copy and paste weights.");
        ImGui::Separator();
    } else {
        ImGui::TextDisabled("Mesh %s is not weighted. Bind it to bones to start.", a->name.c_str());
    }

    // ---- Bind: select the mesh, click Bind, then click each bone to bind (Space/Esc, or Bind again, finishes). ----
    if (iconButton("##wbind", Icon::Weights, "Bind", bindMode, ImVec2(bw, 0), "Click each bone to bind it to the mesh, then press Space, Esc or Bind again")) {
        if (bindMode) finishBind();
        else { bindMode = true; bindPending.clear(); doc.tool = Tool::Weights; setStatus("Bind: click each bone to bind (Space/Esc finishes)"); }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!a->isWeighted() || weightBones.size() != 2);
    if (iconButton("##wswap", Icon::Sync, "Swap", false, ImVec2(bw, 0), "Swaps the weights of the two selected bones")) {
        doc.checkpoint("Swap weights");
        a = selectedAttachment(&si);
        std::vector<int> two(weightBones.begin(), weightBones.end());
        swapBoneWeights(*a, doc.sk, two[0], two[1]);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!a->isWeighted() || weightBones.empty());
    if (iconButton("##wremove", Icon::Delete, "Remove", false, ImVec2(-1, 0), "Removes the selected bones from the binding")) {
        doc.checkpoint("Remove bones from binding");
        a = selectedAttachment(&si);
        for (int b : std::vector<int>(weightBones.begin(), weightBones.end())) removeBoneWeights(*a, doc.sk, slotBone, b);
        weightBones.clear();
    }
    ImGui::EndDisabled();
    if (ImGui::Button("Bind selected")) {
        if (sel.empty()) setStatus("Select bones in the tree (Ctrl+click) keeping the mesh selected, or use Bind and click them");
        else { bindPending = sel; finishBind(); }
    }
    if (a->isWeighted()) {
        ImGui::SameLine();
        if (ImGui::Button("Unbind")) {
            doc.checkpoint("Unbind weights");
            a = selectedAttachment(&si);
            unbindWeights(*a, doc.sk, slotBone);
            weightBones.clear();
        }
    }
    if (bindMode) ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Click bones to bind (%d so far)", (int)bindPending.size());
    ImGui::EndDisabled();
    // Apply: the weights are already live on the mesh; this finishes (a pending Bind too) and leaves the Weights view.
    if (iconButton("##wapply", Icon::Save, "Apply", false, ImVec2(96 * u, 0), "Done with the weights: leave the Weights view and go back to the transform tools (or double-click empty space)")) applyWeights();
    ImGui::End();
}

} // namespace osteon
