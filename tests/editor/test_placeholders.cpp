// Skin placeholders: attachment names on a slot that each skin fills with its own attachment.
#include "../Test.h"
#include "Document.h"
#include <cstdio>

using namespace osteon;

TEST(doc_skin_placeholders_are_filled_per_skin_and_persist) {
    Document doc;
    doc.newProject();
    int slot = doc.createSlot(0, "hat");
    std::string ph = doc.createPlaceholder(slot, "head");
    CHECK(ph == "head" && doc.isPlaceholder(slot, "head"));
    CHECK(doc.createPlaceholder(slot, "head") == "head2");  // unique per slot
    CHECK(doc.data.slots[slot].attachment == "head");       // the first one shows in setup
    int skinA = doc.createSkin("A"), skinB = doc.createSkin("B");
    doc.skin = skinA;
    doc.addRegionAttachment(slot, "hatA", {0, 0}, "head", true);
    CHECK(doc.data.skins[skinA].get(slot, "head") != nullptr);
    CHECK(doc.data.skins[skinB].get(slot, "head") == nullptr);
    CHECK(doc.data.skins[0].get(slot, "head") == nullptr);
    CHECK(doc.data.findAttachment(skinA, slot, "head") != nullptr);
    CHECK(doc.data.findAttachment(skinB, slot, "head") == nullptr);
    // A project file keeps the placeholders; a plain export leaves them out.
    std::string err, path = "osteon_ph_test.osteon";
    CHECK(doc.saveProject(path, err));
    Document back;
    CHECK(back.openFile(path, err));
    CHECK(back.data.slots[slot].placeholders.size() == 2);
    std::remove(path.c_str());
    JsonWriteOptions o;
    CHECK(writeSkeletonJson(doc.data, o).find("placeholders") == std::string::npos);
    // Deleting an empty placeholder removes it.
    doc.selection = {{SelType::Attachment, slot, "head2"}};
    doc.deleteSelection();
    CHECK(!doc.isPlaceholder(slot, "head2"));
    CHECK(doc.isPlaceholder(slot, "head"));
}

TEST(doc_renaming_an_empty_placeholder_follows_into_the_slot) {
    Document doc;
    doc.newProject();
    int slot = doc.createSlot(0, "hat");
    doc.createPlaceholder(slot, "head");
    doc.selection = {{SelType::Attachment, slot, "head"}};
    CHECK(doc.renameSelection("cap"));
    CHECK(doc.isPlaceholder(slot, "cap") && !doc.isPlaceholder(slot, "head"));
    CHECK(doc.data.slots[slot].attachment == "cap");
}
