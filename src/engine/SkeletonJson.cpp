#include "SkeletonJson.h"
#include "json.hpp"

using nlohmann::json;
using nlohmann::ordered_json;

namespace osteon {

namespace {

float num(const json& j, const char* k, float def) {
    auto it = j.find(k);
    return it != j.end() && it->is_number() ? it->get<float>() : def;
}
int inum(const json& j, const char* k, int def) {
    auto it = j.find(k);
    return it != j.end() && it->is_number() ? it->get<int>() : def;
}
bool boolean(const json& j, const char* k, bool def) {
    auto it = j.find(k);
    return it != j.end() && it->is_boolean() ? it->get<bool>() : def;
}
std::string str(const json& j, const char* k, const std::string& def = {}) {
    auto it = j.find(k);
    return it != j.end() && it->is_string() ? it->get<std::string>() : def;
}
std::vector<float> floats(const json& j, const char* k) {
    std::vector<float> v;
    auto it = j.find(k);
    if (it != j.end() && it->is_array())
        for (auto& x : *it) v.push_back(x.get<float>());
    return v;
}
std::vector<int> ints(const json& j, const char* k) {
    std::vector<int> v;
    auto it = j.find(k);
    if (it != j.end() && it->is_array())
        for (auto& x : *it) v.push_back(x.get<int>());
    return v;
}

struct Reader {
    SkeletonData& d;
    JsonResult& res;
    bool v3 = false; // 3.x data conventions

    // Parses vertex data of length `verticesLength` floats (unweighted) or weighted otherwise.
    void readVertices(const json& j, Attachment& a, int verticesLength) {
        std::vector<float> v = floats(j, "vertices");
        a.worldVerticesLength = verticesLength;
        if ((int)v.size() == verticesLength) {
            a.vertices = v;
            a.bones.clear();
            return;
        }
        a.vertices.clear();
        a.bones.clear();
        bool ok = verticesLength >= 0 && verticesLength % 2 == 0;
        int verts = 0, boneCount = (int)d.bones.size();
        for (size_t i = 0; ok && i < v.size();) {
            int count = (int)v[i++];
            if (count < 0 || count > boneCount || i + 4 * (size_t)count > v.size()) { ok = false; break; }
            a.bones.push_back(count);
            for (int k = 0; k < count; k++) {
                int b = (int)v[i];
                if (b < 0 || b >= boneCount) { ok = false; break; }
                a.bones.push_back(b);
                a.vertices.push_back(v[i + 1]);
                a.vertices.push_back(v[i + 2]);
                a.vertices.push_back(v[i + 3]);
                i += 4;
            }
            verts++;
        }
        if (!ok || verts * 2 != verticesLength) {
            res.warnings.push_back("Attachment has invalid vertex data; it was reset");
            a.bones.clear();
            a.vertices.assign(std::max(0, verticesLength), 0.0f);
        }
    }

    bool readAttachment(const json& j, const std::string& name, int slot, Attachment& a) {
        a.name = str(j, "name", name);
        std::string type = str(j, "type", "region");
        a.path = str(j, "path");
        a.color = Color::fromHex(str(j, "color"));
        a.selectable = boolean(j, "select", true); a.exportable = boolean(j, "export", true);
        a.showName = boolean(j, "showName", false); a.wireframe = boolean(j, "wireframe", false);
        if (auto s = j.find("sequence"); s != j.end() && s->is_object()) {
            a.seqCount = inum(*s, "count", 0);
            a.seqStart = inum(*s, "start", 1);
            a.seqDigits = inum(*s, "digits", 0);
            a.seqSetup = inum(*s, "setup", 0);
        }
        if (type == "region") {
            a.type = AttachmentType::Region;
            a.x = num(j, "x", 0); a.y = num(j, "y", 0);
            a.scaleX = num(j, "scaleX", 1); a.scaleY = num(j, "scaleY", 1);
            a.rotation = num(j, "rotation", 0);
            a.width = num(j, "width", 32); a.height = num(j, "height", 32);
        } else if (type == "mesh") {
            a.type = AttachmentType::Mesh;
            a.uvs = floats(j, "uvs");
            a.triangles = ints(j, "triangles");
            readVertices(j, a, (int)a.uvs.size());
            a.hullLength = inum(j, "hull", 0) / (v3 ? 1 : 1);
            // Spine JSON stores hull as a vertex count.
            a.edges = ints(j, "edges");
            for (int& e : a.edges) e /= 2; // stored as float indices (vertex*2)
            a.width = num(j, "width", 0); a.height = num(j, "height", 0);
            {
                int nv = (int)a.uvs.size() / 2;
                bool bad = a.hullLength < 0 || a.hullLength > nv;
                for (int t : a.triangles) if (t < 0 || t >= nv) bad = true;
                if (a.triangles.size() % 3) bad = true;
                for (int e : a.edges) if (e < 0 || e >= nv) bad = true;
                if (bad) {
                    res.warnings.push_back("Mesh '" + name + "' has invalid triangles/hull; they were cleared");
                    a.triangles.clear(); a.edges.clear(); a.hullLength = std::min(std::max(a.hullLength, 0), nv);
                }
            }
        } else if (type == "linkedmesh") {
            a.type = AttachmentType::LinkedMesh;
            a.parentMesh = str(j, "parent");
            a.parentSkin = str(j, "skin");
            a.inheritTimelines = v3 ? boolean(j, "deform", true) : boolean(j, "timelines", true);
            a.width = num(j, "width", 0); a.height = num(j, "height", 0);
        } else if (type == "boundingbox") {
            a.type = AttachmentType::BoundingBox;
            readVertices(j, a, inum(j, "vertexCount", 0) * 2);
            if (!j.contains("color")) a.color = Color(0.38f, 0.94f, 0, 1);
        } else if (type == "path") {
            a.type = AttachmentType::Path;
            a.closed = boolean(j, "closed", false);
            a.constantSpeed = boolean(j, "constantSpeed", true);
            a.lengths = floats(j, "lengths");
            readVertices(j, a, inum(j, "vertexCount", 0) * 2);
            if (!j.contains("color")) a.color = Color(1, 0.5f, 0, 1);
        } else if (type == "point") {
            a.type = AttachmentType::Point;
            a.x = num(j, "x", 0); a.y = num(j, "y", 0);
            a.rotation = num(j, "rotation", 0);
            if (!j.contains("color")) a.color = Color(0.94f, 0.94f, 0, 1);
        } else if (type == "clipping") {
            a.type = AttachmentType::Clipping;
            a.endSlot = str(j, "end");
            readVertices(j, a, inum(j, "vertexCount", 0) * 2);
            if (!j.contains("color")) a.color = Color(0.8f, 0.2f, 0.2f, 1);
        } else {
            res.warnings.push_back("Unknown attachment type '" + type + "' for " + name);
            return false;
        }
        (void)slot;
        return true;
    }

    void readSkinAttachments(const json& atts, Skin& skin) {
        for (auto& [slotName, slotMap] : atts.items()) {
            int slot = d.findSlot(slotName);
            if (slot < 0) { res.warnings.push_back("Skin '" + skin.name + "': unknown slot " + slotName); continue; }
            for (auto& [attName, aj] : slotMap.items()) {
                Attachment a;
                if (readAttachment(aj, attName, slot, a)) {
                    a.name = attName;
                    skin.set(slot, a);
                }
            }
        }
    }

    // Reads a curve for `key` covering `valueCount` values. 4.x: "stepped" or 4*n absolute floats.
    // 3.x: number cx1 with c2..c4 normalised, applied to every value.
    void readCurve(const json& kj, Key& key, const json* nextKey, int valueCount, const std::vector<std::pair<float, float>>& values) {
        auto it = kj.find("curve");
        if (it == kj.end()) return;
        if (it->is_string()) {
            if (it->get<std::string>() == "stepped") key.curve = CurveType::Stepped;
            return;
        }
        if (it->is_array()) {
            key.curve = CurveType::Bezier;
            for (auto& x : *it) key.bezier.push_back(x.get<float>());
            // 3.x array form [cx1, cy1, cx2, cy2] normalised.
            if (v3 && key.bezier.size() == 4 && nextKey) {
                std::vector<float> n = key.bezier;
                key.bezier.clear();
                appendNormalized(key, *nextKey, n[0], n[1], n[2], n[3], valueCount, values);
            }
            return;
        }
        if (it->is_number() && nextKey) {
            appendNormalized(key, *nextKey, it->get<float>(), num(kj, "c2", 0), num(kj, "c3", 1), num(kj, "c4", 1), valueCount, values);
        }
    }

    void appendNormalized(Key& key, const json& next, float c1, float c2, float c3, float c4, int valueCount,
                          const std::vector<std::pair<float, float>>& values) {
        key.curve = CurveType::Bezier;
        float t0 = key.time, t1 = num(next, "time", 0);
        for (int i = 0; i < valueCount; i++) {
            float v0 = i < (int)values.size() ? values[i].first : 0, v1 = i < (int)values.size() ? values[i].second : 1;
            key.bezier.push_back(t0 + (t1 - t0) * c1);
            key.bezier.push_back(v0 + (v1 - v0) * c2);
            key.bezier.push_back(t0 + (t1 - t0) * c3);
            key.bezier.push_back(v0 + (v1 - v0) * c4);
        }
    }

    // Generic curve timeline reader. `fields` are JSON key names per value with defaults.
    void readCurveTimeline(const json& arr, Timeline& tl, std::vector<std::pair<const char*, float>> fields) {
        int n = (int)fields.size();
        for (size_t i = 0; i < arr.size(); i++) {
            const json& kj = arr[i];
            Key k;
            k.time = num(kj, "time", 0);
            for (int f = 0; f < n; f++) k.v[f] = num(kj, fields[f].first, fields[f].second);
            tl.keys.push_back(k);
        }
        for (size_t i = 0; i < arr.size(); i++) {
            const json* next = i + 1 < arr.size() ? &arr[i + 1] : nullptr;
            std::vector<std::pair<float, float>> vals;
            for (int f = 0; f < n; f++) vals.push_back({tl.keys[i].v[f], next ? tl.keys[i + 1].v[f] : tl.keys[i].v[f]});
            readCurve(arr[i], tl.keys[i], next, n, vals);
        }
    }

    void readColorTimeline(const json& arr, Timeline& tl, bool alpha, bool two, bool alpha2Light) {
        for (size_t i = 0; i < arr.size(); i++) {
            const json& kj = arr[i];
            Key k;
            k.time = num(kj, "time", 0);
            Color light = Color::fromHex(str(kj, two ? "light" : "color"));
            k.v[0] = light.r; k.v[1] = light.g; k.v[2] = light.b;
            int o = 3;
            if (alpha || alpha2Light) { k.v[3] = light.a; o = 4; }
            if (two) {
                Color dark = Color::fromHex(str(kj, "dark"), "000000ff");
                k.v[o] = dark.r; k.v[o + 1] = dark.g; k.v[o + 2] = dark.b;
            }
            tl.keys.push_back(k);
        }
        int n = timelineValueCount(tl.type);
        for (size_t i = 0; i < arr.size(); i++) {
            const json* next = i + 1 < arr.size() ? &arr[i + 1] : nullptr;
            std::vector<std::pair<float, float>> vals;
            for (int f = 0; f < n; f++) vals.push_back({tl.keys[i].v[f], next ? tl.keys[i + 1].v[f] : tl.keys[i].v[f]});
            readCurve(arr[i], tl.keys[i], next, n, vals);
        }
    }

    void readAnimation(const std::string& name, const json& aj) {
        Animation anim;
        anim.name = name;
        if (auto bones = aj.find("bones"); bones != aj.end())
            for (auto& [boneName, tls] : bones->items()) {
                int bone = d.findBone(boneName);
                if (bone < 0) { res.warnings.push_back("Animation " + name + ": unknown bone " + boneName); continue; }
                for (auto& [tn, arr] : tls.items()) {
                    Timeline tl;
                    tl.target = bone;
                    if (tn == "rotate") { tl.type = TimelineType::Rotate; readCurveTimeline(arr, tl, {{v3 ? "angle" : "value", 0}}); if (!v3) {} }
                    else if (tn == "translate") { tl.type = TimelineType::Translate; readCurveTimeline(arr, tl, {{"x", 0}, {"y", 0}}); }
                    else if (tn == "translatex") { tl.type = TimelineType::TranslateX; readCurveTimeline(arr, tl, {{"value", 0}}); }
                    else if (tn == "translatey") { tl.type = TimelineType::TranslateY; readCurveTimeline(arr, tl, {{"value", 0}}); }
                    else if (tn == "scale") { tl.type = TimelineType::Scale; readCurveTimeline(arr, tl, {{"x", 1}, {"y", 1}}); }
                    else if (tn == "scalex") { tl.type = TimelineType::ScaleX; readCurveTimeline(arr, tl, {{"value", 1}}); }
                    else if (tn == "scaley") { tl.type = TimelineType::ScaleY; readCurveTimeline(arr, tl, {{"value", 1}}); }
                    else if (tn == "shear") { tl.type = TimelineType::Shear; readCurveTimeline(arr, tl, {{"x", 0}, {"y", 0}}); }
                    else if (tn == "shearx") { tl.type = TimelineType::ShearX; readCurveTimeline(arr, tl, {{"value", 0}}); }
                    else if (tn == "sheary") { tl.type = TimelineType::ShearY; readCurveTimeline(arr, tl, {{"value", 0}}); }
                    else if (tn == "inherit") {
                        tl.type = TimelineType::Inherit;
                        for (auto& kj : arr) { Key k; k.time = num(kj, "time", 0); k.intValue = (int)inheritFromName(str(kj, "inherit", "normal")); tl.keys.push_back(k); }
                    } else { res.warnings.push_back("Unsupported bone timeline " + tn); continue; }
                    // 3.x rotate keys were absolute "angle" offsets, same meaning as 4.x "value".
                    anim.timelines.push_back(std::move(tl));
                }
            }
        if (auto slots = aj.find("slots"); slots != aj.end())
            for (auto& [slotName, tls] : slots->items()) {
                int slot = d.findSlot(slotName);
                if (slot < 0) { res.warnings.push_back("Animation " + name + ": unknown slot " + slotName); continue; }
                for (auto& [tn, arr] : tls.items()) {
                    Timeline tl;
                    tl.target = slot;
                    if (tn == "attachment") {
                        tl.type = TimelineType::Attachment;
                        for (auto& kj : arr) {
                            Key k; k.time = num(kj, "time", 0);
                            k.str = kj.contains("name") && kj["name"].is_string() ? kj["name"].get<std::string>() : "";
                            tl.keys.push_back(k);
                        }
                    } else if (tn == "rgba" || (v3 && tn == "color")) { tl.type = TimelineType::RGBA; readColorTimeline(arr, tl, true, false, false); }
                    else if (tn == "rgb") { tl.type = TimelineType::RGB; readColorTimeline(arr, tl, false, false, false); }
                    else if (tn == "alpha") { tl.type = TimelineType::Alpha; readCurveTimeline(arr, tl, {{"value", 1}}); }
                    else if (tn == "rgba2" || (v3 && tn == "twoColor")) { tl.type = TimelineType::RGBA2; readColorTimeline(arr, tl, true, true, true); }
                    else if (tn == "rgb2") { tl.type = TimelineType::RGB2; readColorTimeline(arr, tl, false, true, false); }
                    else if (tn == "sequence") {
                        tl.type = TimelineType::Sequence; // stored only; playback of sequences is not simulated
                        for (auto& kj : arr) { Key k; k.time = num(kj, "time", 0); k.intValue = inum(kj, "index", 0); k.str = str(kj, "mode", "hold"); k.v[0] = num(kj, "delay", 0); tl.keys.push_back(k); }
                    } else { res.warnings.push_back("Unsupported slot timeline " + tn); continue; }
                    anim.timelines.push_back(std::move(tl));
                }
            }
        auto readConstraintMap = [&](const char* key, auto fn) {
            if (auto m = aj.find(key); m != aj.end() && m->is_object())
                for (auto& [cn, arr] : m->items()) fn(cn, arr);
        };
        readConstraintMap("ik", [&](const std::string& cn, const json& arr) {
            int idx = -1;
            for (size_t i = 0; i < d.ik.size(); i++) if (d.ik[i].name == cn) idx = (int)i;
            if (idx < 0) return;
            Timeline tl; tl.type = TimelineType::Ik; tl.target = idx;
            readCurveTimeline(arr, tl, {{"mix", 1}, {"softness", 0}});
            for (size_t i = 0; i < arr.size(); i++) {
                tl.keys[i].flag1 = boolean(arr[i], "bendPositive", true);
                tl.keys[i].flag2 = boolean(arr[i], "compress", false);
                tl.keys[i].flag3 = boolean(arr[i], "stretch", false);
            }
            anim.timelines.push_back(std::move(tl));
        });
        readConstraintMap("transform", [&](const std::string& cn, const json& arr) {
            int idx = -1;
            for (size_t i = 0; i < d.transforms.size(); i++) if (d.transforms[i].name == cn) idx = (int)i;
            if (idx < 0) return;
            Timeline tl; tl.type = TimelineType::Transform; tl.target = idx;
            if (v3) readCurveTimeline(arr, tl, {{"rotateMix", 1}, {"translateMix", 1}, {"translateMix", 1}, {"scaleMix", 1}, {"scaleMix", 1}, {"shearMix", 1}});
            else {
                readCurveTimeline(arr, tl, {{"mixRotate", 1}, {"mixX", 1}, {"mixY", 1}, {"mixScaleX", 1}, {"mixScaleY", 1}, {"mixShearY", 1}});
                for (size_t i = 0; i < arr.size(); i++) {
                    if (!arr[i].contains("mixY")) tl.keys[i].v[2] = tl.keys[i].v[1];
                    if (!arr[i].contains("mixScaleY")) tl.keys[i].v[4] = tl.keys[i].v[3];
                }
            }
            anim.timelines.push_back(std::move(tl));
        });
        readConstraintMap("path", [&](const std::string& cn, const json& tls) {
            int idx = -1;
            for (size_t i = 0; i < d.paths.size(); i++) if (d.paths[i].name == cn) idx = (int)i;
            if (idx < 0) return;
            for (auto& [tn, arr] : tls.items()) {
                Timeline tl; tl.target = idx;
                if (tn == "position") { tl.type = TimelineType::Path; readCurveTimeline(arr, tl, {{"value", 0}}); }
                else if (tn == "mix") { tl.type = TimelineType::PathMix; readCurveTimeline(arr, tl, {{"mixRotate", 1}, {"mixX", 1}, {"mixY", 1}}); }
                else { res.warnings.push_back("Unsupported path timeline " + tn); continue; }
                anim.timelines.push_back(std::move(tl));
            }
        });
        readConstraintMap("physics", [&](const std::string& cn, const json& tls) {
            int idx = -1; // an empty name is the global row
            if (!cn.empty()) {
                for (size_t i = 0; i < d.physics.size(); i++) if (d.physics[i].name == cn) idx = (int)i;
                if (idx < 0) return;
            }
            static const std::pair<const char*, TimelineType> kinds[] = {
                {"inertia", TimelineType::PhysicsInertia}, {"strength", TimelineType::PhysicsStrength}, {"damping", TimelineType::PhysicsDamping},
                {"mass", TimelineType::PhysicsMass}, {"wind", TimelineType::PhysicsWind}, {"gravity", TimelineType::PhysicsGravity},
                {"mix", TimelineType::PhysicsMix}, {"reset", TimelineType::PhysicsReset}};
            for (auto& [tn, arr] : tls.items()) {
                bool known = false;
                for (auto& [name, type] : kinds) {
                    if (tn != name) continue;
                    known = true;
                    Timeline tl; tl.type = type; tl.target = idx;
                    if (type == TimelineType::PhysicsReset) { for (auto& kj : arr) { Key k; k.time = num(kj, "time", 0); tl.keys.push_back(k); } }
                    else readCurveTimeline(arr, tl, {{"value", type == TimelineType::PhysicsMix || type == TimelineType::PhysicsInertia || type == TimelineType::PhysicsDamping || type == TimelineType::PhysicsMass ? 1 : 0}});
                    anim.timelines.push_back(std::move(tl));
                }
                if (!known) res.warnings.push_back("Unsupported physics timeline " + tn);
            }
        });
        auto readDeform = [&](const json& skins, bool nested) {
            for (auto& [skinName, slots] : skins.items()) {
                int skin = d.findSkin(skinName);
                if (skin < 0) continue;
                for (auto& [slotName, atts] : slots.items()) {
                    int slot = d.findSlot(slotName);
                    if (slot < 0) continue;
                    for (auto& [attName, val] : atts.items()) {
                        const json* arr = &val;
                        if (nested) {
                            auto it = val.find("deform");
                            if (it == val.end()) continue;
                            arr = &*it;
                        }
                        const Attachment* att = d.skins[skin].get(slot, attName);
                        if (!att) continue;
                        const Attachment* geom = att;
                        if (att->type == AttachmentType::LinkedMesh) geom = d.findAttachment(d.findSkin(att->parentSkin), slot, att->parentMesh);
                        if (!geom) continue;
                        int len = geom->deformLength();
                        Timeline tl; tl.type = TimelineType::Deform; tl.target = slot; tl.skin = skinName; tl.attachment = attName;
                        for (auto& kj : *arr) {
                            Key k; k.time = num(kj, "time", 0);
                            k.deform.assign(len, 0.0f);
                            std::vector<float> v = floats(kj, "vertices");
                            int off = std::max(0, inum(kj, "offset", 0));
                            for (size_t i = 0; i < v.size() && off + (int)i < len; i++) k.deform[off + i] = v[i];
                            tl.keys.push_back(k);
                        }
                        for (size_t i = 0; i < arr->size(); i++) {
                            const json* next = i + 1 < arr->size() ? &(*arr)[i + 1] : nullptr;
                            readCurve((*arr)[i], tl.keys[i], next, 1, {{0.0f, 1.0f}});
                        }
                        anim.timelines.push_back(std::move(tl));
                    }
                }
            }
        };
        if (auto dj = aj.find("deform"); dj != aj.end()) readDeform(*dj, false);          // 3.8 - 4.1
        if (auto dj = aj.find("attachments"); dj != aj.end()) readDeform(*dj, true);      // 4.2
        if (auto doj = aj.find(v3 ? "draworder" : "drawOrder"); doj != aj.end() || (doj = aj.find("drawOrder")) != aj.end()) {
            Timeline tl; tl.type = TimelineType::DrawOrder; tl.target = -1;
            int n = (int)d.slots.size();
            for (auto& kj : *doj) {
                Key k; k.time = num(kj, "time", 0);
                auto offs = kj.find("offsets");
                if (offs != kj.end() && offs->is_array()) {
                    std::vector<int> order(n, -1), unchanged;
                    int orig = 0;
                    for (auto& o : *offs) {
                        int si = d.findSlot(str(o, "slot"));
                        if (si < 0) continue;
                        while (orig != si && orig < n) unchanged.push_back(orig++);
                        int pos = orig + inum(o, "offset", 0);
                        if (pos >= 0 && pos < n) order[pos] = orig;
                        orig++;
                    }
                    while (orig < n) unchanged.push_back(orig++);
                    for (int i = n - 1; i >= 0; i--)
                        if (order[i] == -1 && !unchanged.empty()) { order[i] = unchanged.back(); unchanged.pop_back(); }
                    if (std::find(order.begin(), order.end(), -1) != order.end()) {
                        res.warnings.push_back("Animation " + name + ": invalid draw order key ignored");
                        order.clear();
                    }
                    k.drawOrder = order;
                }
                tl.keys.push_back(k);
            }
            anim.timelines.push_back(std::move(tl));
        }
        if (auto ej = aj.find("events"); ej != aj.end()) {
            Timeline tl; tl.type = TimelineType::Event; tl.target = -1;
            for (auto& kj : *ej) {
                Key k; k.time = num(kj, "time", 0);
                k.str = str(kj, "name");
                if (kj.contains("int")) { k.flag1 = true; k.intValue = inum(kj, "int", 0); }
                if (kj.contains("float")) { k.flag2 = true; k.v[0] = num(kj, "float", 0); }
                if (kj.contains("string")) { k.flag3 = true; k.str2 = str(kj, "string"); }
                k.volume = num(kj, "volume", 1);
                k.balance = num(kj, "balance", 0);
                tl.keys.push_back(k);
            }
            anim.timelines.push_back(std::move(tl));
        }
        for (auto& tl : anim.timelines)
            std::stable_sort(tl.keys.begin(), tl.keys.end(), [](const Key& a, const Key& b) { return a.time < b.time; });
        d.animations.push_back(std::move(anim));
    }

    void read(const json& root) {
        if (auto sk = root.find("skeleton"); sk != root.end()) {
            d.hash = str(*sk, "hash");
            std::string ver = str(*sk, "spine");
            if (!ver.empty()) {
                d.version = ver;
                v3 = ver[0] == '3';
            }
            d.x = num(*sk, "x", 0); d.y = num(*sk, "y", 0);
            d.width = num(*sk, "width", 0); d.height = num(*sk, "height", 0);
            d.fps = num(*sk, "fps", 30);
            d.referenceScale = num(*sk, "referenceScale", 100);
            d.imagesPath = str(*sk, "images");
            d.audioPath = str(*sk, "audio");
        }
        if (auto bones = root.find("bones"); bones != root.end())
            for (auto& bj : *bones) {
                BoneData b;
                b.name = str(bj, "name");
                std::string parent = str(bj, "parent");
                if (!parent.empty()) {
                    b.parent = d.findBone(parent);
                    if (b.parent < 0) { res.error = "Parent bone not found: " + parent; return; }
                }
                b.length = num(bj, "length", 0);
                b.x = num(bj, "x", 0); b.y = num(bj, "y", 0);
                b.rotation = num(bj, "rotation", 0);
                b.scaleX = num(bj, "scaleX", 1); b.scaleY = num(bj, "scaleY", 1);
                b.shearX = num(bj, "shearX", 0); b.shearY = num(bj, "shearY", 0);
                b.inherit = inheritFromName(str(bj, v3 ? "transform" : "inherit", str(bj, "transform", "normal")));
                b.skinRequired = boolean(bj, "skin", false);
                b.color = Color::fromHex(str(bj, "color"), "989898ff");
                b.icon = str(bj, "icon");
                b.visible = boolean(bj, "visible", true);
                b.line = boolean(bj, "line", false);
                b.thickness = num(bj, "thickness", 100);
                d.bones.push_back(b);
            }
        if (auto slots = root.find("slots"); slots != root.end())
            for (auto& sj : *slots) {
                SlotData s;
                s.name = str(sj, "name");
                s.bone = d.findBone(str(sj, "bone"));
                if (s.bone < 0) { res.error = "Slot bone not found: " + str(sj, "bone"); return; }
                s.color = Color::fromHex(str(sj, "color"));
                if (sj.contains("dark")) { s.hasDark = true; s.dark = Color::fromHex(str(sj, "dark"), "000000ff"); }
                s.attachment = str(sj, "attachment");
                s.blend = blendFromName(str(sj, "blend", "normal"));
                s.visible = boolean(sj, "visible", true);
                d.slots.push_back(s);
            }
        auto boneList = [&](const json& cj, const char* key) {
            std::vector<int> r;
            if (auto it = cj.find(key); it != cj.end())
                for (auto& n : *it) { int b = d.findBone(n.get<std::string>()); if (b >= 0) r.push_back(b); }
            return r;
        };
        if (auto ik = root.find("ik"); ik != root.end())
            for (auto& cj : *ik) {
                IkConstraintData c;
                c.name = str(cj, "name"); c.order = inum(cj, "order", 0); c.skinRequired = boolean(cj, "skin", false);
                c.bones = boneList(cj, "bones");
                c.target = d.findBone(str(cj, "target"));
                c.mix = num(cj, "mix", 1); c.softness = num(cj, "softness", 0);
                c.bendDirection = boolean(cj, "bendPositive", true) ? 1 : -1;
                c.compress = boolean(cj, "compress", false); c.stretch = boolean(cj, "stretch", false); c.uniform = boolean(cj, "uniform", false);
                if (c.bones.empty() || c.target < 0) { res.warnings.push_back("IK constraint ignored: " + c.name); continue; }
                d.ik.push_back(c);
            }
        if (auto tc = root.find("transform"); tc != root.end())
            for (auto& cj : *tc) {
                TransformConstraintData c;
                c.name = str(cj, "name"); c.order = inum(cj, "order", 0); c.skinRequired = boolean(cj, "skin", false);
                c.bones = boneList(cj, "bones");
                c.target = d.findBone(str(cj, "target"));
                c.local = boolean(cj, "local", false); c.relative = boolean(cj, "relative", false);
                c.offsetRotation = num(cj, "rotation", 0); c.offsetX = num(cj, "x", 0); c.offsetY = num(cj, "y", 0);
                c.offsetScaleX = num(cj, "scaleX", 0); c.offsetScaleY = num(cj, "scaleY", 0); c.offsetShearY = num(cj, "shearY", 0);
                if (v3) {
                    c.mixRotate = num(cj, "rotateMix", 1); c.mixX = c.mixY = num(cj, "translateMix", 1);
                    c.mixScaleX = c.mixScaleY = num(cj, "scaleMix", 1); c.mixShearY = num(cj, "shearMix", 1);
                } else {
                    c.mixRotate = num(cj, "mixRotate", 1); c.mixX = num(cj, "mixX", 1); c.mixY = num(cj, "mixY", c.mixX);
                    c.mixScaleX = num(cj, "mixScaleX", 1); c.mixScaleY = num(cj, "mixScaleY", c.mixScaleX); c.mixShearY = num(cj, "mixShearY", 1);
                }
                if (c.bones.empty() || c.target < 0) { res.warnings.push_back("Transform constraint ignored: " + c.name); continue; }
                d.transforms.push_back(c);
            }
        if (auto pc = root.find("path"); pc != root.end())
            for (auto& cj : *pc) {
                PathConstraintData c;
                c.name = str(cj, "name"); c.order = inum(cj, "order", 0); c.skinRequired = boolean(cj, "skin", false);
                c.bones = boneList(cj, "bones");
                c.target = d.findSlot(str(cj, "target"));
                std::string pm = str(cj, "positionMode", "percent"), sm = str(cj, "spacingMode", "length"), rm = str(cj, "rotateMode", "tangent");
                c.positionMode = pm == "fixed" ? PathConstraintData::Position::Fixed : PathConstraintData::Position::Percent;
                c.spacingMode = sm == "fixed" ? PathConstraintData::Spacing::Fixed : sm == "percent" ? PathConstraintData::Spacing::Percent
                              : sm == "proportional" ? PathConstraintData::Spacing::Proportional : PathConstraintData::Spacing::Length;
                c.rotateMode = rm == "chain" ? PathConstraintData::Rotate::Chain : rm == "chainScale" ? PathConstraintData::Rotate::ChainScale : PathConstraintData::Rotate::Tangent;
                c.offsetRotation = num(cj, "rotation", 0); c.position = num(cj, "position", 0); c.spacing = num(cj, "spacing", 0);
                if (v3) { c.mixRotate = num(cj, "rotateMix", 1); c.mixX = c.mixY = num(cj, "translateMix", 1); }
                else { c.mixRotate = num(cj, "mixRotate", 1); c.mixX = num(cj, "mixX", 1); c.mixY = num(cj, "mixY", c.mixX); }
                if (c.bones.empty() || c.target < 0) { res.warnings.push_back("Path constraint ignored: " + c.name); continue; }
                d.paths.push_back(c);
            }
        if (auto ph = root.find("physics"); ph != root.end())
            for (auto& cj : *ph) {
                PhysicsConstraintData c;
                c.name = str(cj, "name"); c.order = inum(cj, "order", 0); c.skinRequired = boolean(cj, "skin", false);
                c.bone = d.findBone(str(cj, "bone"));
                c.x = num(cj, "x", 0); c.y = num(cj, "y", 0); c.rotate = num(cj, "rotate", 0);
                c.scaleX = num(cj, "scaleX", 0); c.shearX = num(cj, "shearX", 0); c.limit = num(cj, "limit", 5000);
                c.step = 1.0f / std::min(std::max(num(cj, "fps", 60), 1.0f), 1000.0f); c.inertia = num(cj, "inertia", 1); c.strength = num(cj, "strength", 100);
                c.damping = num(cj, "damping", 1); c.massInverse = 1.0f / std::max(num(cj, "mass", 1), 1e-3f); c.wind = num(cj, "wind", 0);
                c.gravity = num(cj, "gravity", 0); c.mix = num(cj, "mix", 1);
                c.inertiaGlobal = boolean(cj, "inertiaGlobal", false); c.strengthGlobal = boolean(cj, "strengthGlobal", false);
                c.dampingGlobal = boolean(cj, "dampingGlobal", false); c.massGlobal = boolean(cj, "massGlobal", false);
                c.windGlobal = boolean(cj, "windGlobal", false); c.gravityGlobal = boolean(cj, "gravityGlobal", false); c.mixGlobal = boolean(cj, "mixGlobal", false);
                if (c.bone < 0) { res.warnings.push_back("Physics constraint ignored: " + c.name); continue; }
                d.physics.push_back(c);
            }
        if (auto skins = root.find("skins"); skins != root.end()) {
            if (skins->is_array()) {
                for (auto& sj : *skins) {
                    Skin skin;
                    skin.name = str(sj, "name");
                    skin.bones = boneList(sj, "bones");
                    for (const char* key : {"ik", "transform", "path", "physics"})
                        if (auto it = sj.find(key); it != sj.end())
                            for (auto& n : *it) skin.constraints.push_back(n.get<std::string>());
                    if (auto a = sj.find("attachments"); a != sj.end()) readSkinAttachments(*a, skin);
                    d.skins.push_back(std::move(skin));
                }
            } else if (skins->is_object()) { // pre-3.8 / Photoshop export layout
                for (auto& [name, a] : skins->items()) {
                    Skin skin;
                    skin.name = name;
                    readSkinAttachments(a, skin);
                    d.skins.push_back(std::move(skin));
                }
            }
        }
        // Default skin first.
        int def = d.findSkin("default");
        if (def > 0) std::rotate(d.skins.begin(), d.skins.begin() + def, d.skins.begin() + def + 1);
        d.defaultSkin();
        if (auto ev = root.find("events"); ev != root.end())
            for (auto& [name, ej] : ev->items()) {
                EventData e;
                e.name = name;
                e.intValue = inum(ej, "int", 0); e.floatValue = num(ej, "float", 0);
                e.stringValue = str(ej, "string"); e.audio = str(ej, "audio");
                e.volume = num(ej, "volume", 1); e.balance = num(ej, "balance", 0);
                d.events.push_back(e);
            }
        if (auto an = root.find("animations"); an != root.end())
            for (auto& [name, aj] : an->items()) readAnimation(name, aj);
        if (d.bones.empty()) {
            BoneData root; root.name = "root";
            d.bones.push_back(root);
        }
        res.ok = res.error.empty();
    }
};

// ---- writer ----

std::string fmtColor(const Color& c, bool alpha = true) { return c.toHex(alpha); }

// Order of a weighted attachment's influences when written: within each vertex, sorted by bone
// index. The Spine editor rejects weighted meshes whose influence order differs between vertices.
// Returns, for each written influence, the index of the stored influence it comes from.
std::vector<int> influenceOrder(const Attachment& a) {
    std::vector<int> order;
    int infl = 0;
    for (size_t i = 0; i < a.bones.size();) {
        int count = a.bones[i++];
        std::vector<std::pair<int, int>> v; // (bone, influence index)
        for (int k = 0; k < count; k++) v.push_back({a.bones[i++], infl++});
        std::stable_sort(v.begin(), v.end());
        for (auto& p : v) order.push_back(p.second);
    }
    return order;
}

ordered_json writeVertices(const Attachment& a) {
    ordered_json v = ordered_json::array();
    if (!a.isWeighted()) {
        for (float f : a.vertices) v.push_back(f);
        return v;
    }
    // Bone index of each stored influence.
    std::vector<int> boneOf;
    for (size_t i = 0; i < a.bones.size();) {
        int count = a.bones[i++];
        for (int k = 0; k < count; k++) boneOf.push_back(a.bones[i++]);
    }
    std::vector<int> order = influenceOrder(a);
    size_t o = 0;
    for (size_t i = 0; i < a.bones.size();) {
        int count = a.bones[i];
        i += count + 1;
        v.push_back(count);
        for (int k = 0; k < count; k++, o++) {
            int src = order[o];
            v.push_back(boneOf[src]);
            v.push_back(a.vertices[src * 3]);
            v.push_back(a.vertices[src * 3 + 1]);
            v.push_back(a.vertices[src * 3 + 2]);
        }
    }
    return v;
}

ordered_json writeAttachment(const Attachment& a, bool nonessential, bool project) {
    ordered_json j;
    if (project) { // editor options live only in project files
        if (!a.selectable) j["select"] = false;
        if (!a.exportable) j["export"] = false;
        if (a.showName) j["showName"] = true;
        if (a.wireframe) j["wireframe"] = true;
    }
    if (a.type != AttachmentType::Region) j["type"] = attachmentTypeName(a.type);
    if (!a.path.empty() && a.path != a.name) j["path"] = a.path;
    bool defColor = a.color == Color();
    switch (a.type) {
    case AttachmentType::Region:
        if (a.x != 0) j["x"] = a.x;
        if (a.y != 0) j["y"] = a.y;
        if (a.scaleX != 1) j["scaleX"] = a.scaleX;
        if (a.scaleY != 1) j["scaleY"] = a.scaleY;
        if (a.rotation != 0) j["rotation"] = a.rotation;
        j["width"] = a.width; j["height"] = a.height;
        if (!defColor) j["color"] = fmtColor(a.color);
        break;
    case AttachmentType::Mesh: {
        j["uvs"] = a.uvs;
        j["triangles"] = a.triangles;
        j["vertices"] = writeVertices(a);
        j["hull"] = a.hullLength;
        if (nonessential) {
            if (!a.edges.empty()) {
                std::vector<int> e;
                for (int x : a.edges) e.push_back(x * 2);
                j["edges"] = e;
            }
            j["width"] = a.width; j["height"] = a.height;
        }
        if (!defColor) j["color"] = fmtColor(a.color);
        break;
    }
    case AttachmentType::LinkedMesh:
        j["parent"] = a.parentMesh;
        if (!a.parentSkin.empty()) j["skin"] = a.parentSkin;
        if (!a.inheritTimelines) j["timelines"] = false;
        if (nonessential) { j["width"] = a.width; j["height"] = a.height; }
        if (!defColor) j["color"] = fmtColor(a.color);
        break;
    case AttachmentType::BoundingBox:
        j["vertexCount"] = a.vertexCount();
        j["vertices"] = writeVertices(a);
        if (nonessential) j["color"] = fmtColor(a.color);
        break;
    case AttachmentType::Path:
        if (a.closed) j["closed"] = true;
        if (!a.constantSpeed) j["constantSpeed"] = false;
        j["lengths"] = a.lengths;
        j["vertexCount"] = a.vertexCount();
        j["vertices"] = writeVertices(a);
        if (nonessential) j["color"] = fmtColor(a.color);
        break;
    case AttachmentType::Point:
        if (a.x != 0) j["x"] = a.x;
        if (a.y != 0) j["y"] = a.y;
        if (a.rotation != 0) j["rotation"] = a.rotation;
        if (nonessential) j["color"] = fmtColor(a.color);
        break;
    case AttachmentType::Clipping:
        if (!a.endSlot.empty()) j["end"] = a.endSlot;
        j["vertexCount"] = a.vertexCount();
        j["vertices"] = writeVertices(a);
        if (nonessential) j["color"] = fmtColor(a.color);
        break;
    }
    if (a.seqCount > 0) {
        ordered_json s;
        s["count"] = a.seqCount;
        if (a.seqStart != 1) s["start"] = a.seqStart;
        if (a.seqDigits) s["digits"] = a.seqDigits;
        if (a.seqSetup) s["setup"] = a.seqSetup;
        j["sequence"] = s;
    }
    return j;
}

void writeCurve(ordered_json& kj, const Key& k, bool isLast) {
    if (isLast) return;
    if (k.curve == CurveType::Stepped) kj["curve"] = "stepped";
    else if (k.curve == CurveType::Bezier && !k.bezier.empty()) kj["curve"] = k.bezier;
}

ordered_json writeCurveKeys(const Timeline& tl, std::vector<std::pair<const char*, float>> fields) {
    ordered_json arr = ordered_json::array();
    for (size_t i = 0; i < tl.keys.size(); i++) {
        const Key& k = tl.keys[i];
        ordered_json kj = ordered_json::object();
        if (k.time != 0) kj["time"] = k.time;
        for (size_t f = 0; f < fields.size(); f++)
            if (k.v[f] != fields[f].second) kj[fields[f].first] = k.v[f];
        writeCurve(kj, k, i + 1 == tl.keys.size());
        arr.push_back(kj);
    }
    return arr;
}

ordered_json writeColorKeys(const Timeline& tl) {
    ordered_json arr = ordered_json::array();
    for (size_t i = 0; i < tl.keys.size(); i++) {
        const Key& k = tl.keys[i];
        ordered_json kj = ordered_json::object();
        if (k.time != 0) kj["time"] = k.time;
        switch (tl.type) {
        case TimelineType::RGBA: kj["color"] = Color(k.v[0], k.v[1], k.v[2], k.v[3]).toHex(true); break;
        case TimelineType::RGB: kj["color"] = Color(k.v[0], k.v[1], k.v[2]).toHex(false); break;
        case TimelineType::RGBA2:
            kj["light"] = Color(k.v[0], k.v[1], k.v[2], k.v[3]).toHex(true);
            kj["dark"] = Color(k.v[4], k.v[5], k.v[6]).toHex(false);
            break;
        case TimelineType::RGB2:
            kj["light"] = Color(k.v[0], k.v[1], k.v[2]).toHex(false);
            kj["dark"] = Color(k.v[3], k.v[4], k.v[5]).toHex(false);
            break;
        default: break;
        }
        writeCurve(kj, k, i + 1 == tl.keys.size());
        arr.push_back(kj);
    }
    return arr;
}

} // namespace

JsonResult readSkeletonJson(const std::string& text, SkeletonData& out) {
    JsonResult res;
    json root;
    try {
        root = json::parse(text);
    } catch (const std::exception& e) {
        res.error = std::string("Invalid JSON: ") + e.what();
        return res;
    }
    if (auto cs = root.find("constraints"); cs != root.end() && cs->is_array()) {
        // Spine 4.3: one typed array, in evaluation order. Convert to the 4.2 per-type arrays.
        int order = 0;
        for (auto& c : *cs) {
            std::string type = c.value("type", "");
            json e = c;
            e.erase("type");
            e["order"] = order++;
            if (type == "transform") {
                if (e.contains("source")) { e["target"] = e["source"]; e.erase("source"); }
                if (auto props = c.find("properties"); props != c.end() && props->is_object()) {
                    const std::pair<const char*, const char*> chans[] = {{"rotate", "mixRotate"}, {"x", "mixX"}, {"y", "mixY"},
                                                                          {"scaleX", "mixScaleX"}, {"scaleY", "mixScaleY"}, {"shearY", "mixShearY"}};
                    for (auto& [ch, mix] : chans)
                        if (!props->contains(ch)) e[mix] = 0.0f;
                        else if (!e.contains(mix)) e[mix] = 1.0f;
                    e.erase("properties");
                    // Only same-channel copies are modelled; warn when a mapping uses anything else.
                    bool exotic = false;
                    for (auto& [from, spec] : props->items()) {
                        auto to = spec.find("to");
                        if (to == spec.end() || !to->is_object()) { exotic = true; continue; }
                        for (auto& [toName, toSpec] : to->items()) {
                            if (toName != from) exotic = true;
                            if (toSpec.is_object()) for (auto& [k, v] : toSpec.items()) if (k != "max") exotic = true;
                        }
                        for (auto& [k, v] : spec.items()) if (k != "to") exotic = true; // from-side offset/max/scale
                    }
                    if (exotic) res.warnings.push_back("Transform constraint '" + c.value("name", "") + "': 4.3 property mappings are read as same-channel copies");
                }
            }
            if (type == "ik" || type == "transform" || type == "path" || type == "physics") root[type].push_back(e);
            else res.warnings.push_back("Unsupported 4.3 constraint type '" + type + "' (" + c.value("name", "") + ") ignored");
        }
        root.erase("constraints");
    }
    out = SkeletonData();
    Reader r{out, res};
    try {
        r.read(root);
    } catch (const std::exception& e) {
        res.ok = false;
        res.error = std::string("Unexpected data: ") + e.what();
    }
    return res;
}

std::string readEditorBlock(const std::string& text) {
    try {
        json root = json::parse(text);
        auto it = root.find("osteon");
        if (it != root.end()) return it->dump();
    } catch (...) {}
    return {};
}

std::string writeSkeletonJson(const SkeletonData& d, const JsonWriteOptions& opt) {
    ordered_json root;
    ordered_json sk;
    if (!d.hash.empty()) sk["hash"] = d.hash;
    const bool v43 = opt.version.rfind("4.3", 0) == 0;
    sk["spine"] = opt.version;
    sk["x"] = d.x; sk["y"] = d.y; sk["width"] = d.width; sk["height"] = d.height;
    if (d.referenceScale != 100) sk["referenceScale"] = d.referenceScale;
    if (opt.nonessential) {
        sk["fps"] = d.fps;
        if (!d.imagesPath.empty()) sk["images"] = d.imagesPath;
        if (!d.audioPath.empty()) sk["audio"] = d.audioPath;
    }
    root["skeleton"] = sk;

    ordered_json bones = ordered_json::array();
    for (auto& b : d.bones) {
        ordered_json bj;
        bj["name"] = b.name;
        if (b.parent >= 0) bj["parent"] = d.bones[b.parent].name;
        if (b.length != 0) bj["length"] = b.length;
        if (b.x != 0) bj["x"] = b.x;
        if (b.y != 0) bj["y"] = b.y;
        if (b.rotation != 0) bj["rotation"] = b.rotation;
        if (b.scaleX != 1) bj["scaleX"] = b.scaleX;
        if (b.scaleY != 1) bj["scaleY"] = b.scaleY;
        if (b.shearX != 0) bj["shearX"] = b.shearX;
        if (b.shearY != 0) bj["shearY"] = b.shearY;
        if (b.inherit != Inherit::Normal) bj["inherit"] = inheritName(b.inherit);
        if (b.skinRequired) bj["skin"] = true;
        if (opt.nonessential) {
            if (b.color != Color(0x98 / 255.f, 0x98 / 255.f, 0x98 / 255.f, 1)) bj["color"] = fmtColor(b.color);
            if (!b.icon.empty()) bj["icon"] = b.icon;
            if (!b.visible) bj["visible"] = false;
            if (b.line) bj["line"] = true;
            if (b.thickness != 100) bj["thickness"] = b.thickness;
        }
        bones.push_back(bj);
    }
    root["bones"] = bones;

    ordered_json slots = ordered_json::array();
    for (auto& s : d.slots) {
        ordered_json sj;
        sj["name"] = s.name;
        sj["bone"] = d.bones[s.bone].name;
        if (s.color != Color()) sj["color"] = fmtColor(s.color);
        if (s.hasDark) sj["dark"] = fmtColor(s.dark, false);
        if (!s.attachment.empty()) sj["attachment"] = s.attachment;
        if (s.blend != BlendMode::Normal) sj["blend"] = blendName(s.blend);
        if (opt.nonessential && !s.visible) sj["visible"] = false;
        slots.push_back(sj);
    }
    root["slots"] = slots;

    auto names = [&](const std::vector<int>& v) {
        ordered_json a = ordered_json::array();
        for (int i : v) a.push_back(d.bones[i].name);
        return a;
    };
    if (!d.ik.empty()) {
        ordered_json arr = ordered_json::array();
        for (auto& c : d.ik) {
            ordered_json cj;
            cj["name"] = c.name;
            if (c.order) cj["order"] = c.order;
            if (c.skinRequired) cj["skin"] = true;
            cj["bones"] = names(c.bones);
            cj["target"] = d.bones[c.target].name;
            if (c.mix != 1) cj["mix"] = c.mix;
            if (c.softness != 0) cj["softness"] = c.softness;
            if (c.bendDirection < 0) cj["bendPositive"] = false;
            if (c.compress) cj["compress"] = true;
            if (c.stretch) cj["stretch"] = true;
            if (c.uniform) cj["uniform"] = true;
            arr.push_back(cj);
        }
        root["ik"] = arr;
    }
    if (!d.transforms.empty()) {
        ordered_json arr = ordered_json::array();
        for (auto& c : d.transforms) {
            ordered_json cj;
            cj["name"] = c.name;
            if (c.order) cj["order"] = c.order;
            if (c.skinRequired) cj["skin"] = true;
            cj["bones"] = names(c.bones);
            cj["target"] = d.bones[c.target].name;
            if (c.local) cj["local"] = true;
            if (c.relative) cj["relative"] = true;
            if (c.offsetRotation) cj["rotation"] = c.offsetRotation;
            if (c.offsetX) cj["x"] = c.offsetX;
            if (c.offsetY) cj["y"] = c.offsetY;
            if (c.offsetScaleX) cj["scaleX"] = c.offsetScaleX;
            if (c.offsetScaleY) cj["scaleY"] = c.offsetScaleY;
            if (c.offsetShearY) cj["shearY"] = c.offsetShearY;
            if (c.mixRotate != 1) cj["mixRotate"] = c.mixRotate;
            if (c.mixX != 1) cj["mixX"] = c.mixX;
            if (c.mixY != c.mixX) cj["mixY"] = c.mixY;
            if (c.mixScaleX != 1) cj["mixScaleX"] = c.mixScaleX;
            if (c.mixScaleY != c.mixScaleX) cj["mixScaleY"] = c.mixScaleY;
            if (c.mixShearY != 1) cj["mixShearY"] = c.mixShearY;
            arr.push_back(cj);
        }
        root["transform"] = arr;
    }
    if (!d.paths.empty()) {
        ordered_json arr = ordered_json::array();
        for (auto& c : d.paths) {
            ordered_json cj;
            cj["name"] = c.name;
            if (c.order) cj["order"] = c.order;
            if (c.skinRequired) cj["skin"] = true;
            cj["bones"] = names(c.bones);
            cj["target"] = d.slots[c.target].name;
            if (c.positionMode == PathConstraintData::Position::Fixed) cj["positionMode"] = "fixed";
            const char* sm[] = {"length", "fixed", "percent", "proportional"};
            if (c.spacingMode != PathConstraintData::Spacing::Length) cj["spacingMode"] = sm[(int)c.spacingMode];
            const char* rm[] = {"tangent", "chain", "chainScale"};
            if (c.rotateMode != PathConstraintData::Rotate::Tangent) cj["rotateMode"] = rm[(int)c.rotateMode];
            if (c.offsetRotation) cj["rotation"] = c.offsetRotation;
            if (c.position) cj["position"] = c.position;
            if (c.spacing) cj["spacing"] = c.spacing;
            if (c.mixRotate != 1) cj["mixRotate"] = c.mixRotate;
            if (c.mixX != 1) cj["mixX"] = c.mixX;
            if (c.mixY != c.mixX) cj["mixY"] = c.mixY;
            arr.push_back(cj);
        }
        root["path"] = arr;
    }
    if (!d.physics.empty()) {
        ordered_json arr = ordered_json::array();
        for (auto& c : d.physics) {
            ordered_json cj;
            cj["name"] = c.name;
            if (c.order) cj["order"] = c.order;
            if (c.skinRequired) cj["skin"] = true;
            cj["bone"] = d.bones[c.bone].name;
            if (c.x) cj["x"] = c.x;
            if (c.y) cj["y"] = c.y;
            if (c.rotate) cj["rotate"] = c.rotate;
            if (c.scaleX) cj["scaleX"] = c.scaleX;
            if (c.shearX) cj["shearX"] = c.shearX;
            if (c.limit != 5000) cj["limit"] = c.limit;
            if (std::fabs(1 / c.step - 60) > 1e-3) cj["fps"] = 1 / c.step;
            if (c.inertia != 1) cj["inertia"] = c.inertia;
            if (c.strength != 100) cj["strength"] = c.strength;
            if (c.damping != 1) cj["damping"] = c.damping;
            if (c.massInverse != 1) cj["mass"] = 1 / c.massInverse;
            if (c.wind) cj["wind"] = c.wind;
            if (c.gravity) cj["gravity"] = c.gravity;
            if (c.mix != 1) cj["mix"] = c.mix;
            if (c.inertiaGlobal) cj["inertiaGlobal"] = true;
            if (c.strengthGlobal) cj["strengthGlobal"] = true;
            if (c.dampingGlobal) cj["dampingGlobal"] = true;
            if (c.massGlobal) cj["massGlobal"] = true;
            if (c.windGlobal) cj["windGlobal"] = true;
            if (c.gravityGlobal) cj["gravityGlobal"] = true;
            if (c.mixGlobal) cj["mixGlobal"] = true;
            arr.push_back(cj);
        }
        root["physics"] = arr;
    }

    if (v43) {
        // 4.3 layout: one "constraints" array in evaluation order, each entry tagged with its type.
        struct Entry { int order; ordered_json j; };
        std::vector<Entry> all;
        for (const char* key : {"ik", "transform", "path", "physics"}) {
            if (!root.contains(key)) continue;
            for (auto& c : root[key]) {
                ordered_json e;
                e["type"] = key;
                for (auto& [k, v] : c.items()) {
                    if (k == "order") continue;
                    if (std::string(key) == "transform" && k == "target") e["source"] = v;
                    else e[k] = v;
                }
                if (std::string(key) == "transform") {
                    // Map each mixed channel from the source to the same channel, as Spine 4.3 exports do.
                    ordered_json props = ordered_json::object();
                    const std::pair<const char*, const char*> chans[] = {{"rotate", "mixRotate"}, {"x", "mixX"}, {"y", "mixY"},
                                                                          {"scaleX", "mixScaleX"}, {"scaleY", "mixScaleY"}, {"shearY", "mixShearY"}};
                    for (auto& [ch, mix] : chans) {
                        float m = c.contains(mix) ? c[mix].get<float>() : (std::string(mix) == "mixY" && c.contains("mixX") ? c["mixX"].get<float>()
                                  : std::string(mix) == "mixScaleY" && c.contains("mixScaleX") ? c["mixScaleX"].get<float>() : 1.0f);
                        if (m != 0) props[ch]["to"][ch]["max"] = 100;
                    }
                    e["properties"] = props;
                }
                all.push_back({c.value("order", 0), e});
            }
            root.erase(key);
        }
        std::stable_sort(all.begin(), all.end(), [](const Entry& a, const Entry& b) { return a.order < b.order; });
        if (!all.empty()) {
            ordered_json arr = ordered_json::array();
            for (auto& e : all) arr.push_back(e.j);
            root["constraints"] = arr;
        }
    }

    ordered_json skins = ordered_json::array();
    for (auto& s : d.skins) {
        ordered_json sj;
        sj["name"] = s.name;
        if (!s.bones.empty()) sj["bones"] = names(s.bones);
        ordered_json atts = ordered_json::object();
        for (auto& [slot, m] : s.attachments) {
            if (m.empty()) continue;
            ordered_json sm = ordered_json::object();
            for (auto& [name, a] : m) {
                if (opt.editorBlock.empty() && !a.exportable) continue; // Export unchecked: left out of exported data
                sm[name] = writeAttachment(a, opt.nonessential, !opt.editorBlock.empty());
            }
            atts[d.slots[slot].name] = sm;
        }
        sj["attachments"] = atts;
        skins.push_back(sj);
    }
    root["skins"] = skins;

    if (!d.events.empty()) {
        ordered_json ev = ordered_json::object();
        for (auto& e : d.events) {
            ordered_json ej = ordered_json::object();
            if (e.intValue) ej["int"] = e.intValue;
            if (e.floatValue) ej["float"] = e.floatValue;
            if (!e.stringValue.empty()) ej["string"] = e.stringValue;
            if (!e.audio.empty()) {
                ej["audio"] = e.audio;
                if (e.volume != 1) ej["volume"] = e.volume;
                if (e.balance) ej["balance"] = e.balance;
            }
            ev[e.name] = ej;
        }
        root["events"] = ev;
    }

    ordered_json anims = ordered_json::object();
    for (auto& a : d.animations) {
        ordered_json aj = ordered_json::object();
        ordered_json bonesJ = ordered_json::object(), slotsJ = ordered_json::object(), ikJ = ordered_json::object(),
                     tcJ = ordered_json::object(), pathJ = ordered_json::object(), physJ = ordered_json::object(), defJ = ordered_json::object();
        ordered_json drawOrder = ordered_json::array(), events = ordered_json::array();
        for (auto& tl : a.timelines) {
            if (tl.keys.empty()) continue;
            switch (tl.type) {
            case TimelineType::Rotate: bonesJ[d.bones[tl.target].name]["rotate"] = writeCurveKeys(tl, {{"value", 0}}); break;
            case TimelineType::Translate: bonesJ[d.bones[tl.target].name]["translate"] = writeCurveKeys(tl, {{"x", 0}, {"y", 0}}); break;
            case TimelineType::TranslateX: bonesJ[d.bones[tl.target].name]["translatex"] = writeCurveKeys(tl, {{"value", 0}}); break;
            case TimelineType::TranslateY: bonesJ[d.bones[tl.target].name]["translatey"] = writeCurveKeys(tl, {{"value", 0}}); break;
            case TimelineType::Scale: bonesJ[d.bones[tl.target].name]["scale"] = writeCurveKeys(tl, {{"x", 1}, {"y", 1}}); break;
            case TimelineType::ScaleX: bonesJ[d.bones[tl.target].name]["scalex"] = writeCurveKeys(tl, {{"value", 1}}); break;
            case TimelineType::ScaleY: bonesJ[d.bones[tl.target].name]["scaley"] = writeCurveKeys(tl, {{"value", 1}}); break;
            case TimelineType::Shear: bonesJ[d.bones[tl.target].name]["shear"] = writeCurveKeys(tl, {{"x", 0}, {"y", 0}}); break;
            case TimelineType::ShearX: bonesJ[d.bones[tl.target].name]["shearx"] = writeCurveKeys(tl, {{"value", 0}}); break;
            case TimelineType::ShearY: bonesJ[d.bones[tl.target].name]["sheary"] = writeCurveKeys(tl, {{"value", 0}}); break;
            case TimelineType::Inherit: {
                ordered_json arr = ordered_json::array();
                for (auto& k : tl.keys) { ordered_json kj; if (k.time) kj["time"] = k.time; kj["inherit"] = inheritName((Inherit)k.intValue); arr.push_back(kj); }
                bonesJ[d.bones[tl.target].name]["inherit"] = arr;
                break;
            }
            case TimelineType::Attachment: {
                ordered_json arr = ordered_json::array();
                for (auto& k : tl.keys) {
                    ordered_json kj = ordered_json::object();
                    if (k.time) kj["time"] = k.time;
                    kj["name"] = k.str.empty() ? ordered_json(nullptr) : ordered_json(k.str);
                    arr.push_back(kj);
                }
                slotsJ[d.slots[tl.target].name]["attachment"] = arr;
                break;
            }
            case TimelineType::RGBA: case TimelineType::RGB: case TimelineType::RGBA2: case TimelineType::RGB2:
                slotsJ[d.slots[tl.target].name][timelineName(tl.type)] = writeColorKeys(tl);
                break;
            case TimelineType::Alpha: slotsJ[d.slots[tl.target].name]["alpha"] = writeCurveKeys(tl, {{"value", 1}}); break;
            case TimelineType::Sequence: {
                ordered_json arr = ordered_json::array();
                for (auto& k : tl.keys) {
                    ordered_json kj; if (k.time) kj["time"] = k.time;
                    if (k.str != "hold") kj["mode"] = k.str;
                    if (k.intValue) kj["index"] = k.intValue;
                    if (k.v[0]) kj["delay"] = k.v[0];
                    arr.push_back(kj);
                }
                slotsJ[d.slots[tl.target].name]["sequence"] = arr;
                break;
            }
            case TimelineType::Deform: {
                ordered_json arr = ordered_json::array();
                // Weighted deform offsets follow the written (sorted) influence order.
                std::vector<int> order;
                int ski = d.findSkin(tl.skin);
                if (const Attachment* att = ski >= 0 ? d.skins[ski].get(tl.target, tl.attachment) : nullptr)
                    if (att->isWeighted()) order = influenceOrder(*att);
                for (size_t i = 0; i < tl.keys.size(); i++) {
                    Key k = tl.keys[i];
                    if (!order.empty() && k.deform.size() == order.size() * 2) {
                        std::vector<float> src = k.deform;
                        for (size_t q = 0; q < order.size(); q++) {
                            k.deform[q * 2] = src[order[q] * 2];
                            k.deform[q * 2 + 1] = src[order[q] * 2 + 1];
                        }
                    }
                    ordered_json kj = ordered_json::object();
                    if (k.time) kj["time"] = k.time;
                    // Trim zero runs at both ends, as Spine's exporter does.
                    int start = 0, end = (int)k.deform.size();
                    while (start < end && k.deform[start] == 0) start++;
                    while (end > start && k.deform[end - 1] == 0) end--;
                    if (end > start) {
                        if (start) kj["offset"] = start;
                        kj["vertices"] = std::vector<float>(k.deform.begin() + start, k.deform.begin() + end);
                    }
                    writeCurve(kj, k, i + 1 == tl.keys.size());
                    arr.push_back(kj);
                }
                defJ[tl.skin][d.slots[tl.target].name][tl.attachment]["deform"] = arr;
                break;
            }
            case TimelineType::DrawOrder:
                for (auto& k : tl.keys) {
                    ordered_json kj = ordered_json::object();
                    if (k.time) kj["time"] = k.time;
                    if (!k.drawOrder.empty()) {
                        int n = (int)k.drawOrder.size();
                        std::vector<int> pos(n);
                        for (int i = 0; i < n; i++) pos[k.drawOrder[i]] = i;
                        ordered_json offs = ordered_json::array();
                        for (int s = 0; s < n; s++)
                            if (pos[s] != s) offs.push_back({{"slot", d.slots[s].name}, {"offset", pos[s] - s}});
                        // Verify the sparse form reconstructs the order; otherwise list every slot.
                        std::vector<int> order(n, -1), unchanged;
                        int orig = 0;
                        for (auto& o : offs) {
                            int si = d.findSlot(o["slot"].get<std::string>());
                            while (orig != si) unchanged.push_back(orig++);
                            order[orig + o["offset"].get<int>()] = orig;
                            orig++;
                        }
                        while (orig < n) unchanged.push_back(orig++);
                        for (int i = n - 1; i >= 0; i--) if (order[i] == -1) { order[i] = unchanged.back(); unchanged.pop_back(); }
                        if (order != k.drawOrder) {
                            offs = ordered_json::array();
                            for (int s = 0; s < n; s++) offs.push_back({{"slot", d.slots[s].name}, {"offset", pos[s] - s}});
                        }
                        kj["offsets"] = offs;
                    }
                    drawOrder.push_back(kj);
                }
                break;
            case TimelineType::Event:
                for (auto& k : tl.keys) {
                    ordered_json kj = ordered_json::object();
                    if (k.time) kj["time"] = k.time;
                    kj["name"] = k.str;
                    if (k.flag1) kj["int"] = k.intValue;
                    if (k.flag2) kj["float"] = k.v[0];
                    if (k.flag3) kj["string"] = k.str2;
                    if (k.volume != 1) kj["volume"] = k.volume;
                    if (k.balance) kj["balance"] = k.balance;
                    events.push_back(kj);
                }
                break;
            case TimelineType::Ik: {
                ordered_json arr = writeCurveKeys(tl, {{"mix", 1}, {"softness", 0}});
                for (size_t i = 0; i < tl.keys.size(); i++) {
                    if (!tl.keys[i].flag1) arr[i]["bendPositive"] = false;
                    if (tl.keys[i].flag2) arr[i]["compress"] = true;
                    if (tl.keys[i].flag3) arr[i]["stretch"] = true;
                }
                ikJ[d.ik[tl.target].name] = arr;
                break;
            }
            case TimelineType::Transform:
                tcJ[d.transforms[tl.target].name] = writeCurveKeys(tl, {{"mixRotate", 1}, {"mixX", 1}, {"mixY", 1}, {"mixScaleX", 1}, {"mixScaleY", 1}, {"mixShearY", 1}});
                break;
            case TimelineType::Path: pathJ[d.paths[tl.target].name]["position"] = writeCurveKeys(tl, {{"value", 0}}); break;
            case TimelineType::PathMix: pathJ[d.paths[tl.target].name]["mix"] = writeCurveKeys(tl, {{"mixRotate", 1}, {"mixX", 1}, {"mixY", 1}}); break;
            default:
                if (isPhysicsTimeline(tl.type)) {
                    std::string cname = tl.target >= 0 && tl.target < (int)d.physics.size() ? d.physics[tl.target].name : std::string();
                    if (tl.target >= (int)d.physics.size()) break;
                    if (tl.type == TimelineType::PhysicsReset) {
                        ordered_json arr = ordered_json::array();
                        for (auto& k : tl.keys) { ordered_json kj = ordered_json::object(); if (k.time) kj["time"] = k.time; arr.push_back(kj); }
                        physJ[cname]["reset"] = arr;
                    } else {
                        float def = tl.type == TimelineType::PhysicsMix || tl.type == TimelineType::PhysicsInertia || tl.type == TimelineType::PhysicsDamping || tl.type == TimelineType::PhysicsMass ? 1 : 0;
                        physJ[cname][timelineName(tl.type) == std::string("physics mix") ? "mix" : timelineName(tl.type)] = writeCurveKeys(tl, {{"value", def}});
                    }
                }
                break;
            }
        }
        if (!physJ.empty()) aj["physics"] = physJ;
        if (!slotsJ.empty()) aj["slots"] = slotsJ;
        if (!bonesJ.empty()) aj["bones"] = bonesJ;
        if (!ikJ.empty()) aj["ik"] = ikJ;
        if (!tcJ.empty()) aj["transform"] = tcJ;
        if (!pathJ.empty()) aj["path"] = pathJ;
        if (!defJ.empty()) aj["attachments"] = defJ;
        if (!drawOrder.empty()) aj["drawOrder"] = drawOrder;
        if (!events.empty()) aj["events"] = events;
        anims[a.name] = aj;
    }
    root["animations"] = anims;

    if (!opt.editorBlock.empty()) {
        try { root["osteon"] = ordered_json::parse(opt.editorBlock); } catch (...) {}
    }
    return root.dump(opt.pretty ? 1 : -1, '\t');
}

} // namespace osteon
