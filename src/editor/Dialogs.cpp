// Export, Texture Packer, Settings, Hotkeys and About dialogs.
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

void Editor::exportDialog() {
    ImGui::SetNextWindowSize(ImVec2(620 * uiScale, 380 * uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Export", &showExport, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
    static int type = 0; // 0 JSON, 1 PNG sequence, 2 GIF
    static std::string out;
    static bool pretty = true, nonessential = true, transparent = true, openAfter = false, allAnims = false;
    static float scale = 1, fps = 30;
    static int pad = 2;
    static Color gifBg(0.2f, 0.2f, 0.2f, 1);
    static std::string jsonVersion = "4.2.00";
    if (out.empty()) out = doc.projectPath.empty() ? (doc.imagesDir.empty() ? "" : parentDir(doc.imagesDir)) : parentDir(doc.projectPath);
    ImGui::BeginChild("##types", ImVec2(130 * uiScale, 0), ImGuiChildFlags_Borders);
    ImGui::TextDisabled("Data");
    if (ImGui::Selectable("JSON", type == 0)) type = 0;
    ImGui::TextDisabled("Image");
    if (ImGui::Selectable("PNG sequence", type == 1)) type = 1;
    if (ImGui::Selectable("GIF", type == 2)) type = 2;
    ImGui::Separator();
    ImGui::TextDisabled("Not available:");
    ImGui::TextDisabled("Binary (.skel)");
    ImGui::TextDisabled("Video, PSD");
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushItemWidth(-140 * uiScale);
    inputPath("Output folder", out);
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) { std::string f = pickFolderDialog(hwnd, L"Export folder"); if (!f.empty()) out = f; }
    std::string base = doc.projectPath.empty() ? "skeleton" : stripExtension(fileName(doc.projectPath));
    if (type == 0) {
        ImGui::Checkbox("Pretty print", &pretty);
        ImGui::Checkbox("Nonessential data (bone colors, mesh edges, images path)", &nonessential);
        // The Spine editor imports only data whose version equals its own (the Trial is always the latest).
        static int target = 0;
        static char version43[16] = "4.3.26";
        ImGui::RadioButton("Spine 4.2 (runtimes)", &target, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Spine 4.3 editor", &target, 1);
        if (target == 1) { ImGui::SameLine(); ImGui::SetNextItemWidth(80 * uiScale); ImGui::InputText("##ver", version43, sizeof version43); }
        jsonVersion = target == 0 ? "4.2.00" : version43;
        ImGui::TextDisabled("Writes %s.json, version %s.", base.c_str(), jsonVersion.c_str());
    } else {
        int anim = std::max(0, doc.animation);
        ImGui::Checkbox("All animations", &allAnims);
        if (!allAnims) {
            if (ImGui::BeginCombo("Animation", doc.data.animations.empty() ? "(setup pose)" : doc.data.animations[std::min(anim, (int)doc.data.animations.size() - 1)].name.c_str())) {
                for (size_t i = 0; i < doc.data.animations.size(); i++)
                    if (ImGui::Selectable(doc.data.animations[i].name.c_str(), (int)i == doc.animation)) doc.animation = (int)i;
                ImGui::EndCombo();
            }
        }
        ImGui::DragFloat("FPS", &fps, 0.5f, 1, 120, "%.0f");
        ImGui::DragFloat("Scale", &scale, 0.01f, 0.05f, 4, "%.2f");
        ImGui::DragInt("Padding", &pad, 1, 0, 100);
        if (type == 1) ImGui::Checkbox("Transparent background", &transparent);
        else { float c[3] = {gifBg.r, gifBg.g, gifBg.b}; if (ImGui::ColorEdit3("Background", c)) gifBg = Color(c[0], c[1], c[2], 1); }
    }
    ImGui::Checkbox("Open folder after export", &openAfter);
    ImGui::PopItemWidth();
    ImGui::Separator();
    if (ImGui::Button("Export", ImVec2(120 * uiScale, 0))) {
        if (out.empty() || !makeDirs(out)) setStatus("Choose a valid output folder");
        else if (type == 0) {
            std::string err, path = joinPath(out, base + ".json");
            if (doc.exportJson(path, pretty, nonessential, err, jsonVersion)) setStatus("Exported " + path);
            else setStatus(err);
        } else {
            std::vector<int> anims;
            if (doc.data.animations.empty()) anims = {-1};
            else if (allAnims) for (size_t i = 0; i < doc.data.animations.size(); i++) anims.push_back((int)i);
            else anims = {std::max(0, doc.animation)};
            int files = 0;
            for (int a : anims) {
                std::vector<std::vector<uint8_t>> frames;
                int w, h;
                renderFrames(a, fps, scale, pad, frames, w, h, type == 1 && transparent, gifBg);
                std::string name = a >= 0 ? doc.data.animations[a].name : "setup";
                if (type == 1) {
                    for (size_t f = 0; f < frames.size(); f++) {
                        char suffix[32];
                        std::snprintf(suffix, sizeof suffix, "_%04zu.png", f);
                        if (savePngFile(joinPath(out, name + suffix), w, h, frames[f].data())) files++;
                    }
                } else if (saveGifFile(joinPath(out, name + ".gif"), w, h, frames, std::max(2, (int)std::round(100 / fps)))) files++;
            }
            setStatus("Exported " + std::to_string(files) + " file(s) to " + out);
        }
        if (openAfter) openInExplorer(out);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120 * uiScale, 0))) showExport = false;
    ImGui::EndGroup();
    ImGui::End();
}

void Editor::packerDialog() {
    ImGui::SetNextWindowSize(ImVec2(520 * uiScale, 330 * uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Texture Packer", &showPacker, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
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

void Editor::settingsDialog() {
    ImGui::SetNextWindowSize(ImVec2(460 * uiScale, 300 * uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Settings", &showSettings, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
    // User interface theme.
    ImGui::TextUnformatted("Theme");
    ImGui::SameLine();
    {
        int theme = gLightUi ? 1 : 0;
        bool changed = ImGui::RadioButton("Dark", &theme, 0);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Light", &theme, 1);
        if (changed) {
            gLightUi = theme == 1;
            applyUiTheme(uiScale, gLightUi);
            // The canvas checkerboard follows the theme (it can still be changed below).
            if (gLightUi) { bgA = Color(0.74f, 0.74f, 0.74f, 1); bgB = Color(0.79f, 0.79f, 0.79f, 1); }
            else { bgA = Color(0.318f, 0.318f, 0.318f, 1); bgB = Color(0.337f, 0.337f, 0.337f, 1); }
            writeTextFile(joinPath(appDataDir(), "ui_theme.txt"), gLightUi ? "light" : "dark");
        }
    }
    ImGui::Separator();
    float a[3] = {bgA.r, bgA.g, bgA.b}, b[3] = {bgB.r, bgB.g, bgB.b};
    if (ImGui::ColorEdit3("Background A", a)) bgA = Color(a[0], a[1], a[2], 1);
    if (ImGui::ColorEdit3("Background B", b)) bgB = Color(b[0], b[1], b[2], 1);
    ImGui::Checkbox("Show axes", &showAxes);
    ImGui::Checkbox("Image names", &showImageNames);
    ImGui::Checkbox("Selectable bones", &selectBones);
    ImGui::SameLine(); ImGui::Checkbox("Selectable images", &selectImages);
    ImGui::SameLine(); ImGui::Checkbox("Selectable others", &selectOthers);
    ImGui::Separator();
    ImGui::TextUnformatted("Ghosting (onion skin, I)");
    ImGui::Checkbox("Enabled", &ghosting);
    sliderInt("Frames each side", &ghostFrames, 1, 8);
    sliderInt("Frame step", &ghostStep, 1, 10);
    sliderFloat("Opacity", &ghostAlpha, 0.05f, 1);
    ImGui::Separator();
    if (ImGui::Button("Reset window layout")) resetLayout = true;
    ImGui::SameLine();
    if (ImGui::Button("Open settings folder")) openInExplorer(appDataDir());
    ImGui::End();
}

void Editor::hotkeysDialog() {
    ImGui::SetNextWindowSize(ImVec2(520 * uiScale, 520 * uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hotkeys", &showHotkeys, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
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
    ImGui::End();
}

void Editor::aboutDialog() {
    if (!ImGui::Begin("About Osteon", &showAbout, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
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
