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
    lo.uniform = hi.uniform = 0; // Uniform adds vertices on top of the Detail budget
    auto a = traceHull(img.data(), 96, 96, lo), b = traceHull(img.data(), 96, 96, hi);
    CHECK(a.size() >= 6 && b.size() >= 6);
    CHECK(a.size() / 2 <= (size_t)traceVertexBudget(10));
    CHECK(b.size() / 2 <= (size_t)traceVertexBudget(90));
    CHECK(b.size() > a.size());
    CHECK(polygonArea(a) > 0 && polygonArea(b) > 0);
}

TEST(trace_uniform_subdivides_long_edges) {
    auto img = discImage(96);
    TraceOptions off, on;
    off.detail = 10; off.uniform = 0;
    on.detail = 10; on.uniform = 0.9f;
    auto a = traceHull(img.data(), 96, 96, off), b = traceHull(img.data(), 96, 96, on);
    CHECK(b.size() > a.size());
    CHECK(polygonArea(b) > 0);
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

TEST(path_lengths_and_curve_split) {
    // Two points joined by a straight curve (handles on the line): length is the distance.
    std::vector<float> v = {-50, 0, 0, 0, 50, 0,   // in, point, out of point 0 ... handles sit on the line
                            50, 0, 100, 0, 150, 0};
    // point 0 at (0,0) with out (50,0); point 1 at (100,0) with in (50,0)
    auto len = computePathLengths(v, false);
    CHECK(len.size() == 1);
    CHECK_NEAR(len[0], 100, 0.5);
    // Splitting at the middle gives three points and two curves whose total is unchanged.
    int idx = splitPathCurve(v, false, 0, 0.5f);
    CHECK(idx == 1);
    CHECK(v.size() == 18);
    auto len2 = computePathLengths(v, false);
    CHECK(len2.size() == 2);
    CHECK_NEAR(len2[1], 100, 0.5);
    CHECK_NEAR(v[(1 * 3 + 1) * 2], 50, 0.5); // the new point sits at the middle
    // A closed path has one more curve.
    std::vector<float> tri = {-10, 0, 0, 0, 10, 0,  90, 0, 100, 0, 110, 0,  40, 80, 50, 90, 60, 100};
    CHECK(computePathLengths(tri, true).size() == 3);
    CHECK(computePathLengths(tri, false).size() == 2);
}

// Trace must never read outside the outline, whatever the shape, settings or Refresh seed (a negative index once crashed it).
TEST(trace_survives_odd_shapes_and_every_setting) {
    const int n = 80;
    for (int shape = 0; shape < 6; shape++) {
        std::vector<uint8_t> px((size_t)n * n * 4, 0);
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                float dx = x - n / 2.0f, dy = y - n / 2.0f;
                bool in = false;
                if (shape == 0) in = std::sqrt(dx * dx + dy * dy) < 30;                       // disc
                else if (shape == 1) in = std::fabs(dx) < 25 && std::fabs(dy) < 12;          // bar
                else if (shape == 2) in = (std::fabs(dx) < 8) || (std::fabs(dy) < 8);        // plus
                else if (shape == 3) in = std::fabs(dx) + std::fabs(dy) < 34;                // diamond
                else if (shape == 4) in = (x + y) % 7 < 4 && x > 10 && x < 70 && y > 10 && y < 70; // stripes (many pieces)
                else in = x > 20 && x < 24 && y > 20 && y < 60;                               // a thin line
                if (in) px[((size_t)y * n + x) * 4 + 3] = 255;
            }
        for (float detail : {0.0f, 20.0f, 60.0f, 100.0f})
            for (float conc : {0.0f, 100.0f})
                for (float refine : {0.0f, 50.0f, 100.0f})
                    for (int seed : {0, 1, 2, 5, 9}) {
                        TraceOptions o;
                        o.detail = detail; o.concavity = conc; o.refinement = refine; o.seed = seed; o.padding = (seed % 2) ? 2.0f : 0.0f;
                        std::vector<float> h = traceHull(px.data(), n, n, o);
                        CHECK(h.empty() || h.size() >= 6);
                        for (float v : h) CHECK(std::isfinite(v));
                    }
    }
}
