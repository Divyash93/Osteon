#include "Test.h"
#include "Geometry.h"
#include <set>

using namespace osteon;

static double trisArea(const std::vector<float>& pts, const std::vector<int>& tris, bool* degenerate = nullptr) {
    double total = 0;
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        float ax = pts[tris[i] * 2], ay = pts[tris[i] * 2 + 1];
        float bx = pts[tris[i + 1] * 2], by = pts[tris[i + 1] * 2 + 1];
        float cx = pts[tris[i + 2] * 2], cy = pts[tris[i + 2] * 2 + 1];
        double a = 0.5 * ((bx - ax) * (cy - ay) - (by - ay) * (cx - ax));
        if (degenerate && std::fabs(a) < 1e-6) *degenerate = true;
        total += std::fabs(a);
    }
    return total;
}

TEST(geometry_triangulate_square) {
    std::vector<float> sq{0, 0, 10, 0, 10, 10, 0, 10};
    auto t = triangulate(sq);
    CHECK(t.size() == 6);
    CHECK_NEAR(trisArea(sq, t), 100, 1e-4);
    std::vector<float> cw{0, 0, 0, 10, 10, 10, 10, 0};
    CHECK(triangulate(cw).size() == 6);
    CHECK_NEAR(trisArea(cw, triangulate(cw)), 100, 1e-4);
}

TEST(geometry_triangulate_concave_l) {
    std::vector<float> l{0, 0, 20, 0, 20, 10, 10, 10, 10, 30, 0, 30};
    auto t = triangulate(l);
    CHECK(t.size() == (6 - 2) * 3);
    CHECK_NEAR(trisArea(l, t), std::fabs(polygonArea(l)), 1e-3);
    CHECK_NEAR(std::fabs(polygonArea(l)), 400, 1e-3);
    // Reverse winding.
    std::vector<float> r;
    for (int i = 5; i >= 0; i--) { r.push_back(l[i * 2]); r.push_back(l[i * 2 + 1]); }
    auto tr = triangulate(r);
    CHECK(tr.size() == 12);
    CHECK_NEAR(trisArea(r, tr), 400, 1e-3);
}

TEST(geometry_triangulate_mesh_interior) {
    std::vector<float> pts{0, 0, 20, 0, 20, 10, 10, 10, 10, 30, 0, 30};
    int hull = 6;
    auto interior = generateInteriorPoints(std::vector<float>(pts.begin(), pts.end()), 4);
    CHECK(!interior.empty());
    pts.insert(pts.end(), interior.begin(), interior.end());
    pts.insert(pts.end(), {5, 0}); // on a hull edge
    int n = (int)pts.size() / 2;
    auto t = triangulateMesh(pts, hull);
    std::set<int> used(t.begin(), t.end());
    CHECK((int)used.size() == n);
    bool degenerate = false;
    CHECK_NEAR(trisArea(pts, t, &degenerate), 400, 1e-2);
    CHECK(!degenerate);
    // Triangle count of a triangulation: 2n - h - 2 (h boundary vertices, here 7).
    CHECK((int)t.size() / 3 == 2 * n - 7 - 2);
}

TEST(geometry_trace_hull_circle) {
    int w = 100, h = 100;
    std::vector<uint8_t> img(w * h * 4, 0);
    float r = 35;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = x + 0.5f - 50, dy = y + 0.5f - 45;
            if (dx * dx + dy * dy <= r * r) img[(y * w + x) * 4 + 3] = 255;
        }
    img[(2 * w + 2) * 4 + 3] = 255; // stray pixel, smaller component
    auto hull = traceHull(img.data(), w, h, 10, 32, 0);
    CHECK(hull.size() >= 6 && hull.size() <= 64);
    double area = polygonArea(hull), circle = PI * r * r;
    CHECK(area > 0);
    CHECK(std::fabs(area - circle) / circle < 0.1);
    for (size_t i = 0; i < hull.size(); i += 2) CHECK(hull[i + 1] > 5); // stray pixel ignored
    auto padded = traceHull(img.data(), w, h, 10, 32, 3);
    CHECK(polygonArea(padded) > area);
    auto t = triangulate(hull);
    CHECK_NEAR(trisArea(hull, t), area, area * 1e-3);
}

static SkeletonData weightSkeleton() {
    SkeletonData d;
    BoneData root; root.name = "root";
    BoneData a; a.name = "a"; a.parent = 0; a.x = 10; a.rotation = 30; a.length = 50;
    BoneData b; b.name = "b"; b.parent = 1; b.x = 50; b.rotation = -45; b.length = 40; b.scaleX = 1.5f;
    BoneData c; c.name = "c"; c.parent = 0; c.x = -20; c.y = 40;
    d.bones = {root, a, b, c};
    return d;
}

static Attachment gridMesh() {
    Attachment m;
    m.type = AttachmentType::Mesh;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 5; x++) { m.vertices.push_back(x * 25.0f - 10); m.vertices.push_back(y * 20.0f - 15); }
    m.worldVerticesLength = (int)m.vertices.size();
    return m;
}

static void checkSums(const Attachment& m) {
    for (size_t b = 0, v = 0; b < m.bones.size();) {
        int count = m.bones[b++];
        CHECK(count >= 1 && count <= 4);
        float sum = 0;
        for (int i = 0; i < count; i++, b++, v += 3) sum += m.vertices[v + 2];
        CHECK_NEAR(sum, 1, 1e-4);
    }
}

TEST(geometry_bind_weights_preserves_world) {
    SkeletonData d = weightSkeleton();
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment m = gridMesh();
    int slotBone = 1;
    auto before = meshWorldVertices(m, sk, slotBone);
    bindWeights(m, sk, slotBone, {0, 1, 2, 3});
    CHECK(m.isWeighted());
    CHECK(countVertices(m) == 20);
    CHECK(m.worldVerticesLength == 40);
    checkSums(m);
    auto after = meshWorldVertices(m, sk, slotBone);
    CHECK(after.size() == before.size());
    for (size_t i = 0; i < before.size(); i++) CHECK_NEAR(after[i], before[i], 1e-3);

    pruneWeights(m, 0.1f);
    checkSums(m);
    after = meshWorldVertices(m, sk, slotBone);
    for (size_t i = 0; i < before.size(); i++) CHECK_NEAR(after[i], before[i], 1e-3);

    unbindWeights(m, sk, 2);
    CHECK(!m.isWeighted());
    after = meshWorldVertices(m, sk, 2);
    for (size_t i = 0; i < before.size(); i++) CHECK_NEAR(after[i], before[i], 1e-3);
}

TEST(geometry_set_vertex_weight) {
    SkeletonData d = weightSkeleton();
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment m = gridMesh();
    auto before = meshWorldVertices(m, sk, 0);
    bindWeights(m, sk, 0, {1, 2});
    setVertexWeight(m, sk, 3, 1, 0.7f);
    setVertexWeight(m, sk, 5, 3, 0.25f); // bone not yet influencing
    setVertexWeight(m, sk, 7, 2, 0.0f);
    setVertexWeight(m, sk, 8, 2, 1.0f);
    checkSums(m);
    auto after = meshWorldVertices(m, sk, 0);
    for (size_t i = 0; i < before.size(); i++) CHECK_NEAR(after[i], before[i], 1e-3);
    // Inspect vertex 3 and 5.
    size_t b = 0, v = 0;
    for (int vert = 0; vert < 6; vert++) {
        int count = m.bones[b++];
        for (int i = 0; i < count; i++, b++, v += 3) {
            if (vert == 3 && m.bones[b] == 1) CHECK_NEAR(m.vertices[v + 2], 0.7, 1e-4);
            if (vert == 5 && m.bones[b] == 3) CHECK_NEAR(m.vertices[v + 2], 0.25, 1e-4);
        }
    }
    // Moving a bone now moves weighted vertices.
    sk.bones[2].rotation += 20;
    sk.updateWorldTransform();
    auto moved = meshWorldVertices(m, sk, 0);
    CHECK(std::fabs(moved[16] - before[16]) + std::fabs(moved[17] - before[17]) > 1e-2);
}

TEST(geometry_pack_rects) {
    std::vector<PackRect> rects;
    unsigned seed = 7;
    auto rnd = [&](int lo, int hi) { seed = seed * 1103515245u + 12345u; return lo + (int)((seed >> 8) % (unsigned)(hi - lo + 1)); };
    for (int i = 0; i < 60; i++) rects.push_back({i, rnd(8, 120), rnd(8, 60)});
    rects.push_back({60, 250, 30});
    int maxW = 256, maxH = 256, pad = 2;
    int pages = packRects(rects, maxW, maxH, pad, true, true);
    CHECK(pages >= 1);
    for (auto& r : rects) {
        CHECK(r.page >= 0 && r.page < pages);
        int w = r.rotated ? r.h : r.w, h = r.rotated ? r.w : r.h;
        CHECK(r.x >= 0 && r.y >= 0 && r.x + w <= maxW && r.y + h <= maxH);
    }
    for (size_t i = 0; i < rects.size(); i++)
        for (size_t j = i + 1; j < rects.size(); j++) {
            const PackRect &a = rects[i], &b = rects[j];
            if (a.page != b.page) continue;
            int aw = a.rotated ? a.h : a.w, ah = a.rotated ? a.w : a.h, bw = b.rotated ? b.h : b.w, bh = b.rotated ? b.w : b.h;
            bool apart = a.x + aw + pad <= b.x || b.x + bw + pad <= a.x || a.y + ah + pad <= b.y || b.y + bh + pad <= a.y;
            CHECK(apart);
        }
    std::vector<PackRect> big{{0, 300, 10}};
    packRects(big, 256, 256, 0, false, false);
    CHECK(big[0].page == -1);

    std::vector<std::string> files, names;
    std::vector<std::pair<int, int>> sizes;
    for (int p = 0; p < pages; p++) { files.push_back("page" + std::to_string(p) + ".png"); sizes.push_back(packedPageSize(rects, p, true)); }
    for (auto& r : rects) names.push_back("r" + std::to_string(r.id));
    std::string atlas = writeAtlas(files, sizes, rects, names);
    CHECK(atlas.rfind("page0.png\nsize:", 0) == 0);
    CHECK(atlas.find("filter:Linear,Linear\n") != std::string::npos);
    CHECK(atlas.find("\nr60\nbounds:") != std::string::npos);
}
