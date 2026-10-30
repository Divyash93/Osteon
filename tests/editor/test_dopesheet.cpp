// Dopesheet behaviour that lives in the document: Adjust shifts the selected keys together.
#include "../Test.h"
#include "Document.h"

using namespace osteon;

static Document adjustDoc(int& timelineIndex) {
    Document doc;
    doc.newProject();
    BoneData arm; arm.name = "arm"; arm.parent = 0; arm.length = 50;
    doc.data.bones.push_back(arm);
    doc.updatePose();
    doc.createAnimation("a");
    doc.animation = 0;
    doc.setMode(Mode::Animate);
    Animation& an = doc.data.animations[0];
    Timeline& tl = an.get(TimelineType::Rotate, 1);
    tl.setKey(0.0f).v[0] = 0;
    tl.setKey(1.0f).v[0] = 50;
    tl.setKey(2.0f).v[0] = 85;
    timelineIndex = 0;
    doc.updatePose();
    return doc;
}

TEST(dopesheet_adjust_shifts_all_selected_keys_by_the_same_amount) {
    int ti = 0;
    Document doc = adjustDoc(ti);
    doc.adjust = true;
    doc.adjustKeys = {{ti, 0}, {ti, 1}, {ti, 2}};
    doc.setTime(1.0f);
    doc.updatePose();
    Document::LocalPose p = doc.bonePose(1);
    p.rotation += 15;
    doc.setBonePose(1, p, (int)Channel::Rotate);
    Timeline& tl = doc.data.animations[0].timelines[ti];
    CHECK(tl.keys.size() == 3); // no new key was made
    CHECK_NEAR(tl.keys[0].v[0], 15, 1e-3);
    CHECK_NEAR(tl.keys[1].v[0], 65, 1e-3);
    CHECK_NEAR(tl.keys[2].v[0], 100, 1e-3);
}

TEST(dopesheet_adjust_leaves_unselected_keys_alone) {
    int ti = 0;
    Document doc = adjustDoc(ti);
    doc.adjust = true;
    doc.adjustKeys = {{ti, 1}};
    doc.setTime(1.0f);
    doc.updatePose();
    Document::LocalPose p = doc.bonePose(1);
    p.rotation += 10;
    doc.setBonePose(1, p, (int)Channel::Rotate);
    Timeline& tl = doc.data.animations[0].timelines[ti];
    CHECK_NEAR(tl.keys[0].v[0], 0, 1e-3);
    CHECK_NEAR(tl.keys[1].v[0], 60, 1e-3);
    CHECK_NEAR(tl.keys[2].v[0], 85, 1e-3);
}

TEST(dopesheet_without_adjust_keys_at_the_current_frame) {
    int ti = 0;
    Document doc = adjustDoc(ti);
    doc.setTime(0.5f);
    Document::LocalPose p = doc.bonePose(1);
    p.rotation += 10;
    doc.setBonePose(1, p, (int)Channel::Rotate);
    CHECK(doc.data.animations[0].timelines[ti].keys.size() == 4);
}
