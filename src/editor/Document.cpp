#include "Document.h"
#include "Geometry.h"
#include "Gfx.h"
#include "Psd.h"
#include "Util.h"
#include "json.hpp"
#include <cstring>

namespace osteon {

static constexpr size_t kMaxUndo = 200;

// ---- history ----

void Document::checkpoint(const std::string& label) {
    cancelEdit();
    undoStack.push_back({data, selection, label});
    if (undoStack.size() > kMaxUndo) undoStack.pop_front();
    redoStack.clear();
    dirty = true;
}

void Document::beginEdit(const std::string& label) {
    if (!openEdit) openEdit = Snapshot{data, selection, label};
}

void Document::commitEdit() {
    if (!openEdit) return;
    undoStack.push_back(std::move(*openEdit));
    openEdit.reset();
    if (undoStack.size() > kMaxUndo) undoStack.pop_front();
    redoStack.clear();
    dirty = true;
}

void Document::cancelEdit() { openEdit.reset(); }

void Document::undo() {
    commitEdit();
    if (undoStack.empty()) return;
    redoStack.push_back({data, selection, undoStack.back().label});
    data = std::move(undoStack.back().data);
    selection = undoStack.back().selection;
    undoStack.pop_back();
    clearPending();
    dirty = true;
    validateSelection();
}

void Document::redo() {
    if (redoStack.empty()) return;
    undoStack.push_back({data, selection, redoStack.back().label});
    data = std::move(redoStack.back().data);
    selection = redoStack.back().selection;
    redoStack.pop_back();
    clearPending();
    dirty = true;
    validateSelection();
}

// ---- pose ----

Animation* Document::currentAnimation() {
    return animation >= 0 && animation < (int)data.animations.size() ? &data.animations[animation] : nullptr;
}

float Document::animationDuration() {
    Animation* a = currentAnimation();
    return a ? a->duration() : 0;
}

void Document::setTime(float t) {
    if (t != time) clearPending();
    time = std::max(0.0f, t);
}

void Document::setMode(Mode m) {
    if (m == mode) return;
    commitEdit();
    mode = m;
    playing = false;
    clearPending();
    if (m == Mode::Animate && animation < 0 && !data.animations.empty()) animation = 0;
    if (m == Mode::Setup && (tool == Tool::Pose)) tool = Tool::Rotate;
}

void Document::updatePose(float physicsDelta) {
    if (skin >= (int)data.skins.size()) skin = 0;
    if (sk.needsRebuild(data)) {
        sk.skin = skin;
        sk.build(data);
    } else if (sk.skin != skin) {
        sk.skin = skin;
        sk.updateCacheOrder();
    } else {
        // Constraint order/targets and inherit flags can change without changing counts.
        sk.updateCacheOrder();
    }
    Animation* anim = mode == Mode::Animate ? currentAnimation() : nullptr;
    // Stopped with Simulate off (or Deterministic on): the physics is re-run from frame 0 up to the current frame.
    if (anim && !playing && !sk.physics.empty() && (deterministic || !simulate) && (time != simTime || animation != simAnim || simInvalid)) {
        const float step = 1.0f / 60;
        int steps = std::min(600, (int)std::floor(time / step));
        sk.time = 0;
        sk.setToSetupPose();
        applyAnimation(*anim, sk, 0, 0, false, nullptr, 1, MixBlend::Setup);
        sk.updateWorldTransform(Physics::Reset);
        for (int i = 1; i <= steps; i++) {
            sk.setToSetupPose();
            applyAnimation(*anim, sk, (i - 1) * step, i * step, false, nullptr, 1, MixBlend::Setup);
            sk.update(step);
            sk.updateWorldTransform(Physics::Update);
        }
        simTime = time; simAnim = animation; simInvalid = false;
    }
    sk.setToSetupPose();
    if (anim) applyAnimation(*anim, sk, time, time, false, nullptr, 1, MixBlend::Setup);
    for (auto& [bi, p] : pending) {
        if (bi >= (int)sk.bones.size()) continue;
        Bone& b = sk.bones[bi];
        if (p.channels & (int)Channel::Rotate) b.rotation = p.rotation;
        if (p.channels & (int)Channel::Translate) { b.x = p.x; b.y = p.y; }
        if (p.channels & (int)Channel::Scale) { b.scaleX = p.sx; b.scaleY = p.sy; }
        if (p.channels & (int)Channel::Shear) { b.shearX = p.shx; b.shearY = p.shy; }
    }
    for (auto& [key, pd] : pendingDeform) { // show unkeyed vertex moves until keyed or the time changes
        if (key.first >= (int)sk.slots.size()) continue;
        sk.slots[key.first].deform = pd.offsets;
        sk.slots[key.first].deformFor = key.second;
    }
    sk.update(physicsDelta);
    Physics phys = Physics::None;
    if (mode == Mode::Animate) phys = playing ? Physics::Update : Physics::Pose;
    sk.updateWorldTransform(phys);
}

// ---- selection ----

bool Document::isSelected(const Sel& s) const {
    return std::find(selection.begin(), selection.end(), s) != selection.end();
}

bool Document::moveBone(int bone, int target, bool after) {
    int n = (int)data.bones.size();
    if (bone <= 0 || bone >= n || target <= 0 || target >= n || bone == target || data.bones[bone].parent != data.bones[target].parent) return false;
    std::string name = data.bones[bone].name;
    checkpoint("Move bone");
    if (!data.moveBone(bone, target, after)) return false;
    selection = {{SelType::Bone, data.findBone(name), ""}};
    sk = Skeleton(); // bone indices changed: rebuild the runtime skeleton
    clearPending();
    updatePose();
    dirty = true;
    return true;
}

bool Document::moveSlotNextTo(int slot, int target, bool after) {
    int n = (int)data.slots.size();
    if (slot < 0 || slot >= n || target < 0 || target >= n || slot == target) return false;
    if (data.slots[slot].bone != data.slots[target].bone) reparentSlot(slot, data.slots[target].bone); // keeps its place on screen
    int to = after ? (slot < target ? target : target + 1) : (slot < target ? target - 1 : target);
    if (to == slot) return true; // already there (only the bone may have changed)
    checkpoint("Draw order");
    data.moveSlot(slot, to);
    selection = {{SelType::Slot, std::max(0, std::min(n - 1, to)), ""}};
    sk = Skeleton();
    clearPending();
    updatePose();
    dirty = true;
    return true;
}

void Document::selectAll() {
    selection.clear();
    for (size_t i = 0; i < data.bones.size(); i++)
        if (data.bones[i].visible) selection.push_back({SelType::Bone, (int)i, ""});
    for (size_t i = 0; i < data.slots.size(); i++)
        if (data.slots[i].visible) selection.push_back({SelType::Slot, (int)i, ""});
}

void Document::selectRange(const Sel& from, const Sel& to, const std::vector<Sel>& order, bool keepExisting) {
    auto a = std::find(order.begin(), order.end(), from), b = std::find(order.begin(), order.end(), to);
    if (a == order.end() || b == order.end()) { select(to, keepExisting); return; }
    if (!keepExisting) selection.clear();
    for (auto it = std::min(a, b); it <= std::max(a, b); ++it)
        if (!isSelected(*it)) selection.push_back(*it);
}

void Document::select(const Sel& s, bool additive) {
    if (!additive) selection.clear();
    auto it = std::find(selection.begin(), selection.end(), s);
    if (it != selection.end()) {
        if (additive) selection.erase(it);
        return;
    }
    if (s.type != SelType::None) selection.push_back(s);
}

std::vector<int> Document::selectedBones() const {
    std::vector<int> r;
    for (auto& s : selection)
        if (s.type == SelType::Bone) r.push_back(s.index);
    return r;
}

std::vector<int> Document::selectedSlots() const {
    std::vector<int> r;
    for (auto& s : selection)
        if (s.type == SelType::Slot || s.type == SelType::Attachment) r.push_back(s.index);
    return r;
}

int Document::selectionSlot() const {
    Sel p = primary();
    return p.type == SelType::Slot || p.type == SelType::Attachment ? p.index : -1;
}

void Document::validateSelection() {
    std::vector<Sel> keep;
    for (auto& s : selection) {
        bool ok = true;
        switch (s.type) {
        case SelType::Bone: ok = s.index >= 0 && s.index < (int)data.bones.size(); break;
        case SelType::Slot: ok = s.index >= 0 && s.index < (int)data.slots.size(); break;
        case SelType::Attachment:
            ok = s.index >= 0 && s.index < (int)data.slots.size() && data.findAttachment(skin, s.index, s.name);
            break;
        case SelType::Ik: ok = s.index < (int)data.ik.size(); break;
        case SelType::Transform: ok = s.index < (int)data.transforms.size(); break;
        case SelType::Path: ok = s.index < (int)data.paths.size(); break;
        case SelType::Physics: ok = s.index < (int)data.physics.size(); break;
        case SelType::Skin: ok = s.index < (int)data.skins.size(); break;
        case SelType::Animation: ok = s.index < (int)data.animations.size(); break;
        case SelType::Event: ok = s.index < (int)data.events.size(); break;
        default: break;
        }
        if (ok) keep.push_back(s);
    }
    selection = keep;
    if (animation >= (int)data.animations.size()) animation = data.animations.empty() ? -1 : 0;
}

// ---- bone edits ----

Document::LocalPose Document::bonePose(int bi) const {
    if (mode == Mode::Setup || bi >= (int)sk.bones.size()) {
        const BoneData& b = data.bones[bi];
        return {b.rotation, b.x, b.y, b.scaleX, b.scaleY, b.shearX, b.shearY};
    }
    const Bone& b = sk.bones[bi];
    return {b.rotation, b.x, b.y, b.scaleX, b.scaleY, b.shearX, b.shearY};
}

void Document::setBonePose(int bi, const LocalPose& p, int ch) {
    if (bi < 0 || bi >= (int)data.bones.size()) return;
    if (mode == Mode::Setup) {
        BoneData& b = data.bones[bi];
        if (ch & (int)Channel::Rotate) b.rotation = p.rotation;
        if (ch & (int)Channel::Translate) { b.x = p.x; b.y = p.y; }
        if (ch & (int)Channel::Scale) { b.scaleX = p.sx; b.scaleY = p.sy; }
        if (ch & (int)Channel::Shear) { b.shearX = p.shx; b.shearY = p.shy; }
        dirty = true;
        return;
    }
    Pending& pd = pending[bi];
    if (!pd.channels) {
        LocalPose cur = bonePose(bi);
        pd.rotation = cur.rotation; pd.x = cur.x; pd.y = cur.y; pd.sx = cur.sx; pd.sy = cur.sy; pd.shx = cur.shx; pd.shy = cur.shy;
    }
    pd.channels |= ch;
    if (ch & (int)Channel::Rotate) pd.rotation = p.rotation;
    if (ch & (int)Channel::Translate) { pd.x = p.x; pd.y = p.y; }
    if (ch & (int)Channel::Scale) { pd.sx = p.sx; pd.sy = p.sy; }
    if (ch & (int)Channel::Shear) { pd.shx = p.shx; pd.shy = p.shy; }
    if (autoKey) keyBone(bi, ch);
}

void Document::poseDrag(int bi, const LocalPose& start, Vec2 startOrigin, Vec2 grab, Vec2 mouse) {
    if (bi < 0 || bi >= (int)sk.bones.size()) return;
    LocalPose p = start;
    if (data.bones[bi].parent < 0) { // nothing to turn about: the root moves
        Vec2 l = sk.bones[bi].worldToParent(sk, startOrigin + (mouse - grab));
        p.x = l.x; p.y = l.y;
        setBonePose(bi, p, (int)Channel::Translate);
        return;
    }
    // Turn about the bone's own joint by the angle the mouse has swept around it.
    float a0 = atan2Deg(grab.y - startOrigin.y, grab.x - startOrigin.x);
    float a1 = atan2Deg(mouse.y - startOrigin.y, mouse.x - startOrigin.x);
    float d = wrapDeg(a1 - a0);
    const WorldMatrix& pm = sk.parentMatrix(bi);
    bool reflect = pm.a * pm.d - pm.b * pm.c < 0; // a mirrored parent flips the sense of rotation
    p.rotation = start.rotation + (reflect ? -d : d);
    setBonePose(bi, p, (int)Channel::Rotate);
}

static float frameTime(float t, float fps) { return fps > 0 ? std::round(t * fps) / fps : t; }

void Document::keyBone(int bi, int ch) {
    if (bi < 0 || bi >= (int)data.bones.size()) return;
    if (!currentAnimation()) {
        if (data.animations.empty()) createAnimation("animation");
        animation = 0;
    }
    Animation& an = *currentAnimation();
    const BoneData& s = data.bones[bi];
    LocalPose p = bonePose(bi);
    auto it = pending.find(bi);
    if (it != pending.end()) {
        const Pending& pd = it->second;
        if (pd.channels & (int)Channel::Rotate) p.rotation = pd.rotation;
        if (pd.channels & (int)Channel::Translate) { p.x = pd.x; p.y = pd.y; }
        if (pd.channels & (int)Channel::Scale) { p.sx = pd.sx; p.sy = pd.sy; }
        if (pd.channels & (int)Channel::Shear) { p.shx = pd.shx; p.shy = pd.shy; }
    }
    float t = frameTime(time, data.fps);
    if (adjust && !adjustKeys.empty()) {
        // Adjust: the edit's change (new value minus what the animation gives now) is added to every selected key of that property.
        auto shiftKeys = [&](TimelineType type, int vc, const float* now, bool ratio) {
            Timeline* cur = nullptr;
            for (auto& tl : an.timelines) if (tl.type == type && tl.target == bi) cur = &tl;
            float old[2] = {ratio ? 1.0f : 0.0f, ratio ? 1.0f : 0.0f}; // what the animation gives now, read before any key moves
            if (cur) for (int v = 0; v < vc; v++) old[v] = timelineValue(*cur, v, time);
            for (auto& [ti, ki] : adjustKeys) {
                if (ti < 0 || ti >= (int)an.timelines.size()) continue;
                Timeline& tl = an.timelines[ti];
                if (tl.type != type || tl.target != bi || ki < 0 || ki >= (int)tl.keys.size()) continue;
                for (int v = 0; v < vc; v++) {
                    if (ratio) tl.keys[ki].v[v] *= old[v] != 0 ? now[v] / old[v] : 1.0f;
                    else tl.keys[ki].v[v] += now[v] - old[v];
                }
            }
        };
        if (ch & (int)Channel::Rotate) { float n[1] = {p.rotation - s.rotation}; shiftKeys(TimelineType::Rotate, 1, n, false); }
        if (ch & (int)Channel::Translate) { float n[2] = {p.x - s.x, p.y - s.y}; shiftKeys(TimelineType::Translate, 2, n, false); }
        if (ch & (int)Channel::Scale) { float n[2] = {s.scaleX != 0 ? p.sx / s.scaleX : 0, s.scaleY != 0 ? p.sy / s.scaleY : 0}; shiftKeys(TimelineType::Scale, 2, n, true); }
        if (ch & (int)Channel::Shear) { float n[2] = {p.shx - s.shearX, p.shy - s.shearY}; shiftKeys(TimelineType::Shear, 2, n, false); }
        if (it != pending.end()) { it->second.channels &= ~ch; if (!it->second.channels) pending.erase(it); }
        dirty = true;
        return;
    }
    if (ch & (int)Channel::Rotate) an.get(TimelineType::Rotate, bi).setKey(t).v[0] = p.rotation - s.rotation;
    if (ch & (int)Channel::Translate) {
        Key& k = an.get(TimelineType::Translate, bi).setKey(t);
        k.v[0] = p.x - s.x; k.v[1] = p.y - s.y;
    }
    if (ch & (int)Channel::Scale) {
        Key& k = an.get(TimelineType::Scale, bi).setKey(t);
        k.v[0] = s.scaleX != 0 ? p.sx / s.scaleX : 0;
        k.v[1] = s.scaleY != 0 ? p.sy / s.scaleY : 0;
    }
    if (ch & (int)Channel::Shear) {
        Key& k = an.get(TimelineType::Shear, bi).setKey(t);
        k.v[0] = p.shx - s.shearX; k.v[1] = p.shy - s.shearY;
    }
    if (it != pending.end()) {
        it->second.channels &= ~ch;
        if (!it->second.channels) pending.erase(it);
    }
    dirty = true;
}

void Document::keyBoneIfAuto(int bi, int ch) { if (mode == Mode::Animate && autoKey) keyBone(bi, ch); }

int Document::keyState(int target, bool slot) const {
    if (slot)
        for (auto& [key, pd] : pendingDeform) if (key.first == target) return 3;
    if (animation < 0 || animation >= (int)data.animations.size()) return 0;
    float t = frameTime(time, data.fps);
    int state = 0;
    for (auto& tl : data.animations[animation].timelines) {
        if (tl.target != target) continue;
        bool isSlot = tl.type == TimelineType::Attachment || tl.type == TimelineType::RGBA || tl.type == TimelineType::RGB ||
                      tl.type == TimelineType::Alpha || tl.type == TimelineType::RGBA2 || tl.type == TimelineType::RGB2 ||
                      tl.type == TimelineType::Deform; // a mesh deform marks its slot too, as in Spine
        bool isBone = tl.type == TimelineType::Rotate || tl.type == TimelineType::Translate || tl.type == TimelineType::TranslateX ||
                      tl.type == TimelineType::TranslateY || tl.type == TimelineType::Scale || tl.type == TimelineType::ScaleX ||
                      tl.type == TimelineType::ScaleY || tl.type == TimelineType::Shear || tl.type == TimelineType::ShearX ||
                      tl.type == TimelineType::ShearY;
        if (slot ? !isSlot : !isBone) continue;
        if (tl.findKey(t) >= 0) return 2;
        if (!tl.keys.empty()) state = 1;
    }
    return state;
}

std::string Document::skinNameFor(int slot, const std::string& attachment) const {
    int s = skin >= 0 && skin < (int)data.skins.size() && data.skins[skin].get(slot, attachment) ? skin : 0;
    return data.skins.empty() ? std::string("default") : data.skins[s].name;
}

void Document::setDeform(int slot, const std::string& attachment, const std::vector<float>& offsets) {
    if (!currentAnimation()) return;
    std::string skinName = skinNameFor(slot, attachment);
    if (autoKey) {
        currentAnimation()->get(TimelineType::Deform, slot, skinName, attachment).setKey(frameTime(time, data.fps)).deform = offsets;
        pendingDeform.erase({slot, attachment});
    } else pendingDeform[{slot, attachment}] = {skinName, offsets};
    dirty = true;
}

void Document::keyDeform(int slot, const std::string& attachment) {
    auto it = pendingDeform.find({slot, attachment});
    if (it == pendingDeform.end() || !currentAnimation()) return;
    currentAnimation()->get(TimelineType::Deform, slot, it->second.skin, attachment).setKey(frameTime(time, data.fps)).deform = it->second.offsets;
    pendingDeform.erase(it);
    dirty = true;
}

int Document::deformKeyState(int slot, const std::string& attachment) const {
    if (pendingDeform.count({slot, attachment})) return 3;
    if (animation < 0 || animation >= (int)data.animations.size()) return 0;
    float t = frameTime(time, data.fps);
    int state = 0;
    for (auto& tl : data.animations[animation].timelines) {
        if (tl.type != TimelineType::Deform || tl.target != slot || tl.attachment != attachment) continue;
        if (tl.findKey(t) >= 0) return 2;
        if (!tl.keys.empty()) state = 1;
    }
    return state;
}

bool Document::hasKey(int bi, TimelineType t) const {
    if (animation < 0 || animation >= (int)data.animations.size()) return false;
    for (auto& tl : data.animations[animation].timelines)
        if (tl.type == t && tl.target == bi) return tl.findKey(frameTime(time, data.fps)) >= 0;
    return false;
}

void Document::keySlotColor(int si) {
    if (!currentAnimation()) return;
    Key& k = currentAnimation()->get(TimelineType::RGBA, si).setKey(frameTime(time, data.fps));
    const Color& c = sk.slots[si].color;
    k.v[0] = c.r; k.v[1] = c.g; k.v[2] = c.b; k.v[3] = c.a;
    dirty = true;
}

void Document::keySlotAttachment(int si) {
    if (!currentAnimation()) return;
    currentAnimation()->get(TimelineType::Attachment, si).setKey(frameTime(time, data.fps)).str = sk.slots[si].attachment;
    dirty = true;
}

void Document::setSlotAttachment(int si, const std::string& name) {
    if (mode == Mode::Setup) {
        data.slots[si].attachment = name;
    } else {
        if (!currentAnimation()) return;
        currentAnimation()->get(TimelineType::Attachment, si).setKey(frameTime(time, data.fps)).str = name;
    }
    dirty = true;
}

void Document::setSlotColor(int si, const Color& c) {
    if (mode == Mode::Setup) data.slots[si].color = c;
    else if (currentAnimation()) {
        Key& k = currentAnimation()->get(TimelineType::RGBA, si).setKey(frameTime(time, data.fps));
        k.v[0] = c.r; k.v[1] = c.g; k.v[2] = c.b; k.v[3] = c.a;
    }
    dirty = true;
}

void Document::deleteKeysAt(float t, const std::vector<int>& bones) {
    Animation* an = currentAnimation();
    if (!an) return;
    t = frameTime(t, data.fps);
    for (auto& tl : an->timelines) {
        if (!bones.empty() && !(isBoneTimeline(tl.type) && std::find(bones.begin(), bones.end(), tl.target) != bones.end())) continue;
        int k = tl.findKey(t);
        if (k >= 0) tl.keys.erase(tl.keys.begin() + k);
    }
    an->timelines.erase(std::remove_if(an->timelines.begin(), an->timelines.end(), [](const Timeline& tl) { return tl.keys.empty(); }),
                        an->timelines.end());
}

// ---- structure ----

// Local transform for a child of `parent` given world origin/direction/length (normal inherit).
static void localFromWorld(const Skeleton& sk, int parent, Vec2 start, Vec2 end, BoneData& b) {
    const WorldMatrix& p = parent >= 0 ? (const WorldMatrix&)sk.bones[parent] : sk.rootMatrix;
    float det = p.a * p.d - p.b * p.c;
    float id = std::fabs(det) < 1e-9f ? 0 : 1 / det;
    float dx = start.x - p.worldX, dy = start.y - p.worldY;
    b.x = (dx * p.d - dy * p.b) * id;
    b.y = (dy * p.a - dx * p.c) * id;
    Vec2 dir = end - start;
    float len = dir.len();
    if (len < 1e-3f) { b.length = 0; return; }
    float lx = (dir.x * p.d - dir.y * p.b) * id, ly = (dir.y * p.a - dir.x * p.c) * id;
    b.rotation = atan2Deg(ly, lx);
    float c = cosDeg(b.rotation), s = sinDeg(b.rotation);
    float wx = p.a * c + p.b * s, wy = p.c * c + p.d * s;
    float unit = std::sqrt(wx * wx + wy * wy);
    b.length = unit > 1e-6f ? len / unit : len;
}

void Document::dropDeformKeys(int slot, const std::string& attachment) {
    for (auto& an : data.animations)
        an.timelines.erase(std::remove_if(an.timelines.begin(), an.timelines.end(), [&](const Timeline& t) {
            return t.type == TimelineType::Deform && t.target == slot && t.attachment == attachment; }), an.timelines.end());
}

void Document::dropMeshBindings(int slot, Attachment& mesh) {
    if (mesh.isWeighted()) unbindWeights(mesh, sk, data.slots[slot].bone);
    dropDeformKeys(slot, mesh.name);
}

bool Document::deleteMeshVertices(int slot, const std::string& name, std::vector<int> idx) {
    Attachment* a = nullptr;
    for (int s : {skin, 0})
        if (!a && s >= 0 && s < (int)data.skins.size()) a = data.skins[s].get(slot, name);
    if (!a || a->type != AttachmentType::Mesh || a->isWeighted() || idx.empty()) return false;
    std::sort(idx.begin(), idx.end());
    idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
    int hullGone = 0;
    for (int i : idx) { if (i < 0 || i >= a->vertexCount()) return false; if (i < a->hullLength) hullGone++; }
    if (a->vertexCount() - (int)idx.size() < 3 || a->hullLength - hullGone < 3) return false;
    checkpoint("Delete vertices");
    for (int s : {skin, 0})
        if (s >= 0 && s < (int)data.skins.size()) if (Attachment* p = data.skins[s].get(slot, name)) { a = p; break; }
    for (auto it = idx.rbegin(); it != idx.rend(); ++it) {
        a->vertices.erase(a->vertices.begin() + *it * 2, a->vertices.begin() + *it * 2 + 2);
        a->uvs.erase(a->uvs.begin() + *it * 2, a->uvs.begin() + *it * 2 + 2);
    }
    a->worldVerticesLength = (int)a->vertices.size();
    a->hullLength -= hullGone;
    if (hullGone > 0) {
        // Removing a hull vertex joins its neighbours, which can leave other vertices outside the outline. Those would be
        // dropped from the mesh, so they join the hull instead (ordered around the middle) when that gives a clean outline.
        const int H = a->hullLength, n = a->vertexCount();
        std::vector<float> poly(a->vertices.begin(), a->vertices.begin() + H * 2);
        std::vector<int> outside;
        for (int j = H; j < n; j++)
            if (!pointInPolygon(poly, a->vertices[j * 2], a->vertices[j * 2 + 1])) outside.push_back(j);
        if (!outside.empty()) {
            std::vector<int> ring;
            for (int j = 0; j < H; j++) ring.push_back(j);
            ring.insert(ring.end(), outside.begin(), outside.end());
            float cx = 0, cy = 0;
            for (int j : ring) { cx += a->vertices[j * 2]; cy += a->vertices[j * 2 + 1]; }
            cx /= ring.size(); cy /= ring.size();
            std::sort(ring.begin(), ring.end(), [&](int x, int y) {
                return std::atan2(a->vertices[x * 2 + 1] - cy, a->vertices[x * 2] - cx) < std::atan2(a->vertices[y * 2 + 1] - cy, a->vertices[y * 2] - cx);
            });
            auto orient = [&](int i, int j, int k) {
                return (a->vertices[j * 2] - a->vertices[i * 2]) * (a->vertices[k * 2 + 1] - a->vertices[i * 2 + 1]) -
                       (a->vertices[j * 2 + 1] - a->vertices[i * 2 + 1]) * (a->vertices[k * 2] - a->vertices[i * 2]);
            };
            bool simple = true;
            const int m = (int)ring.size();
            for (int e = 0; e < m && simple; e++)
                for (int f = e + 2; f < m && simple; f++) {
                    if (e == 0 && f == m - 1) continue; // adjacent through the wrap
                    int a0 = ring[e], a1 = ring[(e + 1) % m], b0 = ring[f], b1 = ring[(f + 1) % m];
                    if (orient(a0, a1, b0) * orient(a0, a1, b1) < 0 && orient(b0, b1, a0) * orient(b0, b1, a1) < 0) simple = false;
                }
            if (simple) {
                std::vector<char> used(n, 0);
                std::vector<float> nv, nuv;
                for (int j : ring) { used[j] = 1; nv.push_back(a->vertices[j * 2]); nv.push_back(a->vertices[j * 2 + 1]); nuv.push_back(a->uvs[j * 2]); nuv.push_back(a->uvs[j * 2 + 1]); }
                for (int j = 0; j < n; j++)
                    if (!used[j]) { nv.push_back(a->vertices[j * 2]); nv.push_back(a->vertices[j * 2 + 1]); nuv.push_back(a->uvs[j * 2]); nuv.push_back(a->uvs[j * 2 + 1]); }
                a->vertices = nv;
                a->uvs = nuv;
                a->hullLength = m;
            }
        }
    }
    a->triangles = triangulateMesh(a->vertices, a->hullLength);
    a->edges.clear();
    for (int h = 0; h < a->hullLength; h++) { a->edges.push_back(h); a->edges.push_back((h + 1) % a->hullLength); }
    dropDeformKeys(slot, name);
    return true;
}

static float frameTime(float t, float fps);

void Document::keyPhysics(TimelineType type, int target, float value) {
    if (!currentAnimation()) return;
    currentAnimation()->get(type, target).setKey(frameTime(time, data.fps)).v[0] = value;
    dirty = true;
    simInvalid = true;
}

void Document::keyPhysicsReset(int target) {
    if (!currentAnimation()) return;
    currentAnimation()->get(TimelineType::PhysicsReset, target).setKey(frameTime(time, data.fps));
    dirty = true;
    simInvalid = true;
}

bool Document::hasPhysicsKey(TimelineType type, int target) const {
    if (animation < 0 || animation >= (int)data.animations.size()) return false;
    for (auto& tl : data.animations[animation].timelines)
        if (tl.type == type && tl.target == target && tl.findKey(frameTime(time, data.fps)) >= 0) return true;
    return false;
}

void Document::resetPhysics(int target) {
    osteon::resetPhysics(sk, target);
    if (mode == Mode::Animate && autoKey && currentAnimation()) { checkpoint("Key physics reset"); keyPhysicsReset(target); }
    simInvalid = true;
}

int Document::createBone(int parent, Vec2 ws, Vec2 we, const std::string& name) {
    checkpoint("New bone");
    BoneData b;
    b.name = data.uniqueBoneName(name);
    b.parent = parent;
    localFromWorld(sk, parent, ws, we, b);
    int idx = data.addBone(b);
    data.sortBones();
    idx = data.findBone(b.name);
    selection = {{SelType::Bone, idx, ""}};
    updatePose();
    return idx;
}

int Document::createSlot(int bone, const std::string& name) {
    SlotData s;
    s.name = data.uniqueSlotName(name);
    s.bone = bone;
    return data.addSlot(s);
}

void Document::addRegionAttachment(int si, const std::string& image, Vec2 world) {
    Attachment a;
    a.type = AttachmentType::Region;
    a.name = image;
    a.path = image;
    std::string file = imageFileFor(image);
    int w = 0, h = 0;
    if (!file.empty() && loadImageFileSize(file, w, h)) { a.width = (float)w; a.height = (float)h; }
    else { a.width = a.height = 64; }
    const Bone& b = sk.bones[data.slots[si].bone];
    Vec2 l = b.worldToLocal(world);
    a.x = l.x; a.y = l.y;
    // Keep the image upright and at 100% on screen regardless of the bone's world transform.
    a.rotation = -b.worldRotationX();
    float sx = b.worldScaleX(), sy = b.worldScaleY();
    if (sx > 1e-6f) a.scaleX = 1 / sx;
    if (sy > 1e-6f) a.scaleY = 1 / sy;
    data.defaultSkin().set(si, a);
    data.slots[si].attachment = a.name;
}

void Document::deleteSelection() {
    if (selection.empty()) return;
    checkpoint("Delete");
    // Delete in an order that keeps indices valid: highest index first per type.
    std::vector<Sel> sel = selection;
    std::sort(sel.begin(), sel.end(), [](const Sel& a, const Sel& b) { return a.type != b.type ? a.type > b.type : a.index > b.index; });
    for (auto& s : sel) {
        switch (s.type) {
        case SelType::Bone: if (s.index > 0 && s.index < (int)data.bones.size()) data.removeBone(s.index); break;
        case SelType::Slot: data.removeSlot(s.index); break;
        case SelType::Attachment:
            if (s.index < (int)data.slots.size()) {
                if (skin < (int)data.skins.size() && data.skins[skin].get(s.index, s.name)) data.skins[skin].remove(s.index, s.name);
                else data.defaultSkin().remove(s.index, s.name);
                if (data.slots[s.index].attachment == s.name) data.slots[s.index].attachment.clear();
            }
            break;
        case SelType::Ik: if (s.index < (int)data.ik.size()) data.ik.erase(data.ik.begin() + s.index); break;
        case SelType::Transform: if (s.index < (int)data.transforms.size()) data.transforms.erase(data.transforms.begin() + s.index); break;
        case SelType::Path: if (s.index < (int)data.paths.size()) data.paths.erase(data.paths.begin() + s.index); break;
        case SelType::Physics: if (s.index < (int)data.physics.size()) data.physics.erase(data.physics.begin() + s.index); break;
        case SelType::Skin: if (s.index > 0 && s.index < (int)data.skins.size()) data.skins.erase(data.skins.begin() + s.index); break;
        case SelType::Animation: if (s.index < (int)data.animations.size()) data.animations.erase(data.animations.begin() + s.index); break;
        case SelType::Event: if (s.index < (int)data.events.size()) data.events.erase(data.events.begin() + s.index); break;
        default: break;
        }
    }
    // Constraint timelines reference constraints by index; drop ones that no longer exist.
    for (auto& an : data.animations)
        an.timelines.erase(std::remove_if(an.timelines.begin(), an.timelines.end(), [&](const Timeline& tl) {
            switch (tl.type) {
            case TimelineType::Ik: return tl.target >= (int)data.ik.size();
            case TimelineType::Transform: return tl.target >= (int)data.transforms.size();
            case TimelineType::Path: case TimelineType::PathMix: return tl.target >= (int)data.paths.size();
            default: return isPhysicsTimeline(tl.type) && tl.target >= (int)data.physics.size();
            }
        }), an.timelines.end());
    selection.clear();
    validateSelection();
}

void Document::duplicateSelection() {
    Sel p = primary();
    switch (p.type) {
    case SelType::Animation: {
        checkpoint("Duplicate animation");
        Animation a = data.animations[p.index];
        a.name = data.uniqueAnimationName(a.name);
        data.animations.push_back(a);
        selection = {{SelType::Animation, (int)data.animations.size() - 1, ""}};
        break;
    }
    case SelType::Skin: {
        checkpoint("Duplicate skin");
        Skin s = data.skins[p.index];
        std::string base = s.name;
        s.name = base + " copy";
        for (int i = 2; data.findSkin(s.name) >= 0; i++) s.name = base + " copy" + std::to_string(i);
        data.skins.push_back(s);
        selection = {{SelType::Skin, (int)data.skins.size() - 1, ""}};
        break;
    }
    case SelType::Slot: {
        checkpoint("Duplicate slot");
        SlotData s = data.slots[p.index];
        s.name = data.uniqueSlotName(s.name);
        int ni = data.addSlot(s);
        data.moveSlot(ni, p.index + 1);
        ni = p.index + 1;
        for (auto& sk2 : data.skins) {
            auto it = sk2.attachments.find(p.index);
            if (it != sk2.attachments.end()) sk2.attachments[ni] = SlotAttachments(it->second);
        }
        selection = {{SelType::Slot, ni, ""}};
        break;
    }
    case SelType::Bone: {
        if (p.index <= 0) break;
        checkpoint("Duplicate bone");
        BoneData b = data.bones[p.index];
        b.name = data.uniqueBoneName(b.name);
        data.addBone(b);
        data.sortBones();
        selection = {{SelType::Bone, data.findBone(b.name), ""}};
        break;
    }
    case SelType::Attachment: {
        checkpoint("Duplicate attachment");
        const Attachment* a = data.findAttachment(skin, p.index, p.name);
        if (!a) break;
        Attachment c = *a;
        std::string base = c.name;
        for (int i = 2; data.findAttachment(skin, p.index, c.name); i++) c.name = base + std::to_string(i);
        Skin& target = skin < (int)data.skins.size() && data.skins[skin].get(p.index, p.name) ? data.skins[skin] : data.defaultSkin();
        target.set(p.index, c);
        selection = {{SelType::Attachment, p.index, c.name}};
        break;
    }
    default: break;
    }
}

bool Document::renameSelection(const std::string& n) {
    Sel p = primary();
    if (n.empty()) return false;
    auto taken = [&](auto& v, int self) {
        for (size_t i = 0; i < v.size(); i++) if ((int)i != self && v[i].name == n) return true;
        return false;
    };
    switch (p.type) {
    case SelType::Bone: if (taken(data.bones, p.index)) return false; checkpoint("Rename"); data.bones[p.index].name = n; return true;
    case SelType::Slot: if (taken(data.slots, p.index)) return false; checkpoint("Rename"); data.slots[p.index].name = n; return true;
    case SelType::Ik: if (taken(data.ik, p.index)) return false; checkpoint("Rename"); data.ik[p.index].name = n; return true;
    case SelType::Transform: if (taken(data.transforms, p.index)) return false; checkpoint("Rename"); data.transforms[p.index].name = n; return true;
    case SelType::Path: if (taken(data.paths, p.index)) return false; checkpoint("Rename"); data.paths[p.index].name = n; return true;
    case SelType::Physics: if (taken(data.physics, p.index)) return false; checkpoint("Rename"); data.physics[p.index].name = n; return true;
    case SelType::Skin:
        if (p.index == 0 || taken(data.skins, p.index)) return false;
        checkpoint("Rename");
        for (auto& an : data.animations) for (auto& tl : an.timelines) if (tl.skin == data.skins[p.index].name) tl.skin = n;
        data.skins[p.index].name = n;
        return true;
    case SelType::Animation: if (taken(data.animations, p.index)) return false; checkpoint("Rename"); data.animations[p.index].name = n; return true;
    case SelType::Event: {
        if (taken(data.events, p.index)) return false;
        checkpoint("Rename");
        std::string old = data.events[p.index].name;
        for (auto& an : data.animations) for (auto& tl : an.timelines) if (tl.type == TimelineType::Event) for (auto& k : tl.keys) if (k.str == old) k.str = n;
        data.events[p.index].name = n;
        return true;
    }
    case SelType::Attachment: {
        for (auto& s : data.skins) if (s.get(p.index, n)) return false;
        checkpoint("Rename");
        for (auto& s : data.skins)
            if (Attachment* a = s.get(p.index, p.name)) {
                Attachment c = *a;
                if (c.path.empty()) c.path = c.name; // keep the image reference
                c.name = n;
                s.remove(p.index, p.name);
                s.set(p.index, c);
            }
        if (data.slots[p.index].attachment == p.name) data.slots[p.index].attachment = n;
        for (auto& an : data.animations)
            for (auto& tl : an.timelines) {
                if (tl.type == TimelineType::Attachment && tl.target == p.index) for (auto& k : tl.keys) if (k.str == p.name) k.str = n;
                if (tl.type == TimelineType::Deform && tl.target == p.index && tl.attachment == p.name) tl.attachment = n;
            }
        selection.back().name = n;
        return true;
    }
    default: return false;
    }
}

void Document::reparentBones(const std::vector<int>& bones, int newParent) {
    if (bones.empty() || newParent < 0) return;
    checkpoint("Set parent");
    std::vector<std::string> names;
    std::vector<WorldMatrix> worlds;
    for (int b : bones) {
        if (b <= 0 || data.isDescendant(newParent, b)) continue;
        names.push_back(data.bones[b].name);
        worlds.push_back(sk.bones[b]);
    }
    std::string parentName = data.bones[newParent].name;
    for (auto& n : names) data.setParent(data.findBone(n), data.findBone(parentName));
    // Recompute local transforms so world placement is unchanged (setup pose).
    Skeleton tmp(data);
    tmp.updateWorldTransform(Physics::None);
    for (size_t i = 0; i < names.size(); i++) {
        int bi = data.findBone(names[i]);
        Bone& b = tmp.bones[bi];
        (WorldMatrix&)b = worlds[i];
        b.updateAppliedTransform(tmp);
        BoneData& bd = data.bones[bi];
        bd.x = b.ax; bd.y = b.ay; bd.rotation = b.arotation;
        bd.scaleX = b.ascaleX; bd.scaleY = b.ascaleY; bd.shearX = b.ashearX; bd.shearY = b.ashearY;
        tmp.setBonesToSetupPose();
        tmp.updateWorldTransform(Physics::None);
    }
    selection.clear();
    for (auto& n : names) selection.push_back({SelType::Bone, data.findBone(n), ""});
}

void Document::reparentSlot(int si, int newBone) {
    if (si < 0 || newBone < 0 || data.slots[si].bone == newBone) return;
    checkpoint("Set slot bone");
    const Bone& ob = sk.bones[data.slots[si].bone];
    const Bone& nb = sk.bones[newBone];
    auto remap = [&](float& x, float& y) {
        Vec2 w = ob.localToWorld({x, y});
        Vec2 l = nb.worldToLocal(w);
        x = l.x; y = l.y;
    };
    float dRot = ob.worldRotationX() - nb.worldRotationX();
    for (auto& s : data.skins) {
        auto it = s.attachments.find(si);
        if (it == s.attachments.end()) continue;
        for (auto& [n, a] : it->second) {
            if (a.type == AttachmentType::Region || a.type == AttachmentType::Point) {
                remap(a.x, a.y);
                a.rotation += dRot;
            } else if (a.isVertex() && !a.isWeighted()) {
                for (size_t i = 0; i + 1 < a.vertices.size(); i += 2) remap(a.vertices[i], a.vertices[i + 1]);
            }
        }
    }
    data.slots[si].bone = newBone;
}

int Document::createAnimation(const std::string& name) {
    checkpoint("New animation");
    Animation a;
    a.name = data.uniqueAnimationName(name);
    data.animations.push_back(a);
    animation = (int)data.animations.size() - 1;
    return animation;
}

int Document::createSkin(const std::string& name) {
    checkpoint("New skin");
    data.defaultSkin();
    Skin s;
    s.name = name;
    for (int i = 2; data.findSkin(s.name) >= 0; i++) s.name = name + std::to_string(i);
    data.skins.push_back(s);
    return (int)data.skins.size() - 1;
}

int Document::createEvent(const std::string& name) {
    checkpoint("New event");
    EventData e;
    e.name = name;
    for (int i = 2; data.findEvent(e.name) >= 0; i++) e.name = name + std::to_string(i);
    data.events.push_back(e);
    return (int)data.events.size() - 1;
}

template <class T> static std::string uniqueIn(const std::vector<T>& v, const std::string& base) {
    auto has = [&](const std::string& n) { for (auto& x : v) if (x.name == n) return true; return false; };
    if (!has(base)) return base;
    for (int i = 2;; i++) if (!has(base + std::to_string(i))) return base + std::to_string(i);
}

static int nextOrder(const SkeletonData& d) {
    int o = -1;
    for (auto& c : d.ik) o = std::max(o, c.order);
    for (auto& c : d.transforms) o = std::max(o, c.order);
    for (auto& c : d.paths) o = std::max(o, c.order);
    for (auto& c : d.physics) o = std::max(o, c.order);
    return o + 1;
}

int Document::createIk(const std::string& name, const std::vector<int>& bonesIn, int target, const Vec2* newTargetAt) {
    // Spine: one bone, or a parent and its immediate child (parent first). The target may not be one of the
    // constrained bones or below them, or the constraint would chase its own tip.
    std::vector<int> bones = bonesIn;
    if (bones.empty() || bones.size() > 2 || (target < 0 && !newTargetAt)) return -1;
    if (bones.size() == 2) {
        if (data.bones[bones[1]].parent != bones[0]) {
            if (data.bones[bones[0]].parent != bones[1]) return -1;
            std::swap(bones[0], bones[1]);
        }
    }
    if (target >= 0) {
        for (int t = target; t >= 0; t = data.bones[t].parent)
            if (std::find(bones.begin(), bones.end(), t) != bones.end()) return -1;
    }
    std::vector<std::string> names;
    for (int b : bones) names.push_back(data.bones[b].name);
    std::string targetName;
    if (target < 0) { // a new target bone under the root, one undo step with the constraint
        Vec2 at = *newTargetAt;
        createBone(0, at, {at.x + 30, at.y}, names.back() + " target");
        for (size_t i = 0; i < bones.size(); i++) bones[i] = data.findBone(names[i]);
        target = selection.empty() ? -1 : selection[0].index;
        if (target < 0) return -1;
    } else {
        checkpoint("New IK constraint");
    }
    IkConstraintData c;
    c.name = uniqueIn(data.ik, name);
    c.order = nextOrder(data);
    c.bones = bones;
    c.target = target;
    // Keep the current bend direction so creating the constraint does not flip the limb.
    if (c.bones.size() == 2) {
        const Bone& p = sk.bones[c.bones[0]];
        const Bone& ch = sk.bones[c.bones[1]];
        Vec2 tip = ch.localToWorld({data.bones[c.bones[1]].length, 0});
        float cross = (ch.worldX - p.worldX) * (tip.y - p.worldY) - (ch.worldY - p.worldY) * (tip.x - p.worldX);
        c.bendDirection = cross >= 0 ? 1 : -1;
    }
    data.ik.push_back(c);
    selection = {{SelType::Ik, (int)data.ik.size() - 1, ""}};
    return (int)data.ik.size() - 1;
}

int Document::createTransform(const std::string& name, const std::vector<int>& bones, int target) {
    if (bones.empty() || target < 0) return -1;
    checkpoint("New transform constraint");
    TransformConstraintData c;
    c.name = uniqueIn(data.transforms, name);
    c.order = nextOrder(data);
    c.bones = bones;
    c.target = target;
    // Spine's default: offsets captured so bones keep their pose; mixes start at 100%.
    const Bone& t = sk.bones[target];
    if (!bones.empty()) {
        const Bone& b = sk.bones[bones[0]];
        c.offsetRotation = wrapDeg(b.worldRotationX() - t.worldRotationX());
        Vec2 l = t.worldToLocal({b.worldX, b.worldY});
        c.offsetX = l.x; c.offsetY = l.y;
        c.offsetScaleX = b.worldScaleX() - t.worldScaleX();
        c.offsetScaleY = b.worldScaleY() - t.worldScaleY();
    }
    data.transforms.push_back(c);
    selection = {{SelType::Transform, (int)data.transforms.size() - 1, ""}};
    return (int)data.transforms.size() - 1;
}

int Document::createPath(const std::string& name, const std::vector<int>& bones, int slot) {
    if (bones.empty() || slot < 0) return -1;
    checkpoint("New path constraint");
    PathConstraintData c;
    c.name = uniqueIn(data.paths, name);
    c.order = nextOrder(data);
    c.bones = bones;
    c.target = slot;
    data.paths.push_back(c);
    selection = {{SelType::Path, (int)data.paths.size() - 1, ""}};
    return (int)data.paths.size() - 1;
}

int Document::createPhysics(const std::string& name, int bone) {
    if (bone < 0) return -1;
    checkpoint("New physics constraint");
    PhysicsConstraintData c;
    c.name = uniqueIn(data.physics, name);
    c.order = nextOrder(data);
    c.bone = bone;
    c.rotate = 1;
    data.physics.push_back(c);
    selection = {{SelType::Physics, (int)data.physics.size() - 1, ""}};
    return (int)data.physics.size() - 1;
}

void Document::convertToMesh(int si, const std::string& name) {
    Attachment* a = nullptr;
    for (auto& s : data.skins) if ((a = s.get(si, name))) break;
    if (!a || a->type != AttachmentType::Region) return;
    checkpoint("Convert to mesh");
    float c[8];
    a->regionCorners(c);
    a->type = AttachmentType::Mesh;
    if (a->path.empty()) a->path = a->name;
    a->vertices.assign(c, c + 8);
    a->bones.clear();
    a->worldVerticesLength = 8;
    a->uvs = {0, 1, 0, 0, 1, 0, 1, 1};
    a->triangles = {0, 1, 2, 2, 3, 0};
    a->hullLength = 4;
    a->edges = {0, 1, 1, 2, 2, 3, 3, 0};
    a->width *= std::fabs(a->scaleX); a->height *= std::fabs(a->scaleY);
}

void Document::convertToRegion(int si, const std::string& name) {
    Attachment* a = nullptr;
    for (auto& s : data.skins) if ((a = s.get(si, name))) break;
    if (!a || a->type != AttachmentType::Mesh) return;
    checkpoint("Convert to region");
    for (auto& s : data.skins) if ((a = s.get(si, name))) break;
    if (a->isWeighted()) unbindWeights(*a, sk, data.slots[si].bone);
    // The image's corners in bone space (BL, TL, TR, BR), from where the UVs put it; the attachment's own placement if the fit fails.
    float c[8];
    double cx[3], cy[3];
    if (a->vertexCount() >= 3 && fitAffine(a->uvs, a->vertices, a->vertexCount(), cx, cy)) {
        static const float uv[8] = {0, 1, 0, 0, 1, 0, 1, 1};
        for (int k = 0; k < 4; k++) {
            c[k * 2] = (float)(cx[0] * uv[k * 2] + cx[1] * uv[k * 2 + 1] + cx[2]);
            c[k * 2 + 1] = (float)(cy[0] * uv[k * 2] + cy[1] * uv[k * 2 + 1] + cy[2]);
        }
    } else {
        Attachment t = *a;
        t.scaleX = t.scaleY = 1;
        t.regionCorners(c);
    }
    float mx = (c[0] + c[2] + c[4] + c[6]) / 4, my = (c[1] + c[3] + c[5] + c[7]) / 4;
    a->width = std::hypot(c[4] - c[2], c[5] - c[3]);
    a->height = std::hypot(c[2] - c[0], c[3] - c[1]);
    a->rotation = std::atan2(c[5] - c[3], c[4] - c[2]) * 57.2957795f;
    a->x = mx; a->y = my;
    a->scaleX = a->scaleY = 1;
    a->type = AttachmentType::Region;
    a->vertices.clear(); a->uvs.clear(); a->triangles.clear(); a->edges.clear(); a->bones.clear();
    a->hullLength = 0; a->worldVerticesLength = 0; a->wireframe = false;
    dropDeformKeys(si, name);
}

void Document::createLinkedMesh(int si, const std::string& meshName) {
    int srcSkin = -1;
    for (size_t k = 0; k < data.skins.size(); k++)
        if (const Attachment* a = data.skins[k].get(si, meshName); a && a->type == AttachmentType::Mesh) { srcSkin = (int)k; break; }
    if (srcSkin < 0) return;
    checkpoint("New linked mesh");
    const Attachment src = *data.skins[srcSkin].get(si, meshName);
    Attachment l;
    l.type = AttachmentType::LinkedMesh;
    l.name = meshName + " linked";
    for (int i = 2; data.findAttachment(skin, si, l.name); i++) l.name = meshName + " linked" + std::to_string(i);
    l.path = src.imagePath();
    l.parentMesh = meshName;
    l.parentSkin = srcSkin == 0 ? "" : data.skins[srcSkin].name;
    l.width = src.width; l.height = src.height;
    l.color = src.color;
    Skin& target = skin < (int)data.skins.size() ? data.skins[skin] : data.defaultSkin();
    target.set(si, l);
    selection = {{SelType::Attachment, si, l.name}};
}

void Document::addAttachment(int si, AttachmentType type, const std::string& base) {
    if (si < 0) return;
    checkpoint("New attachment");
    Attachment a;
    a.type = type;
    a.name = base;
    for (int i = 2; data.findAttachment(skin, si, a.name); i++) a.name = base + std::to_string(i);
    float r = 50;
    switch (type) {
    case AttachmentType::BoundingBox:
        a.color = Color(0.38f, 0.94f, 0, 1);
        a.vertices = {-r, -r, -r, r, r, r, r, -r};
        a.worldVerticesLength = 8;
        break;
    case AttachmentType::Clipping:
        a.color = Color(0.8f, 0.2f, 0.2f, 1);
        a.vertices = {-r, -r, -r, r, r, r, r, -r};
        a.worldVerticesLength = 8;
        break;
    case AttachmentType::Path:
        a.color = Color(1, 0.5f, 0, 1);
        // Two curves: [handleIn, point, handleOut] per point, open path.
        a.vertices = {-150, -40, -100, 0, -50, 40, 50, 40, 100, 0, 150, -40};
        a.worldVerticesLength = 12;
        a.lengths = {200};
        break;
    case AttachmentType::Point:
        a.color = Color(0.94f, 0.94f, 0, 1);
        break;
    default: break;
    }
    Skin& target = skin < (int)data.skins.size() ? data.skins[skin] : data.defaultSkin();
    target.set(si, a);
    data.slots[si].attachment = a.name;
    selection = {{SelType::Attachment, si, a.name}};
}

// ---- images & files ----

std::string Document::imageFileFor(const std::string& name) const {
    if (imagesDir.empty()) return {};
    for (const char* ext : {".png", ".jpg", ".jpeg", ".bmp"}) {
        std::string p = joinPath(imagesDir, name + ext);
        if (fileExists(p)) return p;
    }
    return {};
}

std::string Document::imagePathFor(const Attachment& a, int sequenceIndex) const {
    std::string p = a.imagePath();
    if (a.seqCount > 0) { // sequence frames: path + zero-padded (start + index)
        int index = sequenceIndex >= 0 ? sequenceIndex : a.seqSetup;
        char buf[32];
        std::snprintf(buf, sizeof buf, "%0*d", std::max(1, a.seqDigits), a.seqStart + std::max(0, index));
        p += buf;
    }
    return imageFileFor(p);
}

void Document::scanImages() {
    imageNames.clear();
    if (imagesDir.empty()) return;
    for (auto& f : listFiles(imagesDir, {".png", ".jpg", ".jpeg", ".bmp"})) imageNames.push_back(stripExtension(f));
}

void Document::newProject() {
    data = SkeletonData();
    BoneData root;
    root.name = "root";
    data.bones.push_back(root);
    data.defaultSkin();
    sk = Skeleton();
    projectPath.clear();
    imagesDir.clear();
    imageNames.clear();
    undoStack.clear(); redoStack.clear(); openEdit.reset();
    selection.clear(); clearPending();
    mode = Mode::Setup; animation = -1; time = 0; skin = 0; playing = false;
    dirty = false;
    updatePose();
}

bool Document::openFile(const std::string& path, std::string& error) {
    std::string text;
    if (!readTextFile(path, text)) { error = "Cannot read " + path; return false; }
    SkeletonData d;
    JsonResult r = readSkeletonJson(text, d);
    if (!r.ok) { error = r.error; return false; }
    newProject();
    data = std::move(d);
    warnings = r.warnings;
    projectPath = normalizePath(path);
    std::string dir = parentDir(projectPath);
    imagesDir = data.imagesPath.empty() ? joinPath(dir, "images") : joinPath(dir, data.imagesPath);
    if (!dirExists(imagesDir)) imagesDir = dir; // Photoshop exports often sit next to their images
    applyEditorState(readEditorBlock(text));
    if (extension(path) != ".osteon") projectPath.clear(); // imported data: Save asks for a project file
    scanImages();
    if (!data.animations.empty()) animation = 0;
    dirty = false;
    sk = Skeleton();
    updatePose();
    addRecent(path);
    return true;
}

std::string Document::editorState() const {
    nlohmann::json j;
    j["imagesDir"] = imagesDir;
    j["skin"] = skin;
    j["animation"] = animation;
    j["autoKey"] = autoKey;
    j["loop"] = loop;
    return j.dump();
}

void Document::applyEditorState(const std::string& text) {
    if (text.empty()) return;
    try {
        auto j = nlohmann::json::parse(text);
        if (j.contains("imagesDir") && j["imagesDir"].is_string() && dirExists(j["imagesDir"].get<std::string>())) imagesDir = j["imagesDir"];
        if (j.contains("skin")) skin = j["skin"];
        if (j.contains("animation")) animation = j["animation"];
        if (j.contains("autoKey")) autoKey = j["autoKey"];
        if (j.contains("loop")) loop = j["loop"];
    } catch (...) {}
}

bool Document::saveProject(const std::string& path, std::string& error) {
    commitEdit();
    std::string dir = parentDir(path);
    // Store the images folder relative to the project when possible.
    std::string rel = imagesDir;
    if (!imagesDir.empty() && imagesDir.rfind(dir + "/", 0) == 0) rel = "./" + imagesDir.substr(dir.size() + 1) + "/";
    SkeletonData copy = data;
    copy.imagesPath = rel;
    JsonWriteOptions opt;
    opt.editorBlock = editorState();
    if (!writeTextFile(path, writeSkeletonJson(copy, opt))) { error = "Cannot write " + path; return false; }
    data.imagesPath = rel;
    projectPath = normalizePath(path);
    dirty = false;
    addRecent(path);
    return true;
}

bool Document::exportJson(const std::string& path, bool pretty, bool nonessential, std::string& error, const std::string& version) {
    JsonWriteOptions opt;
    opt.version = version;
    opt.pretty = pretty;
    opt.nonessential = nonessential;
    SkeletonData copy = data;
    std::string dir = parentDir(path);
    if (!imagesDir.empty() && imagesDir.rfind(dir + "/", 0) == 0) copy.imagesPath = "./" + imagesDir.substr(dir.size() + 1) + "/";
    else copy.imagesPath = imagesDir.empty() ? "" : imagesDir + "/";
    if (!writeTextFile(path, writeSkeletonJson(copy, opt))) { error = "Cannot write " + path; return false; }
    return true;
}

int Document::importImagesAsSlots() {
    scanImages();
    std::vector<std::string> missing;
    for (auto& n : imageNames) {
        bool used = false;
        for (auto& s : data.skins)
            for (auto& [si, m] : s.attachments)
                for (auto& [an, a] : m) used |= a.imagePath() == n;
        if (!used) missing.push_back(n);
    }
    if (missing.empty()) return 0;
    checkpoint("Import images");
    updatePose();
    for (auto& n : missing) {
        int si = createSlot(0, fileName(n));
        addRegionAttachment(si, n, {0, 0});
    }
    return (int)missing.size();
}

static std::string safeFileStem(const std::string& in) {
    std::string s;
    for (unsigned char c : in) s += (c < 32 || std::strchr("<>:\"/\\|?*", c)) ? '_' : (char)c;
    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    size_t b = s.find_first_not_of(' ');
    s = b == std::string::npos ? "" : s.substr(b);
    return s.empty() ? "layer" : s;
}

int Document::importPsd(const std::string& psdPath, std::string& error, int* hidden) {
    PsdDocument psd;
    if (!readPsd(psdPath, psd, error)) return -1;
    std::string stem = safeFileStem(stripExtension(fileName(psdPath)));
    std::string outDir = imagesDir.empty() ? joinPath(parentDir(psdPath), stem + "_images") : joinPath(imagesDir, stem);
    if (!makeDirs(outDir)) { error = "Cannot create folder " + outDir; return -1; }
    std::string prefix = imagesDir.empty() ? "" : stem + "/"; // attachment path relative to the images folder
    int hid = 0;
    std::vector<const PsdLayer*> vis;
    for (auto& L : psd.layers) { if (L.visible) vis.push_back(&L); else hid++; }
    if (hidden) *hidden = hid;
    if (vis.empty()) { error = "The PSD has no visible pixel layers"; return -1; }

    // Write the PNGs first so a failure leaves the document untouched.
    std::vector<std::string> names;
    for (auto* L : vis) {
        std::string base = safeFileStem(L->name), n = base;
        for (int i = 2;; i++) {
            bool used = false;
            for (auto& e : names) used |= e == n;
            if (!used) break;
            n = base + "_" + std::to_string(i);
        }
        if (!savePngFile(joinPath(outDir, n + ".png"), L->width(), L->height(), L->rgba.data())) {
            error = "Cannot write " + joinPath(outDir, n + ".png");
            return -1;
        }
        names.push_back(n);
    }

    checkpoint("Import PSD");
    if (imagesDir.empty()) imagesDir = outDir;
    scanImages();
    for (size_t i = 0; i < vis.size(); i++) {
        const PsdLayer& L = *vis[i];
        int si = createSlot(0, L.name.empty() ? names[i] : L.name);
        Attachment a;
        a.type = AttachmentType::Region;
        a.name = a.path = prefix + names[i];
        a.width = (float)L.width();
        a.height = (float)L.height();
        a.x = (L.left + L.right) / 2.0f - psd.width / 2.0f;
        a.y = psd.height / 2.0f - (L.top + L.bottom) / 2.0f;
        a.color.a = L.opacity;
        data.defaultSkin().set(si, a);
        data.slots[si].attachment = a.name;
    }
    updatePose();
    return (int)vis.size();
}

void Document::addRecent(const std::string& path) {
    std::string p = normalizePath(path);
    recentFiles.erase(std::remove(recentFiles.begin(), recentFiles.end(), p), recentFiles.end());
    recentFiles.insert(recentFiles.begin(), p);
    if (recentFiles.size() > 10) recentFiles.resize(10);
    saveRecent();
}

void Document::loadRecent() {
    std::string text;
    recentFiles.clear();
    if (!readTextFile(joinPath(appDataDir(), "recent.txt"), text)) return;
    size_t s = 0;
    while (s < text.size()) {
        size_t e = text.find('\n', s);
        std::string line = text.substr(s, e == std::string::npos ? std::string::npos : e - s);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) recentFiles.push_back(line);
        if (e == std::string::npos) break;
        s = e + 1;
    }
}

void Document::saveRecent() {
    std::string text;
    for (auto& r : recentFiles) text += r + "\n";
    writeTextFile(joinPath(appDataDir(), "recent.txt"), text);
}

} // namespace osteon
