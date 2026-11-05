// Turns a posed Skeleton into batched triangles (positions, uvs, colours, indices, sub meshes), including clipping.
// Engine independent: the Unity and any other renderer only copy the arrays.
using System;
using System.Collections.Generic;

namespace Osteon2D
{
    public struct SubMesh { public int page; public BlendMode blend; public int startIndex, indexCount; }

    public sealed class MeshBuilder
    {
        public readonly Atlas atlas;
        public bool premultiplyAlpha;             // multiply rgb by alpha (for premultiplied-alpha shaders)
        public bool tintBlack;                    // fill the dark colour array
        public bool useClipping = true;

        public float[] positions = new float[2 * 256];   // x,y per vertex
        public float[] uvs = new float[2 * 256];         // u,v per vertex, v pointing UP (texture space of Unity / OpenGL)
        public Color4[] colors = new Color4[256];
        public Color4[] darks = new Color4[256];
        public int[] indices = new int[3 * 256];
        public readonly List<SubMesh> subMeshes = new List<SubMesh>();
        public int vertexCount, indexCount;

        float[] world = new float[2 * 1024];
        readonly List<float> clipPoly = new List<float>();
        List<float[]> clipConvex = new List<float[]>();
        string clipEndSlot;
        bool clipping;

        public MeshBuilder(Atlas atlas) { this.atlas = atlas; }

        void EnsureVertices(int add)
        {
            int need = vertexCount + add;
            if (need <= colors.Length) return;
            int n = Math.Max(need, colors.Length * 2);
            Array.Resize(ref positions, n * 2); Array.Resize(ref uvs, n * 2); Array.Resize(ref colors, n); Array.Resize(ref darks, n);
        }
        void EnsureIndices(int add)
        {
            int need = indexCount + add;
            if (need > indices.Length) Array.Resize(ref indices, Math.Max(need, indices.Length * 2));
        }

        public void Clear() { vertexCount = 0; indexCount = 0; subMeshes.Clear(); }

        static string FrameName(Attachment a, int frame)
        {
            string path = string.IsNullOrEmpty(a.path) ? a.name : a.path;
            if (a.seqCount <= 0) return path;
            string digits = (a.seqStart + frame).ToString();
            while (digits.Length < a.seqDigits) digits = "0" + digits;
            return path + digits;
        }

        /// <summary>Rebuilds all geometry from the current pose.</summary>
        public void Build(Skeleton sk)
        {
            Clear();
            clipping = false;
            var data = sk.data;
            foreach (int si in sk.drawOrder)
            {
                var slot = sk.slots[si];
                var att = sk.AttachmentFor(slot);
                if (!sk.bones[slot.bone].active) { if (clipping && slot.index == EndIndex(sk)) clipping = false; continue; }
                if (att != null && att.type == AttachmentType.Clipping && useClipping)
                {
                    StartClip(sk, slot, att);
                    continue;
                }
                if (att != null && (att.type == AttachmentType.Region || att.type == AttachmentType.Mesh || att.type == AttachmentType.LinkedMesh))
                    AddAttachment(sk, slot, att);
                if (clipping && slot.index == EndIndex(sk)) clipping = false;
            }
        }

        int EndIndex(Skeleton sk) { return clipEndSlot == null ? -1 : sk.data.FindSlot(clipEndSlot); }

        void StartClip(Skeleton sk, Slot slot, Attachment att)
        {
            int n = att.worldVerticesLength;
            if (world.Length < n) world = new float[n];
            if (!WorldPolygon(sk, slot, att, world)) return;
            clipPoly.Clear();
            for (int i = 0; i < n; i++) clipPoly.Add(world[i]);
            clipConvex = Convexes(clipPoly);
            clipEndSlot = att.endSlot;
            clipping = clipConvex.Count > 0;
            if (string.IsNullOrEmpty(clipEndSlot)) clipEndSlot = null; // clips to the end of the draw order
        }

        // Clipping polygons are vertex attachments on the slot's bone (unweighted or weighted).
        static bool WorldPolygon(Skeleton sk, Slot slot, Attachment att, float[] outV)
        {
            var bone = sk.bones[slot.bone];
            if (!att.IsWeighted)
            {
                for (int i = 0; i + 1 < att.worldVerticesLength; i += 2)
                {
                    outV[i] = att.vertices[i] * bone.a + att.vertices[i + 1] * bone.b + bone.worldX;
                    outV[i + 1] = att.vertices[i] * bone.c + att.vertices[i + 1] * bone.d + bone.worldY;
                }
                return true;
            }
            int b = 0, vi = 0;
            for (int w = 0; w < att.worldVerticesLength; w += 2)
            {
                float wx = 0, wy = 0;
                int n = att.bones[b++];
                for (int k = 0; k < n; k++, vi += 3)
                {
                    var bn = sk.bones[att.bones[b++]];
                    float vx = att.vertices[vi], vy = att.vertices[vi + 1], weight = att.vertices[vi + 2];
                    wx += (vx * bn.a + vy * bn.b + bn.worldX) * weight;
                    wy += (vx * bn.c + vy * bn.d + bn.worldY) * weight;
                }
                outV[w] = wx; outV[w + 1] = wy;
            }
            return true;
        }

        void AddAttachment(Skeleton sk, Slot slot, Attachment att)
        {
            var sd = sk.data.slots[slot.index];
            int frame = slot.sequenceIndex >= 0 ? slot.sequenceIndex : att.seqSetup;
            var region = atlas.Find(FrameName(att, frame));
            if (region == null) return;
            var page = atlas.pages[region.page];

            int vcount; float[] srcUv; int[] tris;
            if (att.type == AttachmentType.Region)
            {
                vcount = 4; tris = RegionTris;
                srcUv = RegionUvs;
            }
            else
            {
                var geom = sk.Geometry(slot, att);
                if (geom == null || geom.uvs == null) return;
                vcount = geom.worldVerticesLength / 2; srcUv = geom.uvs; tris = geom.triangles;
            }
            if (world.Length < vcount * 2) world = new float[vcount * 2];
            if (!sk.WorldVertices(slot, att, world)) return;

            // colour: skeleton * slot * attachment
            var c = new Color4(sk.color.r * slot.color.r * att.color.r, sk.color.g * slot.color.g * att.color.g,
                               sk.color.b * slot.color.b * att.color.b, sk.color.a * slot.color.a * att.color.a);
            var dk = new Color4(slot.dark.r, slot.dark.g, slot.dark.b, 1);
            if (premultiplyAlpha)
            {
                c.r *= c.a; c.g *= c.a; c.b *= c.a;
                dk.r *= c.a; dk.g *= c.a; dk.b *= c.a;
                if (sd.blend == BlendMode.Additive) c.a = 0;
            }

            // Source vertices with page uvs.
            var uv = new float[vcount * 2];
            for (int i = 0; i < vcount; i++)
            {
                float pu, pv;
                region.PageUV(page, srcUv[i * 2], srcUv[i * 2 + 1], out pu, out pv);
                uv[i * 2] = pu; uv[i * 2 + 1] = 1f - pv; // page v pointed down; flip for texture space
            }

            int startIndex = indexCount;
            if (clipping)
            {
                for (int t = 0; t + 2 < tris.Length; t += 3) ClipTriangle(world, uv, tris[t], tris[t + 1], tris[t + 2], c, dk);
            }
            else
            {
                EnsureVertices(vcount); EnsureIndices(tris.Length);
                int baseV = vertexCount;
                for (int i = 0; i < vcount; i++)
                {
                    positions[(baseV + i) * 2] = world[i * 2]; positions[(baseV + i) * 2 + 1] = world[i * 2 + 1];
                    uvs[(baseV + i) * 2] = uv[i * 2]; uvs[(baseV + i) * 2 + 1] = uv[i * 2 + 1];
                    colors[baseV + i] = c; darks[baseV + i] = dk;
                }
                for (int t = 0; t < tris.Length; t++) indices[indexCount++] = baseV + tris[t];
                vertexCount += vcount;
            }
            int added = indexCount - startIndex;
            if (added <= 0) return;
            if (subMeshes.Count > 0)
            {
                var last = subMeshes[subMeshes.Count - 1];
                if (last.page == region.page && last.blend == sd.blend) { last.indexCount += added; subMeshes[subMeshes.Count - 1] = last; return; }
            }
            subMeshes.Add(new SubMesh { page = region.page, blend = sd.blend, startIndex = startIndex, indexCount = added });
        }

        static readonly int[] RegionTris = { 0, 1, 2, 2, 3, 0 };
        static readonly float[] RegionUvs = { 0, 1, 0, 0, 1, 0, 1, 1 }; // BL, TL, TR, BR with v pointing down

        // ---- clipping (Sutherland-Hodgman against the convex pieces of the clip polygon) ----

        static float Area(List<float> p)
        {
            float a = 0; int n = p.Count / 2;
            for (int i = 0; i < n; i++) { int j = (i + 1) % n; a += p[i * 2] * p[j * 2 + 1] - p[j * 2] * p[i * 2 + 1]; }
            return a / 2;
        }

        // Ear clipping into triangles; every triangle is convex.
        static List<float[]> Convexes(List<float> poly)
        {
            var result = new List<float[]>();
            int n = poly.Count / 2;
            if (n < 3) return result;
            var idx = new List<int>();
            for (int i = 0; i < n; i++) idx.Add(i);
            if (Area(poly) < 0) idx.Reverse();
            int guard = 0;
            while (idx.Count > 3 && guard++ < 10000)
            {
                bool clipped = false;
                for (int i = 0; i < idx.Count; i++)
                {
                    int i0 = idx[(i + idx.Count - 1) % idx.Count], i1 = idx[i], i2 = idx[(i + 1) % idx.Count];
                    float ax = poly[i0 * 2], ay = poly[i0 * 2 + 1], bx = poly[i1 * 2], by = poly[i1 * 2 + 1], cx = poly[i2 * 2], cy = poly[i2 * 2 + 1];
                    if ((bx - ax) * (cy - ay) - (by - ay) * (cx - ax) <= 0) continue; // reflex
                    bool inside = false;
                    foreach (int j in idx)
                    {
                        if (j == i0 || j == i1 || j == i2) continue;
                        if (PointInTri(poly[j * 2], poly[j * 2 + 1], ax, ay, bx, by, cx, cy)) { inside = true; break; }
                    }
                    if (inside) continue;
                    result.Add(new[] { ax, ay, bx, by, cx, cy });
                    idx.RemoveAt(i); clipped = true; break;
                }
                if (!clipped) break;
            }
            if (idx.Count == 3)
            {
                int i0 = idx[0], i1 = idx[1], i2 = idx[2];
                result.Add(new[] { poly[i0 * 2], poly[i0 * 2 + 1], poly[i1 * 2], poly[i1 * 2 + 1], poly[i2 * 2], poly[i2 * 2 + 1] });
            }
            return result;
        }

        static bool PointInTri(float px, float py, float ax, float ay, float bx, float by, float cx, float cy)
        {
            float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
            float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
            float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
            bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
            return !(neg && pos);
        }

        // Vertex of the polygon being clipped: x, y, u, v.
        struct CV { public float x, y, u, v; }

        void ClipTriangle(float[] w, float[] uv, int i0, int i1, int i2, Color4 c, Color4 dk)
        {
            foreach (var convex in clipConvex)
            {
                var poly = new List<CV>
                {
                    new CV { x = w[i0 * 2], y = w[i0 * 2 + 1], u = uv[i0 * 2], v = uv[i0 * 2 + 1] },
                    new CV { x = w[i1 * 2], y = w[i1 * 2 + 1], u = uv[i1 * 2], v = uv[i1 * 2 + 1] },
                    new CV { x = w[i2 * 2], y = w[i2 * 2 + 1], u = uv[i2 * 2], v = uv[i2 * 2 + 1] }
                };
                for (int e = 0; e < 3 && poly.Count > 0; e++)
                {
                    float ex = convex[e * 2], ey = convex[e * 2 + 1];
                    float fx = convex[((e + 1) % 3) * 2], fy = convex[((e + 1) % 3) * 2 + 1];
                    var next = new List<CV>();
                    for (int i = 0; i < poly.Count; i++)
                    {
                        CV cur = poly[i], prev = poly[(i + poly.Count - 1) % poly.Count];
                        float sc = Side(ex, ey, fx, fy, cur.x, cur.y), sp = Side(ex, ey, fx, fy, prev.x, prev.y);
                        if (sc >= 0)
                        {
                            if (sp < 0) next.Add(Lerp(prev, cur, sp / (sp - sc)));
                            next.Add(cur);
                        }
                        else if (sp >= 0) next.Add(Lerp(prev, cur, sp / (sp - sc)));
                    }
                    poly = next;
                }
                if (poly.Count < 3) continue;
                EnsureVertices(poly.Count); EnsureIndices((poly.Count - 2) * 3);
                int baseV = vertexCount;
                foreach (var p in poly)
                {
                    positions[vertexCount * 2] = p.x; positions[vertexCount * 2 + 1] = p.y;
                    uvs[vertexCount * 2] = p.u; uvs[vertexCount * 2 + 1] = p.v;
                    colors[vertexCount] = c; darks[vertexCount] = dk; vertexCount++;
                }
                for (int k = 1; k + 1 < poly.Count; k++) { indices[indexCount++] = baseV; indices[indexCount++] = baseV + k; indices[indexCount++] = baseV + k + 1; }
            }
        }

        // The convex pieces are counter-clockwise (y up); positive = inside.
        static float Side(float ax, float ay, float bx, float by, float px, float py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); }
        static CV Lerp(CV a, CV b, float t) { return new CV { x = a.x + (b.x - a.x) * t, y = a.y + (b.y - a.y) * t, u = a.u + (b.u - a.u) * t, v = a.v + (b.v - a.v) * t }; }
    }
}
