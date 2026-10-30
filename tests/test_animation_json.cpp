#include "Test.h"
#include "Animation.h"
#include "SkeletonJson.h"
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace osteon;

static SkeletonData twoBones() {
    SkeletonData d;
    BoneData root; root.name = "root";
    BoneData arm; arm.name = "arm"; arm.parent = 0; arm.rotation = 10; arm.x = 5; arm.length = 40;
    d.bones = {root, arm};
    SlotData s; s.name = "s"; s.bone = 1; s.attachment = "a";
    d.slots = {s};
    Attachment a; a.name = "a"; a.width = 10; a.height = 20;
    Attachment b = a; b.name = "b";
    d.defaultSkin().set(0, a);
    d.defaultSkin().set(0, b);
    return d;
}

TEST(linear_rotate_is_relative_to_setup) {
    SkeletonData d = twoBones();
    Animation an; an.name = "x";
    Timeline& tl = an.get(TimelineType::Rotate, 1);
    tl.setKey(0).v[0] = 0;
    tl.setKey(1).v[0] = 90;
    d.animations.push_back(an);
    Skeleton s(d);
    applyAnimation(d.animations[0], s, 0, 0.5f, false, nullptr, 1, MixBlend::Setup);
    CHECK_NEAR(s.bones[1].rotation, 55, 1e-4);
}

TEST(stepped_and_bezier_curves) {
    Timeline tl; tl.type = TimelineType::TranslateX;
    Key& k0 = tl.setKey(0); k0.v[0] = 0; k0.curve = CurveType::Stepped;
    tl.setKey(1).v[0] = 10;
    CHECK_NEAR(timelineValue(tl, 0, 0.99f), 0, 1e-6);
    CHECK_NEAR(timelineValue(tl, 0, 1.0f), 10, 1e-6);
    // Bezier with handles on the straight line equals linear.
    tl.keys[0].curve = CurveType::Bezier;
    tl.keys[0].bezier = {1.0f / 3, 10.0f / 3, 2.0f / 3, 20.0f / 3};
    CHECK_NEAR(timelineValue(tl, 0, 0.25f), 2.5, 1e-3);
    // Ease-in: both handles low => value lags behind linear.
    tl.keys[0].bezier = {0.5f, 0, 0.75f, 0};
    CHECK(timelineValue(tl, 0, 0.5f) < 5);
}

TEST(scale_is_multiplicative_and_mix_alpha) {
    SkeletonData d = twoBones();
    d.bones[1].scaleX = 2;
    Animation an; an.name = "x";
    Timeline& tl = an.get(TimelineType::ScaleX, 1);
    tl.setKey(0).v[0] = 3;
    d.animations.push_back(an);
    Skeleton s(d);
    applyAnimation(d.animations[0], s, 0, 0, false, nullptr, 1, MixBlend::Setup);
    CHECK_NEAR(s.bones[1].scaleX, 6, 1e-5);
    s.setToSetupPose();
    applyAnimation(d.animations[0], s, 0, 0, false, nullptr, 0.5f, MixBlend::Setup);
    CHECK_NEAR(s.bones[1].scaleX, 4, 1e-5);
}

TEST(attachment_timeline_switches) {
    SkeletonData d = twoBones();
    Animation an; an.name = "x";
    Timeline& tl = an.get(TimelineType::Attachment, 0);
    tl.setKey(0.5f).str = "b";
    tl.setKey(1.0f).str = "";
    d.animations.push_back(an);
    Skeleton s(d);
    applyAnimation(d.animations[0], s, 0, 0.2f, false, nullptr, 1, MixBlend::Setup);
    CHECK(s.slots[0].attachment == "a");
    applyAnimation(d.animations[0], s, 0, 0.7f, false, nullptr, 1, MixBlend::Setup);
    CHECK(s.slots[0].attachment == "b");
    applyAnimation(d.animations[0], s, 0, 1.0f, false, nullptr, 1, MixBlend::Setup);
    CHECK(s.slots[0].attachment.empty());
}

TEST(events_fire_once_across_loop) {
    SkeletonData d = twoBones();
    EventData e; e.name = "step"; e.intValue = 7;
    d.events.push_back(e);
    Animation an; an.name = "x";
    Timeline& tl = an.get(TimelineType::Event, -1);
    tl.setKey(0.25f).str = "step";
    an.get(TimelineType::Rotate, 1).setKey(1.0f);
    d.animations.push_back(an);
    Skeleton s(d);
    std::vector<FiredEvent> ev;
    applyAnimation(d.animations[0], s, 0.1f, 0.2f, true, &ev, 1, MixBlend::Setup);
    CHECK(ev.empty());
    applyAnimation(d.animations[0], s, 0.2f, 0.3f, true, &ev, 1, MixBlend::Setup);
    CHECK(ev.size() == 1 && ev[0].intValue == 7);
    ev.clear();
    applyAnimation(d.animations[0], s, 0.9f, 1.3f, true, &ev, 1, MixBlend::Setup); // wraps past 1.0 -> 0.3
    CHECK(ev.size() == 1);
}

TEST(draw_order_timeline) {
    SkeletonData d = twoBones();
    SlotData s2; s2.name = "s2"; s2.bone = 0;
    d.slots.push_back(s2);
    Animation an; an.name = "x";
    an.get(TimelineType::DrawOrder, -1).setKey(0).drawOrder = {1, 0};
    d.animations.push_back(an);
    Skeleton s(d);
    applyAnimation(d.animations[0], s, 0, 0, false, nullptr, 1, MixBlend::Setup);
    CHECK(s.drawOrder[0] == 1 && s.drawOrder[1] == 0);
}

TEST(animation_state_crossfade) {
    SkeletonData d = twoBones();
    Animation a; a.name = "a"; a.get(TimelineType::Rotate, 1).setKey(0).v[0] = 0;
    Animation b; b.name = "b"; b.get(TimelineType::Rotate, 1).setKey(0).v[0] = 100;
    d.animations = {a, b};
    Skeleton s(d);
    AnimationState st(&d);
    st.defaultMix = 1;
    st.setAnimation(0, 0, true);
    st.update(0.1f);
    st.setAnimation(0, 1, true);
    st.update(0.5f);
    s.setToSetupPose();
    st.apply(s);
    CHECK_NEAR(s.bones[1].rotation, 10 + 50, 1e-3); // halfway through a 1s mix
    st.update(0.6f);
    s.setToSetupPose();
    st.apply(s);
    CHECK_NEAR(s.bones[1].rotation, 110, 1e-3);
}

static std::string readFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

TEST(json_reads_user_photoshop_export) {
    std::string text = readFile("C:/Users/Div/Desktop/images/charachter.json");
    if (text.empty()) { std::printf("    (skipped: user file not found)\n"); return; }
    SkeletonData d;
    JsonResult r = readSkeletonJson(text, d);
    CHECK(r.ok);
    CHECK(d.bones.size() == 1);
    CHECK(d.slots.size() == 18);
    CHECK(d.imagesPath == "C:/Users/Div/Desktop/images/");
    const Attachment* head = d.findAttachment(0, d.findSlot("Head"), "Head");
    CHECK(head != nullptr);
    if (head) { CHECK_NEAR(head->width, 163, 1e-4); CHECK_NEAR(head->x, 1021.5, 1e-4); }
    CHECK(d.findAnimation("animation") >= 0);
}

TEST(json_roundtrip_preserves_everything) {
    SkeletonData d = twoBones();
    d.bones[1].inherit = Inherit::NoScale;
    d.slots[0].blend = BlendMode::Additive;
    d.slots[0].hasDark = true;
    d.slots[0].dark = Color(0.2f, 0, 0, 1);
    Attachment m; m.type = AttachmentType::Mesh; m.name = "m";
    m.uvs = {0, 1, 0, 0, 1, 0}; m.triangles = {0, 1, 2}; m.hullLength = 3; m.worldVerticesLength = 6;
    m.bones = {1, 0, 2, 0, 1, 1, 1}; // v0: root; v1: root+arm; v2: arm
    m.vertices = {0, 0, 1, 1, 1, 0.5f, 2, 2, 0.5f, 3, 3, 1};
    m.edges = {0, 1, 1, 2, 2, 0};
    d.defaultSkin().set(0, m);
    IkConstraintData ik; ik.name = "ik"; ik.bones = {1}; ik.target = 0; ik.bendDirection = -1;
    d.ik.push_back(ik);
    EventData e; e.name = "ev"; e.stringValue = "hi";
    d.events.push_back(e);
    Animation an; an.name = "walk";
    Timeline& tr = an.get(TimelineType::Translate, 1);
    const std::vector<float> bez = {0.2f, 1, 0.4f, 2, 0.2f, 2, 0.4f, 3};
    { Key& k = tr.setKey(0); k.v[0] = 1; k.v[1] = 2; k.curve = CurveType::Bezier; k.bezier = bez; }
    { Key& k2 = tr.setKey(1); k2.v[0] = 3; k2.v[1] = 4; }
    Timeline& df = an.get(TimelineType::Deform, 0, "default", "m");
    df.setKey(0.5f).deform = {0, 0, 1, 1, 0, 0, 0, 0};
    an.get(TimelineType::Event, -1).setKey(0.25f).str = "ev";
    an.get(TimelineType::DrawOrder, -1).setKey(0.1f);
    Timeline& col = an.get(TimelineType::RGBA, 0);
    Key& ck = col.setKey(0); ck.v[0] = 1; ck.v[1] = 0.5f; ck.v[2] = 0; ck.v[3] = 1;
    d.animations.push_back(an);

    std::string text = writeSkeletonJson(d);
    SkeletonData r;
    JsonResult res = readSkeletonJson(text, r);
    CHECK(res.ok);
    CHECK(r.bones[1].inherit == Inherit::NoScale);
    CHECK(r.slots[0].blend == BlendMode::Additive && r.slots[0].hasDark);
    const Attachment* rm = r.findAttachment(0, 0, "m");
    CHECK(rm && rm->type == AttachmentType::Mesh && rm->bones == m.bones && rm->vertices == m.vertices && rm->edges == m.edges);
    CHECK(r.ik.size() == 1 && r.ik[0].bendDirection == -1);
    CHECK(r.animations.size() == 1);
    const Animation& ra = r.animations[0];
    const Timeline* rt = const_cast<Animation&>(ra).find(TimelineType::Translate, 1);
    CHECK(rt && rt->keys.size() == 2 && rt->keys[0].curve == CurveType::Bezier && rt->keys[0].bezier == bez);
    const Timeline* rd = const_cast<Animation&>(ra).find(TimelineType::Deform, 0, "default", "m");
    CHECK(rd && rd->keys.size() == 1 && rd->keys[0].deform == std::vector<float>({0, 0, 1, 1, 0, 0, 0, 0}));
    // Writing again yields identical text (stable round trip).
    CHECK(writeSkeletonJson(r) == text);
}

TEST(json_draw_order_offsets_roundtrip) {
    SkeletonData d;
    BoneData root; root.name = "root"; d.bones = {root};
    for (int i = 0; i < 5; i++) { SlotData s; s.name = "s" + std::to_string(i); d.slots.push_back(s); }
    Animation an; an.name = "x";
    an.get(TimelineType::DrawOrder, -1).setKey(0).drawOrder = {3, 0, 4, 1, 2};
    d.animations.push_back(an);
    SkeletonData r;
    CHECK(readSkeletonJson(writeSkeletonJson(d), r).ok);
    CHECK(r.animations[0].timelines[0].keys[0].drawOrder == std::vector<int>({3, 0, 4, 1, 2}));
}

TEST(json_reads_spine38_curves_and_names) {
    const char* text = R"({"skeleton":{"spine":"3.8.99"},
      "bones":[{"name":"root"},{"name":"b","parent":"root","transform":"noScale"}],
      "slots":[{"name":"s","bone":"b"}],
      "skins":[{"name":"default","attachments":{"s":{"r":{"width":2,"height":2}}}}],
      "animations":{"a":{"bones":{"b":{"rotate":[{"angle":0,"curve":0.25,"c2":0,"c3":0.75,"c4":1},{"time":1,"angle":40}]}}}}})";
    SkeletonData d;
    JsonResult r = readSkeletonJson(text, d);
    CHECK(r.ok);
    CHECK(d.bones[1].inherit == Inherit::NoScale);
    const Timeline& tl = d.animations[0].timelines[0];
    CHECK(tl.keys[0].curve == CurveType::Bezier && tl.keys[0].bezier.size() == 4);
    CHECK_NEAR(tl.keys[0].bezier[0], 0.25, 1e-6);
    CHECK_NEAR(tl.keys[0].bezier[3], 40, 1e-5);
    CHECK_NEAR(timelineValue(tl, 0, 0.5f), 20, 1e-3); // symmetric ease is 50% at midpoint
}

// Writes a feature-rich skeleton using the user's images, for loading into Spine / runtimes by hand.
// Runs only when OSTEON_EXPORT_SAMPLE names an output .json path.
#include "Geometry.h"
TEST(json_export_compat_sample) {
    const char* out = std::getenv("OSTEON_EXPORT_SAMPLE");
    if (!out) return;
    SkeletonData d;
    d.imagesPath = "C:/Users/Div/Desktop/images/";
    BoneData root; root.name = "root";
    BoneData hip; hip.name = "hip"; hip.parent = 0; hip.x = 989; hip.y = -1000; hip.rotation = 90; hip.length = 180;
    BoneData chest; chest.name = "chest"; chest.parent = 1; chest.x = 180; chest.length = 160;
    BoneData neck; neck.name = "neck"; neck.parent = 2; neck.x = 160; neck.length = 120;
    BoneData target; target.name = "look-target"; target.parent = 0; target.x = 1100; target.y = -420;
    d.bones = {root, hip, chest, neck, target};
    SlotData shirt; shirt.name = "shirt"; shirt.bone = 1; shirt.attachment = "shirt";
    SlotData head; head.name = "Head"; head.bone = 3; head.attachment = "Head"; head.blend = BlendMode::Normal;
    SlotData eye; eye.name = "left eye"; eye.bone = 3; eye.attachment = "left eye"; eye.color = Color(1, 1, 1, 1);
    d.slots = {shirt, head, eye};
    Skeleton sk(d);
    sk.updateWorldTransform(Physics::None);
    // Shirt as a mesh weighted to hip+chest.
    Attachment m; m.type = AttachmentType::Mesh; m.name = "shirt"; m.width = 276; m.height = 517;
    {
        Attachment r; r.width = 276; r.height = 517;
        const Bone& b = sk.bones[1];
        Vec2 l = b.worldToLocal({989, -825.5f});
        r.x = l.x; r.y = l.y; r.rotation = -90;
        float c[8]; r.regionCorners(c);
        m.vertices.assign(c, c + 8); m.worldVerticesLength = 8;
        m.uvs = {0, 1, 0, 0, 1, 0, 1, 1}; m.hullLength = 4; m.triangles = triangulateMesh(m.vertices, 4);
        m.edges = {0, 1, 1, 2, 2, 3, 3, 0};
        bindWeights(m, sk, 1, {1, 2});
    }
    d.defaultSkin().set(0, m);
    auto region = [&](int slot, const char* name, float wx, float wy, float w, float h) {
        Attachment a; a.name = name; a.width = w; a.height = h;
        const Bone& b = sk.bones[d.slots[slot].bone];
        Vec2 l = b.worldToLocal({wx, wy}); a.x = l.x; a.y = l.y; a.rotation = -b.worldRotationX();
        d.defaultSkin().set(slot, a);
    };
    region(1, "Head", 1021.5f, -534, 163, 284);
    region(2, "left eye", 1039, -479, 20, 24);
    IkConstraintData ik; ik.name = "look"; ik.bones = {3}; ik.target = 4; ik.mix = 0.5f;
    d.ik.push_back(ik);
    TransformConstraintData tc; tc.name = "follow"; tc.bones = {2}; tc.target = 1; tc.mixRotate = 0.3f; tc.mixX = tc.mixY = tc.mixScaleX = tc.mixScaleY = tc.mixShearY = 0; tc.order = 1;
    d.transforms.push_back(tc);
    EventData ev; ev.name = "blink"; ev.intValue = 2; d.events.push_back(ev);
    Animation an; an.name = "idle";
    Timeline& rot = an.get(TimelineType::Rotate, 2);
    { Key& k = rot.setKey(0); k.v[0] = 0; k.curve = CurveType::Bezier; k.bezier = {0.25f, 0, 0.5f, 8}; }
    rot.setKey(0.75f).v[0] = 8;
    { Key& k = rot.keys[1]; k.curve = CurveType::Bezier; k.bezier = {1.0f, 8, 1.25f, 0}; }
    rot.setKey(1.5f).v[0] = 0;
    { Timeline& tr = an.get(TimelineType::Translate, 4); Key& k = tr.setKey(0); k.v[0] = 0; k.v[1] = 0; Key& k2 = tr.setKey(1.5f); k2.v[0] = -150; k2.v[1] = 20; }
    { Timeline& c = an.get(TimelineType::RGBA, 2); Key& k = c.setKey(0.6f); k.v[0] = k.v[1] = k.v[2] = k.v[3] = 1; k.curve = CurveType::Stepped; Key& k2 = c.setKey(0.7f); k2.v[0] = k2.v[1] = k2.v[2] = 1; k2.v[3] = 0; Key& k3 = c.setKey(0.8f); k3.v[0] = k3.v[1] = k3.v[2] = k3.v[3] = 1; }
    an.get(TimelineType::Event, -1).setKey(0.65f).str = "blink";
    { Timeline& df = an.get(TimelineType::Deform, 0, "default", "shirt"); df.setKey(0).deform.assign(m.deformLength(), 0.0f); Key& k = df.setKey(0.75f); k.deform.assign(m.deformLength(), 0.0f); for (size_t i = 0; i < k.deform.size(); i += 2) k.deform[i] = 6; df.setKey(1.5f).deform.assign(m.deformLength(), 0.0f); }
    an.get(TimelineType::DrawOrder, -1).setKey(0.5f).drawOrder = {1, 0, 2};
    an.get(TimelineType::DrawOrder, -1).setKey(1.0f);
    d.animations.push_back(an);
    std::ofstream(out) << writeSkeletonJson(d);
    JsonWriteOptions o43; o43.version = "4.3.26"; // for importing into the Spine 4.3 editor
    std::ofstream(std::string(out) + ".43.json") << writeSkeletonJson(d, o43);
    SkeletonData back;
    CHECK(readSkeletonJson(writeSkeletonJson(d), back).ok);
}

TEST(json_weighted_influences_written_sorted_with_deform) {
    SkeletonData d;
    BoneData root; root.name = "root";
    BoneData a; a.name = "a"; a.parent = 0;
    BoneData b; b.name = "b"; b.parent = 0;
    d.bones = {root, a, b};
    SlotData s; s.name = "s"; s.bone = 0; d.slots = {s};
    Attachment m; m.type = AttachmentType::Mesh; m.name = "m";
    m.uvs = {0, 0, 1, 0, 0, 1}; m.triangles = {0, 1, 2}; m.hullLength = 3; m.worldVerticesLength = 6;
    // Vertex 1 stores its influences as (b, a): must be written as (a, b).
    m.bones = {1, 1, 2, 2, 1, 1, 2};
    m.vertices = {0, 0, 1, 20, 21, 0.25f, 10, 11, 0.75f, 5, 5, 1};
    d.defaultSkin().set(0, m);
    Animation an; an.name = "x";
    an.get(TimelineType::Deform, 0, "default", "m").setKey(0).deform = {0, 0, 2, 2, 1, 1, 0, 0};
    d.animations.push_back(an);
    SkeletonData r;
    CHECK(readSkeletonJson(writeSkeletonJson(d), r).ok);
    const Attachment* rm = r.findAttachment(0, 0, "m");
    CHECK(rm && rm->bones == std::vector<int>({1, 1, 2, 1, 2, 1, 2}));
    if (rm) { CHECK_NEAR(rm->vertices[3], 10, 1e-6); CHECK_NEAR(rm->vertices[6], 20, 1e-6); }
    // Deform offsets moved with their influences.
    CHECK(r.animations[0].timelines[0].keys[0].deform == std::vector<float>({0, 0, 1, 1, 2, 2, 0, 0}));
}

TEST(json_spine43_constraints_roundtrip) {
    SkeletonData d;
    BoneData root; root.name = "root";
    BoneData a; a.name = "a"; a.parent = 0; a.length = 10;
    BoneData t; t.name = "t"; t.parent = 0;
    d.bones = {root, a, t};
    IkConstraintData ik; ik.name = "ik"; ik.bones = {1}; ik.target = 2; ik.order = 1; ik.mix = 0.5f;
    TransformConstraintData tc; tc.name = "tc"; tc.bones = {1}; tc.target = 2; tc.order = 0;
    tc.mixX = tc.mixY = tc.mixScaleX = tc.mixScaleY = tc.mixShearY = 0; tc.mixRotate = 0.4f; tc.offsetRotation = 30;
    d.ik.push_back(ik); d.transforms.push_back(tc);
    JsonWriteOptions opt; opt.version = "4.3.26";
    std::string text = writeSkeletonJson(d, opt);
    CHECK(text.find("\"constraints\"") != std::string::npos);
    CHECK(text.find("\"ik\": [") == std::string::npos);
    SkeletonData r;
    CHECK(readSkeletonJson(text, r).ok);
    CHECK(r.ik.size() == 1 && r.transforms.size() == 1);
    CHECK(r.transforms[0].order < r.ik[0].order); // array order preserved as evaluation order
    CHECK_NEAR(r.ik[0].mix, 0.5, 1e-6);
    CHECK_NEAR(r.transforms[0].mixRotate, 0.4, 1e-6);
    CHECK_NEAR(r.transforms[0].mixX, 0, 1e-6);
    CHECK_NEAR(r.transforms[0].offsetRotation, 30, 1e-6);
    CHECK(r.transforms[0].target == 2);
}

TEST(json_reads_public_spine43_example) {
    // Esoteric's public spineboy-pro 4.3 export, if downloaded for verification (skipped otherwise).
    std::string text = readFile((std::string(std::getenv("TEMP") ? std::getenv("TEMP") : "") + "/claude/ref/sb.json").c_str());
    if (text.empty()) { std::printf("    (skipped: reference file not present)\n"); return; }
    SkeletonData d;
    JsonResult r = readSkeletonJson(text, d);
    CHECK(r.ok);
    CHECK(d.bones.size() > 50);
    CHECK(d.ik.size() >= 6 && d.transforms.size() >= 5);
    CHECK(d.findAnimation("walk") >= 0 && d.findAnimation("run") >= 0);
    // Every animation evaluates without touching invalid indices.
    Skeleton s(d);
    for (auto& a : d.animations) {
        s.setToSetupPose();
        applyAnimation(a, s, 0, a.duration() / 2, true, nullptr, 1, MixBlend::Setup);
        s.updateWorldTransform(Physics::Pose);
    }
    float x0, y0, x1, y1;
    CHECK(s.bounds(x0, y0, x1, y1));
}

TEST(sequence_timeline_modes) {
    SkeletonData d;
    BoneData root; root.name = "root"; d.bones = {root};
    SlotData s; s.name = "s"; s.attachment = "walk"; d.slots = {s};
    Attachment a; a.name = "walk"; a.seqCount = 4; a.seqDigits = 2; a.seqStart = 1;
    d.defaultSkin().set(0, a);
    auto at = [&](const char* mode, float t) {
        Animation an; an.name = "x";
        Key& k = an.get(TimelineType::Sequence, 0, "default", "walk").setKey(0);
        k.str = mode; k.v[0] = 0.1f; k.intValue = 0;
        Skeleton sk(d);
        applyAnimation(an, sk, t, t, false, nullptr, 1, MixBlend::Setup);
        return sk.slots[0].sequenceIndex;
    };
    CHECK(at("hold", 0.35f) == 0);
    CHECK(at("once", 0.25f) == 2);
    CHECK(at("once", 0.95f) == 3);
    CHECK(at("loop", 0.55f) == 1);
    CHECK(at("pingpong", 0.45f) == 2);   // 0 1 2 3 2 1 0 ... -> index 4 maps to 2
    CHECK(at("loopReverse", 0.05f) == 3);
    // Round trip through JSON.
    Animation an; an.name = "x";
    Key& k = an.get(TimelineType::Sequence, 0, "default", "walk").setKey(0.5f);
    k.str = "loop"; k.v[0] = 1 / 12.0f; k.intValue = 2;
    d.animations.push_back(an);
    SkeletonData r;
    CHECK(readSkeletonJson(writeSkeletonJson(d), r).ok);
    const Attachment* ra = r.findAttachment(0, 0, "walk");
    CHECK(ra && ra->seqCount == 4 && ra->seqDigits == 2);
    CHECK(r.animations[0].timelines.size() == 1 && r.animations[0].timelines[0].keys[0].str == "loop");
}

TEST(json_bone_icon_roundtrips) {
    SkeletonData d;
    BoneData root; root.name = "root";
    BoneData a; a.name = "a"; a.parent = 0; a.icon = "star";
    BoneData b; b.name = "b"; b.parent = 0; // default icon: nothing written
    d.bones = {root, a, b};
    std::string text = writeSkeletonJson(d);
    SkeletonData r;
    CHECK(readSkeletonJson(text, r).ok);
    CHECK(r.bones[1].icon == "star");
    CHECK(r.bones[2].icon.empty());
    CHECK(text.find("\"icon\"") != std::string::npos);
}
