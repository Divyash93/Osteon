// The Settings dialog, laid out like Spine's: a page list on the left, rows of title + description + control on the right,
// Credits / Defaults / Save / Cancel along the bottom. Edits are made on a copy and applied by Save.
#include "App.h"
#include "Icons.h"
#include "Util.h"
#include <map>
#include <sstream>

namespace osteon {

namespace {

struct Draft {
    bool light = false;
    bool autosave = true;
    int autosaveSeconds = 30;
    bool audio = true;
    bool splash = true;
    int defaultFps = 30;
    int dopeRow = 23;
    int treeIndent = 14;
    Color bgA{0.318f, 0.318f, 0.318f, 1}, bgB{0.337f, 0.337f, 0.337f, 1};
    bool axes = true, imageNames = false, rulers = false;
    bool selBones = true, selImages = true, selOthers = true;
    bool ghosting = false;
    int ghostFrames = 3, ghostStep = 2;
    float ghostAlpha = 0.35f;
    int panelOpacity = 80;
    bool autoKeyDefault = false;
    ImVec4 meshDot{1.0f, 1.0f, 1.0f, 0.90f}, meshVertex{1.0f, 0.78f, 0.16f, 1.0f};
};

enum Page { kFiles, kGeneral, kInterface, kViewport, kDopesheet, kPageCount };

const ImVec4 kTeal(0.53f, 0.82f, 0.86f, 1);

// True while the canvas colours are still one of the two themes' defaults (so a theme change may swap them).
bool isThemeDefaultBg(const Color& a, const Color& b) {
    auto isNear = [](const Color& c, float r, float g, float bl) { return std::fabs(c.r - r) < 0.003f && std::fabs(c.g - g) < 0.003f && std::fabs(c.b - bl) < 0.003f; };
    return (isNear(a, 0.318f, 0.318f, 0.318f) && isNear(b, 0.337f, 0.337f, 0.337f)) || (isNear(a, 0.74f, 0.74f, 0.74f) && isNear(b, 0.79f, 0.79f, 0.79f));
}

} // namespace

// ---- persistence (settings.txt: key value per line) ----

void Editor::loadSettingsFile() {
    std::string text;
    if (!readTextFile(joinPath(appDataDir(), "settings.txt"), text)) return;
    std::map<std::string, std::string> kv;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        size_t sp = line.find(' ');
        if (sp != std::string::npos) kv[line.substr(0, sp)] = line.substr(sp + 1);
    }
    auto num = [&](const char* key, float def) { auto it = kv.find(key); if (it == kv.end()) return def; try { return std::stof(it->second); } catch (...) { return def; } };
    auto flag = [&](const char* key, bool def) { return num(key, def ? 1.0f : 0.0f) != 0; };
    autosaveEnabled = flag("autosave", autosaveEnabled);
    autosaveSeconds = std::max(5, std::min(600, (int)num("autosaveSeconds", (float)autosaveSeconds)));
    audioEnabled = flag("audio", audioEnabled);
    splash.enabled = flag("splash", splash.enabled);
    Document::defaultFps = std::max(1, std::min(240, (int)num("defaultFps", (float)Document::defaultFps)));
    dopeRowHeight = std::max(14, std::min(60, (int)num("dopeRow", (float)dopeRowHeight)));
    treeIndent = std::max(4, std::min(40, (int)num("treeIndent", (float)treeIndent)));
    showAxes = flag("axes", showAxes); showImageNames = flag("imageNames", showImageNames); showRulers = flag("rulers", showRulers);
    selectBones = flag("selBones", selectBones); selectImages = flag("selImages", selectImages); selectOthers = flag("selOthers", selectOthers);
    ghosting = flag("ghosting", ghosting);
    ghostFrames = std::max(1, std::min(8, (int)num("ghostFrames", (float)ghostFrames)));
    ghostStep = std::max(1, std::min(10, (int)num("ghostStep", (float)ghostStep)));
    ghostAlpha = std::max(0.05f, std::min(1.0f, num("ghostAlpha", ghostAlpha)));
    panelOpacity = std::max(20, std::min(100, (int)num("panelOpacity", (float)panelOpacity)));
    autoKeyDefault = num("autoKeyDefault", autoKeyDefault ? 1.f : 0.f) != 0;
    if (kv.count("bgAr")) bgA = Color(num("bgAr", bgA.r), num("bgAg", bgA.g), num("bgAb", bgA.b), 1);
    if (kv.count("bgBr")) bgB = Color(num("bgBr", bgB.r), num("bgBg", bgB.g), num("bgBb", bgB.b), 1);
}

void Editor::saveSettingsFile() const {
    std::ostringstream o;
    o << "autosave " << autosaveEnabled << "\nautosaveSeconds " << autosaveSeconds << "\naudio " << audioEnabled << "\nsplash " << splash.enabled << "\ndefaultFps " << Document::defaultFps
      << "\ndopeRow " << dopeRowHeight << "\ntreeIndent " << treeIndent << "\naxes " << showAxes << "\nimageNames " << showImageNames << "\nrulers " << showRulers
      << "\nselBones " << selectBones << "\nselImages " << selectImages << "\nselOthers " << selectOthers << "\nghosting " << ghosting
      << "\nghostFrames " << ghostFrames << "\nghostStep " << ghostStep << "\nghostAlpha " << ghostAlpha << "\npanelOpacity " << panelOpacity << "\nautoKeyDefault " << (autoKeyDefault ? 1 : 0)
      << "\nbgAr " << bgA.r << "\nbgAg " << bgA.g << "\nbgAb " << bgA.b << "\nbgBr " << bgB.r << "\nbgBg " << bgB.g << "\nbgBb " << bgB.b << "\n";
    writeTextFile(joinPath(appDataDir(), "settings.txt"), o.str());
}

void Editor::applyTreeIndent() { ImGui::GetStyle().IndentSpacing = treeIndent * uiScale; }

// ---- the dialog ----

void Editor::settingsDialog() {
    ImGui::SetNextWindowSize(ImVec2(820 * uiScale, 620 * uiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(700 * uiScale, 460 * uiScale), ImVec2(1e9f, 1e9f));
    if (!ImGui::Begin("Settings", &showSettings, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) { ImGui::End(); return; }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Settings");
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 30 * uiScale);
    if (iconOnlyButton("##setclose", Icon::Delete, false, 24 * uiScale, "Close")) showSettings = false;
    ImGui::Separator();
    static Draft d;
    static bool open = false;
    static int page = kInterface;
    if (!open) { // a fresh copy of the current settings each time the dialog opens
        open = true;
        d.light = gLightUi; d.autosave = autosaveEnabled; d.autosaveSeconds = autosaveSeconds; d.audio = audioEnabled; d.splash = splash.enabled;
        d.defaultFps = Document::defaultFps; d.dopeRow = dopeRowHeight; d.treeIndent = treeIndent; d.bgA = bgA; d.bgB = bgB;
        d.axes = showAxes; d.imageNames = showImageNames; d.rulers = showRulers;
        d.selBones = selectBones; d.selImages = selectImages; d.selOthers = selectOthers;
        d.ghosting = ghosting; d.ghostFrames = ghostFrames; d.ghostStep = ghostStep; d.ghostAlpha = ghostAlpha; d.panelOpacity = panelOpacity; d.autoKeyDefault = autoKeyDefault; d.meshDot = meshDotColor; d.meshVertex = meshSelColor;
    }
    const float u = uiScale;

    // ---- page list ----
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18 * u, 14 * u));
    ImGui::BeginChild("##setpages", ImVec2(170 * u, -40 * u), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    auto group = [&](const char* title) { ImGui::Dummy(ImVec2(0, 3 * u)); ImGui::TextUnformatted(title); };
    auto entry = [&](const char* title, int p, float indent) {
        ImGui::Indent(indent * u);
        if (ImGui::Selectable(title, page == p)) page = p;
        ImGui::Unindent(indent * u);
    };
    group("Application");
    entry("Files", kFiles, 12); entry("General", kGeneral, 12); entry("User interface", kInterface, 12);
    entry("Viewport", kViewport, 0);
    group("Behavior");
    entry("Dopesheet", kDopesheet, 12);
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- rows ----
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28 * u, 16 * u));
    ImGui::BeginChild("##setrows", ImVec2(0, -40 * u), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    static const char* titles[] = {"Files", "General", "User interface", "Viewport", "Dopesheet"};
    ImGui::TextColored(kTeal, "%s", titles[page]);
    ImGui::Separator();
    const float controlX = std::max(300.0f * u, ImGui::GetContentRegionAvail().x - 228 * u);
    auto row = [&](const char* title, const char* desc, auto control) {
        ImGui::Dummy(ImVec2(0, 3 * u));
        ImVec2 top = ImGui::GetCursorScreenPos();
        ImGui::TextUnformatted(title);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.74f, 0.76f, 1));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + controlX - 16 * u);
        ImGui::TextWrapped("%s", desc);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImVec2 after = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(top.x + controlX, top.y));
        ImGui::SetNextItemWidth(190 * u);
        control();
        if (ImGui::GetCursorScreenPos().y < after.y) ImGui::SetCursorScreenPos(after);
        ImGui::Dummy(ImVec2(0, 2 * u));
    };
    auto check = [&](const char* id, bool& v) { ImGui::Checkbox(id, &v); };

    switch (page) {
    case kFiles:
        row("Auto-save", "Keeps a recovery copy of unsaved work. If Osteon closes unexpectedly it offers to restore it the next time it starts.", [&] { check("##autosave", d.autosave); });
        row("Auto-save interval", "Seconds between recovery copies while there are unsaved changes.", [&] { ImGui::BeginDisabled(!d.autosave); sliderInt("##autosecs", &d.autosaveSeconds, 5, 600, "%d"); ImGui::EndDisabled(); });
        row("Settings folder", "Where Osteon keeps its settings, layouts, recovery copies and crash reports.", [&] { if (ImGui::Button("Open folder", ImVec2(190 * u, 0))) openInExplorer(appDataDir()); });
        break;
    case kGeneral:
        row("Play event audio", "Play an event's audio file when the playhead passes its key during playback in Animate mode.", [&] { check("##audio", d.audio); });
        row("Splash screen", "Show the Osteon splash screen when the program starts. It stays until you click the empty area around it.", [&] { check("##splash", d.splash); });
        row("Window layout", "Puts every panel back where it started for the current mode.", [&] { if (ImGui::Button("Reset layout", ImVec2(190 * u, 0))) resetLayout = resetToolbarPanels = true; });
        break;
    case kInterface:
        row("Theme", "The colour scheme of the user interface. The canvas checkerboard follows it unless you change it in Viewport.", [&] {
            static const char* themes[] = {"Dark", "Light"};
            int t = d.light ? 1 : 0;
            if (ImGui::BeginCombo("##theme", themes[t])) {
                for (int i = 0; i < 2; i++) if (ImGui::Selectable(themes[i], t == i)) d.light = i == 1;
                ImGui::EndCombo();
            }
        });
        row("Default timeline FPS", "The timeline frames per second to use for new projects.", [&] { sliderInt("##fps", &d.defaultFps, 1, 120, "%d"); });
        row("Auto Key on by default", "Whether Auto Key is on when a project is created or opened. Off by default; you can still switch it on or off any time from the timeline.", [&] { check("##autokeydef", d.autoKeyDefault); });
        row("Panel opacity", "How opaque the floating toolbar at the bottom of the viewport is, in percent.", [&] { sliderInt("##panelop", &d.panelOpacity, 20, 100, "%d%%"); });
        row("Mesh dots", "The colour of a mesh's dots in the viewport.", [&] { ImGui::ColorEdit4("##meshdot", &d.meshDot.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf); });
        row("Mesh vertices", "The colour of the selected vertices while editing a mesh.", [&] { ImGui::ColorEdit4("##meshvtx", &d.meshVertex.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf); });
        row("Tree indentation", "The amount in pixels to indent nodes in the tree view hierarchy.", [&] { sliderInt("##indent", &d.treeIndent, 4, 40, "%d"); });
        break;
    case kViewport:
        row("Background A", "The first colour of the viewport checkerboard.", [&] { float c[3] = {d.bgA.r, d.bgA.g, d.bgA.b}; if (ImGui::ColorEdit3("##bga", c, ImGuiColorEditFlags_NoInputs)) d.bgA = Color(c[0], c[1], c[2], 1); });
        row("Background B", "The second colour of the viewport checkerboard.", [&] { float c[3] = {d.bgB.r, d.bgB.g, d.bgB.b}; if (ImGui::ColorEdit3("##bgb", c, ImGuiColorEditFlags_NoInputs)) d.bgB = Color(c[0], c[1], c[2], 1); });
        row("Show axes", "Draw the world axes through the origin.", [&] { check("##axes", d.axes); });
        row("Rulers", "Show rulers along the viewport edges (Views > Rulers).", [&] { check("##rulers", d.rulers); });
        row("Image names", "Always draw the names of images in the viewport.", [&] { check("##imgnames", d.imageNames); });
        row("Selectable bones", "Bones can be picked by clicking them in the viewport.", [&] { check("##selb", d.selBones); });
        row("Selectable images", "Images can be picked by clicking them in the viewport.", [&] { check("##seli", d.selImages); });
        row("Selectable others", "Bounding boxes, paths, points and clipping can be picked in the viewport.", [&] { check("##selo", d.selOthers); });
        row("Ghosting", "Show faded copies of the animation's neighbouring frames (onion skin, I).", [&] { check("##ghost", d.ghosting); });
        row("Ghost frames each side", "How many frames before and after the current one are drawn.", [&] { sliderInt("##ghostn", &d.ghostFrames, 1, 8, "%d"); });
        row("Ghost frame step", "Frames between ghosts.", [&] { sliderInt("##ghosts", &d.ghostStep, 1, 10, "%d"); });
        row("Ghost opacity", "How visible the ghosts are.", [&] { sliderFloat("##ghosta", &d.ghostAlpha, 0.05f, 1, "%.2f"); });
        break;
    case kDopesheet:
        row("Row height", "The height of each row in the dopesheet, in pixels.", [&] { sliderInt("##rowh", &d.dopeRow, 14, 60, "%d"); });
        break;
    default: break;
    }
    ImGui::EndChild();

    // ---- bottom bar ----
    ImGui::Separator();
    if (iconButton("##setcredits", Icon::Question, "Credits", false, ImVec2(98 * u, 26 * u))) showAbout = true;
    ImGui::SameLine();
    if (iconButton("##setdefaults", Icon::Sync, "Defaults", false, ImVec2(98 * u, 26 * u))) {
        Draft def;
        def.light = false;
        d = def;
        d.light = gLightUi; // the theme is not reset by Defaults: it would flip the whole window under the user
    }
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - (2 * 98 + 8) * u);
    bool save = iconButton("##setsave", Icon::Save, "Save", false, ImVec2(98 * u, 26 * u));
    ImGui::SameLine();
    bool cancel = iconButton("##setcancel", Icon::Delete, "Cancel", false, ImVec2(98 * u, 26 * u));
    ImGui::End();

    if (!showSettings) cancel = true; // closing the window discards the edits
    if (save) {
        const bool themeChanged = d.light != gLightUi;
        autosaveEnabled = d.autosave; autosaveSeconds = d.autosaveSeconds; audioEnabled = d.audio; splash.enabled = d.splash;
        Document::defaultFps = d.defaultFps; dopeRowHeight = d.dopeRow; treeIndent = d.treeIndent;
        bgA = d.bgA; bgB = d.bgB;
        showAxes = d.axes; showRulers = d.rulers; showImageNames = d.imageNames;
        selectBones = d.selBones; selectImages = d.selImages; selectOthers = d.selOthers;
        ghosting = d.ghosting; ghostFrames = d.ghostFrames; ghostStep = d.ghostStep; ghostAlpha = d.ghostAlpha; panelOpacity = d.panelOpacity; autoKeyDefault = d.autoKeyDefault; doc.autoKey = autoKeyDefault; meshDotColor = d.meshDot; meshSelColor = d.meshVertex;
        if (themeChanged) {
            gLightUi = d.light;
            applyUiTheme(uiScale, gLightUi);
            if (isThemeDefaultBg(d.bgA, d.bgB)) {
                if (gLightUi) { bgA = Color(0.74f, 0.74f, 0.74f, 1); bgB = Color(0.79f, 0.79f, 0.79f, 1); }
                else { bgA = Color(0.318f, 0.318f, 0.318f, 1); bgB = Color(0.337f, 0.337f, 0.337f, 1); }
            }
            writeTextFile(joinPath(appDataDir(), "ui_theme.txt"), gLightUi ? "light" : "dark");
        }
        applyTreeIndent();
        saveSettingsFile();
        setStatus("Settings saved");
    }
    if (save || cancel) { open = false; showSettings = false; }
}

} // namespace osteon
