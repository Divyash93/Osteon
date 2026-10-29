#include "Test.h"
#include "Skeleton.h"

using namespace osteon;

// Root + 3 bones (length 20) constrained to a straight vertical 2-curve path from (0,0) to (0,200).
static SkeletonData pathRig() {
    SkeletonData d;
    BoneData root; root.name = "root";
    d.bones.push_back(root);
    for (int i = 0; i < 3; i++) {
        BoneData b; b.name = "b" + std::to_string(i); b.parent = 0; b.x = 5; b.y = (float)i; b.length = 20;
        d.bones.push_back(b);
    }
    SlotData s; s.name = "path"; s.bone = 0; s.attachment = "p";
    d.slots.push_back(s);
    Attachment a; a.type = AttachmentType::Path; a.name = "p";
    float ys[] = {-100 / 3.0f, 0, 100 / 3.0f, 200 / 3.0f, 100, 400 / 3.0f, 500 / 3.0f, 200, 700 / 3.0f};
    for (float y : ys) { a.vertices.push_back(0); a.vertices.push_back(y); }
    a.worldVerticesLength = (int)a.vertices.size();
    a.lengths = {100, 200};
    Skin skin; skin.name = "default";
    skin.set(0, a);
    d.skins.push_back(skin);
    PathConstraintData pc; pc.name = "path"; pc.bones = {1, 2, 3}; pc.target = 0;
    pc.positionMode = PathConstraintData::Position::Fixed; pc.position = 10;
    pc.spacingMode = PathConstraintData::Spacing::Length; pc.spacing = 10;
    pc.rotateMode = PathConstraintData::Rotate::Tangent;
    d.paths.push_back(pc);
    return d;
}

TEST(path_length_spacing_along_tangent) {
    SkeletonData d = pathRig();
    Skeleton s(d);
    s.updateWorldTransform();
    for (int i = 0; i < 3; i++) {
        const Bone& b = s.bones[1 + i];
        CHECK_NEAR(b.worldX, 0, 1e-3);
        CHECK_NEAR(b.worldY, 10 + 30 * i, 1e-2);
        CHECK_NEAR(b.worldRotationX(), 90, 1e-2);
        CHECK_NEAR(b.worldScaleX(), 1, 1e-4);
    }
}

TEST(path_not_constant_speed_and_chain) {
    SkeletonData d = pathRig();
    d.skins[0].attachments[0]["p"].constantSpeed = false;
    d.paths[0].rotateMode = PathConstraintData::Rotate::Chain;
    Skeleton s(d);
    s.updateWorldTransform();
    CHECK_NEAR(s.bones[2].worldY, 40, 1e-2);
    CHECK_NEAR(s.bones[3].worldRotationX(), 90, 1e-2);
}

TEST(path_percent_position_midpoint) {
    SkeletonData d = pathRig();
    d.paths[0].positionMode = PathConstraintData::Position::Percent;
    d.paths[0].position = 0.5f;
    Skeleton s(d);
    s.updateWorldTransform();
    CHECK_NEAR(s.bones[1].worldX, 0, 1e-3);
    CHECK_NEAR(s.bones[1].worldY, 100, 1e-2);
}

TEST(path_proportional_spans_path) {
    SkeletonData d = pathRig();
    d.paths[0].positionMode = PathConstraintData::Position::Fixed; d.paths[0].position = 0;
    d.paths[0].spacingMode = PathConstraintData::Spacing::Proportional; d.paths[0].spacing = 1;
    d.paths[0].rotateMode = PathConstraintData::Rotate::ChainScale;
    Skeleton s(d);
    s.updateWorldTransform();
    // Three equal bones spread over 200 units: 66.7 apart, scaled to reach the next point.
    CHECK_NEAR(s.bones[2].worldY, 200 / 3.0, 1e-2);
    CHECK_NEAR(s.bones[3].worldScaleX(), (200 / 3.0) / 20, 1e-3);
}

TEST(path_mix_x_zero_keeps_x) {
    SkeletonData d = pathRig();
    d.paths[0].mixX = 0;
    Skeleton s(d);
    s.updateWorldTransform();
    CHECK_NEAR(s.bones[1].worldX, 5, 1e-4);
    CHECK_NEAR(s.bones[1].worldY, 10, 1e-2);
}

TEST(path_no_path_attachment_is_noop) {
    SkeletonData d = pathRig();
    d.slots[0].attachment = "";
    Skeleton s(d);
    s.updateWorldTransform();
    CHECK_NEAR(s.bones[2].worldX, 5, 1e-4);
    CHECK_NEAR(s.bones[2].worldY, 1, 1e-4);
}

// ---- Physics -------------------------------------------------------------------------------

static SkeletonData physicsRig() {
    SkeletonData d;
    BoneData root; root.name = "root";
    BoneData p; p.name = "parent"; p.parent = 0; p.length = 50;
    BoneData b; b.name = "tail"; b.parent = 1; b.x = 50; b.length = 40;
    d.bones = {root, p, b};
    PhysicsConstraintData pc; pc.name = "phys"; pc.bone = 2;
    pc.x = 1; pc.y = 1; pc.rotate = 1;
    pc.strength = 100; pc.damping = 0.9f; pc.inertia = 1;
    d.physics.push_back(pc);
    return d;
}

static void stepFrames(Skeleton& s, int frames) {
    for (int i = 0; i < frames; i++) { s.update(1.0f / 60); s.updateWorldTransform(Physics::Update); }
}

TEST(physics_static_parent_settles) {
    SkeletonData d = physicsRig();
    Skeleton s(d);
    s.updateWorldTransform(Physics::Reset);
    stepFrames(s, 300);
    CHECK_NEAR(s.bones[2].worldX, 50, 1e-3);
    CHECK_NEAR(s.bones[2].worldY, 0, 1e-3);
    CHECK_NEAR(s.bones[2].worldRotationX(), 0, 1e-3);
}

TEST(physics_moving_parent_lags_then_decays) {
    SkeletonData d = physicsRig();
    Skeleton s(d);
    s.updateWorldTransform(Physics::Reset);
    stepFrames(s, 10);
    s.bones[1].x = 30;
    stepFrames(s, 1);
    CHECK(std::fabs(s.physics[0].xOffset) > 1);
    CHECK(s.bones[2].worldX < 79); // lags behind the parent
    stepFrames(s, 600);
    CHECK_NEAR(s.physics[0].xOffset, 0, 1e-2);
    CHECK_NEAR(s.bones[2].worldX, 80, 1e-2);
    // Rotating the parent swings the tail.
    s.bones[1].rotation = 30;
    stepFrames(s, 1);
    CHECK(std::fabs(s.physics[0].rotateOffset) > 0.01f);
    stepFrames(s, 600);
    CHECK_NEAR(s.physics[0].rotateOffset, 0, 1e-3);
    CHECK_NEAR(s.bones[2].worldRotationX(), 30, 1e-2);
}

TEST(physics_gravity_pushes_down) {
    SkeletonData d = physicsRig();
    d.physics[0].gravity = 2;
    Skeleton s(d);
    s.updateWorldTransform(Physics::Reset);
    stepFrames(s, 600);
    CHECK(s.bones[2].worldY < -1);
    CHECK(s.bones[2].worldRotationX() < 0);
}

TEST(physics_none_leaves_pose) {
    SkeletonData d = physicsRig();
    Skeleton s(d);
    s.updateWorldTransform(Physics::Reset);
    stepFrames(s, 5);
    s.bones[1].x = 30;
    stepFrames(s, 2); // build up nonzero offsets
    CHECK(std::fabs(s.physics[0].xOffset) > 1);
    s.updateWorldTransform(Physics::None);
    CHECK_NEAR(s.bones[2].worldX, 80, 1e-4);
    CHECK_NEAR(s.bones[2].worldY, 0, 1e-4);
}

TEST(physics_mix_zero_no_effect) {
    SkeletonData d = physicsRig();
    d.physics[0].mix = 0;
    Skeleton s(d);
    s.updateWorldTransform(Physics::Reset);
    stepFrames(s, 5);
    s.bones[1].x = 30;
    s.bones[1].rotation = 20;
    stepFrames(s, 1);
    Vec2 expect = s.bones[1].localToWorld({50, 0});
    CHECK_NEAR(s.bones[2].worldX, expect.x, 1e-4);
    CHECK_NEAR(s.bones[2].worldY, expect.y, 1e-4);
    CHECK_NEAR(s.bones[2].worldRotationX(), 20, 1e-3);
}
