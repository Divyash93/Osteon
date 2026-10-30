// Mesh geometry tools: triangulation, image hull tracing, automatic weights, atlas packing.
#include "Geometry.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <map>
#include <queue>
#include <set>
#include <sstream>

namespace osteon {

namespace {

struct P2 { double x, y; };

double orient(P2 a, P2 b, P2 c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); }

std::vector<P2> toPoints(const std::vector<float>& v) {
    std::vector<P2> p(v.size() / 2);
    for (size_t i = 0; i < p.size(); i++) p[i] = {v[i * 2], v[i * 2 + 1]};
    return p;
}

double pointSegmentDist(P2 p, P2 a, P2 b) {
    double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2 : 0;
    t = std::clamp(t, 0.0, 1.0);
    double ex = a.x + dx * t - p.x, ey = a.y + dy * t - p.y;
    return std::sqrt(ex * ex + ey * ey);
}

double extent(const std::vector<P2>& p) {
    if (p.empty()) return 1;
    double minX = p[0].x, maxX = p[0].x, minY = p[0].y, maxY = p[0].y;
    for (P2 q : p) {
        minX = std::min(minX, q.x); maxX = std::max(maxX, q.x);
        minY = std::min(minY, q.y); maxY = std::max(maxY, q.y);
    }
    return std::max(1e-6, std::max(maxX - minX, maxY - minY));
}

} // namespace

// ---- Triangulation ------------------------------------------------------------------------

float polygonArea(const std::vector<float>& polygon) {
    size_t n = polygon.size() / 2;
    double a = 0;
    for (size_t i = 0; i < n; i++) {
        size_t j = (i + 1) % n;
        a += (double)polygon[i * 2] * polygon[j * 2 + 1] - (double)polygon[j * 2] * polygon[i * 2 + 1];
    }
    return (float)(a * 0.5);
}

bool pointInPolygon(const std::vector<float>& polygon, float x, float y) {
    size_t n = polygon.size() / 2;
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = polygon[i * 2], yi = polygon[i * 2 + 1], xj = polygon[j * 2], yj = polygon[j * 2 + 1];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) inside = !inside;
    }
    return inside;
}

std::vector<int> triangulate(const std::vector<float>& polygon) {
    std::vector<int> out;
    std::vector<P2> p = toPoints(polygon);
    int n = (int)p.size();
    if (n < 3) return out;
    std::vector<int> idx(n);
    for (int i = 0; i < n; i++) idx[i] = i;
    if (polygonArea(polygon) < 0) std::reverse(idx.begin(), idx.end()); // work counter-clockwise
    double eps = extent(p) * extent(p) * 1e-12;
    auto same = [&](int a, int b) { return p[a].x == p[b].x && p[a].y == p[b].y; };
    while (idx.size() > 3) {
        int m = (int)idx.size(), ear = -1, fallback = 0;
        double bestCross = -1e300;
        for (int i = 0; i < m && ear < 0; i++) {
            int a = idx[(i + m - 1) % m], b = idx[i], c = idx[(i + 1) % m];
            double cr = orient(p[a], p[b], p[c]);
            if (cr > bestCross) { bestCross = cr; fallback = i; }
            if (cr <= eps) continue; // reflex or collinear
            bool blocked = false;
            for (int j = 0; j < m && !blocked; j++) {
                int v = idx[j];
                if (v == a || v == b || v == c || same(v, a) || same(v, b) || same(v, c)) continue;
                if (orient(p[a], p[b], p[v]) >= 0 && orient(p[b], p[c], p[v]) >= 0 && orient(p[c], p[a], p[v]) >= 0)
                    blocked = true;
            }
            if (!blocked) ear = i;
        }
        if (ear < 0) ear = fallback; // not simple / degenerate: keep making progress
        out.push_back(idx[(ear + m - 1) % m]);
        out.push_back(idx[ear]);
        out.push_back(idx[(ear + 1) % m]);
        idx.erase(idx.begin() + ear);
    }
    out.push_back(idx[0]); out.push_back(idx[1]); out.push_back(idx[2]);
    return out;
}

namespace {

/** Triangle soup with directed-edge lookup, used for point insertion and Lawson flips. */
struct TriMesh {
    const std::vector<P2>& p;
    std::vector<std::array<int, 3>> tris;
    std::map<std::pair<int, int>, int> edges;    // directed edge -> triangle
    std::set<std::pair<int, int>> fixed;          // undirected (min, max) constrained edges
    double eps;

    explicit TriMesh(const std::vector<P2>& pts) : p(pts) { eps = extent(pts) * extent(pts) * 1e-10; }

    static std::pair<int, int> key(int a, int b) { return {std::min(a, b), std::max(a, b)}; }
    void set(int i, std::array<int, 3> t) {
        if (i < (int)tris.size()) {
            const auto& o = tris[i];
            for (int k = 0; k < 3; k++) {
                auto it = edges.find({o[k], o[(k + 1) % 3]});
                if (it != edges.end() && it->second == i) edges.erase(it);
            }
            tris[i] = t;
        } else tris.push_back(t);
        for (int k = 0; k < 3; k++) edges[{t[k], t[(k + 1) % 3]}] = i;
    }
    int add(std::array<int, 3> t) { int i = (int)tris.size(); set(i, t); return i; }
    int find(int a, int b) const { auto it = edges.find({a, b}); return it == edges.end() ? -1 : it->second; }
    static int third(const std::array<int, 3>& t, int a, int b) {
        for (int v : t) if (v != a && v != b) return v;
        return -1;
    }

    static double inCircle(P2 a, P2 b, P2 c, P2 d) {
        double ax = a.x - d.x, ay = a.y - d.y, bx = b.x - d.x, by = b.y - d.y, cx = c.x - d.x, cy = c.y - d.y;
        return (ax * ax + ay * ay) * (bx * cy - cx * by) - (bx * bx + by * by) * (ax * cy - cx * ay) +
               (cx * cx + cy * cy) * (ax * by - bx * ay);
    }

    void legalize(std::vector<std::pair<int, int>>& stack) {
        int guard = 0, limit = 100000 + (int)tris.size() * 200;
        while (!stack.empty() && guard++ < limit) {
            auto [u, v] = stack.back();
            stack.pop_back();
            if (fixed.count(key(u, v))) continue;
            int t1 = find(u, v), t2 = find(v, u);
            if (t1 < 0 || t2 < 0) continue;
            int w = third(tris[t1], u, v), x = third(tris[t2], u, v);
            double scale = std::max(1e-30, std::fabs(orient(p[u], p[v], p[w])) * extent(p) * extent(p));
            if (inCircle(p[u], p[v], p[w], p[x]) <= scale * 1e-9) continue;
            if (orient(p[u], p[x], p[w]) <= eps || orient(p[x], p[v], p[w]) <= eps) continue; // non-convex quad
            set(t1, {u, x, w});
            set(t2, {x, v, w});
            stack.push_back({u, x}); stack.push_back({x, v});
            stack.push_back({v, w}); stack.push_back({w, u});
        }
    }

    bool insert(int k) {
        P2 q = p[k];
        int best = -1, bestEdge = -1;
        double bestMin = -1e300, tol = extent(p) * 1e-7;
        for (int i = 0; i < (int)tris.size(); i++) {
            const auto& t = tris[i];
            double m = 1e300; int me = -1, onEdges = 0;
            for (int e = 0; e < 3; e++) {
                P2 a = p[t[e]], b = p[t[(e + 1) % 3]];
                double len = std::max(1e-30, std::hypot(b.x - a.x, b.y - a.y));
                double d = orient(a, b, q) / len;
                if (std::fabs(d) <= tol) onEdges++;
                if (d < m) { m = d; me = e; }
            }
            if (onEdges >= 2) return false; // coincides with an existing vertex
            if (m > bestMin) { bestMin = m; best = i; bestEdge = me; }
        }
        if (best < 0 || bestMin < -tol) return false; // outside the hull
        std::array<int, 3> t = tris[best];
        std::vector<std::pair<int, int>> stack;
        if (bestMin > tol) {
            set(best, {t[0], t[1], k});
            add({t[1], t[2], k});
            add({t[2], t[0], k});
            stack = {{t[0], t[1]}, {t[1], t[2]}, {t[2], t[0]}};
        } else {
            int a = t[bestEdge], b = t[(bestEdge + 1) % 3], c = t[(bestEdge + 2) % 3];
            int t2 = find(b, a);
            int d = t2 >= 0 ? third(tris[t2], a, b) : -1;
            set(best, {a, k, c});
            add({k, b, c});
            stack = {{b, c}, {c, a}};
            if (t2 >= 0) {
                set(t2, {b, k, d});
                add({k, a, d});
                stack.push_back({a, d}); stack.push_back({d, b});
            }
            if (fixed.erase(key(a, b))) { fixed.insert(key(a, k)); fixed.insert(key(k, b)); }
        }
        legalize(stack);
        return true;
    }
};

} // namespace

std::vector<int> triangulateMesh(const std::vector<float>& points, int hullCount) {
    std::vector<P2> p = toPoints(points);
    int n = (int)p.size();
    hullCount = std::min(hullCount, n);
    if (hullCount < 3) return {};
    std::vector<float> hull(points.begin(), points.begin() + hullCount * 2);
    std::vector<int> ear = triangulate(hull);
    TriMesh m(p);
    for (size_t i = 0; i + 2 < ear.size(); i += 3) {
        std::array<int, 3> t{ear[i], ear[i + 1], ear[i + 2]};
        if (orient(p[t[0]], p[t[1]], p[t[2]]) < 0) std::swap(t[1], t[2]);
        m.add(t);
    }
    for (int i = 0; i < hullCount; i++) m.fixed.insert(TriMesh::key(i, (i + 1) % hullCount));
    // Delaunay-ify the hull triangulation first, then insert interior points.
    std::vector<std::pair<int, int>> stack;
    for (auto& e : m.edges) stack.push_back(e.first);
    m.legalize(stack);
    for (int k = hullCount; k < n; k++) m.insert(k);
    stack.clear();
    for (auto& e : m.edges) stack.push_back(e.first);
    m.legalize(stack);
    std::vector<int> out;
    out.reserve(m.tris.size() * 3);
    for (auto& t : m.tris) {
        if (std::fabs(orient(p[t[0]], p[t[1]], p[t[2]])) <= 0) continue;
        out.insert(out.end(), t.begin(), t.end());
    }
    return out;
}

// ---- Image hull ---------------------------------------------------------------------------

namespace {

// `mult` (optional) scales each point's deviation, so points in concave pockets win a vertex sooner.
void douglasPeucker(const std::vector<P2>& p, int first, int last, double eps, std::vector<char>& keep, const std::vector<double>* mult) {
    std::vector<std::pair<int, int>> stack{{first, last}};
    int n = (int)p.size();
    while (!stack.empty()) {
        auto [a, b] = stack.back();
        stack.pop_back();
        double maxD = -1; int idx = -1;
        for (int i = a + 1; i < b; i++) {
            double d = pointSegmentDist(p[i % n], p[a % n], p[b % n]);
            if (mult) d *= (*mult)[i % n];
            if (d > maxD) { maxD = d; idx = i; }
        }
        if (idx >= 0 && maxD > eps) {
            keep[idx % n] = 1;
            stack.push_back({a, idx});
            stack.push_back({idx, b});
        }
    }
}

/** Indices (ascending) of the loop points kept by Douglas-Peucker; `anchor` is the first one kept. */
std::vector<int> simplifyIndices(const std::vector<P2>& p, double eps, int anchor, const std::vector<double>* mult) {
    int n = (int)p.size();
    std::vector<int> all(n);
    for (int i = 0; i < n; i++) all[i] = i;
    if (n <= 3) return all;
    anchor = ((anchor % n) + n) % n;
    int far = anchor; double farD = -1;
    for (int i = 0; i < n; i++) {
        double d = std::hypot(p[i].x - p[anchor].x, p[i].y - p[anchor].y);
        if (d > farD) { farD = d; far = i; }
    }
    // Run both halves on a rotated copy so anchor is index 0.
    std::vector<P2> q(n);
    std::vector<double> qm;
    for (int i = 0; i < n; i++) q[i] = p[(anchor + i) % n];
    if (mult) { qm.resize(n); for (int i = 0; i < n; i++) qm[i] = (*mult)[(anchor + i) % n]; }
    int qf = (far - anchor + n) % n;
    std::vector<char> keep(n, 0);
    keep[0] = keep[qf] = 1;
    douglasPeucker(q, 0, qf, eps, keep, mult ? &qm : nullptr);
    douglasPeucker(q, qf, n, eps, keep, mult ? &qm : nullptr);
    std::vector<int> out;
    for (int i = 0; i < n; i++) if (keep[i]) out.push_back((anchor + i) % n);
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<P2> pick(const std::vector<P2>& p, const std::vector<int>& idx) {
    std::vector<P2> out;
    for (int i : idx) out.push_back(p[i]);
    return out;
}

std::vector<P2> simplifyClosed(const std::vector<P2>& p, double eps) { return pick(p, simplifyIndices(p, eps, 0, nullptr)); }

/** Convex hull (monotone chain) of loop points. */
std::vector<P2> convexHullOf(std::vector<P2> pts) {
    std::sort(pts.begin(), pts.end(), [](P2 a, P2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    pts.erase(std::unique(pts.begin(), pts.end(), [](P2 a, P2 b) { return a.x == b.x && a.y == b.y; }), pts.end());
    if (pts.size() < 3) return pts;
    std::vector<P2> h(pts.size() * 2);
    size_t k = 0;
    auto cross = [](P2 o, P2 a, P2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
    for (size_t i = 0; i < pts.size(); i++) { while (k >= 2 && cross(h[k - 2], h[k - 1], pts[i]) <= 0) k--; h[k++] = pts[i]; }
    for (size_t i = pts.size() - 1, t = k + 1; i > 0; i--) { while (k >= t && cross(h[k - 2], h[k - 1], pts[i - 1]) <= 0) k--; h[k++] = pts[i - 1]; }
    h.resize(k - 1);
    return h;
}

} // namespace

static std::vector<P2> traceLoop(const uint8_t* rgba, int w, int h, int alphaThreshold) {
    if (!rgba || w <= 0 || h <= 0) return {};
    std::vector<int> label(w * h, -1);
    auto solid = [&](int x, int y) { return rgba[((size_t)y * w + x) * 4 + 3] > alphaThreshold; };
    // Largest 8-connected component.
    int bestLabel = -1, bestSize = 0, bestStart = -1, nextLabel = 0;
    std::vector<int> queue;
    for (int i = 0; i < w * h; i++) {
        if (label[i] >= 0 || !solid(i % w, i / w)) continue;
        int l = nextLabel++, size = 0;
        queue.assign(1, i);
        label[i] = l;
        while (!queue.empty()) {
            int c = queue.back();
            queue.pop_back();
            size++;
            int cx = c % w, cy = c / w;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    int ni = ny * w + nx;
                    if (label[ni] < 0 && solid(nx, ny)) { label[ni] = l; queue.push_back(ni); }
                }
        }
        if (size > bestSize) { bestSize = size; bestLabel = l; bestStart = i; }
    }
    if (bestLabel < 0) return {};
    auto in = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && label[y * w + x] == bestLabel; };
    // Boundary edges along pixel corners, interior on the right in screen space.
    // Directions: 0 = +x, 1 = +y, 2 = -x, 3 = -y. Each corner has at most two outgoing edges.
    const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};
    int W = w + 1;
    std::vector<uint8_t> out((size_t)W * (h + 1), 0); // bitmask of outgoing directions
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (!in(x, y)) continue;
            if (!in(x, y - 1)) out[y * W + x] |= 1;               // top: (x,y) -> (x+1,y)
            if (!in(x + 1, y)) out[y * W + x + 1] |= 2;           // right: (x+1,y) -> (x+1,y+1)
            if (!in(x, y + 1)) out[(y + 1) * W + x + 1] |= 4;     // bottom: (x+1,y+1) -> (x,y+1)
            if (!in(x - 1, y)) out[(y + 1) * W + x] |= 8;         // left: (x,y+1) -> (x,y)
        }
    int sx = bestStart % w, sy = bestStart / w, cx = sx, cy = sy, dir = 0;
    std::vector<P2> loop;
    size_t guard = out.size() * 4 + 8;
    do {
        uint8_t m = out[cy * W + cx];
        int nd = dir;
        if (m & (m - 1)) {
            // Pinch corner: turn away from the interior so diagonal neighbours stay joined.
            int left = (dir + 3) % 4; // y-down: (dir+3)%4 has negative cross product with dir
            nd = (m & (1 << left)) ? left : (dir + 1) % 4;
        } else {
            for (int d = 0; d < 4; d++) if (m & (1 << d)) nd = d;
        }
        if (nd != dir || loop.empty()) loop.push_back({(double)cx, (double)cy});
        out[cy * W + cx] &= (uint8_t)~(1 << nd);
        dir = nd;
        cx += DX[dir]; cy += DY[dir];
    } while ((cx != sx || cy != sy) && --guard);
    // The start corner is always a turn (top-left of the top-left pixel), so loop[0] is a corner.
    return loop;
}

static std::vector<float> finishHull(const std::vector<P2>& simple, float padding) {
    int n = (int)simple.size();
    std::vector<float> result(n * 2);
    for (int i = 0; i < n; i++) {
        P2 a = simple[(i + n - 1) % n], b = simple[i], c = simple[(i + 1) % n];
        double ox = 0, oy = 0;
        if (padding != 0) {
            // Positive-area winding: the outward normal of edge (dx,dy) is (dy,-dx).
            double l1 = std::max(1e-9, std::hypot(b.x - a.x, b.y - a.y)), l2 = std::max(1e-9, std::hypot(c.x - b.x, c.y - b.y));
            double n1x = (b.y - a.y) / l1, n1y = -(b.x - a.x) / l1, n2x = (c.y - b.y) / l2, n2y = -(c.x - b.x) / l2;
            double bx = n1x + n2x, by = n1y + n2y, bl = std::hypot(bx, by);
            if (bl < 1e-9) { bx = n1x; by = n1y; bl = 1; }
            bx /= bl; by /= bl;
            double s = padding / std::max(0.35, bx * n1x + by * n1y); // limit spikes on sharp corners
            ox = bx * s; oy = by * s;
        }
        result[i * 2] = (float)(b.x + ox);
        result[i * 2 + 1] = (float)(b.y + oy);
    }
    if (polygonArea(result) < 0) { // keep a consistent winding
        std::vector<float> r(result.size());
        for (int i = 0; i < n; i++) { r[i * 2] = result[(n - 1 - i) * 2]; r[i * 2 + 1] = result[(n - 1 - i) * 2 + 1]; }
        result.swap(r);
    }
    return result;
}

std::vector<float> traceHull(const uint8_t* rgba, int w, int h, int alphaThreshold, int maxVertices, float padding) {
    std::vector<P2> loop = traceLoop(rgba, w, h, alphaThreshold);
    if (loop.empty()) return {};
    maxVertices = std::max(3, maxVertices);
    double diag = std::hypot((double)w, (double)h);
    std::vector<P2> simple = simplifyClosed(loop, 1.0);
    if ((int)simple.size() > maxVertices) {
        double lo = 1.0, hi = diag;
        simple = simplifyClosed(loop, hi);
        for (int it = 0; it < 30; it++) {
            double mid = (lo + hi) * 0.5;
            std::vector<P2> s = simplifyClosed(loop, mid);
            if ((int)s.size() <= maxVertices) { hi = mid; simple = std::move(s); }
            else lo = mid;
        }
    }
    return finishHull(simple, padding);
}

int traceVertexBudget(float detail) {
    float t = std::min(1.0f, std::max(0.0f, detail / 100.0f));
    return 4 + (int)std::lround(std::pow(t, 1.5f) * 96);
}

std::vector<float> traceHull(const uint8_t* rgba, int w, int h, const TraceOptions& o) {
    std::vector<P2> loop = traceLoop(rgba, w, h, o.alphaThreshold);
    if (loop.empty()) return {};
    int n = (int)loop.size();
    int budget = traceVertexBudget(o.detail);
    double diag = std::hypot((double)w, (double)h);
    // Concavity: points deep inside a pocket of the outline (far from its convex hull) count for more.
    std::vector<double> mult;
    const std::vector<double>* mp = nullptr;
    if (o.concavity > 0 && n > 3) {
        std::vector<P2> ch = convexHullOf(loop);
        int m = (int)ch.size();
        mult.assign(n, 1.0);
        if (m >= 3) {
            for (int i = 0; i < n; i++) {
                double best = 1e18;
                for (int k = 0; k < m; k++) best = std::min(best, pointSegmentDist(loop[i], ch[k], ch[(k + 1) % m]));
                mult[i] = 1.0 + (o.concavity / 100.0) * 4.0 * std::min(1.0, best / (0.08 * diag));
            }
            mp = &mult;
        }
    }
    // The seed (Refresh) moves where the outline is cut and nudges the tolerance, so each refresh differs a little.
    int anchor = o.seed ? (int)(((unsigned)o.seed * 2654435761u) % (unsigned)n) : 0;
    double jitter = o.seed ? 1.0 + 0.12 * (double)(((unsigned)o.seed * 40503u) % 1000u) / 1000.0 : 1.0;
    std::vector<int> idx = simplifyIndices(loop, 0.5 * jitter, anchor, mp);
    if ((int)idx.size() > budget) {
        double lo = 0.5, hi = diag * 4;
        idx = simplifyIndices(loop, hi, anchor, mp);
        for (int it = 0; it < 40; it++) {
            double mid = (lo + hi) * 0.5;
            std::vector<int> s = simplifyIndices(loop, mid * jitter, anchor, mp);
            if ((int)s.size() <= budget) { hi = mid; idx = std::move(s); }
            else lo = mid;
        }
    }
    // Refinement: slide each vertex along the outline to where it hugs the pixels best.
    int passes = (int)std::lround(std::min(100.0f, std::max(0.0f, o.refinement)) / 100.0f * 6);
    int m = (int)idx.size();
    if (m >= 4 && passes > 0) {
        auto cost = [&](int from, int to, int f2, int t2) { // loop points on [from,to] against the chord from..to (unwrapped indices)
            (void)f2; (void)t2;
            double c = 0;
            for (int i = from + 1; i < to; i++) { double d = pointSegmentDist(loop[i % n], loop[from % n], loop[to % n]); c += d * d; }
            return c;
        };
        for (int pass = 0; pass < passes; pass++) {
            for (int k = 0; k < m; k++) {
                int prev = idx[(k + m - 1) % m], next = idx[(k + 1) % m], cur = idx[k];
                // Unwrap around k so prev < cur < next.
                int u = cur;
                if (prev > u) prev -= n;
                if (next < u) next += n;
                int bestJ = cur;
                double bestC = cost(prev, cur, 0, 0) + cost(cur, next, 0, 0);
                for (int j = prev + 1; j < next; j++) {
                    if (j == cur) continue;
                    double c = cost(prev, j, 0, 0) + cost(j, next, 0, 0);
                    if (c < bestC - 1e-9) { bestC = c; bestJ = j; }
                }
                idx[k] = ((bestJ % n) + n) % n;
            }
            std::vector<int> sorted = idx;
            std::sort(sorted.begin(), sorted.end());
            sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
            idx = sorted;
            m = (int)idx.size();
            if (m < 4) break;
        }
    }
    return finishHull(pick(loop, idx), o.padding);
}

bool interpolateUv(const std::vector<float>& vertices, const std::vector<float>& uvs, const std::vector<int>& triangles, float x, float y, float* uv) {
    for (size_t t = 0; t + 2 < triangles.size(); t += 3) {
        int i0 = triangles[t], i1 = triangles[t + 1], i2 = triangles[t + 2];
        float x0 = vertices[i0 * 2], y0 = vertices[i0 * 2 + 1], x1 = vertices[i1 * 2], y1 = vertices[i1 * 2 + 1], x2 = vertices[i2 * 2], y2 = vertices[i2 * 2 + 1];
        float den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
        if (std::fabs(den) < 1e-9f) continue;
        float w0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / den, w1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / den, w2 = 1 - w0 - w1;
        if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
        uv[0] = w0 * uvs[i0 * 2] + w1 * uvs[i1 * 2] + w2 * uvs[i2 * 2];
        uv[1] = w0 * uvs[i0 * 2 + 1] + w1 * uvs[i1 * 2 + 1] + w2 * uvs[i2 * 2 + 1];
        return true;
    }
    return false;
}

std::vector<float> generateVertices(const std::vector<float>& vertices, int hullLength, const std::vector<int>& triangles) {
    int n = (int)vertices.size() / 2;
    if (hullLength < 3 || n < hullLength) return {};
    std::vector<float> hull(vertices.begin(), vertices.begin() + hullLength * 2);
    if (n == 4 && hullLength == 4) {
        // A plain quad becomes a row-and-column grid.
        float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
        for (int i = 0; i < 4; i++) { minX = std::min(minX, vertices[i * 2]); maxX = std::max(maxX, vertices[i * 2]); minY = std::min(minY, vertices[i * 2 + 1]); maxY = std::max(maxY, vertices[i * 2 + 1]); }
        float spacing = std::max(maxX - minX, maxY - minY) / 4;
        return generateInteriorPoints(hull, spacing);
    }
    // Otherwise add points where the mesh is coarsest: the centroid of the largest triangle, one at a time.
    std::vector<float> pts = vertices;
    std::vector<int> tris = triangles;
    std::vector<float> added;
    int batch = std::max(2, (n + 3) / 4);
    for (int b = 0; b < batch; b++) {
        double bestA = 0; int bt = -1;
        for (size_t t = 0; t + 2 < tris.size(); t += 3) {
            P2 a{pts[tris[t] * 2], pts[tris[t] * 2 + 1]}, c{pts[tris[t + 1] * 2], pts[tris[t + 1] * 2 + 1]}, d{pts[tris[t + 2] * 2], pts[tris[t + 2] * 2 + 1]};
            double ar = std::fabs((c.x - a.x) * (d.y - a.y) - (c.y - a.y) * (d.x - a.x)) * 0.5;
            if (ar > bestA) { bestA = ar; bt = (int)t; }
        }
        if (bt < 0) break;
        float cx = (pts[tris[bt] * 2] + pts[tris[bt + 1] * 2] + pts[tris[bt + 2] * 2]) / 3, cy = (pts[tris[bt] * 2 + 1] + pts[tris[bt + 1] * 2 + 1] + pts[tris[bt + 2] * 2 + 1]) / 3;
        pts.push_back(cx); pts.push_back(cy);
        added.push_back(cx); added.push_back(cy);
        tris = triangulateMesh(pts, hullLength);
    }
    return added;
}

std::vector<float> generateInteriorPoints(const std::vector<float>& hull, float spacing) {
    std::vector<float> out;
    std::vector<P2> p = toPoints(hull);
    int n = (int)p.size();
    if (n < 3 || spacing <= 0) return out;
    double minX = p[0].x, maxX = p[0].x, minY = p[0].y, maxY = p[0].y;
    for (P2 q : p) {
        minX = std::min(minX, q.x); maxX = std::max(maxX, q.x);
        minY = std::min(minY, q.y); maxY = std::max(maxY, q.y);
    }
    int cols = (int)std::floor((maxX - minX) / spacing), rows = (int)std::floor((maxY - minY) / spacing);
    if ((size_t)(cols + 1) * (rows + 1) > 4000000) return out;
    double x0 = minX + ((maxX - minX) - cols * spacing) * 0.5, y0 = minY + ((maxY - minY) - rows * spacing) * 0.5;
    for (int r = 0; r <= rows; r++)
        for (int c = 0; c <= cols; c++) {
            P2 q{x0 + c * spacing, y0 + r * spacing};
            if (!pointInPolygon(hull, (float)q.x, (float)q.y)) continue;
            bool ok = true;
            for (int i = 0; i < n && ok; i++)
                if (pointSegmentDist(q, p[i], p[(i + 1) % n]) < spacing * 0.5) ok = false;
            if (ok) { out.push_back((float)q.x); out.push_back((float)q.y); }
        }
    return out;
}

// Least-squares affine map dst = M * (src, 1) over matching points (exact for an unedited mesh). cx/cy: x and y rows.
bool fitAffine(const std::vector<float>& src, const std::vector<float>& dst, int n, double cx[3], double cy[3]) {
    double sx[3][3] = {}, rx[3] = {}, ry[3] = {};
    for (int k = 0; k < n; k++) {
        double u = src[k * 2], v = src[k * 2 + 1], row[3] = {u, v, 1};
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) sx[r][c] += row[r] * row[c];
            rx[r] += row[r] * dst[k * 2];
            ry[r] += row[r] * dst[k * 2 + 1];
        }
    }
    auto solve = [&](double b[3], double out[3]) {
        double m[3][4];
        for (int r = 0; r < 3; r++) { for (int c = 0; c < 3; c++) m[r][c] = sx[r][c]; m[r][3] = b[r]; }
        for (int c = 0; c < 3; c++) {
            int piv = c;
            for (int r = c + 1; r < 3; r++) if (std::fabs(m[r][c]) > std::fabs(m[piv][c])) piv = r;
            for (int k = 0; k < 4; k++) std::swap(m[c][k], m[piv][k]);
            if (std::fabs(m[c][c]) < 1e-12) return false;
            for (int r = 0; r < 3; r++) if (r != c) { double f = m[r][c] / m[c][c]; for (int k = 0; k < 4; k++) m[r][k] -= f * m[c][k]; }
        }
        for (int r = 0; r < 3; r++) out[r] = m[r][3] / m[r][r];
        return true;
    };
    return solve(rx, cx) && solve(ry, cy);
}

// ---- Weights ------------------------------------------------------------------------------

namespace {

struct Influence { int bone; float x, y, w; };
using Weighted = std::vector<std::vector<Influence>>;

Weighted decode(const Attachment& a) {
    Weighted out;
    for (size_t b = 0, v = 0; b < a.bones.size();) {
        int count = a.bones[b++];
        std::vector<Influence> inf;
        for (int i = 0; i < count && b < a.bones.size() && v + 2 < a.vertices.size(); i++, b++, v += 3)
            inf.push_back({a.bones[b], a.vertices[v], a.vertices[v + 1], a.vertices[v + 2]});
        out.push_back(std::move(inf));
    }
    return out;
}

void encode(Attachment& a, const Weighted& wv) {
    a.bones.clear();
    a.vertices.clear();
    for (auto& inf : wv) {
        a.bones.push_back((int)inf.size());
        for (auto& i : inf) {
            a.bones.push_back(i.bone);
            a.vertices.insert(a.vertices.end(), {i.x, i.y, i.w});
        }
    }
    a.worldVerticesLength = (int)wv.size() * 2;
}

Vec2 influenceWorld(const Skeleton& sk, const std::vector<Influence>& inf) {
    Vec2 r;
    for (auto& i : inf) {
        if (i.bone < 0 || i.bone >= (int)sk.bones.size()) continue;
        r = r + sk.bones[i.bone].localToWorld({i.x, i.y}) * i.w;
    }
    return r;
}

/** Rewrites each influence's local coordinates so it maps to `world` in the current pose. */
void relocate(const Skeleton& sk, std::vector<Influence>& inf, Vec2 world) {
    for (auto& i : inf) {
        Vec2 l = sk.bones[i.bone].worldToLocal(world);
        i.x = l.x; i.y = l.y;
    }
}

void normalize(std::vector<Influence>& inf) {
    if (inf.empty()) return;
    float sum = 0;
    for (auto& i : inf) sum += i.w;
    if (sum <= 1e-12f) for (auto& i : inf) i.w = 1.0f / inf.size();
    else for (auto& i : inf) i.w /= sum;
}

} // namespace

int countVertices(const Attachment& mesh) {
    if (!mesh.isWeighted()) return (int)mesh.vertices.size() / 2;
    int n = 0;
    for (size_t b = 0; b < mesh.bones.size(); b += mesh.bones[b] + 1) n++;
    return n;
}

std::vector<float> meshWorldVertices(const Attachment& mesh, const Skeleton& sk, int slotBone) {
    std::vector<float> out;
    if (mesh.isWeighted()) {
        for (auto& inf : decode(mesh)) {
            Vec2 w = influenceWorld(sk, inf);
            out.push_back(w.x); out.push_back(w.y);
        }
    } else {
        const Bone& b = sk.bones[slotBone];
        for (size_t i = 0; i + 1 < mesh.vertices.size(); i += 2) {
            Vec2 w = b.localToWorld({mesh.vertices[i], mesh.vertices[i + 1]});
            out.push_back(w.x); out.push_back(w.y);
        }
    }
    return out;
}

namespace {

using Adjacency = std::vector<std::vector<std::pair<int, double>>>; // vertex -> (neighbour, edge length)

/** Edges of the triangulation. Hull and interior edges alike connect neighbours. */
Adjacency buildAdjacency(const Attachment& mesh, const std::vector<P2>& pts) {
    Adjacency adj(pts.size());
    std::set<std::pair<int, int>> seen;
    auto edge = [&](int a, int b) {
        if (a < 0 || b < 0 || a >= (int)pts.size() || b >= (int)pts.size() || a == b) return;
        if (!seen.insert({std::min(a, b), std::max(a, b)}).second) return;
        double dx = pts[a].x - pts[b].x, dy = pts[a].y - pts[b].y, l = std::sqrt(dx * dx + dy * dy);
        adj[a].push_back({b, l});
        adj[b].push_back({a, l});
    };
    for (size_t t = 0; t + 2 < mesh.triangles.size(); t += 3) {
        edge(mesh.triangles[t], mesh.triangles[t + 1]);
        edge(mesh.triangles[t + 1], mesh.triangles[t + 2]);
        edge(mesh.triangles[t + 2], mesh.triangles[t]);
    }
    return adj;
}

/**
 * Distance from every vertex to a bone, measured along the mesh. The vertices nearest the bone are the sources;
 * everything else is reached through the mesh edges, so a gap in the mesh stops a bone's influence instead of the
 * weights leaking straight across it. Unreachable vertices get infinity.
 */
std::vector<double> topologyDistance(const std::vector<P2>& pts, const Adjacency& adj, P2 a, P2 b, double meanEdge) {
    const double inf = 1e300;
    size_t n = pts.size();
    std::vector<double> d0(n), dist(n, inf);
    double minD = inf;
    for (size_t v = 0; v < n; v++) { d0[v] = pointSegmentDist(pts[v], a, b); minD = std::min(minD, d0[v]); }
    // Sources: the vertices that are as close to the bone as any, plus the vertex nearest each point along the bone,
    // so a long bone starts from every part of the mesh it passes through but not from far-off vertices that only
    // happen to be near it across a gap.
    std::vector<char> source(n, 0);
    for (size_t v = 0; v < n; v++) if (d0[v] <= minD * 1.05 + 1e-6) source[v] = 1;
    double len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
    int samples = std::clamp((int)std::ceil(len / std::max(meanEdge * 0.5, 1e-3)) + 1, 2, 64);
    for (int k = 0; k < samples; k++) {
        double t = samples > 1 ? (double)k / (samples - 1) : 0;
        P2 p{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
        size_t best = 0;
        double bestD = inf;
        for (size_t v = 0; v < n; v++) {
            double dx = pts[v].x - p.x, dy = pts[v].y - p.y, dd = dx * dx + dy * dy;
            if (dd < bestD) { bestD = dd; best = v; }
        }
        source[best] = 1;
    }
    using QE = std::pair<double, int>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> queue;
    for (size_t v = 0; v < n; v++)
        if (source[v]) { dist[v] = d0[v]; queue.push({dist[v], (int)v}); }
    while (!queue.empty()) {
        auto [d, u] = queue.top();
        queue.pop();
        if (d > dist[u]) continue;
        for (auto& [v, len] : adj[u])
            if (d + len < dist[v]) { dist[v] = d + len; queue.push({dist[v], v}); }
    }
    return dist;
}

/** Automatic weights, one row per vertex with one weight per entry of `bones`, each row summing to 1. */
std::vector<std::vector<float>> autoWeightMatrix(const Attachment& mesh, const Skeleton& sk, int slotBone, const std::vector<int>& bones, float falloff) {
    std::vector<float> world = meshWorldVertices(mesh, sk, slotBone);
    std::vector<P2> pts = toPoints(world);
    size_t n = pts.size();
    Adjacency adj = buildAdjacency(mesh, pts);
    double edgeSum = 0;
    size_t edgeCount = 0;
    for (auto& list : adj) for (auto& e : list) { edgeSum += e.second; edgeCount++; }
    double meanEdge = edgeCount ? edgeSum / edgeCount : 0;
    std::vector<std::vector<float>> m(n, std::vector<float>(bones.size(), 0.0f));
    for (size_t j = 0; j < bones.size(); j++) {
        const Bone& bone = sk.bones[bones[j]];
        float len = sk.data ? sk.data->bones[bones[j]].length : 0;
        Vec2 tip = bone.localToWorld({len, 0});
        P2 a{bone.worldX, bone.worldY}, b{tip.x, tip.y};
        std::vector<double> dist;
        if (edgeCount == 0) { // no triangulation to follow: plain distance to the bone
            dist.resize(n);
            for (size_t v = 0; v < n; v++) dist[v] = pointSegmentDist(pts[v], a, b);
        } else dist = topologyDistance(pts, adj, a, b, meanEdge);
        for (size_t v = 0; v < n; v++) {
            double d = dist[v] < 1e299 ? dist[v] : pointSegmentDist(pts[v], a, b) * 4 + 1e3; // an isolated piece: far, but still weighted
            m[v][j] = (float)std::min(1e30, 1.0 / std::pow(std::max(1e-4, d), (double)falloff));
        }
    }
    for (auto& row : m) {
        float sum = 0;
        for (float w : row) sum += w;
        for (float& w : row) w = sum > 0 ? w / sum : 1.0f / row.size();
    }
    return m;
}

} // namespace

void bindWeights(Attachment& mesh, const Skeleton& sk, int slotBone, const std::vector<int>& bones, float falloff) {
    std::vector<int> valid;
    for (int b : bones) if (b >= 0 && b < (int)sk.bones.size() && std::find(valid.begin(), valid.end(), b) == valid.end()) valid.push_back(b);
    if (valid.empty()) return;
    std::vector<float> world = meshWorldVertices(mesh, sk, slotBone);
    int n = (int)world.size() / 2;
    std::vector<std::vector<float>> m = autoWeightMatrix(mesh, sk, slotBone, valid, falloff);
    Weighted wv(n);
    for (int v = 0; v < n; v++) {
        std::vector<Influence> all;
        for (size_t s = 0; s < valid.size(); s++) all.push_back({valid[s], 0, 0, m[v][s]});
        std::stable_sort(all.begin(), all.end(), [](const Influence& a, const Influence& b) { return a.w > b.w; });
        if (all.size() > 4) all.resize(4);
        normalize(all);
        relocate(sk, all, {world[v * 2], world[v * 2 + 1]});
        wv[v] = std::move(all);
    }
    encode(mesh, wv);
}

void unbindWeights(Attachment& mesh, const Skeleton& sk, int slotBone) {
    if (!mesh.isWeighted()) return;
    std::vector<float> world = meshWorldVertices(mesh, sk, slotBone);
    const Bone& b = sk.bones[slotBone];
    mesh.bones.clear();
    mesh.vertices.resize(world.size());
    for (size_t i = 0; i + 1 < world.size(); i += 2) {
        Vec2 l = b.worldToLocal({world[i], world[i + 1]});
        mesh.vertices[i] = l.x; mesh.vertices[i + 1] = l.y;
    }
    mesh.worldVerticesLength = (int)world.size();
}

void normalizeWeights(Attachment& mesh) {
    if (!mesh.isWeighted()) return;
    Weighted wv = decode(mesh);
    for (auto& inf : wv) normalize(inf);
    encode(mesh, wv);
}

namespace {

/** The weights a prune with these settings would keep on one vertex (always at least the strongest). */
std::vector<Influence> prunedInfluences(const std::vector<Influence>& inf, float threshold, int maxBones) {
    if (inf.empty()) return inf;
    std::vector<Influence> sorted = inf;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Influence& a, const Influence& b) { return a.w > b.w; });
    std::vector<Influence> kept;
    for (const Influence& i : sorted) {
        if (maxBones > 0 && (int)kept.size() >= maxBones) break;
        if (i.w >= threshold || kept.empty()) kept.push_back(i);
    }
    return kept;
}

} // namespace

void pruneWeights(Attachment& mesh, float threshold, int maxBones) {
    if (!mesh.isWeighted()) return;
    Weighted wv = decode(mesh);
    for (auto& inf : wv) {
        std::vector<Influence> kept = prunedInfluences(inf, threshold, maxBones);
        if (kept.size() == inf.size()) continue; // nothing removed: leave the weights exactly as they are
        normalize(kept); // the removed weight goes to the bones that stay
        inf = std::move(kept);
    }
    encode(mesh, wv);
}

int pruneCount(const Attachment& mesh, float threshold, int maxBones) {
    if (!mesh.isWeighted()) return 0;
    int removed = 0;
    for (auto& inf : decode(mesh)) removed += (int)inf.size() - (int)prunedInfluences(inf, threshold, maxBones).size();
    return removed;
}

namespace {

bool isLockedBone(const std::vector<int>& locked, int bone) { return std::find(locked.begin(), locked.end(), bone) != locked.end(); }

/**
 * Sets `bone`'s weight on one vertex. The other bones give up (or take) weight in proportion so the total stays
 * 100%, except locked bones, which keep theirs. The vertex stays where it is in the world.
 */
void setWeightOn(std::vector<Influence>& inf, const Skeleton& sk, int bone, float weight, const std::vector<int>& locked) {
    if (isLockedBone(locked, bone)) return; // a locked bone's weights never change
    Vec2 world = influenceWorld(sk, inf);
    auto it = std::find_if(inf.begin(), inf.end(), [&](const Influence& i) { return i.bone == bone; });
    if (it == inf.end()) { inf.push_back({bone, 0, 0, 0}); it = inf.end() - 1; }
    int self = (int)(it - inf.begin());
    float lockedSum = 0, others = 0;
    int otherCount = 0;
    for (int i = 0; i < (int)inf.size(); i++) {
        if (i == self) continue;
        if (isLockedBone(locked, inf[i].bone)) lockedSum += inf[i].w;
        else { others += inf[i].w; otherCount++; }
    }
    float room = std::max(0.0f, 1 - lockedSum); // what the locked bones leave to share
    weight = clampf(weight, 0, room);
    if (otherCount == 0) weight = room; // the only free bone carries the whole vertex: it cannot go below 100% of the room
    float remaining = room - weight;
    for (int i = 0; i < (int)inf.size(); i++) {
        if (i == self) inf[i].w = weight;
        else if (isLockedBone(locked, inf[i].bone)) continue;
        else if (others > 1e-9f) inf[i].w *= remaining / others;
        else inf[i].w = remaining / otherCount;
    }
    std::vector<Influence> kept;
    for (auto& i : inf) if (i.w > 0) kept.push_back(i);
    if (kept.empty()) kept.push_back({bone, 0, 0, 1});
    normalize(kept);
    relocate(sk, kept, world);
    inf = std::move(kept);
}

/** Neighbouring vertices (sharing a triangle edge) for each vertex. */
std::vector<std::vector<int>> vertexNeighbours(const Attachment& mesh, int vertexCount) {
    std::vector<std::set<int>> sets(vertexCount);
    for (size_t t = 0; t + 2 < mesh.triangles.size(); t += 3)
        for (int e = 0; e < 3; e++) {
            int a = mesh.triangles[t + e], b = mesh.triangles[t + (e + 1) % 3];
            if (a >= 0 && b >= 0 && a < vertexCount && b < vertexCount && a != b) { sets[a].insert(b); sets[b].insert(a); }
        }
    std::vector<std::vector<int>> out(vertexCount);
    for (int v = 0; v < vertexCount; v++) out[v].assign(sets[v].begin(), sets[v].end());
    return out;
}

} // namespace

void setVertexWeight(Attachment& mesh, const Skeleton& sk, int vertex, int bone, float weight, const std::vector<int>& locked) {
    if (!mesh.isWeighted() || bone < 0 || bone >= (int)sk.bones.size()) return;
    Weighted wv = decode(mesh);
    if (vertex < 0 || vertex >= (int)wv.size()) return;
    setWeightOn(wv[vertex], sk, bone, weight, locked);
    encode(mesh, wv);
}

void setVertexWeights(Attachment& mesh, const Skeleton& sk, const std::vector<VertexWeightEdit>& edits, const std::vector<int>& locked) {
    if (!mesh.isWeighted() || edits.empty()) return;
    Weighted wv = decode(mesh);
    for (const VertexWeightEdit& e : edits) {
        if (e.vertex < 0 || e.vertex >= (int)wv.size() || e.bone < 0 || e.bone >= (int)sk.bones.size()) continue;
        setWeightOn(wv[e.vertex], sk, e.bone, e.weight, locked);
    }
    encode(mesh, wv);
}

float boneWeightAt(const Attachment& mesh, int vertex, int bone) {
    if (!mesh.isWeighted()) return 0;
    size_t bi = 0, vi = 0;
    for (int v = 0; v < vertex && bi < mesh.bones.size(); v++) { int n = mesh.bones[bi++]; bi += n; vi += n * 3; }
    if (bi >= mesh.bones.size()) return 0;
    int n = mesh.bones[bi++];
    for (int q = 0; q < n; q++) if (mesh.bones[bi + q] == bone) return mesh.vertices[vi + q * 3 + 2];
    return 0;
}

std::vector<int> boundBones(const Attachment& mesh) {
    std::vector<int> bound;
    if (!mesh.isWeighted()) return bound;
    for (size_t i = 0; i < mesh.bones.size();) {
        int n = mesh.bones[i++];
        for (int k = 0; k < n && i < mesh.bones.size(); k++, i++)
            if (std::find(bound.begin(), bound.end(), mesh.bones[i]) == bound.end()) bound.push_back(mesh.bones[i]);
    }
    std::sort(bound.begin(), bound.end());
    return bound;
}

void autoWeights(Attachment& mesh, const Skeleton& sk, int slotBone, const std::vector<int>& bones, const std::vector<int>& vertices,
                 const std::vector<int>& onlyBones, float falloff) {
    std::vector<int> valid;
    for (int b : bones) if (b >= 0 && b < (int)sk.bones.size() && std::find(valid.begin(), valid.end(), b) == valid.end()) valid.push_back(b);
    if (valid.empty()) return;
    if (!mesh.isWeighted()) { bindWeights(mesh, sk, slotBone, valid, falloff); return; } // the first binding is automatic
    std::vector<std::vector<float>> m = autoWeightMatrix(mesh, sk, slotBone, valid, falloff);
    Weighted wv = decode(mesh);
    std::vector<int> list = vertices;
    if (list.empty()) for (int v = 0; v < (int)wv.size(); v++) list.push_back(v);
    for (int v : list) {
        if (v < 0 || v >= (int)wv.size()) continue;
        std::vector<Influence>& inf = wv[v];
        Vec2 world = influenceWorld(sk, inf);
        std::vector<Influence> next;
        if (onlyBones.empty()) {
            for (size_t s = 0; s < valid.size(); s++) next.push_back({valid[s], 0, 0, m[v][s]});
            std::stable_sort(next.begin(), next.end(), [](const Influence& a, const Influence& b) { return a.w > b.w; });
            if (next.size() > 4) next.resize(4);
        } else {
            // Only the chosen bones are recomputed: each gets the weight auto would give it, and the other bones
            // share what is left in the proportions they already had.
            float share = 0, otherSum = 0;
            for (size_t s = 0; s < valid.size(); s++)
                if (std::find(onlyBones.begin(), onlyBones.end(), valid[s]) != onlyBones.end()) share += m[v][s];
            for (auto& i : inf) if (std::find(onlyBones.begin(), onlyBones.end(), i.bone) == onlyBones.end()) otherSum += i.w;
            for (size_t s = 0; s < valid.size(); s++)
                if (std::find(onlyBones.begin(), onlyBones.end(), valid[s]) != onlyBones.end()) next.push_back({valid[s], 0, 0, m[v][s]});
            for (auto& i : inf)
                if (std::find(onlyBones.begin(), onlyBones.end(), i.bone) == onlyBones.end())
                    next.push_back({i.bone, 0, 0, otherSum > 1e-9f ? i.w * (1 - share) / otherSum : 0});
        }
        next.erase(std::remove_if(next.begin(), next.end(), [](const Influence& i) { return i.w <= 1e-6f; }), next.end());
        if (next.empty()) continue;
        normalize(next);
        relocate(sk, next, world);
        inf = std::move(next);
    }
    encode(mesh, wv);
}

void addBonesToBinding(Attachment& mesh, const Skeleton& sk, int slotBone, const std::vector<int>& newBones, float falloff) {
    if (!mesh.isWeighted()) { bindWeights(mesh, sk, slotBone, newBones, falloff); return; }
    std::vector<int> all = boundBones(mesh);
    std::vector<int> fresh;
    for (int b : newBones)
        if (b >= 0 && b < (int)sk.bones.size() && std::find(all.begin(), all.end(), b) == all.end() && std::find(fresh.begin(), fresh.end(), b) == fresh.end()) fresh.push_back(b);
    if (fresh.empty()) return;
    for (int b : fresh) all.push_back(b);
    // Each new bone takes the weight automatic weighting would give it; the bones already there share the rest.
    std::vector<std::vector<float>> m = autoWeightMatrix(mesh, sk, slotBone, all, falloff);
    Weighted wv = decode(mesh);
    for (int v = 0; v < (int)wv.size(); v++)
        for (int b : fresh) {
            size_t idx = std::find(all.begin(), all.end(), b) - all.begin();
            if (m[v][idx] > 0.02f) setWeightOn(wv[v], sk, b, m[v][idx], {});
        }
    encode(mesh, wv);
}

void smoothWeights(Attachment& mesh, const Skeleton& sk, const std::vector<int>& vertices, float amount) {
    if (!mesh.isWeighted()) return;
    Weighted wv = decode(mesh);
    std::vector<std::vector<int>> nb = vertexNeighbours(mesh, (int)wv.size());
    std::vector<int> list = vertices;
    if (list.empty()) for (int v = 0; v < (int)wv.size(); v++) list.push_back(v);
    Weighted old = wv; // every vertex is averaged from the weights as they were before this pass
    amount = clampf(amount, 0, 1);
    for (int v : list) {
        if (v < 0 || v >= (int)wv.size() || nb[v].empty()) continue;
        std::map<int, float> sum;
        for (int n : nb[v]) for (auto& i : old[n]) sum[i.bone] += i.w / nb[v].size();
        std::map<int, float> own;
        for (auto& i : old[v]) own[i.bone] = i.w;
        std::map<int, float> out;
        for (auto& [b, w] : own) out[b] += (1 - amount) * w;
        for (auto& [b, w] : sum) out[b] += amount * w;
        Vec2 world = influenceWorld(sk, old[v]);
        std::vector<Influence> next;
        for (auto& [b, w] : out) if (w > 1e-5f) next.push_back({b, 0, 0, w});
        if (next.empty()) continue;
        normalize(next);
        relocate(sk, next, world);
        wv[v] = std::move(next);
    }
    encode(mesh, wv);
}

void swapBoneWeights(Attachment& mesh, const Skeleton& sk, int boneA, int boneB) {
    if (!mesh.isWeighted() || boneA == boneB) return;
    Weighted wv = decode(mesh);
    for (auto& inf : wv) {
        Vec2 world = influenceWorld(sk, inf);
        auto a = std::find_if(inf.begin(), inf.end(), [&](const Influence& i) { return i.bone == boneA; });
        auto b = std::find_if(inf.begin(), inf.end(), [&](const Influence& i) { return i.bone == boneB; });
        if (a != inf.end() && b != inf.end()) std::swap(a->w, b->w);
        else if (a != inf.end()) a->bone = boneB;
        else if (b != inf.end()) b->bone = boneA;
        else continue;
        relocate(sk, inf, world);
    }
    encode(mesh, wv);
}

void updateBindings(Attachment& mesh, const Skeleton& sk) {
    if (!mesh.isWeighted()) return;
    Weighted wv = decode(mesh);
    // Where each vertex is now becomes its new bind position, relative to the bones as they are now.
    for (auto& inf : wv) relocate(sk, inf, influenceWorld(sk, inf));
    encode(mesh, wv);
}

void removeBoneWeights(Attachment& mesh, const Skeleton& sk, int slotBone, int bone) {
    if (!mesh.isWeighted()) return;
    Weighted wv = decode(mesh);
    for (auto& inf : wv) {
        Vec2 world = influenceWorld(sk, inf);
        inf.erase(std::remove_if(inf.begin(), inf.end(), [&](const Influence& i) { return i.bone == bone; }), inf.end());
        if (inf.empty()) inf.push_back({slotBone, 0, 0, 1}); // nothing left: the vertex follows the slot's bone
        normalize(inf);
        relocate(sk, inf, world);
    }
    encode(mesh, wv);
    bool onlySlotBone = true;
    for (auto& inf : decode(mesh)) for (auto& i : inf) onlySlotBone &= i.bone == slotBone;
    if (onlySlotBone) unbindWeights(mesh, sk, slotBone); // no bones left: the mesh is no longer weighted
}

WeightSet copyWeights(const Attachment& mesh, const std::vector<int>& vertices) {
    WeightSet out;
    if (!mesh.isWeighted()) return out;
    Weighted wv = decode(mesh);
    for (int v : vertices) {
        std::vector<std::pair<int, float>> row;
        if (v >= 0 && v < (int)wv.size()) for (auto& i : wv[v]) row.push_back({i.bone, i.w});
        out.push_back(row);
    }
    return out;
}

bool pasteWeights(Attachment& mesh, const Skeleton& sk, const std::vector<int>& vertices, const WeightSet& set) {
    if (!mesh.isWeighted() || set.empty() || set.size() != vertices.size()) return false; // the same number of vertices
    Weighted wv = decode(mesh);
    for (size_t k = 0; k < vertices.size(); k++) {
        int v = vertices[k];
        if (v < 0 || v >= (int)wv.size()) continue;
        Vec2 world = influenceWorld(sk, wv[v]);
        std::vector<Influence> next;
        for (auto& [bone, w] : set[k]) if (bone >= 0 && bone < (int)sk.bones.size() && w > 0) next.push_back({bone, 0, 0, w});
        if (next.empty()) continue;
        normalize(next);
        relocate(sk, next, world);
        wv[v] = std::move(next);
    }
    encode(mesh, wv);
    return true;
}

// ---- Atlas packing ------------------------------------------------------------------------

namespace {

struct Rect { int x, y, w, h; };

bool contains(const Rect& a, const Rect& b) { return b.x >= a.x && b.y >= a.y && b.x + b.w <= a.x + a.w && b.y + b.h <= a.y + a.h; }

struct MaxRectsBin {
    std::vector<Rect> free;
    explicit MaxRectsBin(int w, int h) { free.push_back({0, 0, w, h}); }

    /** Best short side fit; returns false if it does not fit. */
    bool find(int w, int h, Rect& out, int& shortFit, int& longFit) const {
        bool found = false;
        for (const Rect& f : free) {
            if (w > f.w || h > f.h) continue;
            int lh = f.w - w, lv = f.h - h, s = std::min(lh, lv), l = std::max(lh, lv);
            if (!found || s < shortFit || (s == shortFit && l < longFit)) {
                out = {f.x, f.y, w, h}; shortFit = s; longFit = l; found = true;
            }
        }
        return found;
    }

    void place(const Rect& r) {
        std::vector<Rect> next;
        for (const Rect& f : free) {
            if (r.x >= f.x + f.w || r.x + r.w <= f.x || r.y >= f.y + f.h || r.y + r.h <= f.y) { next.push_back(f); continue; }
            if (r.x > f.x) next.push_back({f.x, f.y, r.x - f.x, f.h});
            if (r.x + r.w < f.x + f.w) next.push_back({r.x + r.w, f.y, f.x + f.w - (r.x + r.w), f.h});
            if (r.y > f.y) next.push_back({f.x, f.y, f.w, r.y - f.y});
            if (r.y + r.h < f.y + f.h) next.push_back({f.x, r.y + r.h, f.w, f.y + f.h - (r.y + r.h)});
        }
        free.clear();
        for (size_t i = 0; i < next.size(); i++) {
            bool redundant = false;
            for (size_t j = 0; j < next.size() && !redundant; j++)
                if (i != j && contains(next[j], next[i]) && (!contains(next[i], next[j]) || j < i)) redundant = true;
            if (!redundant) free.push_back(next[i]);
        }
    }
};

int floorPow2(int v) { int p = 1; while (p * 2 <= v) p *= 2; return p; }
int ceilPow2(int v) { int p = 1; while (p < v) p *= 2; return p; }

} // namespace

int packRects(std::vector<PackRect>& rects, int maxW, int maxH, int padding, bool allowRotate, bool pow2) {
    if (pow2) { maxW = floorPow2(std::max(1, maxW)); maxH = floorPow2(std::max(1, maxH)); }
    padding = std::max(0, padding);
    std::vector<int> order(rects.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        int ma = std::max(rects[a].w, rects[a].h), mb = std::max(rects[b].w, rects[b].h);
        if (ma != mb) return ma > mb;
        return rects[a].w * rects[a].h > rects[b].w * rects[b].h;
    });
    // Each rect reserves padding on its right/bottom; the bin grows by padding to compensate,
    // so rects stay within maxW x maxH and are at least `padding` apart.
    std::vector<MaxRectsBin> pages;
    for (int i : order) {
        PackRect& r = rects[i];
        int w = r.w + padding, h = r.h + padding;
        r.page = -1;
        r.rotated = false;
        auto tryBin = [&](const MaxRectsBin& bin, Rect& best, bool& rot) {
            int s = INT_MAX, l = INT_MAX, s2, l2;
            Rect c;
            bool ok = false;
            if (bin.find(w, h, c, s2, l2)) { best = c; s = s2; l = l2; rot = false; ok = true; }
            if (allowRotate && r.w != r.h && bin.find(h, w, c, s2, l2) && (!ok || s2 < s || (s2 == s && l2 < l))) {
                best = c; rot = true; ok = true;
            }
            return ok;
        };
        Rect placed;
        bool rot = false;
        for (size_t p = 0; p < pages.size() && r.page < 0; p++)
            if (tryBin(pages[p], placed, rot)) r.page = (int)p;
        if (r.page < 0) {
            MaxRectsBin bin(maxW + padding, maxH + padding);
            if (!tryBin(bin, placed, rot)) continue; // too large for any page
            pages.push_back(bin);
            r.page = (int)pages.size() - 1;
        }
        pages[r.page].place(placed);
        r.x = placed.x;
        r.y = placed.y;
        r.rotated = rot;
    }
    return (int)pages.size();
}

std::pair<int, int> packedPageSize(const std::vector<PackRect>& rects, int page, bool pow2) {
    int w = 0, h = 0;
    for (const PackRect& r : rects) {
        if (r.page != page) continue;
        w = std::max(w, r.x + (r.rotated ? r.h : r.w));
        h = std::max(h, r.y + (r.rotated ? r.w : r.h));
    }
    if (pow2) { w = ceilPow2(std::max(1, w)); h = ceilPow2(std::max(1, h)); }
    return {w, h};
}

std::string writeAtlas(const std::vector<std::string>& pageFiles, const std::vector<std::pair<int, int>>& pageSizes,
                       const std::vector<PackRect>& rects, const std::vector<std::string>& names) {
    std::ostringstream s;
    for (size_t p = 0; p < pageFiles.size(); p++) {
        if (p > 0) s << "\n";
        s << pageFiles[p] << "\n";
        if (p < pageSizes.size()) s << "size:" << pageSizes[p].first << "," << pageSizes[p].second << "\n";
        s << "filter:Linear,Linear\n";
        for (size_t i = 0; i < rects.size(); i++) {
            const PackRect& r = rects[i];
            if (r.page != (int)p) continue;
            s << (i < names.size() ? names[i] : std::to_string(r.id)) << "\n";
            s << "bounds:" << r.x << "," << r.y << "," << r.w << "," << r.h << "\n";
            if (r.rotated) s << "rotate:90\n";
        }
    }
    return s.str();
}

} // namespace osteon
