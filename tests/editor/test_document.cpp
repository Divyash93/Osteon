// Editor document behaviour: undo/redo, Setup vs Animate editing, keying, structural edits.
#include "../Test.h"
#include "Document.h"
#include "Util.h"

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
