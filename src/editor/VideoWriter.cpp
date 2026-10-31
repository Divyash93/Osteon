#include "VideoWriter.h"
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace osteon {

namespace {

struct Buf {
    std::vector<uint8_t> d;
    size_t size() const { return d.size(); }
    void u8(uint32_t v) { d.push_back((uint8_t)v); }
    void le16(uint32_t v) { u8(v); u8(v >> 8); }
    void le32(uint32_t v) { u8(v); u8(v >> 8); u8(v >> 16); u8(v >> 24); }
    void be16(uint32_t v) { u8(v >> 8); u8(v); }
    void be32(uint32_t v) { u8(v >> 24); u8(v >> 16); u8(v >> 8); u8(v); }
    void fourcc(const char* s) { for (int i = 0; i < 4; i++) u8((uint8_t)s[i]); }
    void bytes(const uint8_t* p, size_t n) { d.insert(d.end(), p, p + n); }
    void zeros(size_t n) { d.insert(d.end(), n, 0); }
    void setLe32(size_t at, uint32_t v) { for (int i = 0; i < 4; i++) d[at + i] = (uint8_t)(v >> (8 * i)); }
    void setBe32(size_t at, uint32_t v) { for (int i = 0; i < 4; i++) d[at + i] = (uint8_t)(v >> (24 - 8 * i)); }
};

// A QuickTime atom: size, type, then the payload written by `body`.
template <class F> void atom(Buf& b, const char* type, F body) {
    size_t at = b.size();
    b.be32(0);
    b.fourcc(type);
    body();
    b.setBe32(at, (uint32_t)(b.size() - at));
}

bool framesOk(int w, int h, int fps, const std::vector<std::vector<uint8_t>>& frames) {
    if (w <= 0 || h <= 0 || fps <= 0 || frames.empty()) return false;
    for (auto& f : frames) if (f.size() < (size_t)w * h * 4) return false;
    return true;
}

} // namespace

bool buildAvi(int w, int h, int fps, const std::vector<std::vector<uint8_t>>& frames, const FrameEncoder& encoder, std::vector<uint8_t>& out) {
    if (!framesOk(w, h, fps, frames)) return false;
    // Frame payloads.
    std::vector<std::vector<uint8_t>> chunks;
    const int stride = (w * 3 + 3) & ~3;
    for (auto& f : frames) {
        std::vector<uint8_t> data;
        if (encoder) {
            if (!encoder(f.data(), w, h, data) || data.empty()) return false;
        } else { // bottom-up BGR rows, each padded to 4 bytes
            data.assign((size_t)stride * h, 0);
            for (int y = 0; y < h; y++) {
                const uint8_t* src = &f[(size_t)(h - 1 - y) * w * 4];
                uint8_t* dst = &data[(size_t)y * stride];
                for (int x = 0; x < w; x++) { dst[x * 3] = src[x * 4 + 2]; dst[x * 3 + 1] = src[x * 4 + 1]; dst[x * 3 + 2] = src[x * 4]; }
            }
        }
        chunks.push_back(std::move(data));
    }
    size_t maxChunk = 0, totalBytes = 0;
    for (auto& c : chunks) { maxChunk = std::max(maxChunk, c.size()); totalBytes += c.size(); }
    const uint32_t n = (uint32_t)frames.size();
    const char* handler = encoder ? "MJPG" : "DIB ";

    Buf b;
    b.fourcc("RIFF"); b.le32(0); b.fourcc("AVI ");
    // hdrl
    b.fourcc("LIST"); size_t hdrlSize = b.size(); b.le32(0); b.fourcc("hdrl");
    b.fourcc("avih"); b.le32(56);
    b.le32(1000000 / (uint32_t)fps);                       // microseconds per frame
    b.le32((uint32_t)((double)totalBytes / n * fps));      // max bytes per second
    b.le32(0); b.le32(0x10);                               // padding granularity, flags: has index
    b.le32(n); b.le32(0); b.le32(1);                       // total frames, initial frames, streams
    b.le32((uint32_t)maxChunk); b.le32((uint32_t)w); b.le32((uint32_t)h); b.zeros(16);
    b.fourcc("LIST"); size_t strlSize = b.size(); b.le32(0); b.fourcc("strl");
    b.fourcc("strh"); b.le32(56);
    b.fourcc("vids"); b.fourcc(handler);
    b.le32(0); b.le16(0); b.le16(0); b.le32(0);            // flags, priority, language, initial frames
    b.le32(1); b.le32((uint32_t)fps); b.le32(0); b.le32(n); // scale, rate, start, length
    b.le32((uint32_t)maxChunk); b.le32(0xFFFFFFFFu); b.le32(0);
    b.le16(0); b.le16(0); b.le16((uint32_t)w); b.le16((uint32_t)h);
    b.fourcc("strf"); b.le32(40);
    b.le32(40); b.le32((uint32_t)w); b.le32((uint32_t)h); b.le16(1); b.le16(24);
    if (encoder) b.fourcc("MJPG"); else b.le32(0);
    b.le32((uint32_t)maxChunk); b.zeros(16);
    b.setLe32(strlSize, (uint32_t)(b.size() - strlSize - 4));
    b.setLe32(hdrlSize, (uint32_t)(b.size() - hdrlSize - 4));
    // movi
    b.fourcc("LIST"); size_t moviSize = b.size(); b.le32(0); b.fourcc("movi");
    const size_t moviStart = b.size() - 4; // index offsets count from the 'movi' fourcc
    std::vector<uint32_t> offsets, sizes;
    for (auto& c : chunks) {
        offsets.push_back((uint32_t)(b.size() - moviStart));
        sizes.push_back((uint32_t)c.size());
        b.fourcc(encoder ? "00dc" : "00db"); b.le32((uint32_t)c.size());
        b.bytes(c.data(), c.size());
        if (c.size() & 1) b.u8(0);
    }
    b.setLe32(moviSize, (uint32_t)(b.size() - moviSize - 4));
    // idx1
    b.fourcc("idx1"); b.le32(16 * n);
    for (uint32_t i = 0; i < n; i++) { b.fourcc(encoder ? "00dc" : "00db"); b.le32(0x10); b.le32(offsets[i]); b.le32(sizes[i]); }
    b.setLe32(4, (uint32_t)(b.size() - 8));
    out = std::move(b.d);
    return true;
}

bool buildMov(int w, int h, int fps, const std::vector<std::vector<uint8_t>>& frames, const FrameEncoder& encoder, const char codec[5],
              bool hasAlpha, std::vector<uint8_t>& out) {
    if (!framesOk(w, h, fps, frames) || !encoder) return false;
    std::vector<std::vector<uint8_t>> samples;
    for (auto& f : frames) {
        std::vector<uint8_t> data;
        if (!encoder(f.data(), w, h, data) || data.empty()) return false;
        samples.push_back(std::move(data));
    }
    const uint32_t n = (uint32_t)samples.size();
    const uint32_t timescale = (uint32_t)fps * 1000, frameDur = 1000;
    const uint32_t duration = n * frameDur;
    Buf b;
    atom(b, "ftyp", [&] { b.fourcc("qt  "); b.be32(0x20050300); b.fourcc("qt  "); });
    size_t firstSample = 0;
    atom(b, "mdat", [&] {
        firstSample = b.size();
        for (auto& s : samples) b.bytes(s.data(), s.size());
    });
    atom(b, "moov", [&] {
        atom(b, "mvhd", [&] {
            b.be32(0); b.be32(0); b.be32(0);              // version/flags, creation, modification
            b.be32(timescale); b.be32(duration);
            b.be32(0x00010000); b.be16(0x0100); b.zeros(10); // rate, volume, reserved
            const uint32_t m[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
            for (uint32_t v : m) b.be32(v);
            b.zeros(24);                                    // preview, poster, selection, current time
            b.be32(2);                                      // next track id
        });
        atom(b, "trak", [&] {
            atom(b, "tkhd", [&] {
                b.be32(0x0000000F); b.be32(0); b.be32(0);   // version/flags (enabled, in movie, in preview), times
                b.be32(1); b.be32(0); b.be32(duration);     // track id, reserved, duration
                b.zeros(8); b.be16(0); b.be16(0); b.be16(0); b.be16(0); // reserved, layer, alternate group, volume, reserved
                const uint32_t m[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
                for (uint32_t v : m) b.be32(v);
                b.be32((uint32_t)w << 16); b.be32((uint32_t)h << 16);
            });
            atom(b, "mdia", [&] {
                atom(b, "mdhd", [&] { b.be32(0); b.be32(0); b.be32(0); b.be32(timescale); b.be32(duration); b.be16(0x55C4); b.be16(0); });
                atom(b, "hdlr", [&] { b.be32(0); b.fourcc("mhlr"); b.fourcc("vide"); b.be32(0); b.be32(0); b.be32(0); b.u8(5); b.bytes((const uint8_t*)"Video", 5); });
                atom(b, "minf", [&] {
                    atom(b, "vmhd", [&] { b.be32(1); b.be16(0); b.be16(0); b.be16(0); b.be16(0); });
                    atom(b, "hdlr", [&] { b.be32(0); b.fourcc("dhlr"); b.fourcc("alis"); b.be32(0); b.be32(0); b.be32(0); b.u8(5); b.bytes((const uint8_t*)"Alias", 5); });
                    atom(b, "dinf", [&] { atom(b, "dref", [&] { b.be32(0); b.be32(1); atom(b, "alis", [&] { b.be32(1); }); }); });
                    atom(b, "stbl", [&] {
                        atom(b, "stsd", [&] {
                            b.be32(0); b.be32(1);
                            atom(b, codec, [&] {
                                b.zeros(6); b.be16(1);                    // reserved, data reference index
                                b.be16(0); b.be16(0); b.be32(0); b.be32(0); b.be32(0x200); // version, revision, vendor, temporal, spatial quality
                                b.be16((uint32_t)w); b.be16((uint32_t)h);
                                b.be32(0x00480000); b.be32(0x00480000);   // 72 dpi
                                b.be32(0); b.be16(1);                     // data size, frame count
                                const char* name = std::strncmp(codec, "png ", 4) == 0 ? "PNG" : "Photo - JPEG";
                                size_t nl = std::strlen(name);
                                b.u8((uint32_t)nl); b.bytes((const uint8_t*)name, nl); b.zeros(31 - nl);
                                b.be16(hasAlpha ? 32 : 24); b.be16(0xFFFF); // depth, colour table id
                            });
                        });
                        atom(b, "stts", [&] { b.be32(0); b.be32(1); b.be32(n); b.be32(frameDur); });
                        atom(b, "stsc", [&] { b.be32(0); b.be32(1); b.be32(1); b.be32(n); b.be32(1); }); // every sample in one chunk
                        atom(b, "stsz", [&] { b.be32(0); b.be32(0); b.be32(n); for (auto& s : samples) b.be32((uint32_t)s.size()); });
                        atom(b, "stco", [&] { b.be32(0); b.be32(1); b.be32((uint32_t)firstSample); });
                    });
                });
            });
        });
    });
    out = std::move(b.d);
    return true;
}

static bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::wstring wide;
    int n = MultiByteToWideChar(65001, 0, path.c_str(), -1, nullptr, 0);
    wide.assign(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(65001, 0, path.c_str(), -1, wide.data(), n);
    FILE* f = _wfopen(wide.c_str(), L"wb");
    if (!f) return false;
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = std::fclose(f) == 0 && ok;
    return ok;
}

bool writeAvi(const std::string& path, int w, int h, int fps, const std::vector<std::vector<uint8_t>>& frames, const FrameEncoder& encoder) {
    std::vector<uint8_t> bytes;
    return buildAvi(w, h, fps, frames, encoder, bytes) && writeFile(path, bytes);
}

bool writeMov(const std::string& path, int w, int h, int fps, const std::vector<std::vector<uint8_t>>& frames, const FrameEncoder& encoder,
              const char codec[5], bool hasAlpha) {
    std::vector<uint8_t> bytes;
    return buildMov(w, h, fps, frames, encoder, codec, hasAlpha, bytes) && writeFile(path, bytes);
}

} // namespace osteon
