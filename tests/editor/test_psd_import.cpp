// PSD import into a Document: slots, attachment placement, PNG output, undo.
#include "../Test.h"
#include "Document.h"
#include "Gfx.h"
#include "Util.h"
#include "test_psd.h"
#include <windows.h>
#include <filesystem>
#include <fstream>

using namespace osteon;
namespace fs = std::filesystem;

static std::string freshDir(const char* name) {
    fs::path p = fs::path("C:/Users/Div/AppData/Local/Temp/claude/C--Users-Div-Desktop-Spine2d/6ebadf2a-6fff-49be-89a9-f30b11a3bb0f/scratchpad/psdtest") / name;
    std::error_code ec;
    fs::remove_all(p, ec);
    fs::create_directories(p, ec);
    return p.generic_string();
}

static std::string writeFixture(const std::string& dir) {
    auto bytes = psdtest::makeFixture();
    std::string path = dir + "/hero.psd";
    std::ofstream f(path, std::ios::binary);
    f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
    return path;
}

static void checkImport(Document& doc, const std::string& imgDir, const std::string& prefix) {
    CHECK(doc.data.slots.size() == 3);
    if (doc.data.slots.size() != 3) return;
    CHECK(doc.data.slots[0].name == "Background");
    CHECK(doc.data.slots[1].name == "Arm \xC3\xA9");
    CHECK(doc.data.slots[2].name == "Top"); // last slot draws on top
    auto* bg = doc.data.defaultSkin().get(0, doc.data.slots[0].attachment);
    auto* arm = doc.data.defaultSkin().get(1, doc.data.slots[1].attachment);
    auto* top = doc.data.defaultSkin().get(2, doc.data.slots[2].attachment);
    CHECK(bg && arm && top);
    if (!(bg && arm && top)) return;
    // Canvas is 16x12, centred on the origin, y up.
    CHECK_NEAR(bg->width, 8, 1e-4);  CHECK_NEAR(bg->height, 6, 1e-4);
    CHECK_NEAR(bg->x, -4, 1e-4);     CHECK_NEAR(bg->y, 3, 1e-4);
    CHECK_NEAR(arm->width, 10, 1e-4); CHECK_NEAR(arm->height, 8, 1e-4);
    CHECK_NEAR(arm->x, -1, 1e-4);    CHECK_NEAR(arm->y, 1, 1e-4);
    CHECK_NEAR(top->x, -7, 1e-4);    CHECK_NEAR(top->y, 2, 1e-4);
    CHECK_NEAR(top->color.a, 128 / 255.0, 1e-4);
    CHECK(bg->path == prefix + "Background");
    CHECK(fileExists(imgDir + "/Background.png"));
    CHECK(fileExists(imgDir + "/Arm \xC3\xA9.png"));
    CHECK(fileExists(imgDir + "/Top.png"));
    CHECK(!fileExists(imgDir + "/Hidden.png"));
    int w = 0, h = 0;
    CHECK(loadImageFileSize(imgDir + "/Arm \xC3\xA9.png", w, h));
    CHECK(w == 10 && h == 8);
    CHECK(!doc.imagePathFor(*bg).empty()); // the attachment resolves to a real file
}

TEST(psd_import_creates_slots_and_pngs) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::string dir = freshDir("a");
    std::string psd = writeFixture(dir);
    Document doc;
    doc.newProject();
    std::string err;
    int hidden = 0;
    int n = doc.importPsd(psd, err, &hidden);
    CHECK(n == 3);
    CHECK(hidden == 1);
    CHECK(doc.imagesDir == dir + "/hero_images");
    checkImport(doc, dir + "/hero_images", "");
    doc.undo();
    CHECK(doc.data.slots.empty());
}

TEST(psd_import_into_existing_images_folder) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::string dir = freshDir("b");
    std::string psd = writeFixture(dir);
    Document doc;
    doc.newProject();
    doc.imagesDir = dir + "/art";
    fs::create_directories(doc.imagesDir);
    std::string err;
    CHECK(doc.importPsd(psd, err) == 3);
    CHECK(doc.imagesDir == dir + "/art");
    checkImport(doc, dir + "/art/hero", "hero/");
    // Importing again keeps names unique.
    CHECK(doc.importPsd(psd, err) == 3);
    CHECK(doc.data.slots.size() == 6);
    CHECK(doc.data.slots[3].name != doc.data.slots[0].name);
}

TEST(psd_import_reports_bad_file) {
    std::string dir = freshDir("c");
    std::ofstream(dir + "/bad.psd", std::ios::binary) << "garbage";
    Document doc;
    doc.newProject();
    std::string err;
    CHECK(doc.importPsd(dir + "/bad.psd", err) < 0);
    CHECK(!err.empty());
    CHECK(doc.data.slots.empty());
}

static void writePng(const std::string& path, int w, int h) {
    std::vector<unsigned char> px((size_t)w * h * 4, 255);
    CHECK(savePngFile(path, w, h, px.data()));
}

TEST(import_images_adds_one_slot_per_file_and_copies_outsiders) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::string dir = freshDir("img");
    fs::create_directories(dir + "/src");
    writePng(dir + "/src/a.png", 4, 3);
    writePng(dir + "/src/b.png", 6, 5);
    Document doc;
    doc.newProject();
    std::string err;
    // No images folder yet: the first image's folder is adopted, nothing is copied.
    CHECK(doc.importImageFiles({dir + "/src/a.png", dir + "/src/b.png"}, {10, 20}, err) == 2);
    CHECK(doc.imagesDir == dir + "/src");
    CHECK(doc.data.slots.size() == 2);
    CHECK(doc.data.slots[0].name == "a" && doc.data.slots[1].name == "b");
    auto* b = doc.data.defaultSkin().get(1, doc.data.slots[1].attachment);
    CHECK(b && b->width == 6 && b->height == 5);
    doc.undo();
    CHECK(doc.data.slots.empty());
    // With an images folder set, files from elsewhere are copied in; a name clash gets a suffix.
    fs::create_directories(dir + "/other");
    writePng(dir + "/other/a.png", 2, 2);
    CHECK(doc.importImageFiles({dir + "/other/a.png"}, {0, 0}, err) == 1);
    CHECK(fileExists(dir + "/src/a_2.png"));
    CHECK(doc.importImageFiles({dir + "/other/notes.txt"}, {0, 0}, err) < 0);
}
