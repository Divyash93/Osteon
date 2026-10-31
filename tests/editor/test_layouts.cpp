// Named layouts: names, the file format, save / load / rename / delete.
#include "../Test.h"
#include "Layouts.h"
#include "Util.h"
#include <filesystem>

using namespace osteon;

struct TempDir {
    std::string path = "osteon_layouts_test_dir";
    TempDir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    ~TempDir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};

static SavedLayout sample(bool first, const char* ini) {
    SavedLayout l;
    for (int i = 0; i < kLayoutPanels; i++) l.panels[i] = (i % 2 == 0) == first;
    l.ini = ini;
    return l;
}

TEST(layout_names_are_cleaned_and_unusable_ones_rejected) {
    CHECK(cleanLayoutName("  My layout  ") == "My layout");
    CHECK(cleanLayoutName("Animating 2").size() == 11);
    CHECK(cleanLayoutName("").empty() && cleanLayoutName("   ").empty());
    CHECK(cleanLayoutName("a/b").empty() && cleanLayoutName("a\\b").empty() && cleanLayoutName("a:b").empty() && cleanLayoutName("what?").empty());
    CHECK(cleanLayoutName("con").empty() && cleanLayoutName("NUL").empty() && cleanLayoutName("lpt1").empty());
    CHECK(cleanLayoutName("trailing.").empty());
    CHECK(cleanLayoutName(std::string(49, 'x')).empty() && !cleanLayoutName(std::string(48, 'x')).empty());
}

TEST(layout_file_format_round_trips_and_rejects_garbage) {
    SavedLayout l = sample(true, "[Window][Tree]\nPos=1,2\nSize=3,4\n\n[Docking][Data]\nDockSpace ID=0x1\n");
    SavedLayout back;
    CHECK(parseLayout(serializeLayout(l), back));
    CHECK(back.panels == l.panels && back.ini == l.ini);
    CHECK(!parseLayout("", back));
    CHECK(!parseLayout("something else\n", back));
    CHECK(!parseLayout("osteon-layout 1\npanels 1 0 1\n[ini]\n", back));            // too few flags
    CHECK(!parseLayout("osteon-layout 1\npanels 1 0 1 0 1 0 1 0 1 0 1 0 2\n[ini]\n", back)); // a flag that is not 0 / 1
}

TEST(layout_save_load_list_overwrite_without_twins) {
    TempDir t;
    CHECK(listLayouts(t.path).empty());
    CHECK(saveLayout(t.path, "Setup wide", sample(true, "A")));
    CHECK(saveLayout(t.path, "animate", sample(false, "B")));
    auto names = listLayouts(t.path);
    CHECK(names.size() == 2 && names[0] == "animate" && names[1] == "Setup wide");         // sorted without regard to case
    SavedLayout got;
    CHECK(loadLayout(t.path, "animate", got) && got.ini == "B" && !got.panels[0]);
    CHECK(!loadLayout(t.path, "missing", got));
    // Saving under the same name in another case replaces the layout, it does not make a second one.
    CHECK(saveLayout(t.path, "ANIMATE", sample(true, "C")));
    CHECK(listLayouts(t.path).size() == 2);
    CHECK(loadLayout(t.path, "animate", got) && got.ini == "C");
    CHECK(!saveLayout(t.path, "bad/name", sample(true, "D")) && !saveLayout(t.path, "", sample(true, "D")));
    CHECK(layoutExists(t.path, "setup WIDE") && !layoutExists(t.path, "nope"));
}

TEST(layout_rename_and_delete) {
    TempDir t;
    saveLayout(t.path, "One", sample(true, "1"));
    saveLayout(t.path, "Two", sample(false, "2"));
    CHECK(renameLayout(t.path, "One", "Uno"));
    CHECK(!layoutExists(t.path, "One") && layoutExists(t.path, "Uno"));
    SavedLayout got;
    CHECK(loadLayout(t.path, "Uno", got) && got.ini == "1");                                // the contents moved with the name
    CHECK(!renameLayout(t.path, "Uno", "two"));                                              // another layout already has that name
    CHECK(!renameLayout(t.path, "Nothing", "Whatever"));                                     // source missing
    CHECK(!renameLayout(t.path, "Uno", "bad:name") && layoutExists(t.path, "Uno"));          // unusable target leaves it alone
    CHECK(renameLayout(t.path, "Uno", "UNO"));                                               // a change of case only is allowed
    CHECK(listLayouts(t.path).size() == 2);
    CHECK(renameLayout(t.path, "UNO", "UNO"));                                               // renaming to itself is a no-op success
    CHECK(deleteLayout(t.path, "Two"));
    CHECK(!layoutExists(t.path, "Two") && listLayouts(t.path).size() == 1);
    CHECK(!deleteLayout(t.path, "Two"));                                                     // already gone
}
