// Texture unpacker: reading .atlas text and cutting regions out of pages.
#include "Test.h"
#include "Geometry.h"
#include <cstring>

using namespace osteon;

TEST(atlas_parse_reads_the_4x_layout) {
    const char* text =
        "page1.png\n"
        "size:256,128\n"
        "filter:Linear,Linear\n"
        "hat\n"
        "bounds:10,20,30,40\n"
        "rotate:90\n"
        "offsets:2,3,50,60\n"
        "arm\n"
        "bounds:100,0,16,16\n"
        "\n"
        "page2.png\n"
        "size:64,64\n"
        "leg\n"
        "bounds:0,0,8,8\n";
    std::vector<AtlasPage> pages;
    std::vector<AtlasRegion> regions;
    CHECK(parseAtlas(text, pages, regions));
    CHECK(pages.size() == 2 && pages[0].file == "page1.png" && pages[0].w == 256 && pages[0].h == 128 && pages[1].w == 64);
    CHECK(regions.size() == 3);
    CHECK(regions[0].name == "hat" && regions[0].x == 10 && regions[0].w == 30 && regions[0].h == 40 && regions[0].rotated);
    CHECK(regions[0].origW == 50 && regions[0].origH == 60 && regions[0].offX == 2 && regions[0].offY == 3);
    CHECK(regions[1].name == "arm" && !regions[1].rotated && regions[1].origW == 16 && regions[1].page == 0);
    CHECK(regions[2].name == "leg" && regions[2].page == 1);
}

TEST(atlas_parse_reads_the_older_layout_and_rejects_empty) {
    const char* text = "old.png\nsize: 64, 64\nformat: RGBA8888\nfilter: Nearest,Nearest\nrepeat: none\n"
                       "head\n  rotate: true\n  xy: 4, 8\n  size: 10, 20\n  orig: 12, 22\n  offset: 1, 2\n  index: -1\n";
    std::vector<AtlasPage> pages;
    std::vector<AtlasRegion> regions;
    CHECK(parseAtlas(text, pages, regions));
    CHECK(regions.size() == 1 && regions[0].rotated && regions[0].x == 4 && regions[0].y == 8 && regions[0].w == 10 && regions[0].h == 20);
    CHECK(regions[0].origW == 12 && regions[0].offY == 2);
    CHECK(!parseAtlas("", pages, regions));
}

TEST(atlas_pack_then_unpack_returns_the_original_pixels) {
    // Three images of different sizes, packed with rotation allowed, written to a page the way the packer does.
    const int sizes[3][2] = {{20, 7}, {5, 30}, {12, 12}};
    std::vector<std::vector<uint8_t>> img(3);
    std::vector<PackRect> rects;
    std::vector<std::string> names;
    for (int i = 0; i < 3; i++) {
        int w = sizes[i][0], h = sizes[i][1];
        img[i].resize((size_t)w * h * 4);
        for (int p = 0; p < w * h; p++) { img[i][p * 4] = (uint8_t)(p * 7 + i * 31); img[i][p * 4 + 1] = (uint8_t)(i * 80); img[i][p * 4 + 2] = (uint8_t)p; img[i][p * 4 + 3] = 255; }
        rects.push_back({i, w, h});
        names.push_back("img" + std::to_string(i));
    }
    int pages = packRects(rects, 64, 64, 1, true, false);
    CHECK(pages == 1);
    auto [pw, ph] = packedPageSize(rects, 0, false);
    std::vector<uint8_t> page((size_t)pw * ph * 4, 0);
    for (auto& r : rects) {
        int sw = sizes[r.id][0], sh = sizes[r.id][1];
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++) {
                int dx = r.rotated ? r.x + (sh - 1 - y) : r.x + x, dy = r.rotated ? r.y + x : r.y + y;
                std::memcpy(&page[((size_t)dy * pw + dx) * 4], &img[r.id][((size_t)y * sw + x) * 4], 4);
            }
    }
    std::string atlas = writeAtlas({"p.png"}, {{pw, ph}}, rects, names);
    std::vector<AtlasPage> ap;
    std::vector<AtlasRegion> ar;
    CHECK(parseAtlas(atlas, ap, ar));
    CHECK(ar.size() == 3);
    for (auto& r : ar) {
        int id = r.name.back() - '0', w, h;
        std::vector<uint8_t> out = extractAtlasRegion(page.data(), pw, ph, r, w, h);
        CHECK(w == sizes[id][0] && h == sizes[id][1]);
        CHECK(out == img[id]);
    }
}

TEST(atlas_extract_places_trimmed_regions_on_the_original_canvas) {
    std::vector<uint8_t> page(8 * 8 * 4, 0);
    for (int y = 2; y < 4; y++) for (int x = 3; x < 6; x++) { uint8_t* p = &page[(y * 8 + x) * 4]; p[0] = 200; p[3] = 255; }
    AtlasRegion r;
    r.x = 3; r.y = 2; r.w = 3; r.h = 2; r.origW = 10; r.origH = 6; r.offX = 4; r.offY = 1;
    int w, h;
    std::vector<uint8_t> out = extractAtlasRegion(page.data(), 8, 8, r, w, h);
    CHECK(w == 10 && h == 6);
    // offsets are from the bottom: the region's top row is at 6 - 1 - 2 = 3
    CHECK(out[(3 * 10 + 4) * 4 + 3] == 255);
    CHECK(out[(0 * 10 + 0) * 4 + 3] == 0);
    CHECK(extractAtlasRegion(page.data(), 8, 8, AtlasRegion{"x", 0, 7, 7, 3, 3}, w, h).empty()); // runs off the page
}
