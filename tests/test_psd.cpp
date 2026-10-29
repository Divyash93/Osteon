// PSD reader: layers, bounds, visibility, groups, raw + RLE pixels, unsupported files.
#include "Test.h"
#include "Psd.h"
#include "test_psd.h"

using namespace osteon;
using psdtest::solid;
using psdtest::pattern;
using psdtest::makeFixture;

TEST(psd_reads_layers_groups_and_pixels) {
    std::vector<uint8_t> armPx, topPx, bgPx = pattern(8, 6, 1);
    auto bytes = makeFixture(&armPx, &topPx);
    PsdDocument doc;
    std::string err;
    CHECK(readPsdMemory(bytes, doc, err));
    CHECK(err.empty());
    CHECK(doc.width == 16 && doc.height == 12);
    CHECK(doc.layers.size() == 4); // dividers and the empty layer are dropped
    if (doc.layers.size() != 4) return;

    auto& bg = doc.layers[0];
    CHECK(bg.name == "Background");
    CHECK(bg.left == 0 && bg.top == 0 && bg.right == 8 && bg.bottom == 6);
    CHECK(bg.visible && bg.groupPath.empty());
    CHECK_NEAR(bg.opacity, 1.0, 1e-6);
    CHECK(bg.rgba == bgPx); // raw channels

    auto& arm = doc.layers[1];
    CHECK(arm.name == "Arm \xC3\xA9"); // from the Unicode name block
    CHECK(arm.left == 2 && arm.top == 1 && arm.right == 12 && arm.bottom == 9);
    CHECK(arm.groupPath.size() == 1 && arm.groupPath[0] == "Group 1");
    CHECK(arm.rgba == armPx); // RLE channels, exact

    auto& hid = doc.layers[2];
    CHECK(hid.name == "Hidden");
    CHECK(!hid.visible);
    CHECK(hid.groupPath.size() == 1);
    CHECK(hid.rgba.size() == 16);
    CHECK(hid.rgba[0] == 1 && hid.rgba[1] == 2 && hid.rgba[2] == 3 && hid.rgba[3] == 4);

    auto& top = doc.layers[3];
    CHECK(top.name == "Top");
    CHECK(top.left == -1 && top.right == 3);
    CHECK(top.groupPath.empty());
    CHECK_NEAR(top.opacity, 128 / 255.0, 1e-6);
    CHECK(top.rgba == topPx); // RLE with a skipped user-mask channel
}

TEST(psd_layer_without_alpha_is_opaque) {
    psdtest::Layer l; l.name = "NoAlpha"; l.right = 2; l.bottom = 2; l.withAlpha = false; l.rgba = solid(2, 2, 9, 8, 7, 0);
    auto bytes = psdtest::writePsd(4, 4, {l});
    PsdDocument doc; std::string err;
    CHECK(readPsdMemory(bytes, doc, err));
    CHECK(doc.layers.size() == 1);
    if (doc.layers.empty()) return;
    CHECK(doc.layers[0].rgba[0] == 9 && doc.layers[0].rgba[3] == 255);
}

TEST(psd_large_format_lengths) {
    psdtest::Layer l; l.name = "Big"; l.right = 3; l.bottom = 2; l.rgba = solid(3, 2, 10, 20, 30, 40);
    auto bytes = psdtest::writePsd(4, 4, {l}, 8, 3, 2);
    PsdDocument doc; std::string err;
    CHECK(readPsdMemory(bytes, doc, err));
    CHECK(doc.layers.size() == 1);
    if (doc.layers.empty()) return;
    CHECK(doc.layers[0].rgba == l.rgba);
}

TEST(psd_rejects_unsupported_files) {
    psdtest::Layer l; l.name = "L"; l.right = 2; l.bottom = 2; l.rgba = solid(2, 2, 1, 2, 3, 4);
    PsdDocument doc; std::string err;
    CHECK(!readPsdMemory(psdtest::writePsd(4, 4, {l}, 16), doc, err));
    CHECK(err.find("16-bit") != std::string::npos);
    CHECK(!readPsdMemory(psdtest::writePsd(4, 4, {l}, 8, 4), doc, err));
    CHECK(err.find("CMYK") != std::string::npos);
    std::vector<uint8_t> junk = {'n', 'o', 't', ' ', 'a', ' ', 'p', 's', 'd'};
    CHECK(!readPsdMemory(junk, doc, err));
    CHECK(!err.empty());
    // ZIP-compressed channel data (compression word 2) is reported, not misread.
    psdtest::Layer z = l; z.forceComp = 2;
    PsdDocument d2; std::string e2;
    CHECK(!readPsdMemory(psdtest::writePsd(4, 4, {z}), d2, e2));
    CHECK(e2.find("ZIP") != std::string::npos);
    CHECK(!readPsd("C:/definitely/not/here.psd", doc, err));
}

// A real Photoshop file, when present on this machine (skipped otherwise, like the Spineboy checks).
TEST(psd_reads_real_photoshop_file_if_present) {
    for (const char* path : {"C:/Users/Div/Desktop/charachter.psd", "C:/Users/Div/Desktop/Golden Spike.psd"}) {
        FILE* f = std::fopen(path, "rb");
        if (!f) { std::printf("    (skipped: %s not found)\n", path); continue; }
        std::fclose(f);
        PsdDocument doc;
        std::string err;
        bool ok = readPsd(path, doc, err);
        std::printf("    %s: %s, %dx%d, %d layers\n", path, ok ? "read" : err.c_str(), doc.width, doc.height, (int)doc.layers.size());
        if (!ok) continue; // an unsupported mode is reported, not a crash
        CHECK(doc.width > 0 && doc.height > 0);
        for (auto& L : doc.layers) {
            CHECK((int)L.rgba.size() == L.width() * L.height() * 4);
            CHECK(L.width() > 0 && L.height() > 0);
        }
    }
}
