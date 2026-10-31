// Skin bones and skin constraints: "skin required" bones/constraints are active only in skins that list them.
#include "../Test.h"
#include "Document.h"

using namespace osteon;

static Document rig() {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "arm");
    doc.createBone(doc.data.findBone("arm"), {50, 0}, {90, 0}, "hand");
    doc.createBone(0, {0, 50}, {0, 90}, "target");
    int arm = doc.data.findBone("arm"), hand = doc.data.findBone("hand"), target = doc.data.findBone("target");
    doc.createIk("reach", {arm, hand}, target);
    doc.createSkin("A");
    doc.createSkin("B");
    return doc;
}

TEST(skin_required_bone_is_active_only_in_skins_that_list_it) {
    Document doc = rig();
    int hand = doc.data.findBone("hand");
    doc.data.bones[hand].skinRequired = true;
    doc.setSkinBone(1, hand, true);                 // skin A lists it
    doc.skin = 0; doc.updatePose();
    CHECK(!doc.sk.bones[hand].active);              // default skin: hidden
    doc.skin = 1; doc.updatePose();
    CHECK(doc.sk.bones[hand].active);
    doc.skin = 2; doc.updatePose();
    CHECK(!doc.sk.bones[hand].active);
    doc.undo();                                      // adding the bone was one undo step
    CHECK(doc.data.skins[1].bones.empty());
}

TEST(skin_bones_follow_bone_deletion_and_reordering) {
    Document doc = rig();
    int target = doc.data.findBone("target");
    doc.data.bones[target].skinRequired = true;
    doc.setSkinBone(1, target, true);
    doc.selection = {{SelType::Bone, doc.data.findBone("arm"), ""}};
    doc.deleteSelection();                           // removes arm and hand: indices shift
    int now = doc.data.findBone("target");
    CHECK(doc.data.skins[1].bones.size() == 1 && doc.data.skins[1].bones[0] == now);
    doc.selection = {{SelType::Bone, now, ""}};
    doc.deleteSelection();
    CHECK(doc.data.skins[1].bones.empty());
}

TEST(skin_required_constraint_follows_skin_rename_delete_and_round_trips) {
    Document doc = rig();
    doc.data.ik[0].skinRequired = true;
    doc.setSkinConstraint(1, "reach", true);
    doc.skin = 0; doc.updatePose();
    CHECK(!doc.sk.ik[0].active);
    doc.skin = 1; doc.updatePose();
    CHECK(doc.sk.ik[0].active);
    for (const char* version : {"4.2.00", "4.3.26"}) {
        JsonWriteOptions o; o.version = version;
        SkeletonData back;
        CHECK(readSkeletonJson(writeSkeletonJson(doc.data, o), back).ok);
        CHECK(back.skins.size() == 3 && back.skins[1].constraints.size() == 1 && back.skins[1].constraints[0] == "reach");
    }
    doc.selection = {{SelType::Ik, 0, ""}};
    CHECK(doc.renameSelection("grab"));
    CHECK(doc.data.skins[1].constraints[0] == "grab");
    doc.deleteSelection();
    CHECK(doc.data.skins[1].constraints.empty());
}
