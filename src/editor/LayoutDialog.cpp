// Views > Layouts: save the current window layout under a name, and load, rename or delete saved ones.
#include "App.h"
#include "Icons.h"
#include "Layouts.h"
#include "Util.h"

namespace osteon {

SavedLayout Editor::captureLayout() const {
    SavedLayout l;
    bool* flags[kLayoutPanels] = {const_cast<bool*>(&showTree), const_cast<bool*>(&showProps), const_cast<bool*>(&showDopesheet), const_cast<bool*>(&showGraph),
                                  const_cast<bool*>(&showAnimations), const_cast<bool*>(&showSkins), const_cast<bool*>(&showEvents), const_cast<bool*>(&showMeshTools),
                                  const_cast<bool*>(&showWeights), const_cast<bool*>(&showPreview), const_cast<bool*>(&showMetrics), const_cast<bool*>(&showOutline),
                                  const_cast<bool*>(&showProblems)};
    for (int i = 0; i < kLayoutPanels; i++) l.panels[i] = *flags[i];
    l.ini = ImGui::SaveIniSettingsToMemory();
    return l;
}

void Editor::applyLayout(const SavedLayout& l) {
    bool* flags[kLayoutPanels] = {&showTree, &showProps, &showDopesheet, &showGraph, &showAnimations, &showSkins, &showEvents, &showMeshTools,
                                  &showWeights, &showPreview, &showMetrics, &showOutline, &showProblems};
    for (int i = 0; i < kLayoutPanels; i++) *flags[i] = l.panels[i];
    if (!l.ini.empty()) ImGui::LoadIniSettingsFromMemory(l.ini.c_str());
    layoutIni[(int)layoutMode] = l.ini;
}

void Editor::layoutsDialog() {
    ImGui::SetNextWindowSize(ImVec2(560 * uiScale, 460 * uiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(480 * uiScale, 340 * uiScale), ImVec2(1e9f, 1e9f));
    if (!ImGui::Begin("Layouts", &showLayouts, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) { ImGui::End(); return; }
    const float u = uiScale;
    static std::string nameBuf, renaming, renameBuf, armedDelete;
    static double armedUntil = 0;
    static bool focusName = false, focusRename = false;
    const std::string dir = layoutsDir();
    std::vector<std::string> names = listLayouts(dir);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Layouts");
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 30 * u);
    if (iconOnlyButton("##layclose", Icon::Delete, false, 24 * u, "Close")) showLayouts = false;
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20 * u, 14 * u));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * u, 9 * u));
    ImGui::BeginChild("##laybody", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);

    // ---- save the current layout ----
    ImGui::TextColored(ImVec4(0.53f, 0.82f, 0.86f, 1), "Save the current layout");
    ImGui::TextWrapped("Remembers which panels are open and where they are docked, for the %s.", doc.mode == Mode::Setup ? "Setup layout" : "Animate layout");
    char buf[128];
    strncpy_s(buf, nameBuf.c_str(), _TRUNCATE);
    ImGui::SetNextItemWidth(-130 * u);
    if (focusName) { ImGui::SetKeyboardFocusHere(); focusName = false; }
    bool enter = ImGui::InputTextWithHint("##layname", "Layout name", buf, sizeof buf, ImGuiInputTextFlags_EnterReturnsTrue);
    nameBuf = buf;
    ImGui::SameLine();
    const std::string cleaned = cleanLayoutName(nameBuf);
    ImGui::BeginDisabled(cleaned.empty());
    bool save = iconButton("##laysave", Icon::Save, "Save", false, ImVec2(112 * u, 26 * u), layoutExists(dir, cleaned) ? "A layout with this name exists: it is replaced" : "Save the layout");
    ImGui::EndDisabled();
    if ((save || (enter && !cleaned.empty())) && !cleaned.empty()) {
        bool replaced = layoutExists(dir, cleaned);
        if (saveLayout(dir, cleaned, captureLayout())) { setStatus(std::string(replaced ? "Layout replaced: " : "Layout saved: ") + cleaned); nameBuf.clear(); }
        else setStatus("Cannot save the layout");
    }
    if (!nameBuf.empty() && cleaned.empty()) ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.40f, 1), "Names cannot be empty, longer than 48 characters, or contain  < > : \" / \\ | ? *");

    // ---- the saved layouts ----
    ImGui::Dummy(ImVec2(0, 6 * u));
    ImGui::TextColored(ImVec4(0.53f, 0.82f, 0.86f, 1), "Saved layouts");
    ImGui::BeginChild("##laylist", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (names.empty()) ImGui::TextDisabled("No saved layouts yet.");
    for (const std::string& n : names) {
        ImGui::PushID(n.c_str());
        const float bw = 30 * u;
        if (renaming == n) {
            char rb[128];
            strncpy_s(rb, renameBuf.c_str(), _TRUNCATE);
            ImGui::SetNextItemWidth(-(bw * 2 + 12 * u));
            if (focusRename) { ImGui::SetKeyboardFocusHere(); focusRename = false; }
            bool done = ImGui::InputText("##rn", rb, sizeof rb, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            renameBuf = rb;
            ImGui::SameLine();
            std::string to = cleanLayoutName(renameBuf);
            bool ok = iconOnlyButton("##rnok", Icon::Save, false, bw, "Rename") || done;
            ImGui::SameLine();
            bool cancel = iconOnlyButton("##rncancel", Icon::Delete, false, bw, "Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
            if (ok) {
                if (to.empty()) setStatus("That name cannot be used");
                else if (renameLayout(dir, n, to)) { setStatus("Layout renamed to " + to); renaming.clear(); }
                else setStatus("A layout called \"" + to + "\" already exists");
            } else if (cancel) renaming.clear();
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(n.c_str());
            ImGui::SameLine(ImGui::GetContentRegionMax().x - (bw * 3 + 16 * u));
            if (iconOnlyButton("##layload", Icon::Folder, false, bw, "Load this layout")) {
                SavedLayout l;
                if (loadLayout(dir, n, l)) { applyLayout(l); setStatus("Layout loaded: " + n); }
                else setStatus("Cannot read that layout");
            }
            ImGui::SameLine();
            if (iconOnlyButton("##layrename", Icon::Pencil, false, bw, "Rename")) { renaming = n; renameBuf = n; armedDelete.clear(); focusRename = true; }
            ImGui::SameLine();
            const bool armed = armedDelete == n && ImGui::GetTime() < armedUntil;
            if (iconOnlyButton("##laydel", Icon::Delete, armed, bw, armed ? "Click again to delete" : "Delete")) {
                if (armed) { deleteLayout(dir, n); setStatus("Layout deleted: " + n); armedDelete.clear(); }
                else { armedDelete = n; armedUntil = ImGui::GetTime() + 3; }
            }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::End();
}

} // namespace osteon
