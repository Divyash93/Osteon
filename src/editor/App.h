// The editor application: owns the document, renderer and all views (Dear ImGui, docked).
#pragma once
#include "Document.h"
#include <map>
#include <set>
#include "Gfx.h"
#include "Geometry.h"
#include "Util.h"
#include "imgui.h"

namespace osteon {

/** The colour of the i-th bone bound to a mesh in the Weights view; the viewport uses the same one for that bone. */
inline ImU32 weightPaletteColor(size_t i) {
    static const ImU32 palette[] = {IM_COL32(236, 84, 84, 255),  IM_COL32(84, 204, 110, 255), IM_COL32(84, 148, 244, 255), IM_COL32(244, 202, 64, 255),
                                    IM_COL32(196, 118, 236, 255), IM_COL32(84, 214, 214, 255), IM_COL32(244, 146, 64, 255), IM_COL32(160, 224, 96, 255),
                                    IM_COL32(240, 120, 176, 255), IM_COL32(132, 132, 244, 255), IM_COL32(200, 170, 120, 255), IM_COL32(150, 200, 200, 255)};
    return palette[i % (sizeof palette / sizeof palette[0])];
}

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
    enum class MeshMode { Off, Modify, Create, Delete, New } meshMode = MeshMode::Off;
    std::vector<int> meshSelection;
    bool meshTriangles = true; // Edit Mesh "Triangles": draw the triangle wireframe
    bool meshDim = false, meshIsolate = false; // Edit Mesh "Dim" (dim the image) and "Isolate" (hide other attachments)
    TraceOptions trace;                        // Trace settings: Detail, Concavity, Refinement, Alpha threshold, Padding
    bool traceLive = false;                    // after Trace, the sliders re-trace as they move
    std::vector<Vec2> newHull;                 // New: hull vertices placed so far (local to the slot's bone)
    void meshBeginNew();
    void meshFinishNew(bool apply);

    // Weights tool (Spine's Weights view).
    std::set<int> weightBones;          // bones selected in the Bones list: what the brush and the Weight slider change
    int weightBone = -1;                // the one most recently selected of them
    bool weightBrush = true;            // Brush mode; false = Direct mode
    enum class WeightMode { Add, Remove, Replace } weightMode = WeightMode::Add;
    float brushSize = 30, brushStrength = 0.5f, brushFeather = 100; // feather in percent of the size
    float weightValue = 100;            // Direct mode "Weight" slider, percent
    bool showPies = true, showOverlay = false, showSelectedOnly = false; // display options
    bool bindMode = false;              // Bind: click bones to bind them to the selected mesh
    std::vector<int> bindPending;       // the bones clicked so far in Bind mode
    float pruneThreshold = 5;           // percent
    int pruneMaxBones = 4;
    std::set<std::string> weightLocks;  // "slot/attachment/bone" of locked bones
    std::vector<std::vector<std::pair<int, float>>> weightClipboard; // Ctrl+C on selected vertices
    std::vector<int> lockedBonesFor(int slot, const Attachment& mesh) const;
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
