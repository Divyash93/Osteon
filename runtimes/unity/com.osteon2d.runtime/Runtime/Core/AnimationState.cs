// Timeline playback and track/crossfade mixing. Engine independent.
using System;
using System.Collections.Generic;

namespace Osteon2D
{
    public enum MixBlend { Setup, First, Replace, Add }

    public class FiredEvent
    {
        public int eventIndex; public string name; public float time; public int intValue; public float floatValue; public string stringValue; public float volume, balance;
    }

    public static class AnimationApply
    {
        public static int KeyBefore(Key[] keys, float time)
        {
            int i = -1;
            for (int k = 0; k < keys.Length; k++) { if (keys[k].time <= time) i = k; else break; }
            return i;
        }

        public static float BezierValue(float time, float t0, float v0, float cx1, float cy1, float cx2, float cy2, float t1, float v1)
        {
            if (time <= t0) return v0;
            if (time >= t1) return v1;
            double s = (time - t0) / (double)(t1 - t0);
            for (int i = 0; i < 8; i++)
            {
                double u = 1 - s;
                double d = 3 * u * u * (cx1 - t0) + 6 * u * s * (cx2 - cx1) + 3 * s * s * (t1 - cx2);
                if (Math.Abs(d) < 1e-9) break;
                double bx0 = u * u * u * t0 + 3 * u * u * s * cx1 + 3 * u * s * s * cx2 + s * s * s * t1;
                double ns = s - (bx0 - time) / d;
                if (ns < 0 || ns > 1) break;
                s = ns;
            }
            Func<double, double> bx = q => { double u = 1 - q; return u * u * u * t0 + 3 * u * u * q * cx1 + 3 * u * q * q * cx2 + q * q * q * t1; };
            if (Math.Abs(bx(s) - time) > 1e-6)
            {
                double lo = 0, hi = 1;
                for (int i = 0; i < 50; i++) { s = (lo + hi) / 2; if (bx(s) < time) lo = s; else hi = s; }
            }
            double w = 1 - s;
            return (float)(w * w * w * v0 + 3 * w * w * s * cy1 + 3 * w * s * s * cy2 + s * s * s * v1);
        }

        public static float Value(Timeline tl, int vi, float time)
        {
            var keys = tl.keys;
            if (keys.Length == 0) return 0;
            int i = KeyBefore(keys, time);
            if (i < 0) return keys[0].v[vi];
            var k0 = keys[i];
            if (i + 1 >= keys.Length) return k0.v[vi];
            var k1 = keys[i + 1];
            if (k0.curve == CurveType.Stepped) return k0.v[vi];
            if (k0.curve == CurveType.Bezier && k0.bezier != null && k0.bezier.Length >= vi * 4 + 4)
            {
                int o = vi * 4;
                return BezierValue(time, k0.time, k0.v[vi], k0.bezier[o], k0.bezier[o + 1], k0.bezier[o + 2], k0.bezier[o + 3], k1.time, k1.v[vi]);
            }
            float span = k1.time - k0.time;
            float t = span > 0 ? (time - k0.time) / span : 0;
            return k0.v[vi] + (k1.v[vi] - k0.v[vi]) * t;
        }

        static float CurvePercent(Timeline tl, int i, float time)
        {
            var k0 = tl.keys[i]; var k1 = tl.keys[i + 1];
            if (k0.curve == CurveType.Stepped) return 0;
            float span = k1.time - k0.time;
            if (span <= 0) return 0;
            if (k0.curve == CurveType.Bezier && k0.bezier != null && k0.bezier.Length >= 4)
                return BezierValue(time, k0.time, 0, k0.bezier[0], k0.bezier[1], k0.bezier[2], k0.bezier[3], k1.time, 1);
            return (time - k0.time) / span;
        }

        static void ApplyRotate(Timeline tl, Skeleton sk, float time, float alpha, MixBlend blend)
        {
            var bone = sk.bones[tl.target];
            if (!bone.active) return;
            var bd = sk.data.bones[tl.target];
            if (time < tl.keys[0].time)
            {
                if (blend == MixBlend.Setup) bone.rotation = bd.rotation;
                else if (blend == MixBlend.First) bone.rotation += (bd.rotation - bone.rotation) * alpha;
                return;
            }
            float r = Value(tl, 0, time);
            switch (blend)
            {
                case MixBlend.Setup: bone.rotation = bd.rotation + r * alpha; break;
                case MixBlend.First:
                case MixBlend.Replace: r += bd.rotation - bone.rotation; bone.rotation += r * alpha; break;
                case MixBlend.Add: bone.rotation += r * alpha; break;
            }
        }

        static void Offset(ref float value, float setup, float v, float alpha, MixBlend blend, bool before)
        {
            if (before)
            {
                if (blend == MixBlend.Setup) value = setup;
                else if (blend == MixBlend.First) value += (setup - value) * alpha;
                return;
            }
            switch (blend)
            {
                case MixBlend.Setup: value = setup + v * alpha; break;
                case MixBlend.First:
                case MixBlend.Replace: value += (setup + v - value) * alpha; break;
                case MixBlend.Add: value += v * alpha; break;
            }
        }

        static void Scale(ref float value, float setup, float v, float alpha, MixBlend blend, bool before)
        {
            if (before)
            {
                if (blend == MixBlend.Setup) value = setup;
                else if (blend == MixBlend.First) value += (setup - value) * alpha;
                return;
            }
            float x = v * setup;
            if (alpha == 1)
            {
                if (blend == MixBlend.Add) value += x - setup; else value = x;
                return;
            }
            switch (blend)
            {
                case MixBlend.Setup: value = setup + (x - setup) * alpha; break;
                case MixBlend.First:
                case MixBlend.Replace: value += (x - value) * alpha; break;
                case MixBlend.Add: value += (x - setup) * alpha; break;
            }
        }

        static void ApplyBone(Timeline tl, Skeleton sk, float time, float alpha, MixBlend blend)
        {
            var b = sk.bones[tl.target];
            if (!b.active) return;
            var d = sk.data.bones[tl.target];
            bool before = time < tl.keys[0].time;
            switch (tl.type)
            {
                case TimelineType.Rotate: ApplyRotate(tl, sk, time, alpha, blend); break;
                case TimelineType.Translate:
                    Offset(ref b.x, d.x, before ? 0 : Value(tl, 0, time), alpha, blend, before);
                    Offset(ref b.y, d.y, before ? 0 : Value(tl, 1, time), alpha, blend, before); break;
                case TimelineType.TranslateX: Offset(ref b.x, d.x, before ? 0 : Value(tl, 0, time), alpha, blend, before); break;
                case TimelineType.TranslateY: Offset(ref b.y, d.y, before ? 0 : Value(tl, 0, time), alpha, blend, before); break;
                case TimelineType.Scale:
                    Scale(ref b.scaleX, d.scaleX, before ? 1 : Value(tl, 0, time), alpha, blend, before);
                    Scale(ref b.scaleY, d.scaleY, before ? 1 : Value(tl, 1, time), alpha, blend, before); break;
                case TimelineType.ScaleX: Scale(ref b.scaleX, d.scaleX, before ? 1 : Value(tl, 0, time), alpha, blend, before); break;
                case TimelineType.ScaleY: Scale(ref b.scaleY, d.scaleY, before ? 1 : Value(tl, 0, time), alpha, blend, before); break;
                case TimelineType.Shear:
                    Offset(ref b.shearX, d.shearX, before ? 0 : Value(tl, 0, time), alpha, blend, before);
                    Offset(ref b.shearY, d.shearY, before ? 0 : Value(tl, 1, time), alpha, blend, before); break;
                case TimelineType.ShearX: Offset(ref b.shearX, d.shearX, before ? 0 : Value(tl, 0, time), alpha, blend, before); break;
                case TimelineType.ShearY: Offset(ref b.shearY, d.shearY, before ? 0 : Value(tl, 0, time), alpha, blend, before); break;
                case TimelineType.Inherit:
                    if (before) { if (blend == MixBlend.Setup || blend == MixBlend.First) b.inherit = d.inherit; break; }
                    b.inherit = (Inherit)tl.keys[KeyBefore(tl.keys, time)].intValue;
                    break;
            }
        }

        static void MixColor(ref Color4 c, Color4 setup, Color4 v, float alpha, MixBlend blend, bool rgbOnly)
        {
            if (alpha == 1) { c.r = v.r; c.g = v.g; c.b = v.b; if (!rgbOnly) c.a = v.a; return; }
            if (blend == MixBlend.Setup) { c.r = setup.r; c.g = setup.g; c.b = setup.b; if (!rgbOnly) c.a = setup.a; }
            c.r += (v.r - c.r) * alpha; c.g += (v.g - c.g) * alpha; c.b += (v.b - c.b) * alpha;
            if (!rgbOnly) c.a += (v.a - c.a) * alpha;
        }

        static void ApplySlot(Timeline tl, Skeleton sk, float time, float alpha, MixBlend blend)
        {
            var s = sk.slots[tl.target];
            if (!sk.bones[s.bone].active) return;
            var sd = sk.data.slots[tl.target];
            bool before = time < tl.keys[0].time;
            if (tl.type == TimelineType.Attachment)
            {
                if (before) { if (blend == MixBlend.Setup || blend == MixBlend.First) s.attachment = sd.attachment; return; }
                string name = tl.keys[KeyBefore(tl.keys, time)].str;
                if (s.attachment != name) { s.attachment = name; s.deform = new float[0]; s.deformFor = null; }
                return;
            }
            if (before)
            {
                if (blend == MixBlend.Setup) { s.color = sd.color; s.dark = sd.dark; }
                else if (blend == MixBlend.First)
                {
                    MixColor(ref s.color, sd.color, sd.color, alpha, blend, false);
                    MixColor(ref s.dark, sd.dark, sd.dark, alpha, blend, true);
                }
                return;
            }
            if (tl.type == TimelineType.Alpha)
            {
                float a = Value(tl, 0, time);
                if (alpha == 1) s.color.a = a;
                else
                {
                    if (blend == MixBlend.Setup) s.color.a = sd.color.a;
                    s.color.a += (a - s.color.a) * alpha;
                }
                return;
            }
            bool hasAlpha = tl.type == TimelineType.RGBA || tl.type == TimelineType.RGBA2;
            var light = new Color4(Value(tl, 0, time), Value(tl, 1, time), Value(tl, 2, time), hasAlpha ? Value(tl, 3, time) : s.color.a);
            MixColor(ref s.color, sd.color, light, alpha, blend, !hasAlpha);
            if (tl.type == TimelineType.RGBA2 || tl.type == TimelineType.RGB2)
            {
                int o = hasAlpha ? 4 : 3;
                MixColor(ref s.dark, sd.dark, new Color4(Value(tl, o, time), Value(tl, o + 1, time), Value(tl, o + 2, time), 1), alpha, blend, true);
            }
        }

        static void ApplyDeform(Timeline tl, Skeleton sk, float time, float alpha, MixBlend blend)
        {
            var s = sk.slots[tl.target];
            if (!sk.bones[s.bone].active) return;
            var cur = sk.AttachmentFor(s);
            if (cur == null) return;
            bool match = cur.name == tl.attachment;
            if (!match && cur.type == AttachmentType.LinkedMesh && cur.inheritTimelines) match = cur.parentMesh == tl.attachment;
            if (!match) return;
            var geom = sk.Geometry(s, cur);
            if (geom == null) return;
            int n = geom.DeformLength;
            if (s.deformFor != cur.name) { s.deform = new float[0]; s.deformFor = cur.name; }
            if (s.deform.Length != n) s.deform = new float[n];
            var target = new float[n];
            if (time >= tl.keys[0].time)
            {
                int i = KeyBefore(tl.keys, time);
                if (i + 1 >= tl.keys.Length)
                    for (int j = 0; j < n; j++) target[j] = j < tl.keys[i].deform.Length ? tl.keys[i].deform[j] : 0;
                else
                {
                    float p = CurvePercent(tl, i, time);
                    var ka = tl.keys[i].deform; var kb = tl.keys[i + 1].deform;
                    for (int j = 0; j < n; j++)
                    {
                        float a = j < ka.Length ? ka[j] : 0, b = j < kb.Length ? kb[j] : 0;
                        target[j] = a + (b - a) * p;
                    }
                }
            }
            else if (blend != MixBlend.Setup && blend != MixBlend.First) return;
            for (int j = 0; j < n; j++)
            {
                switch (blend)
                {
                    case MixBlend.Setup: s.deform[j] = target[j] * alpha; break;
                    case MixBlend.Add: s.deform[j] += target[j] * alpha; break;
                    default: s.deform[j] += (target[j] - s.deform[j]) * alpha; break;
                }
            }
        }

        static void ApplySequence(Timeline tl, Skeleton sk, float time, MixBlend blend)
        {
            var s = sk.slots[tl.target];
            if (!sk.bones[s.bone].active) return;
            var att = sk.AttachmentFor(s);
            if (att == null || att.seqCount <= 0 || (tl.attachment != null && att.name != tl.attachment)) return;
            if (time < tl.keys[0].time)
            {
                if (blend == MixBlend.Setup || blend == MixBlend.First) s.sequenceIndex = -1;
                return;
            }
            var k = tl.keys[KeyBefore(tl.keys, time)];
            int count = att.seqCount, index = k.intValue;
            string mode = k.str;
            if (mode != "hold" && !string.IsNullOrEmpty(mode) && k.v[0] > 0)
            {
                index += (int)((time - k.time) / k.v[0] + 0.0001f);
                int n = count * 2 - 2;
                if (mode == "once") index = Math.Min(count - 1, index);
                else if (mode == "loop") index %= count;
                else if (mode == "pingpong") { index = n == 0 ? 0 : index % n; if (index >= count) index = n - index; }
                else if (mode == "onceReverse") index = Math.Max(count - 1 - index, 0);
                else if (mode == "loopReverse") index = count - 1 - (index % count);
                else if (mode == "pingpongReverse") { index = n == 0 ? 0 : (index + count - 1) % n; if (index >= count) index = n - index; }
            }
            s.sequenceIndex = Math.Max(0, Math.Min(count - 1, index));
        }

        static void ApplyDrawOrder(Timeline tl, Skeleton sk, float time, MixBlend blend)
        {
            if (time < tl.keys[0].time)
            {
                if (blend == MixBlend.Setup || blend == MixBlend.First) for (int i = 0; i < sk.drawOrder.Length; i++) sk.drawOrder[i] = i;
                return;
            }
            var k = tl.keys[KeyBefore(tl.keys, time)];
            if (k.drawOrder == null || k.drawOrder.Length != sk.drawOrder.Length) for (int i = 0; i < sk.drawOrder.Length; i++) sk.drawOrder[i] = i;
            else Array.Copy(k.drawOrder, sk.drawOrder, k.drawOrder.Length);
        }

        static void FireEvents(Timeline tl, SkeletonData data, float lastTime, float time, List<FiredEvent> output)
        {
            if (output == null) return;
            foreach (var k in tl.keys)
            {
                if (!(k.time > lastTime && k.time <= time)) continue;
                var e = new FiredEvent { eventIndex = data.FindEvent(k.str), name = k.str, time = k.time, intValue = k.intValue, floatValue = k.v[0], volume = k.volume, balance = k.balance, stringValue = "" };
                if (e.eventIndex >= 0)
                {
                    var ed = data.events[e.eventIndex];
                    if (!k.flag1) e.intValue = ed.intValue;
                    if (!k.flag2) e.floatValue = ed.floatValue;
                    e.stringValue = k.flag3 ? k.str2 : ed.stringValue;
                }
                output.Add(e);
            }
        }

        static void ApplyConstraint(Timeline tl, Skeleton sk, float time, float alpha, MixBlend blend)
        {
            bool before = time < tl.keys[0].time;
            if (tl.type == TimelineType.Ik)
            {
                if (tl.target >= sk.ik.Length) return;
                var c = sk.ik[tl.target]; var d = sk.data.ik[tl.target];
                c.mix = MixValue(c.mix, d.mix, before ? 0 : Value(tl, 0, time), before, alpha, blend);
                c.softness = MixValue(c.softness, d.softness, before ? 0 : Value(tl, 1, time), before, alpha, blend);
                if (before)
                {
                    if (blend == MixBlend.Setup || blend == MixBlend.First) { c.bendDirection = d.bendDirection; c.compress = d.compress; c.stretch = d.stretch; }
                }
                else
                {
                    var k = tl.keys[KeyBefore(tl.keys, time)];
                    c.bendDirection = k.flag1 ? 1 : -1; c.compress = k.flag2; c.stretch = k.flag3;
                }
            }
            else if (tl.type == TimelineType.Transform)
            {
                if (tl.target >= sk.transforms.Length) return;
                var c = sk.transforms[tl.target]; var d = sk.data.transforms[tl.target];
                Func<int, float> v = i => before ? 0 : Value(tl, i, time);
                c.mixRotate = MixValue(c.mixRotate, d.mixRotate, v(0), before, alpha, blend);
                c.mixX = MixValue(c.mixX, d.mixX, v(1), before, alpha, blend);
                c.mixY = MixValue(c.mixY, d.mixY, v(2), before, alpha, blend);
                c.mixScaleX = MixValue(c.mixScaleX, d.mixScaleX, v(3), before, alpha, blend);
                c.mixScaleY = MixValue(c.mixScaleY, d.mixScaleY, v(4), before, alpha, blend);
                c.mixShearY = MixValue(c.mixShearY, d.mixShearY, v(5), before, alpha, blend);
            }
        }

        static float MixValue(float value, float setup, float target, bool before, float alpha, MixBlend blend)
        {
            if (before)
            {
                if (blend == MixBlend.Setup) return setup;
                if (blend == MixBlend.First) return value + (setup - value) * alpha;
                return value;
            }
            if (blend == MixBlend.Setup) return setup + (target - setup) * alpha;
            return value + (target - value) * alpha;
        }

        /// <summary>Applies <paramref name="anim"/> to the skeleton's local pose. Events passed by the playhead go to <paramref name="events"/>.</summary>
        public static void Apply(Animation anim, Skeleton sk, float lastTime, float time, bool loop, List<FiredEvent> events, float alpha, MixBlend blend)
        {
            float duration = anim.duration;
            if (loop && duration > 0)
            {
                time = time % duration;
                if (lastTime > 0) lastTime = lastTime % duration;
            }
            foreach (var tl in anim.timelines)
            {
                if (tl.keys.Length == 0) continue;
                switch (tl.type)
                {
                    case TimelineType.Event:
                        if (lastTime > time) { FireEvents(tl, sk.data, lastTime, duration, events); FireEvents(tl, sk.data, -1, time, events); }
                        else FireEvents(tl, sk.data, lastTime, time, events);
                        break;
                    case TimelineType.Rotate: case TimelineType.Translate: case TimelineType.TranslateX: case TimelineType.TranslateY:
                    case TimelineType.Scale: case TimelineType.ScaleX: case TimelineType.ScaleY: case TimelineType.Shear:
                    case TimelineType.ShearX: case TimelineType.ShearY: case TimelineType.Inherit:
                        if (tl.target >= 0 && tl.target < sk.bones.Length) ApplyBone(tl, sk, time, alpha, blend);
                        break;
                    case TimelineType.Attachment: case TimelineType.RGBA: case TimelineType.RGB: case TimelineType.Alpha:
                    case TimelineType.RGBA2: case TimelineType.RGB2:
                        if (tl.target >= 0 && tl.target < sk.slots.Length) ApplySlot(tl, sk, time, alpha, blend);
                        break;
                    case TimelineType.Deform:
                        if (tl.target >= 0 && tl.target < sk.slots.Length) ApplyDeform(tl, sk, time, alpha, blend);
                        break;
                    case TimelineType.Sequence:
                        if (tl.target >= 0 && tl.target < sk.slots.Length) ApplySequence(tl, sk, time, blend);
                        break;
                    case TimelineType.DrawOrder:
                        if (alpha >= 0.5f || blend == MixBlend.Setup) ApplyDrawOrder(tl, sk, time, blend);
                        break;
                    default: ApplyConstraint(tl, sk, time, alpha, blend); break;
                }
            }
        }
    }

    /// <summary>Track based playback with crossfades.</summary>
    public class AnimationState
    {
        public class Entry
        {
            public int animation = -1; public bool loop = true;
            public float trackTime, lastTime = -1, timeScale = 1, alpha = 1, mixTime, mixDuration, delay, animationStart, animationEnd;
            public Entry from, next;
        }

        public readonly SkeletonData data;
        public float timeScale = 1, defaultMix = 0.2f;
        public readonly Dictionary<long, float> mixes = new Dictionary<long, float>();
        public readonly List<Entry> tracks = new List<Entry>();
        public Action<int, FiredEvent> onEvent;

        public AnimationState(SkeletonData data) { this.data = data; }

        static long Key(int from, int to) { return ((long)from << 32) | (uint)to; }
        public void SetMix(string from, string to, float duration) { mixes[Key(data.FindAnimation(from), data.FindAnimation(to))] = duration; }
        public float MixDuration(int from, int to) { float v; return mixes.TryGetValue(Key(from, to), out v) ? v : defaultMix; }

        public Entry SetAnimation(int track, string name, bool loop)
        {
            int a = data.FindAnimation(name);
            if (a < 0) throw new ArgumentException("Animation not found: " + name);
            return SetAnimation(track, a, loop);
        }

        public Entry SetAnimation(int track, int animation, bool loop)
        {
            while (tracks.Count <= track) tracks.Add(null);
            var e = new Entry { animation = animation, loop = loop };
            if (animation >= 0) e.animationEnd = data.animations[animation].duration;
            var cur = tracks[track];
            if (cur != null && cur.animation >= 0)
            {
                e.from = cur; cur.next = null;
                e.mixDuration = MixDuration(cur.animation, animation);
            }
            tracks[track] = e;
            return e;
        }

        public Entry AddAnimation(int track, string name, bool loop, float delay)
        {
            int animation = data.FindAnimation(name);
            if (animation < 0) throw new ArgumentException("Animation not found: " + name);
            if (tracks.Count <= track || tracks[track] == null) return SetAnimation(track, animation, loop);
            var last = tracks[track];
            while (last.next != null) last = last.next;
            var e = new Entry { animation = animation, loop = loop, animationEnd = data.animations[animation].duration };
            if (delay <= 0)
            {
                float d = last.animationEnd - last.animationStart;
                delay = Math.Max(0f, d + delay - MixDuration(last.animation, animation));
            }
            e.delay = delay;
            last.next = e;
            return e;
        }

        public void ClearTrack(int track) { if (track < tracks.Count) tracks[track] = null; }
        public void ClearTracks() { tracks.Clear(); }

        public void Update(float delta)
        {
            delta *= timeScale;
            for (int i = 0; i < tracks.Count; i++)
            {
                var t = tracks[i];
                if (t == null) continue;
                float d = delta * t.timeScale;
                t.lastTime = t.trackTime;
                t.trackTime += d;
                if (t.from != null)
                {
                    t.mixTime += d;
                    if (t.mixTime >= t.mixDuration) t.from = null;
                    else { t.from.lastTime = t.from.trackTime; t.from.trackTime += d; }
                }
                if (t.next != null)
                {
                    float dur = t.animationEnd - t.animationStart;
                    if (t.trackTime >= t.next.delay || (!t.loop && t.trackTime >= dur && t.next.delay <= t.trackTime))
                    {
                        var n = t.next;
                        n.from = t;
                        n.mixDuration = MixDuration(t.animation, n.animation);
                        t.next = null;
                        tracks[i] = n;
                    }
                }
            }
        }

        public void Apply(Skeleton sk)
        {
            var events = new List<FiredEvent>();
            for (int ti = 0; ti < tracks.Count; ti++)
            {
                var t = tracks[ti];
                if (t == null || t.animation < 0 || t.animation >= data.animations.Count) continue;
                var blend = ti == 0 ? MixBlend.First : MixBlend.Replace;
                float alpha = t.alpha;
                if (t.from != null && t.from.animation >= 0 && t.from.animation < data.animations.Count)
                {
                    AnimationApply.Apply(data.animations[t.from.animation], sk, t.from.lastTime, t.from.trackTime, t.from.loop, null, 1, blend);
                    blend = MixBlend.Replace;
                    alpha *= t.mixDuration > 0 ? Math.Min(1f, t.mixTime / t.mixDuration) : 1f;
                }
                var a = data.animations[t.animation];
                float dur = a.duration;
                float time = t.loop ? t.trackTime : Math.Min(t.trackTime, dur);
                float last = t.lastTime < 0 ? -1 : (t.loop ? t.lastTime : Math.Min(t.lastTime, dur));
                int before = events.Count;
                AnimationApply.Apply(a, sk, last, time, t.loop, events, alpha, blend);
                if (onEvent != null) for (int i = before; i < events.Count; i++) onEvent(ti, events[i]);
            }
        }
    }
}
