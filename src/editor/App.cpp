#include "App.h"
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
    for (Mode m : {Mode::Setup, Mode::Animate}) readTextFile(layoutFile(m), layoutIni[(int)m]);
    const PanelVis& v = panelVis[(int)Mode::Setup];
    showDopesheet = v.dopesheet; showGraph = v.graph; showAnimations = v.animations; showPreview = v.preview;
    if (!layoutIni[(int)Mode::Setup].empty()) ImGui::LoadIniSettingsFromMemory(layoutIni[(int)Mode::Setup].c_str());
    resetLayout = layoutIni[(int)Mode::Setup].empty();
    previewState = AnimationState(&doc.data);
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
    if (!layoutIni[n].empty()) ImGui::LoadIniSettingsFromMemory(layoutIni[n].c_str());
    else resetLayout = true;
    layoutMode = doc.mode;
}

void Editor::shutdown() {
    layoutIni[(int)layoutMode] = ImGui::SaveIniSettingsToMemory();
    for (Mode m : {Mode::Setup, Mode::Animate})
        if (!layoutIni[(int)m].empty()) writeTextFile(layoutFile(m), layoutIni[(int)m]);
    viewRT.release();
    previewRT.release();
    exportRT.release();
    gfx.shutdown();
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
    gfx.reloadImages();
    camX = camY = 0;
    zoom = 1;
    setStatus("New project");
}

void Editor::cmdOpen() {
    std::string p = openFileDialog(hwnd, L"Osteon / Spine JSON\0*.osteon;*.json\0All files\0*.*\0", L"Open Project");
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
    std::string suggested = doc.projectPath.empty() ? (doc.imagesDir.empty() ? "skeleton.osteon" : joinPath(parentDir(doc.imagesDir), "skeleton.osteon"))
                                                    : doc.projectPath;
    std::string p = saveFileDialog(hwnd, L"Osteon project\0*.osteon\0", L"osteon", suggested, L"Save Project As");
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
    o.slotVisible = [this](int s) { return slotVisible(s); };
    return o;
}

bool Editor::boneVisible(int b) const {
    for (int i = b; i >= 0; i = doc.data.bones[i].parent)
        if (!doc.data.bones[i].visible) return false;
    return true;
}

bool Editor::slotVisible(int s) const {
    if (s < 0 || s >= (int)doc.data.slots.size()) return false;
    return showImages && doc.data.slots[s].visible && boneVisible(doc.data.slots[s].bone);
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
    case SelType::Skin: n = doc.data.skins[p.index].name; break;
    case SelType::Animation: n = doc.data.animations[p.index].name; break;
    case SelType::Event: n = doc.data.events[p.index].name; break;
    default: return;
    }
    renameBuffer = n;
    renaming = true;
}

// Renders an animation's frames (or the setup pose) cropped to the animation's bounds.
void Editor::renderFrames(int anim, float fps, float scale, int pad, std::vector<std::vector<uint8_t>>& frames, int& w, int& h,
                          bool transparent, Color bg) {
    Skeleton s(doc.data);
    s.skin = doc.skin;
    s.updateCacheOrder();
    const Animation* a = anim >= 0 && anim < (int)doc.data.animations.size() ? &doc.data.animations[anim] : nullptr;
    float dur = a ? a->duration() : 0;
    int count = a ? std::max(1, (int)std::floor(dur * fps + 0.5f) + 1) : 1;
    auto pose = [&](int i) {
        s.setToSetupPose();
        if (a) applyAnimation(*a, s, i / fps, i / fps, false, nullptr, 1, MixBlend::Setup);
        s.time = i / fps;
        s.updateWorldTransform(a ? Physics::Update : Physics::None);
    };
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (int i = 0; i < count; i++) {
        pose(i);
        float a0, b0, a1, b1;
        if (s.bounds(a0, b0, a1, b1)) { x0 = std::min(x0, a0); y0 = std::min(y0, b0); x1 = std::max(x1, a1); y1 = std::max(y1, b1); }
    }
    if (x0 > x1) { x0 = y0 = -1; x1 = y1 = 1; }
    w = std::max(1, (int)std::ceil((x1 - x0) * scale) + pad * 2);
    h = std::max(1, (int)std::ceil((y1 - y0) * scale) + pad * 2);
    gfx.ensureTarget(exportRT, w, h);
    RenderOptions opt;
    opt.background = false;
    s.physics.assign(s.physics.size(), PhysicsConstraint());
    frames.clear();
    for (int i = 0; i < count; i++) {
        pose(i);
        gfx.begin(exportRT, (x0 + x1) / 2, (y0 + y1) / 2, scale, opt);
        gfx.drawSkeleton(s, [this](const Attachment& at, int seq) { return doc.imagePathFor(at, seq); }, opt);
        gfx.end();
        std::vector<uint8_t> px = gfx.readback(exportRT);
        if (!transparent)
            for (size_t p = 0; p < px.size(); p += 4) {
                float al = px[p + 3] / 255.0f;
                px[p] = (uint8_t)(px[p] * al + bg.r * 255 * (1 - al));
                px[p + 1] = (uint8_t)(px[p + 1] * al + bg.g * 255 * (1 - al));
                px[p + 2] = (uint8_t)(px[p + 2] * al + bg.b * 255 * (1 - al));
                px[p + 3] = 255;
            }
        frames.push_back(std::move(px));
    }
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
    ImGui::DockBuilderDockWindow("Viewport", main);
    ImGui::DockBuilderDockWindow("Properties", rightBottom);
    ImGui::DockBuilderDockWindow("Dopesheet", bottom);
    ImGui::DockBuilderDockWindow("Graph", bottom);
    ImGui::DockBuilderDockWindow("Animations", rightBottom);
    ImGui::DockBuilderDockWindow("Skins", right);
    ImGui::DockBuilderDockWindow("Events", right);
    ImGui::DockBuilderDockWindow("Mesh Tools", rightBottom);
    ImGui::DockBuilderDockWindow("Weights", rightBottom);
    ImGui::DockBuilderDockWindow("Preview", bottom);
    ImGui::DockBuilderDockWindow("Metrics", rightBottom);
    ImGui::DockBuilderDockWindow("Outline", right);
    ImGui::DockBuilderDockWindow("Tree", right);
    ImGui::DockBuilderDockWindow("Problems", bottom);
    ImGui::DockBuilderDockWindow("Properties", rightBottom);
    ImGui::DockBuilderDockWindow("Dopesheet", bottom);
    ImGui::DockBuilderFinish(dockId);
}

void Editor::frame(float dt) {
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
    if (showMeshTools) meshToolsView();
    if (showWeights) weightsView();
    if (showExport) exportDialog();
    if (showPacker) packerDialog();
    if (showSettings) settingsDialog();
    if (showAbout) aboutDialog();
    if (showHotkeys) hotkeysDialog();
    if (focusDefaults > 0 && --focusDefaults == 0) { // bring the primary tab of each dock area to the front
        ImGui::SetWindowFocus("Tree");
        ImGui::SetWindowFocus("Properties");
        ImGui::SetWindowFocus("Dopesheet");
        ImGui::SetWindowFocus("Viewport");
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
        if (enter || ImGui::Button("OK", ImVec2(100, 0))) {
            if (!doc.renameSelection(buf)) setStatus("Name is empty or already used");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Status line (bottom-left overlay).
    if (!status.empty() && ImGui::GetTime() < statusUntil) {
        ImVec2 p = ImGui::GetMainViewport()->WorkPos;
        ImVec2 s = ImGui::GetMainViewport()->WorkSize;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 ts = ImGui::CalcTextSize(status.c_str());
        ImVec2 a(p.x + 12, p.y + s.y - ts.y - 16);
        dl->AddRectFilled(ImVec2(a.x - 6, a.y - 4), ImVec2(a.x + ts.x + 6, a.y + ts.y + 4), IM_COL32(20, 20, 20, 230), 3);
        dl->AddText(a, IM_COL32(230, 230, 230, 255), status.c_str());
    }
}

void Editor::menuBar() {
    // The top header has no borders: neither around the bar nor around its buttons.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    if (!ImGui::BeginMainMenuBar()) { ImGui::PopStyleVar(2); return; }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project", "Ctrl+Shift+N")) cmdNew();
        if (ImGui::MenuItem("Open Project...", "Ctrl+O")) cmdOpen();
        if (ImGui::BeginMenu("Recent Projects", !doc.recentFiles.empty())) {
            for (auto& r : std::vector<std::string>(doc.recentFiles))
                if (ImGui::MenuItem(r.c_str())) openPath(r);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save Project", "Ctrl+S")) cmdSave();
        if (ImGui::MenuItem("Save Project As...", "Ctrl+Shift+S")) cmdSaveAs();
        ImGui::Separator();
        if (ImGui::MenuItem("Import Data (Spine JSON)...")) cmdImportData();
        if (ImGui::MenuItem("Import PSD...")) cmdImportPsd();
        if (ImGui::MenuItem("Set Images Folder...")) cmdImportImagesFolder();
        if (ImGui::MenuItem("Create Slots For Unused Images", nullptr, false, !doc.imagesDir.empty())) {
            int n = doc.importImagesAsSlots();
            setStatus(std::to_string(n) + " slots created");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Export...", "Ctrl+E")) showExport = true;
        if (ImGui::MenuItem("Texture Packer...", "Ctrl+P")) showPacker = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Settings...", "F12")) showSettings = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        std::string u = "Undo " + doc.undoLabel(), r = "Redo " + doc.redoLabel();
        if (ImGui::MenuItem(u.c_str(), "Ctrl+Z", false, doc.canUndo())) doc.undo();
        if (ImGui::MenuItem(r.c_str(), "Ctrl+Shift+Z", false, doc.canRedo())) doc.redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Rename", "F2", false, !doc.selection.empty())) beginRename();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !doc.selection.empty())) doc.duplicateSelection();
        if (ImGui::MenuItem("Delete", "Del", false, !doc.selection.empty())) doc.deleteSelection();
        if (ImGui::MenuItem("Deselect", "Esc")) doc.clearSelection();
        ImGui::Separator();
        if (ImGui::MenuItem("Setup / Animate Mode", "Ctrl+Tab")) doc.setMode(doc.mode == Mode::Setup ? Mode::Animate : Mode::Setup);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Views")) {
        ImGui::MenuItem("Tree", "Alt+T", &showTree);
        ImGui::MenuItem("Properties", nullptr, &showProps);
        ImGui::MenuItem("Dopesheet", "Alt+D", &showDopesheet);
        ImGui::MenuItem("Graph", "Alt+G", &showGraph);
        ImGui::MenuItem("Animations", "Alt+A", &showAnimations);
        ImGui::MenuItem("Skins", "Alt+S", &showSkins);
        ImGui::MenuItem("Events", nullptr, &showEvents);
        ImGui::MenuItem("Mesh Tools", "Alt+E", &showMeshTools);
        ImGui::MenuItem("Weights", "Alt+W", &showWeights);
        ImGui::MenuItem("Preview", "Alt+R", &showPreview);
        ImGui::MenuItem("Metrics", "Alt+M", &showMetrics);
        ImGui::MenuItem("Outline", "Alt+O", &showOutline);
        ImGui::MenuItem("Problems", "Alt+B", &showProblems);
        ImGui::Separator();
        ImGui::MenuItem("Ghosting", "I", &ghosting);
        ImGui::MenuItem("Rulers", "Ctrl+Shift+R", &showRulers);
        if (ImGui::MenuItem("Zoom To Fit", "Ctrl+Shift+F")) zoomToFit();
        if (ImGui::MenuItem("Zoom To 100%", "Ctrl+F")) zoom100();
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) resetLayout = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Hotkeys", "F1")) showHotkeys = true;
        if (ImGui::MenuItem("About Osteon")) showAbout = true;
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
    ImGui::PopStyleVar(2);
}

void Editor::shortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    bool ctrl = io.KeyCtrl, shift = io.KeyShift, alt = io.KeyAlt;
    auto pressed = [](ImGuiKey k, bool repeat = false) { return ImGui::IsKeyPressed(k, repeat); };

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
    if (ctrl && pressed(ImGuiKey_A)) { doc.selectAll(); return; } // select all bones and slots
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
        } else doc.deleteSelection();
        return;
    }
    if (pressed(ImGuiKey_F2)) { beginRename(); return; }
    if (pressed(ImGuiKey_Escape) || pressed(ImGuiKey_Space)) { doc.clearSelection(); meshSelection.clear(); pickParent = false; pickConstraint = 0; return; }
    if (pressed(ImGuiKey_C)) doc.tool = Tool::Rotate;
    if (pressed(ImGuiKey_V)) doc.tool = Tool::Translate;
    if (pressed(ImGuiKey_X)) doc.tool = Tool::Scale;
    if (pressed(ImGuiKey_Z)) doc.tool = Tool::Shear;
    if (pressed(ImGuiKey_B)) doc.tool = Tool::Pose;
    if (pressed(ImGuiKey_N) && doc.mode == Mode::Setup) doc.tool = Tool::Create;
    if (pressed(ImGuiKey_G)) { doc.tool = Tool::Weights; showWeights = true; }
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
