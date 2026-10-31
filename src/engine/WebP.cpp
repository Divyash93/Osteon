#include "WebP.h"
#include <algorithm>
#include <cstring>
#include <queue>

namespace osteon {

namespace {

struct BitWriter {
    std::vector<uint8_t> out;
    uint64_t acc = 0;
    int nbits = 0;
    void put(uint32_t value, int bits) { // LSB first
        acc |= (uint64_t)(value & (bits >= 32 ? 0xFFFFFFFFu : ((1u << bits) - 1))) << nbits;
        nbits += bits;
        while (nbits >= 8) { out.push_back((uint8_t)acc); acc >>= 8; nbits -= 8; }
    }
    void finish() { if (nbits > 0) { out.push_back((uint8_t)acc); acc = 0; nbits = 0; } }
};

struct Code { std::vector<uint8_t> len; std::vector<uint16_t> bits; int used = 0; int single = -1; };

// Huffman code lengths for `freq`, none longer than `limit` (frequencies are flattened until they fit).
std::vector<uint8_t> huffmanLengths(std::vector<uint32_t> freq, int limit) {
    const int n = (int)freq.size();
    std::vector<uint8_t> len(n, 0);
    for (;;) {
        struct Node { uint64_t w; int left, right; };
        std::vector<Node> nodes;
        using Item = std::pair<uint64_t, int>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
        for (int i = 0; i < n; i++) if (freq[i]) { nodes.push_back({freq[i], -1, -i - 1}); pq.push({freq[i], (int)nodes.size() - 1}); }
        std::fill(len.begin(), len.end(), 0);
        if (nodes.size() == 1) { len[-nodes[0].right - 1] = 1; return len; }
        if (nodes.empty()) return len;
        while (pq.size() > 1) {
            Item a = pq.top(); pq.pop();
            Item b = pq.top(); pq.pop();
            nodes.push_back({a.first + b.first, a.second, b.second});
            pq.push({a.first + b.first, (int)nodes.size() - 1});
        }
        // Depth of every leaf.
        int maxLen = 0;
        std::vector<std::pair<int, int>> stack = {{pq.top().second, 0}};
        while (!stack.empty()) {
            auto [idx, depth] = stack.back();
            stack.pop_back();
            const Node& nd = nodes[idx];
            if (nd.left == -1 && nd.right < 0) { len[-nd.right - 1] = (uint8_t)depth; maxLen = std::max(maxLen, depth); continue; }
            stack.push_back({nd.left, depth + 1});
            stack.push_back({nd.right, depth + 1});
        }
        if (maxLen <= limit) return len;
        for (auto& f : freq) if (f) f = (f + 1) / 2 + 1; // flatten and retry
    }
}

// Canonical codes, stored bit-reversed so they can be written LSB first.
void assignCodes(Code& c) {
    const int n = (int)c.len.size();
    int maxLen = 0;
    for (uint8_t l : c.len) maxLen = std::max<int>(maxLen, l);
    std::vector<int> count(maxLen + 2, 0), next(maxLen + 2, 0);
    for (uint8_t l : c.len) if (l) count[l]++;
    int code = 0;
    for (int b = 1; b <= maxLen; b++) { code = (code + count[b - 1]) << 1; next[b] = code; }
    c.bits.assign(n, 0);
    for (int i = 0; i < n; i++) {
        int l = c.len[i];
        if (!l) continue;
        int v = next[l]++, r = 0;
        for (int b = 0; b < l; b++) r |= ((v >> b) & 1) << (l - 1 - b);
        c.bits[i] = (uint16_t)r;
    }
}

Code makeCode(const std::vector<uint32_t>& freq, int limit) {
    Code c;
    for (uint32_t f : freq) if (f) c.used++;
    for (size_t i = 0; i < freq.size(); i++) if (freq[i]) { c.single = (int)i; break; }
    c.len = c.used >= 2 ? huffmanLengths(freq, limit) : std::vector<uint8_t>(freq.size(), 0);
    assignCodes(c);
    return c;
}

// Writes the prefix code description. alphabet = number of symbols (280 for green, 256, 40 ...).
void writeCode(BitWriter& bw, const Code& c, int alphabet) {
    if (c.used <= 1) { // simple code with one symbol (which then takes no bits per pixel)
        int sym = c.single < 0 ? 0 : c.single;
        bw.put(1, 1);                 // simple
        bw.put(0, 1);                 // one symbol
        if (sym < 2) { bw.put(0, 1); bw.put(sym, 1); } else { bw.put(1, 1); bw.put(sym, 8); }
        return;
    }
    bw.put(0, 1); // normal code
    // Code lengths are sent through a second Huffman code over the symbols 0..15.
    std::vector<uint32_t> lf(19, 0);
    for (int i = 0; i < alphabet; i++) lf[c.len[i]]++;
    std::vector<uint8_t> ll = huffmanLengths(lf, 7);
    int lused = 0;
    for (uint8_t l : ll) if (l) lused++;
    if (lused == 1) { // a lone length symbol still needs a 1-bit code
        for (int i = 0; i < 19; i++) if (ll[i]) ll[i] = 1;
    }
    Code lc;
    lc.len = ll;
    assignCodes(lc);
    static const int order[19] = {17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    int num = 19;
    while (num > 4 && ll[order[num - 1]] == 0) num--;
    bw.put(num - 4, 4);
    for (int i = 0; i < num; i++) bw.put(ll[order[i]], 3);
    bw.put(0, 1); // max_symbol = the whole alphabet
    for (int i = 0; i < alphabet; i++) {
        if (lused == 1) continue; // a single length symbol costs no bits
        bw.put(lc.bits[c.len[i]], ll[c.len[i]]);
    }
}

void putSymbol(BitWriter& bw, const Code& c, int sym) {
    if (c.used <= 1) return;
    bw.put(c.bits[sym], c.len[sym]);
}

// The VP8L bitstream of one image.
std::vector<uint8_t> vp8lStream(int w, int h, const uint8_t* rgba, bool& hasAlpha) {
    hasAlpha = false;
    std::vector<uint8_t> a(w * h), r(w * h), g(w * h), b(w * h);
    for (int i = 0; i < w * h; i++) {
        const uint8_t* p = rgba + (size_t)i * 4;
        g[i] = p[1];
        r[i] = (uint8_t)(p[0] - p[1]); // subtract-green transform
        b[i] = (uint8_t)(p[2] - p[1]);
        a[i] = p[3];
        if (p[3] != 255) hasAlpha = true;
    }
    std::vector<uint32_t> fg(280, 0), fr(256, 0), fb(256, 0), fa(256, 0);
    for (int i = 0; i < w * h; i++) { fg[g[i]]++; fr[r[i]]++; fb[b[i]]++; fa[a[i]]++; }
    Code cg = makeCode(fg, 15), cr = makeCode(fr, 15), cb = makeCode(fb, 15), ca = makeCode(fa, 15);
    BitWriter bw;
    bw.put(0x2f, 8);                    // signature
    bw.put(w - 1, 14); bw.put(h - 1, 14);
    bw.put(hasAlpha ? 1 : 0, 1);
    bw.put(0, 3);                       // version
    bw.put(1, 1); bw.put(2, 2);         // transform present: subtract green
    bw.put(0, 1);                       // no more transforms
    bw.put(0, 1);                       // no colour cache
    bw.put(0, 1);                       // no meta prefix image
    writeCode(bw, cg, 280);
    writeCode(bw, cr, 256);
    writeCode(bw, cb, 256);
    writeCode(bw, ca, 256);
    Code cd; cd.used = 0; cd.len.assign(40, 0); // distance codes are never used
    writeCode(bw, cd, 40);
    for (int i = 0; i < w * h; i++) {
        putSymbol(bw, cg, g[i]);
        putSymbol(bw, cr, r[i]);
        putSymbol(bw, cb, b[i]);
        putSymbol(bw, ca, a[i]);
    }
    bw.finish();
    return bw.out;
}

void le32(std::vector<uint8_t>& o, uint32_t v) { for (int i = 0; i < 4; i++) o.push_back((uint8_t)(v >> (8 * i))); }
void le24(std::vector<uint8_t>& o, uint32_t v) { for (int i = 0; i < 3; i++) o.push_back((uint8_t)(v >> (8 * i))); }
void fourcc(std::vector<uint8_t>& o, const char* s) { o.insert(o.end(), s, s + 4); }
void chunk(std::vector<uint8_t>& o, const char* id, const std::vector<uint8_t>& payload) {
    fourcc(o, id);
    le32(o, (uint32_t)payload.size());
    o.insert(o.end(), payload.begin(), payload.end());
    if (payload.size() & 1) o.push_back(0);
}
bool sizeOk(int w, int h) { return w >= 1 && h >= 1 && w <= 16384 && h <= 16384; }

} // namespace

std::vector<uint8_t> encodeWebpLossless(int w, int h, const uint8_t* rgba) {
    if (!sizeOk(w, h) || !rgba) return {};
    bool alpha = false;
    std::vector<uint8_t> stream = vp8lStream(w, h, rgba, alpha);
    std::vector<uint8_t> out;
    fourcc(out, "RIFF"); le32(out, 0); fourcc(out, "WEBP");
    chunk(out, "VP8L", stream);
    uint32_t size = (uint32_t)out.size() - 8;
    for (int i = 0; i < 4; i++) out[4 + i] = (uint8_t)(size >> (8 * i));
    return out;
}

std::vector<uint8_t> encodeWebpAnimation(int w, int h, const std::vector<std::vector<uint8_t>>& frames, int frameMs, int loopCount) {
    if (!sizeOk(w, h) || frames.empty()) return {};
    for (auto& f : frames) if (f.size() < (size_t)w * h * 4) return {};
    std::vector<std::vector<uint8_t>> streams;
    bool anyAlpha = false;
    for (auto& f : frames) {
        bool alpha = false;
        streams.push_back(vp8lStream(w, h, f.data(), alpha));
        anyAlpha |= alpha;
    }
    std::vector<uint8_t> out;
    fourcc(out, "RIFF"); le32(out, 0); fourcc(out, "WEBP");
    std::vector<uint8_t> vp8x;
    vp8x.push_back((uint8_t)(0x02 | (anyAlpha ? 0x10 : 0))); // animation (+ alpha) flags
    le24(vp8x, 0);
    le24(vp8x, (uint32_t)(w - 1)); le24(vp8x, (uint32_t)(h - 1));
    chunk(out, "VP8X", vp8x);
    std::vector<uint8_t> anim = {0xFF, 0xFF, 0xFF, 0xFF, (uint8_t)loopCount, (uint8_t)(loopCount >> 8)}; // background colour, loop count
    chunk(out, "ANIM", anim);
    for (auto& s : streams) {
        std::vector<uint8_t> anmf;
        le24(anmf, 0); le24(anmf, 0);                          // frame x, y (in units of 2 pixels)
        le24(anmf, (uint32_t)(w - 1)); le24(anmf, (uint32_t)(h - 1));
        le24(anmf, (uint32_t)std::max(1, frameMs));
        anmf.push_back(0x01);                                  // do not blend with the previous frame, no disposal
        chunk(anmf, "VP8L", s);
        chunk(out, "ANMF", anmf);
    }
    uint32_t size = (uint32_t)out.size() - 8;
    for (int i = 0; i < 4; i++) out[4 + i] = (uint8_t)(size >> (8 * i));
    return out;
}

} // namespace osteon
