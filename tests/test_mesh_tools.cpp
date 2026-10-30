// Mesh Tools: Trace settings (Detail, Concavity, Refinement, Refresh) and Generate.
#include "Test.h"
#include "Geometry.h"
#include <cmath>

using namespace osteon;

// A 96x96 image with a filled disc and a notch cut into one side.
static std::vector<uint8_t> discImage(int n) {
    std::vector<uint8_t> px((size_t)n * n * 4, 0);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            float dx = x - n / 2.0f, dy = y - n / 2.0f;
            bool in = std::sqrt(dx * dx + dy * dy) < n * 0.42f;
            if (dx > 6 && std::fabs(dy) < 8) in = false; // notch
            if (in) px[((size_t)y * n + x) * 4 + 3] = 255;
        }
    return px;
}

TEST(trace_detail_controls_vertex_count) {
    auto img = discImage(96);
    TraceOptions lo, hi;
    lo.detail = 10; hi.detail = 90;
    auto a = traceHull(img.data(), 96, 96, lo), b = traceHull(img.data(), 96, 96, hi);
    CHECK(a.size() >= 6 && b.size() >= 6);
    CHECK(a.size() / 2 <= (size_t)traceVertexBudget(10));
    CHECK(b.size() / 2 <= (size_t)traceVertexBudget(90));
    CHECK(b.size() > a.size());
    CHECK(polygonArea(a) > 0 && polygonArea(b) > 0);
}

TEST(trace_refresh_changes_result_but_stays_valid) {
    auto img = discImage(96);
    TraceOptions o;
    o.detail = 30;
    auto base = traceHull(img.data(), 96, 96, o);
    bool differs = false;
    for (int seed = 1; seed <= 6; seed++) {
        o.seed = seed;
        auto r = traceHull(img.data(), 96, 96, o);
        CHECK(r.size() >= 6);
        CHECK(polygonArea(r) > 0);
        if (r != base) differs = true;
    }
    CHECK(differs);
}

TEST(trace_concavity_and_refinement_keep_the_hull_valid) {
    auto img = discImage(96);
    for (float c : {0.0f, 50.0f, 100.0f})
        for (float r : {0.0f, 100.0f}) {
            TraceOptions o;
            o.detail = 35; o.concavity = c; o.refinement = r; o.padding = 0;
            auto h = traceHull(img.data(), 96, 96, o);
            CHECK(h.size() >= 6);
            CHECK(h.size() / 2 <= (size_t)traceVertexBudget(35));
            CHECK(polygonArea(h) > 0);
        }
    // Concavity pulls vertices into the notch: with it on, the hull has a vertex near the notch's inner corner.
    TraceOptions on, off;
    on.detail = off.detail = 30; on.concavity = 100; off.concavity = 0; on.padding = off.padding = 0;
    auto nearNotch = [&](const std::vector<float>& h) {
        int c = 0;
        for (size_t i = 0; i + 1 < h.size(); i += 2) if (h[i] > 50 && h[i] < 62 && std::fabs(h[i + 1] - 48) < 12) c++;
        return c;
    };
    CHECK(nearNotch(traceHull(img.data(), 96, 96, on)) >= nearNotch(traceHull(img.data(), 96, 96, off)));
}

TEST(generate_quad_makes_a_grid_and_repeats_add_more) {
    std::vector<float> v = {-50, -50, -50, 50, 50, 50, 50, -50};
    std::vector<int> tris = triangulateMesh(v, 4);
    std::vector<float> grid = generateVertices(v, 4, tris);
    CHECK(grid.size() / 2 >= 4);
    for (size_t i = 0; i + 1 < grid.size(); i += 2) CHECK(pointInPolygon(v, grid[i], grid[i + 1]));
    // Add them and click again: more points, all inside.
    std::vector<float> all = v;
    all.insert(all.end(), grid.begin(), grid.end());
    std::vector<int> t2 = triangulateMesh(all, 4);
    std::vector<float> more = generateVertices(all, 4, t2);
    CHECK(!more.empty());
    for (size_t i = 0; i + 1 < more.size(); i += 2) CHECK(pointInPolygon(v, more[i], more[i + 1]));
}

TEST(interpolate_uv_inside_and_outside) {
    std::vector<float> v = {0, 0, 0, 10, 10, 10, 10, 0};
    std::vector<float> uv = {0, 1, 0, 0, 1, 0, 1, 1};
    std::vector<int> tris = triangulateMesh(v, 4);
    float out[2];
    CHECK(interpolateUv(v, uv, tris, 5, 5, out));
    CHECK_NEAR(out[0], 0.5f, 1e-3);
    CHECK_NEAR(out[1], 0.5f, 1e-3);
    CHECK(!interpolateUv(v, uv, tris, 20, 5, out));
}
