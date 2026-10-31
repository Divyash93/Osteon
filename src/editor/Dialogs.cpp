// Texture Packer and Unpacker, Settings, Hotkeys and About dialogs (the Export dialog is in ExportDialog.cpp).
#include "App.h"
#include "Geometry.h"
#include "Icons.h"

namespace osteon {

static bool inputPath(const char* label, std::string& s) {
    char buf[1024];
    strncpy_s(buf, s.c_str(), _TRUNCATE);
    bool r = ImGui::InputText(label, buf, sizeof buf);
    if (r) s = buf;
    return r;
}

void Editor::packerDialog() {
    ImGui::SetNextWindowSize(ImVec2(520 * uiScale, 330 * uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Texture Packer", &showPacker, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
    windowShadow();
    static std::string input, output, name = "skeleton";
    static int maxSize = 2048, padding = 2;
    static bool rotate = false, pow2 = true, trimNone = true;
    if (input.empty()) input = doc.imagesDir;
    if (output.empty() && !input.empty()) output = joinPath(parentDir(input), "export");
    ImGui::PushItemWidth(-140 * uiScale);
    inputPath("Input folder", input);
    ImGui::SameLine();
    if (ImGui::Button("...##in")) { std::string f = pickFolderDialog(hwnd, L"Images folder"); if (!f.empty()) input = f; }
    inputPath("Output folder", output);
    ImGui::SameLine();
    if (ImGui::Button("...##out")) { std::string f = pickFolderDialog(hwnd, L"Output folder"); if (!f.empty()) output = f; }
    char nb[128];
    strncpy_s(nb, name.c_str(), _TRUNCATE);
    if (ImGui::InputText("Atlas name", nb, sizeof nb)) name = nb;
    ImGui::DragInt("Max page size", &maxSize, 16, 64, 8192);
    ImGui::DragInt("Padding", &padding, 1, 0, 32);
    ImGui::Checkbox("Allow rotation", &rotate);
    ImGui::SameLine();
    ImGui::Checkbox("Power of two", &pow2);
    ImGui::PopItemWidth();
    ImGui::TextDisabled("Packs every image in the input folder into %s.atlas + PNG pages (libGDX/Spine atlas format).", name.c_str());
    if (ImGui::Button("Pack", ImVec2(120 * uiScale, 0))) {
        std::vector<std::string> files = listFiles(input, {".png"});
        std::vector<PackRect> rects;
        std::vector<std::string> names;
        std::vector<std::vector<uint8_t>> pixels;
        std::vector<std::pair<int, int>> sizes;
        for (auto& f : files) {
            int w, h;
            std::vector<uint8_t> px;
            if (!loadImageFile(joinPath(input, f), w, h, px)) continue;
            rects.push_back({(int)rects.size(), w, h});
            names.push_back(stripExtension(f));
            pixels.push_back(std::move(px));
            sizes.push_back({w, h});
        }
        if (rects.empty()) setStatus("No PNG images in the input folder");
        else if (!makeDirs(output)) setStatus("Cannot create output folder");
        else {
            int pages = packRects(rects, maxSize, maxSize, padding, rotate, pow2);
            bool tooBig = false;
            for (auto& r : rects) tooBig |= r.page < 0;
            if (tooBig) setStatus("Some images are larger than the max page size");
            else {
                std::vector<std::string> pageFiles;
                std::vector<std::pair<int, int>> pageSizes;
                for (int p = 0; p < pages; p++) {
                    auto [pw, ph] = packedPageSize(rects, p, pow2);
                    std::vector<uint8_t> page((size_t)pw * ph * 4, 0);
                    for (auto& r : rects) {
                        if (r.page != p) continue;
                        const auto& src = pixels[r.id];
                        int sw = sizes[r.id].first, sh = sizes[r.id].second;
                        for (int y = 0; y < sh; y++)
                            for (int x = 0; x < sw; x++) {
                                // Rotated regions are stored 90 degrees clockwise.
                                int dx = r.rotated ? r.x + (sh - 1 - y) : r.x + x;
                                int dy = r.rotated ? r.y + x : r.y + y;
                                if (dx < 0 || dy < 0 || dx >= pw || dy >= ph) continue;
                                memcpy(&page[((size_t)dy * pw + dx) * 4], &src[((size_t)y * sw + x) * 4], 4);
                            }
                    }
                    std::string pf = name + (pages > 1 ? "_" + std::to_string(p + 1) : "") + ".png";
                    savePngFile(joinPath(output, pf), pw, ph, page.data());
                    pageFiles.push_back(pf);
                    pageSizes.push_back({pw, ph});
                }
                std::vector<std::string> orderedNames(rects.size());
                for (size_t i = 0; i < rects.size(); i++) orderedNames[i] = names[rects[i].id];
                writeTextFile(joinPath(output, name + ".atlas"), writeAtlas(pageFiles, pageSizes, rects, orderedNames));
                setStatus("Packed " + std::to_string(rects.size()) + " images into " + std::to_string(pages) + " page(s)");
                openInExplorer(output);
            }
        }
    }
    ImGui::End();
}

void Editor::unpackerDialog() {
    ImGui::SetNextWindowSize(ImVec2(520 * uiScale, 250 * uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Texture Unpacker", &showUnpacker, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
    windowShadow();
    static std::string atlasPath, output;
    ImGui::PushItemWidth(-140 * uiScale);
    inputPath("Atlas file", atlasPath);
    ImGui::SameLine();
    if (ImGui::Button("...##atlas")) {
        std::string f = openFileDialog(hwnd, L"Atlas (*.atlas)\0*.atlas\0All files\0*.*\0", L"Open atlas");
        if (!f.empty()) { atlasPath = f; output = joinPath(parentDir(f), stripExtension(fileName(f)) + "_images"); }
    }
    inputPath("Output folder", output);
    ImGui::SameLine();
    if (ImGui::Button("...##unout")) { std::string f = pickFolderDialog(hwnd, L"Output folder"); if (!f.empty()) output = f; }
    ImGui::PopItemWidth();
    ImGui::TextDisabled("Cuts every region of a .atlas (libGDX / Spine) back out of its page PNGs into one PNG per region, named after the region.");
    if (ImGui::Button("Unpack", ImVec2(120 * uiScale, 0))) {
        std::string text;
        std::vector<AtlasPage> pages;
        std::vector<AtlasRegion> regions;
        if (atlasPath.empty() || !readTextFile(atlasPath, text) || !parseAtlas(text, pages, regions)) setStatus("Cannot read an atlas from that file");
        else if (output.empty() || !makeDirs(output)) setStatus("Choose a valid output folder");
        else {
            std::vector<std::vector<uint8_t>> pageData(pages.size());
            std::vector<std::pair<int, int>> pageSize(pages.size());
            int written = 0, failed = 0;
            for (auto& r : regions) {
                if (r.page < 0 || r.page >= (int)pages.size()) { failed++; continue; }
                auto& pd = pageData[r.page];
                if (pd.empty()) {
                    int w = 0, h = 0;
                    if (!loadImageFile(joinPath(parentDir(atlasPath), pages[r.page].file), w, h, pd)) pd.clear();
                    pageSize[r.page] = {w, h};
                }
                int w = 0, h = 0;
                std::vector<uint8_t> px = pd.empty() ? std::vector<uint8_t>() : extractAtlasRegion(pd.data(), pageSize[r.page].first, pageSize[r.page].second, r, w, h);
                if (px.empty()) { failed++; continue; }
                std::string name = r.name;
                if (r.index >= 0) name += "_" + std::to_string(r.index);
                std::string file = joinPath(output, name + ".png");
                makeDirs(parentDir(file)); // names such as "arm/upper" become sub-folders
                if (savePngFile(file, w, h, px.data())) written++; else failed++;
            }
            setStatus("Unpacked " + std::to_string(written) + " image(s)" + (failed ? ", " + std::to_string(failed) + " failed" : "") + " to " + output);
            if (written) openInExplorer(output);
        }
    }
    ImGui::End();
}

void Editor::hotkeysDialog() {
    ImGui::SetNextWindowSize(ImVec2(520 * uiScale, 520 * uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hotkeys", &showHotkeys, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
    windowShadow();
    static const char* rows[][2] = {
        {"Undo / Redo", "Ctrl+Z / Ctrl+Shift+Z, Ctrl+Y"}, {"Setup / Animate mode", "Ctrl+Tab"}, {"Rulers", "Ctrl+Shift+R"},
        {"Zoom to fit / 100%", "Ctrl+Shift+F / Ctrl+F"}, {"Pan / Zoom", "Right or middle drag, J+drag / wheel"},
        {"Rotate / Translate / Scale / Shear", "C / V / X / Z"}, {"Pose / Create / Weights", "B / N / G"},
        {"Nudge (10px / 1px)", "Arrows / Shift+Arrows"}, {"Show / hide bones, images", "Ctrl+B / Ctrl+I"},
        {"Deselect", "Space, Esc"}, {"Rename / Duplicate / Delete", "F2 / Ctrl+D / Del"}, {"Visibility / Show all", "H / Ctrl+H"},
        {"Draw order up / down", "+ / -"}, {"Set parent", "P then click bone"}, {"Next / previous skin", ". / ,"},
        {"Play / play backward", "D / A"}, {"First / last key", "Q / E"}, {"Next / previous key", "W / S"},
        {"Next / previous frame (x10)", "R / F (Shift)"}, {"Loop start / end", "[ / ]"}, {"Key edited / key active", "K / L"},
        {"Next / previous animation", "Y / T"}, {"Ghosting", "I"}, {"Views", "Alt+T/D/G/A/S/E/W/R/M/O/B"},
        {"New / Open / Save / Save As", "Ctrl+Shift+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S"}, {"Export / Texture packer / Settings", "Ctrl+E / Ctrl+P / F12"},
    };
    if (ImGui::BeginTable("##hk", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        for (auto& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(r[0]);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(r[1]);
        }
        ImGui::EndTable();
    }
    // Remapping: pick a shortcut key above (as it is listed), then the key that should trigger it instead.
    ImGui::Separator();
    ImGui::TextUnformatted("Remap a key");
    static int capture = 0; // 1 = waiting for the key to replace, 2 = waiting for the new key
    static ImGuiKey fromKey = ImGuiKey_None, toKey = ImGuiKey_None;
    auto keyName = [](ImGuiKey k) { return k == ImGuiKey_None ? std::string("(none)") : std::string(ImGui::GetKeyName(k)); };
    if (capture) {
        for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++) {
            if (k >= ImGuiKey_LeftCtrl && k <= ImGuiKey_RightSuper) continue; // modifiers are not remappable
            if (k >= ImGuiKey_MouseLeft && k <= ImGuiKey_MouseWheelY) continue;
            if (ImGui::IsKeyPressed((ImGuiKey)k, false)) {
                if (k == ImGuiKey_Escape) { capture = 0; break; }
                if (capture == 1) { fromKey = (ImGuiKey)k; capture = 2; }
                else { toKey = (ImGuiKey)k; capture = 0; }
                break;
            }
        }
    }
    if (ImGui::Button((capture == 1 ? "Press the shortcut key..." : ("Shortcut key: " + keyName(fromKey)).c_str()))) { capture = 1; }
    ImGui::SameLine();
    if (ImGui::Button((capture == 2 ? "Press the new key..." : ("New key: " + keyName(toKey)).c_str()))) { capture = 2; }
    ImGui::SameLine();
    ImGui::BeginDisabled(fromKey == ImGuiKey_None || toKey == ImGuiKey_None || fromKey == toKey);
    if (ImGui::Button("Apply")) { keyRemap[fromKey] = toKey; saveKeyRemap(); fromKey = toKey = ImGuiKey_None; }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Esc cancels a capture. The original key keeps its own shortcut unless you remap it too, so swap two keys by remapping both.");
    ImGuiKey removeKey = ImGuiKey_None;
    for (auto& [from, to] : keyRemap) {
        ImGui::BulletText("%s acts as %s", ImGui::GetKeyName(to), ImGui::GetKeyName(from));
        ImGui::SameLine();
        if (ImGui::SmallButton(("Reset##" + std::to_string((int)from)).c_str())) removeKey = from;
    }
    if (removeKey != ImGuiKey_None) { keyRemap.erase(removeKey); saveKeyRemap(); }
    ImGui::End();
}

void Editor::aboutDialog() {
    if (!ImGui::Begin("About Osteon", &showAbout, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
    windowShadow();
    ImGui::Text("Osteon 0.1.0");
    ImGui::TextUnformatted("Native 2D skeletal animation editor (C++20, Direct3D 11, Dear ImGui).");
    ImGui::TextUnformatted("Reads and writes Spine-format JSON (reads 3.8 / 4.x, writes 4.2).");
    ImGui::Separator();
    ImGui::TextUnformatted("Made by Divyash");
    ImGui::SameLine();
    if (ImGui::SmallButton("github.com/Divyash93")) openUrl("https://github.com/Divyash93");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open the author's GitHub page");
    ImGui::Separator();
    ImGui::TextDisabled("Third-party: Dear ImGui (MIT), nlohmann/json (MIT).");
    ImGui::TextDisabled("Not affiliated with Esoteric Software. Spine is a trademark of Esoteric Software LLC.");
    ImGui::End();
}

} // namespace osteon
