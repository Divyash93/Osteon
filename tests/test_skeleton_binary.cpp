// Binary .skel writer/reader: everything the format can carry survives a round trip (compared through the JSON writer).
#include "Test.h"
#include "SkeletonBinary.h"
#include <cmath>

using namespace osteon;

static float q(float v) { return std::round(v * 255.f) / 255.f; } // colours are stored as bytes

static SkeletonData richRig() {
    SkeletonData d;
    d.hash = ""; d.x = -10; d.y = -5; d.width = 200; d.height = 300; d.fps = 24; d.imagesPath = "./images/"; d.audioPath = "./audio/";
    BoneData root; root.name = "root"; d.bones.push_back(root);
    BoneData hip; hip.name = "hip"; hip.parent = 0; hip.x = 12.5f; hip.y = 3; hip.rotation = 20; hip.length = 40; hip.inherit = Inherit::NoScale; d.bones.push_back(hip);
    BoneData arm; arm.name = "arm"; arm.parent = 1; arm.length = 30; arm.scaleX = 1.5f; arm.shearX = 4; arm.skinRequired = true; arm.color = Color(q(0.2f), q(0.4f), q(0.6f), 1); d.bones.push_back(arm);
    BoneData hand; hand.name = "hand"; hand.parent = 2; hand.x = 30; hand.length = 10; d.bones.push_back(hand);
    BoneData tgt; tgt.name = "target"; tgt.parent = 0; tgt.x = 50; tgt.y = 20; d.bones.push_back(tgt);
    for (int i = 0; i < 5; i++) {
        SlotData s; s.name = "slot" + std::to_string(i); s.bone = i == 4 ? 4 : i % 4;
        s.attachment = i == 0 ? "body" : i == 1 ? "mesh" : i == 3 ? "walk" : "";
        if (i == 1) { s.color = Color(q(0.5f), q(0.25f), q(0.75f), q(0.5f)); s.hasDark = true; s.dark = Color(q(0.1f), q(0.2f), q(0.3f), 1); s.blend = BlendMode::Additive; }
        d.slots.push_back(s);
    }
    Attachment body; body.type = AttachmentType::Region; body.name = "body"; body.path = "imgs/body"; body.x = 4; body.y = -2; body.rotation = 15;
    body.width = 64; body.height = 80; body.scaleX = 1.1f; body.color = Color(q(0.9f), q(0.8f), q(0.7f), 1);
    d.defaultSkin().set(0, body);
    Attachment mesh; mesh.type = AttachmentType::Mesh; mesh.name = "mesh"; mesh.width = 32; mesh.height = 32;
    mesh.uvs = {0, 0, 1, 0, 1, 1, 0, 1}; mesh.triangles = {0, 1, 2, 0, 2, 3}; mesh.hullLength = 4; mesh.edges = {0, 1, 1, 2, 2, 3, 3, 0};
    // 4 weighted vertices: v0 {bones 2,1}, v1 {2}, v2 {1,2}, v3 {2}; 6 influences of (x, y, weight)
    mesh.bones = {2, 2, 1, 1, 2, 2, 1, 2, 1, 2};
    mesh.vertices = {1, 2, 0.4f, 3, 4, 0.6f, 5, 6, 1.f, 7, 8, 0.5f, 9, 10, 0.5f, 11, 12, 1.f};
    mesh.worldVerticesLength = 8;
    d.defaultSkin().set(1, mesh);
    Attachment lm; lm.type = AttachmentType::LinkedMesh; lm.name = "lmesh"; lm.parentMesh = "mesh"; lm.parentSkin = "alt"; lm.width = 8; lm.height = 9; lm.inheritTimelines = false;
    d.defaultSkin().set(1, lm);
    Attachment box; box.type = AttachmentType::BoundingBox; box.name = "box"; box.vertices = {0, 0, 10, 0, 10, 10, 0, 10}; box.worldVerticesLength = 8; box.color = Color(q(1), q(0), q(0), q(0.5f));
    d.defaultSkin().set(2, box);
    Attachment pt; pt.type = AttachmentType::Point; pt.name = "muzzle"; pt.x = 3; pt.y = 4; pt.rotation = 90; pt.color = Color(q(0), q(1), q(0), 1);
    d.defaultSkin().set(2, pt);
    Attachment path; path.type = AttachmentType::Path; path.name = "path"; path.closed = true; path.constantSpeed = false;
    path.vertices = {0, 0, 5, 5, 10, 5, 15, 0, 20, -5, 25, -5}; path.worldVerticesLength = 12; path.lengths = {30.f, 40.f};
    d.defaultSkin().set(3, path);
    Attachment clip; clip.type = AttachmentType::Clipping; clip.name = "clip"; clip.vertices = {0, 0, 10, 0, 10, 10}; clip.worldVerticesLength = 6; clip.endSlot = "slot3";
    d.defaultSkin().set(2, clip);
    Attachment seq; seq.type = AttachmentType::Region; seq.name = "walk"; seq.width = 10; seq.height = 10; seq.seqCount = 4; seq.seqStart = 1; seq.seqDigits = 2; seq.seqSetup = 1;
    d.defaultSkin().set(3, seq);

    Skin alt; alt.name = "alt"; alt.bones = {2}; alt.color = Color(q(0.2f), q(0.2f), q(0.8f), 1);
    Attachment altBody = body; altBody.name = "body"; altBody.x = 9;
    alt.set(0, altBody);
    d.skins.push_back(alt);

    IkConstraintData ik; ik.name = "arm-ik"; ik.order = 0; ik.bones = {2, 3}; ik.target = 4; ik.mix = 0.5f; ik.softness = 3; ik.bendDirection = -1; ik.compress = true; ik.stretch = true;
    d.ik.push_back(ik);
    TransformConstraintData tc; tc.name = "follow"; tc.order = 1; tc.bones = {3}; tc.target = 4; tc.mixRotate = 0.5f; tc.mixX = 0.25f; tc.mixY = 0.25f; tc.mixScaleX = 0; tc.mixScaleY = 0; tc.mixShearY = 1;
    tc.offsetRotation = 10; tc.offsetX = 2; tc.relative = true; tc.local = true;
    d.transforms.push_back(tc);
    PathConstraintData pc; pc.name = "pathc"; pc.order = 2; pc.bones = {3}; pc.target = 3; pc.spacingMode = PathConstraintData::Spacing::Percent; pc.rotateMode = PathConstraintData::Rotate::Chain;
    pc.position = 0.25f; pc.spacing = 0.1f; pc.mixRotate = 0.75f; pc.mixX = 0.5f; pc.mixY = 0.5f;
    d.paths.push_back(pc);
    PhysicsConstraintData ph; ph.name = "wobble"; ph.order = 3; ph.bone = 3; ph.x = 0.5f; ph.rotate = 1; ph.limit = 100; ph.step = 1.0f / 30; ph.inertia = 0.9f; ph.strength = 50; ph.damping = 0.8f;
    ph.massInverse = 0.5f; ph.wind = 2; ph.gravity = 3; ph.mix = 0.7f; ph.windGlobal = true;
    d.physics.push_back(ph);
    d.skins[1].constraints = {"arm-ik", "wobble"};

    EventData ev; ev.name = "step"; ev.intValue = 3; ev.floatValue = 1.5f; ev.stringValue = "left"; ev.audio = "step.wav"; ev.volume = 0.5f; ev.balance = -0.25f;
    d.events.push_back(ev);
    EventData ev2; ev2.name = "hit"; d.events.push_back(ev2);

    Animation an; an.name = "all";
    auto add = [&](TimelineType t, int target, std::initializer_list<std::pair<float, std::initializer_list<float>>> keys) {
        Timeline tl; tl.type = t; tl.target = target;
        for (auto& [time, vals] : keys) { Key k; k.time = time; int i = 0; for (float v : vals) k.v[i++] = v; tl.keys.push_back(k); }
        an.timelines.push_back(tl);
        return (int)an.timelines.size() - 1;
    };
    int r = add(TimelineType::Rotate, 1, {{0, {0}}, {0.5f, {30}}, {1, {-10}}});
    an.timelines[r].keys[0].curve = CurveType::Bezier; an.timelines[r].keys[0].bezier = {0.1f, 0.f, 0.4f, 20.f};
    an.timelines[r].keys[1].curve = CurveType::Stepped;
    add(TimelineType::Translate, 2, {{0, {0, 0}}, {1, {5, -6}}});
    add(TimelineType::ScaleX, 2, {{0, {1}}, {1, {1.4f}}});
    add(TimelineType::ShearY, 2, {{0, {0}}, {1, {7}}});
    int inh = add(TimelineType::Inherit, 2, {{0, {}}, {0.5f, {}}});
    an.timelines[inh].keys[1].intValue = (int)Inherit::OnlyTranslation;
    int at = add(TimelineType::Attachment, 0, {{0, {}}, {0.5f, {}}});
    an.timelines[at].keys[0].str = "body"; an.timelines[at].keys[1].str = "";
    add(TimelineType::RGBA, 1, {{0, {q(1), q(1), q(1), q(1)}}, {1, {q(0.5f), q(0.25f), q(0.75f), q(0.5f)}}});
    add(TimelineType::RGB, 1, {{0, {q(1), q(0), q(0)}}, {1, {q(0), q(1), q(0)}}});
    add(TimelineType::Alpha, 2, {{0, {q(1)}}, {1, {q(0.5f)}}});
    add(TimelineType::RGBA2, 1, {{0, {q(1), q(1), q(1), q(1), q(0), q(0), q(0)}}, {1, {q(0.2f), q(0.4f), q(0.6f), q(0.8f), q(0.1f), q(0.3f), q(0.5f)}}});
    int ikt = add(TimelineType::Ik, 0, {{0, {1, 0}}, {1, {0.5f, 4}}});
    an.timelines[ikt].keys[0].flag1 = true; an.timelines[ikt].keys[1].flag1 = false; an.timelines[ikt].keys[1].flag2 = true; an.timelines[ikt].keys[1].flag3 = true;
    add(TimelineType::Transform, 0, {{0, {1, 1, 1, 1, 1, 1}}, {1, {0.5f, 0.4f, 0.3f, 0.2f, 0.1f, 0.f}}});
    add(TimelineType::Path, 0, {{0, {0}}, {1, {0.5f}}});
    add(TimelineType::PathMix, 0, {{0, {1, 1, 1}}, {1, {0.5f, 0.25f, 0.75f}}});
    add(TimelineType::PhysicsWind, -1, {{0, {0}}, {1, {4}}}); // the file groups timelines by constraint, the global row first
    add(TimelineType::PhysicsMix, 0, {{0, {1}}, {1, {0.5f}}});
    int rs = add(TimelineType::PhysicsReset, 0, {{0.25f, {}}});
    (void)rs;
    // Deform of the weighted mesh (6 influences -> 12 floats) with leading and trailing zeros.
    {
        Timeline tl; tl.type = TimelineType::Deform; tl.target = 1; tl.skin = "default"; tl.attachment = "mesh";
        Key a; a.time = 0; a.deform.assign(12, 0.f);
        Key b; b.time = 1; b.deform.assign(12, 0.f); b.deform[2] = 1.5f; b.deform[3] = -2; b.deform[8] = 4;
        a.curve = CurveType::Bezier; a.bezier = {0.25f, 0.f, 0.75f, 1.f};
        tl.keys = {a, b};
        an.timelines.push_back(tl);
    }
    {
        Timeline tl; tl.type = TimelineType::Sequence; tl.target = 3;
        Key a; a.time = 0; a.str = "loop"; a.intValue = 2; a.v[0] = 0.1f; Key b; b.time = 1; b.str = "hold";
        tl.keys = {a, b};
        an.timelines.push_back(tl);
    }
    {
        Timeline tl; tl.type = TimelineType::DrawOrder; tl.target = -1;
        Key a; a.time = 0; Key b; b.time = 0.5f; b.drawOrder = {1, 0, 2, 4, 3};
        tl.keys = {a, b};
        an.timelines.push_back(tl);
    }
    {
        Timeline tl; tl.type = TimelineType::Event; tl.target = -1;
        Key a; a.time = 0.25f; a.str = "step"; Key b; b.time = 0.75f; b.str = "step"; b.flag1 = true; b.intValue = 9; b.flag3 = true; b.str2 = "right"; b.volume = 0.25f; b.balance = 0.5f;
        Key c; c.time = 0.8f; c.str = "hit"; c.flag2 = true; c.v[0] = 2.5f;
        tl.keys = {a, b, c};
        an.timelines.push_back(tl);
    }
    d.animations.push_back(an);
    Animation empty; empty.name = "empty"; d.animations.push_back(empty);
    return d;
}

static std::string asJson(const SkeletonData& d) {
    JsonWriteOptions o; o.pretty = false;
    return writeSkeletonJson(d, o);
}

TEST(binary_round_trip_keeps_every_feature) {
    SkeletonData d = richRig();
    std::vector<std::string> warnings;
    std::vector<uint8_t> bytes = writeSkeletonBinary(d, {}, &warnings);
    for (auto& w : warnings) test::fail(__FILE__, __LINE__, "warning: " + w);
    CHECK(bytes.size() > 100);
    SkeletonData back;
    JsonResult res = readSkeletonBinary(bytes.data(), bytes.size(), back);
    if (!res.ok) test::fail(__FILE__, __LINE__, res.error);
    CHECK(res.ok);
    std::string a = asJson(d), b = asJson(back);
    if (a != b) {
        size_t i = 0;
        while (i < a.size() && i < b.size() && a[i] == b[i]) i++;
        test::fail(__FILE__, __LINE__, "JSON differs at " + std::to_string(i) + ": ..." + a.substr(i > 60 ? i - 60 : 0, 140) + "\n  vs ..." + b.substr(i > 60 ? i - 60 : 0, 140));
    }
}

TEST(binary_without_nonessential_data_is_smaller_and_loads) {
    SkeletonData d = richRig();
    BinaryWriteOptions lean; lean.nonessential = false;
    auto full = writeSkeletonBinary(d);
    auto small = writeSkeletonBinary(d, lean);
    CHECK(small.size() < full.size());
    SkeletonData back;
    CHECK(readSkeletonBinary(small.data(), small.size(), back).ok);
    CHECK(back.bones.size() == d.bones.size());
    CHECK(back.animations.size() == d.animations.size());
}

TEST(binary_is_smaller_than_json) {
    SkeletonData d = richRig();
    CHECK(writeSkeletonBinary(d).size() < asJson(d).size());
}

TEST(binary_reader_rejects_truncated_and_garbage_data) {
    SkeletonData d = richRig();
    auto bytes = writeSkeletonBinary(d);
    for (size_t cut : {size_t(0), size_t(3), size_t(20), bytes.size() / 2, bytes.size() - 1}) {
        SkeletonData back;
        CHECK(!readSkeletonBinary(bytes.data(), cut, back).ok);
    }
    // Flip bytes everywhere: the reader must never crash or hang (result may be ok or an error).
    for (size_t i = 0; i < bytes.size(); i += 7) {
        auto copy = bytes;
        copy[i] ^= 0xA5;
        SkeletonData back;
        readSkeletonBinary(copy.data(), copy.size(), back);
    }
    CHECK(true);
}

TEST(binary_reports_what_the_4_2_format_cannot_carry) {
    SkeletonData d = richRig();
    SliderConstraintData s; s.name = "slide"; s.animation = "all"; d.sliders.push_back(s);
    d.transforms[0].materialiseLinks();
    std::vector<std::string> warnings;
    writeSkeletonBinary(d, {}, &warnings);
    CHECK(warnings.size() >= 2);
}

TEST(binary_round_trips_the_character_rig_if_present) {
    for (const char* path : {"samples/character_rig.osteon", "../samples/character_rig.osteon"}) {
        FILE* f = std::fopen(path, "rb");
        if (!f) continue;
        std::string text;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        std::fclose(f);
        SkeletonData d;
        if (!readSkeletonJson(text, d).ok) { CHECK(false); return; }
        auto bytes = writeSkeletonBinary(d);
        SkeletonData back;
        CHECK(readSkeletonBinary(bytes.data(), bytes.size(), back).ok);
        std::printf("    rig: %zu bytes json -> %zu bytes binary\n", text.size(), bytes.size());
        // Colours and times are stored at the precision of the format, so compare the pose-relevant counts and one animation's length.
        CHECK(back.bones.size() == d.bones.size());
        CHECK(back.slots.size() == d.slots.size());
        CHECK(back.animations.size() == d.animations.size());
        for (size_t i = 0; i < d.animations.size() && i < back.animations.size(); i++) {
            CHECK(back.animations[i].timelines.size() == d.animations[i].timelines.size());
            CHECK_NEAR(back.animations[i].duration(), d.animations[i].duration(), 1e-5);
        }
        return;
    }
}
