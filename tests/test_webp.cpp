// Lossless WebP writer: container structure (pixel-exact decoding was checked against libwebp through Pillow, see PROGRESS.md).
#include "Test.h"
#include "WebP.h"
#include <cstring>

using namespace osteon;

static uint32_t rd32(const std::vector<uint8_t>& d, size_t at) { return d[at] | (d[at + 1] << 8) | (d[at + 2] << 16) | ((uint32_t)d[at + 3] << 24); }
static bool is(const std::vector<uint8_t>& d, size_t at, const char* cc) { return at + 4 <= d.size() && std::memcmp(&d[at], cc, 4) == 0; }

TEST(webp_still_has_riff_vp8l_structure_and_size_fields) {
    const int w = 37, h = 21;
    std::vector<uint8_t> px((size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) { px[i * 4] = (uint8_t)i; px[i * 4 + 1] = (uint8_t)(i * 3); px[i * 4 + 2] = 90; px[i * 4 + 3] = i % 5 ? 255 : 128; }
    auto out = encodeWebpLossless(w, h, px.data());
    CHECK(out.size() > 30);
    CHECK(is(out, 0, "RIFF") && is(out, 8, "WEBP") && is(out, 12, "VP8L"));
    CHECK(rd32(out, 4) == out.size() - 8);
    CHECK(rd32(out, 16) + 20 <= out.size() && out[20] == 0x2f);       // VP8L signature
    uint32_t bits = out[21] | (out[22] << 8) | (out[23] << 16) | ((uint32_t)out[24] << 24);
    CHECK((int)(bits & 0x3FFF) == w - 1 && (int)((bits >> 14) & 0x3FFF) == h - 1);
    CHECK(((bits >> 28) & 1) == 1);                                      // alpha is used
    std::vector<uint8_t> opaque(px);
    for (int i = 0; i < w * h; i++) opaque[i * 4 + 3] = 255;
    auto o2 = encodeWebpLossless(w, h, opaque.data());
    CHECK(((o2[24] >> 4) & 1) == 0);                                      // no alpha flag when fully opaque
}

TEST(webp_animation_has_one_frame_chunk_per_frame) {
    const int w = 16, h = 8, n = 4;
    std::vector<std::vector<uint8_t>> frames(n, std::vector<uint8_t>((size_t)w * h * 4, 200));
    auto out = encodeWebpAnimation(w, h, frames, 80, 0);
    CHECK(is(out, 0, "RIFF") && is(out, 8, "WEBP") && is(out, 12, "VP8X") && rd32(out, 4) == out.size() - 8);
    CHECK((out[20] & 0x02) != 0);                                        // animation flag
    int count = 0;
    for (size_t at = 12; at + 8 <= out.size();) {
        if (is(out, at, "ANMF")) count++;
        uint32_t size = rd32(out, at + 4);
        at += 8 + size + (size & 1);
    }
    CHECK(count == n);
}

TEST(webp_rejects_bad_input) {
    std::vector<uint8_t> px(16, 0);
    CHECK(encodeWebpLossless(0, 4, px.data()).empty());
    CHECK(encodeWebpLossless(4, 4, nullptr).empty());
    CHECK(encodeWebpLossless(20000, 1, px.data()).empty());
    CHECK(encodeWebpAnimation(4, 4, {}, 50, 0).empty());
    CHECK(encodeWebpAnimation(4, 4, {px}, 50, 0).empty());               // frame buffer too small for 4x4
}
