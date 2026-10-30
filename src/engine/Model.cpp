#include "Model.h"
#include <functional>
#include <set>

namespace osteon {

const char* inheritName(Inherit i) {
    switch (i) {
    case Inherit::OnlyTranslation: return "onlyTranslation";
    case Inherit::NoRotationOrReflection: return "noRotationOrReflection";
    case Inherit::NoScale: return "noScale";
    case Inherit::NoScaleOrReflection: return "noScaleOrReflection";
    default: return "normal";
    }
}
Inherit inheritFromName(const std::string& s) {
    if (s == "onlyTranslation") return Inherit::OnlyTranslation;
    if (s == "noRotationOrReflection") return Inherit::NoRotationOrReflection;
    if (s == "noScale") return Inherit::NoScale;
    if (s == "noScaleOrReflection") return Inherit::NoScaleOrReflection;
    return Inherit::Normal;
}
const char* blendName(BlendMode b) {
    switch (b) {
    case BlendMode::Additive: return "additive";
    case BlendMode::Multiply: return "multiply";
    case BlendMode::Screen: return "screen";
    default: return "normal";
    }
}
BlendMode blendFromName(const std::string& s) {
    if (s == "additive") return BlendMode::Additive;
    if (s == "multiply") return BlendMode::Multiply;
    if (s == "screen") return BlendMode::Screen;
    return BlendMode::Normal;
}
const char* attachmentTypeName(AttachmentType t) {
    switch (t) {
    case AttachmentType::Mesh: return "mesh";
    case AttachmentType::LinkedMesh: return "linkedmesh";
    case AttachmentType::BoundingBox: return "boundingbox";
    case AttachmentType::Path: return "path";
    case AttachmentType::Point: return "point";
    case AttachmentType::Clipping: return "clipping";
    default: return "region";
    }
}

void Attachment::regionCorners(float out[8]) const {
    float hw = width / 2 * scaleX, hh = height / 2 * scaleY;
    float c = cosDeg(rotation), s = sinDeg(rotation);
    const float pts[8] = {-hw, -hh, -hw, hh, hw, hh, hw, -hh};
    for (int i = 0; i < 8; i += 2) {
        out[i] = pts[i] * c - pts[i + 1] * s + x;
        out[i + 1] = pts[i] * s + pts[i + 1] * c + y;
    }
}

Attachment* Skin::get(int slot, const std::string& name) {
    auto it = attachments.find(slot);
    if (it == attachments.end()) return nullptr;
    auto a = it->second.find(name);
    return a == it->second.end() ? nullptr : &a->second;
}
const Attachment* Skin::get(int slot, const std::string& name) const {
    return const_cast<Skin*>(this)->get(slot, name);
}
void Skin::remove(int slot, const std::string& name) {
    auto it = attachments.find(slot);
    if (it != attachments.end()) it->second.erase(name);
}

// ---- timelines ----

int timelineValueCount(TimelineType t) {
    switch (t) {
    case TimelineType::Translate: case TimelineType::Scale: case TimelineType::Shear: return 2;
    case TimelineType::RGBA: return 4;
    case TimelineType::RGB: return 3;
    case TimelineType::RGBA2: return 7;
    case TimelineType::RGB2: return 6;
    case TimelineType::Ik: return 2;          // mix, softness
    case TimelineType::Transform: return 6;
    case TimelineType::PathMix: return 3;
    case TimelineType::Attachment: case TimelineType::DrawOrder: case TimelineType::Event:
    case TimelineType::Inherit: case TimelineType::Sequence: case TimelineType::PhysicsReset: return 0;
    default: return 1;
    }
}

const char* timelineName(TimelineType t) {
    switch (t) {
    case TimelineType::Rotate: return "rotate";
    case TimelineType::Translate: return "translate";
    case TimelineType::TranslateX: return "translatex";
    case TimelineType::TranslateY: return "translatey";
    case TimelineType::Scale: return "scale";
    case TimelineType::ScaleX: return "scalex";
    case TimelineType::ScaleY: return "scaley";
    case TimelineType::Shear: return "shear";
    case TimelineType::ShearX: return "shearx";
    case TimelineType::ShearY: return "sheary";
    case TimelineType::Inherit: return "inherit";
    case TimelineType::Attachment: return "attachment";
    case TimelineType::RGBA: return "rgba";
    case TimelineType::RGB: return "rgb";
    case TimelineType::Alpha: return "alpha";
    case TimelineType::RGBA2: return "rgba2";
    case TimelineType::RGB2: return "rgb2";
    case TimelineType::Deform: return "deform";
    case TimelineType::Sequence: return "sequence";
    case TimelineType::DrawOrder: return "drawOrder";
    case TimelineType::Event: return "events";
    case TimelineType::Ik: return "ik";
    case TimelineType::Transform: return "transform";
    case TimelineType::Path: return "position";
    case TimelineType::PathMix: return "mix";
    case TimelineType::PhysicsInertia: return "inertia";
    case TimelineType::PhysicsStrength: return "strength";
    case TimelineType::PhysicsDamping: return "damping";
    case TimelineType::PhysicsMass: return "mass";
    case TimelineType::PhysicsWind: return "wind";
    case TimelineType::PhysicsGravity: return "gravity";
    case TimelineType::PhysicsMix: return "physics mix";
    case TimelineType::PhysicsReset: return "reset";
    }
    return "?";
}

bool isBoneTimeline(TimelineType t) { return t <= TimelineType::Inherit; }
bool isSlotTimeline(TimelineType t) { return t >= TimelineType::Attachment && t <= TimelineType::RGB2; }
bool isPhysicsTimeline(TimelineType t) { return t >= TimelineType::PhysicsInertia && t <= TimelineType::PhysicsReset; }

int Timeline::findKey(float time, float eps) const {
    for (size_t i = 0; i < keys.size(); i++)
        if (std::fabs(keys[i].time - time) <= eps) return (int)i;
    return -1;
}

Key& Timeline::setKey(float time) {
    int i = findKey(time);
    if (i >= 0) return keys[i];
    auto it = std::lower_bound(keys.begin(), keys.end(), time, [](const Key& k, float t) { return k.time < t; });
    Key k;
    k.time = time;
    return *keys.insert(it, k);
}

float Animation::duration() const {
    float d = 0;
    for (auto& t : timelines)
        for (auto& k : t.keys) d = std::max(d, k.time);
    return d;
}

Timeline* Animation::find(TimelineType type, int target, const std::string& skin, const std::string& att) {
    for (auto& t : timelines)
        if (t.type == type && t.target == target && t.skin == skin && t.attachment == att) return &t;
    return nullptr;
}

Timeline& Animation::get(TimelineType type, int target, const std::string& skin, const std::string& att) {
    if (auto* t = find(type, target, skin, att)) return *t;
    Timeline t;
    t.type = type; t.target = target; t.skin = skin; t.attachment = att;
    timelines.push_back(t);
    return timelines.back();
}

// ---- skeleton data ----

template <class T> static int findByName(const std::vector<T>& v, const std::string& n) {
    for (size_t i = 0; i < v.size(); i++)
        if (v[i].name == n) return (int)i;
    return -1;
}
int SkeletonData::findBone(const std::string& n) const { return findByName(bones, n); }
int SkeletonData::findSlot(const std::string& n) const { return findByName(slots, n); }
int SkeletonData::findSkin(const std::string& n) const { return findByName(skins, n); }
int SkeletonData::findAnimation(const std::string& n) const { return findByName(animations, n); }
int SkeletonData::findEvent(const std::string& n) const { return findByName(events, n); }

Skin& SkeletonData::defaultSkin() {
    if (skins.empty() || skins[0].name != "default") {
        Skin s;
        s.name = "default";
        skins.insert(skins.begin(), s);
    }
    return skins[0];
}

bool SkeletonData::isDescendant(int bone, int ancestor) const {
    for (int b = bone; b >= 0; b = bones[b].parent)
        if (b == ancestor) return true;
    return false;
}

std::vector<int> SkeletonData::childrenOf(int bone) const {
    std::vector<int> r;
    for (size_t i = 0; i < bones.size(); i++)
        if (bones[i].parent == bone) r.push_back((int)i);
    return r;
}

template <class T> static std::string uniqueName(const std::vector<T>& v, const std::string& base) {
    if (findByName(v, base) < 0) return base;
    for (int i = 2;; i++) {
        std::string n = base + std::to_string(i);
        if (findByName(v, n) < 0) return n;
    }
}
std::string SkeletonData::uniqueBoneName(const std::string& b) const { return uniqueName(bones, b); }
std::string SkeletonData::uniqueSlotName(const std::string& b) const { return uniqueName(slots, b); }
std::string SkeletonData::uniqueAnimationName(const std::string& b) const { return uniqueName(animations, b); }

const Attachment* SkeletonData::findAttachment(int skinIndex, int slot, const std::string& name) const {
    if (name.empty()) return nullptr;
    if (skinIndex >= 0 && skinIndex < (int)skins.size())
        if (auto* a = skins[skinIndex].get(slot, name)) return a;
    if (!skins.empty()) return skins[0].get(slot, name);
    return nullptr;
}

int SkeletonData::addBone(const BoneData& b) {
    bones.push_back(b);
    return (int)bones.size() - 1;
}

// Applies an old->new bone index map (-1 = removed) to every bone reference.
static void remapBones(SkeletonData& d, const std::vector<int>& map) {
    auto m = [&](int i) { return i >= 0 && i < (int)map.size() ? map[i] : -1; };
    for (auto& b : d.bones) b.parent = m(b.parent);
    for (auto& s : d.slots) s.bone = m(s.bone);
    auto remapList = [&](std::vector<int>& v) {
        std::vector<int> out;
        for (int i : v) if (m(i) >= 0) out.push_back(m(i));
        v = out;
    };
    for (auto& c : d.ik) { remapList(c.bones); c.target = m(c.target); }
    for (auto& c : d.transforms) { remapList(c.bones); c.target = m(c.target); }
    for (auto& c : d.paths) remapList(c.bones);
    for (auto& c : d.physics) c.bone = m(c.bone);
    for (auto& sk : d.skins) {
        remapList(sk.bones);
        for (auto& [slot, atts] : sk.attachments)
            for (auto& [n, a] : atts)
                if (a.isWeighted())
                    for (size_t i = 0; i < a.bones.size();) {
                        int cnt = a.bones[i++];
                        for (int k = 0; k < cnt; k++, i++) a.bones[i] = std::max(0, m(a.bones[i]));
                    }
    }
    for (auto& an : d.animations) {
        std::vector<Timeline> keep;
        for (auto& t : an.timelines) {
            if (isBoneTimeline(t.type)) {
                t.target = m(t.target);
                if (t.target < 0) continue;
            }
            keep.push_back(std::move(t));
        }
        an.timelines = std::move(keep);
    }
    // Drop constraints that lost their bones or target.
    d.ik.erase(std::remove_if(d.ik.begin(), d.ik.end(), [](auto& c) { return c.bones.empty() || c.target < 0; }), d.ik.end());
    d.transforms.erase(std::remove_if(d.transforms.begin(), d.transforms.end(), [](auto& c) { return c.bones.empty() || c.target < 0; }), d.transforms.end());
    d.paths.erase(std::remove_if(d.paths.begin(), d.paths.end(), [](auto& c) { return c.bones.empty(); }), d.paths.end());
    d.physics.erase(std::remove_if(d.physics.begin(), d.physics.end(), [](auto& c) { return c.bone < 0; }), d.physics.end());
}

static void remapSlots(SkeletonData& d, const std::vector<int>& map) {
    auto m = [&](int i) { return i >= 0 && i < (int)map.size() ? map[i] : -1; };
    for (auto& sk : d.skins) {
        std::map<int, SlotAttachments> out;
        for (auto& [slot, atts] : sk.attachments)
            if (m(slot) >= 0) out[m(slot)] = std::move(atts);
        sk.attachments = std::move(out);
    }
    for (auto& c : d.paths) c.target = m(c.target);
    d.paths.erase(std::remove_if(d.paths.begin(), d.paths.end(), [](auto& c) { return c.target < 0; }), d.paths.end());
    for (auto& an : d.animations) {
        std::vector<Timeline> keep;
        for (auto& t : an.timelines) {
            if (isSlotTimeline(t.type) || t.type == TimelineType::Deform || t.type == TimelineType::Sequence) {
                t.target = m(t.target);
                if (t.target < 0) continue;
            }
            if (t.type == TimelineType::DrawOrder)
                for (auto& k : t.keys) {
                    if (k.drawOrder.empty()) continue;
                    std::vector<int> o;
                    for (int s : k.drawOrder) if (m(s) >= 0) o.push_back(m(s));
                    k.drawOrder = o;
                }
            keep.push_back(std::move(t));
        }
        an.timelines = std::move(keep);
    }
}

void SkeletonData::removeBone(int index) {
    if (index <= 0 || index >= (int)bones.size()) return; // root cannot be removed
    std::vector<bool> dead(bones.size(), false);
    for (size_t i = 0; i < bones.size(); i++) dead[i] = isDescendant((int)i, index);
    // Remove slots on removed bones first.
    for (int s = (int)slots.size() - 1; s >= 0; s--)
        if (dead[slots[s].bone]) removeSlot(s);
    std::vector<int> map(bones.size(), -1);
    std::vector<BoneData> kept;
    for (size_t i = 0; i < bones.size(); i++)
        if (!dead[i]) { map[i] = (int)kept.size(); kept.push_back(bones[i]); }
    bones = kept;
    // Weighted vertices bound to removed bones would now point at root; that is the
    // best-effort fallback (Spine asks the user to reweight).
    remapBones(*this, map);
}

void SkeletonData::setParent(int bone, int newParent) {
    if (bone <= 0 || newParent < 0 || isDescendant(newParent, bone)) return;
    bones[bone].parent = newParent;
    sortBones();
}

bool SkeletonData::moveBone(int bone, int target, bool after) {
    int n = (int)bones.size();
    if (bone <= 0 || bone >= n || target <= 0 || target >= n || bone == target) return false;
    if (bones[bone].parent != bones[target].parent) return false;
    // Children of every bone, in their current (index) order, with `bone` repositioned beside `target`.
    std::vector<std::vector<int>> kids(n);
    int root = -1;
    for (int i = 0; i < n; i++) {
        if (bones[i].parent < 0) root = i;
        else if (i != bone) kids[bones[i].parent].push_back(i);
    }
    auto& sib = kids[bones[target].parent];
    auto at = std::find(sib.begin(), sib.end(), target);
    sib.insert(after ? at + 1 : at, bone);
    // Depth-first from the root gives a parent-before-child order with the new sibling order.
    std::vector<int> order;
    std::function<void(int)> visit = [&](int i) { order.push_back(i); for (int k : kids[i]) visit(k); };
    if (root < 0) return false;
    visit(root);
    if ((int)order.size() != n) return false;
    std::vector<int> map(n);
    std::vector<BoneData> sorted;
    for (int i : order) { map[i] = (int)sorted.size(); sorted.push_back(bones[i]); }
    bones = sorted;
    remapBones(*this, map);
    return true;
}

void SkeletonData::sortBones() {
    std::vector<int> order;
    std::vector<bool> done(bones.size(), false);
    std::function<void(int)> visit = [&](int i) {
        if (done[i]) return;
        if (bones[i].parent >= 0) visit(bones[i].parent);
        done[i] = true;
        order.push_back(i);
    };
    for (size_t i = 0; i < bones.size(); i++) visit((int)i);
    bool same = true;
    for (size_t i = 0; i < order.size(); i++) same &= order[i] == (int)i;
    if (same) return;
    std::vector<int> map(bones.size());
    std::vector<BoneData> sorted;
    for (int i : order) { map[i] = (int)sorted.size(); sorted.push_back(bones[i]); }
    bones = sorted;
    remapBones(*this, map);
}

int SkeletonData::addSlot(const SlotData& s) {
    slots.push_back(s);
    return (int)slots.size() - 1;
}

void SkeletonData::removeSlot(int index) {
    if (index < 0 || index >= (int)slots.size()) return;
    std::vector<int> map(slots.size());
    for (size_t i = 0; i < slots.size(); i++) map[i] = (int)i < index ? (int)i : ((int)i == index ? -1 : (int)i - 1);
    slots.erase(slots.begin() + index);
    remapSlots(*this, map);
}

void SkeletonData::moveSlot(int from, int to) {
    int n = (int)slots.size();
    if (from < 0 || from >= n) return;
    to = std::max(0, std::min(n - 1, to));
    if (from == to) return;
    std::vector<int> order(n);
    for (int i = 0; i < n; i++) order[i] = i;
    order.erase(order.begin() + from);
    order.insert(order.begin() + to, from);
    std::vector<int> map(n);
    std::vector<SlotData> out;
    for (int i = 0; i < n; i++) { map[order[i]] = i; out.push_back(slots[order[i]]); }
    slots = out;
    remapSlots(*this, map);
}

} // namespace osteon
