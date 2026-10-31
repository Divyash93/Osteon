// The start-up splash: pack parsing, image resizing and timing.
#include "../Test.h"
#include "Splash.h"
#include "Util.h"
#include <objbase.h>

using namespace osteon;

static std::vector<uint8_t> readPak() {
    FILE* f = _wfopen(L"assets/splash/splash.pak", L"rb");
    std::vector<uint8_t> out;
    if (!f) return out;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize((size_t)n);
    out.resize(fread(out.data(), 1, out.size(), f));
    fclose(f);
    return out;
}

TEST(splash_pack_parses_and_every_image_decodes) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::vector<uint8_t> bytes = readPak();
    CHECK(bytes.size() > 100000);
    SplashPak p = parseSplashPak(bytes.data(), bytes.size());
    CHECK(p.valid);
    CHECK(p.frames.size() > 20 && p.fps > 0 && p.cardPerPixel > 0);
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    CHECK(decodeImageMemory(p.card.data, p.card.size, w, h, px));
    CHECK(w == p.cardW && h == p.cardH);
    // The GITHUB link lies inside the card.
    CHECK(p.link[0] >= 0 && p.link[1] >= 0 && p.link[2] <= p.cardW && p.link[3] <= p.cardH && p.link[0] < p.link[2] && p.link[1] < p.link[3]);
    int fw0 = 0, fh0 = 0;
    for (size_t i = 0; i < p.frames.size(); i++) {
        int fw = 0, fh = 0;
        std::vector<uint8_t> fp;
        CHECK(decodeImageMemory(p.frames[i].data, p.frames[i].size, fw, fh, fp));
        if (i == 0) { fw0 = fw; fh0 = fh; }
        CHECK(fw == fw0 && fh == fh0);                    // all frames share one size
    }
    // The logo frames sit on the card (top-left region), not off it.
    CHECK(p.placeX > -50 && p.placeY > -50 && p.placeX + fw0 * p.cardPerPixel < p.cardW);
    // The first frame is nearly empty (the logo has not appeared yet), the last one is full.
    auto opaque = [&](const SplashPak::Blob& b) {
        int a = 0, c = 0; std::vector<uint8_t> q;
        decodeImageMemory(b.data, b.size, a, c, q);
        size_t n = 0; for (size_t i = 3; i < q.size(); i += 4) if (q[i] > 128) n++;
        return n;
    };
    CHECK(opaque(p.frames.front()) < opaque(p.frames.back()) / 4);
}

TEST(splash_pack_rejects_malformed_data) {
    std::vector<uint8_t> bytes = readPak();
    CHECK(!parseSplashPak(nullptr, 0).valid);
    CHECK(!parseSplashPak(bytes.data(), 10).valid);
    std::vector<uint8_t> bad = bytes;
    bad[0] = 'X';
    CHECK(!parseSplashPak(bad.data(), bad.size()).valid);          // wrong magic
    CHECK(!parseSplashPak(bytes.data(), bytes.size() / 2).valid);  // truncated: blobs run past the end
    std::vector<uint8_t> noFrames = bytes;
    noFrames[16] = noFrames[17] = noFrames[18] = noFrames[19] = 0; // zero frames
    CHECK(!parseSplashPak(noFrames.data(), noFrames.size()).valid);
}

TEST(splash_resize_averages_and_keeps_transparent_edges_clean) {
    // 4x4 with a left half opaque red and a right half fully transparent (colour data garbage there).
    std::vector<uint8_t> src(4 * 4 * 4);
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
        uint8_t* p = &src[(y * 4 + x) * 4];
        if (x < 2) { p[0] = 255; p[1] = 0; p[2] = 0; p[3] = 255; } else { p[0] = 0; p[1] = 255; p[2] = 255; p[3] = 0; }
    }
    auto out = resizeRgbaArea(src, 4, 4, 2, 2);
    CHECK(out.size() == 2 * 2 * 4);
    CHECK(out[0] == 255 && out[1] == 0 && out[3] == 255);          // opaque red stays red
    CHECK(out[4 + 3] == 0);                                         // fully transparent stays transparent
    auto half = resizeRgbaArea(src, 4, 4, 1, 1);
    CHECK(half[0] == 255 && half[1] == 0 && half[2] == 0);          // the transparent half does not tint the colour
    CHECK(half[3] > 120 && half[3] < 135);                          // and halves the alpha
    CHECK(resizeRgbaArea(src, 4, 4, 0, 2).empty());
    // A flat colour is unchanged by any resize.
    std::vector<uint8_t> flat(10 * 7 * 4, 0);
    for (size_t i = 0; i < flat.size(); i += 4) { flat[i] = 20; flat[i + 1] = 90; flat[i + 2] = 200; flat[i + 3] = 255; }
    auto f2 = resizeRgbaArea(flat, 10, 7, 3, 5);
    for (size_t i = 0; i < f2.size(); i += 4) CHECK(f2[i] == 20 && f2[i + 1] == 90 && f2[i + 2] == 200 && f2[i + 3] == 255);
}

TEST(splash_clock_fades_in_holds_and_ends_by_itself_after_four_seconds) {
    SplashClock c;
    CHECK(c.alpha(0) == 0 && c.alpha(c.fadeIn) == 1);
    CHECK(c.alpha(2.0f) == 1);
    CHECK(!c.finished(3.9f) && c.finished(4.0f));
    CHECK(c.alpha(3.9f) < 0.5f && c.alpha(3.9f) > 0);                // fading out at the end
    // A click while it is fully shown: quick fade, then finished.
    SplashClock d;
    d.dismiss(1.0f);
    CHECK(d.alpha(1.0f) > 0.99f && d.alpha(1.1f) < 0.7f && d.alpha(1.25f) <= 0.01f);
    CHECK(!d.finished(1.2f) && d.finished(1.25f));
    d.dismiss(2.0f);                                                  // a second click changes nothing
    CHECK(d.dismissedAt == 1.0f);
    // A click during the last fade does not extend it.
    SplashClock e;
    e.dismiss(3.9f);
    CHECK(e.finished(4.2f));
}
