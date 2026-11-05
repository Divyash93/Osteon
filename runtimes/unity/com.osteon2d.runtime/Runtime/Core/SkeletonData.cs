// Setup-pose data and the Spine-JSON (4.x) loader. Independent of any game engine.
// Written for Osteon2D from the public JSON format description; it contains no Esoteric Software code.
using System;
using System.Collections.Generic;
using System.Globalization;

namespace Osteon2D
{
    public enum Inherit { Normal, OnlyTranslation, NoRotationOrReflection, NoScale, NoScaleOrReflection }
    public enum BlendMode { Normal, Additive, Multiply, Screen }
    public enum AttachmentType { Region, Mesh, LinkedMesh, BoundingBox, Path, Point, Clipping }
    public enum CurveType { Linear, Stepped, Bezier }

    public enum TimelineType
    {
        Rotate, Translate, TranslateX, TranslateY, Scale, ScaleX, ScaleY, Shear, ShearX, ShearY, Inherit,
        Attachment, RGBA, RGB, Alpha, RGBA2, RGB2, Deform, Sequence, DrawOrder, Event, Ik, Transform
    }

    public struct Color4
    {
        public float r, g, b, a;
        public Color4(float r, float g, float b, float a) { this.r = r; this.g = g; this.b = b; this.a = a; }
        public static Color4 White { get { return new Color4(1, 1, 1, 1); } }
        public static Color4 FromHex(string hex, string fallback)
        {
            if (string.IsNullOrEmpty(hex)) hex = fallback;
            if (hex[0] == '#') hex = hex.Substring(1);
            if (hex.Length == 6) hex += "ff";
            if (hex.Length != 8) hex = fallback;
            Func<int, float> n = k => int.Parse(hex.Substring(k, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture) / 255f;
            return new Color4(n(0), n(2), n(4), n(6));
        }
    }

    public class BoneData
    {
        public string name; public int parent = -1; public float length;
        public float x, y, rotation, scaleX = 1, scaleY = 1, shearX, shearY;
        public Inherit inherit; public bool skinRequired;
    }

    public class SlotData
    {
        public string name; public int bone; public Color4 color = Color4.White;
        public bool hasDark; public Color4 dark = new Color4(0, 0, 0, 1);
        public string attachment; public BlendMode blend;
    }

    public class Attachment
    {
        public AttachmentType type; public string name, path;
        public Color4 color = Color4.White;
        public float x, y, rotation, scaleX = 1, scaleY = 1, width, height;
        public int[] bones;          // weighted layout: per vertex [count, boneIndex * count]; null = unweighted
        public float[] vertices;     // unweighted [x, y] * n, weighted [x, y, weight] * influences
        public int worldVerticesLength;
        public float[] uvs; public int[] triangles; public int hullLength;
        public string parentMesh, parentSkin; public bool inheritTimelines = true;
        public string endSlot;
        public int seqCount, seqStart = 1, seqDigits, seqSetup;
        public bool IsWeighted { get { return bones != null && bones.Length > 0; } }
        public int DeformLength { get { return IsWeighted ? vertices.Length / 3 * 2 : vertices.Length; } }
    }

    public class Skin
    {
        public string name;
        public Dictionary<int, Dictionary<string, Attachment>> attachments = new Dictionary<int, Dictionary<string, Attachment>>();
        public List<int> bones = new List<int>();
        public List<string> constraints = new List<string>();
        public Attachment Get(int slot, string name)
        {
            Dictionary<string, Attachment> m; Attachment a;
            return attachments.TryGetValue(slot, out m) && m.TryGetValue(name, out a) ? a : null;
        }
        public void Set(int slot, string name, Attachment a)
        {
            Dictionary<string, Attachment> m;
            if (!attachments.TryGetValue(slot, out m)) attachments[slot] = m = new Dictionary<string, Attachment>();
            m[name] = a;
        }
    }

    public class EventData
    {
        public string name; public int intValue; public float floatValue; public string stringValue, audioPath; public float volume = 1, balance;
    }

    public class IkConstraintData
    {
        public string name; public int order; public bool skinRequired; public int[] bones; public int target;
        public float mix = 1, softness; public int bendDirection = 1; public bool compress, stretch, uniform;
    }

    public class TransformConstraintData
    {
        public string name; public int order; public bool skinRequired; public int[] bones; public int target;
        public float mixRotate = 1, mixX = 1, mixY = 1, mixScaleX = 1, mixScaleY = 1, mixShearY = 1;
        public float offsetRotation, offsetX, offsetY, offsetScaleX, offsetScaleY, offsetShearY;
        public bool relative, local;
    }

    public class Key
    {
        public float time; public float[] v = new float[7];
        public CurveType curve; public float[] bezier;
        public string str, str2; public float[] deform; public int[] drawOrder;
        public bool flag1, flag2, flag3; public int intValue; public float volume = 1, balance;
    }

    public class Timeline
    {
        public TimelineType type; public int target = -1; public string skin, attachment;
        public Key[] keys = new Key[0];
    }

    public class Animation
    {
        public string name; public Timeline[] timelines; public float duration;
    }

    public class SkeletonData
    {
        public string name = "skeleton", version = "", imagesPath = "";
        public List<BoneData> bones = new List<BoneData>();
        public List<SlotData> slots = new List<SlotData>();
        public List<Skin> skins = new List<Skin>();
        public List<EventData> events = new List<EventData>();
        public List<Animation> animations = new List<Animation>();
        public List<IkConstraintData> ik = new List<IkConstraintData>();
        public List<TransformConstraintData> transforms = new List<TransformConstraintData>();
        public List<string> warnings = new List<string>();
        public float x, y, width, height, fps = 30;

        public int FindBone(string n) { return bones.FindIndex(b => b.name == n); }
        public int FindSlot(string n) { return slots.FindIndex(s => s.name == n); }
        public int FindSkin(string n) { return skins.FindIndex(s => s.name == n); }
        public int FindAnimation(string n) { return animations.FindIndex(a => a.name == n); }
        public int FindEvent(string n) { return events.FindIndex(e => e.name == n); }

        public Attachment FindAttachment(int skinIndex, int slot, string name)
        {
            if (string.IsNullOrEmpty(name)) return null;
            if (skinIndex >= 0 && skinIndex < skins.Count)
            {
                var a = skins[skinIndex].Get(slot, name);
                if (a != null) return a;
            }
            return skins.Count > 0 ? skins[0].Get(slot, name) : null;
        }

        public static SkeletonData FromJson(string text) { return new SkeletonLoader().Load(text); }
    }

    /// <summary>Reads Spine-format 4.x skeleton JSON (also the "skeleton" part of an .osteon project).</summary>
    public sealed class SkeletonLoader
    {
        SkeletonData d;
        static Dictionary<string, object> O(object o) { return MiniJson.Obj(o); }

        static Inherit InheritFrom(string s)
        {
            switch (s)
            {
                case "onlyTranslation": return Inherit.OnlyTranslation;
                case "noRotationOrReflection": return Inherit.NoRotationOrReflection;
                case "noScale": return Inherit.NoScale;
                case "noScaleOrReflection": return Inherit.NoScaleOrReflection;
                default: return Inherit.Normal;
            }
        }
        static BlendMode BlendFrom(string s)
        {
            switch (s)
            {
                case "additive": return BlendMode.Additive;
                case "multiply": return BlendMode.Multiply;
                case "screen": return BlendMode.Screen;
                default: return BlendMode.Normal;
            }
        }

        public SkeletonData Load(string text)
        {
            d = new SkeletonData();
            var root = O(MiniJson.Parse(text));
            if (root == null) throw new FormatException("Not a skeleton JSON object");
            var sk = O(root.ContainsKey("skeleton") ? root["skeleton"] : null);
            if (sk != null)
            {
                d.version = MiniJson.Str(sk, "spine", "");
                d.x = MiniJson.Num(sk, "x", 0); d.y = MiniJson.Num(sk, "y", 0);
                d.width = MiniJson.Num(sk, "width", 0); d.height = MiniJson.Num(sk, "height", 0);
                d.fps = MiniJson.Num(sk, "fps", 30); d.imagesPath = MiniJson.Str(sk, "images", "");
            }
            if (d.version.StartsWith("3"))
                throw new NotSupportedException("Osteon2D runtimes read 4.x skeleton data; re-export this skeleton from Osteon2D.");

            foreach (var bo in MiniJson.Arr(Get(root, "bones")) ?? new List<object>())
            {
                var bj = O(bo);
                var b = new BoneData();
                b.name = MiniJson.Str(bj, "name", "");
                string parent = MiniJson.Str(bj, "parent", "");
                if (parent != "") { b.parent = d.FindBone(parent); if (b.parent < 0) throw new FormatException("Parent bone not found: " + parent); }
                b.length = MiniJson.Num(bj, "length", 0);
                b.x = MiniJson.Num(bj, "x", 0); b.y = MiniJson.Num(bj, "y", 0); b.rotation = MiniJson.Num(bj, "rotation", 0);
                b.scaleX = MiniJson.Num(bj, "scaleX", 1); b.scaleY = MiniJson.Num(bj, "scaleY", 1);
                b.shearX = MiniJson.Num(bj, "shearX", 0); b.shearY = MiniJson.Num(bj, "shearY", 0);
                b.inherit = InheritFrom(MiniJson.Str(bj, "inherit", MiniJson.Str(bj, "transform", "normal")));
                b.skinRequired = MiniJson.Bool(bj, "skin", false);
                d.bones.Add(b);
            }
            if (d.bones.Count == 0) d.bones.Add(new BoneData { name = "root" });

            foreach (var so in MiniJson.Arr(Get(root, "slots")) ?? new List<object>())
            {
                var sj = O(so);
                var s = new SlotData();
                s.name = MiniJson.Str(sj, "name", "");
                s.bone = d.FindBone(MiniJson.Str(sj, "bone", ""));
                if (s.bone < 0) throw new FormatException("Slot bone not found for slot " + s.name);
                s.color = Color4.FromHex(MiniJson.Str(sj, "color", ""), "ffffffff");
                if (sj.ContainsKey("dark")) { s.hasDark = true; s.dark = Color4.FromHex(MiniJson.Str(sj, "dark", ""), "000000ff"); }
                s.attachment = MiniJson.Str(sj, "attachment", "");
                s.blend = BlendFrom(MiniJson.Str(sj, "blend", "normal"));
                d.slots.Add(s);
            }

            Func<Dictionary<string, object>, string, int[]> boneList = (cj, key) =>
            {
                var r = new List<int>();
                foreach (var n in MiniJson.Arr(Get(cj, key)) ?? new List<object>()) { int bi = d.FindBone((string)n); if (bi >= 0) r.Add(bi); }
                return r.ToArray();
            };
            foreach (var co in MiniJson.Arr(Get(root, "ik")) ?? new List<object>())
            {
                var cj = O(co);
                var c = new IkConstraintData();
                c.name = MiniJson.Str(cj, "name", ""); c.order = MiniJson.Int(cj, "order", 0); c.skinRequired = MiniJson.Bool(cj, "skin", false);
                c.bones = boneList(cj, "bones"); c.target = d.FindBone(MiniJson.Str(cj, "target", ""));
                c.mix = MiniJson.Num(cj, "mix", 1); c.softness = MiniJson.Num(cj, "softness", 0);
                c.bendDirection = MiniJson.Bool(cj, "bendPositive", true) ? 1 : -1;
                c.compress = MiniJson.Bool(cj, "compress", false); c.stretch = MiniJson.Bool(cj, "stretch", false); c.uniform = MiniJson.Bool(cj, "uniform", false);
                if (c.bones.Length == 0 || c.target < 0) { d.warnings.Add("IK constraint ignored: " + c.name); continue; }
                d.ik.Add(c);
            }
            foreach (var co in MiniJson.Arr(Get(root, "transform")) ?? new List<object>())
            {
                var cj = O(co);
                var c = new TransformConstraintData();
                c.name = MiniJson.Str(cj, "name", ""); c.order = MiniJson.Int(cj, "order", 0); c.skinRequired = MiniJson.Bool(cj, "skin", false);
                c.bones = boneList(cj, "bones"); c.target = d.FindBone(MiniJson.Str(cj, "target", ""));
                c.local = MiniJson.Bool(cj, "local", false); c.relative = MiniJson.Bool(cj, "relative", false) || MiniJson.Bool(cj, "additive", false);
                c.offsetRotation = MiniJson.Num(cj, "rotation", 0); c.offsetX = MiniJson.Num(cj, "x", 0); c.offsetY = MiniJson.Num(cj, "y", 0);
                c.offsetScaleX = MiniJson.Num(cj, "scaleX", 0); c.offsetScaleY = MiniJson.Num(cj, "scaleY", 0); c.offsetShearY = MiniJson.Num(cj, "shearY", 0);
                c.mixRotate = MiniJson.Num(cj, "mixRotate", 1); c.mixX = MiniJson.Num(cj, "mixX", 1); c.mixY = MiniJson.Num(cj, "mixY", c.mixX);
                c.mixScaleX = MiniJson.Num(cj, "mixScaleX", 1); c.mixScaleY = MiniJson.Num(cj, "mixScaleY", c.mixScaleX); c.mixShearY = MiniJson.Num(cj, "mixShearY", 1);
                if (cj.ContainsKey("properties")) d.warnings.Add("Transform constraint '" + c.name + "' uses 4.3 property links, which this runtime does not apply yet");
                if (c.bones.Length == 0 || c.target < 0) { d.warnings.Add("Transform constraint ignored: " + c.name); continue; }
                d.transforms.Add(c);
            }
            foreach (string k in new[] { "path", "physics", "slider" })
                if (MiniJson.Arr(Get(root, k)) != null && MiniJson.Arr(Get(root, k)).Count > 0)
                    d.warnings.Add("'" + k + "' constraints are not supported by this runtime yet; their bones keep the animated pose");

            var skins = Get(root, "skins");
            if (skins is List<object>)
            {
                foreach (var so in (List<object>)skins)
                {
                    var sj = O(so);
                    var skin = new Skin { name = MiniJson.Str(sj, "name", "") };
                    skin.bones.AddRange(boneList(sj, "bones"));
                    foreach (string key in new[] { "ik", "transform", "path", "physics", "slider", "constraints" })
                        foreach (var n in MiniJson.Arr(Get(sj, key)) ?? new List<object>()) skin.constraints.Add((string)n);
                    ReadSkinAttachments(O(Get(sj, "attachments")), skin);
                    d.skins.Add(skin);
                }
            }
            int def = d.FindSkin("default");
            if (def > 0) { var s0 = d.skins[def]; d.skins.RemoveAt(def); d.skins.Insert(0, s0); }
            if (d.skins.Count == 0) d.skins.Add(new Skin { name = "default" });

            var ev = O(Get(root, "events"));
            if (ev != null)
                foreach (var kv in ev)
                {
                    var ej = O(kv.Value);
                    d.events.Add(new EventData
                    {
                        name = kv.Key, intValue = MiniJson.Int(ej, "int", 0), floatValue = MiniJson.Num(ej, "float", 0),
                        stringValue = MiniJson.Str(ej, "string", ""), audioPath = MiniJson.Str(ej, "audio", ""),
                        volume = MiniJson.Num(ej, "volume", 1), balance = MiniJson.Num(ej, "balance", 0)
                    });
                }
            var anims = O(Get(root, "animations"));
            if (anims != null) foreach (var kv in anims) d.animations.Add(ReadAnimation(kv.Key, O(kv.Value)));
            return d;
        }

        static object Get(Dictionary<string, object> o, string k) { object v; return o != null && o.TryGetValue(k, out v) ? v : null; }

        void ReadSkinAttachments(Dictionary<string, object> atts, Skin skin)
        {
            if (atts == null) return;
            foreach (var sl in atts)
            {
                int slot = d.FindSlot(sl.Key);
                if (slot < 0) { d.warnings.Add("Skin '" + skin.name + "': unknown slot " + sl.Key); continue; }
                foreach (var at in O(sl.Value))
                {
                    var a = ReadAttachment(O(at.Value), at.Key);
                    if (a != null) { a.name = at.Key; skin.Set(slot, at.Key, a); }
                }
            }
        }

        void ReadVertices(Dictionary<string, object> j, Attachment a, int verticesLength)
        {
            float[] v = MiniJson.Floats(j, "vertices");
            a.worldVerticesLength = verticesLength;
            if (v.Length == verticesLength) { a.vertices = v; a.bones = null; return; }
            var bones = new List<int>(); var verts = new List<float>();
            int boneCount = d.bones.Count;
            for (int i = 0; i < v.Length;)
            {
                int count = (int)v[i++];
                bones.Add(count);
                for (int k = 0; k < count; k++)
                {
                    int b = (int)v[i];
                    if (b < 0 || b >= boneCount) throw new FormatException("Weighted vertex references bone " + b);
                    bones.Add(b); verts.Add(v[i + 1]); verts.Add(v[i + 2]); verts.Add(v[i + 3]);
                    i += 4;
                }
            }
            a.bones = bones.ToArray(); a.vertices = verts.ToArray();
        }

        Attachment ReadAttachment(Dictionary<string, object> j, string name)
        {
            var a = new Attachment();
            a.name = MiniJson.Str(j, "name", name);
            a.path = MiniJson.Str(j, "path", "");
            a.color = Color4.FromHex(MiniJson.Str(j, "color", ""), "ffffffff");
            var seq = O(Get(j, "sequence"));
            if (seq != null)
            {
                a.seqCount = MiniJson.Int(seq, "count", 0); a.seqStart = MiniJson.Int(seq, "start", 1);
                a.seqDigits = MiniJson.Int(seq, "digits", 0); a.seqSetup = MiniJson.Int(seq, "setup", 0);
            }
            string type = MiniJson.Str(j, "type", "region");
            switch (type)
            {
                case "region":
                    a.type = AttachmentType.Region;
                    a.x = MiniJson.Num(j, "x", 0); a.y = MiniJson.Num(j, "y", 0);
                    a.scaleX = MiniJson.Num(j, "scaleX", 1); a.scaleY = MiniJson.Num(j, "scaleY", 1);
                    a.rotation = MiniJson.Num(j, "rotation", 0);
                    a.width = MiniJson.Num(j, "width", 32); a.height = MiniJson.Num(j, "height", 32);
                    break;
                case "mesh":
                    a.type = AttachmentType.Mesh;
                    a.uvs = MiniJson.Floats(j, "uvs"); a.triangles = MiniJson.Ints(j, "triangles");
                    ReadVertices(j, a, a.uvs.Length);
                    a.hullLength = MiniJson.Int(j, "hull", 0);
                    a.width = MiniJson.Num(j, "width", 0); a.height = MiniJson.Num(j, "height", 0);
                    break;
                case "linkedmesh":
                    a.type = AttachmentType.LinkedMesh;
                    a.parentMesh = MiniJson.Str(j, "parent", ""); a.parentSkin = MiniJson.Str(j, "skin", "");
                    a.inheritTimelines = MiniJson.Bool(j, "timelines", MiniJson.Bool(j, "deform", true));
                    a.width = MiniJson.Num(j, "width", 0); a.height = MiniJson.Num(j, "height", 0);
                    break;
                case "boundingbox": a.type = AttachmentType.BoundingBox; ReadVertices(j, a, MiniJson.Int(j, "vertexCount", 0) * 2); break;
                case "path": a.type = AttachmentType.Path; ReadVertices(j, a, MiniJson.Int(j, "vertexCount", 0) * 2); break;
                case "point": a.type = AttachmentType.Point; a.x = MiniJson.Num(j, "x", 0); a.y = MiniJson.Num(j, "y", 0); a.rotation = MiniJson.Num(j, "rotation", 0); break;
                case "clipping":
                    a.type = AttachmentType.Clipping; a.endSlot = MiniJson.Str(j, "end", "");
                    ReadVertices(j, a, MiniJson.Int(j, "vertexCount", 0) * 2);
                    break;
                default: d.warnings.Add("Unknown attachment type " + type); return null;
            }
            return a;
        }

        // ---- animations ----

        static int ValueCount(TimelineType t)
        {
            switch (t)
            {
                case TimelineType.Translate: case TimelineType.Scale: case TimelineType.Shear: case TimelineType.Ik: return 2;
                case TimelineType.RGBA: return 4;
                case TimelineType.RGB: return 3;
                case TimelineType.RGBA2: return 7;
                case TimelineType.RGB2: return 6;
                case TimelineType.Transform: return 6;
                case TimelineType.Attachment: case TimelineType.DrawOrder: case TimelineType.Event: case TimelineType.Inherit: case TimelineType.Sequence: return 0;
                default: return 1;
            }
        }

        static void ReadCurve(Dictionary<string, object> kj, Key key)
        {
            object c;
            if (!kj.TryGetValue("curve", out c)) return;
            if (c is string) { if ((string)c == "stepped") key.curve = CurveType.Stepped; return; }
            var l = c as List<object>;
            if (l != null)
            {
                key.curve = CurveType.Bezier;
                key.bezier = new float[l.Count];
                for (int i = 0; i < l.Count; i++) key.bezier[i] = (float)(double)l[i];
            }
        }

        static Timeline CurveTimeline(TimelineType type, int target, List<object> arr, string[] fields, float[] defs)
        {
            var tl = new Timeline { type = type, target = target, keys = new Key[arr.Count] };
            for (int i = 0; i < arr.Count; i++)
            {
                var kj = O(arr[i]);
                var k = new Key { time = MiniJson.Num(kj, "time", 0) };
                for (int f = 0; f < fields.Length; f++) k.v[f] = MiniJson.Num(kj, fields[f], defs[f]);
                ReadCurve(kj, k);
                tl.keys[i] = k;
            }
            return tl;
        }

        static Timeline ColorTimeline(TimelineType type, int target, List<object> arr)
        {
            bool alpha = type == TimelineType.RGBA || type == TimelineType.RGBA2;
            bool two = type == TimelineType.RGBA2 || type == TimelineType.RGB2;
            var tl = new Timeline { type = type, target = target, keys = new Key[arr.Count] };
            for (int i = 0; i < arr.Count; i++)
            {
                var kj = O(arr[i]);
                var k = new Key { time = MiniJson.Num(kj, "time", 0) };
                var light = Color4.FromHex(MiniJson.Str(kj, two ? "light" : "color", ""), "ffffffff");
                k.v[0] = light.r; k.v[1] = light.g; k.v[2] = light.b;
                int o = 3;
                if (alpha) { k.v[3] = light.a; o = 4; }
                if (two)
                {
                    var dark = Color4.FromHex(MiniJson.Str(kj, "dark", ""), "000000ff");
                    k.v[o] = dark.r; k.v[o + 1] = dark.g; k.v[o + 2] = dark.b;
                }
                ReadCurve(kj, k);
                tl.keys[i] = k;
            }
            return tl;
        }

        Animation ReadAnimation(string name, Dictionary<string, object> aj)
        {
            var list = new List<Timeline>();
            var bones = O(Get(aj, "bones"));
            if (bones != null)
                foreach (var bkv in bones)
                {
                    int bone = d.FindBone(bkv.Key);
                    if (bone < 0) { d.warnings.Add("Animation " + name + ": unknown bone " + bkv.Key); continue; }
                    foreach (var tkv in O(bkv.Value))
                    {
                        var arr = MiniJson.Arr(tkv.Value);
                        float[] z = { 0, 0 }, one = { 1, 1 };
                        switch (tkv.Key)
                        {
                            case "rotate": list.Add(CurveTimeline(TimelineType.Rotate, bone, arr, new[] { "value" }, z)); break;
                            case "translate": list.Add(CurveTimeline(TimelineType.Translate, bone, arr, new[] { "x", "y" }, z)); break;
                            case "translatex": list.Add(CurveTimeline(TimelineType.TranslateX, bone, arr, new[] { "value" }, z)); break;
                            case "translatey": list.Add(CurveTimeline(TimelineType.TranslateY, bone, arr, new[] { "value" }, z)); break;
                            case "scale": list.Add(CurveTimeline(TimelineType.Scale, bone, arr, new[] { "x", "y" }, one)); break;
                            case "scalex": list.Add(CurveTimeline(TimelineType.ScaleX, bone, arr, new[] { "value" }, one)); break;
                            case "scaley": list.Add(CurveTimeline(TimelineType.ScaleY, bone, arr, new[] { "value" }, one)); break;
                            case "shear": list.Add(CurveTimeline(TimelineType.Shear, bone, arr, new[] { "x", "y" }, z)); break;
                            case "shearx": list.Add(CurveTimeline(TimelineType.ShearX, bone, arr, new[] { "value" }, z)); break;
                            case "sheary": list.Add(CurveTimeline(TimelineType.ShearY, bone, arr, new[] { "value" }, z)); break;
                            case "inherit":
                                {
                                    var tl = new Timeline { type = TimelineType.Inherit, target = bone, keys = new Key[arr.Count] };
                                    for (int i = 0; i < arr.Count; i++)
                                    {
                                        var kj = O(arr[i]);
                                        tl.keys[i] = new Key { time = MiniJson.Num(kj, "time", 0), intValue = (int)InheritFrom(MiniJson.Str(kj, "inherit", "normal")) };
                                    }
                                    list.Add(tl); break;
                                }
                            default: d.warnings.Add("Unsupported bone timeline " + tkv.Key); break;
                        }
                    }
                }
            var slots = O(Get(aj, "slots"));
            if (slots != null)
                foreach (var skv in slots)
                {
                    int slot = d.FindSlot(skv.Key);
                    if (slot < 0) { d.warnings.Add("Animation " + name + ": unknown slot " + skv.Key); continue; }
                    foreach (var tkv in O(skv.Value))
                    {
                        var arr = MiniJson.Arr(tkv.Value);
                        switch (tkv.Key)
                        {
                            case "attachment":
                                {
                                    var tl = new Timeline { type = TimelineType.Attachment, target = slot, keys = new Key[arr.Count] };
                                    for (int i = 0; i < arr.Count; i++)
                                    {
                                        var kj = O(arr[i]);
                                        tl.keys[i] = new Key { time = MiniJson.Num(kj, "time", 0), str = MiniJson.Str(kj, "name", "") };
                                    }
                                    list.Add(tl); break;
                                }
                            case "rgba": list.Add(ColorTimeline(TimelineType.RGBA, slot, arr)); break;
                            case "rgb": list.Add(ColorTimeline(TimelineType.RGB, slot, arr)); break;
                            case "alpha": list.Add(CurveTimeline(TimelineType.Alpha, slot, arr, new[] { "value" }, new float[] { 1 })); break;
                            case "rgba2": list.Add(ColorTimeline(TimelineType.RGBA2, slot, arr)); break;
                            case "rgb2": list.Add(ColorTimeline(TimelineType.RGB2, slot, arr)); break;
                            case "sequence":
                                {
                                    var tl = new Timeline { type = TimelineType.Sequence, target = slot, keys = new Key[arr.Count] };
                                    for (int i = 0; i < arr.Count; i++)
                                    {
                                        var kj = O(arr[i]);
                                        var k = new Key { time = MiniJson.Num(kj, "time", 0), intValue = MiniJson.Int(kj, "index", 0), str = MiniJson.Str(kj, "mode", "hold") };
                                        k.v[0] = MiniJson.Num(kj, "delay", 0);
                                        tl.keys[i] = k;
                                    }
                                    list.Add(tl); break;
                                }
                            default: d.warnings.Add("Unsupported slot timeline " + tkv.Key); break;
                        }
                    }
                }
            var ikm = O(Get(aj, "ik"));
            if (ikm != null)
                foreach (var kv in ikm)
                {
                    int idx = d.ik.FindIndex(c => c.name == kv.Key);
                    if (idx < 0) continue;
                    var arr = MiniJson.Arr(kv.Value);
                    var tl = CurveTimeline(TimelineType.Ik, idx, arr, new[] { "mix", "softness" }, new float[] { 1, 0 });
                    for (int i = 0; i < arr.Count; i++)
                    {
                        var kj = O(arr[i]);
                        tl.keys[i].flag1 = MiniJson.Bool(kj, "bendPositive", true);
                        tl.keys[i].flag2 = MiniJson.Bool(kj, "compress", false);
                        tl.keys[i].flag3 = MiniJson.Bool(kj, "stretch", false);
                    }
                    list.Add(tl);
                }
            var tcm = O(Get(aj, "transform"));
            if (tcm != null)
                foreach (var kv in tcm)
                {
                    int idx = d.transforms.FindIndex(c => c.name == kv.Key);
                    if (idx < 0) continue;
                    var arr = MiniJson.Arr(kv.Value);
                    var tl = CurveTimeline(TimelineType.Transform, idx, arr, new[] { "mixRotate", "mixX", "mixY", "mixScaleX", "mixScaleY", "mixShearY" }, new float[] { 1, 1, 1, 1, 1, 1 });
                    for (int i = 0; i < arr.Count; i++)
                    {
                        var kj = O(arr[i]);
                        if (!kj.ContainsKey("mixY")) tl.keys[i].v[2] = tl.keys[i].v[1];
                        if (!kj.ContainsKey("mixScaleY")) tl.keys[i].v[4] = tl.keys[i].v[3];
                    }
                    list.Add(tl);
                }
            // Deform lives under "attachments" (4.2) or "deform" (4.0/4.1): skin -> slot -> attachment.
            foreach (string dk in new[] { "attachments", "deform" })
            {
                var dm = O(Get(aj, dk));
                if (dm == null) continue;
                foreach (var skin in dm)
                {
                    int si = d.FindSkin(skin.Key);
                    if (si < 0) continue;
                    foreach (var slotKv in O(skin.Value))
                    {
                        int slot = d.FindSlot(slotKv.Key);
                        if (slot < 0) continue;
                        foreach (var attKv in O(slotKv.Value))
                        {
                            object arrObj = attKv.Value;
                            if (dk == "attachments") { var inner = O(attKv.Value); arrObj = Get(inner, "deform"); if (arrObj == null) continue; }
                            var arr = MiniJson.Arr(arrObj);
                            var att = d.skins[si].Get(slot, attKv.Key);
                            if (att == null || arr == null) continue;
                            var geom = att;
                            if (att.type == AttachmentType.LinkedMesh)
                            {
                                int ps = string.IsNullOrEmpty(att.parentSkin) ? 0 : d.FindSkin(att.parentSkin);
                                geom = d.FindAttachment(ps, slot, att.parentMesh);
                                if (geom == null) continue;
                            }
                            int len = geom.DeformLength;
                            var tl = new Timeline { type = TimelineType.Deform, target = slot, skin = skin.Key, attachment = attKv.Key, keys = new Key[arr.Count] };
                            for (int i = 0; i < arr.Count; i++)
                            {
                                var kj = O(arr[i]);
                                var k = new Key { time = MiniJson.Num(kj, "time", 0), deform = new float[len] };
                                float[] v = MiniJson.Floats(kj, "vertices");
                                int off = Math.Max(0, MiniJson.Int(kj, "offset", 0));
                                for (int n = 0; n < v.Length && off + n < len; n++) k.deform[off + n] = v[n];
                                ReadCurve(kj, k);
                                tl.keys[i] = k;
                            }
                            list.Add(tl);
                        }
                    }
                }
            }
            var dor = MiniJson.Arr(Get(aj, "drawOrder"));
            if (dor != null)
            {
                int n = d.slots.Count;
                var tl = new Timeline { type = TimelineType.DrawOrder, keys = new Key[dor.Count] };
                for (int ki = 0; ki < dor.Count; ki++)
                {
                    var kj = O(dor[ki]);
                    var k = new Key { time = MiniJson.Num(kj, "time", 0) };
                    var offs = MiniJson.Arr(Get(kj, "offsets"));
                    if (offs != null)
                    {
                        var order = new int[n]; for (int i = 0; i < n; i++) order[i] = -1;
                        var unchanged = new List<int>();
                        int orig = 0;
                        foreach (var oo in offs)
                        {
                            var oj = O(oo);
                            int si = d.FindSlot(MiniJson.Str(oj, "slot", ""));
                            if (si < 0) continue;
                            while (orig != si && orig < n) unchanged.Add(orig++);
                            int pos = orig + MiniJson.Int(oj, "offset", 0);
                            if (pos >= 0 && pos < n) order[pos] = orig;
                            orig++;
                        }
                        while (orig < n) unchanged.Add(orig++);
                        for (int i = n - 1; i >= 0; i--)
                            if (order[i] == -1 && unchanged.Count > 0) { order[i] = unchanged[unchanged.Count - 1]; unchanged.RemoveAt(unchanged.Count - 1); }
                        if (Array.IndexOf(order, -1) >= 0) order = null;
                        k.drawOrder = order;
                    }
                    tl.keys[ki] = k;
                }
                list.Add(tl);
            }
            var evs = MiniJson.Arr(Get(aj, "events"));
            if (evs != null)
            {
                var tl = new Timeline { type = TimelineType.Event, keys = new Key[evs.Count] };
                for (int i = 0; i < evs.Count; i++)
                {
                    var kj = O(evs[i]);
                    var k = new Key { time = MiniJson.Num(kj, "time", 0), str = MiniJson.Str(kj, "name", "") };
                    if (kj.ContainsKey("int")) { k.flag1 = true; k.intValue = MiniJson.Int(kj, "int", 0); }
                    if (kj.ContainsKey("float")) { k.flag2 = true; k.v[0] = MiniJson.Num(kj, "float", 0); }
                    if (kj.ContainsKey("string")) { k.flag3 = true; k.str2 = MiniJson.Str(kj, "string", ""); }
                    k.volume = MiniJson.Num(kj, "volume", 1); k.balance = MiniJson.Num(kj, "balance", 0);
                    tl.keys[i] = k;
                }
                list.Add(tl);
            }
            var anim = new Animation { name = name, timelines = list.ToArray() };
            foreach (var tl in anim.timelines)
            {
                Array.Sort(tl.keys, (a, b) => a.time.CompareTo(b.time)); // Array.Sort is unstable; keys are normally sorted already
                foreach (var k in tl.keys) if (k.time > anim.duration) anim.duration = k.time;
            }
            return anim;
        }
    }
}
