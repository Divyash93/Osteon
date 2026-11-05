// The viewport's right-click menu on bones and images, and the confirmation shown before a delete takes parented objects with it.
#include "App.h"
#include "Icons.h"
#include "Util.h"
#include <set>

namespace osteon {

// ---- delete with a warning ----

void Editor::requestDelete() {
    if (doc.selection.empty()) return;
    delInfo = doc.describeDelete();
    delBonesAlso = delSlotsAlso = 0;
    // Several objects selected: count what goes with the bones that are not selected themselves.
    if (doc.selection.size() > 1) {
        std::set<int> selBones, selSlots;
        for (int b : doc.selectedBones()) selBones.insert(b);
        for (int s : doc.selectedSlots()) selSlots.insert(s);
        for (size_t i = 1; i < doc.data.bones.size(); i++) {
            if (selBones.count((int)i)) continue;
            for (int b : selBones) if (doc.data.isDescendant((int)i, b)) { delBonesAlso++; break; }
        }
        for (size_t s = 0; s < doc.data.slots.size(); s++) {
            if (selSlots.count((int)s)) continue;
            for (int b : selBones) if (doc.data.isDescendant(doc.data.slots[s].bone, b)) { delSlotsAlso++; break; }
        }
        if (delBonesAlso + delSlotsAlso > 0) { delState = 1; delInfo.kind = 3; return; }
        doc.deleteSelection();
        return;
    }
    if (delInfo.kind == 1 && (delInfo.childBones > 0 || delInfo.slots > 0)) { delState = 1; return; }
    if (delInfo.kind == 2 && delInfo.canDeleteBone) { delState = 1; return; }
    doc.deleteSelection();
}

void Editor::deleteDialog() {
    if (delState == 0) return;
    if (delState == 1) { ImGui::OpenPopup("Delete?"); delState = 2; }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22 * uiScale, 18 * uiScale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10 * uiScale, 10 * uiScale));
    bool open = ImGui::BeginPopupModal("Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    if (!open) { if (delState == 2 && !ImGui::IsPopupOpen("Delete?")) delState = 0; return; }
    const float u = uiScale;
    auto plural = [](int n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); };
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 380 * u);
    bool close = false;
    const ImVec2 bsz(150 * u, 28 * u);
    if (delInfo.kind == 1) {
        std::string what;
        if (delInfo.childBones > 0) what += plural(delInfo.childBones, "child bone", "child bones");
        if (delInfo.slots > 0) what += std::string(what.empty() ? "" : " and ") + plural(delInfo.slots, "image", "images");
        ImGui::TextWrapped("Deleting the bone \"%s\" also deletes %s parented to it.", delInfo.name.c_str(), what.c_str());
        ImGui::TextDisabled(tr("You can undo this with Ctrl+Z."));
        ImGui::PopTextWrapPos();
        if (iconButton("##deldo", Icon::Delete, tr("Delete bone"), false, bsz)) { doc.deleteSelection(); close = true; }
    } else if (delInfo.kind == 2) {
        std::string also;
        if (delInfo.childBones > 0) also += plural(delInfo.childBones, "child bone", "child bones");
        int otherImages = delInfo.slots - 1;
        if (otherImages > 0) also += std::string(also.empty() ? "" : " and ") + plural(otherImages, "other image", "other images");
        ImGui::TextWrapped("The image \"%s\" is parented to the bone \"%s\".", delInfo.name.c_str(), delInfo.boneName.c_str());
        if (!also.empty()) ImGui::TextWrapped("Deleting the bone as well also deletes %s.", also.c_str());
        ImGui::TextDisabled(tr("You can undo this with Ctrl+Z."));
        ImGui::PopTextWrapPos();
        if (iconButton("##delimg", Icon::Delete, tr("Delete image only"), false, bsz)) { doc.deleteSelection(); close = true; }
        ImGui::SameLine();
        if (iconButton("##delboth", Icon::Delete, tr("Image and bone"), false, bsz)) { doc.deleteSlotAndItsBone(); close = true; }
    } else {
        std::string what;
        if (delBonesAlso > 0) what += plural(delBonesAlso, "child bone", "child bones");
        if (delSlotsAlso > 0) what += std::string(what.empty() ? "" : " and ") + plural(delSlotsAlso, "image", "images");
        ImGui::TextWrapped("The selected objects have %s parented to them, which will be deleted too.", what.c_str());
        ImGui::TextDisabled(tr("You can undo this with Ctrl+Z."));
        ImGui::PopTextWrapPos();
        if (iconButton("##deldo", Icon::Delete, tr("Delete all"), false, bsz)) { doc.deleteSelection(); close = true; }
    }
    ImGui::SameLine();
    if (iconButton("##delcancel", Icon::Undo, tr("Cancel"), false, bsz) || ImGui::IsKeyPressed(ImGuiKey_Escape)) close = true;
    if (close) { ImGui::CloseCurrentPopup(); delState = 0; }
    ImGui::EndPopup();
}

// ---- the context menu ----

void Editor::openObjectMenu(ImVec2 mouse) {
    int bone = pickBone(mouse);
    int slot = bone < 0 ? pickSlot(mouse) : -1;
    if (bone >= 0) {
        Sel s{SelType::Bone, bone, ""};
        if (!doc.isSelected(s)) doc.select(s);
        objMenuTarget = 1;
    } else if (slot >= 0) {
        const Attachment* a = doc.sk.attachmentFor(doc.sk.slots[slot]);
        Sel s = doc.mode == Mode::Setup && a ? Sel{SelType::Attachment, slot, a->name} : Sel{SelType::Slot, slot, ""};
        if (!doc.isSelected(s) && !doc.isSelected({SelType::Slot, slot, ""})) doc.select(s);
        objMenuTarget = 2;
    } else {
        objMenuTarget = 0;
    }
    ImGui::OpenPopup("##objmenu");
}

void Editor::objectContextMenu() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8 * uiScale, 8 * uiScale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * uiScale, 5 * uiScale));
    bool open = ImGui::BeginPopup("##objmenu");
    ImGui::PopStyleVar(2);
    if (!open) return;
    const bool setup = doc.mode == Mode::Setup;
    const bool image = objMenuTarget == 2;
    const bool bone = objMenuTarget == 1;
    const bool any = image || bone;
    int slotIndex = image ? doc.selectionSlot() : -1;
    Attachment* att = nullptr;
    if (image) { int s = slotIndex; att = selectedAttachment(&s); if (!att && slotIndex >= 0) att = const_cast<Attachment*>(doc.sk.attachmentFor(doc.sk.slots[slotIndex])); }
    Document::DeleteInfo info = doc.describeDelete();

    if (image) {
        bool isRegion = att && att->type == AttachmentType::Region, isMesh = att && att->type == AttachmentType::Mesh;
        if (ImGui::MenuItem(tr("Mesh"), nullptr, isMesh, setup && (isRegion || isMesh))) {
            if (isRegion) doc.convertToMesh(slotIndex, att->name);
            else { meshMode = MeshMode::Modify; showMeshTools = true; }
        }
        if (ImGui::BeginMenu(tr("Replace Image"), setup && att && (isRegion || isMesh || att->type == AttachmentType::LinkedMesh) && !doc.imageNames.empty())) {
            for (auto& img : doc.imageNames)
                if (ImGui::MenuItem(img.c_str(), nullptr, img == att->imagePath())) {
                    doc.checkpoint("Replace image");
                    att->path = img == att->name ? "" : img;
                    int w, h;
                    if (att->type == AttachmentType::Region && loadImageFileSize(doc.imageFileFor(img), w, h)) { att->width = (float)w; att->height = (float)h; }
                }
            ImGui::EndMenu();
        }
        ImGui::Separator();
    }
    if (any) {
        if (ImGui::MenuItem(tr("Set Parent"), "P", false, setup || bone)) { pickParent = true; setStatus("Click the new parent bone (Esc cancels)"); }
        ImGui::Separator();
    }
    if (ImGui::MenuItem(tr("Copy"), "Ctrl+C", false, any)) setStatus(doc.copySelection() ? "Copied" : "Nothing to copy");
    if (ImGui::MenuItem(tr("Paste"), "Ctrl+V", false, !Document::clipboard().empty())) { int n = doc.pasteObjects(false); setStatus("Pasted " + std::to_string(n) + " object(s)"); }
    if (ImGui::MenuItem(tr("Paste Special"), nullptr, false, !Document::clipboard().empty() && setup)) { int n = doc.pasteObjects(true); setStatus("Pasted " + std::to_string(n) + " object(s) under the selected bone"); }
    if (ImGui::MenuItem(tr("Duplicate"), "Ctrl+D", false, any)) doc.duplicateSelection();
    if (image && ImGui::MenuItem(tr("Delete Slot"), "Delete|Backspace", false, true)) requestDelete(); // asks first when the image hangs on a bone
    if (any && ImGui::MenuItem(tr("Delete Bone"), nullptr, false, setup && (image ? info.canDeleteBone : info.kind == 1))) {
        if (image) doc.select({SelType::Bone, info.bone, ""});
        requestDelete();
    }
    ImGui::Separator();
    if (ImGui::MenuItem(tr("Rename..."), "F2", false, any)) beginRename();
    ImGui::Separator();
    if (ImGui::MenuItem(tr("Copy Transform"), "Ctrl+Shift+C", false, any)) setStatus(doc.copyTransform() ? "Transform copied" : "Nothing to copy");
    if (ImGui::MenuItem(tr("Paste Transform"), "Ctrl+Shift+V", false, any && doc.transformClipboard().kind != 0)) { int n = doc.pasteTransform(); setStatus(n ? "Transform pasted" : "That transform does not fit the selection"); }
    if (image) {
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Copy Slot Property"), nullptr, false, true)) setStatus(doc.copySlotProperty() ? "Slot property copied" : "Nothing to copy");
        if (ImGui::MenuItem(tr("Paste Slot Property"), nullptr, false, doc.slotPropertyClipboard().valid)) { int n = doc.pasteSlotProperty(); setStatus(n ? "Slot property pasted" : "Nothing selected"); }
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Bring Forward"), "=", false, setup)) doc.moveSlotsInDrawOrder(Document::DrawOrderMove::Forward);
        if (ImGui::MenuItem(tr("Send Backward"), "-", false, setup)) doc.moveSlotsInDrawOrder(Document::DrawOrderMove::Backward);
        if (ImGui::MenuItem(tr("Bring to Front"), "Ctrl+=", false, setup)) doc.moveSlotsInDrawOrder(Document::DrawOrderMove::ToFront);
        if (ImGui::MenuItem(tr("Send to Back"), "Ctrl+-", false, setup)) doc.moveSlotsInDrawOrder(Document::DrawOrderMove::ToBack);
    }
    if (any) {
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Hide"))) doc.setVisibility(false, false);
        if (ImGui::MenuItem(tr("Hide Others"))) doc.setVisibility(true, false);
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Lock"))) doc.setLocked(false, true);
        if (ImGui::MenuItem(tr("Lock Others"))) doc.setLocked(true, true);
        ImGui::Separator();
    }
    if (ImGui::MenuItem(tr("Show & Unlock All"))) doc.showAndUnlockAll();
    ImGui::EndPopup();
}

} // namespace osteon
