// Editor document behaviour: undo/redo, Setup vs Animate editing, keying, structural edits.
#include "../Test.h"
#include "Document.h"
#include "Util.h"
#include <chrono>
#include <cmath>

using namespace osteon;

static Document makeDoc() {
    Document doc;
    doc.newProject();
    BoneData arm; arm.name = "arm"; arm.parent = 0; arm.x = 10; arm.length = 50;
    doc.data.bones.push_back(arm);
    SlotData s; s.name = "s"; s.bone = 0; s.attachment = "img";
    doc.data.slots.push_back(s);
    Attachment a; a.name = "img"; a.x = 30; a.y = 5; a.width = 20; a.height = 10;
    doc.data.defaultSkin().set(0, a);
    doc.updatePose();
    return doc;
}

TEST(doc_undo_redo_restores_data) {
    Document doc = makeDoc();
    doc.checkpoint("Rotate");
    doc.data.bones[1].rotation = 45;
    doc.undo();
    CHECK_NEAR(doc.data.bones[1].rotation, 0, 1e-6);
    doc.redo();
    CHECK_NEAR(doc.data.bones[1].rotation, 45, 1e-6);
}

TEST(doc_begin_commit_edit_is_one_step) {
    Document doc = makeDoc();
    doc.beginEdit("Drag");
    for (int i = 0; i < 10; i++) doc.data.bones[1].x += 1;
    doc.commitEdit();
    CHECK(doc.undoStack.size() == 1);
    doc.undo();
    CHECK_NEAR(doc.data.bones[1].x, 10, 1e-6);
}

TEST(doc_setup_mode_edits_data) {
    Document doc = makeDoc();
    Document::LocalPose p = doc.bonePose(1);
    p.rotation = 30;
    doc.setBonePose(1, p, (int)Channel::Rotate);
    CHECK_NEAR(doc.data.bones[1].rotation, 30, 1e-6);
    CHECK(doc.data.animations.empty());
}

TEST(doc_animate_autokey_writes_relative_keys) {
    Document doc = makeDoc();
    doc.data.bones[1].rotation = 10;
    doc.createAnimation("walk");
    doc.setMode(Mode::Animate);
    doc.setTime(0.5f);
    doc.updatePose();
    Document::LocalPose p = doc.bonePose(1);
    p.rotation = 40;
    p.x = 25;
    doc.setBonePose(1, p, (int)Channel::Rotate | (int)Channel::Translate);
    Animation& an = doc.data.animations[0];
    Timeline* r = an.find(TimelineType::Rotate, 1);
    Timeline* t = an.find(TimelineType::Translate, 1);
    CHECK(r && r->keys.size() == 1);
    CHECK(t && t->keys.size() == 1);
    if (r) { CHECK_NEAR(r->keys[0].v[0], 30, 1e-4); CHECK_NEAR(r->keys[0].time, 0.5, 1e-6); }
    if (t) CHECK_NEAR(t->keys[0].v[0], 15, 1e-4);
    CHECK_NEAR(doc.data.bones[1].rotation, 10, 1e-6); // setup pose untouched
    doc.updatePose();
    CHECK_NEAR(doc.sk.bones[1].rotation, 40, 1e-4);
}

TEST(doc_animate_without_autokey_keeps_pending_until_keyed) {
    Document doc = makeDoc();
    doc.createAnimation("walk");
    doc.setMode(Mode::Animate);
    doc.autoKey = false;
    Document::LocalPose p = doc.bonePose(1);
    p.rotation = 20;
    doc.setBonePose(1, p, (int)Channel::Rotate);
    CHECK(doc.data.animations[0].timelines.empty());
    doc.updatePose();
    CHECK_NEAR(doc.sk.bones[1].rotation, 20, 1e-4); // shown although not keyed
    doc.keyBone(1, (int)Channel::Rotate);
    CHECK(doc.pending.empty());
    CHECK(doc.data.animations[0].find(TimelineType::Rotate, 1) != nullptr);
}

static const Timeline* deformTimeline(const Document& doc) {
    for (auto& tl : doc.data.animations[0].timelines) if (tl.type == TimelineType::Deform) return &tl;
    return nullptr;
}

TEST(doc_animate_mesh_deform_pending_then_keyed) {
    Document doc = makeDoc();
    doc.convertToMesh(0, "img");
    doc.createAnimation("walk");
    doc.setMode(Mode::Animate);
    doc.autoKey = false;
    doc.updatePose();
    const Attachment* a = doc.data.defaultSkin().get(0, "img");
    std::vector<float> before, after;
    CHECK(doc.sk.worldVertices(doc.sk.slots[0], *a, before));
    std::vector<float> offsets(a->deformLength(), 0.0f);
    offsets[0] = 5; // move the first vertex 5 units along the bone's x
    doc.setDeform(0, "img", offsets);
    CHECK(deformTimeline(doc) == nullptr);            // not keyed yet
    CHECK(doc.deformKeyState(0, "img") == 3);         // pending
    doc.updatePose();
    CHECK(doc.sk.worldVertices(doc.sk.slots[0], *a, after));
    CHECK_NEAR(after[0] - before[0], 5, 1e-3);        // shown although not keyed
    doc.keyDeform(0, "img");
    CHECK(doc.pendingDeform.empty());
    const Timeline* tl = deformTimeline(doc);
    CHECK(tl && tl->keys.size() == 1 && tl->keys[0].deform == offsets);
    CHECK(doc.deformKeyState(0, "img") == 2);         // keyed at this frame
}

TEST(doc_animate_mesh_deform_autokey_keys_at_once) {
    Document doc = makeDoc();
    doc.convertToMesh(0, "img");
    doc.createAnimation("walk");
    doc.setMode(Mode::Animate);
    doc.autoKey = true;
    const Attachment* a = doc.data.defaultSkin().get(0, "img");
    std::vector<float> offsets(a->deformLength(), 1.0f);
    doc.setDeform(0, "img", offsets);
    CHECK(doc.pendingDeform.empty());
    CHECK(deformTimeline(doc) != nullptr);
}

TEST(doc_select_all_takes_visible_bones_and_slots) {
    Document doc = makeDoc();
    doc.selectAll();
    CHECK(doc.selection.size() == doc.data.bones.size() + doc.data.slots.size());
    CHECK(doc.isSelected({SelType::Bone, 1, ""}));
    CHECK(doc.isSelected({SelType::Slot, 0, ""}));
    doc.data.slots[0].visible = false;
    doc.selectAll();
    CHECK(!doc.isSelected({SelType::Slot, 0, ""})); // hidden items are not selected
}

TEST(doc_shift_range_selects_rows_between_anchor_and_click) {
    Document doc = makeDoc();
    std::vector<Sel> order = {{SelType::Bone, 0, ""}, {SelType::Bone, 1, ""}, {SelType::Slot, 0, ""}};
    doc.select({SelType::Bone, 0, ""});
    doc.selectRange({SelType::Bone, 0, ""}, {SelType::Slot, 0, ""}, order, false);
    CHECK(doc.selection.size() == 3);
    // Range from the bottom back up replaces the selection with just the rows in between.
    doc.selectRange({SelType::Slot, 0, ""}, {SelType::Bone, 1, ""}, order, false);
    CHECK(doc.selection.size() == 2);
    CHECK(!doc.isSelected({SelType::Bone, 0, ""}));
    // Ctrl+Shift keeps what was selected.
    doc.select({SelType::Bone, 0, ""});
    doc.selectRange({SelType::Bone, 0, ""}, {SelType::Slot, 0, ""}, order, true);
    CHECK(doc.selection.size() == 3);
}

TEST(doc_pending_pose_discarded_on_time_change) {
    Document doc = makeDoc();
    doc.createAnimation("walk");
    doc.setMode(Mode::Animate);
    doc.autoKey = false;
    Document::LocalPose p = doc.bonePose(1);
    p.rotation = 20;
    doc.setBonePose(1, p, (int)Channel::Rotate);
    doc.setTime(1);
    CHECK(doc.pending.empty());
}

TEST(doc_reparent_bone_keeps_world_transform) {
    Document doc = makeDoc();
    BoneData b; b.name = "b"; b.parent = 0; b.x = 100; b.y = 50; b.rotation = 15;
    doc.data.bones.push_back(b);
    doc.data.bones[1].rotation = 70;
    doc.data.bones[1].scaleX = 1.5f;
    doc.updatePose();
    Bone before = doc.sk.bones[2];
    doc.reparentBones({2}, 1);
    doc.updatePose();
    int bi = doc.data.findBone("b");
    CHECK(doc.data.bones[bi].parent == doc.data.findBone("arm"));
    const Bone& after = doc.sk.bones[bi];
    CHECK_NEAR(after.worldX, before.worldX, 1e-3);
    CHECK_NEAR(after.worldY, before.worldY, 1e-3);
    CHECK_NEAR(after.a, before.a, 1e-4);
    CHECK_NEAR(after.d, before.d, 1e-4);
}

TEST(doc_reparent_slot_keeps_image_in_place) {
    Document doc = makeDoc();
    doc.data.bones[1].rotation = 30;
    doc.updatePose();
    std::vector<float> before, after;
    doc.sk.worldVertices(doc.sk.slots[0], *doc.sk.attachmentFor(doc.sk.slots[0]), before);
    doc.reparentSlot(0, 1);
    doc.updatePose();
    CHECK(doc.sk.slots[0].bone == 1); // runtime notices the slot's new bone
    doc.sk.worldVertices(doc.sk.slots[0], *doc.sk.attachmentFor(doc.sk.slots[0]), after);
    for (size_t i = 0; i < before.size(); i++) CHECK_NEAR(after[i], before[i], 1e-3);
}

TEST(doc_create_bone_from_world_points) {
    Document doc = makeDoc();
    int b = doc.createBone(1, {10, 0}, {10, 80});
    doc.updatePose();
    const Bone& bone = doc.sk.bones[b];
    CHECK_NEAR(bone.worldX, 10, 1e-3);
    CHECK_NEAR(bone.worldY, 0, 1e-3);
    CHECK_NEAR(bone.worldRotationX(), 90, 1e-3);
    CHECK_NEAR(doc.data.bones[b].length, 80, 1e-3);
    CHECK(doc.isSelected({SelType::Bone, b, ""}));
}

TEST(doc_delete_bone_and_undo) {
    Document doc = makeDoc();
    doc.data.slots[0].bone = 1;
    doc.selection = {{SelType::Bone, 1, ""}};
    doc.deleteSelection();
    CHECK(doc.data.bones.size() == 1 && doc.data.slots.empty());
    doc.undo();
    CHECK(doc.data.bones.size() == 2 && doc.data.slots.size() == 1);
}

TEST(doc_rename_attachment_updates_references) {
    Document doc = makeDoc();
    doc.createAnimation("a");
    doc.data.animations[0].get(TimelineType::Attachment, 0).setKey(0).str = "img";
    doc.selection = {{SelType::Attachment, 0, "img"}};
    CHECK(doc.renameSelection("face"));
    CHECK(doc.data.slots[0].attachment == "face");
    const Attachment* a = doc.data.findAttachment(0, 0, "face");
    CHECK(a && a->imagePath() == "img"); // still points at the same image file
    CHECK(doc.data.animations[0].timelines[0].keys[0].str == "face");
}

TEST(doc_convert_region_to_mesh_keeps_geometry) {
    Document doc = makeDoc();
    doc.updatePose();
    std::vector<float> before, after;
    doc.sk.worldVertices(doc.sk.slots[0], *doc.sk.attachmentFor(doc.sk.slots[0]), before);
    doc.convertToMesh(0, "img");
    doc.updatePose();
    const Attachment* m = doc.sk.attachmentFor(doc.sk.slots[0]);
    CHECK(m && m->type == AttachmentType::Mesh && m->vertexCount() == 4);
    if (m) {
        doc.sk.worldVertices(doc.sk.slots[0], *m, after);
        for (size_t i = 0; i < 8; i++) CHECK_NEAR(after[i], before[i], 1e-3);
    }
}

TEST(doc_save_and_open_project_roundtrip) {
    Document doc = makeDoc();
    doc.createAnimation("idle");
    doc.data.animations[0].get(TimelineType::Rotate, 1).setKey(0.25f).v[0] = 12;
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    std::string path = normalizePath(std::string(tmp) + "osteon_test_project.osteon");
    std::string err;
    CHECK(doc.saveProject(path, err));
    Document other;
    other.newProject();
    CHECK(other.openFile(path, err));
    CHECK(other.data.bones.size() == 2 && other.data.findBone("arm") == 1);
    CHECK(other.data.animations.size() == 1 && other.data.animations[0].timelines[0].keys[0].v[0] == 12);
    CHECK(!other.projectPath.empty() && !other.dirty);
    DeleteFileA(path.c_str());
}

TEST(doc_linked_mesh_shares_geometry) {
    Document doc = makeDoc();
    doc.convertToMesh(0, "img");
    int sk = doc.createSkin("red");
    doc.skin = sk;
    doc.createLinkedMesh(0, "img");
    const Attachment* l = doc.data.skins[sk].get(0, "img linked");
    CHECK(l && l->type == AttachmentType::LinkedMesh && l->parentMesh == "img" && l->parentSkin.empty());
    doc.data.slots[0].attachment = "img linked";
    doc.updatePose();
    std::vector<float> lv, mv;
    CHECK(doc.sk.worldVertices(doc.sk.slots[0], *l, lv));
    doc.sk.worldVertices(doc.sk.slots[0], *doc.data.skins[0].get(0, "img"), mv);
    CHECK(lv == mv);
}

TEST(doc_sequence_frame_image_names) {
    Document doc = makeDoc();
    Attachment a; a.name = "walk"; a.path = "walk"; a.seqCount = 3; a.seqStart = 1; a.seqDigits = 2;
    doc.imagesDir.clear();
    CHECK(doc.imagePathFor(a, 1).empty()); // no images folder: nothing resolves
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    std::string dir = normalizePath(std::string(tmp) + "osteon_seq_test");
    makeDirs(dir);
    for (const char* f : {"walk01.png", "walk02.png", "walk03.png"}) writeTextFile(joinPath(dir, f), "x");
    doc.imagesDir = dir;
    CHECK(fileName(doc.imagePathFor(a, 1)) == "walk02.png");
    CHECK(fileName(doc.imagePathFor(a, -1)) == "walk01.png"); // setup frame
}

TEST(doc_tree_drag_reorders_bones_and_slots) {
    Document doc = makeDoc();                      // root, arm; slot "s" on root
    BoneData leg; leg.name = "leg"; leg.parent = 0; leg.length = 40;
    doc.data.bones.push_back(leg);
    SlotData s2; s2.name = "s2"; s2.bone = 2;      // on the leg
    doc.data.slots.push_back(s2);
    doc.updatePose();
    CHECK(doc.moveBone(2, 1, false));              // leg before arm
    CHECK(doc.data.bones[1].name == "leg" && doc.data.bones[2].name == "arm");
    CHECK(doc.data.slots[1].bone == 1);            // s2 followed the leg to its new index
    CHECK(doc.isSelected({SelType::Bone, 1, ""})); // the moved bone stays selected, at its new index
    doc.undo();
    CHECK(doc.data.bones[1].name == "arm" && doc.data.bones[2].name == "leg");
    CHECK(!doc.moveBone(1, 1, true));              // not a move
    // Slots: s2 before s in the draw order, taking on s's bone (root).
    CHECK(doc.moveSlotNextTo(1, 0, false));
    CHECK(doc.data.slots[0].name == "s2" && doc.data.slots[1].name == "s");
    CHECK(doc.data.slots[0].bone == 0);
}

TEST(doc_rename_animation_and_unique_names) {
    Document doc = makeDoc();
    int a = doc.createAnimation("animation");
    int b = doc.createAnimation("animation");
    CHECK(doc.data.animations[a].name == "animation");
    CHECK(doc.data.animations[b].name != "animation"); // a second "animation" gets its own name
    doc.selection = {{SelType::Animation, b, ""}};
    CHECK(doc.renameSelection("walk"));
    CHECK(doc.data.animations[b].name == "walk");
    doc.selection = {{SelType::Animation, a, ""}};
    CHECK(!doc.renameSelection("walk"));               // already used
    CHECK(!doc.renameSelection(""));                   // empty
    CHECK(doc.data.animations[a].name == "animation");
    doc.undo();                                         // renames are undoable
    CHECK(doc.data.animations[b].name == "animation" || doc.data.animations[b].name != "walk");
}

TEST(doc_create_ik_rules) {
    Document doc = makeDoc(); // root(0), arm(1)
    int fore = doc.createBone(1, {60, 0}, {110, 0}, "fore");
    int hand = doc.createBone(fore, {110, 0}, {140, 0}, "hand");
    int root = 0, arm = doc.data.findBone("arm");
    fore = doc.data.findBone("fore"); hand = doc.data.findBone("hand");
    // Three bones, or two that are not parent and child, are refused.
    CHECK(doc.createIk("ik", {arm, fore, hand}, root) < 0);
    CHECK(doc.createIk("ik", {arm, hand}, root) < 0);
    // The target may not be a constrained bone or below it.
    CHECK(doc.createIk("ik", {arm}, hand) < 0);
    CHECK(doc.data.ik.empty());
    // Child listed first is reordered parent first.
    int i = doc.createIk("ik", {fore, arm}, root);
    CHECK(i >= 0);
    CHECK(doc.data.ik[i].bones[0] == arm && doc.data.ik[i].bones[1] == fore);
}

TEST(doc_create_ik_with_new_target_is_one_undo_step) {
    Document doc = makeDoc();
    int arm = doc.data.findBone("arm");
    size_t bonesBefore = doc.data.bones.size(), steps = doc.undoStack.size();
    Vec2 at{100, 40};
    int i = doc.createIk("ik", {arm}, -1, &at);
    CHECK(i >= 0);
    CHECK(doc.data.bones.size() == bonesBefore + 1);
    CHECK(doc.undoStack.size() == steps + 1);
    doc.updatePose();
    const Bone& t = doc.sk.bones[doc.data.ik[i].target];
    CHECK_NEAR(t.worldX, 100, 1e-3);
    CHECK_NEAR(t.worldY, 40, 1e-3);
    doc.undo();
    CHECK(doc.data.bones.size() == bonesBefore && doc.data.ik.empty());
}

TEST(doc_backup_restores_unsaved_work_and_marks_it_unsaved) {
    Document doc = makeDoc();
    doc.data.fps = 24;
    doc.dirty = true;
    std::string path = joinPath(appDataDir(), "recovery_test_backup.osteon"), err;
    CHECK(doc.writeBackup(path, err));
    CHECK(doc.dirty); // writing a backup does not count as saving
    CHECK(doc.projectPath.empty());
    Document other;
    other.newProject();
    CHECK(other.restoreBackup(path, "C:/work/rig.osteon", err));
    CHECK(other.data.bones.size() == doc.data.bones.size());
    CHECK_NEAR(other.data.fps, 24, 1e-6);
    CHECK(other.dirty);
    CHECK(other.projectPath == "C:/work/rig.osteon");
    CHECK(other.imagesDir.empty()); // not the folder the backup sits in
    std::remove(path.c_str());
}

TEST(doc_deleting_a_constraint_keeps_the_other_constraints_keys) {
    Document doc = makeDoc();
    int a = doc.createBone(0, {0, 0}, {40, 0}, "a"), t1 = doc.createBone(0, {0, 50}, {10, 50}, "t1"), t2 = doc.createBone(0, {0, 80}, {10, 80}, "t2");
    int ik0 = doc.createIk("first", {a}, t1), ik1 = doc.createIk("second", {a}, t2);
    CHECK(ik0 >= 0 && ik1 >= 0);
    int an = doc.createAnimation("anim");
    for (int target : {ik0, ik1}) {
        Timeline tl; tl.type = TimelineType::Ik; tl.target = target;
        Key k; k.time = 0; tl.keys.push_back(k);
        doc.data.animations[an].timelines.push_back(tl);
    }
    doc.selection = {{SelType::Ik, ik0, ""}};
    doc.deleteSelection();
    CHECK(doc.data.ik.size() == 1 && doc.data.ik[0].name == "second");
    int ikTimelines = 0;
    for (auto& tl : doc.data.animations[an].timelines)
        if (tl.type == TimelineType::Ik) { ikTimelines++; CHECK(tl.target == 0); }
    CHECK(ikTimelines == 1);
}

TEST(json_malformed_vertex_data_is_rejected_not_crashed) {
    const char* text = R"({"skeleton":{"spine":"4.2.00"},"bones":[{"name":"root"}],"slots":[{"name":"s","bone":"root","attachment":"m"}],
      "skins":[{"name":"default","attachments":{"s":{"m":{"type":"mesh","uvs":[0,0,1,0,1,1],"triangles":[0,1,9],"hull":3,
      "vertices":[3,0,1,2,1]}}}}]})";
    SkeletonData d;
    JsonResult r = readSkeletonJson(text, d);
    CHECK(r.ok);
    CHECK(!r.warnings.empty());
    Skeleton sk(d);
    sk.updateWorldTransform(Physics::None);
    const Attachment* a = d.skins[0].get(0, "m");
    CHECK(a != nullptr && a->triangles.empty() && a->vertices.size() == 6);
    std::vector<float> out(6);
    sk.worldVertices(sk.slots[0], *a, out);
    for (float f : out) CHECK(std::isfinite(f));
}

TEST(doc_large_skeleton_edits_undo_and_poses_stay_fast_and_finite) {
    Document doc;
    doc.newProject();
    for (int i = 0; i < 300; i++) doc.createBone(i == 0 ? 0 : i, {0.f, (float)i * 10}, {0.f, (float)i * 10 + 8}, "b" + std::to_string(i));
    for (int i = 0; i < 150; i++) doc.createSlot(1 + i, "s" + std::to_string(i));
    int an = doc.createAnimation("a");
    doc.setMode(Mode::Animate);
    doc.animation = an;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 200; i++) {
        doc.setTime((i % 20) * 0.05f);
        Document::LocalPose p = doc.bonePose(1 + i % 300);
        p.rotation += 1;
        doc.setBonePose(1 + i % 300, p, (int)Channel::Rotate);
        doc.updatePose();
    }
    for (int i = 0; i < 100; i++) doc.undo();
    for (int i = 0; i < 100; i++) doc.redo();
    doc.updatePose();
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    for (auto& b : doc.sk.bones) { CHECK(std::isfinite(b.a)); CHECK(std::isfinite(b.worldX)); }
    CHECK(ms < 5000);
}

TEST(doc_compensation_keeps_children_and_images_in_place) {
    Document doc = makeDoc(); // root(0), arm(1) child of root at x=10
    int hand = doc.createBone(1, {60, 0}, {90, 0}, "hand");
    doc.setMode(Mode::Setup);
    doc.addRegionAttachment(doc.createSlot(1, "img"), "none", {30, 0});
    auto worldOf = [&](int b) { Skeleton s(doc.data); s.updateWorldTransform(Physics::None); return Vec2{s.bones[b].worldX, s.bones[b].worldY}; };
    Vec2 handBefore = worldOf(hand);
    // Without compensation the hand moves with its parent.
    doc.boneCompensation = false;
    Document::LocalPose p = doc.bonePose(1); p.rotation += 30;
    doc.setBonePose(1, p, (int)Channel::Rotate);
    Vec2 moved = worldOf(hand);
    CHECK(std::fabs(moved.x - handBefore.x) + std::fabs(moved.y - handBefore.y) > 1);
    doc.data.bones[1].rotation = 0;
    // With compensation it stays put.
    doc.boneCompensation = true;
    p = doc.bonePose(1); p.rotation += 30;
    doc.setBonePose(1, p, (int)Channel::Rotate);
    Vec2 kept = worldOf(hand);
    CHECK_NEAR(kept.x, handBefore.x, 0.01f);
    CHECK_NEAR(kept.y, handBefore.y, 0.01f);
}

TEST(doc_unchanged_open_edit_does_not_swallow_undo) {
    Document doc = makeDoc();
    doc.checkpoint("Real change");
    doc.data.bones[1].x = 99;
    doc.beginEdit("Click only");   // gesture that changes nothing
    doc.undo();                    // must undo "Real change", not the empty edit
    CHECK(doc.data.bones[1].x != 99);
    CHECK(doc.canRedo());
}

TEST(doc_reparent_slot_keeps_image_world_size_under_scaled_bone) {
    Document doc = makeDoc();
    doc.data.bones[1].scaleX = doc.data.bones[1].scaleY = 2;
    doc.sk.build(doc.data);
    int slot = doc.createSlot(0, "img");
    doc.addRegionAttachment(slot, "none", {0, 0});
    auto size = [&] {
        Skeleton s(doc.data); s.updateWorldTransform(Physics::None);
        const Attachment* a = doc.data.defaultSkin().get(slot, "none");
        return a->scaleX * s.bones[doc.data.slots[slot].bone].worldScaleX();
    };
    float before = size();
    doc.updatePose();
    doc.reparentSlot(slot, 1);
    CHECK_NEAR(size(), before, 0.001f);
}

TEST(doc_update_pose_cost_on_large_skeleton) {
    Document doc;
    doc.newProject();
    for (int i = 0; i < 500; i++) doc.createBone(i == 0 ? 0 : i, {0.f, (float)i * 10}, {0.f, (float)i * 10 + 8}, "b" + std::to_string(i));
    doc.updatePose();
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 200; i++) doc.updatePose();
    double perPose = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 200;
    std::printf("      updatePose on 500 bones: %.3f ms\n", perPose);
    CHECK(perPose < 5.0); // a 60 fps frame is 16 ms
}

TEST(doc_split_bone_keeps_the_pose_and_moves_children) {
    Document doc = makeDoc(); // arm: length 50 at x=10
    int hand = doc.createBone(1, {60, 0}, {90, 0}, "hand");
    hand = doc.data.findBone("hand");
    auto world = [&](const char* n) { Skeleton s(doc.data); s.updateWorldTransform(Physics::None); const Bone& b = s.bones[doc.data.findBone(n)]; return Vec2{b.worldX, b.worldY}; };
    Vec2 before = world("hand");
    int nb = doc.splitBone(doc.data.findBone("arm"));
    CHECK(nb >= 0);
    CHECK_NEAR(doc.data.bones[doc.data.findBone("arm")].length, 25.f, 1e-4f);
    CHECK_NEAR(doc.data.bones[nb].length, 25.f, 1e-4f);
    CHECK(doc.data.bones[doc.data.findBone("hand")].parent == nb);
    Vec2 after = world("hand");
    CHECK_NEAR(after.x, before.x, 1e-3f); CHECK_NEAR(after.y, before.y, 1e-3f);
    doc.undo();
    CHECK(doc.data.findBone("arm2") < 0);
    CHECK(doc.data.bones[doc.data.findBone("hand")].parent == doc.data.findBone("arm"));
    (void)hand;
}

TEST(doc_slider_create_rename_delete_and_bone_removal) {
    Document doc = makeDoc();
    int src = doc.createBone(0, {0, 0}, {10, 0}, "src");
    int an = doc.createAnimation("swing");
    doc.data.animations[an].timelines.push_back([] { Timeline t; t.type = TimelineType::Rotate; t.target = 1; Key k; k.time = 1; t.keys.push_back(k); return t; }());
    int s = doc.createSlider("slider", an);
    CHECK(s == 0 && doc.data.sliders[0].animation == "swing");
    CHECK_NEAR(doc.data.sliders[0].frameTo, doc.data.fps, 1e-3f);
    doc.data.sliders[0].bone = src;
    // Renaming the animation follows into the slider.
    doc.selection = {{SelType::Animation, an, ""}};
    CHECK(doc.renameSelection("wave"));
    CHECK(doc.data.sliders[0].animation == "wave");
    // Deleting the source bone leaves the slider valid and on its own frame.
    doc.selection = {{SelType::Bone, doc.data.findBone("src"), ""}};
    doc.deleteSelection();
    CHECK(doc.data.sliders[0].bone == -1);
    doc.updatePose();
    // Deleting the animation clears the reference instead of dangling.
    doc.selection = {{SelType::Animation, doc.data.findAnimation("wave"), ""}};
    doc.deleteSelection();
    CHECK(doc.data.sliders[0].animation.empty());
    doc.updatePose();
    doc.selection = {{SelType::Slider, 0, ""}};
    doc.deleteSelection();
    CHECK(doc.data.sliders.empty());
    doc.undo();
    CHECK(doc.data.sliders.size() == 1);
}
