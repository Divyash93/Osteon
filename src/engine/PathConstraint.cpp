// Path constraint solver: lays bones out along a cubic Bezier path attachment.
#include "Skeleton.h"

namespace osteon {

namespace {

constexpr float EPS = 0.00001f;
constexpr int SUBDIV = 10; // segments per curve for arc-length sampling

struct Curve { float x0, y0, x1, y1, x2, y2, x3, y3; };

Curve curveAt(const std::vector<float>& w, int i, int vertexCount) {
    // Curve i spans vertices 3i+1 (point), 3i+2 (out), 3i+3 (in), 3i+4 (point), wrapping for closed paths.
    int k[4];
    for (int j = 0; j < 4; j++) k[j] = ((3 * i + 1 + j) % vertexCount) * 2;
    return {w[k[0]], w[k[0] + 1], w[k[1]], w[k[1] + 1], w[k[2]], w[k[2] + 1], w[k[3]], w[k[3] + 1]};
}

Vec2 bezierPoint(const Curve& c, float t) {
    float mt = 1 - t, a = mt * mt * mt, b = 3 * mt * mt * t, cc = 3 * mt * t * t, d = t * t * t;
    return {a * c.x0 + b * c.x1 + cc * c.x2 + d * c.x3, a * c.y0 + b * c.y1 + cc * c.y2 + d * c.y3};
}

/** Tangent angle (radians) at t, with fallbacks for degenerate handles. */
float bezierAngle(const Curve& c, float t) {
    float mt = 1 - t;
    float dx = 3 * (mt * mt * (c.x1 - c.x0) + 2 * mt * t * (c.x2 - c.x1) + t * t * (c.x3 - c.x2));
    float dy = 3 * (mt * mt * (c.y1 - c.y0) + 2 * mt * t * (c.y2 - c.y1) + t * t * (c.y3 - c.y2));
    if (dx * dx + dy * dy < EPS) {
        // Handle coincides with its point: use the direction toward the far control points.
        if (t < 0.5f) { dx = c.x2 - c.x0; dy = c.y2 - c.y0; }
        else { dx = c.x3 - c.x1; dy = c.y3 - c.y1; }
        if (dx * dx + dy * dy < EPS) { dx = c.x3 - c.x0; dy = c.y3 - c.y0; }
    }
    return std::atan2(dy, dx);
}

/** Fills seg[0..SUBDIV] with cumulative arc length within the curve; returns the curve length. */
float sampleCurve(const Curve& c, float* seg) {
    seg[0] = 0;
    Vec2 prev{c.x0, c.y0};
    for (int s = 1; s <= SUBDIV; s++) {
        Vec2 p = bezierPoint(c, (float)s / SUBDIV);
        seg[s] = seg[s - 1] + (p - prev).len();
        prev = p;
    }
    return seg[SUBDIV];
}

} // namespace

void applyPath(Skeleton& sk, int index) {
    const PathConstraintData& d = sk.data->paths[index];
    PathConstraint& c = sk.paths[index];
    if (!c.active) return;
    if (c.mixRotate == 0 && c.mixX == 0 && c.mixY == 0) return;
    if (d.target < 0 || d.target >= (int)sk.slots.size()) return;
    const Slot& slot = sk.slots[d.target];
    const Attachment* att = sk.attachmentFor(slot);
    if (!att || att->type != AttachmentType::Path) return;
    int boneCount = (int)d.bones.size();
    if (boneCount == 0) return;

    // ---- Path geometry in world space.
    std::vector<float>& world = c.world;
    if (!sk.worldVertices(slot, *att, world)) return;
    int vertexCount = (int)world.size() / 2;
    bool closed = att->closed;
    int curveCount = closed ? vertexCount / 3 : vertexCount / 3 - 1;
    if (curveCount < 1) return;

    std::vector<float>& curves = c.curves;     // cumulative curve lengths
    std::vector<float>& segments = c.segments; // per-curve arc-length tables (constant speed)
    curves.assign(curveCount, 0);
    bool constantSpeed = att->constantSpeed || (int)att->lengths.size() < curveCount;
    if (constantSpeed) {
        segments.assign((size_t)curveCount * (SUBDIV + 1), 0);
        float total = 0;
        for (int i = 0; i < curveCount; i++) {
            total += sampleCurve(curveAt(world, i, vertexCount), &segments[(size_t)i * (SUBDIV + 1)]);
            curves[i] = total;
        }
    } else {
        for (int i = 0; i < curveCount; i++) curves[i] = att->lengths[i];
    }
    float pathLength = curves[curveCount - 1];

    // ---- Spaces between consecutive path samples.
    bool tangents = d.rotateMode == PathConstraintData::Rotate::Tangent;
    bool scale = d.rotateMode == PathConstraintData::Rotate::ChainScale;
    int pointCount = tangents ? boneCount : boneCount + 1;
    std::vector<float>& spaces = c.spaces;
    std::vector<float>& lengths = c.lengths; // world bone lengths
    spaces.assign(pointCount, 0);
    lengths.assign(boneCount, 0);
    float spacing = c.spacing, sum = 0;
    for (int i = 0; i < boneCount; i++) {
        const Bone& b = sk.bones[d.bones[i]];
        float setupLength = sk.data->bones[b.index].length;
        float x = setupLength * b.a, y = setupLength * b.c;
        float len = std::sqrt(x * x + y * y);
        lengths[i] = len;
        if (i + 1 >= pointCount) continue;
        float s = spacing;
        switch (d.spacingMode) {
        case PathConstraintData::Spacing::Length:
            s = setupLength < EPS ? spacing : (setupLength + spacing) * len / setupLength;
            break;
        case PathConstraintData::Spacing::Fixed: s = spacing; break;
        case PathConstraintData::Spacing::Percent: s = spacing * pathLength; break;
        case PathConstraintData::Spacing::Proportional: s = len; sum += len; break;
        }
        spaces[i + 1] = s;
    }
    if (d.spacingMode == PathConstraintData::Spacing::Proportional && sum > 0) {
        float m = spacing * pathLength / sum;
        for (int i = 1; i < pointCount; i++) spaces[i] *= m;
    }

    // ---- Sample positions (x, y, tangent radians) along the path.
    std::vector<float>& positions = c.positions;
    positions.assign((size_t)pointCount * 3, 0);
    float position = c.position;
    if (d.positionMode == PathConstraintData::Position::Percent) position *= pathLength;
    float p = position;
    for (int i = 0; i < pointCount; i++) {
        p += spaces[i];
        float q = p;
        float* out = &positions[(size_t)i * 3];
        if (closed) {
            if (pathLength > 0) {
                q = std::fmod(q, pathLength);
                if (q < 0) q += pathLength;
            } else q = 0;
        } else if (q < 0) {
            // Extrapolate before the start along the starting tangent.
            Curve cv = curveAt(world, 0, vertexCount);
            float r = bezierAngle(cv, 0);
            out[0] = cv.x0 + q * std::cos(r);
            out[1] = cv.y0 + q * std::sin(r);
            out[2] = r;
            continue;
        } else if (q > pathLength) {
            Curve cv = curveAt(world, curveCount - 1, vertexCount);
            float r = bezierAngle(cv, 1), over = q - pathLength;
            out[0] = cv.x3 + over * std::cos(r);
            out[1] = cv.y3 + over * std::sin(r);
            out[2] = r;
            continue;
        }
        // Find the curve containing q.
        int ci = 0;
        while (ci < curveCount - 1 && q > curves[ci]) ci++;
        float start = ci == 0 ? 0 : curves[ci - 1], curveLen = curves[ci] - start;
        float local = q - start, t;
        if (curveLen < EPS) t = 0;
        else if (constantSpeed) {
            const float* seg = &segments[(size_t)ci * (SUBDIV + 1)];
            int s = 1;
            while (s < SUBDIV && local > seg[s]) s++;
            float segLen = seg[s] - seg[s - 1];
            float f = segLen < EPS ? 0 : (local - seg[s - 1]) / segLen;
            t = (s - 1 + clampf(f, 0, 1)) / SUBDIV;
        } else t = clampf(local / curveLen, 0, 1);
        Curve cv = curveAt(world, ci, vertexCount);
        Vec2 pt = bezierPoint(cv, t);
        out[0] = pt.x;
        out[1] = pt.y;
        out[2] = bezierAngle(cv, t);
    }

    // ---- Apply to bones.
    float offsetRotation = d.offsetRotation;
    if (offsetRotation != 0) {
        const Bone& pb = sk.bones[slot.bone];
        offsetRotation *= pb.a * pb.d - pb.b * pb.c > 0 ? degRad : -degRad;
    }
    for (int i = 0; i < boneCount; i++) {
        Bone& b = sk.bones[d.bones[i]];
        const float* cur = &positions[(size_t)i * 3];
        b.worldX += (cur[0] - b.worldX) * c.mixX;
        b.worldY += (cur[1] - b.worldY) * c.mixY;
        float dx = 0, dy = 0;
        if (!tangents) {
            const float* next = cur + 3;
            dx = next[0] - cur[0];
            dy = next[1] - cur[1];
            if (scale && lengths[i] >= EPS) {
                float s = (std::sqrt(dx * dx + dy * dy) / lengths[i] - 1) * c.mixRotate + 1;
                b.a *= s;
                b.c *= s;
            }
        }
        if (c.mixRotate > 0) {
            float r;
            if (tangents) r = cur[2];
            else if (spaces[i + 1] < EPS) r = cur[5];
            else r = std::atan2(dy, dx);
            r = wrapDeg((r - std::atan2(b.c, b.a) + offsetRotation) * radDeg) * degRad * c.mixRotate;
            float cs = std::cos(r), sn = std::sin(r);
            float a = b.a, bb = b.b, cc = b.c, dd = b.d;
            b.a = cs * a - sn * cc;
            b.b = cs * bb - sn * dd;
            b.c = sn * a + cs * cc;
            b.d = sn * bb + cs * dd;
        }
        b.updateAppliedTransform(sk);
    }
}

} // namespace osteon
