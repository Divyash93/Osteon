// Mesh geometry tools: triangulation, image hull tracing, automatic weights, atlas packing.
#include "Geometry.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <map>
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

void douglasPeucker(const std::vector<P2>& p, int first, int last, double eps, std::vector<char>& keep) {
    std::vector<std::pair<int, int>> stack{{first, last}};
    int n = (int)p.size();
    while (!stack.empty()) {
        auto [a, b] = stack.back();
        stack.pop_back();
        double maxD = -1; int idx = -1;
        for (int i = a + 1; i < b; i++) {
            double d = pointSegmentDist(p[i % n], p[a % n], p[b % n]);
            if (d > maxD) { maxD = d; idx = i; }
        }
        if (idx >= 0 && maxD > eps) {
            keep[idx % n] = 1;
            stack.push_back({a, idx});
            stack.push_back({idx, b});
        }
    }
}

std::vector<P2> simplifyClosed(const std::vector<P2>& p, double eps) {
    int n = (int)p.size();
    if (n <= 3) return p;
    int far = 0; double farD = -1;
    for (int i = 1; i < n; i++) {
        double d = std::hypot(p[i].x - p[0].x, p[i].y - p[0].y);
        if (d > farD) { farD = d; far = i; }
    }
    std::vector<char> keep(n, 0);
    keep[0] = keep[far] = 1;
    douglasPeucker(p, 0, far, eps, keep);
    douglasPeucker(p, far, n, eps, keep);
    std::vector<P2> out;
    for (int i = 0; i < n; i++) if (keep[i]) out.push_back(p[i]);
    return out;
}

} // namespace

std::vector<float> traceHull(const uint8_t* rgba, int w, int h, int alphaThreshold, int maxVertices, float padding) {
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

void bindWeights(Attachment& mesh, const Skeleton& sk, int slotBone, const std::vector<int>& bones, float falloff) {
    std::vector<int> valid;
    for (int b : bones) if (b >= 0 && b < (int)sk.bones.size()) valid.push_back(b);
    if (valid.empty()) return;
    std::vector<float> world = meshWorldVertices(mesh, sk, slotBone);
    int n = (int)world.size() / 2;
    struct Seg { P2 a, b; };
    std::vector<Seg> segs;
    for (int b : valid) {
        const Bone& bone = sk.bones[b];
        float len = sk.data ? sk.data->bones[b].length : 0;
        Vec2 tip = bone.localToWorld({len, 0});
        segs.push_back({{bone.worldX, bone.worldY}, {tip.x, tip.y}});
    }
    Weighted wv(n);
    for (int v = 0; v < n; v++) {
        P2 q{world[v * 2], world[v * 2 + 1]};
        std::vector<Influence> all;
        for (size_t s = 0; s < segs.size(); s++) {
            double d = std::max(1e-4, pointSegmentDist(q, segs[s].a, segs[s].b));
            all.push_back({valid[s], 0, 0, (float)std::min(1e30, 1.0 / std::pow(d, (double)falloff))});
        }
        std::stable_sort(all.begin(), all.end(), [](const Influence& a, const Influence& b) { return a.w > b.w; });
        if (all.size() > 4) all.resize(4);
        normalize(all);
        relocate(sk, all, {(float)q.x, (float)q.y});
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

void pruneWeights(Attachment& mesh, float threshold) {
    if (!mesh.isWeighted()) return;
    Weighted wv = decode(mesh);
    for (auto& inf : wv) {
        if (inf.empty()) continue;
        auto strongest = *std::max_element(inf.begin(), inf.end(), [](const Influence& a, const Influence& b) { return a.w < b.w; });
        inf.erase(std::remove_if(inf.begin(), inf.end(), [&](const Influence& i) { return i.w < threshold; }), inf.end());
        if (inf.empty()) inf.push_back(strongest);
        normalize(inf);
    }
    encode(mesh, wv);
}

void setVertexWeight(Attachment& mesh, const Skeleton& sk, int vertex, int bone, float weight) {
    if (!mesh.isWeighted() || bone < 0 || bone >= (int)sk.bones.size()) return;
    Weighted wv = decode(mesh);
    if (vertex < 0 || vertex >= (int)wv.size()) return;
    auto& inf = wv[vertex];
    Vec2 world = influenceWorld(sk, inf);
    weight = clampf(weight, 0, 1);
    auto it = std::find_if(inf.begin(), inf.end(), [&](const Influence& i) { return i.bone == bone; });
    if (it == inf.end()) { inf.push_back({bone, 0, 0, 0}); it = inf.end() - 1; }
    int self = (int)(it - inf.begin());
    float others = 0;
    for (int i = 0; i < (int)inf.size(); i++) if (i != self) others += inf[i].w;
    int otherCount = (int)inf.size() - 1;
    if (otherCount == 0) weight = 1; // sole influence must carry the whole vertex
    for (int i = 0; i < (int)inf.size(); i++) {
        if (i == self) inf[i].w = weight;
        else if (others > 1e-9f) inf[i].w *= (1 - weight) / others;
        else inf[i].w = (1 - weight) / otherCount;
    }
    std::vector<Influence> kept;
    for (auto& i : inf) if (i.w > 0) kept.push_back(i);
    if (kept.empty()) kept.push_back({bone, 0, 0, 1});
    normalize(kept);
    relocate(sk, kept, world);
    inf = std::move(kept);
    encode(mesh, wv);
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
