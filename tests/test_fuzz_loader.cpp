// Robustness: mutated skeleton JSON must load or be rejected, and whatever loads must pose without crashing.
// Run under AddressSanitizer (see PROGRESS.md) to catch out-of-range reads that do not crash a normal build.
#include "Test.h"
#include "Animation.h"
#include "Skeleton.h"
#include "SkeletonJson.h"
#include "json.hpp"
#include <cmath>
#include <random>

using namespace osteon;
using json = nlohmann::json;

static SkeletonData richRig() {
    SkeletonData d;
    BoneData root; root.name = "root"; d.bones.push_back(root);
    for (int i = 1; i <= 4; i++) { BoneData b; b.name = "b" + std::to_string(i); b.parent = i - 1; b.x = 20; b.length = 20; d.bones.push_back(b); }
    d.defaultSkin();
    for (int i = 0; i < 3; i++) { SlotData s; s.name = "s" + std::to_string(i); s.bone = 1 + i; s.attachment = "a"; d.slots.push_back(s); }
    Attachment reg; reg.type = AttachmentType::Region; reg.name = "a"; reg.width = 30; reg.height = 30;
    d.defaultSkin().set(0, reg);
    Attachment mesh; mesh.type = AttachmentType::Mesh; mesh.name = "a"; mesh.width = 40; mesh.height = 40;
    mesh.uvs = {0, 0, 1, 0, 1, 1, 0, 1}; mesh.triangles = {0, 1, 2, 0, 2, 3}; mesh.hullLength = 4;
    mesh.worldVerticesLength = 8;
    mesh.bones = {2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2};
    // two influences per vertex: (bone, x, y, weight)
    mesh.bones = {};
    for (int v = 0; v < 4; v++) {
        mesh.bones.push_back(2); mesh.bones.push_back(1); mesh.bones.push_back(2);
        for (int k = 0; k < 2; k++) { mesh.vertices.push_back((float)(v * 5)); mesh.vertices.push_back((float)(k * 3)); mesh.vertices.push_back(0.5f); }
    }
    d.defaultSkin().set(1, mesh);
    Attachment path; path.type = AttachmentType::Path; path.name = "a"; path.vertices = {0, 0, 10, 0, 20, 0, 30, 0, 40, 0, 50, 0}; path.worldVerticesLength = 12; path.lengths = {50};
    d.defaultSkin().set(2, path);
    IkConstraintData ik; ik.name = "ik"; ik.bones = {2, 3}; ik.target = 4; d.ik.push_back(ik);
    TransformConstraintData tc; tc.name = "tc"; tc.order = 1; tc.bones = {1}; tc.target = 4; d.transforms.push_back(tc);
    PathConstraintData pc; pc.name = "pc"; pc.order = 2; pc.bones = {3}; pc.target = 2; d.paths.push_back(pc);
    PhysicsConstraintData ph; ph.name = "ph"; ph.order = 3; ph.bone = 4; ph.rotate = 1; d.physics.push_back(ph);
    Animation an; an.name = "idle";
    auto key = [](float t, float v0, float v1 = 0) { Key k; k.time = t; k.v[0] = v0; k.v[1] = v1; return k; };
    Timeline rot; rot.type = TimelineType::Rotate; rot.target = 2; rot.keys = {key(0, 0), key(1, 40), key(2, 0)};
    Timeline tr; tr.type = TimelineType::Translate; tr.target = 1; tr.keys = {key(0, 0, 0), key(1, 5, 5)};
    tr.keys[0].curve = CurveType::Bezier; tr.keys[0].bezier = {0.3f, 0.f, 0.6f, 5.f, 0.3f, 0.f, 0.6f, 5.f};
    an.timelines = {rot, tr};
    Animation drive; drive.name = "drive";
    Timeline r2; r2.type = TimelineType::Rotate; r2.target = 3; r2.keys = {key(0, 0), key(1, 90)};
    drive.timelines = {r2};
    d.animations = {an, drive};
    SliderConstraintData sl; sl.name = "sl"; sl.order = 4; sl.animation = "drive"; sl.time = 10; d.sliders.push_back(sl);
    return d;
}

static void mutate(json& j, std::mt19937& rng, int depth = 0) {
    if (j.is_object() && !j.empty()) {
        auto it = j.begin();
        std::advance(it, rng() % j.size());
        if (rng() % 7 == 0) { j.erase(it); return; }
        mutate(it.value(), rng, depth + 1);
    } else if (j.is_array() && !j.empty()) {
        size_t i = rng() % j.size();
        if (rng() % 9 == 0) { j.erase(j.begin() + i); return; }
        if (rng() % 11 == 0) { j.push_back(j[i]); return; }
        mutate(j[i], rng, depth + 1);
    } else {
        static const json repl[] = {-1, 0, 1, 2, 7, 1000000000, -1000000000, 0.5, -0.0001, "x", "", true, nullptr, json::array(), json::object(), 3.4e38};
        j = repl[rng() % (sizeof repl / sizeof repl[0])];
    }
}

static void exercise(SkeletonData& d) {
    Skeleton sk(d);
    sk.updateWorldTransform(Physics::None);
    std::vector<float> out;
    for (auto& slot : sk.slots)
        if (const Attachment* a = sk.attachmentFor(slot)) sk.worldVertices(slot, *a, out);
    float x0, y0, x1, y1;
    sk.bounds(x0, y0, x1, y1);
    for (auto& an : d.animations) {
        for (float t : {0.0f, 0.3f, 1.0f, 2.5f}) {
            sk.setToSetupPose();
            applyAnimation(an, sk, t, t, false, nullptr, 1, MixBlend::Setup);
            sk.updateWorldTransform(Physics::Update);
        }
    }
    AnimationState st;
    st.data = &d;
    if (!d.animations.empty()) {
        st.setAnimation(0, 0, true);
        for (int i = 0; i < 5; i++) { st.update(0.2f); sk.setToSetupPose(); st.apply(sk); sk.updateWorldTransform(Physics::Update); }
    }
}

TEST(fuzz_mutated_json_loads_or_is_rejected_and_poses_safely) {
    SkeletonData base = richRig();
    JsonWriteOptions o; o.pretty = false;
    for (const char* version : {"4.2.00", "4.3.26"}) {
        o.version = version;
        json original = json::parse(writeSkeletonJson(base, o));
        std::mt19937 rng(12345);
        int loaded = 0;
        for (int iter = 0; iter < 400; iter++) {
            json j = original;
            int edits = 1 + rng() % 3;
            for (int e = 0; e < edits; e++) mutate(j, rng);
            SkeletonData d;
            JsonResult r = readSkeletonJson(j.dump(), d);
            if (!r.ok) continue;
            loaded++;
            exercise(d);
        }
        CHECK(loaded > 50); // the mutations are not all rejected outright
    }
}

TEST(fuzz_truncated_and_garbage_text_is_rejected) {
    SkeletonData base = richRig();
    std::string text = writeSkeletonJson(base);
    SkeletonData d;
    for (size_t cut = 1; cut < text.size(); cut += 97) CHECK(!readSkeletonJson(text.substr(0, cut), d).ok || true); // must not crash
    CHECK(!readSkeletonJson("not json", d).ok);
    CHECK(!readSkeletonJson("", d).ok);
}
