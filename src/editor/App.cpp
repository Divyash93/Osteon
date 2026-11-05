#include "App.h"
#include "Audio.h"
#include "Geometry.h"
#include "Icons.h"
#include <sstream>
#include "imgui_internal.h"

namespace osteon {

// ---- widget helpers (undo-aware) ----

static void trackEdit(Document& doc, const char* label) {
    if (ImGui::IsItemActivated()) doc.beginEdit(label);
    if (ImGui::IsItemDeactivatedAfterEdit()) doc.commitEdit();
    else if (ImGui::IsItemDeactivated()) doc.cancelEdit();
}

// Numeric fields are typed: click, enter a value. The speed argument is kept so call sites stay the same.
bool dragFloatEdit(Document& doc, const char* label, float* v, float speed, const char* fmt, const char* undoLabel) {
    (void)speed;
    bool changed = ImGui::InputFloat(label, v, 0, 0, fmt);
    trackEdit(doc, undoLabel);
    if (changed) doc.dirty = true;
    return changed;
}

bool sliderFloatEdit(Document& doc, const char* label, float* v, float lo, float hi, const char* fmt, const char* undoLabel) {
    bool changed = sliderFloat(label, v, lo, hi, fmt);
    if (sliderActivated()) doc.beginEdit(undoLabel);
    if (sliderDeactivated()) { if (sliderDeactivatedAfterEdit()) doc.commitEdit(); else doc.cancelEdit(); }
    if (changed) doc.dirty = true;
    return changed;
}

bool dragFloat2Edit(Document& doc, const char* label, float* v, float speed, const char* fmt, const char* undoLabel) {
    (void)speed;
    bool changed = ImGui::InputFloat2(label, v, fmt);
    trackEdit(doc, undoLabel);
    if (changed) doc.dirty = true;
    return changed;
}

bool colorEdit(Document& doc, const char* label, Color& c, bool alpha, const char* undoLabel) {
    float f[4] = {c.r, c.g, c.b, c.a};
    bool changed = alpha ? ImGui::ColorEdit4(label, f, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)
                         : ImGui::ColorEdit3(label, f, ImGuiColorEditFlags_NoInputs);
    trackEdit(doc, undoLabel);
    if (changed) { c = Color(f[0], f[1], f[2], alpha ? f[3] : c.a); doc.dirty = true; }
    return changed;
}

bool checkboxEdit(Document& doc, const char* label, bool* v, const char* undoLabel) {
    bool old = *v;
    if (ImGui::Checkbox(label, v)) {
        bool nv = *v;
        *v = old;
        doc.checkpoint(undoLabel);
        *v = nv;
        return true;
    }
    return false;
}

void helpMarker(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// ---- lifecycle ----

void Editor::init() {
    doc.newProject();
    doc.loadRecent();
    {
        std::string st;
        loadKeyRemap();
        if (readTextFile(joinPath(appDataDir(), "autosave_settings.txt"), st)) { std::istringstream in(st); int e = 1, sec = 30; if (in >> e >> sec) { autosaveEnabled = e != 0; autosaveSeconds = std::max(5, std::min(600, sec)); } }
    }
    checkRecovery();
    if (gLightUi) { bgA = Color(0.74f, 0.74f, 0.74f, 1); bgB = Color(0.79f, 0.79f, 0.79f, 1); } // the canvas checkerboard follows the theme
    loadSettingsFile();
    doc.autoKey = autoKeyDefault;
    applyTreeIndent();
    for (Mode m : {Mode::Setup, Mode::Animate}) readTextFile(layoutFile(m), layoutIni[(int)m]);
    const PanelVis& v = panelVis[(int)Mode::Setup];
    showDopesheet = v.dopesheet; showGraph = v.graph; showAnimations = v.animations; showPreview = v.preview;
    if (!layoutIni[(int)Mode::Setup].empty()) loadLayoutIni(layoutIni[(int)Mode::Setup]);
    resetLayout = layoutIni[(int)Mode::Setup].empty();
    previewState = AnimationState(&doc.data);
    // Mesh colours: four lines of "r g b a".
    std::string mc;
    if (readTextFile(joinPath(appDataDir(), "mesh_colors.txt"), mc)) {
        std::istringstream in(mc);
        ImVec4* cols[4] = {&meshDotColor, &meshSelColor, &meshTriColor, &meshHullColor};
        for (ImVec4* c : cols) { float r, g, b, a; if (in >> r >> g >> b >> a) *c = ImVec4(r, g, b, a); }
        meshTriColor = ImVec4(1.0f, 1.0f, 1.0f, 0.47f); meshHullColor = ImVec4(0.27f, 0.82f, 0.92f, 1.0f); // only the dot and vertex colours are user settings
    }
    std::string tl;
    if (readTextFile(joinPath(appDataDir(), "toolbar_layout.txt"), tl)) {
        std::istringstream in(tl);
        std::string ver;
        int locked = 1;
        // "v2": positions saved only for panels moved by hand. Older files held stale default positions, so they are ignored.
        if ((in >> ver) && ver == "v2" && (in >> locked)) {
            toolbarLocked = locked != 0;
            for (ImVec2& p : tbPos) { float x, y; if (in >> x >> y) { p = ImVec2(x, y); if (x >= 0) tbCustom = true; } }
        }
    }
}

std::string Editor::layoutFile(Mode m) const {
    return joinPath(appDataDir(), m == Mode::Setup ? "layout_setup.ini" : "layout_animate.ini");
}

void Editor::beforeNewFrame() {
    if (doc.mode == layoutMode) return;
    int o = (int)layoutMode, n = (int)doc.mode;
    layoutIni[o] = ImGui::SaveIniSettingsToMemory();
    panelVis[o] = {showDopesheet, showGraph, showAnimations, showPreview};
    const PanelVis& v = panelVis[n];
    showDopesheet = v.dopesheet; showGraph = v.graph; showAnimations = v.animations; showPreview = v.preview;
    if (!layoutIni[n].empty()) loadLayoutIni(layoutIni[n]);
    else resetLayout = true;
    layoutMode = doc.mode;
}

void Editor::shutdown() {
    splash.release();
    clearAutosave(); // a normal exit: nothing left to recover
    writeTextFile(joinPath(appDataDir(), "autosave_settings.txt"), std::to_string(autosaveEnabled ? 1 : 0) + " " + std::to_string(autosaveSeconds));
    layoutIni[(int)layoutMode] = ImGui::SaveIniSettingsToMemory();
    for (Mode m : {Mode::Setup, Mode::Animate})
        if (!layoutIni[(int)m].empty()) writeTextFile(layoutFile(m), layoutIni[(int)m]);
    {
        std::ostringstream out;
        for (const ImVec4* c : {&meshDotColor, &meshSelColor, &meshTriColor, &meshHullColor}) out << c->x << ' ' << c->y << ' ' << c->z << ' ' << c->w << '\n';
        writeTextFile(joinPath(appDataDir(), "mesh_colors.txt"), out.str());
    }
    {
        std::ostringstream out;
        out << "v2 " << (toolbarLocked ? 1 : 0);
        for (const ImVec2& p : tbPos) out << ' ' << (tbCustom ? p.x : -1.0f) << ' ' << (tbCustom ? p.y : -1.0f);
        writeTextFile(joinPath(appDataDir(), "toolbar_layout.txt"), out.str());
    }
    viewRT.release();
    previewRT.release();
    exportRT.release();
    gfx.shutdown();
}

// ---- Auto-save and crash recovery ----

static std::string recoveryDir() { std::string d = joinPath(appDataDir(), "recovery"); makeDirs(d); return d; }

bool Editor::autosaveNow() {
    if (!doc.dirty) return false;
    std::string dir = recoveryDir(), err;
    if (!doc.writeBackup(joinPath(dir, "autosave.osteon"), err)) return false;
    SYSTEMTIME t;
    GetLocalTime(&t);
    char when[64];
    std::snprintf(when, sizeof when, "%04d-%02d-%02d %02d:%02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    writeTextFile(joinPath(dir, "autosave.txt"), doc.projectPath + "\n" + when + "\n");
    return true;
}

void Editor::clearAutosave() {
    std::string dir = recoveryDir();
    for (const char* f : {"autosave.osteon", "autosave.txt"}) DeleteFileW(widen(joinPath(dir, f)).c_str());
}

void Editor::autosaveTick() {
    if (!autosaveEnabled || recovery.pending) return; // do not overwrite the copy that is waiting to be restored
    double now = ImGui::GetTime();
    if (now - lastAutosave < std::max(5, autosaveSeconds)) return;
    lastAutosave = now;
    if (doc.dirty) autosaveNow();
    else clearAutosave(); // everything is saved: nothing to recover
}

void Editor::checkRecovery() {
    std::string dir = recoveryDir(), meta;
    if (!fileExists(joinPath(dir, "autosave.osteon")) || !readTextFile(joinPath(dir, "autosave.txt"), meta)) return;
    size_t nl = meta.find('\n');
    recovery.original = meta.substr(0, nl);
    while (!recovery.original.empty() && (recovery.original.back() == '\r' || recovery.original.back() == '\n')) recovery.original.pop_back();
    if (nl != std::string::npos) {
        recovery.when = meta.substr(nl + 1);
        while (!recovery.when.empty() && (recovery.when.back() == '\r' || recovery.when.back() == '\n')) recovery.when.pop_back();
    }
    recovery.pending = true;
}

void Editor::recoveryDialog() {
    if (!recovery.pending || splash.active()) return; // the restore question waits until the splash is gone
    ImGui::OpenPopup("Restore unsaved work?");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Restore unsaved work?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted(tr("Osteon did not close properly last time."));
        std::string name = recovery.original.empty() ? "an untitled project" : "\"" + fileName(recovery.original) + "\"";
        ImGui::Text("An auto-saved copy of %s from %s was found.", name.c_str(), recovery.when.c_str());
        ImGui::Spacing();
        if (ImGui::Button(tr("Restore it"), ImVec2(130 * uiScale, 0))) {
            std::string err;
            if (doc.restoreBackup(joinPath(recoveryDir(), "autosave.osteon"), recovery.original, err)) {
                gfx.reloadImages();
                previewState = AnimationState(&doc.data);
                fitRequested = true;
                setStatus("Restored the auto-saved copy. Save it (Ctrl+S) to keep it.");
            } else setStatus("Could not restore the auto-saved copy: " + err);
            recovery.pending = false;
            lastAutosave = ImGui::GetTime();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr("Discard it"), ImVec2(130 * uiScale, 0))) {
            clearAutosave();
            recovery.pending = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Editor::setStatus(const std::string& s, double seconds) {
    status = s;
    statusUntil = ImGui::GetTime() + seconds;
}

bool Editor::confirmDiscard() {
    if (!doc.dirty) return true;
    int r = MessageBoxW(hwnd, L"The project has unsaved changes. Save before continuing?", L"Osteon", MB_YESNOCANCEL | MB_ICONWARNING);
    if (r == IDCANCEL) return false;
    if (r == IDYES) return cmdSave();
    return true;
}

void Editor::openPath(const std::string& path) {
    std::string ext = extension(path);
    if (dirExists(path)) { // a dropped folder of images
        doc.imagesDir = normalizePath(path);
        int n = doc.importImagesAsSlots();
        setStatus("Images folder set: " + path + (n ? " (" + std::to_string(n) + " slots created)" : ""));
        fitRequested = true;
        return;
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
        // A single dropped image: use its folder, then create a slot for it.
        doc.imagesDir = parentDir(path);
        doc.scanImages();
        doc.checkpoint("Import image");
        doc.updatePose();
        int si = doc.createSlot(0, stripExtension(fileName(path)));
        doc.addRegionAttachment(si, stripExtension(fileName(path)), {camX, camY});
        doc.selection = {{SelType::Slot, si, ""}};
        return;
    }
    if (!confirmDiscard()) return;
    std::string err;
    if (doc.openFile(path, err)) {
        gfx.reloadImages();
        previewState = AnimationState(&doc.data);
        doc.autoKey = autoKeyDefault;
        fitRequested = true;
        setStatus("Opened " + path + (doc.warnings.empty() ? "" : " (" + std::to_string(doc.warnings.size()) + " warnings, see Problems)"));
        if (!doc.warnings.empty()) showProblems = true;
    } else {
        MessageBoxW(hwnd, widen("Could not open file:\n" + err).c_str(), L"Osteon", MB_ICONERROR);
    }
}

void Editor::cmdNew() {
    if (!confirmDiscard()) return;
    doc.newProject();
    doc.autoKey = autoKeyDefault;
    gfx.reloadImages();
    camX = camY = 0;
    zoom = 1;
    setStatus("New project");
}

void Editor::cmdOpen() {
    std::string p = openFileDialog(hwnd, L"Osteon / Spine JSON\0*.ost;*.osteon;*.json\0All files\0*.*\0", L"Open Project");
    if (!p.empty()) openPath(p);
}

bool Editor::cmdSave() {
    if (doc.projectPath.empty()) return cmdSaveAs();
    std::string err;
    if (!doc.saveProject(doc.projectPath, err)) {
        MessageBoxW(hwnd, widen(err).c_str(), L"Osteon", MB_ICONERROR);
        return false;
    }
    setStatus("Saved " + doc.projectPath);
    return true;
}

bool Editor::cmdSaveAs() {
    std::string suggested = doc.projectPath.empty() ? (doc.imagesDir.empty() ? "skeleton.ost" : joinPath(parentDir(doc.imagesDir), "skeleton.ost"))
                                                    : doc.projectPath;
    std::string p = saveFileDialog(hwnd, L"Osteon project\0*.ost\0", L"ost", suggested, L"Save Project As");
    if (p.empty()) return false;
    std::string err;
    if (!doc.saveProject(p, err)) {
        MessageBoxW(hwnd, widen(err).c_str(), L"Osteon", MB_ICONERROR);
        return false;
    }
    setStatus("Saved " + p);
    return true;
}

void Editor::cmdImportImagesFolder() {
    std::string dir = pickFolderDialog(hwnd, L"Choose images folder");
    if (dir.empty()) return;
    doc.imagesDir = dir;
    gfx.reloadImages();
    int n = doc.importImagesAsSlots();
    setStatus(std::to_string(doc.imageNames.size()) + " images found, " + std::to_string(n) + " new slots created");
    fitRequested = true;
}

void Editor::cmdImportData() {
    std::string p = openFileDialog(hwnd, L"Spine JSON\0*.json\0All files\0*.*\0", L"Import Data");
    if (!p.empty()) openPath(p);
}

void Editor::cmdImportImages(bool multiple) {
    const wchar_t* filter = L"Images *.png;*.jpg;*.jpeg;*.bmp All files *.* ";
    std::vector<std::string> files;
    if (multiple) files = openFilesDialog(hwnd, filter, L"Import Images");
    else { std::string p = openFileDialog(hwnd, filter, L"Import Image"); if (!p.empty()) files.push_back(p); }
    if (files.empty()) return;
    std::string err;
    int n = doc.importImageFiles(files, {camX, camY}, err);
    if (n < 0) {
        MessageBoxW(hwnd, widen("Could not import:\n" + err).c_str(), L"Osteon", MB_ICONERROR);
        return;
    }
    gfx.reloadImages();
    previewState = AnimationState(&doc.data);
    setStatus(n == 1 ? "Imported " + fileName(files[0]) : "Imported " + std::to_string(n) + " images");
}

void Editor::cmdImportPsd() {
    std::string p = openFileDialog(hwnd, L"Photoshop\0*.psd;*.psb\0All files\0*.*\0", L"Import PSD");
    if (p.empty()) return;
    std::string err;
    int hidden = 0;
    int n = doc.importPsd(p, err, &hidden);
    if (n < 0) {
        MessageBoxW(hwnd, widen("Could not import PSD:\n" + err).c_str(), L"Osteon", MB_ICONERROR);
        return;
    }
    gfx.reloadImages();
    previewState = AnimationState(&doc.data);
    fitRequested = true;
    setStatus("Imported " + std::to_string(n) + " layers from " + fileName(p) + (hidden ? " (" + std::to_string(hidden) + " hidden skipped)" : ""));
}

void Editor::zoomToFit() {
    doc.updatePose();
    float x0, y0, x1, y1;
    if (!doc.sk.bounds(x0, y0, x1, y1)) {
        x0 = y0 = -200; x1 = y1 = 200;
        for (auto& b : doc.sk.bones) {
            x0 = std::min(x0, b.worldX); x1 = std::max(x1, b.worldX);
            y0 = std::min(y0, b.worldY); y1 = std::max(y1, b.worldY);
        }
    }
    camX = (x0 + x1) / 2;
    camY = (y0 + y1) / 2;
    float w = std::max(1.0f, x1 - x0), h = std::max(1.0f, y1 - y0);
    if (vpSize.x > 10 && vpSize.y > 10) zoom = std::min(vpSize.x / w, vpSize.y / h) * 0.85f;
    zoom = clampf(zoom, 0.01f, 50);
}

void Editor::zoom100() { zoom = 1; }

RenderOptions Editor::renderOptions() {
    RenderOptions o;
    o.bgA = bgA;
    o.bgB = bgB;
    o.checker = 50; // world units: the checkerboard zooms with the view
    o.checkerWorld = true;
    o.slotVisible = [this](int s) { return slotVisible(s); };
    return o;
}

bool Editor::boneVisible(int b) const {
    // A hidden bone is not drawn or picked, but its children and its images still are (hide the subtree with a right click on the eye).
    return b >= 0 && b < (int)doc.data.bones.size() && doc.data.bones[b].visible;
}

bool Editor::slotVisible(int s) const {
    if (s < 0 || s >= (int)doc.data.slots.size()) return false;
    if (meshIsolate && meshMode != MeshMode::Off) { // Isolate: only the attachment being edited stays visible
        for (auto it = doc.selection.rbegin(); it != doc.selection.rend(); ++it)
            if (it->type == SelType::Attachment) { if (it->index != s) return false; break; }
    }
    return showImages && doc.data.slots[s].visible;
}

Attachment* Editor::selectedAttachment(int* slotOut) {
    // The most recently selected attachment, so bones can be added to the selection (e.g. to bind weights).
    Sel p;
    for (auto it = doc.selection.rbegin(); it != doc.selection.rend(); ++it)
        if (it->type == SelType::Attachment) { p = *it; break; }
    if (p.type != SelType::Attachment) return nullptr;
    if (slotOut) *slotOut = p.index;
    for (int s : {doc.skin, 0})
        if (s >= 0 && s < (int)doc.data.skins.size())
            if (Attachment* a = doc.data.skins[s].get(p.index, p.name)) return a;
    return nullptr;
}

void Editor::beginRename() {
    Sel p = doc.primary();
    std::string n;
    switch (p.type) {
    case SelType::Bone: n = doc.data.bones[p.index].name; break;
    case SelType::Slot: n = doc.data.slots[p.index].name; break;
    case SelType::Attachment: n = p.name; break;
    case SelType::Ik: n = doc.data.ik[p.index].name; break;
    case SelType::Transform: n = doc.data.transforms[p.index].name; break;
    case SelType::Path: n = doc.data.paths[p.index].name; break;
    case SelType::Physics: n = doc.data.physics[p.index].name; break;
    case SelType::Slider: n = doc.data.sliders[p.index].name; break;
    case SelType::Skin: n = doc.data.skins[p.index].name; break;
    case SelType::Animation: n = doc.data.animations[p.index].name; break;
    case SelType::Event: n = doc.data.events[p.index].name; break;
    default: return;
    }
    renameBuffer = n;
    renaming = true;
}

void Editor::newAnimation() {
    int i = doc.createAnimation("animation");
    doc.setMode(Mode::Animate);
    doc.animation = i;
    doc.selection = {{SelType::Animation, i, ""}};
    beginRename(); // the Rename dialog: type a name and press Enter, or Esc to keep "animation"
}

// ---- frame ----

void Editor::dockLayout(unsigned dockId) {
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->WorkSize);
    ImGuiID main = dockId;
    ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.24f, nullptr, &main);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.30f, nullptr, &main);
    ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.42f, nullptr, &right);
    ImGui::DockBuilderDockWindow(trWindow("Viewport"), main);
    ImGui::DockBuilderDockWindow(trWindow("Properties"), rightBottom);
    ImGui::DockBuilderDockWindow(trWindow("Dopesheet"), bottom);
    ImGui::DockBuilderDockWindow(trWindow("Graph"), bottom);
    ImGui::DockBuilderDockWindow(trWindow("Animations"), rightBottom);
    ImGui::DockBuilderDockWindow(trWindow("Skins"), right);
    ImGui::DockBuilderDockWindow(trWindow("Events"), right);
    ImGui::DockBuilderDockWindow(trWindow("Mesh Tools"), rightBottom);
    ImGui::DockBuilderDockWindow(trWindow("Weights"), rightBottom);
    ImGui::DockBuilderDockWindow(trWindow("Preview"), bottom);
    ImGui::DockBuilderDockWindow(trWindow("Metrics"), rightBottom);
    ImGui::DockBuilderDockWindow(trWindow("Outline"), right);
    ImGui::DockBuilderDockWindow(trWindow("Tree"), right);
    ImGui::DockBuilderDockWindow(trWindow("Problems"), bottom);
    ImGui::DockBuilderDockWindow(trWindow("Properties"), rightBottom);
    ImGui::DockBuilderDockWindow(trWindow("Dopesheet"), bottom);
    ImGui::DockBuilderFinish(dockId);
}

bool Editor::keyPressedMapped(ImGuiKey logical, bool repeat) const {
    auto it = keyRemap.find(logical);
    return ImGui::IsKeyPressed(it != keyRemap.end() ? it->second : logical, repeat);
}

static ImGuiKey keyFromName(const std::string& name) {
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++)
        if (name == ImGui::GetKeyName((ImGuiKey)k)) return (ImGuiKey)k;
    return ImGuiKey_None;
}

void Editor::loadKeyRemap() {
    keyRemap.clear();
    std::string text;
    if (!readTextFile(joinPath(appDataDir(), "hotkeys_remap.txt"), text)) return;
    std::istringstream in(text);
    std::string a, b;
    while (in >> a >> b) {
        ImGuiKey from = keyFromName(a), to = keyFromName(b);
        if (from != ImGuiKey_None && to != ImGuiKey_None && from != to) keyRemap[from] = to;
    }
}

void Editor::saveKeyRemap() const {
    std::string out;
    for (auto& [from, to] : keyRemap) out += std::string(ImGui::GetKeyName(from)) + " " + ImGui::GetKeyName(to) + "\n";
    writeTextFile(joinPath(appDataDir(), "hotkeys_remap.txt"), out);
}

void Editor::frame(float dt) {
    if (!doc.playing && wasPlaying) stopAllAudio();
    wasPlaying = doc.playing;
    if (!pendingOpen.empty()) {
        std::string p = pendingOpen;
        pendingOpen.clear();
        openPath(p);
    }

    // Playback advances time before posing.
    if (doc.playing && doc.mode == Mode::Animate) {
        float dur = doc.animationDuration();
        float start = doc.loopStart >= 0 ? doc.loopStart : 0, end = doc.loopEnd >= 0 ? doc.loopEnd : dur;
        float t = doc.time + dt * doc.speed * (doc.playBackward ? -1 : 1);
        if (end > start) {
            if (t > end) t = doc.loop ? start + std::fmod(t - start, end - start) : end;
            if (t < start) t = doc.loop ? end - std::fmod(start - t, end - start) : start;
            if (!doc.loop && (t == end || t == start)) doc.playing = false;
        } else t = 0;
        doc.clearPending();
        // Events with audio play as the playhead passes them (forward playback only).
        if (!doc.playBackward && audioEnabled)
            if (Animation* an = doc.currentAnimation())
                for (const FiredEvent& e : eventsBetween(doc.data, *an, doc.time, t, doc.loop)) {
                    if (e.event < 0) continue;
                    const EventData& ed = doc.data.events[e.event];
                    if (ed.audio.empty()) continue;
                    std::string path = ed.audio;
                    bool absolute = path.size() > 1 && (path[1] == ':' || path[0] == '/' || path[0] == '\\');
                    if (!absolute) {
                        std::string base = !doc.data.audioPath.empty() ? doc.data.audioPath : (doc.projectPath.empty() ? doc.imagesDir : parentDir(doc.projectPath));
                        if (!doc.data.audioPath.empty() && !(doc.data.audioPath.size() > 1 && doc.data.audioPath[1] == ':') && !doc.projectPath.empty())
                            base = joinPath(parentDir(doc.projectPath), doc.data.audioPath);
                        path = joinPath(base, path);
                    }
                    playAudioFile(path, e.volume);
                }
        doc.time = t;
    }
    doc.updatePose(doc.playing ? dt * doc.speed : 0);

    menuBar();
    ImGuiID dockId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None);
    static int focusDefaults = 0;
    if (resetLayout || !ImGui::DockBuilderGetNode(dockId)->IsSplitNode()) {
        dockLayout(dockId);
        resetLayout = false;
        focusDefaults = 2;
    }
    shortcuts();

    viewport();
    if (showTree) treeView();
    if (showProps) propertiesView();
    if (showDopesheet) dopesheetView();
    if (showGraph) graphView();
    if (showAnimations) animationsView();
    if (showSkins) skinsView();
    if (showEvents) eventsView();
    if (showPreview) previewView(dt);
    if (showMetrics) metricsView();
    if (showOutline) outlineView();
    if (showProblems) problemsView();
    // Mesh editing and the Weights view exclude each other. Choosing Edit Mesh while Weights is active asks the user to
    // leave it first (Apply, or a double click on empty space); choosing Weights while editing a mesh just leaves Edit Mesh.
    if (doc.tool == Tool::Weights && meshMode != MeshMode::Off) {
        meshMode = MeshMode::Off; newHull.clear();
        if (prevFrameTool == Tool::Weights) {
            weightsNoticeUntil = ImGui::GetTime() + 6;
            setStatus("Leave the Weights view first to edit the mesh: click Apply in Weights, or double-click empty space");
        }
    }
    prevFrameTool = doc.tool;
    if (showMeshTools) meshToolsView();
    else if (meshMode != MeshMode::Off) { meshMode = MeshMode::Off; newHull.clear(); meshSelection.clear(); } // closing Mesh Tools leaves Edit Mesh
    if (showWeights) weightsView();
    if (showExport) exportDialog();
    if (showPacker) packerDialog();
    if (showUnpacker) unpackerDialog();
    if (showLayouts) layoutsDialog();
    deleteDialog();
    recoveryDialog();
    autosaveTick();
    if (showSettings) settingsDialog();
    if (showAbout) aboutDialog();
    if (showHotkeys) hotkeysDialog();
    if (focusDefaults > 0 && --focusDefaults == 0) { // bring the primary tab of each dock area to the front
        ImGui::SetWindowFocus(trWindow("Tree"));
        ImGui::SetWindowFocus(trWindow("Properties"));
        ImGui::SetWindowFocus(trWindow("Dopesheet"));
        ImGui::SetWindowFocus(trWindow("Viewport"));
    }

    // Rename popup (F2).
    if (renaming) { ImGui::OpenPopup("Rename"); renaming = false; }
    if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char buf[256];
        if (ImGui::IsWindowAppearing()) {
            strncpy_s(buf, renameBuffer.c_str(), _TRUNCATE);
            ImGui::SetKeyboardFocusHere();
        }
        bool enter = ImGui::InputText("##name", buf, sizeof buf, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (enter || ImGui::Button(tr("OK"), ImVec2(100, 0))) {
            if (!doc.renameSelection(buf)) setStatus("Name is empty or already used");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr("Cancel"), ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Status line (bottom-left overlay).
    if (!status.empty() && ImGui::GetTime() < statusUntil) {
        ImVec2 p = ImGui::GetMainViewport()->WorkPos;
        ImVec2 s = ImGui::GetMainViewport()->WorkSize;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 ts = ImGui::CalcTextSize(status.c_str());
        ImVec2 a(p.x + 12, p.y + s.y - ts.y - 16);
        dl->AddRectFilled(ImVec2(a.x - 6, a.y - 4), ImVec2(a.x + ts.x + 6, a.y + ts.y + 4), (theme::hex(theme::kPanel) & 0x00FFFFFF) | (235u << 24), 3);
        dl->AddText(a, theme::hex(theme::kText), status.c_str());
    }

    // The start-up splash is drawn last so it sits over everything.
    splash.draw(gfx, uiScale, [](const char* url) { openUrl(url); });
}

void Editor::menuBar() {
    // The title bar: no borders, a dark slate bar with the logo, the menus, quick icons and the window buttons.
    const float u = uiScale;
    headerHeight = std::floor(26 * u);
    headerHot.clear();
    if (!headerLogo) {
        std::vector<uint8_t> bytes, px;
        HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(302), RT_RCDATA);
        if (r) {
            HGLOBAL g = LoadResource(nullptr, r);
            const uint8_t* p = g ? (const uint8_t*)LockResource(g) : nullptr;
            int w = 0, h = 0;
            if (p && decodeImageMemory(p, SizeofResource(nullptr, r), w, h, px)) headerLogo = gfx.createTexture(w, h, px.data());
        }
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4 * u, 0)); // in a menu bar this is the gap between the menu titles
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg)); // the theme's own grey: the bar blends into the window
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9 * u, (headerHeight - ImGui::GetFontSize()) / 2)); // sets the bar's height
    bool barOpen = ImGui::BeginMainMenuBar();
    ImGui::PopStyleVar();
    if (!barOpen) { ImGui::PopStyleColor(); ImGui::PopStyleVar(3); return; }
    if (headerLogo) { // the app icon at the left
        ImVec2 p = ImGui::GetMainViewport()->Pos;
        ImGui::GetWindowDrawList()->AddImage((ImTextureID)headerLogo, ImVec2(p.x + 8 * u, p.y + (headerHeight - 16 * u) / 2), ImVec2(p.x + 8 * u + 16 * u, p.y + (headerHeight + 16 * u) / 2));
    }
    ImGui::SetCursorPosX(30 * u);
    if (headerMenu(tr("File"))) {
        if (iconMenuItem(Icon::DocNew, tr("New Project"), "Ctrl+Shift+N")) cmdNew();
        if (iconMenuItem(Icon::Folder, tr("Open Project..."), "Ctrl+O")) cmdOpen();
        if (iconBeginMenu(Icon::Animations, tr("Recent Projects"), !doc.recentFiles.empty())) {
            for (auto& r : std::vector<std::string>(doc.recentFiles))
                if (ImGui::MenuItem(r.c_str())) openPath(r);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (iconMenuItem(Icon::Save, tr("Save Project"), "Ctrl+S")) cmdSave();
        if (iconMenuItem(Icon::SaveAs, tr("Save Project As..."), "Ctrl+Shift+S")) cmdSaveAs();
        ImGui::Separator();
        if (iconMenuItem(Icon::Import, tr("Import Data (Spine JSON)..."))) cmdImportData();
        if (iconMenuItem(Icon::Psd, tr("Import PSD..."))) cmdImportPsd();
        if (iconMenuItem(Icon::Slot, tr("Import Image..."))) cmdImportImages(false);
        if (iconMenuItem(Icon::ImagesFolder, tr("Import Multiple Images..."))) cmdImportImages(true);
        if (iconMenuItem(Icon::ImagesFolder, tr("Set Images Folder..."))) cmdImportImagesFolder();
        if (iconMenuItem(Icon::Slot, tr("Create Slots For Unused Images"), nullptr, false, !doc.imagesDir.empty())) {
            int n = doc.importImagesAsSlots();
            setStatus(std::to_string(n) + " slots created");
        }
        ImGui::Separator();
        if (iconMenuItem(Icon::Export, tr("Export..."), "Ctrl+E")) showExport = true;
        if (iconMenuItem(Icon::Atlas, tr("Texture Packer..."), "Ctrl+P")) showPacker = true;
        if (iconMenuItem(Icon::Atlas, tr("Texture Unpacker..."))) showUnpacker = true;
        ImGui::Separator();
        if (iconMenuItem(Icon::Gear, tr("Settings..."), "F12")) showSettings = true;
        ImGui::Separator();
        if (iconMenuItem(Icon::Exit, tr("Exit"), "Alt+F4")) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        headerMenuEnd();
    }
    if (headerMenu(tr("Edit"))) {
        std::string u = "Undo " + doc.undoLabel(), r = "Redo " + doc.redoLabel();
        if (iconMenuItem(Icon::Undo, u.c_str(), "Ctrl+Z", false, doc.canUndo())) doc.undo();
        if (iconMenuItem(Icon::Redo, r.c_str(), "Ctrl+Shift+Z", false, doc.canRedo())) doc.redo();
        ImGui::Separator();
        if (iconMenuItem(Icon::Pencil, tr("Rename"), "F2", false, !doc.selection.empty())) beginRename();
        if (iconMenuItem(Icon::Copy, tr("Duplicate"), "Ctrl+D", false, !doc.selection.empty())) doc.duplicateSelection();
        if (iconMenuItem(Icon::Delete, tr("Delete"), "Del", false, !doc.selection.empty())) requestDelete();
        if (iconMenuItem(Icon::DotOff, tr("Deselect"), "Esc")) doc.clearSelection();
        ImGui::Separator();
        if (iconMenuItem(Icon::Animations, tr("Setup / Animate Mode"), "Ctrl+Tab")) doc.setMode(doc.mode == Mode::Setup ? Mode::Animate : Mode::Setup);
        headerMenuEnd();
    }
    if (headerMenu(tr("Window"))) {
        ImGui::MenuItem(tr("Tree"), "Alt+T", &showTree);
        ImGui::MenuItem(tr("Properties"), nullptr, &showProps);
        ImGui::MenuItem(tr("Dopesheet"), "Alt+D", &showDopesheet);
        ImGui::MenuItem(tr("Graph"), "Alt+G", &showGraph);
        ImGui::MenuItem(tr("Animations"), "Alt+A", &showAnimations);
        ImGui::MenuItem(tr("Skins"), "Alt+S", &showSkins);
        ImGui::MenuItem(tr("Events"), nullptr, &showEvents);
        ImGui::MenuItem(tr("Mesh Tools"), "Alt+E", &showMeshTools);
        ImGui::MenuItem(tr("Weights"), "Alt+W", &showWeights);
        ImGui::MenuItem(tr("Preview"), "Alt+R", &showPreview);
        ImGui::MenuItem(tr("Metrics"), "Alt+M", &showMetrics);
        ImGui::MenuItem(tr("Outline"), "Alt+O", &showOutline);
        ImGui::MenuItem(tr("Problems"), "Alt+B", &showProblems);
        ImGui::Separator();
        ImGui::MenuItem(tr("Line Bones"), nullptr, &lineBonesAll);
        ImGui::MenuItem(tr("Ghosting"), "I", &ghosting);
        ImGui::MenuItem(tr("Rulers"), "Ctrl+Shift+R", &showRulers);
        if (ImGui::MenuItem(tr("Zoom To Fit"), "Ctrl+Shift+F")) zoomToFit();
        if (ImGui::MenuItem("Zoom To 100%", "Ctrl+F")) zoom100();
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Reset Layout"))) resetLayout = resetToolbarPanels = true;
        if (ImGui::BeginMenu(tr("Layouts"))) {
            std::vector<std::string> saved = listLayouts(layoutsDir());
            for (const std::string& n : saved)
                if (ImGui::MenuItem(n.c_str())) {
                    SavedLayout l;
                    if (loadLayout(layoutsDir(), n, l)) { applyLayout(l); setStatus("Layout loaded: " + n); }
                    else setStatus("Cannot read that layout");
                }
            if (!saved.empty()) ImGui::Separator();
            if (ImGui::MenuItem(tr("Save Layout As..."))) showLayouts = true;
            if (ImGui::MenuItem(tr("Manage Layouts..."), nullptr, false, !saved.empty())) showLayouts = true;
            ImGui::EndMenu();
        }
        headerMenuEnd();
    }
    if (headerMenu(tr("Help"))) {
        if (iconMenuItem(Icon::Question, tr("Hotkeys"), "F1")) showHotkeys = true;
        if (iconMenuItem(Icon::Skeleton, tr("About Osteon"))) showAbout = true;
        headerMenuEnd();
    }
    // Quick icons: project, history, import / export, tools.
    headerSeparator();
    if (headerIcon("##hnew", Icon::DocNew, tr("New project (Ctrl+Shift+N)"))) cmdNew();
    if (headerIcon("##hopen", Icon::Folder, tr("Open project (Ctrl+O)"))) cmdOpen();
    if (headerIcon("##hsave", Icon::Save, tr("Save project (Ctrl+S)"))) cmdSave();
    headerSeparator();
    ImGui::BeginDisabled(!doc.canUndo());
    if (headerIcon("##hundo", Icon::Undo, tr("Undo (Ctrl+Z)"))) doc.undo();
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!doc.canRedo());
    if (headerIcon("##hredo", Icon::Redo, tr("Redo (Ctrl+Shift+Z)"))) doc.redo();
    ImGui::EndDisabled();
    headerSeparator();
    if (headerIcon("##himport", Icon::Import, tr("Import data (Spine JSON)..."))) cmdImportData();
    if (headerIcon("##hexport", Icon::Export, tr("Export... (Ctrl+E)"))) showExport = true;
    headerSeparator();
    if (headerIcon("##hatlas", Icon::Atlas, tr("Texture packer (Ctrl+P)"))) showPacker = true;
    if (headerIcon("##hsettings", Icon::Gear, tr("Settings (F12)"))) showSettings = true;
    // The project name, centred (empty space: part of the drag area).
    {
        std::string title = doc.projectPath.empty() ? "Untitled" : fileName(doc.projectPath);
        if (doc.dirty) title += " *";
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 ts = ImGui::CalcTextSize(title.c_str());
        ImGui::GetWindowDrawList()->AddText(ImVec2(vp->Pos.x + vp->Size.x / 2 - ts.x / 2, vp->Pos.y + (headerHeight - ts.y) / 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), title.c_str());
    }
    headerWindowButtons();
    ImGui::EndMainMenuBar();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void Editor::shortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    bool ctrl = io.KeyCtrl, shift = io.KeyShift, alt = io.KeyAlt;
    auto pressed = [this](ImGuiKey k, bool repeat = false) { return keyPressedMapped(k, repeat); };

    if (ctrl && pressed(ImGuiKey_Z)) { shift ? doc.redo() : doc.undo(); return; }
    if (ctrl && pressed(ImGuiKey_Y)) { doc.redo(); return; }
    if (ctrl && shift && pressed(ImGuiKey_N)) { cmdNew(); return; }
    if (ctrl && pressed(ImGuiKey_O)) { cmdOpen(); return; }
    if (ctrl && shift && pressed(ImGuiKey_S)) { cmdSaveAs(); return; }
    if (ctrl && pressed(ImGuiKey_S)) { cmdSave(); return; }
    if (ctrl && pressed(ImGuiKey_E)) { showExport = true; return; }
    if (ctrl && pressed(ImGuiKey_P)) { showPacker = true; return; }
    if (pressed(ImGuiKey_F12)) { showSettings = true; return; }
    if (pressed(ImGuiKey_F1)) { showHotkeys = true; return; }
    if (ctrl && pressed(ImGuiKey_Tab)) { doc.setMode(doc.mode == Mode::Setup ? Mode::Animate : Mode::Setup); return; }
    if (ctrl && shift && pressed(ImGuiKey_F)) { zoomToFit(); return; }
    if (ctrl && !shift && pressed(ImGuiKey_F)) { zoom100(); return; }
    if (ctrl && shift && pressed(ImGuiKey_R)) { showRulers = !showRulers; return; }
    if (alt) {
        if (pressed(ImGuiKey_T)) showTree = !showTree;
        if (pressed(ImGuiKey_D)) showDopesheet = !showDopesheet;
        if (pressed(ImGuiKey_G)) showGraph = !showGraph;
        if (pressed(ImGuiKey_A)) showAnimations = !showAnimations;
        if (pressed(ImGuiKey_S)) showSkins = !showSkins;
        if (pressed(ImGuiKey_E)) showMeshTools = !showMeshTools;
        if (pressed(ImGuiKey_W)) showWeights = !showWeights;
        if (pressed(ImGuiKey_R)) showPreview = !showPreview;
        if (pressed(ImGuiKey_M)) showMetrics = !showMetrics;
        if (pressed(ImGuiKey_O)) showOutline = !showOutline;
        if (pressed(ImGuiKey_B)) showProblems = !showProblems;
        return;
    }
    // Remaining single-key shortcuts only apply when no popup is open.
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;
    if (ctrl && pressed(ImGuiKey_D)) { doc.duplicateSelection(); return; }
    // Weights tool, with a weighted mesh selected: Ctrl+A selects all its vertices, Ctrl+C / Ctrl+V copy and paste weights.
    if (ctrl && doc.tool == Tool::Weights) {
        int wsi = -1;
        Attachment* wa = selectedAttachment(&wsi);
        if (wa && wa->type == AttachmentType::Mesh && wa->isWeighted() && doc.mode == Mode::Setup) {
            if (pressed(ImGuiKey_A)) {
                meshSelection.clear();
                for (int k = 0; k < wa->vertexCount(); k++) meshSelection.push_back(k);
                return;
            }
            if (pressed(ImGuiKey_C) && !meshSelection.empty()) {
                std::vector<int> ordered = meshSelection;
                std::sort(ordered.begin(), ordered.end());
                weightClipboard = copyWeights(*wa, ordered);
                setStatus("Copied the weights of " + std::to_string(ordered.size()) + " vertices");
                return;
            }
            if (pressed(ImGuiKey_V) && !meshSelection.empty()) {
                std::vector<int> ordered = meshSelection;
                std::sort(ordered.begin(), ordered.end());
                doc.checkpoint("Paste weights");
                wa = selectedAttachment(&wsi);
                if (pasteWeights(*wa, doc.sk, ordered, weightClipboard)) setStatus("Pasted weights");
                else setStatus("Select the same number of vertices as were copied");
                return;
            }
        }
    }
    if (ctrl && pressed(ImGuiKey_A)) { doc.selectAll(); return; } // select all bones and slots
    if (doc.mode == Mode::Setup && ctrl) { // copy / paste of objects and of their transform; draw order to the front / back
        if (!shift && pressed(ImGuiKey_C)) { setStatus(doc.copySelection() ? "Copied" : "Nothing to copy"); return; }
        if (!shift && pressed(ImGuiKey_V)) { int n = doc.pasteObjects(false); if (n) setStatus("Pasted " + std::to_string(n) + " object(s)"); return; }
        if (shift && pressed(ImGuiKey_C)) { setStatus(doc.copyTransform() ? "Transform copied" : "Nothing to copy"); return; }
        if (shift && pressed(ImGuiKey_V)) { int n = doc.pasteTransform(); if (n) setStatus("Transform pasted"); return; }
        if (pressed(ImGuiKey_Equal) || pressed(ImGuiKey_KeypadAdd)) { doc.moveSlotsInDrawOrder(Document::DrawOrderMove::ToFront); return; }
        if (pressed(ImGuiKey_Minus) || pressed(ImGuiKey_KeypadSubtract)) { doc.moveSlotsInDrawOrder(Document::DrawOrderMove::ToBack); return; }
    }
    if (ctrl && pressed(ImGuiKey_H)) { // show all bones/slots
        doc.checkpoint("Show all");
        for (auto& b : doc.data.bones) b.visible = true;
        for (auto& s : doc.data.slots) s.visible = true;
        return;
    }
    if (ctrl && pressed(ImGuiKey_B)) { showBones = !showBones; return; }
    if (ctrl && pressed(ImGuiKey_I)) { showImages = !showImages; return; }
    if (ctrl) return;

    if (pressed(ImGuiKey_Delete) || pressed(ImGuiKey_Backspace)) {
        if (doc.mode == Mode::Animate && !selKeys.empty()) {
            // Handled by the dopesheet when it has focus; here delete selected keys too.
            if (Animation* an = doc.currentAnimation()) {
                doc.checkpoint("Delete keys");
                std::sort(selKeys.begin(), selKeys.end(), [](const KeyRef& a, const KeyRef& b) { return a.timeline != b.timeline ? a.timeline > b.timeline : a.key > b.key; });
                for (auto& k : selKeys)
                    if (k.timeline < (int)an->timelines.size() && k.key < (int)an->timelines[k.timeline].keys.size())
                        an->timelines[k.timeline].keys.erase(an->timelines[k.timeline].keys.begin() + k.key);
                an->timelines.erase(std::remove_if(an->timelines.begin(), an->timelines.end(), [](const Timeline& t) { return t.keys.empty(); }), an->timelines.end());
                selKeys.clear();
            }
        } else requestDelete();
        return;
    }
    if (pressed(ImGuiKey_F2)) { beginRename(); return; }
    if (pressed(ImGuiKey_Escape) || pressed(ImGuiKey_Space)) {
        if (meshMode == MeshMode::New) { meshFinishNew(false); return; }                // New: cancel
        if (meshMode != MeshMode::Off) { meshMode = MeshMode::Off; meshSelection.clear(); return; } // leave Edit Mesh
        if (bindMode) { finishBind(); return; }                                   // Bind: finish and apply
        if (doc.tool == Tool::Weights && !meshSelection.empty()) { meshSelection.clear(); return; } // Weights: deselect vertices first
        doc.clearSelection(); meshSelection.clear(); pickParent = false; pickConstraint = 0;
        return;
    }
    if (pressed(ImGuiKey_C)) doc.tool = Tool::Rotate;
    if (pressed(ImGuiKey_V)) doc.tool = Tool::Translate;
    if (pressed(ImGuiKey_X)) doc.tool = Tool::Scale;
    if (pressed(ImGuiKey_Z)) doc.tool = Tool::Shear;
    if (pressed(ImGuiKey_B)) doc.tool = Tool::Pose;
    if (pressed(ImGuiKey_N) && doc.mode == Mode::Setup) doc.tool = Tool::Create;
    if (pressed(ImGuiKey_G)) { doc.tool = Tool::Weights; showWeights = true; ImGui::SetWindowFocus(trWindow("Weights")); }
    if (pressed(ImGuiKey_P) && doc.mode == Mode::Setup && !doc.selection.empty()) { pickParent = true; setStatus("Set Parent: click the new parent bone"); }
    if (pressed(ImGuiKey_H)) {
        doc.checkpoint("Visibility");
        for (auto& s : doc.selection) {
            if (s.type == SelType::Bone) doc.data.bones[s.index].visible = !doc.data.bones[s.index].visible;
            if (s.type == SelType::Slot) doc.data.slots[s.index].visible = !doc.data.slots[s.index].visible;
        }
    }
    if (pressed(ImGuiKey_I)) ghosting = !ghosting;
    if (pressed(ImGuiKey_Period) || pressed(ImGuiKey_Comma)) {
        int n = (int)doc.data.skins.size();
        if (n) doc.skin = (doc.skin + (pressed(ImGuiKey_Period) ? 1 : n - 1)) % n;
    }
    // Draw order (+ / -) for selected slots.
    if (doc.mode == Mode::Setup && (pressed(ImGuiKey_Equal, true) || pressed(ImGuiKey_Minus, true) || pressed(ImGuiKey_KeypadAdd, true) || pressed(ImGuiKey_KeypadSubtract, true))) {
        int dir = (pressed(ImGuiKey_Equal, true) || pressed(ImGuiKey_KeypadAdd, true)) ? 1 : -1;
        std::vector<int> slots = doc.selectedSlots();
        if (!slots.empty()) {
            doc.checkpoint("Draw order");
            std::sort(slots.begin(), slots.end());
            if (dir > 0) std::reverse(slots.begin(), slots.end());
            std::vector<std::string> names;
            for (int s : slots) names.push_back(doc.data.slots[s].name);
            for (auto& n : names) { int s = doc.data.findSlot(n); doc.data.moveSlot(s, s + dir); }
            doc.selection.clear();
            for (auto& n : names) doc.selection.push_back({SelType::Slot, doc.data.findSlot(n), ""});
        }
    }
    // Arrow-key nudge (Shift = 1px, otherwise 10px), as in Spine.
    float step = shift ? 1.0f : 10.0f;
    Vec2 nudge;
    if (pressed(ImGuiKey_UpArrow, true)) nudge.y += step;
    if (pressed(ImGuiKey_DownArrow, true)) nudge.y -= step;
    if (pressed(ImGuiKey_LeftArrow, true)) nudge.x -= step;
    if (pressed(ImGuiKey_RightArrow, true)) nudge.x += step;
    if ((nudge.x != 0 || nudge.y != 0) && !doc.selectedBones().empty()) {
        doc.checkpoint("Nudge");
        nudge = nudge * (1 / zoom);
        for (int b : doc.selectedBones()) {
            const Bone& bone = doc.sk.bones[b];
            Vec2 target = bone.worldToParent(doc.sk, Vec2(bone.worldX, bone.worldY) + nudge);
            Document::LocalPose p = doc.bonePose(b);
            p.x = target.x; p.y = target.y;
            doc.setBonePose(b, p, (int)Channel::Translate);
        }
    }
    // Animate mode transport.
    if (doc.mode == Mode::Animate) {
        float fps = doc.data.fps, dur = doc.animationDuration();
        auto keyTimes = [&] {
            std::vector<float> t;
            if (Animation* an = doc.currentAnimation())
                for (auto& tl : an->timelines) for (auto& k : tl.keys) t.push_back(k.time);
            std::sort(t.begin(), t.end());
            return t;
        };
        if (pressed(ImGuiKey_D)) { doc.playBackward = false; doc.playing = !doc.playing; }
        if (pressed(ImGuiKey_A)) { doc.playBackward = true; doc.playing = !doc.playing; }
        if (pressed(ImGuiKey_R, true)) doc.setTime(std::round(doc.time * fps + (shift ? 10 : 1)) / fps);
        if (pressed(ImGuiKey_F, true)) doc.setTime(std::max(0.0f, std::round(doc.time * fps - (shift ? 10 : 1)) / fps));
        if (pressed(ImGuiKey_Q)) { auto t = keyTimes(); doc.setTime(t.empty() ? 0 : t.front()); }
        if (pressed(ImGuiKey_E)) { auto t = keyTimes(); doc.setTime(t.empty() ? dur : t.back()); }
        if (pressed(ImGuiKey_W, true)) { for (float t : keyTimes()) if (t > doc.time + 1e-4f) { doc.setTime(t); break; } }
        if (pressed(ImGuiKey_S, true)) { auto t = keyTimes(); for (auto it = t.rbegin(); it != t.rend(); ++it) if (*it < doc.time - 1e-4f) { doc.setTime(*it); break; } }
        if (pressed(ImGuiKey_LeftBracket)) doc.loopStart = doc.time;
        if (pressed(ImGuiKey_RightBracket)) doc.loopEnd = doc.time;
        if (pressed(ImGuiKey_K)) { // key edited (pending) values
            doc.checkpoint("Key");
            auto pend = doc.pending;
            for (auto& [b, p] : pend) doc.keyBone(b, p.channels);
        }
        if (pressed(ImGuiKey_L)) { // key active tool channel for selected bones
            int ch = doc.tool == Tool::Translate ? (int)Channel::Translate : doc.tool == Tool::Scale ? (int)Channel::Scale
                   : doc.tool == Tool::Shear ? (int)Channel::Shear : (int)Channel::Rotate;
            doc.checkpoint("Key");
            for (int b : doc.selectedBones()) doc.keyBone(b, ch);
        }
        int n = (int)doc.data.animations.size();
        if (n && pressed(ImGuiKey_Y)) { doc.animation = (doc.animation + 1) % n; doc.setTime(0); }
        if (n && pressed(ImGuiKey_T)) { doc.animation = (doc.animation + n - 1) % n; doc.setTime(0); }
    }
}

} // namespace osteon
