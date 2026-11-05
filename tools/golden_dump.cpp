// Dumps reference poses from the C++ engine so the Unity (C#) and Godot (GDScript) runtimes can be checked against it.
//   osteon_golden <skeleton.json|.osteon> <scenario>...
// Scenarios:   apply:<animation>:<time>[:<skin>]        applyAnimation at a time (non looping), setup pose first
//              state:<animA>:<animB>:<t1>:<t2>[:<skin>] play A for t1, switch to B (default crossfade), advance t2
// Output is one JSON document on stdout: bone world matrices, draw order, slot colours and world vertices.
#include "../src/engine/Animation.h"
#include "../src/engine/SkeletonJson.h"
#include <cstdio>
#include <fstream>
#include <sstream>

using namespace osteon;

static std::vector<std::string> split(const std::string& s, char c) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s) { if (ch == c) { out.push_back(cur); cur.clear(); } else cur += ch; }
    out.push_back(cur);
    return out;
}

static void dumpPose(const Skeleton& sk, const char* name) {
    std::printf("{\"name\":\"%s\",\"bones\":[", name);
    for (size_t i = 0; i < sk.bones.size(); i++) {
        const Bone& b = sk.bones[i];
        std::printf("%s[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g]", i ? "," : "", b.a, b.b, b.c, b.d, b.worldX, b.worldY);
    }
    std::printf("],\"drawOrder\":[");
    for (size_t i = 0; i < sk.drawOrder.size(); i++) std::printf("%s%d", i ? "," : "", sk.drawOrder[i]);
    std::printf("],\"slots\":[");
    bool first = true;
    for (int si : sk.drawOrder) {
        const Slot& s = sk.slots[si];
        const Attachment* a = sk.attachmentFor(s);
        std::printf("%s{\"i\":%d,\"color\":[%.9g,%.9g,%.9g,%.9g]", first ? "" : ",", si, s.color.r, s.color.g, s.color.b, s.color.a);
        first = false;
        std::vector<float> v;
        if (a && (a->type == AttachmentType::Region || a->type == AttachmentType::Mesh || a->type == AttachmentType::LinkedMesh) &&
            sk.bones[s.bone].active && sk.worldVertices(s, *a, v)) {
            std::printf(",\"att\":\"%s\",\"verts\":[", a->name.c_str());
            for (size_t k = 0; k < v.size(); k++) std::printf("%s%.9g", k ? "," : "", v[k]);
            std::printf("]");
        }
        std::printf("}");
    }
    std::printf("]}");
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: osteon_golden <skeleton.json> <scenario>...\n"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    SkeletonData data;
    JsonResult r = readSkeletonJson(ss.str(), data);
    if (!r.ok) { std::fprintf(stderr, "load failed: %s\n", r.error.c_str()); return 1; }
    std::printf("{\"bones\":%zu,\"slots\":%zu,\"samples\":[", data.bones.size(), data.slots.size());
    for (int i = 2; i < argc; i++) {
        auto p = split(argv[i], ':');
        Skeleton sk(data);
        std::string label = argv[i];
        if (i > 2) std::printf(",");
        if (p[0] == "apply" && p.size() >= 3) {
            if (p.size() > 3) sk.skin = std::max(0, data.findSkin(p[3]));
            sk.updateCacheOrder();
            sk.setToSetupPose();
            int ai = data.findAnimation(p[1]);
            if (ai < 0) { std::fprintf(stderr, "no animation %s\n", p[1].c_str()); return 1; }
            applyAnimation(data.animations[ai], sk, -1, (float)std::atof(p[2].c_str()), false, nullptr, 1, MixBlend::Setup);
            sk.updateWorldTransform(Physics::Pose);
        } else if (p[0] == "state" && p.size() >= 5) {
            if (p.size() > 5) sk.skin = std::max(0, data.findSkin(p[5]));
            sk.updateCacheOrder();
            AnimationState st(&data);
            st.setAnimation(0, data.findAnimation(p[1]), true);
            st.update((float)std::atof(p[3].c_str()));
            sk.setToSetupPose();
            st.apply(sk);
            st.setAnimation(0, data.findAnimation(p[2]), true);
            st.update((float)std::atof(p[4].c_str()));
            sk.setToSetupPose();
            st.apply(sk);
            sk.updateWorldTransform(Physics::Pose);
        } else {
            std::fprintf(stderr, "bad scenario %s\n", argv[i]);
            return 1;
        }
        dumpPose(sk, label.c_str());
    }
    std::printf("]}\n");
    return 0;
}
