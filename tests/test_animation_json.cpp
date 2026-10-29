#include "Test.h"
#include "Animation.h"
#include "SkeletonJson.h"
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
