// Physics constraint editing: keys, Reset, and the Simulate / Deterministic rules.
#include "../Test.h"
#include "Document.h"

using namespace osteon;

static Document physicsDoc() {
    Document doc;
    doc.newProject();
    BoneData tail; tail.name = "tail"; tail.parent = 0; tail.length = 40;
    doc.data.bones.push_back(tail);
    doc.updatePose();
    doc.createPhysics("p", 1);
    doc.createAnimation("a");
    doc.animation = 0;
    doc.setMode(Mode::Animate);
    return doc;
}

TEST(physics_key_property_and_detect_it) {
    Document doc = physicsDoc();
    doc.setTime(0.5f);
    CHECK(!doc.hasPhysicsKey(TimelineType::PhysicsMix, 0));
    doc.keyPhysics(TimelineType::PhysicsMix, 0, 0.25f);
    CHECK(doc.hasPhysicsKey(TimelineType::PhysicsMix, 0));
    CHECK(!doc.hasPhysicsKey(TimelineType::PhysicsMix, -1));
    doc.updatePose();
    CHECK_NEAR(doc.sk.physics[0].mix, 0.25f, 1e-4);
}

TEST(physics_reset_is_keyed_with_auto_key_only) {
    Document doc = physicsDoc();
    doc.setTime(1.0f);
    doc.autoKey = false;
    doc.resetPhysics(0);
    CHECK(!doc.hasPhysicsKey(TimelineType::PhysicsReset, 0));
    doc.autoKey = true;
    doc.resetPhysics(0);
    CHECK(doc.hasPhysicsKey(TimelineType::PhysicsReset, 0));
    doc.resetPhysics(-1); // Reset All keys the global row
    CHECK(doc.hasPhysicsKey(TimelineType::PhysicsReset, -1));
}

TEST(physics_deterministic_scrub_gives_the_same_pose_every_time) {
    Document doc = physicsDoc();
    doc.data.physics[0].rotate = 1;
    doc.data.physics[0].strength = 20;
    Timeline& tl = doc.data.animations[0].get(TimelineType::Rotate, 0);
    tl.setKey(0).v[0] = 0;
    tl.setKey(0.5f).v[0] = 60;
    tl.setKey(1.0f).v[0] = 0;
    doc.deterministic = true;
    doc.setTime(0.8f);
    doc.updatePose();
    float first = doc.sk.bones[1].rotation;
    doc.setTime(0.2f);
    doc.updatePose();
    doc.setTime(0.8f);
    doc.updatePose();
    CHECK_NEAR(doc.sk.bones[1].rotation, first, 1e-3);
}

TEST(physics_delete_drops_its_timelines) {
    Document doc = physicsDoc();
    doc.keyPhysics(TimelineType::PhysicsMix, 0, 0.5f);
    doc.keyPhysics(TimelineType::PhysicsMix, -1, 0.5f);
    doc.selection = {{SelType::Physics, 0, ""}};
    doc.deleteSelection();
    CHECK(doc.data.physics.empty());
    int left = 0;
    for (auto& tl : doc.data.animations[0].timelines) left++;
    CHECK(left == 1); // the global row stays
}
