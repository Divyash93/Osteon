// IK and transform constraint solvers.
#include "Skeleton.h"

namespace osteon {

void applyIk(Skeleton& sk, int index) {
    const IkConstraintData& d = sk.data->ik[index];
    IkConstraint& c = sk.ik[index];
    if (c.mix == 0) return;
    const Bone& target = sk.bones[d.target];
    if (d.bones.size() == 1)
        applyIk1(sk, sk.bones[d.bones[0]], target.worldX, target.worldY, c.compress, c.stretch, d.uniform, c.mix);
    else
        applyIk2(sk, sk.bones[d.bones[0]], sk.bones[d.bones[1]], target.worldX, target.worldY, c.bendDirection, c.stretch,
                 d.uniform, c.softness, c.mix);
}

void applyIk1(Skeleton& sk, Bone& bone, float targetX, float targetY, bool compress, bool stretch, bool uniform, float alpha) {
    const WorldMatrix& p = sk.parentMatrix(bone.index);
    float pa = p.a, pb = p.b, pc = p.c, pd = p.d;
    float rotationIK = -bone.ashearX - bone.arotation, tx, ty;
    switch (bone.inherit) {
    case Inherit::OnlyTranslation:
        tx = (targetX - bone.worldX) * (sk.scaleX < 0 ? -1.0f : 1.0f);
        ty = (targetY - bone.worldY) * (sk.scaleY < 0 ? -1.0f : 1.0f);
        break;
    case Inherit::NoRotationOrReflection: {
        float s = std::fabs(pa * pd - pb * pc) / std::max(0.0001f, pa * pa + pc * pc);
        float sa = pa / sk.scaleX, sc = pc / sk.scaleY;
        pb = -sc * s * sk.scaleX;
        pd = sa * s * sk.scaleY;
        rotationIK += atan2Deg(sc, sa);
    } // fall through
    default: {
        float x = targetX - p.worldX, y = targetY - p.worldY;
        float dd = pa * pd - pb * pc;
        if (std::fabs(dd) <= 0.0001f) { tx = 0; ty = 0; }
        else {
            tx = (x * pd - y * pb) / dd - bone.ax;
            ty = (y * pa - x * pc) / dd - bone.ay;
        }
    }
    }
    rotationIK += atan2Deg(ty, tx);
    if (bone.ascaleX < 0) rotationIK += 180;
    if (rotationIK > 180) rotationIK -= 360;
    else if (rotationIK < -180) rotationIK += 360;
    float sx = bone.ascaleX, sy = bone.ascaleY;
    if (compress || stretch) {
        if (bone.inherit == Inherit::NoScale || bone.inherit == Inherit::NoScaleOrReflection) {
            tx = targetX - bone.worldX;
            ty = targetY - bone.worldY;
        }
        float b = sk.data->bones[bone.index].length * sx;
        if (b > 0.0001f) {
            float dd = tx * tx + ty * ty;
            if ((compress && dd < b * b) || (stretch && dd > b * b)) {
                float s = (std::sqrt(dd) / b - 1) * alpha + 1;
                sx *= s;
                if (uniform) sy *= s;
            }
        }
    }
    bone.updateWorldTransform(sk, bone.ax, bone.ay, bone.arotation + rotationIK * alpha, sx, sy, bone.ashearX, bone.ashearY);
}

void applyIk2(Skeleton& sk, Bone& parent, Bone& child, float targetX, float targetY, int bendDir, bool stretch, bool uniform,
              float softness, float alpha) {
    if (parent.inherit != Inherit::Normal || child.inherit != Inherit::Normal) return;
    float px = parent.ax, py = parent.ay, psx = parent.ascaleX, psy = parent.ascaleY, sx = psx, sy = psy, csx = child.ascaleX;
    int os1, os2, s2;
    if (psx < 0) { psx = -psx; os1 = 180; s2 = -1; }
    else { os1 = 0; s2 = 1; }
    if (psy < 0) { psy = -psy; s2 = -s2; }
    if (csx < 0) { csx = -csx; os2 = 180; }
    else os2 = 0;
    float cx = child.ax, cy, cwx, cwy, a = parent.a, b = parent.b, c = parent.c, d = parent.d;
    bool u = std::fabs(psx - psy) <= 0.0001f;
    if (!u || stretch) {
        cy = 0;
        cwx = a * cx + parent.worldX;
        cwy = c * cx + parent.worldY;
    } else {
        cy = child.ay;
        cwx = a * cx + b * cy + parent.worldX;
        cwy = c * cx + d * cy + parent.worldY;
    }
    const WorldMatrix& pp = sk.parentMatrix(parent.index);
    a = pp.a; b = pp.b; c = pp.c; d = pp.d;
    float id = a * d - b * c, x = cwx - pp.worldX, y = cwy - pp.worldY;
    id = std::fabs(id) <= 0.0001f ? 0 : 1 / id;
    float dx = (x * d - y * b) * id - px, dy = (y * a - x * c) * id - py;
    float l1 = std::sqrt(dx * dx + dy * dy), l2 = sk.data->bones[child.index].length * csx, a1, a2;
    if (l1 < 0.0001f) {
        applyIk1(sk, parent, targetX, targetY, false, stretch, false, alpha);
        child.updateWorldTransform(sk, cx, cy, 0, child.ascaleX, child.ascaleY, child.ashearX, child.ashearY);
        return;
    }
    x = targetX - pp.worldX;
    y = targetY - pp.worldY;
    float tx = (x * d - y * b) * id - px, ty = (y * a - x * c) * id - py;
    float dd = tx * tx + ty * ty;
    if (softness != 0) {
        softness *= psx * (csx + 1) * 0.5f;
        float td = std::sqrt(dd), sd = td - l1 - l2 * psy + softness;
        if (sd > 0) {
            float p = std::min(1.0f, sd / (softness * 2)) - 1;
            p = (sd - softness * (1 - p * p)) / td;
            tx -= p * tx;
            ty -= p * ty;
            dd = tx * tx + ty * ty;
        }
    }
    if (u) {
        l2 *= psx;
        float cs = (dd - l1 * l1 - l2 * l2) / (2 * l1 * l2);
        if (cs < -1) { cs = -1; a2 = PI * bendDir; }
        else if (cs > 1) {
            cs = 1;
            a2 = 0;
            if (stretch) {
                a = (std::sqrt(dd) / (l1 + l2) - 1) * alpha + 1;
                sx *= a;
                if (uniform) sy *= a;
            }
        } else a2 = std::acos(cs) * bendDir;
        a = l1 + l2 * cs;
        b = l2 * std::sin(a2);
        a1 = std::atan2(ty * a - tx * b, tx * a + ty * b);
    } else {
        a = psx * l2;
        b = psy * l2;
        float aa = a * a, bb = b * b, ta = std::atan2(ty, tx);
        c = bb * l1 * l1 + aa * dd - aa * bb;
        float c1 = -2 * bb * l1, c2 = bb - aa;
        d = c1 * c1 - 4 * c2 * c;
        bool solved = false;
        if (d >= 0) {
            float q = std::sqrt(d);
            if (c1 < 0) q = -q;
            q = -(c1 + q) * 0.5f;
            float r0 = q / c2, r1 = c / q;
            float r = std::fabs(r0) < std::fabs(r1) ? r0 : r1;
            r0 = dd - r * r;
            if (r0 >= 0) {
                y = std::sqrt(r0) * bendDir;
                a1 = ta - std::atan2(y, r);
                a2 = std::atan2(y / psy, (r - l1) / psx);
                solved = true;
            }
        }
        if (!solved) {
            float minAngle = PI, minX = l1 - a, minDist = minX * minX, minY = 0;
            float maxAngle = 0, maxX = l1 + a, maxDist = maxX * maxX, maxY = 0;
            c = -a * l1 / (aa - bb);
            if (c >= -1 && c <= 1) {
                c = std::acos(c);
                x = a * std::cos(c) + l1;
                y = b * std::sin(c);
                d = x * x + y * y;
                if (d < minDist) { minAngle = c; minDist = d; minX = x; minY = y; }
                if (d > maxDist) { maxAngle = c; maxDist = d; maxX = x; maxY = y; }
            }
            if (dd <= (minDist + maxDist) * 0.5f) {
                a1 = ta - std::atan2(minY * bendDir, minX);
                a2 = minAngle * bendDir;
            } else {
                a1 = ta - std::atan2(maxY * bendDir, maxX);
                a2 = maxAngle * bendDir;
            }
        }
    }
    float os = std::atan2(cy, cx) * s2;
    float rotation = parent.arotation;
    a1 = (a1 - os) * radDeg + os1 - rotation;
    if (a1 > 180) a1 -= 360;
    else if (a1 < -180) a1 += 360;
    parent.updateWorldTransform(sk, px, py, rotation + a1 * alpha, sx, sy, 0, 0);
    rotation = child.arotation;
    a2 = ((a2 + os) * radDeg - child.ashearX) * s2 + os2 - rotation;
    if (a2 > 180) a2 -= 360;
    else if (a2 < -180) a2 += 360;
    child.updateWorldTransform(sk, cx, cy, rotation + a2 * alpha, child.ascaleX, child.ascaleY, child.ashearX, child.ashearY);
}

// ---- Transform constraint -----------------------------------------------------------------

static void transformAbsoluteWorld(Skeleton& sk, const TransformConstraintData& d, const TransformConstraint& c) {
    const Bone& t = sk.bones[d.target];
    float ta = t.a, tb = t.b, tc = t.c, td = t.d;
    float degRadReflect = ta * td - tb * tc > 0 ? degRad : -degRad;
    float offsetRotation = d.offsetRotation * degRadReflect, offsetShearY = d.offsetShearY * degRadReflect;
    for (int bi : d.bones) {
        Bone& b = sk.bones[bi];
        if (c.mixRotate != 0) {
            float a = b.a, bb = b.b, cc = b.c, dd = b.d;
            float r = std::atan2(tc, ta) - std::atan2(cc, a) + offsetRotation;
            r = wrapRad(r) * c.mixRotate;
            float cs = std::cos(r), sn = std::sin(r);
            b.a = cs * a - sn * cc;
            b.b = cs * bb - sn * dd;
            b.c = sn * a + cs * cc;
            b.d = sn * bb + cs * dd;
        }
        if (c.mixX != 0 || c.mixY != 0) {
            Vec2 w = t.localToWorld({d.offsetX, d.offsetY});
            b.worldX += (w.x - b.worldX) * c.mixX;
            b.worldY += (w.y - b.worldY) * c.mixY;
        }
        if (c.mixScaleX != 0) {
            float s = std::sqrt(b.a * b.a + b.c * b.c);
            if (s != 0) s = (s + (std::sqrt(ta * ta + tc * tc) - s + d.offsetScaleX) * c.mixScaleX) / s;
            b.a *= s; b.c *= s;
        }
        if (c.mixScaleY != 0) {
            float s = std::sqrt(b.b * b.b + b.d * b.d);
            if (s != 0) s = (s + (std::sqrt(tb * tb + td * td) - s + d.offsetScaleY) * c.mixScaleY) / s;
            b.b *= s; b.d *= s;
        }
        if (c.mixShearY > 0) {
            float by = std::atan2(b.d, b.b);
            float r = std::atan2(td, tb) - std::atan2(tc, ta) - (by - std::atan2(b.c, b.a));
            r = wrapRad(r);
            r = by + (r + offsetShearY) * c.mixShearY;
            float s = std::sqrt(b.b * b.b + b.d * b.d);
            b.b = std::cos(r) * s;
            b.d = std::sin(r) * s;
        }
        b.updateAppliedTransform(sk);
    }
}

static void transformRelativeWorld(Skeleton& sk, const TransformConstraintData& d, const TransformConstraint& c) {
    const Bone& t = sk.bones[d.target];
    float ta = t.a, tb = t.b, tc = t.c, td = t.d;
    float degRadReflect = ta * td - tb * tc > 0 ? degRad : -degRad;
    float offsetRotation = d.offsetRotation * degRadReflect, offsetShearY = d.offsetShearY * degRadReflect;
    for (int bi : d.bones) {
        Bone& b = sk.bones[bi];
        if (c.mixRotate != 0) {
            float a = b.a, bb = b.b, cc = b.c, dd = b.d;
            float r = wrapRad(std::atan2(tc, ta) + offsetRotation) * c.mixRotate;
            float cs = std::cos(r), sn = std::sin(r);
            b.a = cs * a - sn * cc;
            b.b = cs * bb - sn * dd;
            b.c = sn * a + cs * cc;
            b.d = sn * bb + cs * dd;
        }
        if (c.mixX != 0 || c.mixY != 0) {
            Vec2 w = t.localToWorld({d.offsetX, d.offsetY});
            b.worldX += w.x * c.mixX;
            b.worldY += w.y * c.mixY;
        }
        if (c.mixScaleX != 0) {
            float s = (std::sqrt(ta * ta + tc * tc) - 1 + d.offsetScaleX) * c.mixScaleX + 1;
            b.a *= s; b.c *= s;
        }
        if (c.mixScaleY != 0) {
            float s = (std::sqrt(tb * tb + td * td) - 1 + d.offsetScaleY) * c.mixScaleY + 1;
            b.b *= s; b.d *= s;
        }
        if (c.mixShearY > 0) {
            float r = wrapRad(std::atan2(td, tb) - std::atan2(tc, ta));
            float by = std::atan2(b.d, b.b);
            r = by + (r - PI / 2 + offsetShearY) * c.mixShearY;
            float s = std::sqrt(b.b * b.b + b.d * b.d);
            b.b = std::cos(r) * s;
            b.d = std::sin(r) * s;
        }
        b.updateAppliedTransform(sk);
    }
}

static void transformLocal(Skeleton& sk, const TransformConstraintData& d, const TransformConstraint& c, bool relative) {
    const Bone& t = sk.bones[d.target];
    for (int bi : d.bones) {
        Bone& b = sk.bones[bi];
        float rotation = b.arotation, x = b.ax, y = b.ay, sx = b.ascaleX, sy = b.ascaleY, shy = b.ashearY;
        if (relative) {
            rotation += (t.arotation + d.offsetRotation) * c.mixRotate;
            x += (t.ax + d.offsetX) * c.mixX;
            y += (t.ay + d.offsetY) * c.mixY;
            sx *= ((t.ascaleX - 1 + d.offsetScaleX) * c.mixScaleX) + 1;
            sy *= ((t.ascaleY - 1 + d.offsetScaleY) * c.mixScaleY) + 1;
            shy += (t.ashearY + d.offsetShearY) * c.mixShearY;
        } else {
            if (c.mixRotate != 0) rotation += wrapDeg(t.arotation - rotation + d.offsetRotation) * c.mixRotate;
            x += (t.ax - x + d.offsetX) * c.mixX;
            y += (t.ay - y + d.offsetY) * c.mixY;
            if (c.mixScaleX != 0 && sx != 0) sx = (sx + (t.ascaleX - sx + d.offsetScaleX) * c.mixScaleX) / sx * sx;
            if (c.mixScaleY != 0 && sy != 0) sy = (sy + (t.ascaleY - sy + d.offsetScaleY) * c.mixScaleY) / sy * sy;
            if (c.mixShearY != 0) shy += wrapDeg(t.ashearY - shy + d.offsetShearY) * c.mixShearY;
        }
        b.updateWorldTransform(sk, x, y, rotation, sx, sy, b.ashearX, shy);
    }
}

// One property of a bone, local (applied) or world.
float transformBoneProp(const Bone& b, int prop, bool local) {
    if (local) {
        switch (prop) {
        case 0: return b.arotation; case 1: return b.ax; case 2: return b.ay;
        case 3: return b.ascaleX; case 4: return b.ascaleY; default: return b.ashearY;
        }
    }
    switch (prop) {
    case 0: return wrapDeg(b.worldRotationX()); case 1: return b.worldX; case 2: return b.worldY;
    case 3: return std::sqrt(b.a * b.a + b.c * b.c); case 4: return std::sqrt(b.b * b.b + b.d * b.d);
    default: return wrapDeg(b.worldRotationY() - b.worldRotationX() - 90);
    }
}

// Value a link produces for a source property value: [fromMin, fromMax] -> [toMin, toMax].
float evalTransformLink(const TransformLink& l, float source, bool clamp) {
    float span = l.fromMax - l.fromMin;
    float t = span != 0 ? (source - l.fromMin) / span : 0;
    if (clamp) t = clampf(t, 0, 1);
    return l.toMin + t * (l.toMax - l.toMin);
}

static void transformMapped(Skeleton& sk, const TransformConstraintData& d, const TransformConstraint& c) {
    const Bone& src = sk.bones[d.target];
    const float mix[6] = {c.mixRotate, c.mixX, c.mixY, c.mixScaleX, c.mixScaleY, c.mixShearY};
    float val[6]; bool has[6] = {};
    for (const TransformLink& l : d.links) {
        if (l.to < 0 || l.to > 5 || l.from < 0 || l.from > 5) continue;
        val[l.to] = evalTransformLink(l, transformBoneProp(src, l.from, d.localSource) + d.offsetOf(l.from), d.clamp); // the offset belongs to the source property
        has[l.to] = true;
    }
    for (int bi : d.bones) {
        Bone& b = sk.bones[bi];
        float cur[6];
        for (int p = 0; p < 6; p++) cur[p] = transformBoneProp(b, p, d.localTarget);
        float out[6];
        for (int p = 0; p < 6; p++) {
            out[p] = cur[p];
            if (!has[p] || mix[p] == 0) continue;
            float target = val[p];
            bool angle = p == 0 || p == 5;
            if (d.relative) {
                if (p == 3 || p == 4) out[p] = cur[p] * ((val[p] - 1) * mix[p] + 1);
                else out[p] = cur[p] + target * mix[p];
            } else {
                float delta = target - cur[p];
                out[p] = cur[p] + (angle ? wrapDeg(delta) : delta) * mix[p];
            }
        }
        if (d.localTarget) {
            b.updateWorldTransform(sk, out[1], out[2], out[0], out[3], out[4], b.ashearX, out[5]);
            continue;
        }
        // World target: rebuild the matrix from scale and shear, then rotate it and place the origin.
        float rot = std::atan2(b.c, b.a);
        float sx = out[3], sy = out[4];
        float ang2 = (rot * radDeg + 90 + out[5]) * degRad;
        float dr = out[0] * degRad - rot;
        float a = std::cos(rot) * sx, c2 = std::sin(rot) * sx;
        float bb = std::cos(ang2) * sy, dd = std::sin(ang2) * sy;
        float cs = std::cos(dr), sn = std::sin(dr);
        b.a = cs * a - sn * c2; b.c = sn * a + cs * c2;
        b.b = cs * bb - sn * dd; b.d = sn * bb + cs * dd;
        b.worldX = out[1]; b.worldY = out[2];
        b.updateAppliedTransform(sk);
    }
}

void applyTransform(Skeleton& sk, int index) {
    const TransformConstraintData& d = sk.data->transforms[index];
    const TransformConstraint& c = sk.transforms[index];
    if (c.mixRotate == 0 && c.mixX == 0 && c.mixY == 0 && c.mixScaleX == 0 && c.mixScaleY == 0 && c.mixShearY == 0) return;
    if (d.mapped) { transformMapped(sk, d, c); return; }
    if (d.local) transformLocal(sk, d, c, d.relative);
    else if (d.relative) transformRelativeWorld(sk, d, c);
    else transformAbsoluteWorld(sk, d, c);
}

} // namespace osteon
