// Binary skeleton writer and reader. See SkeletonBinary.h for the format notes and the honest caveats.
#include "SkeletonBinary.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>

namespace osteon {
namespace {

// ---- primitives ------------------------------------------------------------------------------

uint32_t byteOf(float v) { return (uint32_t)std::lround(std::min(std::max(v, 0.f), 1.f) * 255); }
uint32_t rgba8888(float r, float g, float b, float a) { return (byteOf(r) << 24) | (byteOf(g) << 16) | (byteOf(b) << 8) | byteOf(a); }
uint32_t rgb888(float r, float g, float b) { return (byteOf(r) << 16) | (byteOf(g) << 8) | byteOf(b); }

struct Out {
    std::vector<uint8_t> b;
    void u8(int v) { b.push_back((uint8_t)v); }
    void boolean(bool v) { u8(v ? 1 : 0); }
    void i32(uint32_t v) { for (int s = 24; s >= 0; s -= 8) u8((v >> s) & 0xff); }
    void i16(int v) { u8((v >> 8) & 0xff); u8(v & 0xff); }
    void f32(float f) { uint32_t v; std::memcpy(&v, &f, 4); i32(v); }
    void var(uint32_t v) { while (v >= 0x80) { u8((v & 0x7f) | 0x80); v >>= 7; } u8((int)v); }   // unsigned varint, 7 bits per byte, low group first
    void svar(int32_t v) { var(((uint32_t)v << 1) ^ (uint32_t)(v >> 31)); }                     // zigzag
    void str(const std::string& s) { var((uint32_t)s.size() + 1); b.insert(b.end(), s.begin(), s.end()); } // 0 = null, 1 = empty, else length + 1
    void strN(const std::string& s) { if (s.empty()) var(0); else str(s); }                    // empty becomes null
};

struct StringTable {
    std::vector<std::string> list;
    std::map<std::string, uint32_t> index;
    uint32_t ref(const std::string& s) { // 0 = null, else table index + 1
        if (s.empty()) return 0;
        auto it = index.find(s);
        if (it != index.end()) return it->second;
        list.push_back(s);
        return index[s] = (uint32_t)list.size();
    }
};

struct In {
    const uint8_t* p; size_t n, i = 0;
    In(const uint8_t* d, size_t sz) : p(d), n(sz) {}
    uint8_t u8() { if (i >= n) throw std::runtime_error("Unexpected end of data"); return p[i++]; }
    bool boolean() { return u8() != 0; }
    uint32_t i32() { uint32_t v = 0; for (int k = 0; k < 4; k++) v = (v << 8) | u8(); return v; }
    int i16() { int v = (u8() << 8) | u8(); return (int16_t)v; }
    float f32() { uint32_t v = i32(); float f; std::memcpy(&f, &v, 4); return f; }
    uint32_t var() {
        uint32_t v = 0;
        for (int s = 0; s < 35; s += 7) { uint8_t b = u8(); v |= (uint32_t)(b & 0x7f) << s; if (!(b & 0x80)) return v; }
        throw std::runtime_error("Bad variable-length integer");
    }
    int32_t svar() { uint32_t v = var(); return (int32_t)((v >> 1) ^ (0u - (v & 1))); }
    // A count of items that each take at least one byte: refuses counts the remaining data cannot hold.
    uint32_t cnt(size_t bytesPerItem = 1) { uint32_t c = var(); if ((size_t)c * bytesPerItem > n - i) throw std::runtime_error("Bad count"); return c; }
    std::string str() {
        uint32_t l = var();
        if (l <= 1) return {};
        if (l - 1 > n - i) throw std::runtime_error("Bad string");
        std::string s((const char*)p + i, l - 1);
        i += l - 1;
        return s;
    }
};

const char* kSeqModes[] = {"hold", "once", "loop", "pingpong", "onceReverse", "loopReverse", "pingpongReverse"};
int seqModeCode(const std::string& s) { for (int k = 0; k < 7; k++) if (s == kSeqModes[k]) return k; return 0; }

enum { kAttachRegion = 0, kAttachBox, kAttachMesh, kAttachLinked, kAttachPath, kAttachPoint, kAttachClip };
int typeCode(AttachmentType t) {
    switch (t) {
    case AttachmentType::Region: return kAttachRegion; case AttachmentType::BoundingBox: return kAttachBox;
    case AttachmentType::Mesh: return kAttachMesh; case AttachmentType::LinkedMesh: return kAttachLinked;
    case AttachmentType::Path: return kAttachPath; case AttachmentType::Point: return kAttachPoint;
    case AttachmentType::Clipping: return kAttachClip;
    }
    return 0;
}

// Slot timeline codes: attachment, rgba, rgb, rgba2, rgb2, alpha.
int slotCode(TimelineType t) {
    switch (t) {
    case TimelineType::Attachment: return 0; case TimelineType::RGBA: return 1; case TimelineType::RGB: return 2;
    case TimelineType::RGBA2: return 3; case TimelineType::RGB2: return 4; case TimelineType::Alpha: return 5;
    default: return -1;
    }
}
// Bone timeline codes equal the TimelineType order: rotate, translate(x,y), scale(x,y), shear(x,y), inherit.
const int kBoneTimelineCount = 11;

std::vector<int> influenceOrder(const Attachment& a) { // same ordering the JSON writer uses: influences sorted by bone within a vertex
    std::vector<int> order;
    int infl = 0;
    for (size_t i = 0; i < a.bones.size();) {
        int count = a.bones[i++];
        std::vector<std::pair<int, int>> v;
        for (int k = 0; k < count; k++) { v.push_back({a.bones[i++], infl++}); }
        std::stable_sort(v.begin(), v.end());
        for (auto& pr : v) order.push_back(pr.second);
    }
    return order;
}

// ---- writer ----------------------------------------------------------------------------------

struct Writer {
    const SkeletonData& d;
    const BinaryWriteOptions& opt;
    std::vector<std::string>* warnings;
    Out o;
    StringTable st;

    void warn(const std::string& s) { if (warnings) warnings->push_back(s); }

    void vertices(const Attachment& a, int vcount) {
        o.boolean(a.isWeighted());
        if (!a.isWeighted()) { for (int k = 0; k < vcount * 2; k++) o.f32(k < (int)a.vertices.size() ? a.vertices[k] : 0.f); return; }
        std::vector<int> boneOf;
        for (size_t i = 0; i < a.bones.size();) { int count = a.bones[i++]; for (int k = 0; k < count; k++) boneOf.push_back(a.bones[i++]); }
        std::vector<int> order = influenceOrder(a);
        size_t w = 0;
        for (size_t i = 0; i < a.bones.size();) {
            int count = a.bones[i];
            i += count + 1;
            o.var(count);
            for (int k = 0; k < count; k++, w++) {
                int src = order[w];
                o.var(boneOf[src]);
                o.f32(a.vertices[src * 3]); o.f32(a.vertices[src * 3 + 1]); o.f32(a.vertices[src * 3 + 2]);
            }
        }
    }

    void sequence(const Attachment& a) { o.var(a.seqCount); o.var(a.seqStart); o.var(a.seqDigits); o.var(a.seqSetup); }

    void attachment(const Attachment& a) {
        bool hasPath = !a.path.empty() && a.path != a.name;
        bool hasColor = a.color != Color();
        int flags = typeCode(a.type);
        switch (a.type) {
        case AttachmentType::Region: case AttachmentType::Mesh: case AttachmentType::LinkedMesh:
            if (hasPath) flags |= 16;
            if (hasColor) flags |= 32;
            if (a.seqCount > 0) flags |= 64;
            if (a.type == AttachmentType::Region && a.rotation != 0) flags |= 128;
            if (a.type == AttachmentType::LinkedMesh && a.inheritTimelines) flags |= 128;
            break;
        case AttachmentType::Path:
            if (a.closed) flags |= 16;
            if (a.constantSpeed) flags |= 32;
            break;
        default: break;
        }
        o.u8(flags);
        auto pathColorSeq = [&] {
            if (hasPath) o.var(st.ref(a.path));
            if (hasColor) o.i32(rgba8888(a.color.r, a.color.g, a.color.b, a.color.a));
            if (a.seqCount > 0) sequence(a);
        };
        switch (a.type) {
        case AttachmentType::Region:
            pathColorSeq();
            if (a.rotation != 0) o.f32(a.rotation);
            o.f32(a.x); o.f32(a.y); o.f32(a.scaleX); o.f32(a.scaleY); o.f32(a.width); o.f32(a.height);
            break;
        case AttachmentType::Mesh: {
            pathColorSeq();
            int vc = (int)a.uvs.size() / 2;
            o.var(vc);
            for (float f : a.uvs) o.f32(f);
            o.var((uint32_t)a.triangles.size());
            for (int t : a.triangles) o.i16(t);
            vertices(a, vc);
            o.var(a.hullLength);
            if (opt.nonessential) {
                o.var((uint32_t)a.edges.size());
                for (int e : a.edges) o.i16(e * 2);
                o.f32(a.width); o.f32(a.height);
            }
            break;
        }
        case AttachmentType::LinkedMesh:
            pathColorSeq();
            o.var(st.ref(a.parentSkin)); o.var(st.ref(a.parentMesh));
            if (opt.nonessential) { o.f32(a.width); o.f32(a.height); }
            break;
        case AttachmentType::BoundingBox:
            o.var(a.vertexCount()); vertices(a, a.vertexCount());
            if (opt.nonessential) o.i32(rgba8888(a.color.r, a.color.g, a.color.b, a.color.a));
            break;
        case AttachmentType::Path: {
            o.var(a.vertexCount()); vertices(a, a.vertexCount());
            for (int k = 0; k < a.vertexCount() / 3; k++) o.f32(k < (int)a.lengths.size() ? a.lengths[k] : 0.f);
            if (opt.nonessential) o.i32(rgba8888(a.color.r, a.color.g, a.color.b, a.color.a));
            break;
        }
        case AttachmentType::Point:
            o.f32(a.rotation); o.f32(a.x); o.f32(a.y);
            if (opt.nonessential) o.i32(rgba8888(a.color.r, a.color.g, a.color.b, a.color.a));
            break;
        case AttachmentType::Clipping: {
            int end = d.findSlot(a.endSlot);
            if (end < 0) { warn("Clipping attachment '" + a.name + "' has no end slot"); end = 0; }
            o.var(end);
            o.var(a.vertexCount()); vertices(a, a.vertexCount());
            if (opt.nonessential) o.i32(rgba8888(a.color.r, a.color.g, a.color.b, a.color.a));
            break;
        }
        }
    }

    void skinSlots(const Skin& s) {
        std::vector<std::pair<int, std::vector<const Attachment*>>> slots;
        for (auto& [slot, m] : s.attachments) {
            std::vector<const Attachment*> list;
            for (auto& [name, a] : m) if (a.exportable) list.push_back(&a);
            if (!list.empty()) slots.push_back({slot, list});
        }
        o.var((uint32_t)slots.size());
        for (auto& [slot, list] : slots) {
            o.var(slot);
            o.var((uint32_t)list.size());
            for (auto* a : list) { o.var(st.ref(a->name)); attachment(*a); }
        }
    }

    void writeCurve(const Key& k, int channels) {
        if (k.curve == CurveType::Bezier && k.bezier.size() >= (size_t)channels * 4) {
            o.u8(2);
            for (int i = 0; i < channels * 4; i++) o.f32(k.bezier[i]);
        } else o.u8(k.curve == CurveType::Stepped ? 1 : 0);
    }
    int bezierCount(const Timeline& tl, int channels) {
        int n = 0;
        for (size_t i = 0; i + 1 < tl.keys.size(); i++) if (tl.keys[i].curve == CurveType::Bezier && tl.keys[i].bezier.size() >= (size_t)channels * 4) n += channels;
        return n;
    }
    // bezier count, then per key: time, values, and (from the second key on) the curve of the previous key.
    template <class F> void curveKeys(const Timeline& tl, int channels, F vals) {
        o.var(bezierCount(tl, channels));
        for (size_t i = 0; i < tl.keys.size(); i++) {
            o.f32(tl.keys[i].time);
            vals(tl.keys[i]);
            if (i > 0) writeCurve(tl.keys[i - 1], channels);
        }
    }

    void slotTimeline(const Timeline& tl) {
        o.u8(slotCode(tl.type));
        o.var((uint32_t)tl.keys.size());
        switch (tl.type) {
        case TimelineType::Attachment: for (auto& k : tl.keys) { o.f32(k.time); o.var(st.ref(k.str)); } break;
        case TimelineType::RGBA: curveKeys(tl, 4, [&](const Key& k) { o.i32(rgba8888(k.v[0], k.v[1], k.v[2], k.v[3])); }); break;
        case TimelineType::RGB: curveKeys(tl, 3, [&](const Key& k) { o.i32(rgba8888(k.v[0], k.v[1], k.v[2], 0)); }); break;
        case TimelineType::RGBA2: curveKeys(tl, 7, [&](const Key& k) { o.i32(rgba8888(k.v[0], k.v[1], k.v[2], k.v[3])); o.i32(rgb888(k.v[4], k.v[5], k.v[6])); }); break;
        case TimelineType::RGB2: curveKeys(tl, 6, [&](const Key& k) { o.i32(rgba8888(k.v[0], k.v[1], k.v[2], 0)); o.i32(rgb888(k.v[3], k.v[4], k.v[5])); }); break;
        default: curveKeys(tl, 1, [&](const Key& k) { o.u8((int)byteOf(k.v[0])); }); break; // alpha
        }
    }

    // ---- animation ----

    void deformKeys(const Timeline& tl, const Attachment* att) {
        std::vector<int> order;
        if (att && att->isWeighted()) order = influenceOrder(*att);
        o.var((uint32_t)tl.keys.size());
        int bez = 0;
        for (size_t i = 0; i + 1 < tl.keys.size(); i++) if (tl.keys[i].curve == CurveType::Bezier && tl.keys[i].bezier.size() >= 4) bez++;
        o.var(bez);
        for (size_t i = 0; i < tl.keys.size(); i++) {
            Key k = tl.keys[i];
            if (!order.empty() && k.deform.size() == order.size() * 2) {
                std::vector<float> src = k.deform;
                for (size_t q = 0; q < order.size(); q++) { k.deform[q * 2] = src[order[q] * 2]; k.deform[q * 2 + 1] = src[order[q] * 2 + 1]; }
            }
            o.f32(k.time);
            if (i > 0) writeCurve(tl.keys[i - 1], 1);
            int start = 0, end = (int)k.deform.size();
            while (start < end && k.deform[start] == 0) start++;
            while (end > start && k.deform[end - 1] == 0) end--;
            if (end == start) { o.var(0); continue; }
            o.var(end - start); o.var(start);
            for (int v = start; v < end; v++) o.f32(k.deform[v]);
        }
    }

    void drawOrderKeys(const std::vector<const Key*>& keys) {
        o.var((uint32_t)keys.size());
        int n = (int)d.slots.size();
        for (auto* kp : keys) {
            const Key& k = *kp;
            o.f32(k.time);
            if (k.drawOrder.empty() || (int)k.drawOrder.size() != n) { o.var(0); continue; }
            std::vector<int> pos(n);
            for (int i = 0; i < n; i++) pos[k.drawOrder[i]] = i;
            std::vector<std::pair<int, int>> offs;
            for (int s = 0; s < n; s++) if (pos[s] != s) offs.push_back({s, pos[s] - s});
            // Check the sparse form rebuilds the order, else list every slot.
            std::vector<int> order(n, -1), unchanged;
            int orig = 0;
            for (auto& [slot, off] : offs) {
                while (orig != slot) unchanged.push_back(orig++);
                order[orig + off] = orig;
                orig++;
            }
            while (orig < n) unchanged.push_back(orig++);
            for (int i = n - 1; i >= 0; i--) if (order[i] == -1) { order[i] = unchanged.back(); unchanged.pop_back(); }
            if (order != k.drawOrder) { offs.clear(); for (int s = 0; s < n; s++) offs.push_back({s, pos[s] - s}); }
            o.var((uint32_t)offs.size());
            for (auto& [slot, off] : offs) { o.var(slot); o.var((uint32_t)off); }
        }
    }

    void eventKeys(const std::vector<const Key*>& keys) {
        o.var((uint32_t)keys.size());
        for (auto* kp : keys) {
            const Key& k = *kp;
            int ei = d.findEvent(k.str);
            const EventData dummy;
            const EventData& e = ei >= 0 ? d.events[ei] : dummy;
            o.f32(k.time);
            o.var(ei < 0 ? 0 : ei);
            o.svar(k.flag1 ? k.intValue : e.intValue);
            o.f32(k.flag2 ? k.v[0] : e.floatValue);
            o.boolean(k.flag3);
            if (k.flag3) o.str(k.str2);
            if (!e.audio.empty()) { o.f32(k.volume); o.f32(k.balance); }
        }
    }

    void animation(const Animation& a) {
        std::map<int, std::vector<const Timeline*>> slotTl, boneTl, ikTl, tcTl, pathTl, physTl;
        struct AttKey { int skin, slot; std::string att; bool operator<(const AttKey& r) const { return std::tie(skin, slot, att) < std::tie(r.skin, r.slot, r.att); } };
        std::map<AttKey, std::vector<const Timeline*>> attTl;
        std::vector<const Key*> drawKeys, eventKeysV;
        int total = 0;
        for (auto& tl : a.timelines) {
            if (tl.keys.empty()) continue;
            if (slotCode(tl.type) >= 0 && tl.target >= 0 && tl.target < (int)d.slots.size()) { slotTl[tl.target].push_back(&tl); total++; }
            else if ((int)tl.type < kBoneTimelineCount && tl.target >= 0 && tl.target < (int)d.bones.size()) { boneTl[tl.target].push_back(&tl); total++; }
            else if (tl.type == TimelineType::Ik && tl.target >= 0 && tl.target < (int)d.ik.size()) { ikTl[tl.target].push_back(&tl); total++; }
            else if (tl.type == TimelineType::Transform && tl.target >= 0 && tl.target < (int)d.transforms.size()) { tcTl[tl.target].push_back(&tl); total++; }
            else if ((tl.type == TimelineType::Path || tl.type == TimelineType::PathMix) && tl.target >= 0 && tl.target < (int)d.paths.size()) { pathTl[tl.target].push_back(&tl); total++; }
            else if (isPhysicsTimeline(tl.type) && tl.target < (int)d.physics.size()) { physTl[tl.target + 1].push_back(&tl); total++; }
            else if (tl.type == TimelineType::Deform) {
                int si = d.findSkin(tl.skin);
                if (si < 0 || tl.target < 0) continue;
                attTl[{si, tl.target, tl.attachment}].push_back(&tl); total++;
            } else if (tl.type == TimelineType::Sequence) {
                std::string att = tl.attachment;
                if (att.empty() && tl.target >= 0 && tl.target < (int)d.slots.size()) att = d.slots[tl.target].attachment;
                if (att.empty() || tl.target < 0) { warn("Animation '" + a.name + "': a sequence timeline without an attachment was left out"); continue; }
                attTl[{0, tl.target, att}].push_back(&tl); total++;
            } else if (tl.type == TimelineType::DrawOrder) { for (auto& k : tl.keys) drawKeys.push_back(&k); total++; }
            else if (tl.type == TimelineType::Event) { for (auto& k : tl.keys) eventKeysV.push_back(&k); total++; }
            else if (isSliderTimeline(tl.type)) warn("Animation '" + a.name + "': slider timelines are not part of the 4.2 binary format and were left out");
        }
        auto byTime = [](const Key* x, const Key* y) { return x->time < y->time; };
        std::stable_sort(drawKeys.begin(), drawKeys.end(), byTime);
        std::stable_sort(eventKeysV.begin(), eventKeysV.end(), byTime);

        o.var(total);
        o.var((uint32_t)slotTl.size());
        for (auto& [slot, list] : slotTl) {
            o.var(slot); o.var((uint32_t)list.size());
            std::stable_sort(const_cast<std::vector<const Timeline*>&>(list).begin(), const_cast<std::vector<const Timeline*>&>(list).end(),
                             [](const Timeline* x, const Timeline* y) { return slotCode(x->type) < slotCode(y->type); });
            for (auto* tl : list) slotTimeline(*tl);
        }
        o.var((uint32_t)boneTl.size());
        for (auto& [bone, list] : boneTl) {
            o.var(bone); o.var((uint32_t)list.size());
            for (auto* tl : list) {
                o.u8((int)tl->type);
                o.var((uint32_t)tl->keys.size());
                if (tl->type == TimelineType::Inherit) { for (auto& k : tl->keys) { o.f32(k.time); o.u8(k.intValue); } continue; }
                int ch = timelineValueCount(tl->type);
                curveKeys(*tl, ch, [&](const Key& k) { for (int c = 0; c < ch; c++) o.f32(k.v[c]); });
            }
        }
        o.var((uint32_t)ikTl.size());
        for (auto& [ci, list] : ikTl) {
            const Timeline& tl = *list[0];
            o.var(ci); o.var((uint32_t)tl.keys.size()); o.var(bezierCount(tl, 2));
            for (size_t i = 0; i < tl.keys.size(); i++) {
                const Key& k = tl.keys[i];
                o.f32(k.time); o.f32(k.v[0]); o.f32(k.v[1]);
                if (i > 0) writeCurve(tl.keys[i - 1], 2);
                o.u8(k.flag1 ? 1 : 255); o.boolean(k.flag2); o.boolean(k.flag3);
            }
        }
        o.var((uint32_t)tcTl.size());
        for (auto& [ci, list] : tcTl) {
            const Timeline& tl = *list[0];
            o.var(ci); o.var((uint32_t)tl.keys.size());
            curveKeys(tl, 6, [&](const Key& k) { for (int c = 0; c < 6; c++) o.f32(k.v[c]); });
        }
        o.var((uint32_t)pathTl.size());
        for (auto& [ci, list] : pathTl) {
            o.var(ci); o.var((uint32_t)list.size());
            for (auto* tl : list) {
                bool mix = tl->type == TimelineType::PathMix;
                o.u8(mix ? 2 : 0);
                o.var((uint32_t)tl->keys.size());
                int ch = mix ? 3 : 1;
                curveKeys(*tl, ch, [&](const Key& k) { for (int c = 0; c < ch; c++) o.f32(k.v[c]); });
            }
        }
        o.var((uint32_t)physTl.size());
        for (auto& [ci, list] : physTl) {
            o.var(ci); o.var((uint32_t)list.size());
            for (auto* tl : list) {
                o.u8((int)tl->type - (int)TimelineType::PhysicsInertia);
                o.var((uint32_t)tl->keys.size());
                if (tl->type == TimelineType::PhysicsReset) { for (auto& k : tl->keys) o.f32(k.time); continue; }
                curveKeys(*tl, 1, [&](const Key& k) { o.f32(k.v[0]); });
            }
        }
        // Deform and sequence timelines, grouped skin > slot > attachment.
        std::map<int, std::map<int, std::map<std::string, std::vector<const Timeline*>>>> bySkin;
        for (auto& [key, list] : attTl) bySkin[key.skin][key.slot][key.att] = list;
        o.var((uint32_t)bySkin.size());
        for (auto& [skin, slots] : bySkin) {
            o.var(skin); o.var((uint32_t)slots.size());
            for (auto& [slot, atts] : slots) {
                o.var(slot); o.var((uint32_t)atts.size());
                for (auto& [att, list] : atts) {
                    o.var(st.ref(att)); o.var((uint32_t)list.size());
                    for (auto* tl : list) {
                        if (tl->type == TimelineType::Deform) {
                            o.u8(0);
                            deformKeys(*tl, d.skins[skin].get(slot, att));
                        } else {
                            o.u8(1);
                            o.var((uint32_t)tl->keys.size());
                            for (auto& k : tl->keys) { o.f32(k.time); o.i32(((uint32_t)k.intValue << 4) | (uint32_t)seqModeCode(k.str)); o.f32(k.v[0]); }
                        }
                    }
                }
            }
        }
        drawOrderKeys(drawKeys);
        eventKeys(eventKeysV);
    }

    // ---- everything ----

    std::vector<uint8_t> run() {
        // Body first (it fills the shared string table), header and table are put in front afterwards.
        o.var((uint32_t)d.bones.size());
        for (size_t i = 0; i < d.bones.size(); i++) {
            const BoneData& b = d.bones[i];
            o.str(b.name);
            if (i > 0) o.var(b.parent < 0 ? 0 : b.parent);
            o.f32(b.rotation); o.f32(b.x); o.f32(b.y); o.f32(b.scaleX); o.f32(b.scaleY); o.f32(b.shearX); o.f32(b.shearY); o.f32(b.length);
            o.var((uint32_t)b.inherit); o.boolean(b.skinRequired);
            if (opt.nonessential) { o.i32(rgba8888(b.color.r, b.color.g, b.color.b, b.color.a)); o.strN(b.icon); o.boolean(b.visible); }
        }
        o.var((uint32_t)d.slots.size());
        for (auto& s : d.slots) {
            o.str(s.name); o.var(s.bone);
            o.i32(rgba8888(s.color.r, s.color.g, s.color.b, s.color.a));
            o.i32(s.hasDark ? rgb888(s.dark.r, s.dark.g, s.dark.b) : 0xffffffffu);
            o.var(st.ref(s.attachment));
            o.var((uint32_t)s.blend);
            if (opt.nonessential) o.boolean(s.visible);
        }
        std::vector<std::string> constraintOrder; // index space of skin constraint lists: ik, transform, path, physics
        o.var((uint32_t)d.ik.size());
        for (auto& c : d.ik) {
            constraintOrder.push_back(c.name);
            o.str(c.name); o.var(c.order); o.var((uint32_t)c.bones.size());
            for (int b : c.bones) o.var(b);
            o.var(c.target);
            int flags = (c.skinRequired ? 1 : 0) | (c.bendDirection > 0 ? 2 : 0) | (c.compress ? 4 : 0) | (c.stretch ? 8 : 0) | (c.uniform ? 16 : 0);
            if (c.mix != 0) flags |= 32;
            if (c.mix != 0 && c.mix != 1) flags |= 64;
            if (c.softness != 0) flags |= 128;
            o.u8(flags);
            if (flags & 64) o.f32(c.mix);
            if (flags & 128) o.f32(c.softness);
        }
        o.var((uint32_t)d.transforms.size());
        for (auto& c : d.transforms) {
            constraintOrder.push_back(c.name);
            if (c.mapped) warn("Transform constraint '" + c.name + "': 4.3 property links are not part of the 4.2 binary format, the same-channel copy was written");
            o.str(c.name); o.var(c.order); o.var((uint32_t)c.bones.size());
            for (int b : c.bones) o.var(b);
            o.var(c.target);
            o.u8((c.skinRequired ? 1 : 0) | ((c.local || c.localSource) ? 2 : 0) | (c.relative ? 4 : 0) | 8 | 16 | 32 | 64 | 128);
            o.f32(c.offsetRotation); o.f32(c.offsetX); o.f32(c.offsetY); o.f32(c.offsetScaleX); o.f32(c.offsetScaleY);
            o.u8(1 | 2 | 4 | 8 | 16 | 32 | 64);
            o.f32(c.offsetShearY); o.f32(c.mixRotate); o.f32(c.mixX); o.f32(c.mixY); o.f32(c.mixScaleX); o.f32(c.mixScaleY); o.f32(c.mixShearY);
        }
        o.var((uint32_t)d.paths.size());
        for (auto& c : d.paths) {
            constraintOrder.push_back(c.name);
            o.str(c.name); o.var(c.order); o.boolean(c.skinRequired); o.var((uint32_t)c.bones.size());
            for (int b : c.bones) o.var(b);
            o.var(c.target);
            o.var((uint32_t)c.positionMode); o.var((uint32_t)c.spacingMode); o.var((uint32_t)c.rotateMode);
            o.f32(c.offsetRotation); o.f32(c.position); o.f32(c.spacing); o.f32(c.mixRotate); o.f32(c.mixX); o.f32(c.mixY);
        }
        o.var((uint32_t)d.physics.size());
        for (auto& c : d.physics) {
            constraintOrder.push_back(c.name);
            o.str(c.name); o.var(c.order); o.var(c.bone);
            o.u8((c.skinRequired ? 1 : 0) | 2 | 4 | 8 | 16 | 32 | 64 | 128);
            o.f32(c.x); o.f32(c.y); o.f32(c.rotate); o.f32(c.scaleX); o.f32(c.shearX); o.f32(c.limit);
            o.u8((int)std::lround(std::min(std::max(1.0 / c.step, 1.0), 255.0)));
            o.f32(c.inertia); o.f32(c.strength); o.f32(c.damping); o.f32(c.massInverse); o.f32(c.wind); o.f32(c.gravity);
            o.u8((c.inertiaGlobal ? 1 : 0) | (c.strengthGlobal ? 2 : 0) | (c.dampingGlobal ? 4 : 0) | (c.massGlobal ? 8 : 0) | (c.windGlobal ? 16 : 0) | (c.gravityGlobal ? 32 : 0) | (c.mixGlobal ? 64 : 0) | 128);
            o.f32(c.mix);
        }
        if (!d.sliders.empty()) warn("Slider constraints (Spine 4.3) are not part of the 4.2 binary format and were left out");

        // Skins: the default skin first (no name, bones or constraints), then the named ones.
        Skin emptyDefault;
        skinSlots(d.skins.empty() ? emptyDefault : d.skins[0]);
        o.var(d.skins.empty() ? 0u : (uint32_t)d.skins.size() - 1);
        for (size_t si = 1; si < d.skins.size(); si++) {
            const Skin& s = d.skins[si];
            o.var(st.ref(s.name));
            if (opt.nonessential) o.i32(rgba8888(s.color.r, s.color.g, s.color.b, s.color.a));
            o.var((uint32_t)s.bones.size());
            for (int b : s.bones) o.var(b);
            std::vector<uint32_t> cs;
            for (auto& n : s.constraints) { auto it = std::find(constraintOrder.begin(), constraintOrder.end(), n); if (it != constraintOrder.end()) cs.push_back((uint32_t)(it - constraintOrder.begin())); }
            o.var((uint32_t)cs.size());
            for (uint32_t c : cs) o.var(c);
            skinSlots(s);
        }

        o.var((uint32_t)d.events.size());
        for (auto& e : d.events) {
            o.str(e.name); o.svar(e.intValue); o.f32(e.floatValue); o.strN(e.stringValue); o.strN(e.audio);
            if (!e.audio.empty()) { o.f32(e.volume); o.f32(e.balance); }
        }
        o.var((uint32_t)d.animations.size());
        for (auto& a : d.animations) { o.str(a.name); animation(a); }

        // Assemble: header, string table, body.
        Out h;
        uint64_t hash = 1469598103934665603ull; // FNV-1a of the hash text: the file carries a 64-bit hash
        for (char ch : d.hash) { hash ^= (uint8_t)ch; hash *= 1099511628211ull; }
        if (d.hash.empty()) hash = 0;
        h.i32((uint32_t)(hash & 0xffffffffu)); h.i32((uint32_t)(hash >> 32));
        h.str(opt.version);
        h.f32(d.x); h.f32(d.y); h.f32(d.width); h.f32(d.height); h.f32(d.referenceScale);
        h.boolean(opt.nonessential);
        if (opt.nonessential) { h.f32(d.fps); h.strN(d.imagesPath); h.strN(d.audioPath); }
        h.var((uint32_t)st.list.size());
        for (auto& s : st.list) h.str(s);
        h.b.insert(h.b.end(), o.b.begin(), o.b.end());
        return h.b;
    }
};


// ---- reader ----------------------------------------------------------------------------------

struct Reader {
    In in;
    SkeletonData& d;
    JsonResult& res;
    std::vector<std::string> strings;
    bool nonessential = false;
    Reader(const uint8_t* p, size_t n, SkeletonData& out, JsonResult& r) : in(p, n), d(out), res(r) {}

    std::string ref() { uint32_t i = in.var(); if (i == 0) return {}; if (i > strings.size()) throw std::runtime_error("Bad string reference"); return strings[i - 1]; }
    int index(size_t limit, const char* what) { uint32_t v = in.var(); if (v >= limit) throw std::runtime_error(std::string("Bad ") + what + " index"); return (int)v; }
    static Color fromRgba(uint32_t c) { return Color(((c >> 24) & 255) / 255.f, ((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f); }
    static Color fromRgb(uint32_t c) { return Color(((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f, 1.f); }

    void vertices(Attachment& a, int vcount) {
        bool weighted = in.boolean();
        a.worldVerticesLength = vcount * 2;
        if (!weighted) {
            if ((size_t)vcount * 8 > in.n - in.i) throw std::runtime_error("Bad vertex count");
            for (int k = 0; k < vcount * 2; k++) a.vertices.push_back(in.f32());
            return;
        }
        for (int v = 0; v < vcount; v++) {
            int count = (int)in.cnt(13);
            a.bones.push_back(count);
            for (int k = 0; k < count; k++) {
                a.bones.push_back(index(d.bones.size(), "bone"));
                a.vertices.push_back(in.f32()); a.vertices.push_back(in.f32()); a.vertices.push_back(in.f32());
            }
        }
    }

    Attachment attachment(const std::string& name) {
        Attachment a; a.name = name;
        int flags = in.u8();
        int type = flags & 7;
        auto pathColorSeq = [&] {
            if (flags & 16) a.path = ref();
            if (flags & 32) a.color = fromRgba(in.i32());
            if (flags & 64) { a.seqCount = (int)in.var(); a.seqStart = (int)in.var(); a.seqDigits = (int)in.var(); a.seqSetup = (int)in.var(); }
        };
        auto nonColor = [&] { if (nonessential) a.color = fromRgba(in.i32()); };
        switch (type) {
        case kAttachRegion:
            a.type = AttachmentType::Region; pathColorSeq();
            if (flags & 128) a.rotation = in.f32();
            a.x = in.f32(); a.y = in.f32(); a.scaleX = in.f32(); a.scaleY = in.f32(); a.width = in.f32(); a.height = in.f32();
            break;
        case kAttachMesh: {
            a.type = AttachmentType::Mesh; pathColorSeq();
            int vc = (int)in.cnt(8);
            for (int k = 0; k < vc * 2; k++) a.uvs.push_back(in.f32());
            int tc = (int)in.cnt(2);
            for (int k = 0; k < tc; k++) a.triangles.push_back(in.i16());
            vertices(a, vc);
            a.hullLength = (int)in.var();
            if (nonessential) {
                int ec = (int)in.cnt(2);
                for (int k = 0; k < ec; k++) a.edges.push_back(in.i16() / 2);
                a.width = in.f32(); a.height = in.f32();
            }
            break;
        }
        case kAttachLinked:
            a.type = AttachmentType::LinkedMesh; pathColorSeq();
            a.inheritTimelines = (flags & 128) != 0;
            a.parentSkin = ref(); a.parentMesh = ref();
            if (nonessential) { a.width = in.f32(); a.height = in.f32(); }
            break;
        case kAttachBox: {
            a.type = AttachmentType::BoundingBox;
            int vc = (int)in.cnt(1);
            vertices(a, vc); nonColor();
            break;
        }
        case kAttachPath: {
            a.type = AttachmentType::Path;
            a.closed = (flags & 16) != 0; a.constantSpeed = (flags & 32) != 0;
            int vc = (int)in.cnt(1);
            vertices(a, vc);
            int lc = vc / 3;
            if ((size_t)lc * 4 > in.n - in.i) throw std::runtime_error("Bad path lengths");
            for (int k = 0; k < lc; k++) a.lengths.push_back(in.f32());
            nonColor();
            break;
        }
        case kAttachPoint:
            a.type = AttachmentType::Point;
            a.rotation = in.f32(); a.x = in.f32(); a.y = in.f32(); nonColor();
            break;
        case kAttachClip: {
            a.type = AttachmentType::Clipping;
            int end = index(d.slots.size(), "slot");
            a.endSlot = d.slots[end].name;
            int vc = (int)in.cnt(1);
            vertices(a, vc); nonColor();
            break;
        }
        default: throw std::runtime_error("Unknown attachment type");
        }
        return a;
    }

    void skinSlots(Skin& s) {
        int slots = (int)in.cnt();
        for (int i = 0; i < slots; i++) {
            int slot = index(d.slots.size(), "slot");
            int n = (int)in.cnt();
            for (int k = 0; k < n; k++) { std::string name = ref(); s.set(slot, attachment(name)); }
        }
    }

    void readCurve(Key& k, int channels) {
        int t = in.u8();
        if (t == 1) k.curve = CurveType::Stepped;
        else if (t == 2) { k.curve = CurveType::Bezier; for (int i = 0; i < channels * 4; i++) k.bezier.push_back(in.f32()); }
        else k.curve = CurveType::Linear;
    }
    template <class F> void curveKeys(Timeline& tl, int count, int channels, F vals) {
        in.var(); // bezier count: only needed by readers that preallocate
        for (int i = 0; i < count; i++) {
            Key k; k.time = in.f32();
            vals(k);
            tl.keys.push_back(k);
            if (i > 0) readCurve(tl.keys[i - 1], channels);
        }
    }
    void floatKeys(Timeline& tl, int count, int channels) {
        curveKeys(tl, count, channels, [&](Key& k) { for (int c = 0; c < channels; c++) k.v[c] = in.f32(); });
    }

    void animation(Animation& a) {
        in.var(); // total timeline count
        int slotCount = (int)in.cnt();
        for (int i = 0; i < slotCount; i++) {
            int slot = index(d.slots.size(), "slot");
            int n = (int)in.cnt();
            for (int k = 0; k < n; k++) {
                int code = in.u8();
                int frames = (int)in.cnt();
                Timeline tl; tl.target = slot;
                switch (code) {
                case 0:
                    tl.type = TimelineType::Attachment;
                    for (int f = 0; f < frames; f++) { Key key; key.time = in.f32(); key.str = ref(); tl.keys.push_back(key); }
                    break;
                case 1: tl.type = TimelineType::RGBA; curveKeys(tl, frames, 4, [&](Key& key) { Color c = fromRgba(in.i32()); key.v[0] = c.r; key.v[1] = c.g; key.v[2] = c.b; key.v[3] = c.a; }); break;
                case 2: tl.type = TimelineType::RGB; curveKeys(tl, frames, 3, [&](Key& key) { Color c = fromRgba(in.i32()); key.v[0] = c.r; key.v[1] = c.g; key.v[2] = c.b; }); break;
                case 3: tl.type = TimelineType::RGBA2; curveKeys(tl, frames, 7, [&](Key& key) {
                            Color c = fromRgba(in.i32()), dk = fromRgb(in.i32());
                            key.v[0] = c.r; key.v[1] = c.g; key.v[2] = c.b; key.v[3] = c.a; key.v[4] = dk.r; key.v[5] = dk.g; key.v[6] = dk.b; }); break;
                case 4: tl.type = TimelineType::RGB2; curveKeys(tl, frames, 6, [&](Key& key) {
                            Color c = fromRgba(in.i32()), dk = fromRgb(in.i32());
                            key.v[0] = c.r; key.v[1] = c.g; key.v[2] = c.b; key.v[3] = dk.r; key.v[4] = dk.g; key.v[5] = dk.b; }); break;
                case 5: tl.type = TimelineType::Alpha; curveKeys(tl, frames, 1, [&](Key& key) { key.v[0] = in.u8() / 255.f; }); break;
                default: throw std::runtime_error("Unknown slot timeline type");
                }
                a.timelines.push_back(std::move(tl));
            }
        }
        int boneCount = (int)in.cnt();
        for (int i = 0; i < boneCount; i++) {
            int bone = index(d.bones.size(), "bone");
            int n = (int)in.cnt();
            for (int k = 0; k < n; k++) {
                int code = in.u8();
                if (code >= kBoneTimelineCount) throw std::runtime_error("Unknown bone timeline type");
                int frames = (int)in.cnt();
                Timeline tl; tl.type = (TimelineType)code; tl.target = bone;
                if (tl.type == TimelineType::Inherit) {
                    for (int f = 0; f < frames; f++) { Key key; key.time = in.f32(); key.intValue = in.u8(); tl.keys.push_back(key); }
                } else floatKeys(tl, frames, timelineValueCount(tl.type));
                a.timelines.push_back(std::move(tl));
            }
        }
        int ikCount = (int)in.cnt();
        for (int i = 0; i < ikCount; i++) {
            Timeline tl; tl.type = TimelineType::Ik; tl.target = index(d.ik.size(), "IK constraint");
            int frames = (int)in.cnt();
            in.var();
            for (int f = 0; f < frames; f++) {
                Key key; key.time = in.f32(); key.v[0] = in.f32(); key.v[1] = in.f32();
                tl.keys.push_back(key);
                if (f > 0) readCurve(tl.keys[f - 1], 2);
                Key& cur = tl.keys.back();
                cur.flag1 = (int8_t)in.u8() > 0; cur.flag2 = in.boolean(); cur.flag3 = in.boolean();
            }
            a.timelines.push_back(std::move(tl));
        }
        int tcCount = (int)in.cnt();
        for (int i = 0; i < tcCount; i++) {
            Timeline tl; tl.type = TimelineType::Transform; tl.target = index(d.transforms.size(), "transform constraint");
            int frames = (int)in.cnt();
            floatKeys(tl, frames, 6);
            a.timelines.push_back(std::move(tl));
        }
        int pathCount = (int)in.cnt();
        for (int i = 0; i < pathCount; i++) {
            int ci = index(d.paths.size(), "path constraint");
            int n = (int)in.cnt();
            for (int k = 0; k < n; k++) {
                int code = in.u8();
                int frames = (int)in.cnt();
                Timeline tl; tl.target = ci;
                if (code == 2) { tl.type = TimelineType::PathMix; floatKeys(tl, frames, 3); }
                else if (code == 0) { tl.type = TimelineType::Path; floatKeys(tl, frames, 1); }
                else throw std::runtime_error("Unknown path timeline type");
                a.timelines.push_back(std::move(tl));
            }
        }
        int physCount = (int)in.cnt();
        for (int i = 0; i < physCount; i++) {
            int ci = (int)in.var() - 1;
            if (ci < -1 || ci >= (int)d.physics.size()) throw std::runtime_error("Bad physics constraint index");
            int n = (int)in.cnt();
            for (int k = 0; k < n; k++) {
                int code = in.u8();
                if (code > 7) throw std::runtime_error("Unknown physics timeline type");
                int frames = (int)in.cnt();
                Timeline tl; tl.type = (TimelineType)((int)TimelineType::PhysicsInertia + code); tl.target = ci;
                if (tl.type == TimelineType::PhysicsReset) { for (int f = 0; f < frames; f++) { Key key; key.time = in.f32(); tl.keys.push_back(key); } }
                else floatKeys(tl, frames, 1);
                a.timelines.push_back(std::move(tl));
            }
        }
        int skinCount = (int)in.cnt();
        for (int i = 0; i < skinCount; i++) {
            int skin = index(d.skins.size(), "skin");
            int slots = (int)in.cnt();
            for (int s = 0; s < slots; s++) {
                int slot = index(d.slots.size(), "slot");
                int atts = (int)in.cnt();
                for (int q = 0; q < atts; q++) {
                    std::string att = ref();
                    const Attachment* base = d.skins[skin].get(slot, att);
                    int n = (int)in.cnt();
                    for (int k = 0; k < n; k++) {
                        int code = in.u8();
                        int frames = (int)in.cnt();
                        Timeline tl; tl.target = slot;
                        if (code == 0) {
                            tl.type = TimelineType::Deform; tl.skin = d.skins[skin].name; tl.attachment = att;
                            int len = base ? base->deformLength() : 0;
                            in.var();
                            for (int f = 0; f < frames; f++) {
                                Key key; key.time = in.f32();
                                tl.keys.push_back(key);
                                if (f > 0) readCurve(tl.keys[f - 1], 1);
                                Key& cur = tl.keys.back();
                                cur.deform.assign(len, 0.f);
                                int cnt = (int)in.cnt(4);
                                if (cnt > 0) {
                                    int start = (int)in.var();
                                    if (start < 0 || start + cnt > len) throw std::runtime_error("Bad deform range");
                                    for (int v = 0; v < cnt; v++) cur.deform[start + v] = in.f32();
                                }
                            }
                        } else if (code == 1) {
                            tl.type = TimelineType::Sequence; // the engine keeps sequence timelines per slot only
                            for (int f = 0; f < frames; f++) {
                                Key key; key.time = in.f32();
                                uint32_t mi = in.i32();
                                key.intValue = (int)(mi >> 4); int mode = (int)(mi & 15);
                                key.str = kSeqModes[mode > 6 ? 0 : mode]; key.v[0] = in.f32();
                                tl.keys.push_back(key);
                            }
                        } else throw std::runtime_error("Unknown attachment timeline type");
                        a.timelines.push_back(std::move(tl));
                    }
                }
            }
        }
        int dc = (int)in.cnt();
        if (dc > 0) {
            Timeline tl; tl.type = TimelineType::DrawOrder; tl.target = -1;
            int n = (int)d.slots.size();
            for (int i = 0; i < dc; i++) {
                Key key; key.time = in.f32();
                int changed = (int)in.cnt();
                if (changed > 0) {
                    std::vector<int> order(n, -1), unchanged;
                    int orig = 0;
                    for (int c = 0; c < changed; c++) {
                        int slot = (int)in.var();
                        if (slot < orig || slot >= n) throw std::runtime_error("Bad draw order");
                        while (orig != slot) unchanged.push_back(orig++);
                        int off = (int)in.var();
                        if (orig + off < 0 || orig + off >= n) throw std::runtime_error("Bad draw order");
                        order[orig + off] = orig;
                        orig++;
                    }
                    while (orig < n) unchanged.push_back(orig++);
                    for (int q = n - 1; q >= 0; q--) if (order[q] == -1) { if (unchanged.empty()) throw std::runtime_error("Bad draw order"); order[q] = unchanged.back(); unchanged.pop_back(); }
                    key.drawOrder = order;
                }
                tl.keys.push_back(key);
            }
            a.timelines.push_back(std::move(tl));
        }
        int ec = (int)in.cnt();
        if (ec > 0) {
            Timeline tl; tl.type = TimelineType::Event; tl.target = -1;
            for (int i = 0; i < ec; i++) {
                Key key; key.time = in.f32();
                int ei = index(d.events.size(), "event");
                const EventData& e = d.events[ei];
                key.str = e.name;
                key.intValue = in.svar(); key.v[0] = in.f32();
                key.flag1 = key.intValue != e.intValue; key.flag2 = key.v[0] != e.floatValue;
                if (in.boolean()) { key.flag3 = true; key.str2 = in.str(); }
                if (!e.audio.empty()) { key.volume = in.f32(); key.balance = in.f32(); }
                tl.keys.push_back(key);
            }
            a.timelines.push_back(std::move(tl));
        }
    }

    void read() {
        uint32_t lo = in.i32(), hi = in.i32();
        uint64_t hash = ((uint64_t)hi << 32) | lo;
        d.hash = hash ? std::to_string(hash) : std::string();
        d.version = in.str();
        d.x = in.f32(); d.y = in.f32(); d.width = in.f32(); d.height = in.f32(); d.referenceScale = in.f32();
        nonessential = in.boolean();
        if (nonessential) { d.fps = in.f32(); d.imagesPath = in.str(); d.audioPath = in.str(); }
        int sc = (int)in.cnt();
        for (int i = 0; i < sc; i++) strings.push_back(in.str());

        int bones = (int)in.cnt();
        for (int i = 0; i < bones; i++) {
            BoneData b; b.name = in.str();
            if (i > 0) { b.parent = (int)in.var(); if (b.parent >= i) throw std::runtime_error("Bad bone parent"); }
            b.rotation = in.f32(); b.x = in.f32(); b.y = in.f32(); b.scaleX = in.f32(); b.scaleY = in.f32(); b.shearX = in.f32(); b.shearY = in.f32(); b.length = in.f32();
            uint32_t inh = in.var(); if (inh > 4) throw std::runtime_error("Bad inherit mode");
            b.inherit = (Inherit)inh; b.skinRequired = in.boolean();
            if (nonessential) { b.color = fromRgba(in.i32()); b.icon = in.str(); b.visible = in.boolean(); }
            d.bones.push_back(b);
        }
        int slots = (int)in.cnt();
        for (int i = 0; i < slots; i++) {
            SlotData s; s.name = in.str(); s.bone = index(d.bones.size(), "bone");
            s.color = fromRgba(in.i32());
            uint32_t dark = in.i32();
            if (dark != 0xffffffffu) { s.hasDark = true; s.dark = fromRgb(dark); }
            s.attachment = ref();
            uint32_t bl = in.var(); if (bl > 3) throw std::runtime_error("Bad blend mode");
            s.blend = (BlendMode)bl;
            if (nonessential) s.visible = in.boolean();
            d.slots.push_back(s);
        }
        auto boneList = [&](std::vector<int>& list) { int n = (int)in.cnt(); for (int k = 0; k < n; k++) list.push_back(index(d.bones.size(), "bone")); };
        std::vector<std::string> constraintOrder;
        int ik = (int)in.cnt();
        for (int i = 0; i < ik; i++) {
            IkConstraintData c; c.name = in.str(); c.order = (int)in.var(); boneList(c.bones); c.target = index(d.bones.size(), "bone");
            int flags = in.u8();
            c.skinRequired = flags & 1; c.bendDirection = (flags & 2) ? 1 : -1; c.compress = flags & 4; c.stretch = flags & 8; c.uniform = flags & 16;
            c.mix = (flags & 32) ? ((flags & 64) ? in.f32() : 1.f) : 0.f;
            c.softness = (flags & 128) ? in.f32() : 0.f;
            constraintOrder.push_back(c.name); d.ik.push_back(c);
        }
        int tc = (int)in.cnt();
        for (int i = 0; i < tc; i++) {
            TransformConstraintData c; c.name = in.str(); c.order = (int)in.var(); boneList(c.bones); c.target = index(d.bones.size(), "bone");
            int f1 = in.u8();
            c.skinRequired = f1 & 1; c.local = f1 & 2; c.relative = f1 & 4;
            if (f1 & 8) c.offsetRotation = in.f32();
            if (f1 & 16) c.offsetX = in.f32();
            if (f1 & 32) c.offsetY = in.f32();
            if (f1 & 64) c.offsetScaleX = in.f32();
            if (f1 & 128) c.offsetScaleY = in.f32();
            int f2 = in.u8();
            if (f2 & 1) c.offsetShearY = in.f32();
            c.mixRotate = (f2 & 2) ? in.f32() : 0.f; c.mixX = (f2 & 4) ? in.f32() : 0.f; c.mixY = (f2 & 8) ? in.f32() : 0.f;
            c.mixScaleX = (f2 & 16) ? in.f32() : 0.f; c.mixScaleY = (f2 & 32) ? in.f32() : 0.f; c.mixShearY = (f2 & 64) ? in.f32() : 0.f;
            constraintOrder.push_back(c.name); d.transforms.push_back(c);
        }
        int pc = (int)in.cnt();
        for (int i = 0; i < pc; i++) {
            PathConstraintData c; c.name = in.str(); c.order = (int)in.var(); c.skinRequired = in.boolean(); boneList(c.bones); c.target = index(d.slots.size(), "slot");
            uint32_t pm = in.var(), sm = in.var(), rm = in.var();
            if (pm > 1 || sm > 3 || rm > 2) throw std::runtime_error("Bad path constraint mode");
            c.positionMode = (PathConstraintData::Position)pm; c.spacingMode = (PathConstraintData::Spacing)sm; c.rotateMode = (PathConstraintData::Rotate)rm;
            c.offsetRotation = in.f32(); c.position = in.f32(); c.spacing = in.f32(); c.mixRotate = in.f32(); c.mixX = in.f32(); c.mixY = in.f32();
            constraintOrder.push_back(c.name); d.paths.push_back(c);
        }
        int ph = (int)in.cnt();
        for (int i = 0; i < ph; i++) {
            PhysicsConstraintData c; c.name = in.str(); c.order = (int)in.var(); c.bone = index(d.bones.size(), "bone");
            int f = in.u8();
            c.skinRequired = f & 1;
            c.x = (f & 2) ? in.f32() : 0.f; c.y = (f & 4) ? in.f32() : 0.f; c.rotate = (f & 8) ? in.f32() : 0.f;
            c.scaleX = (f & 16) ? in.f32() : 0.f; c.shearX = (f & 32) ? in.f32() : 0.f; c.limit = (f & 64) ? in.f32() : 5000.f;
            int fps = in.u8(); c.step = 1.0f / (float)std::max(fps, 1);
            c.inertia = in.f32(); c.strength = in.f32(); c.damping = in.f32();
            c.massInverse = (f & 128) ? in.f32() : 1.f; c.wind = in.f32(); c.gravity = in.f32();
            int g = in.u8();
            c.inertiaGlobal = g & 1; c.strengthGlobal = g & 2; c.dampingGlobal = g & 4; c.massGlobal = g & 8; c.windGlobal = g & 16; c.gravityGlobal = g & 32; c.mixGlobal = g & 64;
            c.mix = (g & 128) ? in.f32() : 1.f;
            constraintOrder.push_back(c.name); d.physics.push_back(c);
        }

        Skin def; def.name = "default";
        skinSlots(def);
        d.skins.push_back(def);
        int skins = (int)in.cnt();
        for (int i = 0; i < skins; i++) {
            Skin s; s.name = ref();
            if (nonessential) s.color = fromRgba(in.i32());
            boneList(s.bones);
            int n = (int)in.cnt();
            for (int k = 0; k < n; k++) { uint32_t c = in.var(); if (c >= constraintOrder.size()) throw std::runtime_error("Bad skin constraint"); s.constraints.push_back(constraintOrder[c]); }
            skinSlots(s);
            d.skins.push_back(s);
        }
        int events = (int)in.cnt();
        for (int i = 0; i < events; i++) {
            EventData e; e.name = in.str(); e.intValue = in.svar(); e.floatValue = in.f32(); e.stringValue = in.str(); e.audio = in.str();
            if (!e.audio.empty()) { e.volume = in.f32(); e.balance = in.f32(); }
            d.events.push_back(e);
        }
        int anims = (int)in.cnt();
        for (int i = 0; i < anims; i++) {
            Animation a; a.name = in.str();
            animation(a);
            d.animations.push_back(std::move(a));
        }
        if (d.bones.empty()) { BoneData root; root.name = "root"; d.bones.push_back(root); }
    }
};

} // namespace

std::vector<uint8_t> writeSkeletonBinary(const SkeletonData& data, const BinaryWriteOptions& opt, std::vector<std::string>* warnings) {
    Writer w{data, opt, warnings, {}, {}};
    return w.run();
}

JsonResult readSkeletonBinary(const uint8_t* bytes, size_t size, SkeletonData& out) {
    JsonResult res;
    out = SkeletonData();
    try {
        Reader r(bytes, size, out, res);
        r.read();
        res.ok = true;
    } catch (const std::exception& e) {
        out = SkeletonData();
        res.ok = false;
        res.error = std::string("Invalid .skel data: ") + e.what();
    }
    return res;
}

} // namespace osteon
