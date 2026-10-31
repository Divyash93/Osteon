// Export options: scale and animation clean-up.
#include "Test.h"
#include "SkeletonJson.h"
#include "Skeleton.h"
#include <cmath>

using namespace osteon;

static SkeletonData rig() {
    SkeletonData d;
    BoneData root; root.name = "root"; d.bones.push_back(root);
    BoneData arm; arm.name = "arm"; arm.parent = 0; arm.x = 10; arm.y = 4; arm.length = 50; arm.rotation = 30; d.bones.push_back(arm);
    SlotData s; s.name = "s"; s.bone = 1; s.attachment = "img"; d.slots.push_back(s);
    Attachment a; a.type = AttachmentType::Region; a.name = "img"; a.x = 5; a.y = 6; a.width = 20; a.height = 40; a.scaleX = 2;
    d.defaultSkin().set(0, a);
    Attachment m; m.type = AttachmentType::Mesh; m.name = "mesh"; m.width = 10; m.height = 10;
    m.uvs = {0, 0, 1, 0, 1, 1}; m.triangles = {0, 1, 2}; m.hullLength = 3;
    m.vertices = {0, 0, 10, 0, 10, 10}; m.worldVerticesLength = 6;
    d.defaultSkin().set(0, m);
    Animation an; an.name = "a";
    Timeline t; t.type = TimelineType::Translate; t.target = 1;
    for (int i = 0; i < 4; i++) { Key k; k.time = i * 0.5f; k.v[0] = i == 3 ? 20.f : 8.f; k.v[1] = -3; t.keys.push_back(k); }
    t.keys[1].curve = CurveType::Bezier; t.keys[1].bezier = {0.2f, 1.f, 0.8f, 6.f, 0.2f, -1.f, 0.8f, -2.f};
    an.timelines.push_back(t);
    Timeline r; r.type = TimelineType::Rotate; r.target = 1;
    for (int i = 0; i < 5; i++) { Key k; k.time = i * 0.25f; k.v[0] = i == 4 ? 45.f : 10.f; r.keys.push_back(k); }
    an.timelines.push_back(r);
    d.animations.push_back(an);
    return d;
}

TEST(export_scale_multiplies_distances_but_not_angles) {
    SkeletonData d = rig();
    scaleSkeletonData(d, 2);
    CHECK_NEAR(d.bones[1].x, 20.f, 1e-4f); CHECK_NEAR(d.bones[1].y, 8.f, 1e-4f); CHECK_NEAR(d.bones[1].length, 100.f, 1e-4f);
    CHECK_NEAR(d.bones[1].rotation, 30.f, 1e-4f);
    const Attachment* a = d.skins[0].get(0, "img");
    CHECK_NEAR(a->x, 10.f, 1e-4f); CHECK_NEAR(a->width, 40.f, 1e-4f); CHECK_NEAR(a->height, 80.f, 1e-4f); CHECK_NEAR(a->scaleX, 2.f, 1e-4f);
    const Attachment* m = d.skins[0].get(0, "mesh");
    CHECK_NEAR(m->vertices[2], 20.f, 1e-4f); CHECK_NEAR(m->uvs[2], 1.f, 1e-4f);
    const Timeline& t = d.animations[0].timelines[0];
    CHECK_NEAR(t.keys[3].v[0], 40.f, 1e-4f); CHECK_NEAR(t.keys[0].v[1], -6.f, 1e-4f);
    CHECK_NEAR(t.keys[1].bezier[3], 12.f, 1e-4f);   // value-part of the first channel's handle, doubled
    CHECK_NEAR(t.keys[1].bezier[0], 0.2f, 1e-4f);   // time part untouched
    CHECK_NEAR(d.animations[0].timelines[1].keys[4].v[0], 45.f, 1e-4f); // rotation untouched
    // The scaled skeleton poses at twice the distance.
    SkeletonData base = rig();
    Skeleton s1(base), s2(d);
    s1.updateWorldTransform(Physics::None); s2.updateWorldTransform(Physics::None);
    CHECK_NEAR(s2.bones[1].worldX, s1.bones[1].worldX * 2, 1e-3f);
    CHECK_NEAR(s2.bones[1].worldY, s1.bones[1].worldY * 2, 1e-3f);
}

TEST(export_scale_of_one_changes_nothing_and_round_trips) {
    SkeletonData d = rig(), e = rig();
    scaleSkeletonData(e, 1);
    CHECK(writeSkeletonJson(d) == writeSkeletonJson(e));
}

TEST(animation_clean_up_removes_repeated_keys_but_keeps_curves) {
    SkeletonData d = rig();
    int removed = cleanUpAnimations(d);
    const Timeline& rot = d.animations[0].timelines[1];
    CHECK(rot.keys.size() == 3);   // 10,10,10,10,45 -> 10,10,45: the middle repeats are gone, the ends stay
    CHECK_NEAR(rot.keys[1].v[0], 10.f, 1e-4f); CHECK_NEAR(rot.keys[1].time, 0.75f, 1e-4f);
    const Timeline& tr = d.animations[0].timelines[0];
    CHECK(tr.keys.size() == 4);    // a Bezier key in the run is never removed
    CHECK(removed == 2);
}
