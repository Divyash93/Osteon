// Object operations behind the viewport's right-click menu: copy / paste, transform and slot-property copy,
// draw order, hide / lock, and what a delete would remove.
#include "Document.h"
#include <algorithm>
#include <set>

namespace osteon {

// ---- clipboards (shared by every document) ----

Document::ObjectClipboard& Document::clipboard() { static ObjectClipboard c; return c; }
Document::TransformClipboard& Document::transformClipboard() { static TransformClipboard c; return c; }
Document::SlotPropertyClipboard& Document::slotPropertyClipboard() { static SlotPropertyClipboard c; return c; }

bool Document::copySelection() {
    std::set<int> boneSet;
    for (int b : selectedBones()) {
        for (size_t i = 0; i < data.bones.size(); i++) if (data.isDescendant((int)i, b)) boneSet.insert((int)i); // the bone and everything below it
    }
    std::set<int> slotSet;
    for (int s : selectedSlots()) if (s >= 0 && s < (int)data.slots.size()) slotSet.insert(s);
    for (size_t s = 0; s < data.slots.size(); s++) if (boneSet.count(data.slots[s].bone)) slotSet.insert((int)s);
    if (boneSet.empty() && slotSet.empty()) return false;
    ObjectClipboard c;
    for (int b : boneSet) {
        if (b == 0) continue; // the root is never copied
        ObjectClipboard::B cb;
        cb.data = data.bones[b];
        cb.parent = data.bones[b].parent >= 0 ? data.bones[data.bones[b].parent].name : std::string();
        c.bones.push_back(cb);
    }
    for (int s : slotSet) {
        ObjectClipboard::S cs;
        cs.data = data.slots[s];
        cs.bone = data.bones[data.slots[s].bone].name;
        for (auto& sk : data.skins) {
            auto it = sk.attachments.find(s);
            if (it != sk.attachments.end()) cs.atts[sk.name] = it->second;
        }
        c.slots.push_back(cs);
    }
    if (c.bones.empty() && c.slots.empty()) return false;
    clipboard() = std::move(c);
    return true;
}

int Document::pasteObjects(bool special) {
    const ObjectClipboard c = clipboard();
    if (c.bones.empty() && c.slots.empty()) return 0;
    std::vector<int> sel = selectedBones();
    int under = special && !sel.empty() ? sel.back() : (special ? 0 : -1); // Paste Special: everything goes under the selected bone
    checkpoint(special ? "Paste special" : "Paste");
    std::map<std::string, std::string> renamed; // copied bone name -> its new name
    std::vector<std::string> newBones;
    // Bones keep their order (parents first); a copied parent maps to its new copy, otherwise the original parent, otherwise the root.
    std::set<std::string> copied;
    for (auto& b : c.bones) copied.insert(b.data.name);
    for (auto& b : c.bones) {
        BoneData nb = b.data;
        nb.name = data.uniqueBoneName(b.data.name);
        if (copied.count(b.parent)) nb.parent = data.findBone(renamed[b.parent]);
        else if (under >= 0) nb.parent = under;
        else { int p = data.findBone(b.parent); nb.parent = p >= 0 ? p : 0; }
        if (nb.parent < 0) nb.parent = 0;
        renamed[b.data.name] = nb.name;
        data.addBone(nb);
        newBones.push_back(nb.name);
    }
    data.sortBones();
    std::vector<std::string> newSlots;
    for (auto& s : c.slots) {
        SlotData ns = s.data;
        ns.name = data.uniqueSlotName(s.data.name);
        int bone = -1;
        auto it = renamed.find(s.bone);
        if (it != renamed.end()) bone = data.findBone(it->second);
        else if (under >= 0) bone = under;
        else bone = data.findBone(s.bone);
        ns.bone = bone >= 0 ? bone : 0;
        int si = data.addSlot(ns);
        for (auto& [skinName, atts] : s.atts) {
            int sk = data.findSkin(skinName);
            Skin& target = sk >= 0 ? data.skins[sk] : data.defaultSkin();
            for (auto& [n, a] : atts) target.set(si, a);
        }
        newSlots.push_back(ns.name);
    }
    selection.clear();
    for (auto& n : newBones) selection.push_back({SelType::Bone, data.findBone(n), ""});
    for (auto& n : newSlots) selection.push_back({SelType::Slot, data.findSlot(n), ""});
    updatePose();
    return (int)(newBones.size() + newSlots.size());
}

// ---- transform ----

bool Document::copyTransform() {
    Sel p = primary();
    TransformClipboard t;
    if (p.type == SelType::Bone && p.index >= 0 && p.index < (int)data.bones.size()) {
        t.kind = 1;
        LocalPose lp = bonePose(p.index);
        float v[7] = {lp.rotation, lp.x, lp.y, lp.sx, lp.sy, lp.shx, lp.shy};
        for (int i = 0; i < 7; i++) t.bone[i] = v[i];
    } else if (p.type == SelType::Slot || p.type == SelType::Attachment) {
        std::string name = p.type == SelType::Attachment ? p.name : (p.index >= 0 && p.index < (int)data.slots.size() ? data.slots[p.index].attachment : std::string());
        const Attachment* a = data.findAttachment(skin, p.index, name);
        if (!a || (a->type != AttachmentType::Region && a->type != AttachmentType::Point)) return false;
        t.kind = 2;
        t.x = a->x; t.y = a->y; t.rotation = a->rotation; t.scaleX = a->scaleX; t.scaleY = a->scaleY;
    } else return false;
    transformClipboard() = t;
    return true;
}

int Document::pasteTransform() {
    const TransformClipboard t = transformClipboard();
    if (t.kind == 0) return 0;
    int n = 0;
    if (t.kind == 1) {
        std::vector<int> bones = selectedBones();
        if (bones.empty()) return 0;
        checkpoint("Paste transform");
        LocalPose lp{t.bone[0], t.bone[1], t.bone[2], t.bone[3], t.bone[4], t.bone[5], t.bone[6]};
        for (int b : bones) { setBonePose(b, lp, (int)Channel::All); n++; }
        return n;
    }
    std::vector<Sel> targets;
    for (auto& s : selection) if (s.type == SelType::Slot || s.type == SelType::Attachment) targets.push_back(s);
    if (targets.empty()) return 0;
    checkpoint("Paste transform");
    for (auto& s : targets) {
        std::string name = s.type == SelType::Attachment ? s.name : (s.index >= 0 && s.index < (int)data.slots.size() ? data.slots[s.index].attachment : std::string());
        for (auto& sk : data.skins)
            if (Attachment* a = sk.get(s.index, name))
                if (a->type == AttachmentType::Region || a->type == AttachmentType::Point) {
                    a->x = t.x; a->y = t.y; a->rotation = t.rotation; a->scaleX = t.scaleX; a->scaleY = t.scaleY;
                    n++;
                    break;
                }
    }
    return n;
}

// ---- slot properties ----

bool Document::copySlotProperty() {
    std::vector<int> slots = selectedSlots();
    if (slots.empty() || slots.back() < 0 || slots.back() >= (int)data.slots.size()) return false;
    const SlotData& s = data.slots[slots.back()];
    SlotPropertyClipboard c;
    c.valid = true; c.color = s.color; c.dark = s.dark; c.hasDark = s.hasDark; c.blend = s.blend;
    slotPropertyClipboard() = c;
    return true;
}

int Document::pasteSlotProperty() {
    const SlotPropertyClipboard c = slotPropertyClipboard();
    if (!c.valid) return 0;
    std::vector<int> slots = selectedSlots();
    if (slots.empty()) return 0;
    checkpoint("Paste slot property");
    int n = 0;
    for (int s : slots) {
        if (s < 0 || s >= (int)data.slots.size()) continue;
        SlotData& sd = data.slots[s];
        sd.color = c.color; sd.dark = c.dark; sd.hasDark = c.hasDark; sd.blend = c.blend;
        n++;
    }
    return n;
}

// ---- draw order ----

void Document::moveSlotsInDrawOrder(DrawOrderMove how) {
    std::vector<int> slots = selectedSlots();
    std::sort(slots.begin(), slots.end());
    slots.erase(std::unique(slots.begin(), slots.end()), slots.end());
    if (slots.empty()) return;
    checkpoint("Draw order");
    std::vector<std::string> names;
    for (int s : slots) names.push_back(data.slots[s].name);
    const int last = (int)data.slots.size() - 1;
    auto order = [&](bool descending) { // current indices of the selected slots
        std::vector<int> r;
        for (auto& n : names) r.push_back(data.findSlot(n));
        std::sort(r.begin(), r.end());
        if (descending) std::reverse(r.begin(), r.end());
        return r;
    };
    switch (how) {
    case DrawOrderMove::Forward: // towards the top (drawn later); the topmost moves first so they do not pass each other
        for (int s : order(true)) if (s < last && std::find(names.begin(), names.end(), data.slots[s + 1].name) == names.end()) data.moveSlot(s, s + 1);
        break;
    case DrawOrderMove::Backward:
        for (int s : order(false)) if (s > 0 && std::find(names.begin(), names.end(), data.slots[s - 1].name) == names.end()) data.moveSlot(s, s - 1);
        break;
    case DrawOrderMove::ToFront: {
        int top = last;
        for (int s : order(true)) { data.moveSlot(s, top); top--; }
        break;
    }
    case DrawOrderMove::ToBack: {
        int bottom = 0;
        for (int s : order(false)) { data.moveSlot(s, bottom); bottom++; }
        break;
    }
    }
    selection.clear();
    for (auto& n : names) selection.push_back({SelType::Slot, data.findSlot(n), ""});
}

// ---- hide / lock ----

void Document::setVisibility(bool others, bool visible) {
    std::set<int> bones, slots;
    for (int b : selectedBones()) bones.insert(b);
    for (int s : selectedSlots()) slots.insert(s);
    if (bones.empty() && slots.empty()) return;
    checkpoint(others ? (visible ? "Show others" : "Hide others") : (visible ? "Show" : "Hide"));
    for (size_t i = 0; i < data.bones.size(); i++) if (bones.count((int)i) != others) data.bones[i].visible = visible;
    for (size_t i = 0; i < data.slots.size(); i++) if (slots.count((int)i) != others) data.slots[i].visible = visible;
    // Hiding others must not hide the selection itself.
    if (others && !visible) { for (int b : bones) data.bones[b].visible = true; for (int s : slots) data.slots[s].visible = true; }
}

void Document::setLocked(bool others, bool locked) {
    std::set<int> bones, slots;
    for (int b : selectedBones()) bones.insert(b);
    for (int s : selectedSlots()) slots.insert(s);
    if (bones.empty() && slots.empty()) return;
    checkpoint(others ? (locked ? "Lock others" : "Unlock others") : (locked ? "Lock" : "Unlock"));
    for (size_t i = 0; i < data.bones.size(); i++) if (bones.count((int)i) != others) data.bones[i].selectable = !locked;
    for (size_t i = 0; i < data.slots.size(); i++) if (slots.count((int)i) != others) data.slots[i].locked = locked;
    if (others && locked) { for (int b : bones) data.bones[b].selectable = true; for (int s : slots) data.slots[s].locked = false; }
}

void Document::showAndUnlockAll() {
    checkpoint("Show and unlock all");
    for (auto& b : data.bones) { b.visible = true; b.selectable = true; }
    for (auto& s : data.slots) { s.visible = true; s.locked = false; }
}

// ---- what a delete would remove ----

Document::DeleteInfo Document::describeDelete() const {
    DeleteInfo d;
    Sel p = primary();
    auto subtree = [&](int bone, int& children, int& slots) {
        children = 0; slots = 0;
        for (size_t i = 0; i < data.bones.size(); i++) if ((int)i != bone && data.isDescendant((int)i, bone)) children++;
        for (auto& s : data.slots) if (data.isDescendant(s.bone, bone)) slots++;
    };
    if (p.type == SelType::Bone && p.index > 0 && p.index < (int)data.bones.size()) {
        d.kind = 1;
        d.name = data.bones[p.index].name;
        subtree(p.index, d.childBones, d.slots);
    } else if ((p.type == SelType::Slot || p.type == SelType::Attachment) && p.index >= 0 && p.index < (int)data.slots.size()) {
        d.kind = 2;
        d.name = data.slots[p.index].name;
        int b = data.slots[p.index].bone;
        d.boneName = data.bones[b].name;
        d.bone = b;
        d.canDeleteBone = b > 0;
        if (b > 0) subtree(b, d.childBones, d.slots);
    }
    return d;
}

void Document::deleteSlotAndItsBone() {
    Sel p = primary();
    if ((p.type != SelType::Slot && p.type != SelType::Attachment) || p.index < 0 || p.index >= (int)data.slots.size()) return;
    int bone = data.slots[p.index].bone;
    if (bone <= 0) { deleteSelection(); return; } // the root stays: just the slot
    selection = {{SelType::Bone, bone, ""}};
    deleteSelection(); // removes the bone, its child bones and every slot on them (this slot included)
}

} // namespace osteon
