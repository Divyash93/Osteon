// Pose-mode behaviour of a character rig: every image follows its own bone, rotations happen about the
// bone's joint, descendants inherit, and unrelated images never move.
#include "../Test.h"
#include "Document.h"
#include "Util.h"
#include <cmath>
#include <map>

using namespace osteon;

namespace {

// root
//  |- torso (up)            image "torso"
//  |   |- head              image "head"       (origin = the neck joint)
//  |   `- upperArm          image "upperArm"
//  |        `- forearm      image "forearm"
//  |             `- hand    image "hand"
//  `- thigh (down)          image "thigh"
//       `- shin            image "shin"
struct Rig {
    Document doc;
    std::map<std::string, int> bone, slot;
};

static Rig makeRig() {
    Rig r;
    r.doc.newProject();
    auto addBone = [&](const char* name, const char* parent, float x, float y, float rot, float len) {
        BoneData b;
        b.name = name;
        b.parent = std::string(parent).empty() ? -1 : r.bone[parent];
        b.x = x; b.y = y; b.rotation = rot; b.length = len;
        r.doc.data.bones.push_back(b);
        r.bone[name] = (int)r.doc.data.bones.size() - 1;
    };
    // The project already holds a root bone at index 0.
    r.bone["root"] = 0;
    addBone("torso", "root", 0, 100, 90, 100);
    addBone("head", "torso", 100, 0, 0, 50);
    addBone("upperArm", "torso", 90, 20, 0, 60);
    addBone("forearm", "upperArm", 60, 0, 0, 50);
    addBone("hand", "forearm", 50, 0, 0, 20);
    addBone("thigh", "root", -20, 100, -90, 80);
    addBone("shin", "thigh", 80, 0, 0, 70);
    for (auto& [name, bi] : r.bone) {
        if (name == "root") continue;
        SlotData s;
        s.name = name;
        s.bone = bi;
        s.attachment = name;
        r.doc.data.slots.push_back(s);
        int si = (int)r.doc.data.slots.size() - 1;
        r.slot[name] = si;
        Attachment a; // spans from the bone's joint to its tip: the joint is the image's left edge
        a.name = name;
        a.x = r.doc.data.bones[bi].length / 2;
        a.y = 0;
        a.width = r.doc.data.bones[bi].length;
        a.height = 24;
        r.doc.data.defaultSkin().set(si, a);
    }
    r.doc.updatePose();
    return r;
}

static std::map<std::string, std::vector<float>> imagePositions(Rig& r) {
    std::map<std::string, std::vector<float>> out;
    r.doc.updatePose();
    for (auto& [name, si] : r.slot) {
        const Attachment* a = r.doc.data.defaultSkin().get(si, name);
        std::vector<float> v;
        r.doc.sk.worldVertices(r.doc.sk.slots[si], *a, v);
        out[name] = v;
    }
    return out;
}

static bool same(const std::vector<float>& a, const std::vector<float>& b, float tol = 1e-3f) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) if (std::fabs(a[i] - b[i]) > tol) return false;
    return true;
}

// Rotates a point set about `pivot` by `deg` (counter-clockwise, y up).
static std::vector<float> rotatedAbout(const std::vector<float>& v, Vec2 pivot, float deg) {
    float c = std::cos(deg * 3.14159265358979f / 180), s = std::sin(deg * 3.14159265358979f / 180);
    std::vector<float> out(v.size());
    for (size_t i = 0; i + 1 < v.size(); i += 2) {
        float x = v[i] - pivot.x, y = v[i + 1] - pivot.y;
        out[i] = pivot.x + c * x - s * y;
        out[i + 1] = pivot.y + s * x + c * y;
    }
    return out;
}

// Rotates `boneName` by `deg` and checks: its image and every descendant's image turn about the bone's joint;
// every other image stays exactly where it was.
static void checkRotation(Rig& r, const char* boneName, float deg) {
    auto before = imagePositions(r);
    int bi = r.bone[boneName];
    Vec2 joint(r.doc.sk.bones[bi].worldX, r.doc.sk.bones[bi].worldY);
    Document::LocalPose p = r.doc.bonePose(bi);
    p.rotation += deg;
    r.doc.setBonePose(bi, p, (int)Channel::Rotate);
    auto after = imagePositions(r);
    for (auto& [name, si] : r.slot) {
        bool follows = r.doc.data.isDescendant(r.doc.data.slots[si].bone, bi) || r.doc.data.slots[si].bone == bi;
        if (follows) CHECK(same(after[name], rotatedAbout(before[name], joint, deg)));
        else CHECK(same(after[name], before[name]));
    }
}

// The Pose tool: grab the bone's tip and sweep the mouse `deg` about its joint. Same expectations as a rotation.
static void checkPoseDrag(Rig& r, const char* boneName, float deg) {
    auto before = imagePositions(r);
    int bi = r.bone[boneName];
    const Bone& b = r.doc.sk.bones[bi];
    Vec2 joint(b.worldX, b.worldY);
    Vec2 grab = b.localToWorld({r.doc.data.bones[bi].length, 0});
    float c = std::cos(deg * 3.14159265358979f / 180), s = std::sin(deg * 3.14159265358979f / 180);
    Vec2 q = grab - joint;
    Vec2 mouse = joint + Vec2(c * q.x - s * q.y, s * q.x + c * q.y);
    r.doc.poseDrag(bi, r.doc.bonePose(bi), joint, grab, mouse);
    auto after = imagePositions(r);
    for (auto& [name, si] : r.slot) {
        bool follows = r.doc.data.isDescendant(r.doc.data.slots[si].bone, bi) || r.doc.data.slots[si].bone == bi;
        if (follows) CHECK(same(after[name], rotatedAbout(before[name], joint, deg)));
        else CHECK(same(after[name], before[name]));
    }
}

static void checkMove(Rig& r, const char* boneName, Vec2 delta) {
    auto before = imagePositions(r);
    int bi = r.bone[boneName];
    const WorldMatrix& pm = r.doc.sk.parentMatrix(bi);
    Document::LocalPose p = r.doc.bonePose(bi);
    Vec2 target(r.doc.sk.bones[bi].worldX + delta.x, r.doc.sk.bones[bi].worldY + delta.y);
    Vec2 l = r.doc.sk.bones[bi].worldToParent(r.doc.sk, target);
    (void)pm;
    p.x = l.x; p.y = l.y;
    r.doc.setBonePose(bi, p, (int)Channel::Translate);
    auto after = imagePositions(r);
    for (auto& [name, si] : r.slot) {
        bool follows = r.doc.data.isDescendant(r.doc.data.slots[si].bone, bi) || r.doc.data.slots[si].bone == bi;
        std::vector<float> expect = before[name];
        if (follows) for (size_t i = 0; i + 1 < expect.size(); i += 2) { expect[i] += delta.x; expect[i + 1] += delta.y; }
        CHECK(same(after[name], expect));
    }
}

} // namespace

TEST(pose_rig_images_start_where_they_were_authored) {
    Rig r = makeRig();
    auto img = imagePositions(r);
    // The head image spans the 50-unit head bone starting at the neck joint (world 0,200): corners at x = 0 +- 12.
    const auto& h = img["head"];
    CHECK(h.size() == 8);
    float minY = 1e9f, maxY = -1e9f;
    for (size_t i = 1; i < h.size(); i += 2) { minY = std::min(minY, h[i]); maxY = std::max(maxY, h[i]); }
    CHECK_NEAR(minY, 200, 1e-3);
    CHECK_NEAR(maxY, 250, 1e-3);
}

TEST(pose_rotate_upper_arm_moves_only_the_arm_chain) {
    Rig r = makeRig();
    checkRotation(r, "upperArm", 35);
}

TEST(pose_rotate_forearm_moves_forearm_and_hand) {
    Rig r = makeRig();
    checkRotation(r, "forearm", -50);
}

TEST(pose_rotate_head_turns_about_the_neck) {
    Rig r = makeRig();
    checkRotation(r, "head", 40);
}

TEST(pose_rotate_torso_carries_its_whole_hierarchy) {
    Rig r = makeRig();
    checkRotation(r, "torso", 25);
}

TEST(pose_rotate_thigh_moves_thigh_and_shin_only) {
    Rig r = makeRig();
    checkRotation(r, "thigh", 20);
}

TEST(pose_tool_turns_the_dragged_bone_not_its_parent) {
    Rig r = makeRig();
    checkPoseDrag(r, "upperArm", 35);  // parent is the torso: the torso image must stay put
    checkPoseDrag(r, "forearm", -50);  // forearm + hand follow, upper arm stays
    checkPoseDrag(r, "head", 40);      // turns about the neck joint
    checkPoseDrag(r, "hand", 25);
}

TEST(pose_tool_on_a_bone_parented_to_the_root_rotates_it) {
    // These bones hang directly off the root. The tool used to translate them instead of rotating.
    Rig r = makeRig();
    int torso = r.bone["torso"], thigh = r.bone["thigh"];
    float torsoX = r.doc.sk.bones[torso].worldX, thighX = r.doc.sk.bones[thigh].worldX;
    checkPoseDrag(r, "torso", 25);
    checkPoseDrag(r, "thigh", -15);
    CHECK_NEAR(r.doc.sk.bones[torso].worldX, torsoX, 1e-3); // rotated about its joint: the joint did not move
    CHECK_NEAR(r.doc.sk.bones[thigh].worldX, thighX, 1e-3);
}

TEST(pose_tool_in_animate_mode_also_turns_the_dragged_bone) {
    Rig r = makeRig();
    r.doc.createAnimation("pose");
    r.doc.setMode(Mode::Animate);
    r.doc.autoKey = false;
    checkPoseDrag(r, "upperArm", 30);
    checkPoseDrag(r, "forearm", 20);
}

TEST(pose_move_individual_bones_without_disturbing_others) {
    Rig r = makeRig();
    checkMove(r, "hand", {12, -7});
    checkMove(r, "head", {-5, 9});
    checkMove(r, "thigh", {3, 4});
    checkMove(r, "upperArm", {-8, 2});
}

TEST(pose_in_animate_mode_matches_setup_mode) {
    Rig r = makeRig();
    r.doc.createAnimation("pose");
    r.doc.setMode(Mode::Animate);
    r.doc.autoKey = false;
    checkRotation(r, "upperArm", 35);   // pending pose, shown but not keyed
    checkRotation(r, "forearm", -50);
    r.doc.autoKey = true;
    checkRotation(r, "head", 40);       // keyed
    r.doc.setMode(Mode::Setup);
    auto setupPose = imagePositions(r);
    // Back in Setup mode nothing from the animation leaks into the setup pose.
    Rig fresh = makeRig();
    auto original = imagePositions(fresh);
    for (auto& [name, v] : original) CHECK(same(setupPose[name], v));
}

TEST(pose_entering_animate_mode_does_not_jump) {
    Rig r = makeRig();
    auto before = imagePositions(r);
    r.doc.createAnimation("pose");
    r.doc.setMode(Mode::Animate);
    auto after = imagePositions(r);
    for (auto& [name, v] : before) CHECK(same(after[name], v));
    r.doc.setMode(Mode::Setup);
    auto back = imagePositions(r);
    for (auto& [name, v] : before) CHECK(same(back[name], v));
}
