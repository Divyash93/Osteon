#include "Psd.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace osteon {
namespace {

struct Reader {
    const std::vector<uint8_t>& d;
    size_t pos = 0;
    explicit Reader(const std::vector<uint8_t>& data) : d(data) {}
    void need(uint64_t n) const {
        if (n > d.size() || pos > d.size() - n) throw std::runtime_error("unexpected end of file (damaged PSD?)");
    }
    void skip(uint64_t n) { need(n); pos += (size_t)n; }
    uint8_t u8() { need(1); return d[pos++]; }
    uint16_t u16() { need(2); uint16_t v = (uint16_t)(d[pos] << 8 | d[pos + 1]); pos += 2; return v; }
    uint32_t u32() { need(4); uint32_t v = (uint32_t)d[pos] << 24 | (uint32_t)d[pos + 1] << 16 | (uint32_t)d[pos + 2] << 8 | d[pos + 3]; pos += 4; return v; }
    uint64_t u64() { uint64_t hi = u32(); return hi << 32 | u32(); }
    int16_t i16() { return (int16_t)u16(); }
    int32_t i32() { return (int32_t)u32(); }
    std::string tag() { need(4); std::string s((const char*)&d[pos], 4); pos += 4; return s; }
};

void appendUtf8(std::string& s, uint32_t c) {
    if (c < 0x80) s += (char)c;
    else if (c < 0x800) { s += (char)(0xC0 | c >> 6); s += (char)(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { s += (char)(0xE0 | c >> 12); s += (char)(0x80 | ((c >> 6) & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
    else { s += (char)(0xF0 | c >> 18); s += (char)(0x80 | ((c >> 12) & 0x3F)); s += (char)(0x80 | ((c >> 6) & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
}

std::string utf16beToUtf8(const uint8_t* p, size_t units) {
    std::string out;
    for (size_t i = 0; i < units; i++) {
        uint32_t c = p[i * 2] << 8 | p[i * 2 + 1];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < units) {
            uint32_t lo = p[i * 2 + 2] << 8 | p[i * 2 + 3];
            if (lo >= 0xDC00 && lo < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); i++; }
        }
        if (c == 0) continue;
        appendUtf8(out, c);
    }
    return out;
}

struct ChannelInfo { int id; uint64_t length; };

struct Record {
    int top = 0, left = 0, bottom = 0, right = 0;
    std::vector<ChannelInfo> channels;
    uint8_t opacity = 255, flags = 0;
    std::string name;
    int divider = 0; // lsct type: 0 normal, 1/2 folder, 3 bounding divider
    std::vector<uint8_t> rgba;
};

// Decodes one channel (its 2-byte compression word is part of `len`) into out (w*h bytes).
void decodeChannel(Reader& r, uint64_t len, int w, int h, bool psb, std::vector<uint8_t>& out) {
    r.need(len);
    size_t end = r.pos + (size_t)len;
    out.assign((size_t)w * h, 0);
    if (len < 2) return;
    uint16_t comp = r.u16();
    if (comp == 0) {
        size_t n = (size_t)w * h;
        if (len - 2 < n) throw std::runtime_error("raw channel data is truncated");
        std::memcpy(out.data(), &r.d[r.pos], n);
    } else if (comp == 1) {
        std::vector<uint64_t> counts(h);
        for (int y = 0; y < h; y++) counts[y] = psb ? r.u32() : r.u16();
        for (int y = 0; y < h; y++) {
            if (counts[y] > end - r.pos) throw std::runtime_error("RLE row exceeds channel data");
            size_t rowEnd = r.pos + (size_t)counts[y];
            uint8_t* row = &out[(size_t)y * w];
            int x = 0;
            while (x < w && r.pos < rowEnd) {
                int n = (int8_t)r.u8();
                if (n >= 0) {
                    int c = n + 1;
                    r.need(c);
                    for (int i = 0; i < c && x < w; i++) row[x++] = r.d[r.pos + i];
                    r.pos += c;
                } else if (n != -128) {
                    int c = 1 - n;
                    uint8_t v = r.u8();
                    for (int i = 0; i < c && x < w; i++) row[x++] = v;
                }
            }
            r.pos = rowEnd;
        }
    } else {
        throw std::runtime_error(std::string(comp == 2 || comp == 3 ? "ZIP-compressed" : "unknown-compression") +
                                 " layer data is not supported; re-save the PSD with Maximize Compatibility or from an app that uses RLE");
    }
    r.pos = end;
}

} // namespace

bool readPsdMemory(const std::vector<uint8_t>& data, PsdDocument& out, std::string& err) {
    out = PsdDocument();
    try {
        Reader r(data);
        if (r.tag() != "8BPS") throw std::runtime_error("not a Photoshop file (missing 8BPS signature)");
        int version = r.u16();
        if (version != 1 && version != 2) throw std::runtime_error("unsupported PSD version");
        bool psb = version == 2;
        r.skip(6);
        r.u16(); // channel count
        uint32_t height = r.u32(), width = r.u32();
        int depth = r.u16(), mode = r.u16();
        if (depth != 8) throw std::runtime_error(std::to_string(depth) + "-bit PSD is not supported; convert to 8 bits/channel (Image > Mode > 8 Bits/Channel)");
        if (mode != 3) {
            const char* names[] = {"Bitmap", "Grayscale", "Indexed", "RGB", "CMYK", "", "", "Multichannel", "Duotone", "Lab"};
            std::string m = mode >= 0 && mode <= 9 && *names[mode] ? names[mode] : "Unknown";
            throw std::runtime_error(m + " colour mode is not supported; convert to RGB (Image > Mode > RGB Color)");
        }
        out.width = (int)width;
        out.height = (int)height;
        r.skip(r.u32()); // colour mode data
        r.skip(r.u32()); // image resources
        uint64_t miscLen = psb ? r.u64() : r.u32();
        if (miscLen == 0) return true;
        uint64_t infoLen = psb ? r.u64() : r.u32();
        if (infoLen == 0) return true;
        r.need(infoLen);
        int count = r.i16();
        if (count < 0) count = -count;

        std::vector<Record> recs(count);
        for (auto& rec : recs) {
            rec.top = r.i32(); rec.left = r.i32(); rec.bottom = r.i32(); rec.right = r.i32();
            int nch = r.u16();
            if (nch > 64) throw std::runtime_error("damaged layer record");
            for (int c = 0; c < nch; c++) {
                ChannelInfo ci;
                ci.id = r.i16();
                ci.length = psb ? r.u64() : r.u32();
                rec.channels.push_back(ci);
            }
            if (r.tag() != "8BIM") throw std::runtime_error("damaged layer record (bad blend signature)");
            r.skip(4); // blend mode key
            rec.opacity = r.u8();
            r.u8();    // clipping
            rec.flags = r.u8();
            r.u8();
            uint32_t extraLen = r.u32();
            r.need(extraLen);
            size_t extraEnd = r.pos + extraLen;
            r.skip(r.u32()); // layer mask
            r.skip(r.u32()); // blending ranges
            int nameLen = r.u8();
            r.need(nameLen);
            rec.name.assign((const char*)&r.d[r.pos], nameLen);
            int pad = (4 - (1 + nameLen) % 4) % 4;
            r.skip((uint64_t)nameLen + pad);
            while (r.pos + 12 <= extraEnd) {
                std::string sig = r.tag();
                if (sig != "8BIM" && sig != "8B64") break;
                std::string key = r.tag();
                static const char* big[] = {"LMsk", "Lr16", "Lr32", "Layr", "Mt16", "Mt32", "Mtrn", "Alph", "FMsk", "lnk2", "FEid", "FXid", "PxSD"};
                bool wide = false;
                if (psb) for (auto b : big) wide |= key == b;
                uint64_t len = wide ? r.u64() : r.u32();
                r.need(len);
                size_t blockEnd = r.pos + (size_t)len;
                if (key == "luni" && len >= 4) {
                    uint32_t units = r.u32();
                    if ((uint64_t)units * 2 + 4 <= len) rec.name = utf16beToUtf8(&r.d[r.pos], units);
                } else if (key == "lsct" && len >= 4) {
                    rec.divider = (int)r.u32();
                }
                r.pos = blockEnd;
            }
            r.pos = extraEnd;
        }

        // Channel image data, in record order.
        for (auto& rec : recs) {
            int w = rec.right - rec.left, h = rec.bottom - rec.top;
            bool pixels = rec.divider == 0 && w > 0 && h > 0;
            if (pixels && (int64_t)w * h > ((int64_t)1 << 28)) throw std::runtime_error("layer is too large");
            std::vector<uint8_t> planes[4]; // R, G, B, A
            bool have[4] = {false, false, false, false};
            for (auto& ch : rec.channels) {
                int slot = ch.id == -1 ? 3 : (ch.id >= 0 && ch.id <= 2 ? ch.id : -1);
                if (!pixels || slot < 0) { r.skip(ch.length); continue; } // includes user masks (-2)
                decodeChannel(r, ch.length, w, h, psb, planes[slot]);
                have[slot] = true;
            }
            if (!pixels || !have[0] || !have[1] || !have[2]) continue;
            size_t n = (size_t)w * h;
            rec.rgba.resize(n * 4);
            for (size_t i = 0; i < n; i++) {
                rec.rgba[i * 4] = planes[0][i];
                rec.rgba[i * 4 + 1] = planes[1][i];
                rec.rgba[i * 4 + 2] = planes[2][i];
                rec.rgba[i * 4 + 3] = have[3] ? planes[3][i] : 255;
            }
        }

        // Group paths: walk top to bottom; a folder record opens a group, its bounding divider closes it.
        std::vector<std::string> stack;
        std::vector<std::vector<std::string>> paths(recs.size());
        for (int i = (int)recs.size() - 1; i >= 0; i--) {
            if (recs[i].divider == 1 || recs[i].divider == 2) { paths[i] = stack; stack.push_back(recs[i].name); }
            else if (recs[i].divider == 3) { if (!stack.empty()) stack.pop_back(); }
            else paths[i] = stack;
        }
        for (size_t i = 0; i < recs.size(); i++) {
            Record& rec = recs[i];
            if (rec.divider != 0 || rec.rgba.empty()) continue;
            PsdLayer L;
            L.name = rec.name;
            L.left = rec.left; L.top = rec.top; L.right = rec.right; L.bottom = rec.bottom;
            L.visible = !(rec.flags & 2);
            L.opacity = rec.opacity / 255.0f;
            L.groupPath = paths[i];
            L.rgba = std::move(rec.rgba);
            out.layers.push_back(std::move(L));
        }
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        out = PsdDocument();
        return false;
    }
}

bool readPsd(const std::string& path, PsdDocument& out, std::string& err) {
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f) { err = "Cannot read " + path; return false; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return readPsdMemory(data, out, err);
}

} // namespace osteon
