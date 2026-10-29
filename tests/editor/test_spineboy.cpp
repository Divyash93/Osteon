// Checks against Esoteric's public Spineboy example (4.3 JSON), copied to samples/spineboy_test by hand.
// The tests skip when the files are missing.
#include "../Test.h"
#include "Document.h"
#include "Geometry.h"
#include "Gfx.h"
#include "Util.h"
#include <cmath>

using namespace osteon;

static const char* kDir = "C:/Users/Div/Desktop/Spine2d/samples/spineboy_test";

static bool openSpineboy(Document& doc) {
    std::string json = joinPath(kDir, "spineboy-pro.json");
    if (!fileExists(json)) { std::printf("    (skipped: %s not found)\n", json.c_str()); return false; }
    doc.newProject();
    std::string err;
    if (!doc.openFile(json, err)) { std::printf("    open failed: %s\n", err.c_str()); return false; }
    return true;
}

static bool finiteBone(const Bone& b) {
    return std::isfinite(b.worldX) && std::isfinite(b.worldY) && std::isfinite(b.a) && std::isfinite(b.b) && std::isfinite(b.c) && std::isfinite(b.d);
}

TEST(spineboy_loads_and_every_animation_evaluates) {
    Document doc;
    if (!openSpineboy(doc)) return;
    CHECK(doc.data.bones.size() == 67);
    CHECK(doc.data.slots.size() == 52);
    CHECK(doc.data.animations.size() == 11);
    Skeleton sk(doc.data);
    std::vector<float> v;
    int nonFinite = 0, poses = 0;
    for (auto& an : doc.data.animations) {
        float dur = an.duration();
        for (int i = 0; i <= 20; i++) {
            float t = dur * i / 20;
            sk.setToSetupPose();
            applyAnimation(an, sk, t, t, false, nullptr, 1, MixBlend::Setup);
            sk.updateWorldTransform(Physics::Pose);
            poses++;
            for (auto& b : sk.bones) if (!finiteBone(b)) nonFinite++;
            for (auto& s : sk.slots) {
                const Attachment* a = sk.attachmentFor(s);
                if (a && sk.worldVertices(s, *a, v))
                    for (float f : v) if (!std::isfinite(f)) nonFinite++;
            }
        }
    }
    CHECK(poses == 11 * 21);
    CHECK(nonFinite == 0);
}

TEST(spineboy_run_cycle_moves_the_feet) {
    Document doc;
    if (!openSpineboy(doc)) return;
    int run = doc.data.findAnimation("run");
    int foot = doc.data.findBone("rear-foot-target");
    CHECK(run >= 0 && foot >= 0);
    if (run < 0 || foot < 0) return;
    Skeleton sk(doc.data);
    float minX = 1e9f, maxX = -1e9f;
    const Animation& an = doc.data.animations[run];
    for (int i = 0; i <= 20; i++) {
        float t = an.duration() * i / 20;
        sk.setToSetupPose();
        applyAnimation(an, sk, t, t, false, nullptr, 1, MixBlend::Setup);
        sk.updateWorldTransform(Physics::Pose);
        minX = std::min(minX, sk.bones[foot].worldX);
        maxX = std::max(maxX, sk.bones[foot].worldX);
    }
    CHECK(maxX - minX > 50); // the foot target swings through a stride
}

TEST(spineboy_head_mesh_unbind_keeps_shape_then_trace_and_rebind) {
    Document doc;
    if (!openSpineboy(doc)) return;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int si = doc.data.findSlot("head");
    CHECK(si >= 0);
    if (si < 0) return;
    Attachment* a = doc.data.skins[0].get(si, "head");
    CHECK(a && a->type == AttachmentType::Mesh && a->isWeighted());
    if (!a) return;
    doc.updatePose();
    std::vector<float> before, after;
    CHECK(doc.sk.worldVertices(doc.sk.slots[si], *a, before));
    int bone = doc.data.slots[si].bone;
    unbindWeights(*a, doc.sk, bone);
    CHECK(!a->isWeighted());
    doc.updatePose();
    CHECK(doc.sk.worldVertices(doc.sk.slots[si], *a, after));
    CHECK(after.size() == before.size());
    for (size_t i = 0; i < before.size() && i < after.size(); i++) CHECK_NEAR(after[i], before[i], 0.05);

    // Trace the head image: the hull must enclose (about) all opaque pixels.
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    std::string file = joinPath(kDir, "images/head.png");
    CHECK(loadImageFile(file, w, h, rgba));
    std::vector<float> hull = traceHull(rgba.data(), w, h, 20, 24, 2);
    CHECK(hull.size() >= 6 && hull.size() <= 24 * 2);
    long opaque = 0;
    for (size_t i = 3; i < rgba.size(); i += 4) if (rgba[i] > 20) opaque++;
    CHECK(std::fabs(polygonArea(hull)) >= opaque * 0.9);
    CHECK(std::fabs(polygonArea(hull)) <= (double)w * h);
    std::vector<int> tris = triangulateMesh(hull, (int)hull.size() / 2);
    CHECK(tris.size() == ((hull.size() / 2) - 2) * 3);

    // Rebind to the original head + hair bones: weights sum to 1 and the shape is preserved.
    std::vector<int> bones;
    for (const char* n : {"head", "hair1", "hair2", "hair3", "hair4"}) bones.push_back(doc.data.findBone(n));
    bindWeights(*a, doc.sk, bone, bones);
    CHECK(a->isWeighted());
    doc.updatePose();
    std::vector<float> rebound;
    CHECK(doc.sk.worldVertices(doc.sk.slots[si], *a, rebound));
    for (size_t i = 0; i < before.size() && i < rebound.size(); i++) CHECK_NEAR(rebound[i], before[i], 0.05);
    size_t vi = 0;
    for (size_t i = 0; i < a->bones.size();) {
        int n = a->bones[i];
        float sum = 0;
        for (int k = 0; k < n; k++, vi += 3) sum += a->vertices[vi + 2];
        CHECK_NEAR(sum, 1.0, 1e-3);
        i += n + 1;
    }
}

TEST(spineboy_hair_physics_lags_then_settles) {
    Document doc;
    if (!openSpineboy(doc)) return;
    int hair = doc.data.findBone("hair2");
    CHECK(hair >= 0);
    if (hair < 0) return;
    PhysicsConstraintData pc;
    pc.name = "hair-physics";
    pc.bone = hair;
    pc.order = 1000; // physics runs after the other constraints, as the editor orders new constraints
    pc.rotate = 1;
    pc.x = 0;
    pc.y = 0;
    pc.inertia = 1;
    pc.strength = 100;
    pc.damping = 0.9f;
    pc.massInverse = 1;
    pc.mix = 1;
    doc.data.physics.push_back(pc);
    Skeleton sk(doc.data);
    sk.setToSetupPose();
    sk.updateWorldTransform(Physics::Reset);
    float rest = sk.bones[hair].worldRotationX();
    // Move the whole skeleton sideways for one second, then hold still for four.
    float maxDev = 0;
    bool finite = true;
    const float dt = 1.0f / 60;
    for (int i = 0; i < 300; i++) {
        float move = i < 60 ? 300.0f * std::sin(i / 60.0f * 3.14159f) : 0; // a diagonal move: some of it is sideways to any bone
        sk.bones[0].x = move;
        sk.bones[0].y = move;
        sk.update(dt);
        sk.updateWorldTransform(Physics::Update);
        float dev = std::fabs(wrapDeg(sk.bones[hair].worldRotationX() - rest));
        finite &= std::isfinite(dev);
        maxDev = std::max(maxDev, dev);
    }
    float end = std::fabs(wrapDeg(sk.bones[hair].worldRotationX() - rest));
    CHECK(finite);
    CHECK(maxDev > 0.01f);  // the hair responds to the motion
    CHECK(maxDev <= 180.0f); // and stays bounded (a violent move can swing a short hair bone a long way)
    CHECK(end < 1.0f);      // and comes back to rest
}
