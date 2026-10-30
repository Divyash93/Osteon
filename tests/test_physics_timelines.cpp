// Physics timelines: inertia, strength, damping, mass, wind, gravity, mix and reset, per constraint or global.
#include "Test.h"
#include "Animation.h"
#include "SkeletonJson.h"

using namespace osteon;

static SkeletonData physicsRig() {
    SkeletonData d;
    BoneData root; root.name = "root";
    BoneData tail; tail.name = "tail"; tail.parent = 0; tail.length = 40;
    d.bones = {root, tail};
    PhysicsConstraintData a; a.name = "a"; a.bone = 1; a.rotate = 1;
    PhysicsConstraintData b = a; b.name = "b";
    d.physics = {a, b};
    Skin def; def.name = "default"; d.skins.push_back(def); // JSON always has a default skin
    return d;
}

TEST(physics_timeline_drives_one_constraint) {
    SkeletonData d = physicsRig();
    Animation an; an.name = "x";
    Timeline& tl = an.get(TimelineType::PhysicsMix, 0);
    tl.setKey(0).v[0] = 1;
    tl.setKey(1).v[0] = 0;
    d.animations.push_back(an);
    Skeleton s(d);
    applyAnimation(d.animations[0], s, 0, 0.5f, false, nullptr, 1, MixBlend::Setup);
    CHECK_NEAR(s.physics[0].mix, 0.5f, 1e-4);
    CHECK_NEAR(s.physics[1].mix, 1.0f, 1e-4); // the other constraint is untouched
}

TEST(physics_mass_timeline_sets_the_inverse) {
    SkeletonData d = physicsRig();
    Animation an; an.name = "x";
    an.get(TimelineType::PhysicsMass, 0).setKey(0).v[0] = 4;
    d.animations.push_back(an);
    Skeleton s(d);
    applyAnimation(d.animations[0], s, 0, 0, false, nullptr, 1, MixBlend::Setup);
    CHECK_NEAR(s.physics[0].massInverse, 0.25f, 1e-5);
}

TEST(physics_global_row_drives_only_constraints_that_opt_in) {
    SkeletonData d = physicsRig();
    d.physics[1].windGlobal = true;
    Animation an; an.name = "x";
    an.get(TimelineType::PhysicsWind, -1).setKey(0).v[0] = 30;     // the global row
    an.get(TimelineType::PhysicsWind, 0).setKey(0).v[0] = 7;       // a's own row
    an.get(TimelineType::PhysicsWind, 1).setKey(0).v[0] = 99;      // b is global: its own row is ignored
    d.animations.push_back(an);
    Skeleton s(d);
    applyAnimation(d.animations[0], s, 0, 0, false, nullptr, 1, MixBlend::Setup);
    CHECK_NEAR(s.physics[0].wind, 7, 1e-5);
    CHECK_NEAR(s.physics[1].wind, 30, 1e-5);
}

TEST(physics_reset_key_restarts_the_simulation_once) {
    SkeletonData d = physicsRig();
    Animation an; an.name = "x";
    an.get(TimelineType::PhysicsReset, 0).setKey(0.5f);
    d.animations.push_back(an);
    Skeleton s(d);
    s.physics[0].reset = false;
    s.physics[1].reset = false;
    applyAnimation(d.animations[0], s, 0.2f, 0.4f, false, nullptr, 1, MixBlend::Setup); // before the key
    CHECK(!s.physics[0].reset);
    applyAnimation(d.animations[0], s, 0.4f, 0.6f, false, nullptr, 1, MixBlend::Setup); // passes it
    CHECK(s.physics[0].reset);
    CHECK(!s.physics[1].reset);
    s.physics[0].reset = false;
    applyAnimation(d.animations[0], s, 0.6f, 0.8f, false, nullptr, 1, MixBlend::Setup); // after it
    CHECK(!s.physics[0].reset);
    // A global reset key restarts every constraint.
    d.animations[0].timelines.clear();
    d.animations[0].get(TimelineType::PhysicsReset, -1).setKey(0.5f);
    applyAnimation(d.animations[0], s, 0.4f, 0.6f, false, nullptr, 1, MixBlend::Setup);
    CHECK(s.physics[0].reset && s.physics[1].reset);
}

TEST(physics_timelines_and_global_flags_round_trip_through_json) {
    SkeletonData d = physicsRig();
    d.physics[1].mixGlobal = true;
    d.physics[0].inertiaGlobal = false;
    Animation an; an.name = "x";
    Timeline& in = an.get(TimelineType::PhysicsInertia, 0);
    in.setKey(0).v[0] = 1;
    in.setKey(1).v[0] = 0.25f;
    an.get(TimelineType::PhysicsMix, -1).setKey(0.5f).v[0] = 0.5f;
    an.get(TimelineType::PhysicsReset, 1).setKey(0.75f);
    d.animations.push_back(an);
    std::string text = writeSkeletonJson(d);
    SkeletonData r;
    JsonResult res = readSkeletonJson(text, r);
    CHECK(res.ok);
    CHECK(r.physics.size() == 2 && r.physics[1].mixGlobal && !r.physics[0].mixGlobal);
    CHECK(r.animations.size() == 1);
    int found = 0;
    for (auto& tl : r.animations[0].timelines) {
        if (tl.type == TimelineType::PhysicsInertia && tl.target == 0) { found++; CHECK(tl.keys.size() == 2); CHECK_NEAR(tl.keys[1].v[0], 0.25f, 1e-5); }
        if (tl.type == TimelineType::PhysicsMix && tl.target == -1) { found++; CHECK_NEAR(tl.keys[0].v[0], 0.5f, 1e-5); }
        if (tl.type == TimelineType::PhysicsReset && tl.target == 1) { found++; CHECK_NEAR(tl.keys[0].time, 0.75f, 1e-5); }
    }
    CHECK(found == 3);
    // Writing it again keeps the same timelines (the row order in the file may differ).
    SkeletonData r2;
    CHECK(readSkeletonJson(writeSkeletonJson(r), r2).ok && r2.animations[0].timelines.size() == r.animations[0].timelines.size());
}
