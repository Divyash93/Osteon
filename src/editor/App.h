// The editor application: owns the document, renderer and all views (Dear ImGui, docked).
#pragma once
#include "Document.h"
#include "Gfx.h"
#include "Util.h"
#include "imgui.h"

namespace osteon {

struct Editor {
    HWND hwnd = nullptr;
    Gfx gfx;
    Document doc;
    RenderTarget viewRT, previewRT, exportRT;

    // Viewport camera (world units, y-up).
    float camX = 0, camY = 0, zoom = 1;
    bool fitRequested = true;
    ImVec2 vpOrigin, vpSize;
    bool vpHovered = false;

    // View options (toolbar "Bones / Images / Others" grid and Options).
    bool showBones = true, showImages = true, showOthers = true;
    bool selectBones = true, selectImages = true, selectOthers = true;
    bool showBoneNames = false, showImageNames = false, showOtherNames = false;
    bool showRulers = false, showAxes = true;
    bool ghosting = false;
    int ghostFrames = 3, ghostStep = 2;
    float ghostAlpha = 0.35f;

    // Panels (View menu).
    bool showTree = true, showProps = true, showDopesheet = true, showGraph = true, showAnimations = true, showSkins = false;
    bool showPreview = false, showMetrics = false, showOutline = false, showMeshTools = false, showWeights = false, showProblems = false;
    bool showColor = false, showEvents = false, showPlayback = true;
    // Dialogs.
    bool showExport = false, showPacker = false, showSettings = false, showAbout = false, showHotkeys = false;
    bool resetLayout = true;

    // Mesh editing (Mesh Tools view).
    enum class MeshMode { Off, Modify, Create, Delete } meshMode = MeshMode::Off;
    std::vector<int> meshSelection;
    bool meshDeformed = true;

    // Weights tool.
    int weightBone = -1;
    float brushSize = 30, brushStrength = 0.1f;
    enum class WeightMode { Add, Remove, Replace, Smooth } weightMode = WeightMode::Add;

    // Viewport interaction state.
    struct Drag {
        bool active = false, moved = false;
        int kind = 0; // 0 none, 1 tool, 2 pan, 3 box select, 4 create bone, 5 mesh vertex, 6 weights brush
        ImVec2 startScreen;
        Vec2 startWorld, lastWorld;
        std::vector<int> bones;
        std::vector<Document::LocalPose> startPose;
        std::vector<Vec2> startWorldPos;
        std::vector<float> startAngles;
        Sel attachment; // when dragging an attachment
        Attachment startAtt;
        int createParent = -1;
        bool parentCaptured = false, deformCaptured = false;
        Document::LocalPose parentStart{};
        std::vector<float> deformBase;
    } drag;
    bool pickParent = false; // "Set Parent" (P): next bone click becomes the parent

    // Timeline view state.
    float tlZoom = 12;     // pixels per frame
    float tlScroll = 0;    // first visible frame
    struct KeyRef { int timeline; int key; bool operator==(const KeyRef& o) const { return timeline == o.timeline && key == o.key; } };
    std::vector<KeyRef> selKeys;
    std::vector<std::pair<Timeline, std::vector<float>>> keyClipboard; // copied keys (relative times)
    float graphZoomY = 1, graphPanY = 0;
    int graphChannel = -1; // timeline index shown in the graph (-1 = selection)

    // Preview view.
    AnimationState previewState;
    Skeleton previewSk;
    int previewAnim = -1;
    float previewMix = 0.2f, previewSpeed = 1;
    bool previewLoop = true;

    // Settings.
    float uiScale = 1;
    Color bgA{0.20f, 0.20f, 0.20f, 1}, bgB{0.235f, 0.235f, 0.235f, 1};

    std::string status;
    double statusUntil = 0;
    std::string renameBuffer;
    bool renaming = false;
    std::string pendingOpen; // file dropped / passed on command line

    // Main loop.
    void init();
    void frame(float dt);
    void shutdown();
    bool confirmDiscard();
    void setStatus(const std::string& s, double seconds = 4);
    void openPath(const std::string& path);

    // Commands.
    void cmdNew();
    void cmdOpen();
    bool cmdSave();
    bool cmdSaveAs();
    void cmdImportImagesFolder();
    void cmdImportData();
    void zoomToFit();
    void zoom100();

    // Views (implemented across Viewport.cpp, Panels.cpp, Timeline.cpp, Dialogs.cpp).
    void dockLayout(unsigned dockId);
    void menuBar();
    void shortcuts();
    void viewport();
    void viewportToolbar();
    void drawOverlay(ImDrawList* dl);
    void viewportInput();
    void treeView();
    void propertiesView();
    void animationsView();
    void skinsView();
    void eventsView();
    void dopesheetView();
    void graphView();
    void playbackBar();
    void previewView(float dt);
    void metricsView();
    void outlineView();
    void problemsView();
    void meshToolsView();
    void weightsView();
    void exportDialog();
    void packerDialog();
    void settingsDialog();
    void aboutDialog();
    void hotkeysDialog();

    // Helpers.
    ImVec2 toScreen(Vec2 w) const { return {vpOrigin.x + (w.x - camX) * zoom + vpSize.x / 2, vpOrigin.y + vpSize.y / 2 - (w.y - camY) * zoom}; }
    Vec2 toWorld(ImVec2 s) const { return {(s.x - vpOrigin.x - vpSize.x / 2) / zoom + camX, (vpOrigin.y + vpSize.y / 2 - s.y) / zoom + camY}; }
    RenderOptions renderOptions();
    int pickBone(ImVec2 screen);
    int pickSlot(ImVec2 screen);
    bool slotVisible(int slot) const;
    bool boneVisible(int bone) const;
    void renderFrames(int anim, float fps, float scale, int pad, std::vector<std::vector<uint8_t>>& frames, int& w, int& h, bool transparent, Color bg);
    std::vector<int> visibleTimelines();
    Attachment* selectedAttachment(int* slotOut = nullptr);
    void beginRename();
};

/** Small widget helpers shared by views. */
bool dragFloatEdit(Document& doc, const char* label, float* v, float speed, const char* fmt = "%.2f", const char* undoLabel = "Edit");
bool dragFloat2Edit(Document& doc, const char* label, float* v, float speed, const char* fmt = "%.2f", const char* undoLabel = "Edit");
bool colorEdit(Document& doc, const char* label, Color& c, bool alpha = true, const char* undoLabel = "Color");
bool checkboxEdit(Document& doc, const char* label, bool* v, const char* undoLabel = "Edit");
void helpMarker(const char* text);

} // namespace osteon
