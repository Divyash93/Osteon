// The root bone and its descendants: edits made under a moved, rotated or scaled root must keep world placement.
#include "../Test.h"
#include "Document.h"
#include <cmath>

using namespace osteon;

static Vec2 worldOrigin(Document& doc, const char* name) {
    Skeleton s(doc.data);
    s.updateWorldTransform(Physics::None);
    const Bone& b = s.bones[doc.data.findBone(name)];
    return {b.worldX, b.worldY};
}

static Vec2 worldTip(Document& doc, const char* name) {
    Skeleton s(doc.data);
    s.updateWorldTransform(Physics::None);
    const Bone& b = s.bones[doc.data.findBone(name)];
    return b.localToWorld({doc.data.bones[doc.data.findBone(name)].length, 0});
}

static void moveRoot(Document& doc, float rot, float x, float y, float sx, float sy) {
    doc.data.bones[0].rotation = rot;
    doc.data.bones[0].x = x; doc.data.bones[0].y = y;
    doc.data.bones[0].scaleX = sx; doc.data.bones[0].scaleY = sy;
    doc.updatePose();
}

TEST(root_new_bone_lands_where_it_was_drawn_under_a_transformed_root) {
    Document doc;
    doc.newProject();
    moveRoot(doc, 35, 40, -20, 1, 1);
    int a = doc.createBone(0, {100, 50}, {160, 90}, "a");
    CHECK(a >= 0);
    Vec2 o = worldOrigin(doc, "a"), t = worldTip(doc, "a");
    CHECK_NEAR(o.x, 100.f, 0.01f); CHECK_NEAR(o.y, 50.f, 0.01f);
    CHECK_NEAR(t.x, 160.f, 0.01f); CHECK_NEAR(t.y, 90.f, 0.01f);
    // A child drawn from that bone's tip.
    doc.createBone(doc.data.findBone("a"), t, {200, 40}, "b");
    Vec2 bo = worldOrigin(doc, "b"), bt = worldTip(doc, "b");
    CHECK_NEAR(bo.x, t.x, 0.01f); CHECK_NEAR(bo.y, t.y, 0.01f);
    CHECK_NEAR(bt.x, 200.f, 0.01f); CHECK_NEAR(bt.y, 40.f, 0.01f);
}

TEST(root_new_bone_under_a_uniformly_scaled_and_mirrored_root) {
    Document doc;
    doc.newProject();
    moveRoot(doc, 20, 10, 10, -2, 2); // mirrored in x
    doc.createBone(0, {50, 30}, {90, 90}, "a");
    Vec2 o = worldOrigin(doc, "a"), t = worldTip(doc, "a");
    CHECK_NEAR(o.x, 50.f, 0.02f); CHECK_NEAR(o.y, 30.f, 0.02f);
    CHECK_NEAR(t.x, 90.f, 0.02f); CHECK_NEAR(t.y, 90.f, 0.02f);
}

TEST(root_moving_the_root_moves_descendants_and_their_images_rigidly) {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "a");
    doc.createBone(doc.data.findBone("a"), {50, 0}, {100, 0}, "b");
    int slot = doc.createSlot(doc.data.findBone("b"), "img");
    doc.addRegionAttachment(slot, "none", {75, 10});
    doc.updatePose();
    Vec2 a0 = worldOrigin(doc, "b");
    float dist0 = std::hypot(a0.x, a0.y);
    doc.setMode(Mode::Setup);
    Document::LocalPose p = doc.bonePose(0);
    p.rotation = 90; p.x = 5; p.y = 7;
    doc.setBonePose(0, p, (int)Channel::Rotate | (int)Channel::Translate);
    doc.updatePose();
    Vec2 b1 = worldOrigin(doc, "b");
    CHECK_NEAR(std::hypot(b1.x - 5, b1.y - 7), dist0, 0.01f);   // rotation about the root's own origin keeps distances
    CHECK_NEAR(b1.x, 5.f - a0.y, 0.01f); CHECK_NEAR(b1.y, 7.f + a0.x, 0.01f);
}

TEST(root_reparenting_to_the_root_keeps_world_pose_under_a_transformed_root) {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "a");
    doc.createBone(doc.data.findBone("a"), {50, 0}, {100, 0}, "b");
    moveRoot(doc, 40, 15, -10, 1.5f, 1.5f);
    Vec2 before = worldOrigin(doc, "b"), tipBefore = worldTip(doc, "b");
    doc.reparentBones({doc.data.findBone("b")}, 0);
    CHECK(doc.data.bones[doc.data.findBone("b")].parent == 0);
    Vec2 after = worldOrigin(doc, "b"), tipAfter = worldTip(doc, "b");
    CHECK_NEAR(after.x, before.x, 0.02f); CHECK_NEAR(after.y, before.y, 0.02f);
    CHECK_NEAR(tipAfter.x, tipBefore.x, 0.02f); CHECK_NEAR(tipAfter.y, tipBefore.y, 0.02f);
}

TEST(root_cannot_be_deleted_reparented_or_duplicated_and_can_be_renamed) {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "a");
    doc.selection = {{SelType::Bone, 0, ""}};
    doc.deleteSelection();
    CHECK(doc.data.bones.size() == 2);
    doc.duplicateSelection();
    CHECK(doc.data.bones.size() == 2);
    doc.reparentBones({0}, doc.data.findBone("a"));
    CHECK(doc.data.bones[0].parent == -1);
    doc.selection = {{SelType::Bone, 0, ""}};
    CHECK(doc.renameSelection("hips"));
    CHECK(doc.data.bones[0].name == "hips");
}

TEST(root_deleting_a_bone_with_descendants_removes_the_subtree_only) {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "a");
    doc.createBone(doc.data.findBone("a"), {50, 0}, {100, 0}, "b");
    doc.createBone(0, {0, 0}, {0, 50}, "c");
    doc.selection = {{SelType::Bone, doc.data.findBone("a"), ""}};
    doc.deleteSelection();
    CHECK(doc.data.bones.size() == 2);
    CHECK(doc.data.findBone("c") >= 0 && doc.data.findBone("a") < 0 && doc.data.findBone("b") < 0);
    doc.undo();
    CHECK(doc.data.bones.size() == 4);
}

TEST(root_new_bone_in_animate_mode_is_placed_from_the_setup_pose_not_the_animated_one) {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "a");
    int an = doc.createAnimation("anim");
    doc.setMode(Mode::Animate);
    doc.animation = an;
    doc.setTime(0.5f);
    doc.autoKey = true;
    Document::LocalPose p = doc.bonePose(doc.data.findBone("a"));
    p.rotation = 70;
    doc.setBonePose(doc.data.findBone("a"), p, (int)Channel::Rotate);
    doc.updatePose();
    doc.setMode(Mode::Setup);
    doc.updatePose();
    // In Setup mode the pose is the setup pose; a bone created now hangs off the setup 'a'.
    doc.createBone(doc.data.findBone("a"), {50, 0}, {100, 0}, "b");
    Vec2 o = worldOrigin(doc, "b");
    CHECK_NEAR(o.x, 50.f, 0.01f); CHECK_NEAR(o.y, 0.f, 0.01f);
    CHECK_NEAR(doc.data.bones[doc.data.findBone("b")].rotation, 0.f, 0.01f);
}

TEST(root_hiding_a_bone_hides_only_that_bone_and_right_click_hides_the_subtree) {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "a");
    doc.createBone(doc.data.findBone("a"), {50, 0}, {100, 0}, "b");
    int slotA = doc.createSlot(doc.data.findBone("a"), "sa");
    int slotB = doc.createSlot(doc.data.findBone("b"), "sb");
    // setSubtreeVisible: the bone, its descendants and their slots; nothing else.
    doc.setSubtreeVisible(doc.data.findBone("a"), false);
    CHECK(!doc.data.bones[doc.data.findBone("a")].visible && !doc.data.bones[doc.data.findBone("b")].visible);
    CHECK(doc.data.bones[0].visible);
    CHECK(!doc.data.slots[slotA].visible && !doc.data.slots[slotB].visible);
    doc.setSubtreeVisible(doc.data.findBone("a"), true);
    CHECK(doc.data.bones[doc.data.findBone("b")].visible && doc.data.slots[slotB].visible);
    doc.undo();
    CHECK(!doc.data.bones[doc.data.findBone("b")].visible);
}

TEST(root_bone_select_and_name_flags_are_saved_in_projects) {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "a");
    doc.data.bones[0].selectable = false;
    doc.data.bones[doc.data.findBone("a")].showName = true;
    JsonWriteOptions o;
    SkeletonData back;
    CHECK(readSkeletonJson(writeSkeletonJson(doc.data, o), back).ok);
    CHECK(!back.bones[0].selectable && back.bones[back.findBone("a")].showName);
    CHECK(back.bones[back.findBone("a")].selectable && !back.bones[0].showName);
}

static Document splitRig() {
    Document doc;
    doc.newProject();
    moveRoot(doc, 25, 5, 9, 1, 1);
    doc.createBone(0, {10, 10}, {110, 10}, "arm");          // length 100 in world terms under the rotated root
    int arm = doc.data.findBone("arm");
    doc.createBone(arm, doc.data.bones.size() ? Vec2{110, 10} : Vec2{}, {150, 60}, "hand");
    return doc;
}

TEST(root_split_into_three_nested_pieces_keeps_everything_in_place) {
    Document doc = splitRig();
    float total = doc.data.bones[doc.data.findBone("arm")].length;
    Vec2 handBefore = worldOrigin(doc, "hand"), handTip = worldTip(doc, "hand"), armTip = worldTip(doc, "arm");
    int last = doc.splitBone(doc.data.findBone("arm"), 3, true, false);
    CHECK(last >= 0);
    CHECK(doc.data.bones.size() == 5);
    const BoneData& a = doc.data.bones[doc.data.findBone("arm")];
    const BoneData& a2 = doc.data.bones[doc.data.findBone("arm2")];
    const BoneData& a3 = doc.data.bones[doc.data.findBone("arm3")];
    CHECK_NEAR(a.length, total / 3, 1e-3f); CHECK_NEAR(a2.length, total / 3, 1e-3f); CHECK_NEAR(a3.length, total / 3, 1e-3f);
    CHECK(a2.parent == doc.data.findBone("arm") && a3.parent == doc.data.findBone("arm2"));   // nested chain
    CHECK(doc.data.bones[doc.data.findBone("hand")].parent == doc.data.findBone("arm3"));        // old child on the last piece
    Vec2 handAfter = worldOrigin(doc, "hand"), tipAfter = worldTip(doc, "hand"), armEnd = worldTip(doc, "arm3");
    CHECK_NEAR(handAfter.x, handBefore.x, 0.01f); CHECK_NEAR(handAfter.y, handBefore.y, 0.01f);
    CHECK_NEAR(tipAfter.x, handTip.x, 0.01f); CHECK_NEAR(tipAfter.y, handTip.y, 0.01f);
    CHECK_NEAR(armEnd.x, armTip.x, 0.01f); CHECK_NEAR(armEnd.y, armTip.y, 0.01f);          // the chain still ends where the bone ended
}

TEST(root_split_not_nested_shares_the_original_parent_and_pieces_line_up) {
    Document doc = splitRig();
    int arm = doc.data.findBone("arm");
    Vec2 tip = worldTip(doc, "arm");
    doc.splitBone(arm, 2, false, false);
    int a2 = doc.data.findBone("arm2");
    CHECK(doc.data.bones[a2].parent == doc.data.bones[doc.data.findBone("arm")].parent);
    Vec2 o2 = worldOrigin(doc, "arm2"), end2 = worldTip(doc, "arm2"), mid = worldTip(doc, "arm");
    CHECK_NEAR(o2.x, mid.x, 0.01f); CHECK_NEAR(o2.y, mid.y, 0.01f);     // starts where the first piece ends
    CHECK_NEAR(end2.x, tip.x, 0.01f); CHECK_NEAR(end2.y, tip.y, 0.01f);
}

TEST(root_split_fibonacci_lengths_decrease_and_add_up) {
    Document doc = splitRig();
    float total = doc.data.bones[doc.data.findBone("arm")].length;
    doc.splitBone(doc.data.findBone("arm"), 4, true, true);   // 3 : 2 : 1 : 1
    float l1 = doc.data.bones[doc.data.findBone("arm")].length, l2 = doc.data.bones[doc.data.findBone("arm2")].length,
          l3 = doc.data.bones[doc.data.findBone("arm3")].length, l4 = doc.data.bones[doc.data.findBone("arm4")].length;
    CHECK_NEAR(l1 + l2 + l3 + l4, total, 1e-3f);
    CHECK_NEAR(l1 / l4, 3.f, 1e-3f); CHECK_NEAR(l2 / l4, 2.f, 1e-3f); CHECK_NEAR(l3 / l4, 1.f, 1e-3f);
    CHECK(doc.splitBone(0, 2, true, false) < 0);              // the zero-length root cannot be split
}

using L = std::vector<std::pair<int, int>>;
// The red dashed parent guide is drawn from Document::selectedParentLinks(): one (child, parent) link per selected bone.
TEST(parent_guide_links_follow_the_selection_and_the_hierarchy) {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "a");
    doc.createBone(doc.data.findBone("a"), {50, 0}, {100, 0}, "b");
    doc.createBone(doc.data.findBone("b"), {100, 0}, {150, 0}, "c");
    int a = doc.data.findBone("a"), b = doc.data.findBone("b"), c = doc.data.findBone("c");
    doc.clearSelection();
    CHECK(doc.selectedParentLinks().empty());                       // nothing selected: no guide
    doc.select({SelType::Bone, 0, ""});
    CHECK(doc.selectedParentLinks().empty());                       // root: no parent, no guide
    doc.select({SelType::Bone, a, ""});
    { L e{{a, 0}}; CHECK(doc.selectedParentLinks() == e); }
    doc.select({SelType::Bone, c, ""});                              // another bone: only its own link, not a's
    { L e{{c, b}}; CHECK(doc.selectedParentLinks() == e); }
    // Re-parenting changes the link; clearing the selection (what a double click on empty space does) removes it.
    doc.reparentBones({c}, a);
    doc.select({SelType::Bone, doc.data.findBone("c"), ""});
    { L e{{doc.data.findBone("c"), doc.data.findBone("a")}}; CHECK(doc.selectedParentLinks() == e); }
    doc.clearSelection();
    CHECK(doc.selectedParentLinks().empty());
}

TEST(parent_guide_selection_changes_never_alter_the_hierarchy_through_undo_and_save) {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "a");
    doc.createBone(doc.data.findBone("a"), {50, 0}, {100, 0}, "b");
    auto parents = [&] { std::vector<std::string> p; for (auto& bn : doc.data.bones) p.push_back(bn.name + ":" + (bn.parent >= 0 ? doc.data.bones[bn.parent].name : "-")); return p; };
    auto before = parents();
    doc.select({SelType::Bone, doc.data.findBone("b"), ""});
    doc.clearSelection();
    doc.select({SelType::Bone, doc.data.findBone("a"), ""});
    CHECK(parents() == before);
    doc.undo(); doc.redo();
    CHECK(parents().size() == before.size());
    std::string err, path = "osteon_guide_test.osteon";
    CHECK(doc.saveProject(path, err));
    Document back;
    CHECK(back.openFile(path, err));
    std::remove(path.c_str());
    CHECK(back.data.bones.size() == doc.data.bones.size());
    CHECK(back.data.bones[back.data.findBone("b")].parent == back.data.findBone("a"));
}
