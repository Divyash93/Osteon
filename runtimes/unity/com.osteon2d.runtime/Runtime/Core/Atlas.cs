// Reader for the libGDX / Spine style .atlas text files that Osteon2D's texture packer writes. Engine independent.
using System;
using System.Collections.Generic;
using System.Globalization;

namespace Osteon2D
{
    public class AtlasPage { public string file; public int w, h; public bool pma; public bool filterNearest; }

    public class AtlasRegion
    {
        public string name; public int page;
        public int x, y, w, h;              // on the page; w,h are the unrotated size
        public bool rotated;                // stored rotated 90 degrees clockwise (h x w on the page)
        public int origW, origH, offX, offY; // trimmed regions
        public int index = -1;

        /// <summary>Maps a point of the ORIGINAL image (u,v in 0..1, v pointing down) to page UVs (v pointing down).</summary>
        public void PageUV(AtlasPage page, float u, float v, out float pu, out float pv)
        {
            float ix = u * origW - offX;                         // pixel inside the packed region image
            float iy = v * origH - (origH - offY - h);
            float px, py;
            if (rotated) { px = x + (h - iy); py = y + ix; }
            else { px = x + ix; py = y + iy; }
            pu = px / page.w; pv = py / page.h;
        }
    }

    public class Atlas
    {
        public readonly List<AtlasPage> pages = new List<AtlasPage>();
        public readonly List<AtlasRegion> regions = new List<AtlasRegion>();
        readonly Dictionary<string, AtlasRegion> byName = new Dictionary<string, AtlasRegion>();

        public AtlasRegion Find(string name) { AtlasRegion r; return byName.TryGetValue(name, out r) ? r : null; }

        static int[] Ints(string v)
        {
            var p = v.Split(',');
            var r = new int[p.Length];
            for (int i = 0; i < p.Length; i++) int.TryParse(p[i].Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out r[i]);
            return r;
        }

        public static Atlas Parse(string text)
        {
            var atlas = new Atlas();
            bool needPage = true;
            AtlasRegion cur = null;
            foreach (var raw in text.Split('\n'))
            {
                string l = raw.Trim();
                if (l.Length == 0) { needPage = true; cur = null; continue; }
                int c = l.IndexOf(':');
                if (c >= 0)
                {
                    string k = l.Substring(0, c).Trim(), v = l.Substring(c + 1).Trim();
                    if (cur != null)
                    {
                        var n = Ints(v);
                        if (k == "bounds" && n.Length >= 4) { cur.x = n[0]; cur.y = n[1]; cur.w = n[2]; cur.h = n[3]; }
                        else if (k == "xy" && n.Length >= 2) { cur.x = n[0]; cur.y = n[1]; }
                        else if (k == "size" && n.Length >= 2) { cur.w = n[0]; cur.h = n[1]; }
                        else if (k == "offsets" && n.Length >= 4) { cur.offX = n[0]; cur.offY = n[1]; cur.origW = n[2]; cur.origH = n[3]; }
                        else if (k == "orig" && n.Length >= 2) { cur.origW = n[0]; cur.origH = n[1]; }
                        else if (k == "offset" && n.Length >= 2) { cur.offX = n[0]; cur.offY = n[1]; }
                        else if (k == "rotate") cur.rotated = v == "true" || v == "90";
                        else if (k == "index") cur.index = n.Length > 0 ? n[0] : -1;
                    }
                    else if (atlas.pages.Count > 0)
                    {
                        var page = atlas.pages[atlas.pages.Count - 1];
                        var n = Ints(v);
                        if (k == "size" && n.Length >= 2) { page.w = n[0]; page.h = n[1]; }
                        else if (k == "pma") page.pma = v == "true";
                        else if (k == "filter") page.filterNearest = v.StartsWith("Nearest");
                    }
                    continue;
                }
                if (needPage) { atlas.pages.Add(new AtlasPage { file = l }); needPage = false; cur = null; }
                else if (atlas.pages.Count > 0)
                {
                    cur = new AtlasRegion { name = l, page = atlas.pages.Count - 1 };
                    atlas.regions.Add(cur);
                }
            }
            foreach (var r in atlas.regions)
            {
                if (r.origW <= 0) r.origW = r.w;
                if (r.origH <= 0) r.origH = r.h;
                string key = r.index >= 0 ? r.name + "_" + r.index : r.name;
                atlas.byName[key] = r;
                if (!atlas.byName.ContainsKey(r.name)) atlas.byName[r.name] = r;
            }
            return atlas;
        }
    }
}
