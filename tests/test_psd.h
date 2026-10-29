// Test-only PSD writer: builds small 8-bit RGB PSD files in memory (raw and RLE channels, groups, hidden layers).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace psdtest {

struct Layer {
    std::string name;        // Pascal name (ASCII)
    std::u16string uname;    // optional 'luni' name
    int left = 0, top = 0, right = 0, bottom = 0;
    bool visible = true;
    uint8_t opacity = 255;
    bool rle = false;
    int forceComp = -1;      // overwrite the compression word (to fake unsupported data)
    bool withAlpha = true;
    bool withMask = false;   // adds a user-mask channel (-2) the reader must skip
    int divider = 0;         // lsct type
    std::vector<uint8_t> rgba;
};

struct Bytes {
    std::vector<uint8_t> b;
    void u8(int v) { b.push_back((uint8_t)v); }
    void u16(int v) { u8(v >> 8); u8(v); }
    void u32(uint32_t v) { u16((int)(v >> 16)); u16((int)(v & 0xFFFF)); }
    void tag(const char* t) { for (int i = 0; i < 4; i++) u8(t[i]); }
    void add(const std::vector<uint8_t>& o) { b.insert(b.end(), o.begin(), o.end()); }
};

inline std::vector<uint8_t> packBits(const uint8_t* row, int w) {
    std::vector<uint8_t> o;
    int i = 0;
    while (i < w) {
        int run = 1;
        while (i + run < w && row[i + run] == row[i] && run < 128) run++;
        if (run >= 3) { o.push_back((uint8_t)(1 - run)); o.push_back(row[i]); i += run; continue; }
        int start = i, n = 0;
        while (i < w && n < 128) {
            int r = 1;
            while (i + r < w && row[i + r] == row[i] && r < 3) r++;
            if (r >= 3) break;
            i++; n++;
        }
        o.push_back((uint8_t)(n - 1));
        for (int k = 0; k < n; k++) o.push_back(row[start + k]);
    }
    return o;
}

// One channel's data including its compression word.
inline std::vector<uint8_t> channelData(const std::vector<uint8_t>& plane, int w, int h, bool rle) {
    Bytes o;
    if (!rle) { o.u16(0); o.add(plane); return o.b; }
    o.u16(1);
    std::vector<std::vector<uint8_t>> rows;
    for (int y = 0; y < h; y++) rows.push_back(packBits(&plane[(size_t)y * w], w));
    for (auto& r : rows) o.u16((int)r.size());
    for (auto& r : rows) o.add(r);
    return o.b;
}

inline std::vector<uint8_t> writePsd(int width, int height, const std::vector<Layer>& layers, int depth = 8, int mode = 3, int version = 1) {
    Bytes out;
    out.tag("8BPS"); out.u16(version);
    for (int i = 0; i < 6; i++) out.u8(0);
    out.u16(3); out.u32(height); out.u32(width); out.u16(depth); out.u16(mode);
    out.u32(0); out.u32(0); // colour mode data, image resources
    bool psb = version == 2;

    Bytes info;
    info.u16((int)layers.size());
    std::vector<std::vector<std::vector<uint8_t>>> chData; // per layer, per channel
    for (auto& L : layers) {
        int w = L.right - L.left, h = L.bottom - L.top;
        std::vector<int> ids = {0, 1, 2};
        if (L.withAlpha) ids.insert(ids.begin(), -1);
        if (L.withMask) ids.push_back(-2);
        std::vector<std::vector<uint8_t>> chans;
        for (int id : ids) {
            std::vector<uint8_t> plane;
            if (L.divider == 0) {
                if (id == -2) plane.assign(6, 0x7F);
                else {
                    int c = id == -1 ? 3 : id;
                    for (int i = 0; i < w * h; i++) plane.push_back(L.rgba[(size_t)i * 4 + c]);
                }
                chans.push_back(id == -2 ? channelData(plane, 6, 1, false) : channelData(plane, w, h, L.rle));
                if (L.forceComp >= 0 && id != -2) chans.back()[1] = (uint8_t)L.forceComp;
            } else {
                chans.push_back({0, 0});
            }
        }
        chData.push_back(chans);

        info.u32(L.top); info.u32(L.left); info.u32(L.bottom); info.u32(L.right);
        info.u16((int)ids.size());
        for (size_t i = 0; i < ids.size(); i++) {
            info.u16(ids[i] & 0xFFFF);
            if (psb) info.u32(0);
            info.u32((uint32_t)chans[i].size());
        }
        info.tag("8BIM"); info.tag("norm");
        info.u8(L.opacity); info.u8(0); info.u8(L.visible ? 0 : 2); info.u8(0);
        Bytes extra;
        extra.u32(0); // layer mask data
        extra.u32(0); // blending ranges
        int nl = (int)L.name.size();
        extra.u8(nl);
        for (char c : L.name) extra.u8(c);
        while ((1 + nl) % 4 != 0) { extra.u8(0); nl++; }
        if (!L.uname.empty()) {
            Bytes blk;
            blk.u32((uint32_t)L.uname.size());
            for (char16_t c : L.uname) blk.u16(c);
            blk.u16(0);
            while (blk.b.size() % 4) blk.u8(0);
            extra.tag("8BIM"); extra.tag("luni"); extra.u32((uint32_t)blk.b.size()); extra.add(blk.b);
        }
        if (L.divider) { extra.tag("8BIM"); extra.tag("lsct"); extra.u32(4); extra.u32(L.divider); }
        info.u32((uint32_t)extra.b.size());
        info.add(extra.b);
    }
    for (auto& chans : chData)
        for (auto& c : chans) info.add(c);
    if (info.b.size() % 2) info.u8(0);

    Bytes misc;
    if (psb) { misc.u32(0); misc.u32((uint32_t)info.b.size()); } else misc.u32((uint32_t)info.b.size());
    // misc length covers the layer-info length field and data
    Bytes full;
    if (psb) { full.u32(0); full.u32((uint32_t)misc.b.size() + (uint32_t)info.b.size()); }
    else full.u32((uint32_t)misc.b.size() + (uint32_t)info.b.size());
    full.add(misc.b);
    full.add(info.b);
    out.add(full.b);
    return out.b;
}


inline std::vector<uint8_t> solid(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::vector<uint8_t> px;
    for (int i = 0; i < w * h; i++) { px.push_back(r); px.push_back(g); px.push_back(b); px.push_back(a); }
    return px;
}

// Gradient so RLE has both runs and literals.
inline std::vector<uint8_t> pattern(int w, int h, int seed) {
    std::vector<uint8_t> px;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            px.push_back((uint8_t)(x < 3 ? 200 : x * 10 + seed));
            px.push_back((uint8_t)(y * 7 + seed));
            px.push_back((uint8_t)(x == y ? 255 : 5));
            px.push_back((uint8_t)(x + y < 20 ? 255 : 128));
        }
    return px;
}

inline std::vector<uint8_t> makeFixture(std::vector<uint8_t>* armPixels = nullptr, std::vector<uint8_t>* topPixels = nullptr) {
    using psdtest::Layer;
    std::vector<Layer> ls;
    Layer bg; bg.name = "Background"; bg.left = 0; bg.top = 0; bg.right = 8; bg.bottom = 6; bg.rgba = pattern(8, 6, 1);
    ls.push_back(bg);
    Layer div; div.name = "</Layer group>"; div.divider = 3;
    ls.push_back(div);
    Layer arm; arm.name = "Arm"; arm.uname = u"Arm \u00e9"; arm.left = 2; arm.top = 1; arm.right = 12; arm.bottom = 9;
    arm.rle = true; arm.rgba = pattern(10, 8, 40);
    if (armPixels) *armPixels = arm.rgba;
    ls.push_back(arm);
    Layer hid; hid.name = "Hidden"; hid.left = 0; hid.top = 0; hid.right = 2; hid.bottom = 2; hid.visible = false; hid.rgba = solid(2, 2, 1, 2, 3, 4);
    ls.push_back(hid);
    Layer grp; grp.name = "Group 1"; grp.divider = 1;
    ls.push_back(grp);
    Layer top; top.name = "Top"; top.left = -1; top.top = 3; top.right = 3; top.bottom = 5; top.opacity = 128;
    top.rle = true; top.withMask = true; top.rgba = pattern(4, 2, 90);
    if (topPixels) *topPixels = top.rgba;
    ls.push_back(top);
    Layer empty; empty.name = "Empty"; empty.left = empty.right = empty.top = empty.bottom = 0;
    ls.push_back(empty);
    return psdtest::writePsd(16, 12, ls);
}

} // namespace psdtest
