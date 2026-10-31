// The editor application: owns the document, renderer and all views (Dear ImGui, docked).
#pragma once
#include "Document.h"
#include <map>
#include <set>
#include "Gfx.h"
#include "Layouts.h"
#include "Splash.h"
#include "Geometry.h"
#include "Util.h"
#include "imgui.h"

namespace osteon {

enum class Icon; // Icons.h

/** The colour of the i-th bone bound to a mesh in the Weights view; the viewport uses the same one for that bone. */
inline ImU32 weightPaletteColor(size_t i) {
    static const ImU32 palette[] = {IM_COL32(236, 84, 84, 255),  IM_COL32(84, 204, 110, 255), IM_COL32(84, 148, 244, 255), IM_COL32(244, 202, 64, 255),
                                    IM_COL32(196, 118, 236, 255), IM_COL32(84, 214, 214, 255), IM_COL32(244, 146, 64, 255), IM_COL32(160, 224, 96, 255),
                                    IM_COL32(240, 120, 176, 255), IM_COL32(132, 132, 244, 255), IM_COL32(200, 170, 120, 255), IM_COL32(150, 200, 200, 255)};
    return palette[i % (sizeof palette / sizeof palette[0])];
}

/** Applies the Dark or Light user interface theme (see Settings > Theme). */
void applyUiTheme(float scale, bool light);

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
    bool showRulers = false, showAxes = true; // rulers are off until switched on (Views > Rulers)
    bool ghosting = false;
    int ghostFrames = 3, ghostStep = 2;
    float ghostAlpha = 0.35f;
    int panelOpacity = 80;                     // percent: the floating toolbar's background (Settings > User interface)

    // Panels (View menu).
    bool showTree = true, showProps = true, showDopesheet = true, showGraph = true, showAnimations = true, showSkins = false;
    bool showPreview = false, showMetrics = false, showOutline = false, showMeshTools = false, showWeights = false, showProblems = false;
    bool showColor = false, showEvents = false, showPlayback = true;
    // Dialogs.
    bool showExport = false, showPacker = false, showUnpacker = false, showSettings = false, showAbout = false, showHotkeys = false;
    bool resetLayout = true;

    // Mesh editing (Mesh Tools view).
    enum class MeshMode { Off, Modify, Create, Delete, New } meshMode = MeshMode::Off;
    std::vector<int> meshSelection;
    bool meshTriangles = true; // Edit Mesh "Triangles": draw the triangle wireframe
    // Mesh display colours (Mesh Tools): dots, the selected dots, the triangle lines and the hull edges. Saved between sessions.
    ImVec4 meshDotColor{1.0f, 1.0f, 1.0f, 0.90f}, meshSelColor{1.0f, 0.78f, 0.16f, 1.0f}, meshTriColor{1.0f, 1.0f, 1.0f, 0.47f}, meshHullColor{0.27f, 0.82f, 0.92f, 1.0f};
    void resetMeshColors() { meshDotColor = {1.0f, 1.0f, 1.0f, 0.90f}; meshSelColor = {1.0f, 0.78f, 0.16f, 1.0f}; meshTriColor = {1.0f, 1.0f, 1.0f, 0.47f}; meshHullColor = {0.27f, 0.82f, 0.92f, 1.0f}; }
    bool meshDim = false, meshIsolate = false; // Edit Mesh "Dim" (dim the image) and "Isolate" (hide other attachments)
    TraceOptions trace;                        // Trace settings: Detail, Concavity, Refinement, Alpha threshold, Padding
    bool traceLive = false;                    // after Trace, the sliders re-trace as they move
    std::vector<Vec2> newHull;                 // New: hull vertices placed so far (local to the slot's bone)
    void meshBeginNew();
    void meshFinishNew(bool apply);

    // Weights tool (Spine's Weights view).
    std::set<int> weightBones;          // bones selected in the Bones list: what the brush and the Weight slider change
    int weightBone = -1;                // the one most recently selected of them
    bool weightBrush = false;           // Brush mode; false = Direct mode
    enum class WeightMode { Add, Remove, Replace } weightMode = WeightMode::Add;
    float brushSize = 30, brushStrength = 0.5f, brushFeather = 100; // feather in percent of the size
    float weightValue = 100;            // Direct mode "Weight" slider, percent
    bool showPies = true, showOverlay = true, showSelectedOnly = false; // display options
    Tool lastTransformTool = Tool::Rotate; // where a double click on empty space returns to
    bool bindMode = false;              // Bind: click bones to bind them to the selected mesh
    std::vector<int> bindPending;       // the bones clicked so far in Bind mode
    float pruneThreshold = 5;           // percent
    int pruneMaxBones = 4;
    std::set<std::string> weightLocks;  // "slot/attachment/bone" of locked bones
    std::vector<std::vector<std::pair<int, float>>> weightClipboard; // Ctrl+C on selected vertices
    std::vector<int> lockedBonesFor(int slot, const Attachment& mesh) const;
    double weightsNoticeUntil = 0;      // while ImGui time is below this, the viewport tells the user to leave Weights first
    Tool prevFrameTool = Tool::Rotate;
    void applyWeights();                // done with the weights: finishes Bind, leaves the Weights view for the transform tools
    void finishBind();                  // applies the bones clicked in Bind mode

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
        int handle = 0; // gizmo handle grabbed: 1 = X axis, 2 = Y axis, 3 = center/ring, 0 = free drag
        Vec2 axX{1, 0}, axY{0, 1};
        Vec2 pivot{0, 0}; // centre of a multi-object group: Rotate and Scale turn about it
        double startTime = 0; // when the press began (the weights brush waits a moment so a plain click can select a vertex)
        std::vector<int> adjustVertices, adjustBones;       // Direct weights drag: what is being adjusted
        std::vector<float> adjustBase;                      // and the weights they had when the drag began
        bool deformCaptured = false;
        int newPoint = -1; // New mode: the hull vertex being dragged
        int pathVertex = -1;       // Path editing: the vertex (handle in, point, handle out) being dragged
        bool pathMirror = false;   // its opposite handle follows, mirrored
        bool editMeshMove = false;                        // the drag started inside Edit Mesh: Modify keeps the image fixed
        bool uvFitted = false, uvFitOk = false;           // Modify (Setup): how the image maps onto the mesh, from the drag start
        double worldToUvX[3] = {}, worldToUvY[3] = {}, uvToWorldX[3] = {}, uvToWorldY[3] = {};
        std::vector<float> startWorld0;                   // the vertices' world positions when the drag began
        std::vector<float> deformBase;
    } drag;
    bool pickParent = false; // "Set Parent" (P): next bone click becomes the parent
    int pickConstraint = 0;  // New > IK (1) / Transform (2) Constraint: next bone click becomes the target
    bool picking() const { return pickParent || pickConstraint != 0; } // either way the hovered bone is highlighted

    // Hover feedback (what the mouse is over) and gizmo state.
    int hoverBone = -1, hoverSlot = -1, hoverHandle = 0;
    float toolbarTop = 0;
    bool lineBonesAll = false;    // Views > Line Bones: every bone is drawn as a plain line
    bool toolbarLocked = true;    // the viewport toolbar moves only when unlocked (its lock button)
    float toolbarOffset = 0;      // horizontal offset from the centred position, in pixels
    std::map<std::string, std::vector<float>> outlineCache; // image file -> traced silhouette (pixels)
    struct GizmoTarget { bool valid = false; Vec2 origin; Vec2 x{1, 0}, y{0, 1}; };
    GizmoTarget gizmoTarget();
    int gizmoHit(ImVec2 mouse);
    void drawGizmo(ImDrawList* dl);
    void drawHover(ImDrawList* dl);
    void drawHoverChip(ImDrawList* dl);
    const std::vector<float>& imageOutline(const std::string& file);

    // Timeline view state.
    float tlZoom = 12;     // pixels per frame
    float tlScroll = 0;    // first visible frame
    float tlRowScroll = 0; // first visible row, in pixels (dopesheet)
    struct KeyRef { int timeline; int key; bool operator==(const KeyRef& o) const { return timeline == o.timeline && key == o.key; } };
    std::vector<KeyRef> selKeys;
    std::vector<std::pair<Timeline, std::vector<float>>> keyClipboard; // copied keys (relative times)
    std::set<std::string> collapsedGroups; // dopesheet group headers that are folded
    // Dopesheet toolbar, as on Spine's Dopesheet page.
    bool dopeLocked = false;            // Lock: the rows stay on the bones and slots that were selected when it was switched on
    std::vector<int> lockBones, lockSlots;
    bool dopeSync = false;              // Sync: rows follow the curves the Graph shows; Lock and Filter are hidden
    std::set<int> dopeFilters;          // Filter: the kinds of rows shown (see DopeFilter), empty = all
    bool dopeFiltersOff = false;        // right-click on the Filter button switches the filters off and on
    bool dopeOffset = false;            // Offset: dragging a key also moves the keys after it
    bool dopeAdjust = false;            // Adjust: a transform tool shifts all the selected keys by the same amount
    bool dopeShowToolbar = true, dopeShowRows = true; // view settings
    enum DopeFilter { FilterCurrentTool, FilterRotate, FilterTranslate, FilterScale, FilterShear, FilterAttachment, FilterColor, FilterDeform,
                      FilterConstraints, FilterDrawOrder, FilterEvents, FilterCount };
    static const char* dopeFilterName(int f);
    bool dopeFilterAllows(const Timeline& t) const;
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
    Color bgA{0.318f, 0.318f, 0.318f, 1}, bgB{0.337f, 0.337f, 0.337f, 1}; // Spine's #515151 / #565656

    std::string status;
    double statusUntil = 0;
    std::string renameBuffer;
    bool renaming = false;
    void newAnimation(); // creates an animation, switches to Animate mode and asks for its name
    std::string pendingOpen; // file dropped / passed on command line

    // Main loop.
    void init();
    /** Called before ImGui::NewFrame: swaps dock layouts when the mode changed (Spine keeps one per mode). */
    void beforeNewFrame();
    Mode layoutMode = Mode::Setup;
    std::string layoutIni[2];
    struct PanelVis { bool dopesheet, graph, animations, preview; } panelVis[2] = {{false, false, false, false}, {true, true, true, false}};
    std::string layoutFile(Mode m) const;
    void frame(float dt);
    void shutdown();

    // Auto-save and crash recovery: a copy of unsaved work is written every few seconds to
    // %APPDATA%/Osteon/recovery; a normal exit deletes it, so finding one at start means Osteon did not close properly.
    bool autosaveEnabled = true;
    int autosaveSeconds = 30;
    double lastAutosave = 0;
    bool autosaveNow();        // writes the recovery copy (only when there are unsaved changes)
    // Hotkey remapping: the editor's logical shortcut key -> the key that triggers it now.
    std::map<ImGuiKey, ImGuiKey> keyRemap;
    bool keyPressedMapped(ImGuiKey logical, bool repeat = false) const;
    void loadKeyRemap();
    void saveKeyRemap() const;
    // The custom title bar (Header.cpp): logo, menus, toolbar icons and the window buttons in one dark bar.
    float headerHeight = 0;                    // pixels; the area above it drags the window
    std::vector<RECT> headerHot;               // interactive parts of the bar (client pixels): they are not drag areas
    void* headerLogo = nullptr;                // texture of the app icon
    bool headerMenu(const char* name);
    bool headerIcon(const char* id, Icon icon, const char* tooltip);
    void headerSeparator();
    void headerMenuEnd();                      // closes a menu opened with headerMenu
    void headerMarkHot();
    void headerWindowButtons();
    bool headerInteractiveAt(int x, int y) const;
    // Right-click menu on bones and images, and the delete confirmation (ObjectMenu.cpp).
    int objMenuTarget = 0;                     // 1 bone, 2 image, 0 empty space
    int delState = 0;                          // 0 none, 1 open it, 2 showing
    Document::DeleteInfo delInfo;
    int delBonesAlso = 0, delSlotsAlso = 0;
    void openObjectMenu(ImVec2 mouse);
    void objectContextMenu();
    void requestDelete();                      // deletes the selection, asking first when parented objects go with it
    void deleteDialog();
    bool showLayouts = false;
    void layoutsDialog();
    SavedLayout captureLayout() const;
    void applyLayout(const SavedLayout& l);
    SplashScreen splash; // shown once at start-up (Settings > General can turn it off)
    int dopeRowHeight = 23, treeIndent = 14; // Settings > User interface / Dopesheet
    void loadSettingsFile();
    void saveSettingsFile() const;
    void applyTreeIndent();
    bool audioEnabled = true, wasPlaying = false; // play event audio during playback
    void autosaveTick();       // called every frame
    void clearAutosave();
    struct Recovery { bool pending = false; std::string original, when; } recovery;
    void checkRecovery();      // at start: is there a recovery copy from an earlier run?
    void recoveryDialog();     // asks whether to restore it
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
    void cmdImportPsd();
    void zoomToFit();
    void zoom100();

    // Views (implemented across Viewport.cpp, Panels.cpp, Timeline.cpp, Dialogs.cpp).
    void dockLayout(unsigned dockId);
    void menuBar();
    void shortcuts();
    void viewport();
    void viewportToolbar();
    void viewportToolbarImpl();
    void drawOverlay(ImDrawList* dl);
    void viewportInput();
    void treeView();
    void propertiesView();
    void animationsView();
    void skinsView();
    void eventsView();
    void dopesheetToolbar();
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
    void unpackerDialog();
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
    /** What one image / video export renders (ExportDialog.cpp). */
    struct ExportRender {
        int anim = -1;               // animation index, -1 = setup pose
        bool currentPose = false;    // one frame at poseTime
        float poseTime = 0;
        float fps = 30;
        float scale = 1;             // when sizeMode == 0
        int sizeMode = 0, sizePx = 100; // 1 = fit a pixel width, 2 = fit a pixel height
        int pad = 2;
        bool transparent = true;
        Color bg{0.2f, 0.2f, 0.2f, 1};
        int warmup = 1;              // physics steps before the first frame
        int frameFrom = 0, frameTo = -1; // inclusive; -1 = to the end
        float smoothing = 1;         // 0 nearest, 1 bilinear
        int supersample = 1;         // 1, 2 or 4: render larger and average down (multisample AA)
        bool drawImages = true;
        bool tightCrop = true;
    };
    void renderFrames(const ExportRender& r, std::vector<std::vector<uint8_t>>& frames, int& w, int& h);
    std::vector<int> visibleTimelines();
    Attachment* selectedAttachment(int* slotOut = nullptr);
    void beginRename();
};

/** Small widget helpers shared by views. */
bool dragFloatEdit(Document& doc, const char* label, float* v, float speed, const char* fmt = "%.2f", const char* undoLabel = "Edit");
/** Modern slider with a typed-value box; one undo step per drag or typed edit. */
bool sliderFloatEdit(Document& doc, const char* label, float* v, float lo, float hi, const char* fmt = "%.2f", const char* undoLabel = "Edit");
bool dragFloat2Edit(Document& doc, const char* label, float* v, float speed, const char* fmt = "%.2f", const char* undoLabel = "Edit");
bool colorEdit(Document& doc, const char* label, Color& c, bool alpha = true, const char* undoLabel = "Color");
bool checkboxEdit(Document& doc, const char* label, bool* v, const char* undoLabel = "Edit");
void helpMarker(const char* text);

} // namespace osteon
