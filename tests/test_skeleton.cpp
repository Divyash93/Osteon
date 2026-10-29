#include "Test.h"
#include "Skeleton.h"

using namespace osteon;

static SkeletonData chain() {
    SkeletonData d;
    BoneData root; root.name = "root";
    BoneData a; a.name = "a"; a.parent = 0; a.x = 10; a.rotation = 90; a.length = 50;
    BoneData b; b.name = "b"; b.parent = 1; b.x = 50; b.length = 30;
    d.bones = {root, a, b};
    return d;
}

TEST(world_transform_chain) {
    SkeletonData d = chain();
    Skeleton s(d);
    s.updateWorldTransform();
    CHECK_NEAR(s.bones[1].worldX, 10, 1e-4);
    CHECK_NEAR(s.bones[1].worldY, 0, 1e-4);
    // b is 50 along a's x-axis, which points up.
    CHECK_NEAR(s.bones[2].worldX, 10, 1e-4);
    CHECK_NEAR(s.bones[2].worldY, 50, 1e-4);
    CHECK_NEAR(s.bones[2].worldRotationX(), 90, 1e-3);
}

TEST(world_transform_scale_and_skeleton_position) {
    SkeletonData d = chain();
    d.bones[1].scaleX = 2;
    Skeleton s(d);
    s.x = 100; s.y = -20;
    s.updateWorldTransform();
    CHECK_NEAR(s.bones[2].worldX, 110, 1e-3);
    CHECK_NEAR(s.bones[2].worldY, 80, 1e-3); // 50 * scaleX 2 along +y, offset -20
}

TEST(inherit_only_translation_ignores_parent_rotation) {
    SkeletonData d = chain();
    d.bones[2].inherit = Inherit::OnlyTranslation;
    Skeleton s(d);
    s.updateWorldTransform();
    CHECK_NEAR(s.bones[2].worldRotationX(), 0, 1e-3);
}

TEST(inherit_no_scale_ignores_parent_scale) {
    SkeletonData d = chain();
    d.bones[1].scaleX = 3; d.bones[1].scaleY = 3;
    d.bones[2].inherit = Inherit::NoScale;
    Skeleton s(d);
    s.updateWorldTransform();
    CHECK_NEAR(s.bones[2].worldScaleX(), 1, 1e-4);
    CHECK_NEAR(s.bones[2].worldRotationX(), 90, 1e-3);
}

TEST(applied_transform_roundtrip_all_inherit_modes) {
    Inherit modes[] = {Inherit::Normal, Inherit::OnlyTranslation, Inherit::NoRotationOrReflection, Inherit::NoScale, Inherit::NoScaleOrReflection};
    for (Inherit m : modes) {
        SkeletonData d = chain();
        d.bones[1].rotation = 30; d.bones[1].scaleX = 1.5f; d.bones[1].scaleY = 0.8f;
        d.bones[2].rotation = 25; d.bones[2].scaleX = 1.2f; d.bones[2].scaleY = 0.7f; d.bones[2].shearY = 10;
        d.bones[2].x = 12; d.bones[2].y = -4;
        d.bones[2].inherit = m;
        Skeleton s(d);
        s.updateWorldTransform();
        Bone before = s.bones[2];
        s.bones[2].updateAppliedTransform(s);
        Bone& b = s.bones[2];
        b.updateWorldTransform(s, b.ax, b.ay, b.arotation, b.ascaleX, b.ascaleY, b.ashearX, b.ashearY);
        CHECK_NEAR(b.a, before.a, 1e-3); CHECK_NEAR(b.b, before.b, 1e-3);
        CHECK_NEAR(b.c, before.c, 1e-3); CHECK_NEAR(b.d, before.d, 1e-3);
        CHECK_NEAR(b.worldX, before.worldX, 1e-3); CHECK_NEAR(b.worldY, before.worldY, 1e-3);
    }
}

TEST(world_to_local_inverse) {
    SkeletonData d = chain();
    d.bones[1].scaleY = 2; d.bones[1].shearX = 15;
    Skeleton s(d);
    s.updateWorldTransform();
    Vec2 w = s.bones[1].localToWorld({7, -3});
    Vec2 l = s.bones[1].worldToLocal(w);
    CHECK_NEAR(l.x, 7, 1e-4); CHECK_NEAR(l.y, -3, 1e-4);
}

TEST(ik_one_bone_points_at_target) {
    SkeletonData d = chain();
    BoneData t; t.name = "target"; t.parent = 0; t.x = 100; t.y = 100;
    d.bones.push_back(t);
    IkConstraintData ik; ik.name = "ik"; ik.bones = {1}; ik.target = 3;
    d.ik.push_back(ik);
    Skeleton s(d);
    s.updateWorldTransform();
    float dx = 100 - s.bones[1].worldX, dy = 100 - s.bones[1].worldY;
    CHECK_NEAR(s.bones[1].worldRotationX(), atan2Deg(dy, dx), 1e-2);
}

TEST(ik_two_bone_tip_reaches_target) {
    SkeletonData d = chain();
    BoneData t; t.name = "target"; t.parent = 0; t.x = 40; t.y = 50;
    d.bones.push_back(t);
    IkConstraintData ik; ik.name = "ik"; ik.bones = {1, 2}; ik.target = 3;
    d.ik.push_back(ik);
    for (int bend : {1, -1}) {
        d.ik[0].bendDirection = bend;
        Skeleton s(d);
        s.updateWorldTransform();
        Vec2 tip = s.bones[2].localToWorld({30, 0});
        CHECK_NEAR(tip.x, 40, 0.05);
        CHECK_NEAR(tip.y, 50, 0.05);
    }
}

TEST(ik_mix_zero_has_no_effect) {
    SkeletonData d = chain();
    BoneData t; t.name = "target"; t.parent = 0; t.x = 40; t.y = -50;
    d.bones.push_back(t);
    IkConstraintData ik; ik.name = "ik"; ik.bones = {1, 2}; ik.target = 3; ik.mix = 0;
    d.ik.push_back(ik);
    Skeleton s(d);
    s.updateWorldTransform();
    CHECK_NEAR(s.bones[1].worldRotationX(), 90, 1e-3);
}

TEST(transform_constraint_copies_world_rotation) {
    SkeletonData d = chain();
    BoneData t; t.name = "t"; t.parent = 0; t.rotation = 45;
    d.bones.push_back(t);
    TransformConstraintData tc; tc.name = "tc"; tc.bones = {2}; tc.target = 3;
    tc.mixX = tc.mixY = tc.mixScaleX = tc.mixScaleY = tc.mixShearY = 0;
    d.transforms.push_back(tc);
    Skeleton s(d);
    s.updateWorldTransform();
    CHECK_NEAR(s.bones[2].worldRotationX(), 45, 1e-3);
    CHECK_NEAR(s.bones[2].arotation, -45, 1e-3); // applied pose updated relative to parent (90)
}

TEST(update_cache_constraint_order) {
    SkeletonData d = chain();
    BoneData t; t.name = "target"; t.parent = 2; // target depends on constrained chain
    d.bones.push_back(t);
    IkConstraintData ik; ik.name = "ik"; ik.bones = {1}; ik.target = 0;
    d.ik.push_back(ik);
    Skeleton s(d);
    // Every bone appears once, and bone 2 (child of constrained bone) comes after the constraint.
    int ikPos = -1, b2Pos = -1, count = 0;
    for (size_t i = 0; i < s.updateCache.size(); i++) {
        auto& c = s.updateCache[i];
        if (c.type == Skeleton::CacheType::Ik) ikPos = (int)i;
        if (c.type == Skeleton::CacheType::Bone) { count++; if (c.index == 2) b2Pos = (int)i; }
    }
    CHECK(ikPos >= 0);
    CHECK(b2Pos > ikPos);
    CHECK(count >= 4);
}

TEST(remove_bone_remaps_slots_and_constraints) {
    SkeletonData d = chain();
    SlotData s1; s1.name = "s1"; s1.bone = 2;
    SlotData s0; s0.name = "s0"; s0.bone = 0;
    d.slots = {s0, s1};
    BoneData t; t.name = "t"; t.parent = 0;
    d.bones.push_back(t);
    IkConstraintData ik; ik.name = "ik"; ik.bones = {1}; ik.target = 3;
    d.ik.push_back(ik);
    d.removeBone(2);
    CHECK(d.bones.size() == 3);
    CHECK(d.slots.size() == 1);
    CHECK(d.ik.size() == 1 && d.ik[0].target == 2);
    d.removeBone(1);
    CHECK(d.ik.empty());
}

TEST(set_parent_keeps_parents_before_children) {
    SkeletonData d = chain();
    BoneData c; c.name = "c"; c.parent = 0;
    d.bones.push_back(c);
    d.setParent(1, 3); // a under c, but c is after a
    for (size_t i = 0; i < d.bones.size(); i++) CHECK(d.bones[i].parent < (int)i);
    CHECK(d.bones[d.findBone("a")].parent == d.findBone("c"));
    CHECK(d.bones[d.findBone("b")].parent == d.findBone("a"));
}
