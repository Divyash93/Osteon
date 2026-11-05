// Checks the C# runtime core against poses dumped from the C++ engine (osteon_golden).
//   CoreTest.exe <skeleton.json> <golden.json>
using System;
using System.Collections.Generic;
using System.IO;
using Osteon2D;

static class CoreTest
{
    static int failures, checks;

    static void Near(string what, float expect, float got)
    {
        checks++;
        float tol = 2e-3f + 1e-4f * Math.Abs(expect);
        if (Math.Abs(expect - got) > tol || float.IsNaN(got))
        {
            failures++;
            if (failures < 25) Console.WriteLine("  MISMATCH " + what + ": expected " + expect + " got " + got);
        }
    }

    static int Main(string[] args)
    {
        var data = SkeletonData.FromJson(File.ReadAllText(args[0]));
        foreach (var w in data.warnings) Console.WriteLine("warning: " + w);
        var golden = MiniJson.Obj(MiniJson.Parse(File.ReadAllText(args[1])));
        var samples = MiniJson.Arr(golden["samples"]);
        foreach (var so in samples)
        {
            var sj = MiniJson.Obj(so);
            string label = (string)sj["name"];
            var p = label.Split(':');
            var sk = new Skeleton(data);
            if (p[0] == "apply")
            {
                if (p.Length > 3) sk.skin = Math.Max(0, data.FindSkin(p[3]));
                sk.UpdateCache();
                sk.SetToSetupPose();
                AnimationApply.Apply(data.animations[data.FindAnimation(p[1])], sk, -1, float.Parse(p[2], System.Globalization.CultureInfo.InvariantCulture), false, null, 1, MixBlend.Setup);
                sk.UpdateWorldTransform();
            }
            else
            {
                if (p.Length > 5) sk.skin = Math.Max(0, data.FindSkin(p[5]));
                sk.UpdateCache();
                var st = new AnimationState(data);
                st.SetAnimation(0, data.FindAnimation(p[1]), true);
                st.Update(float.Parse(p[3], System.Globalization.CultureInfo.InvariantCulture));
                sk.SetToSetupPose(); st.Apply(sk);
                st.SetAnimation(0, data.FindAnimation(p[2]), true);
                st.Update(float.Parse(p[4], System.Globalization.CultureInfo.InvariantCulture));
                sk.SetToSetupPose(); st.Apply(sk);
                sk.UpdateWorldTransform();
            }
            int before = failures;
            var bones = MiniJson.Arr(sj["bones"]);
            for (int i = 0; i < bones.Count; i++)
            {
                var m = MiniJson.Arr(bones[i]);
                var b = sk.bones[i];
                float[] got = { b.a, b.b, b.c, b.d, b.worldX, b.worldY };
                for (int k = 0; k < 6; k++) Near(label + " bone " + data.bones[i].name + "[" + k + "]", (float)(double)m[k], got[k]);
            }
            var order = MiniJson.Arr(sj["drawOrder"]);
            for (int i = 0; i < order.Count; i++) { checks++; if ((int)(double)order[i] != sk.drawOrder[i]) { failures++; Console.WriteLine("  MISMATCH drawOrder " + label + " at " + i); } }
            var slots = MiniJson.Arr(sj["slots"]);
            foreach (var slo in slots)
            {
                var slj = MiniJson.Obj(slo);
                var slot = sk.slots[MiniJson.Int(slj, "i", 0)];
                var col = MiniJson.Arr(slj["color"]);
                float[] gc = { slot.color.r, slot.color.g, slot.color.b, slot.color.a };
                for (int k = 0; k < 4; k++) Near(label + " slot " + slot.index + " color" + k, (float)(double)col[k], gc[k]);
                if (slj.ContainsKey("verts"))
                {
                    var att = sk.AttachmentFor(slot);
                    var v = MiniJson.Floats(slj, "verts");
                    var got = new float[v.Length];
                    checks++;
                    if (att == null || !sk.WorldVertices(slot, att, got)) { failures++; Console.WriteLine("  MISMATCH no vertices " + label + " slot " + slot.index); continue; }
                    for (int k = 0; k < v.Length; k++) Near(label + " slot " + slot.index + " vert" + k, v[k], got[k]);
                }
            }
            Console.WriteLine((failures == before ? "ok   " : "FAIL ") + label);
        }
        // Mesh builder smoke test: a synthetic atlas that has every attachment, then validate the output geometry.
        var sb = new System.Text.StringBuilder("page.png\nsize:512,512\nfilter:Linear,Linear\n");
        foreach (var skin in data.skins)
            foreach (var m in skin.attachments.Values)
                foreach (var kv in m) { var a = kv.Value; sb.Append(string.IsNullOrEmpty(a.path) ? a.name : a.path).Append("\nbounds:0,0,16,16\n"); }
        var mb = new MeshBuilder(Atlas.Parse(sb.ToString()));
        var pose = new Skeleton(data);
        pose.UpdateWorldTransform();
        mb.Build(pose);
        checks++;
        bool bad = mb.vertexCount == 0 || mb.indexCount == 0 || mb.indexCount % 3 != 0;
        for (int i = 0; i < mb.indexCount; i++) if (mb.indices[i] < 0 || mb.indices[i] >= mb.vertexCount) bad = true;
        for (int i = 0; i < mb.vertexCount * 2; i++) if (float.IsNaN(mb.positions[i]) || float.IsNaN(mb.uvs[i])) bad = true;
        int sum = 0; foreach (var sm in mb.subMeshes) sum += sm.indexCount;
        if (sum != mb.indexCount) bad = true;
        if (bad) { failures++; Console.WriteLine("  MISMATCH mesh builder output invalid"); }
        else Console.WriteLine("ok   mesh builder: " + mb.vertexCount + " vertices, " + mb.indexCount / 3 + " triangles, " + mb.subMeshes.Count + " sub meshes");
        Console.WriteLine(checks + " checks, " + failures + " mismatches");
        return failures == 0 ? 0 : 1;
    }
}
