// AVI / MOV writers and the image encoders behind them.
#include "../Test.h"
#include "Gfx.h"
#include "VideoWriter.h"
#include <cstring>
#include <objbase.h>

using namespace osteon;

static uint32_t le32(const std::vector<uint8_t>& d, size_t at) { return d[at] | (d[at + 1] << 8) | (d[at + 2] << 16) | ((uint32_t)d[at + 3] << 24); }
static uint32_t be32(const std::vector<uint8_t>& d, size_t at) { return ((uint32_t)d[at] << 24) | (d[at + 1] << 16) | (d[at + 2] << 8) | d[at + 3]; }
static bool is(const std::vector<uint8_t>& d, size_t at, const char* cc) { return at + 4 <= d.size() && std::memcmp(&d[at], cc, 4) == 0; }

static std::vector<std::vector<uint8_t>> testFrames(int w, int h, int n) {
    std::vector<std::vector<uint8_t>> frames;
    for (int i = 0; i < n; i++) {
        std::vector<uint8_t> f((size_t)w * h * 4);
        for (int p = 0; p < w * h; p++) { f[p * 4] = (uint8_t)(i * 40); f[p * 4 + 1] = (uint8_t)(p % 251); f[p * 4 + 2] = 200; f[p * 4 + 3] = 255; }
        frames.push_back(std::move(f));
    }
    return frames;
}

// Finds an atom by walking the tree; returns its payload start/end.
static bool findAtom(const std::vector<uint8_t>& d, size_t from, size_t to, const char* type, size_t& payload, size_t& end) {
    for (size_t at = from; at + 8 <= to;) {
        uint32_t size = be32(d, at);
        if (size < 8 || at + size > to) return false;
        if (is(d, at + 4, type)) { payload = at + 8; end = at + size; return true; }
        at += size;
    }
    return false;
}

TEST(avi_uncompressed_has_valid_structure_and_bottom_up_bgr_rows) {
    const int w = 5, h = 3;   // 5 px wide: rows are padded (15 bytes -> 16)
    auto frames = testFrames(w, h, 4);
    std::vector<uint8_t> avi;
    CHECK(buildAvi(w, h, 12, frames, nullptr, avi));
    CHECK(is(avi, 0, "RIFF") && is(avi, 8, "AVI ") && le32(avi, 4) == avi.size() - 8);
    CHECK(is(avi, 12, "LIST") && is(avi, 20, "hdrl") && is(avi, 24, "avih"));
    CHECK(le32(avi, 32) == 1000000 / 12);          // microseconds per frame
    CHECK(le32(avi, 48) == 4);                     // total frames
    CHECK(le32(avi, 64) == (uint32_t)w && le32(avi, 68) == (uint32_t)h);
    size_t movi = 0;
    for (size_t i = 12; i + 12 < avi.size(); i++) if (is(avi, i, "LIST") && is(avi, i + 8, "movi")) { movi = i; break; }
    CHECK(movi > 0);
    size_t first = movi + 12;
    CHECK(is(avi, first, "00db") && le32(avi, first + 4) == 16u * h);
    // The first stored row is the bottom image row, in BGR order.
    const uint8_t* row = &avi[first + 8];
    const uint8_t* src = &frames[0][(size_t)(h - 1) * w * 4];
    CHECK(row[0] == src[2] && row[1] == src[1] && row[2] == src[0]);
    // The index has one entry per frame and points at the chunks.
    size_t idx = 0;
    for (size_t i = movi; i + 8 < avi.size(); i++) if (is(avi, i, "idx1")) { idx = i; break; }
    CHECK(idx > 0 && le32(avi, idx + 4) == 16u * 4);
    for (int f = 0; f < 4; f++) {
        size_t e = idx + 8 + 16 * f;
        size_t chunk = movi + 8 + le32(avi, e + 8);   // offsets count from the 'movi' fourcc
        CHECK(is(avi, chunk, "00db") && le32(avi, e + 12) == 16u * h);
    }
}

TEST(avi_mjpeg_and_mov_png_wrap_real_encoded_frames) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const int w = 24, h = 16;
    auto frames = testFrames(w, h, 3);
    FrameEncoder jpeg = [](const uint8_t* px, int fw, int fh, std::vector<uint8_t>& out) { return encodeImageBytes(ImageFormat::Jpeg, fw, fh, px, 80, out); };
    FrameEncoder png = [](const uint8_t* px, int fw, int fh, std::vector<uint8_t>& out) { return encodeImageBytes(ImageFormat::Png, fw, fh, px, 0, out); };
    std::vector<uint8_t> avi;
    CHECK(buildAvi(w, h, 25, frames, jpeg, avi));
    size_t movi = 0;
    for (size_t i = 12; i + 12 < avi.size(); i++) if (is(avi, i, "LIST") && is(avi, i + 8, "movi")) { movi = i; break; }
    CHECK(movi > 0 && is(avi, movi + 12, "00dc"));
    CHECK(avi[movi + 20] == 0xFF && avi[movi + 21] == 0xD8);   // JPEG start of image
    bool handler = false;
    for (size_t i = 0; i + 8 < movi; i++) if (is(avi, i, "vids") && is(avi, i + 4, "MJPG")) handler = true;
    CHECK(handler);

    std::vector<uint8_t> mov;
    CHECK(buildMov(w, h, 25, frames, png, "png ", true, mov));
    size_t p, e;
    CHECK(is(mov, 4, "ftyp") && is(mov, 8, "qt  "));
    size_t at = be32(mov, 0);
    CHECK(is(mov, at + 4, "mdat"));
    size_t moov = at + be32(mov, at);
    CHECK(is(mov, moov + 4, "moov") && moov + be32(mov, moov) == mov.size());
    // Walk moov > trak > mdia > minf > stbl.
    size_t from = moov + 8, to = mov.size();
    for (const char* type : {"trak", "mdia", "minf", "stbl"}) { CHECK(findAtom(mov, from, to, type, p, e)); from = p; to = e; }
    size_t stsz, stszEnd, stco, stcoEnd, stsd, stsdEnd, stts, sttsEnd;
    CHECK(findAtom(mov, from, to, "stsz", stsz, stszEnd) && findAtom(mov, from, to, "stco", stco, stcoEnd));
    CHECK(findAtom(mov, from, to, "stsd", stsd, stsdEnd) && findAtom(mov, from, to, "stts", stts, sttsEnd));
    CHECK(be32(mov, stsz + 8) == 3);                      // three samples
    CHECK(is(mov, stsd + 12, "png "));
    CHECK(be32(mov, stts + 8) == 3 && be32(mov, stts + 12) == 1000);   // 3 frames of one 1/fps tick
    size_t offset = be32(mov, stco + 8);
    for (int i = 0; i < 3; i++) {
        CHECK(mov[offset] == 0x89 && mov[offset + 1] == 'P' && mov[offset + 2] == 'N' && mov[offset + 3] == 'G');   // PNG signature
        offset += be32(mov, stsz + 12 + 4 * i);
    }
    CHECK(offset == moov);                                 // the samples fill the mdat exactly
}

TEST(video_writers_reject_unusable_input_and_the_encoders_round_trip) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::vector<uint8_t> out;
    std::vector<std::vector<uint8_t>> none;
    CHECK(!buildAvi(10, 10, 12, none, nullptr, out));
    auto frames = testFrames(4, 4, 1);
    CHECK(!buildAvi(0, 4, 12, frames, nullptr, out));
    CHECK(!buildAvi(4, 4, 0, frames, nullptr, out));
    FrameEncoder failing = [](const uint8_t*, int, int, std::vector<uint8_t>&) { return false; };
    CHECK(!buildAvi(4, 4, 12, frames, failing, out));
    CHECK(!buildMov(4, 4, 12, frames, nullptr, "png ", true, out));
    // A JPEG and a PNG decode back to the size they were written at.
    std::vector<uint8_t> png, jpg;
    CHECK(encodeImageBytes(ImageFormat::Png, 4, 4, frames[0].data(), 0, png) && png.size() > 8);
    CHECK(encodeImageBytes(ImageFormat::Jpeg, 4, 4, frames[0].data(), 90, jpg) && jpg.size() > 4 && jpg[0] == 0xFF && jpg[1] == 0xD8);
}

TEST(jpeg_file_export_writes_a_readable_image_of_the_right_size) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto frames = testFrames(37, 21, 1);
    std::string path = "osteon_jpeg_test.jpg";
    CHECK(saveJpegFile(path, 37, 21, frames[0].data(), 85));
    int w = 0, h = 0;
    CHECK(loadImageFileSize(path, w, h));
    CHECK(w == 37 && h == 21);
    std::vector<uint8_t> px;
    CHECK(loadImageFile(path, w, h, px) && px.size() == 37u * 21 * 4);
    std::remove(path.c_str());
    // Lower quality gives a smaller file.
    std::vector<uint8_t> hi, lo;
    CHECK(encodeImageBytes(ImageFormat::Jpeg, 37, 21, frames[0].data(), 95, hi) && encodeImageBytes(ImageFormat::Jpeg, 37, 21, frames[0].data(), 20, lo));
    CHECK(lo.size() < hi.size());
}
