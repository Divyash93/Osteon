// Operations behind the viewport right-click menu.
#include "../Test.h"
#include "Document.h"
#include <cmath>

using namespace osteon;

static Document rig() {
    Document doc;
    doc.newProject();
    doc.createBone(0, {0, 0}, {50, 0}, "arm");
    doc.createBone(doc.data.findBone("arm"), {50, 0}, {90, 0}, "hand");
    int s1 = doc.createSlot(doc.data.findBone("arm"), "sleeve");
    doc.addRegionAttachment(s1, "sleeveImg", {25, 0});
    int s2 = doc.createSlot(doc.data.findBone("hand"), "glove");
    doc.addRegionAttachment(s2, "gloveImg", {70, 0});
    int s3 = doc.createSlot(0, "back");
    doc.addRegionAttachment(s3, "backImg", {0, 0});
    return doc;
}

TEST(copy_paste_duplicates_bones_with_their_subtree_and_slots_and_is_undoable) {
    Document doc = rig();
    size_t bones = doc.data.bones.size(), slots = doc.data.slots.size();
    doc.selection = {{SelType::Bone, doc.data.findBone("arm"), ""}};
    CHECK(doc.copySelection());
    CHECK(Document::clipboard().bones.size() == 2 && Document::clipboard().slots.size() == 2);   // arm + hand, sleeve + glove
    CHECK(doc.pasteObjects(false) == 4);
    CHECK(doc.data.bones.size() == bones + 2 && doc.data.slots.size() == slots + 2);
    int arm2 = doc.data.findBone("arm2"), hand2 = doc.data.findBone("hand2");
    CHECK(arm2 >= 0 && hand2 >= 0);
    CHECK(doc.data.bones[hand2].parent == arm2);                           // the copy keeps its internal hierarchy
    CHECK(doc.data.bones[arm2].parent == 0);                               // and hangs off the original parent
    int sleeve2 = doc.data.findSlot("sleeve2");
    CHECK(sleeve2 >= 0 && doc.data.slots[sleeve2].bone == arm2);           // the copied slot sits on the copied bone
    CHECK(doc.data.defaultSkin().get(sleeve2, "sleeveImg") != nullptr);    // with its attachment
    doc.undo();
    CHECK(doc.data.bones.size() == bones && doc.data.slots.size() == slots);
}

TEST(paste_special_puts_everything_under_the_selected_bone) {
    Document doc = rig();
    doc.selection = {{SelType::Slot, doc.data.findSlot("back"), ""}};
    CHECK(doc.copySelection());
    doc.selection = {{SelType::Bone, doc.data.findBone("hand"), ""}};
    CHECK(doc.pasteObjects(true) == 1);
    int back2 = doc.data.findSlot("back2");
    CHECK(back2 >= 0 && doc.data.slots[back2].bone == doc.data.findBone("hand"));
    // A normal paste puts the copy back on the original bone.
    doc.selection = {{SelType::Slot, doc.data.findSlot("back"), ""}};
    doc.copySelection();
    doc.pasteObjects(false);
    CHECK(doc.data.slots[doc.data.findSlot("back3")].bone == 0);
}

TEST(copy_and_paste_transform_for_bones_and_images) {
    Document doc = rig();
    int arm = doc.data.findBone("arm"), hand = doc.data.findBone("hand");
    doc.data.bones[arm].rotation = 33; doc.data.bones[arm].x = 7; doc.data.bones[arm].scaleX = 1.5f;
    doc.selection = {{SelType::Bone, arm, ""}};
    CHECK(doc.copyTransform());
    doc.selection = {{SelType::Bone, hand, ""}};
    CHECK(doc.pasteTransform() == 1);
    CHECK_NEAR(doc.data.bones[hand].rotation, 33.f, 1e-4f); CHECK_NEAR(doc.data.bones[hand].x, 7.f, 1e-4f); CHECK_NEAR(doc.data.bones[hand].scaleX, 1.5f, 1e-4f);
    // Images: the attachment's own transform.
    int sleeve = doc.data.findSlot("sleeve"), glove = doc.data.findSlot("glove");
    doc.data.defaultSkin().get(sleeve, "sleeveImg")->rotation = 12;
    doc.data.defaultSkin().get(sleeve, "sleeveImg")->x = 3;
    doc.selection = {{SelType::Slot, sleeve, ""}};
    CHECK(doc.copyTransform());
    doc.selection = {{SelType::Slot, glove, ""}};
    CHECK(doc.pasteTransform() == 1);
    CHECK_NEAR(doc.data.defaultSkin().get(glove, "gloveImg")->rotation, 12.f, 1e-4f);
    // A bone clipboard does not paste onto an image.
    doc.selection = {{SelType::Bone, arm, ""}};
    doc.copyTransform();
    doc.selection = {{SelType::Slot, glove, ""}};
    CHECK(doc.pasteTransform() == 0);
}

TEST(copy_and_paste_slot_property) {
    Document doc = rig();
    int sleeve = doc.data.findSlot("sleeve"), glove = doc.data.findSlot("glove");
    doc.data.slots[sleeve].color = Color(1, 0, 0, 0.5f);
    doc.data.slots[sleeve].blend = BlendMode::Additive;
    doc.selection = {{SelType::Slot, sleeve, ""}};
    CHECK(doc.copySlotProperty());
    doc.selection = {{SelType::Slot, glove, ""}};
    CHECK(doc.pasteSlotProperty() == 1);
    CHECK(doc.data.slots[glove].blend == BlendMode::Additive && doc.data.slots[glove].color.a == 0.5f);
}

TEST(draw_order_moves_forward_backward_to_front_and_back) {
    Document doc = rig(); // slots: sleeve(0) glove(1) back(2)
    auto order = [&] { std::string s; for (auto& sl : doc.data.slots) s += sl.name[0]; return s; };
    CHECK(order() == "sgb");
    doc.selection = {{SelType::Slot, doc.data.findSlot("sleeve"), ""}};
    doc.moveSlotsInDrawOrder(Document::DrawOrderMove::Forward);
    CHECK(order() == "gsb");
    doc.moveSlotsInDrawOrder(Document::DrawOrderMove::ToFront);
    CHECK(order() == "gbs");
    doc.moveSlotsInDrawOrder(Document::DrawOrderMove::Backward);
    CHECK(order() == "gsb");
    doc.moveSlotsInDrawOrder(Document::DrawOrderMove::ToBack);
    CHECK(order() == "sgb");
    doc.moveSlotsInDrawOrder(Document::DrawOrderMove::Backward);   // already at the back: nothing changes
    CHECK(order() == "sgb");
    // Two selected slots keep their relative order.
    doc.selection = {{SelType::Slot, doc.data.findSlot("sleeve"), ""}, {SelType::Slot, doc.data.findSlot("glove"), ""}};
    doc.moveSlotsInDrawOrder(Document::DrawOrderMove::ToFront);
    CHECK(order() == "bsg");
}

TEST(hide_lock_and_show_and_unlock_all) {
    Document doc = rig();
    int sleeve = doc.data.findSlot("sleeve"), glove = doc.data.findSlot("glove");
    doc.selection = {{SelType::Slot, sleeve, ""}};
    doc.setVisibility(false, false);
    CHECK(!doc.data.slots[sleeve].visible && doc.data.slots[glove].visible);
    doc.setVisibility(true, false);                                   // Hide Others
    CHECK(doc.data.slots[sleeve].visible && !doc.data.slots[glove].visible && !doc.data.bones[doc.data.findBone("arm")].visible);
    doc.setLocked(false, true);
    CHECK(doc.data.slots[sleeve].locked && !doc.data.slots[glove].locked);
    doc.setLocked(true, true);                                         // Lock Others
    CHECK(!doc.data.slots[sleeve].locked && doc.data.slots[glove].locked && !doc.data.bones[doc.data.findBone("arm")].selectable);
    doc.showAndUnlockAll();
    for (auto& s : doc.data.slots) CHECK(s.visible && !s.locked);
    for (auto& b : doc.data.bones) CHECK(b.visible && b.selectable);
    doc.undo();                                                        // one undo step
    CHECK(doc.data.slots[glove].locked);
}

TEST(locked_flag_is_saved_in_projects_but_not_in_exports) {
    Document doc = rig();
    doc.data.slots[doc.data.findSlot("glove")].locked = true;
    std::string err, path = "osteon_lock_test.osteon";
    CHECK(doc.saveProject(path, err));
    Document back;
    CHECK(back.openFile(path, err));
    std::remove(path.c_str());
    CHECK(back.data.slots[back.data.findSlot("glove")].locked && !back.data.slots[back.data.findSlot("sleeve")].locked);
    JsonWriteOptions o;
    CHECK(writeSkeletonJson(doc.data, o).find("locked") == std::string::npos);
}

TEST(describe_delete_counts_what_goes_with_a_bone_or_an_image) {
    Document doc = rig();
    doc.selection = {{SelType::Bone, doc.data.findBone("arm"), ""}};
    auto d = doc.describeDelete();
    CHECK(d.kind == 1 && d.name == "arm" && d.childBones == 1 && d.slots == 2);        // hand, and sleeve + glove
    doc.selection = {{SelType::Slot, doc.data.findSlot("glove"), ""}};
    d = doc.describeDelete();
    CHECK(d.kind == 2 && d.boneName == "hand" && d.canDeleteBone && d.childBones == 0 && d.slots == 1);
    doc.selection = {{SelType::Slot, doc.data.findSlot("back"), ""}};
    CHECK(!doc.describeDelete().canDeleteBone);                                         // parented to the root: only the slot can go
    doc.selection = {{SelType::Bone, 0, ""}};
    CHECK(doc.describeDelete().kind == 0);
}

TEST(delete_slot_and_its_bone_removes_the_parented_bone_tree) {
    Document doc = rig();
    doc.selection = {{SelType::Slot, doc.data.findSlot("sleeve"), ""}};
    doc.deleteSlotAndItsBone();                                        // sleeve is on arm: arm, hand, sleeve and glove all go
    CHECK(doc.data.findBone("arm") < 0 && doc.data.findBone("hand") < 0);
    CHECK(doc.data.findSlot("sleeve") < 0 && doc.data.findSlot("glove") < 0 && doc.data.findSlot("back") >= 0);
    doc.undo();
    CHECK(doc.data.findBone("hand") >= 0 && doc.data.findSlot("glove") >= 0);
    // On the root nothing but the slot is removed.
    doc.selection = {{SelType::Slot, doc.data.findSlot("back"), ""}};
    doc.deleteSlotAndItsBone();
    CHECK(doc.data.findSlot("back") < 0 && doc.data.findBone("arm") >= 0);
}
