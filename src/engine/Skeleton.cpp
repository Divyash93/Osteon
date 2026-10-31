#include "Skeleton.h"

namespace osteon {

// ---- Bone ---------------------------------------------------------------------------------

const WorldMatrix& Skeleton::parentMatrix(int bone) const {
    int p = bones[bone].parent;
    return p >= 0 ? (const WorldMatrix&)bones[p] : rootMatrix;
}

void Bone::updateWorldTransform(Skeleton& sk, float x_, float y_, float rot, float sx, float sy, float shx, float shy) {
    ax = x_; ay = y_; arotation = rot; ascaleX = sx; ascaleY = sy; ashearX = shx; ashearY = shy;
    const float skX = sk.scaleX, skY = sk.scaleY;
    if (parent < 0) {
        float rx = (rot + shx) * degRad, ry = (rot + 90 + shy) * degRad;
        a = std::cos(rx) * sx * skX;
        b = std::cos(ry) * sy * skX;
        c = std::sin(rx) * sx * skY;
        d = std::sin(ry) * sy * skY;
        worldX = x_ * skX + sk.x;
        worldY = y_ * skY + sk.y;
        return;
    }
    const Bone& p = sk.bones[parent];
    float pa = p.a, pb = p.b, pc = p.c, pd = p.d;
    worldX = pa * x_ + pb * y_ + p.worldX;
    worldY = pc * x_ + pd * y_ + p.worldY;

    switch (inherit) {
    case Inherit::Normal: {
        float rx = (rot + shx) * degRad, ry = (rot + 90 + shy) * degRad;
        float la = std::cos(rx) * sx, lb = std::cos(ry) * sy, lc = std::sin(rx) * sx, ld = std::sin(ry) * sy;
        a = pa * la + pb * lc;
        b = pa * lb + pb * ld;
        c = pc * la + pd * lc;
        d = pc * lb + pd * ld;
        return;
    }
    case Inherit::OnlyTranslation: {
        float rx = (rot + shx) * degRad, ry = (rot + 90 + shy) * degRad;
        a = std::cos(rx) * sx;
        b = std::cos(ry) * sy;
        c = std::sin(rx) * sx;
        d = std::sin(ry) * sy;
        break;
    }
    case Inherit::NoRotationOrReflection: {
        float isx = 1 / skX, isy = 1 / skY;
        pa *= isx; pc *= isy;
        float s = pa * pa + pc * pc, prx;
        if (s > 0.0001f) {
            s = std::fabs(pa * pd * isy - pb * isx * pc) / s;
            pb = pc * s;
            pd = pa * s;
            prx = atan2Deg(pc, pa);
        } else {
            pa = 0; pc = 0;
            prx = 90 - atan2Deg(pd, pb);
        }
        float rx = (rot + shx - prx) * degRad, ry = (rot + shy - prx + 90) * degRad;
        float la = std::cos(rx) * sx, lb = std::cos(ry) * sy, lc = std::sin(rx) * sx, ld = std::sin(ry) * sy;
        a = pa * la - pb * lc;
        b = pa * lb - pb * ld;
        c = pc * la + pd * lc;
        d = pc * lb + pd * ld;
        break;
    }
    case Inherit::NoScale:
    case Inherit::NoScaleOrReflection: {
        float r = rot * degRad, cs = std::cos(r), sn = std::sin(r);
        float za = (pa * cs + pb * sn) / skX;
        float zc = (pc * cs + pd * sn) / skY;
        float s = std::sqrt(za * za + zc * zc);
        if (s > 0.00001f) s = 1 / s;
        za *= s; zc *= s;
        s = std::sqrt(za * za + zc * zc);
        if (inherit == Inherit::NoScale && (pa * pd - pb * pc < 0) != ((skX < 0) != (skY < 0))) s = -s;
        r = PI / 2 + std::atan2(zc, za);
        float zb = std::cos(r) * s, zd = std::sin(r) * s;
        float la = cosDeg(shx) * sx, lb = cosDeg(90 + shy) * sy, lc = sinDeg(shx) * sx, ld = sinDeg(90 + shy) * sy;
        a = za * la + zb * lc;
        b = za * lb + zb * ld;
        c = zc * la + zd * lc;
        d = zc * lb + zd * ld;
        break;
    }
    }
    a *= skX; b *= skX; c *= skY; d *= skY;
}

// Decomposes a 2x2 matrix into rotation, scales and shearY (shearX = 0), inverse of the
// composition used in updateWorldTransform.
static void decompose(float ra, float rb, float rc, float rd, float& rot, float& sx, float& sy, float& shy) {
    sx = std::sqrt(ra * ra + rc * rc);
    float det = ra * rd - rb * rc;
    float ly = std::sqrt(rb * rb + rd * rd);
    if (sx > 0.0001f) {
        rot = atan2Deg(rc, ra);
        sy = det < 0 ? -ly : ly;
        float yAngle = det < 0 ? atan2Deg(-rd, -rb) : atan2Deg(rd, rb);
        shy = ly > 0.0001f ? wrapDeg(yAngle - rot - 90) : 0;
    } else {
        sx = 0;
        sy = ly;
        shy = 0;
        rot = 90 - atan2Deg(rd, rb);
    }
}

void Bone::updateAppliedTransform(Skeleton& sk) {
    const WorldMatrix& p = sk.parentMatrix(index);
    float pa = p.a, pb = p.b, pc = p.c, pd = p.d;
    float pid = pa * pd - pb * pc;
    pid = std::fabs(pid) < 1e-12f ? 0 : 1 / pid;
    float dx = worldX - p.worldX, dy = worldY - p.worldY;
    ax = (dx * pd - dy * pb) * pid;
    ay = (dy * pa - dx * pc) * pid;
    if (parent < 0) { // root's parent matrix is the skeleton scale, already handled above
        pa = sk.scaleX; pb = 0; pc = 0; pd = sk.scaleY;
    }
    float skX = sk.scaleX, skY = sk.scaleY;
    float ra, rb, rc, rd;
    auto mulInv = [&](float ma, float mb, float mc, float md) {
        float det = ma * md - mb * mc;
        float id = std::fabs(det) < 1e-12f ? 0 : 1 / det;
        float ia = md * id, ib = -mb * id, ic = -mc * id, idd = ma * id;
        ra = ia * a + ib * c; rb = ia * b + ib * d;
        rc = ic * a + idd * c; rd = ic * b + idd * d;
    };
    ashearX = 0;
    switch (parent < 0 ? Inherit::Normal : inherit) {
    case Inherit::Normal:
        mulInv(pa, pb, pc, pd);
        decompose(ra, rb, rc, rd, arotation, ascaleX, ascaleY, ashearY);
        return;
    case Inherit::OnlyTranslation:
        mulInv(skX, 0, 0, skY);
        decompose(ra, rb, rc, rd, arotation, ascaleX, ascaleY, ashearY);
        return;
    case Inherit::NoRotationOrReflection: {
        float isx = 1 / skX, isy = 1 / skY;
        float qa = pa * isx, qc = pc * isy, qb = pb, qd = pd;
        float s = qa * qa + qc * qc, prx;
        if (s > 0.0001f) {
            s = std::fabs(qa * pd * isy - pb * isx * qc) / s;
            qb = qc * s; qd = qa * s;
            prx = atan2Deg(qc, qa);
        } else {
            qa = 0; qc = 0;
            prx = 90 - atan2Deg(pd, pb);
        }
        // world = S * [[qa,-qb],[qc,qd]] * L(rot - prx)
        mulInv(skX * qa, -skX * qb, skY * qc, skY * qd);
        decompose(ra, rb, rc, rd, arotation, ascaleX, ascaleY, ashearY);
        arotation += prx;
        return;
    }
    default: { // NoScale, NoScaleOrReflection
        // Z (rotation-only frame) points along the world x-axis direction.
        float wa = a / skX, wc = c / skY;
        float theta = std::atan2(wc, wa);
        // Find local rotation r whose parent-transformed direction has angle theta.
        float ia = pd, ib = -pb, ic = -pc, id = pa; // inverse up to scale
        float dirx = std::cos(theta) * skX, diry = std::sin(theta) * skY;
        float lx = ia * dirx + ib * diry, ly = ic * dirx + id * diry;
        if (pa * pd - pb * pc < 0) { lx = -lx; ly = -ly; }
        float r = std::atan2(ly, lx);
        // Rebuild Z exactly as updateWorldTransform does, then L = Z^-1 * S^-1 * W.
        float cs = std::cos(r), sn = std::sin(r);
        float za = (pa * cs + pb * sn) / skX, zc = (pc * cs + pd * sn) / skY;
        float zl = std::sqrt(za * za + zc * zc);
        if (zl > 0.00001f) { za /= zl; zc /= zl; }
        float s = 1;
        if (inherit == Inherit::NoScale && (pa * pd - pb * pc < 0) != ((skX < 0) != (skY < 0))) s = -1;
        float zr = PI / 2 + std::atan2(zc, za);
        float zb = std::cos(zr) * s, zd = std::sin(zr) * s;
        mulInv(skX * za, skX * zb, skY * zc, skY * zd);
        float shx;
        decompose(ra, rb, rc, rd, shx, ascaleX, ascaleY, ashearY);
        arotation = r * radDeg;
        ashearX = shx;
        return;
    }
    }
}

Vec2 Bone::worldToLocal(Vec2 w) const {
    float det = a * d - b * c;
    float id = std::fabs(det) < 1e-12f ? 0 : 1 / det;
    float x = w.x - worldX, y = w.y - worldY;
    return {(x * d - y * b) * id, (y * a - x * c) * id};
}

Vec2 Bone::localToWorld(Vec2 l) const { return {l.x * a + l.y * b + worldX, l.x * c + l.y * d + worldY}; }

Vec2 Bone::worldToParent(const Skeleton& sk, Vec2 w) const {
    const WorldMatrix& p = sk.parentMatrix(index);
    float det = p.a * p.d - p.b * p.c;
    float id = std::fabs(det) < 1e-12f ? 0 : 1 / det;
    float x = w.x - p.worldX, y = w.y - p.worldY;
    return {(x * p.d - y * p.b) * id, (y * p.a - x * p.c) * id};
}

Vec2 Bone::parentToWorld(const Skeleton& sk, Vec2 l) const {
    const WorldMatrix& p = sk.parentMatrix(index);
    return {l.x * p.a + l.y * p.b + p.worldX, l.x * p.c + l.y * p.d + p.worldY};
}

float Bone::worldToLocalRotation(float worldRotation) const {
    float s = sinDeg(worldRotation), cs = cosDeg(worldRotation);
    return atan2Deg(a * s - c * cs, d * cs - b * s) + rotation - shearX;
}

float Bone::localToWorldRotation(float localRotation) const {
    localRotation -= rotation - shearX;
    float s = sinDeg(localRotation), cs = cosDeg(localRotation);
    return atan2Deg(cs * c + s * d, cs * a + s * b);
}

// ---- Skeleton -----------------------------------------------------------------------------

bool Skeleton::needsRebuild(const SkeletonData& d) const {
    if (data != &d || bones.size() != d.bones.size() || slots.size() != d.slots.size() || ik.size() != d.ik.size() ||
        transforms.size() != d.transforms.size() || paths.size() != d.paths.size() || physics.size() != d.physics.size() ||
        sliders.size() != d.sliders.size())
        return true;
    for (size_t i = 0; i < bones.size(); i++)
        if (bones[i].parent != d.bones[i].parent) return true;
    for (size_t i = 0; i < slots.size(); i++)
        if (slots[i].bone != d.slots[i].bone) return true;
    return false;
}

void Skeleton::build(const SkeletonData& d) {
    data = &d;
    bones.assign(d.bones.size(), Bone());
    for (size_t i = 0; i < bones.size(); i++) {
        bones[i].index = (int)i;
        bones[i].parent = d.bones[i].parent;
    }
    for (size_t i = 0; i < bones.size(); i++)
        if (bones[i].parent >= 0) bones[bones[i].parent].children.push_back((int)i);
    slots.assign(d.slots.size(), Slot());
    for (size_t i = 0; i < slots.size(); i++) {
        slots[i].index = (int)i;
        slots[i].bone = d.slots[i].bone;
    }
    ik.assign(d.ik.size(), IkConstraint());
    transforms.assign(d.transforms.size(), TransformConstraint());
    paths.assign(d.paths.size(), PathConstraint());
    physics.assign(d.physics.size(), PhysicsConstraint());
    sliders.assign(d.sliders.size(), SliderConstraint());
    if (skin >= (int)d.skins.size()) skin = 0;
    updateCacheOrder();
    setToSetupPose();
}

void Skeleton::updateCacheOrder() {
    updateCache.clear();
    const SkeletonData& d = *data;
    const Skin* activeSkin = skin >= 0 && skin < (int)d.skins.size() ? &d.skins[skin] : nullptr;
    auto skinHasBone = [&](int b) {
        if (!activeSkin) return false;
        return std::find(activeSkin->bones.begin(), activeSkin->bones.end(), b) != activeSkin->bones.end();
    };
    auto skinHasConstraint = [&](const std::string& n) {
        return activeSkin && std::find(activeSkin->constraints.begin(), activeSkin->constraints.end(), n) != activeSkin->constraints.end();
    };
    for (auto& b : bones) {
        b.active = !d.bones[b.index].skinRequired || skinHasBone(b.index);
        if (b.parent >= 0 && !bones[b.parent].active) b.active = false;
        b.sorted = !b.active;
    }

    std::function<void(int)> sortBone = [&](int i) {
        if (i < 0) return;
        Bone& b = bones[i];
        if (b.sorted) return;
        if (b.parent >= 0) sortBone(b.parent);
        b.sorted = true;
        updateCache.push_back({CacheType::Bone, i});
    };
    std::function<void(const std::vector<int>&)> sortReset = [&](const std::vector<int>& list) {
        for (int i : list) {
            Bone& b = bones[i];
            if (!b.active) continue;
            if (b.sorted) sortReset(b.children);
            b.sorted = false;
        }
    };

    struct C { int order; CacheType type; int index; };
    std::vector<C> cs;
    for (size_t i = 0; i < d.ik.size(); i++) cs.push_back({d.ik[i].order, CacheType::Ik, (int)i});
    for (size_t i = 0; i < d.transforms.size(); i++) cs.push_back({d.transforms[i].order, CacheType::Transform, (int)i});
    for (size_t i = 0; i < d.paths.size(); i++) cs.push_back({d.paths[i].order, CacheType::Path, (int)i});
    for (size_t i = 0; i < d.physics.size(); i++) cs.push_back({d.physics[i].order, CacheType::Physics, (int)i});
    for (size_t i = 0; i < d.sliders.size(); i++) cs.push_back({d.sliders[i].order, CacheType::Slider, (int)i});
    std::stable_sort(cs.begin(), cs.end(), [](const C& x, const C& y) { return x.order < y.order; });

    for (auto& c : cs) {
        switch (c.type) {
        case CacheType::Ik: {
            auto& cd = d.ik[c.index];
            bool act = bones[cd.target].active && (!cd.skinRequired || skinHasConstraint(cd.name));
            for (int b : cd.bones) act &= bones[b].active;
            ik[c.index].active = act;
            if (!act) break;
            sortBone(cd.target);
            int parent = cd.bones[0];
            sortBone(parent);
            if (cd.bones.size() == 1) {
                updateCache.push_back({CacheType::Ik, c.index});
                sortReset(bones[parent].children);
            } else {
                int child = cd.bones.back();
                sortBone(child);
                updateCache.push_back({CacheType::Ik, c.index});
                sortReset(bones[parent].children);
                bones[child].sorted = true;
            }
            break;
        }
        case CacheType::Transform: {
            auto& cd = d.transforms[c.index];
            bool act = bones[cd.target].active && (!cd.skinRequired || skinHasConstraint(cd.name));
            for (int b : cd.bones) act &= bones[b].active;
            transforms[c.index].active = act;
            if (!act) break;
            sortBone(cd.target);
            for (int b : cd.bones) {
                if (cd.mapped ? cd.localTarget : cd.local) sortBone(bones[b].parent);
                sortBone(b);
            }
            updateCache.push_back({CacheType::Transform, c.index});
            for (int b : cd.bones) sortReset(bones[b].children);
            for (int b : cd.bones) bones[b].sorted = true;
            break;
        }
        case CacheType::Path: {
            auto& cd = d.paths[c.index];
            bool act = bones[d.slots[cd.target].bone].active && (!cd.skinRequired || skinHasConstraint(cd.name));
            for (int b : cd.bones) act &= bones[b].active;
            paths[c.index].active = act;
            if (!act) break;
            sortBone(d.slots[cd.target].bone);
            // Weighted path vertices depend on their bones.
            for (auto& sk : d.skins)
                if (auto it = sk.attachments.find(cd.target); it != sk.attachments.end())
                    for (auto& [n, a] : it->second)
                        if (a.type == AttachmentType::Path && a.isWeighted())
                            for (size_t i = 0; i < a.bones.size();) {
                                int cnt = a.bones[i++];
                                for (int k = 0; k < cnt; k++) sortBone(a.bones[i++]);
                            }
            for (int b : cd.bones) sortBone(b);
            updateCache.push_back({CacheType::Path, c.index});
            for (int b : cd.bones) sortReset(bones[b].children);
            for (int b : cd.bones) bones[b].sorted = true;
            break;
        }
        case CacheType::Physics: {
            auto& cd = d.physics[c.index];
            physics[c.index].active = bones[cd.bone].active && (!cd.skinRequired || skinHasConstraint(cd.name));
            if (!physics[c.index].active) break;
            sortBone(cd.bone);
            updateCache.push_back({CacheType::Physics, c.index});
            sortReset(bones[cd.bone].children);
            bones[cd.bone].sorted = true;
            break;
        }
        case CacheType::Slider: {
            auto& cd = d.sliders[c.index];
            int ai = d.findAnimation(cd.animation);
            bool act = ai >= 0 && (cd.bone < 0 || bones[cd.bone].active) && (!cd.skinRequired || skinHasConstraint(cd.name));
            std::vector<int> driven;
            if (act) for (int b : animatedBones(d.animations[ai], (int)bones.size())) if (bones[b].active) driven.push_back(b);
            sliders[c.index].active = act;
            if (!act) break;
            if (cd.bone >= 0) sortBone(cd.bone);
            // The driven bones are posed by the constraint, so they (and what hangs off them) update after it.
            for (int b : driven) sortBone(bones[b].parent);
            updateCache.push_back({CacheType::Slider, c.index});
            for (int b : driven) sortReset(bones[b].children);
            for (int b : driven) bones[b].sorted = true;
            break;
        }
        default: break;
        }
    }
    for (size_t i = 0; i < bones.size(); i++) sortBone((int)i);
}

void Skeleton::setBonesToSetupPose() {
    const SkeletonData& d = *data;
    for (auto& b : bones) {
        const BoneData& bd = d.bones[b.index];
        b.x = bd.x; b.y = bd.y; b.rotation = bd.rotation;
        b.scaleX = bd.scaleX; b.scaleY = bd.scaleY; b.shearX = bd.shearX; b.shearY = bd.shearY;
        b.inherit = bd.inherit;
    }
    for (size_t i = 0; i < ik.size(); i++) {
        auto& c = d.ik[i];
        ik[i].mix = c.mix; ik[i].softness = c.softness; ik[i].bendDirection = c.bendDirection;
        ik[i].compress = c.compress; ik[i].stretch = c.stretch;
    }
    for (size_t i = 0; i < transforms.size(); i++) {
        auto& c = d.transforms[i];
        auto& t = transforms[i];
        t.mixRotate = c.mixRotate; t.mixX = c.mixX; t.mixY = c.mixY;
        t.mixScaleX = c.mixScaleX; t.mixScaleY = c.mixScaleY; t.mixShearY = c.mixShearY;
    }
    for (size_t i = 0; i < paths.size(); i++) {
        auto& c = d.paths[i];
        auto& p = paths[i];
        p.position = c.position; p.spacing = c.spacing;
        p.mixRotate = c.mixRotate; p.mixX = c.mixX; p.mixY = c.mixY;
    }
    for (size_t i = 0; i < sliders.size(); i++) { sliders[i].time = d.sliders[i].time; sliders[i].mix = d.sliders[i].mix; }
    for (size_t i = 0; i < physics.size(); i++) {
        auto& c = d.physics[i];
        auto& p = physics[i];
        p.inertia = c.inertia; p.strength = c.strength; p.damping = c.damping;
        p.massInverse = c.massInverse; p.wind = c.wind; p.gravity = c.gravity; p.mix = c.mix;
    }
}

void Skeleton::setSlotsToSetupPose() {
    const SkeletonData& d = *data;
    drawOrder.resize(slots.size());
    for (auto& s : slots) {
        const SlotData& sd = d.slots[s.index];
        drawOrder[s.index] = s.index;
        s.color = sd.color;
        s.hasDark = sd.hasDark;
        s.dark = sd.dark;
        if (s.attachment != sd.attachment) { s.deform.clear(); s.deformFor.clear(); }
        s.attachment = sd.attachment;
        s.sequenceIndex = -1;
    }
}

void Skeleton::updateWorldTransform(Physics phys) {
    rootMatrix = WorldMatrix{scaleX, 0, 0, scaleY, x, y};
    for (auto& b : bones) {
        b.ax = b.x; b.ay = b.y; b.arotation = b.rotation;
        b.ascaleX = b.scaleX; b.ascaleY = b.scaleY; b.ashearX = b.shearX; b.ashearY = b.shearY;
    }
    for (auto& item : updateCache) {
        switch (item.type) {
        case CacheType::Bone: bones[item.index].update(*this); break;
        case CacheType::Ik: applyIk(*this, item.index); break;
        case CacheType::Transform: applyTransform(*this, item.index); break;
        case CacheType::Path: applyPath(*this, item.index); break;
        case CacheType::Physics: applyPhysics(*this, item.index, phys); break;
        case CacheType::Slider: applySlider(*this, item.index); break;
        }
    }
}

const Attachment* Skeleton::attachmentFor(const Slot& slot) const {
    return data->findAttachment(skin, slot.index, slot.attachment);
}

bool Skeleton::worldVertices(const Slot& slot, const Attachment& att, std::vector<float>& out) const {
    const Bone& bone = bones[slot.bone];
    switch (att.type) {
    case AttachmentType::Region: {
        float local[8];
        att.regionCorners(local);
        out.resize(8);
        for (int i = 0; i < 8; i += 2) {
            Vec2 w = bone.localToWorld({local[i], local[i + 1]});
            out[i] = w.x; out[i + 1] = w.y;
        }
        return true;
    }
    case AttachmentType::Point: {
        Vec2 w = bone.localToWorld({att.x, att.y});
        out = {w.x, w.y};
        return true;
    }
    default: break;
    }
    // Vertex attachments; linked meshes use their parent's geometry.
    const Attachment* geom = &att;
    if (att.type == AttachmentType::LinkedMesh) {
        int ps = att.parentSkin.empty() ? 0 : data->findSkin(att.parentSkin);
        geom = data->findAttachment(ps, slot.index, att.parentMesh);
        if (!geom) return false;
    }
    const std::vector<float>* deform = nullptr;
    if (!slot.deform.empty() && slot.deformFor == att.name && (int)slot.deform.size() == geom->deformLength())
        deform = &slot.deform;
    int count = geom->worldVerticesLength;
    out.resize(count);
    if (!geom->isWeighted()) {
        const std::vector<float>& v = geom->vertices;
        for (int i = 0; i + 1 < count && i + 1 < (int)v.size(); i += 2) {
            float vx = v[i], vy = v[i + 1];
            if (deform) { vx += (*deform)[i]; vy += (*deform)[i + 1]; }
            Vec2 w = bone.localToWorld({vx, vy});
            out[i] = w.x; out[i + 1] = w.y;
        }
        return true;
    }
    const auto& bi = geom->bones;
    const auto& v = geom->vertices;
    size_t b = 0, vi = 0, fi = 0;
    for (int w = 0; w < count; w += 2) {
        float wx = 0, wy = 0;
        int n = bi[b++];
        for (int k = 0; k < n; k++, vi += 3, fi += 2) {
            const Bone& bn = bones[bi[b++]];
            float vx = v[vi], vy = v[vi + 1], weight = v[vi + 2];
            if (deform) { vx += (*deform)[fi]; vy += (*deform)[fi + 1]; }
            wx += (vx * bn.a + vy * bn.b + bn.worldX) * weight;
            wy += (vx * bn.c + vy * bn.d + bn.worldY) * weight;
        }
        out[w] = wx; out[w + 1] = wy;
    }
    return true;
}

bool Skeleton::bounds(float& minX, float& minY, float& maxX, float& maxY) const {
    minX = minY = 1e30f; maxX = maxY = -1e30f;
    std::vector<float> v;
    bool any = false;
    for (int si : drawOrder) {
        const Slot& s = slots[si];
        if (!bones[s.bone].active) continue;
        const Attachment* a = attachmentFor(s);
        if (!a || !(a->type == AttachmentType::Region || a->type == AttachmentType::Mesh || a->type == AttachmentType::LinkedMesh)) continue;
        if (!worldVertices(s, *a, v)) continue;
        for (size_t i = 0; i + 1 < v.size(); i += 2) {
            minX = std::min(minX, v[i]); maxX = std::max(maxX, v[i]);
            minY = std::min(minY, v[i + 1]); maxY = std::max(maxY, v[i + 1]);
            any = true;
        }
    }
    return any;
}

} // namespace osteon
