// Runtime pose: world transforms, IK and transform constraints, slot state. Engine independent.
using System;
using System.Collections.Generic;

namespace Osteon2D
{
    /// <summary>Float math helpers (degrees API) so the core builds on any .NET profile.</summary>
    public static class M
    {
        public const float PI = 3.14159265358979f, DegRad = PI / 180f, RadDeg = 180f / PI;
        public static float Cos(float r) { return (float)Math.Cos(r); }
        public static float Sin(float r) { return (float)Math.Sin(r); }
        public static float Sqrt(float v) { return (float)Math.Sqrt(v); }
        public static float Atan2(float y, float x) { return (float)Math.Atan2(y, x); }
        public static float Acos(float v) { return (float)Math.Acos(v); }
        public static float Abs(float v) { return v < 0 ? -v : v; }
        public static float CosDeg(float d) { return Cos(d * DegRad); }
        public static float SinDeg(float d) { return Sin(d * DegRad); }
        public static float Atan2Deg(float y, float x) { return Atan2(y, x) * RadDeg; }
        public static float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
        public static float WrapDeg(float d)
        {
            d = d % 360f;
            if (d > 180) d -= 360; else if (d <= -180) d += 360;
            return d;
        }
        public static float WrapRad(float r) { return r > PI ? r - PI * 2 : (r < -PI ? r + PI * 2 : r); }
    }

    public class Bone
    {
        public int index, parent = -1;
        public List<int> children = new List<int>();
        public float x, y, rotation, scaleX = 1, scaleY = 1, shearX, shearY;
        public Inherit inherit;
        public float ax, ay, arotation, ascaleX = 1, ascaleY = 1, ashearX, ashearY;
        public float a = 1, b, c, d = 1, worldX, worldY;
        public bool sorted, active = true;
        public float WorldRotationX { get { return M.Atan2Deg(c, a); } }
        public float WorldRotationY { get { return M.Atan2Deg(d, b); } }
    }

    public class Slot
    {
        public int index, bone;
        public Color4 color = Color4.White, dark = new Color4(0, 0, 0, 1);
        public bool hasDark;
        public string attachment;
        public float[] deform = new float[0];
        public string deformFor;
        public int sequenceIndex = -1;
    }

    public class IkConstraint { public float mix = 1, softness; public int bendDirection = 1; public bool compress, stretch, active = true; }
    public class TransformConstraint { public float mixRotate = 1, mixX = 1, mixY = 1, mixScaleX = 1, mixScaleY = 1, mixShearY = 1; public bool active = true; }

    public class Skeleton
    {
        public readonly SkeletonData data;
        public readonly Bone[] bones;
        public readonly Slot[] slots;
        public int[] drawOrder;
        public readonly IkConstraint[] ik;
        public readonly TransformConstraint[] transforms;
        public int skin;                     // index into data.skins
        public float x, y, scaleX = 1, scaleY = 1;
        public Color4 color = Color4.White;
        float rootA = 1, rootD = 1;
        enum CacheType { Bone, Ik, Transform }
        struct CacheItem { public CacheType type; public int index; }
        readonly List<CacheItem> cache = new List<CacheItem>();

        public Skeleton(SkeletonData data)
        {
            this.data = data;
            bones = new Bone[data.bones.Count];
            for (int i = 0; i < bones.Length; i++) bones[i] = new Bone { index = i, parent = data.bones[i].parent };
            for (int i = 0; i < bones.Length; i++) if (bones[i].parent >= 0) bones[bones[i].parent].children.Add(i);
            slots = new Slot[data.slots.Count];
            for (int i = 0; i < slots.Length; i++) slots[i] = new Slot { index = i, bone = data.slots[i].bone };
            drawOrder = new int[slots.Length];
            ik = new IkConstraint[data.ik.Count]; for (int i = 0; i < ik.Length; i++) ik[i] = new IkConstraint();
            transforms = new TransformConstraint[data.transforms.Count]; for (int i = 0; i < transforms.Length; i++) transforms[i] = new TransformConstraint();
            UpdateCache();
            SetToSetupPose();
        }

        public void SetSkin(string name)
        {
            int i = data.FindSkin(name);
            if (i < 0) throw new ArgumentException("Skin not found: " + name);
            skin = i;
            UpdateCache();
            SetSlotsToSetupPose();
        }

        // ---- constraint / bone ordering (same rules as the editor) ----

        public void UpdateCache()
        {
            cache.Clear();
            Skin activeSkin = skin >= 0 && skin < data.skins.Count ? data.skins[skin] : null;
            Func<int, bool> skinHasBone = b => activeSkin != null && activeSkin.bones.Contains(b);
            Func<string, bool> skinHasConstraint = n => activeSkin != null && activeSkin.constraints.Contains(n);
            foreach (var b in bones)
            {
                b.active = !data.bones[b.index].skinRequired || skinHasBone(b.index);
                if (b.parent >= 0 && !bones[b.parent].active) b.active = false;
                b.sorted = !b.active;
            }
            Action<int> sortBone = null;
            sortBone = i =>
            {
                if (i < 0) return;
                var b = bones[i];
                if (b.sorted) return;
                if (b.parent >= 0) sortBone(b.parent);
                b.sorted = true;
                cache.Add(new CacheItem { type = CacheType.Bone, index = i });
            };
            Action<List<int>> sortReset = null;
            sortReset = list =>
            {
                foreach (int i in list)
                {
                    var b = bones[i];
                    if (!b.active) continue;
                    if (b.sorted) sortReset(b.children);
                    b.sorted = false;
                }
            };
            var order = new List<KeyValuePair<int, CacheItem>>();
            for (int i = 0; i < data.ik.Count; i++) order.Add(new KeyValuePair<int, CacheItem>(data.ik[i].order, new CacheItem { type = CacheType.Ik, index = i }));
            for (int i = 0; i < data.transforms.Count; i++) order.Add(new KeyValuePair<int, CacheItem>(data.transforms[i].order, new CacheItem { type = CacheType.Transform, index = i }));
            // stable sort by order
            var sorted = new List<KeyValuePair<int, CacheItem>>();
            foreach (var item in order)
            {
                int pos = sorted.Count;
                while (pos > 0 && sorted[pos - 1].Key > item.Key) pos--;
                sorted.Insert(pos, item);
            }
            foreach (var kv in sorted)
            {
                var c = kv.Value;
                if (c.type == CacheType.Ik)
                {
                    var cd = data.ik[c.index];
                    bool act = bones[cd.target].active && (!cd.skinRequired || skinHasConstraint(cd.name));
                    foreach (int b in cd.bones) act &= bones[b].active;
                    ik[c.index].active = act;
                    if (!act) continue;
                    sortBone(cd.target);
                    int parent = cd.bones[0];
                    sortBone(parent);
                    if (cd.bones.Length == 1)
                    {
                        cache.Add(c);
                        sortReset(bones[parent].children);
                    }
                    else
                    {
                        int child = cd.bones[cd.bones.Length - 1];
                        sortBone(child);
                        cache.Add(c);
                        sortReset(bones[parent].children);
                        bones[child].sorted = true;
                    }
                }
                else
                {
                    var cd = data.transforms[c.index];
                    bool act = bones[cd.target].active && (!cd.skinRequired || skinHasConstraint(cd.name));
                    foreach (int b in cd.bones) act &= bones[b].active;
                    transforms[c.index].active = act;
                    if (!act) continue;
                    sortBone(cd.target);
                    foreach (int b in cd.bones) { if (cd.local) sortBone(bones[b].parent); sortBone(b); }
                    cache.Add(c);
                    foreach (int b in cd.bones) sortReset(bones[b].children);
                    foreach (int b in cd.bones) bones[b].sorted = true;
                }
            }
            for (int i = 0; i < bones.Length; i++) sortBone(i);
        }

        // ---- setup pose ----

        public void SetToSetupPose() { SetBonesToSetupPose(); SetSlotsToSetupPose(); }

        public void SetBonesToSetupPose()
        {
            foreach (var b in bones)
            {
                var bd = data.bones[b.index];
                b.x = bd.x; b.y = bd.y; b.rotation = bd.rotation;
                b.scaleX = bd.scaleX; b.scaleY = bd.scaleY; b.shearX = bd.shearX; b.shearY = bd.shearY;
                b.inherit = bd.inherit;
            }
            for (int i = 0; i < ik.Length; i++)
            {
                var c = data.ik[i];
                ik[i].mix = c.mix; ik[i].softness = c.softness; ik[i].bendDirection = c.bendDirection; ik[i].compress = c.compress; ik[i].stretch = c.stretch;
            }
            for (int i = 0; i < transforms.Length; i++)
            {
                var c = data.transforms[i]; var t = transforms[i];
                t.mixRotate = c.mixRotate; t.mixX = c.mixX; t.mixY = c.mixY; t.mixScaleX = c.mixScaleX; t.mixScaleY = c.mixScaleY; t.mixShearY = c.mixShearY;
            }
        }

        public void SetSlotsToSetupPose()
        {
            for (int i = 0; i < slots.Length; i++)
            {
                var s = slots[i]; var sd = data.slots[i];
                drawOrder[i] = i;
                s.color = sd.color; s.hasDark = sd.hasDark; s.dark = sd.dark;
                if (s.attachment != sd.attachment) { s.deform = new float[0]; s.deformFor = null; }
                s.attachment = sd.attachment;
                s.sequenceIndex = -1;
            }
        }

        // ---- world transforms ----

        public void UpdateWorldTransform()
        {
            rootA = scaleX; rootD = scaleY;
            foreach (var b in bones)
            {
                b.ax = b.x; b.ay = b.y; b.arotation = b.rotation;
                b.ascaleX = b.scaleX; b.ascaleY = b.scaleY; b.ashearX = b.shearX; b.ashearY = b.shearY;
            }
            foreach (var item in cache)
            {
                switch (item.type)
                {
                    case CacheType.Bone: UpdateBone(bones[item.index], bones[item.index].x, bones[item.index].y, bones[item.index].rotation, bones[item.index].scaleX, bones[item.index].scaleY, bones[item.index].shearX, bones[item.index].shearY); break;
                    case CacheType.Ik: ApplyIk(item.index); break;
                    case CacheType.Transform: ApplyTransform(item.index); break;
                }
            }
        }

        // Parent matrix of a bone (the skeleton scale for the root).
        void ParentMatrix(Bone b, out float a, out float bb, out float c, out float d, out float wx, out float wy)
        {
            if (b.parent < 0) { a = rootA; bb = 0; c = 0; d = rootD; wx = x; wy = y; }
            else { var p = bones[b.parent]; a = p.a; bb = p.b; c = p.c; d = p.d; wx = p.worldX; wy = p.worldY; }
        }

        public void UpdateBone(Bone bone, float x_, float y_, float rot, float sx, float sy, float shx, float shy)
        {
            bone.ax = x_; bone.ay = y_; bone.arotation = rot; bone.ascaleX = sx; bone.ascaleY = sy; bone.ashearX = shx; bone.ashearY = shy;
            float skX = scaleX, skY = scaleY;
            if (bone.parent < 0)
            {
                float rx0 = (rot + shx) * M.DegRad, ry0 = (rot + 90 + shy) * M.DegRad;
                bone.a = M.Cos(rx0) * sx * skX; bone.b = M.Cos(ry0) * sy * skX;
                bone.c = M.Sin(rx0) * sx * skY; bone.d = M.Sin(ry0) * sy * skY;
                bone.worldX = x_ * skX + x; bone.worldY = y_ * skY + y;
                return;
            }
            var p = bones[bone.parent];
            float pa = p.a, pb = p.b, pc = p.c, pd = p.d;
            bone.worldX = pa * x_ + pb * y_ + p.worldX;
            bone.worldY = pc * x_ + pd * y_ + p.worldY;
            switch (bone.inherit)
            {
                case Inherit.Normal:
                    {
                        float rx = (rot + shx) * M.DegRad, ry = (rot + 90 + shy) * M.DegRad;
                        float la = M.Cos(rx) * sx, lb = M.Cos(ry) * sy, lc = M.Sin(rx) * sx, ld = M.Sin(ry) * sy;
                        bone.a = pa * la + pb * lc; bone.b = pa * lb + pb * ld;
                        bone.c = pc * la + pd * lc; bone.d = pc * lb + pd * ld;
                        return;
                    }
                case Inherit.OnlyTranslation:
                    {
                        float rx = (rot + shx) * M.DegRad, ry = (rot + 90 + shy) * M.DegRad;
                        bone.a = M.Cos(rx) * sx; bone.b = M.Cos(ry) * sy; bone.c = M.Sin(rx) * sx; bone.d = M.Sin(ry) * sy;
                        break;
                    }
                case Inherit.NoRotationOrReflection:
                    {
                        float isx = 1 / skX, isy = 1 / skY;
                        pa *= isx; pc *= isy;
                        float s = pa * pa + pc * pc, prx;
                        if (s > 0.0001f)
                        {
                            s = M.Abs(pa * pd * isy - pb * isx * pc) / s;
                            pb = pc * s; pd = pa * s;
                            prx = M.Atan2Deg(pc, pa);
                        }
                        else { pa = 0; pc = 0; prx = 90 - M.Atan2Deg(pd, pb); }
                        float rx = (rot + shx - prx) * M.DegRad, ry = (rot + shy - prx + 90) * M.DegRad;
                        float la = M.Cos(rx) * sx, lb = M.Cos(ry) * sy, lc = M.Sin(rx) * sx, ld = M.Sin(ry) * sy;
                        bone.a = pa * la - pb * lc; bone.b = pa * lb - pb * ld;
                        bone.c = pc * la + pd * lc; bone.d = pc * lb + pd * ld;
                        break;
                    }
                default: // NoScale, NoScaleOrReflection
                    {
                        float r = rot * M.DegRad, cs = M.Cos(r), sn = M.Sin(r);
                        float za = (pa * cs + pb * sn) / skX;
                        float zc = (pc * cs + pd * sn) / skY;
                        float s = M.Sqrt(za * za + zc * zc);
                        if (s > 0.00001f) s = 1 / s;
                        za *= s; zc *= s;
                        s = M.Sqrt(za * za + zc * zc);
                        if (bone.inherit == Inherit.NoScale && (pa * pd - pb * pc < 0) != ((skX < 0) != (skY < 0))) s = -s;
                        r = M.PI / 2 + M.Atan2(zc, za);
                        float zb = M.Cos(r) * s, zd = M.Sin(r) * s;
                        float la = M.CosDeg(shx) * sx, lb = M.CosDeg(90 + shy) * sy, lc = M.SinDeg(shx) * sx, ld = M.SinDeg(90 + shy) * sy;
                        bone.a = za * la + zb * lc; bone.b = za * lb + zb * ld;
                        bone.c = zc * la + zd * lc; bone.d = zc * lb + zd * ld;
                        break;
                    }
            }
            bone.a *= skX; bone.b *= skX; bone.c *= skY; bone.d *= skY;
        }

        static void Decompose(float ra, float rb, float rc, float rd, out float rot, out float sx, out float sy, out float shy)
        {
            sx = M.Sqrt(ra * ra + rc * rc);
            float det = ra * rd - rb * rc;
            float ly = M.Sqrt(rb * rb + rd * rd);
            if (sx > 0.0001f)
            {
                rot = M.Atan2Deg(rc, ra);
                sy = det < 0 ? -ly : ly;
                float yAngle = det < 0 ? M.Atan2Deg(-rd, -rb) : M.Atan2Deg(rd, rb);
                shy = ly > 0.0001f ? M.WrapDeg(yAngle - rot - 90) : 0;
            }
            else { sx = 0; sy = ly; shy = 0; rot = 90 - M.Atan2Deg(rd, rb); }
        }

        /// <summary>Recomputes the applied local pose from the bone's current world transform.</summary>
        public void UpdateAppliedTransform(Bone bone)
        {
            float pa, pb, pc, pd, pwx, pwy;
            ParentMatrix(bone, out pa, out pb, out pc, out pd, out pwx, out pwy);
            float pid = pa * pd - pb * pc;
            pid = M.Abs(pid) < 1e-12f ? 0 : 1 / pid;
            float dx = bone.worldX - pwx, dy = bone.worldY - pwy;
            bone.ax = (dx * pd - dy * pb) * pid;
            bone.ay = (dy * pa - dx * pc) * pid;
            float skX = scaleX, skY = scaleY;
            float ra = 0, rb = 0, rc = 0, rd = 0;
            Action<float, float, float, float> mulInv = (ma, mb, mc, md) =>
            {
                float det = ma * md - mb * mc;
                float id = M.Abs(det) < 1e-12f ? 0 : 1 / det;
                float ia = md * id, ib = -mb * id, ic = -mc * id, idd = ma * id;
                ra = ia * bone.a + ib * bone.c; rb = ia * bone.b + ib * bone.d;
                rc = ic * bone.a + idd * bone.c; rd = ic * bone.b + idd * bone.d;
            };
            bone.ashearX = 0;
            Inherit mode = bone.parent < 0 ? Inherit.Normal : bone.inherit;
            float rot, sx, sy, shy;
            switch (mode)
            {
                case Inherit.Normal:
                    mulInv(pa, pb, pc, pd);
                    Decompose(ra, rb, rc, rd, out rot, out sx, out sy, out shy);
                    bone.arotation = rot; bone.ascaleX = sx; bone.ascaleY = sy; bone.ashearY = shy;
                    return;
                case Inherit.OnlyTranslation:
                    mulInv(skX, 0, 0, skY);
                    Decompose(ra, rb, rc, rd, out rot, out sx, out sy, out shy);
                    bone.arotation = rot; bone.ascaleX = sx; bone.ascaleY = sy; bone.ashearY = shy;
                    return;
                case Inherit.NoRotationOrReflection:
                    {
                        float isx = 1 / skX, isy = 1 / skY;
                        float qa = pa * isx, qc = pc * isy, qb = pb, qd = pd;
                        float s = qa * qa + qc * qc, prx;
                        if (s > 0.0001f)
                        {
                            s = M.Abs(qa * pd * isy - pb * isx * qc) / s;
                            qb = qc * s; qd = qa * s;
                            prx = M.Atan2Deg(qc, qa);
                        }
                        else { qa = 0; qc = 0; prx = 90 - M.Atan2Deg(pd, pb); }
                        mulInv(skX * qa, -skX * qb, skY * qc, skY * qd);
                        Decompose(ra, rb, rc, rd, out rot, out sx, out sy, out shy);
                        bone.arotation = rot + prx; bone.ascaleX = sx; bone.ascaleY = sy; bone.ashearY = shy;
                        return;
                    }
                default:
                    {
                        float wa = bone.a / skX, wc = bone.c / skY;
                        float theta = M.Atan2(wc, wa);
                        float ia = pd, ib = -pb, ic = -pc, id = pa;
                        float dirx = M.Cos(theta) * skX, diry = M.Sin(theta) * skY;
                        float lx = ia * dirx + ib * diry, ly = ic * dirx + id * diry;
                        if (pa * pd - pb * pc < 0) { lx = -lx; ly = -ly; }
                        float r = M.Atan2(ly, lx);
                        float cs = M.Cos(r), sn = M.Sin(r);
                        float za = (pa * cs + pb * sn) / skX, zc = (pc * cs + pd * sn) / skY;
                        float zl = M.Sqrt(za * za + zc * zc);
                        if (zl > 0.00001f) { za /= zl; zc /= zl; }
                        float sgn = 1;
                        if (bone.inherit == Inherit.NoScale && (pa * pd - pb * pc < 0) != ((skX < 0) != (skY < 0))) sgn = -1;
                        float zr = M.PI / 2 + M.Atan2(zc, za);
                        float zb = M.Cos(zr) * sgn, zd = M.Sin(zr) * sgn;
                        mulInv(skX * za, skX * zb, skY * zc, skY * zd);
                        float shx;
                        Decompose(ra, rb, rc, rd, out shx, out sx, out sy, out shy);
                        bone.arotation = r * M.RadDeg; bone.ashearX = shx; bone.ascaleX = sx; bone.ascaleY = sy; bone.ashearY = shy;
                        return;
                    }
            }
        }

        public void LocalToWorld(Bone b, float lx, float ly, out float wx, out float wy) { wx = lx * b.a + ly * b.b + b.worldX; wy = lx * b.c + ly * b.d + b.worldY; }

        // ---- IK ----

        void ApplyIk(int index)
        {
            var d = data.ik[index]; var c = ik[index];
            if (c.mix == 0) return;
            var target = bones[d.target];
            if (d.bones.Length == 1) ApplyIk1(bones[d.bones[0]], target.worldX, target.worldY, c.compress, c.stretch, d.uniform, c.mix);
            else ApplyIk2(bones[d.bones[0]], bones[d.bones[1]], target.worldX, target.worldY, c.bendDirection, c.stretch, d.uniform, c.softness, c.mix);
        }

        void ApplyIk1(Bone bone, float targetX, float targetY, bool compress, bool stretch, bool uniform, float alpha)
        {
            float pa, pb, pc, pd, pwx, pwy;
            ParentMatrix(bone, out pa, out pb, out pc, out pd, out pwx, out pwy);
            float rotationIK = -bone.ashearX - bone.arotation, tx, ty;
            bool noRot = false;
            if (bone.inherit == Inherit.OnlyTranslation)
            {
                tx = (targetX - bone.worldX) * (scaleX < 0 ? -1f : 1f);
                ty = (targetY - bone.worldY) * (scaleY < 0 ? -1f : 1f);
            }
            else
            {
                if (bone.inherit == Inherit.NoRotationOrReflection) noRot = true;
                if (noRot)
                {
                    float s = M.Abs(pa * pd - pb * pc) / Math.Max(0.0001f, pa * pa + pc * pc);
                    float sa = pa / scaleX, sc = pc / scaleY;
                    pb = -sc * s * scaleX; pd = sa * s * scaleY;
                    rotationIK += M.Atan2Deg(sc, sa);
                }
                float xx = targetX - pwx, yy = targetY - pwy;
                float dd = pa * pd - pb * pc;
                if (M.Abs(dd) <= 0.0001f) { tx = 0; ty = 0; }
                else { tx = (xx * pd - yy * pb) / dd - bone.ax; ty = (yy * pa - xx * pc) / dd - bone.ay; }
            }
            rotationIK += M.Atan2Deg(ty, tx);
            if (bone.ascaleX < 0) rotationIK += 180;
            if (rotationIK > 180) rotationIK -= 360; else if (rotationIK < -180) rotationIK += 360;
            float sx = bone.ascaleX, sy = bone.ascaleY;
            if (compress || stretch)
            {
                if (bone.inherit == Inherit.NoScale || bone.inherit == Inherit.NoScaleOrReflection) { tx = targetX - bone.worldX; ty = targetY - bone.worldY; }
                float b = data.bones[bone.index].length * sx;
                if (b > 0.0001f)
                {
                    float dd = tx * tx + ty * ty;
                    if ((compress && dd < b * b) || (stretch && dd > b * b))
                    {
                        float s = (M.Sqrt(dd) / b - 1) * alpha + 1;
                        sx *= s; if (uniform) sy *= s;
                    }
                }
            }
            UpdateBone(bone, bone.ax, bone.ay, bone.arotation + rotationIK * alpha, sx, sy, bone.ashearX, bone.ashearY);
        }

        void ApplyIk2(Bone parent, Bone child, float targetX, float targetY, int bendDir, bool stretch, bool uniform, float softness, float alpha)
        {
            if (parent.inherit != Inherit.Normal || child.inherit != Inherit.Normal) return;
            float px = parent.ax, py = parent.ay, psx = parent.ascaleX, psy = parent.ascaleY, sx = psx, sy = psy, csx = child.ascaleX;
            int os1, os2, s2;
            if (psx < 0) { psx = -psx; os1 = 180; s2 = -1; } else { os1 = 0; s2 = 1; }
            if (psy < 0) { psy = -psy; s2 = -s2; }
            if (csx < 0) { csx = -csx; os2 = 180; } else os2 = 0;
            float cx = child.ax, cy, cwx, cwy, a = parent.a, b = parent.b, c = parent.c, d = parent.d;
            bool u = M.Abs(psx - psy) <= 0.0001f;
            if (!u || stretch) { cy = 0; cwx = a * cx + parent.worldX; cwy = c * cx + parent.worldY; }
            else { cy = child.ay; cwx = a * cx + b * cy + parent.worldX; cwy = c * cx + d * cy + parent.worldY; }
            float ppa, ppb, ppc, ppd, ppwx, ppwy;
            ParentMatrix(parent, out ppa, out ppb, out ppc, out ppd, out ppwx, out ppwy);
            a = ppa; b = ppb; c = ppc; d = ppd;
            float id = a * d - b * c, x = cwx - ppwx, y = cwy - ppwy;
            id = M.Abs(id) <= 0.0001f ? 0 : 1 / id;
            float dx = (x * d - y * b) * id - px, dy = (y * a - x * c) * id - py;
            float l1 = M.Sqrt(dx * dx + dy * dy), l2 = data.bones[child.index].length * csx, a1, a2;
            if (l1 < 0.0001f)
            {
                ApplyIk1(parent, targetX, targetY, false, stretch, false, alpha);
                UpdateBone(child, cx, cy, 0, child.ascaleX, child.ascaleY, child.ashearX, child.ashearY);
                return;
            }
            x = targetX - ppwx; y = targetY - ppwy;
            float tx = (x * d - y * b) * id - px, ty = (y * a - x * c) * id - py;
            float dd = tx * tx + ty * ty;
            if (softness != 0)
            {
                softness *= psx * (csx + 1) * 0.5f;
                float td = M.Sqrt(dd), sd = td - l1 - l2 * psy + softness;
                if (sd > 0)
                {
                    float p = Math.Min(1f, sd / (softness * 2)) - 1;
                    p = (sd - softness * (1 - p * p)) / td;
                    tx -= p * tx; ty -= p * ty;
                    dd = tx * tx + ty * ty;
                }
            }
            if (u)
            {
                l2 *= psx;
                float cs = (dd - l1 * l1 - l2 * l2) / (2 * l1 * l2);
                if (cs < -1) { cs = -1; a2 = M.PI * bendDir; }
                else if (cs > 1)
                {
                    cs = 1; a2 = 0;
                    if (stretch)
                    {
                        a = (M.Sqrt(dd) / (l1 + l2) - 1) * alpha + 1;
                        sx *= a; if (uniform) sy *= a;
                    }
                }
                else a2 = M.Acos(cs) * bendDir;
                a = l1 + l2 * cs;
                b = l2 * M.Sin(a2);
                a1 = M.Atan2(ty * a - tx * b, tx * a + ty * b);
            }
            else
            {
                a = psx * l2; b = psy * l2;
                float aa = a * a, bb = b * b, ta = M.Atan2(ty, tx);
                c = bb * l1 * l1 + aa * dd - aa * bb;
                float c1 = -2 * bb * l1, c2 = bb - aa;
                d = c1 * c1 - 4 * c2 * c;
                bool solved = false;
                a1 = 0; a2 = 0;
                if (d >= 0)
                {
                    float q = M.Sqrt(d);
                    if (c1 < 0) q = -q;
                    q = -(c1 + q) * 0.5f;
                    float r0 = q / c2, r1 = c / q;
                    float r = M.Abs(r0) < M.Abs(r1) ? r0 : r1;
                    r0 = dd - r * r;
                    if (r0 >= 0)
                    {
                        y = M.Sqrt(r0) * bendDir;
                        a1 = ta - M.Atan2(y, r);
                        a2 = M.Atan2(y / psy, (r - l1) / psx);
                        solved = true;
                    }
                }
                if (!solved)
                {
                    float minAngle = M.PI, minX = l1 - a, minDist = minX * minX, minY = 0;
                    float maxAngle = 0, maxX = l1 + a, maxDist = maxX * maxX, maxY = 0;
                    c = -a * l1 / (aa - bb);
                    if (c >= -1 && c <= 1)
                    {
                        c = M.Acos(c);
                        x = a * M.Cos(c) + l1; y = b * M.Sin(c);
                        d = x * x + y * y;
                        if (d < minDist) { minAngle = c; minDist = d; minX = x; minY = y; }
                        if (d > maxDist) { maxAngle = c; maxDist = d; maxX = x; maxY = y; }
                    }
                    if (dd <= (minDist + maxDist) * 0.5f) { a1 = ta - M.Atan2(minY * bendDir, minX); a2 = minAngle * bendDir; }
                    else { a1 = ta - M.Atan2(maxY * bendDir, maxX); a2 = maxAngle * bendDir; }
                }
            }
            float os = M.Atan2(cy, cx) * s2;
            float rotation = parent.arotation;
            a1 = (a1 - os) * M.RadDeg + os1 - rotation;
            if (a1 > 180) a1 -= 360; else if (a1 < -180) a1 += 360;
            UpdateBone(parent, px, py, rotation + a1 * alpha, sx, sy, 0, 0);
            rotation = child.arotation;
            a2 = ((a2 + os) * M.RadDeg - child.ashearX) * s2 + os2 - rotation;
            if (a2 > 180) a2 -= 360; else if (a2 < -180) a2 += 360;
            UpdateBone(child, cx, cy, rotation + a2 * alpha, child.ascaleX, child.ascaleY, child.ashearX, child.ashearY);
        }

        // ---- transform constraints ----

        void ApplyTransform(int index)
        {
            var d = data.transforms[index]; var c = transforms[index];
            if (c.mixRotate == 0 && c.mixX == 0 && c.mixY == 0 && c.mixScaleX == 0 && c.mixScaleY == 0 && c.mixShearY == 0) return;
            if (d.local) TransformLocal(d, c, d.relative);
            else if (d.relative) TransformWorld(d, c, true);
            else TransformWorld(d, c, false);
        }

        void TransformWorld(TransformConstraintData d, TransformConstraint c, bool relative)
        {
            var t = bones[d.target];
            float ta = t.a, tb = t.b, tc = t.c, td = t.d;
            float degRadReflect = ta * td - tb * tc > 0 ? M.DegRad : -M.DegRad;
            float offsetRotation = d.offsetRotation * degRadReflect, offsetShearY = d.offsetShearY * degRadReflect;
            foreach (int bi in d.bones)
            {
                var b = bones[bi];
                if (c.mixRotate != 0)
                {
                    float a = b.a, bb = b.b, cc = b.c, dd = b.d;
                    float r = relative ? M.WrapRad(M.Atan2(tc, ta) + offsetRotation) * c.mixRotate
                                       : M.WrapRad(M.Atan2(tc, ta) - M.Atan2(cc, a) + offsetRotation) * c.mixRotate;
                    float cs = M.Cos(r), sn = M.Sin(r);
                    b.a = cs * a - sn * cc; b.b = cs * bb - sn * dd; b.c = sn * a + cs * cc; b.d = sn * bb + cs * dd;
                }
                if (c.mixX != 0 || c.mixY != 0)
                {
                    float wx, wy; LocalToWorld(t, d.offsetX, d.offsetY, out wx, out wy);
                    if (relative) { b.worldX += wx * c.mixX; b.worldY += wy * c.mixY; }
                    else { b.worldX += (wx - b.worldX) * c.mixX; b.worldY += (wy - b.worldY) * c.mixY; }
                }
                if (c.mixScaleX != 0)
                {
                    float s;
                    if (relative) s = (M.Sqrt(ta * ta + tc * tc) - 1 + d.offsetScaleX) * c.mixScaleX + 1;
                    else
                    {
                        s = M.Sqrt(b.a * b.a + b.c * b.c);
                        if (s != 0) s = (s + (M.Sqrt(ta * ta + tc * tc) - s + d.offsetScaleX) * c.mixScaleX) / s;
                    }
                    b.a *= s; b.c *= s;
                }
                if (c.mixScaleY != 0)
                {
                    float s;
                    if (relative) s = (M.Sqrt(tb * tb + td * td) - 1 + d.offsetScaleY) * c.mixScaleY + 1;
                    else
                    {
                        s = M.Sqrt(b.b * b.b + b.d * b.d);
                        if (s != 0) s = (s + (M.Sqrt(tb * tb + td * td) - s + d.offsetScaleY) * c.mixScaleY) / s;
                    }
                    b.b *= s; b.d *= s;
                }
                if (c.mixShearY > 0)
                {
                    float by = M.Atan2(b.d, b.b), r;
                    if (relative)
                    {
                        r = M.WrapRad(M.Atan2(td, tb) - M.Atan2(tc, ta));
                        r = by + (r - M.PI / 2 + offsetShearY) * c.mixShearY;
                    }
                    else
                    {
                        r = M.Atan2(td, tb) - M.Atan2(tc, ta) - (by - M.Atan2(b.c, b.a));
                        r = M.WrapRad(r);
                        r = by + (r + offsetShearY) * c.mixShearY;
                    }
                    float s = M.Sqrt(b.b * b.b + b.d * b.d);
                    b.b = M.Cos(r) * s; b.d = M.Sin(r) * s;
                }
                UpdateAppliedTransform(b);
            }
        }

        void TransformLocal(TransformConstraintData d, TransformConstraint c, bool relative)
        {
            var t = bones[d.target];
            foreach (int bi in d.bones)
            {
                var b = bones[bi];
                float rotation = b.arotation, x = b.ax, y = b.ay, sx = b.ascaleX, sy = b.ascaleY, shy = b.ashearY;
                if (relative)
                {
                    rotation += (t.arotation + d.offsetRotation) * c.mixRotate;
                    x += (t.ax + d.offsetX) * c.mixX;
                    y += (t.ay + d.offsetY) * c.mixY;
                    sx *= ((t.ascaleX - 1 + d.offsetScaleX) * c.mixScaleX) + 1;
                    sy *= ((t.ascaleY - 1 + d.offsetScaleY) * c.mixScaleY) + 1;
                    shy += (t.ashearY + d.offsetShearY) * c.mixShearY;
                }
                else
                {
                    if (c.mixRotate != 0) rotation += M.WrapDeg(t.arotation - rotation + d.offsetRotation) * c.mixRotate;
                    x += (t.ax - x + d.offsetX) * c.mixX;
                    y += (t.ay - y + d.offsetY) * c.mixY;
                    if (c.mixScaleX != 0 && sx != 0) sx = (sx + (t.ascaleX - sx + d.offsetScaleX) * c.mixScaleX) / sx * sx;
                    if (c.mixScaleY != 0 && sy != 0) sy = (sy + (t.ascaleY - sy + d.offsetScaleY) * c.mixScaleY) / sy * sy;
                    if (c.mixShearY != 0) shy += M.WrapDeg(t.ashearY - shy + d.offsetShearY) * c.mixShearY;
                }
                UpdateBone(b, x, y, rotation, sx, sy, b.ashearX, shy);
            }
        }

        // ---- attachments / geometry ----

        public Attachment AttachmentFor(Slot slot) { return data.FindAttachment(skin, slot.index, slot.attachment); }

        /// <summary>Resolves a linked mesh to the mesh whose geometry it uses (or the attachment itself).</summary>
        public Attachment Geometry(Slot slot, Attachment att)
        {
            if (att.type != AttachmentType.LinkedMesh) return att;
            int ps = string.IsNullOrEmpty(att.parentSkin) ? 0 : data.FindSkin(att.parentSkin);
            return data.FindAttachment(ps, slot.index, att.parentMesh);
        }

        /// <summary>World vertices (x,y pairs) of a region (4 corners BL,TL,TR,BR), mesh or linked mesh. False if not drawable.</summary>
        public bool WorldVertices(Slot slot, Attachment att, float[] outV)
        {
            var bone = bones[slot.bone];
            if (att.type == AttachmentType.Region)
            {
                float hw = att.width / 2 * att.scaleX, hh = att.height / 2 * att.scaleY;
                float c = M.CosDeg(att.rotation), s = M.SinDeg(att.rotation);
                float[] pts = { -hw, -hh, -hw, hh, hw, hh, hw, -hh };
                for (int i = 0; i < 8; i += 2)
                {
                    float lx = pts[i] * c - pts[i + 1] * s + att.x, ly = pts[i] * s + pts[i + 1] * c + att.y;
                    outV[i] = lx * bone.a + ly * bone.b + bone.worldX;
                    outV[i + 1] = lx * bone.c + ly * bone.d + bone.worldY;
                }
                return true;
            }
            if (att.type != AttachmentType.Mesh && att.type != AttachmentType.LinkedMesh) return false;
            var geom = Geometry(slot, att);
            if (geom == null) return false;
            float[] deform = null;
            if (slot.deform.Length > 0 && slot.deformFor == att.name && slot.deform.Length == geom.DeformLength) deform = slot.deform;
            int count = geom.worldVerticesLength;
            if (!geom.IsWeighted)
            {
                float[] v = geom.vertices;
                for (int i = 0; i + 1 < count && i + 1 < v.Length; i += 2)
                {
                    float vx = v[i], vy = v[i + 1];
                    if (deform != null) { vx += deform[i]; vy += deform[i + 1]; }
                    outV[i] = vx * bone.a + vy * bone.b + bone.worldX;
                    outV[i + 1] = vx * bone.c + vy * bone.d + bone.worldY;
                }
                return true;
            }
            int b = 0, vi = 0, fi = 0;
            for (int w = 0; w < count; w += 2)
            {
                float wx = 0, wy = 0;
                int n = geom.bones[b++];
                for (int k = 0; k < n; k++, vi += 3, fi += 2)
                {
                    var bn = bones[geom.bones[b++]];
                    float vx = geom.vertices[vi], vy = geom.vertices[vi + 1], weight = geom.vertices[vi + 2];
                    if (deform != null) { vx += deform[fi]; vy += deform[fi + 1]; }
                    wx += (vx * bn.a + vy * bn.b + bn.worldX) * weight;
                    wy += (vx * bn.c + vy * bn.d + bn.worldY) * weight;
                }
                outV[w] = wx; outV[w + 1] = wy;
            }
            return true;
        }
    }
}
