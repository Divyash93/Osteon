// Editor document: skeleton data, runtime pose, selection, modes, undo history, project I/O.
#pragma once
#include "Animation.h"
#include "SkeletonJson.h"
#include <deque>
#include <optional>

namespace osteon {

enum class Mode { Setup, Animate };
enum class Tool { Pose, Weights, Create, Rotate, Translate, Scale, Shear };
enum class Axes { Local, Parent, World };
enum class SelType { None, Skeleton, Bone, Slot, Attachment, Ik, Transform, Path, Physics, Slider, Skin, Animation, Event, Image };
enum class Channel { Rotate = 1, Translate = 2, Scale = 4, Shear = 8, All = 15 };

struct Sel {
    SelType type = SelType::None;
    int index = -1;       // bone/slot/constraint/skin/animation/event index
    std::string name;     // attachment or image name
    bool operator==(const Sel& o) const { return type == o.type && index == o.index && name == o.name; }
};

struct Document {
    static inline int defaultFps = 30; // frames per second of new projects (Settings)
    SkeletonData data;
    Skeleton sk;
    std::string projectPath; // .osteon or .json, empty = untitled
    std::string imagesDir;   // absolute images folder
    bool dirty = false;
    std::vector<std::string> warnings; // from the last import

    Mode mode = Mode::Setup;
    Tool tool = Tool::Rotate;
    Axes axes = Axes::Local;
    bool autoKey = true;
    // Physics (see Spine's Physics Constraints page).
    bool simulate = true;       // Simulate: live physics on playback; off = scrubbing re-runs the physics from frame 0
    bool deterministic = false; // Deterministic: scrubbing always re-runs the physics from frame 0
    bool simInvalid = false;    // set when a physics setting changes, so a stopped pose is re-simulated
    float simTime = -1;
    int simAnim = -2;
    /** Keys a physics property at the current frame; target = constraint index, or -1 for the global row. */
    void keyPhysics(TimelineType type, int target, float value);
    /** Keys a reset (restart of the simulation) at the current frame. */
    void keyPhysicsReset(int target);
    /** True when the animation has a key for this physics property at the current frame. */
    bool hasPhysicsKey(TimelineType type, int target) const;
    /** Reset (one constraint) or Reset All (-1): restarts the simulation now; keyed too when Auto Key is on in Animate mode. */
    void resetPhysics(int target);
    /** Adjust (dopesheet): while on, a bone edit in Animate mode shifts the keys in adjustKeys (timeline, key) by the same amount instead of keying. */
    bool adjust = false;
    std::vector<std::pair<int, int>> adjustKeys;
    bool boneCompensation = false, imageCompensation = false;
    int skin = 0;
    int animation = -1;
    float time = 0;     // seconds
    bool playing = false, playBackward = false, loop = true;
    float speed = 1;
    float loopStart = -1, loopEnd = -1; // seconds, -1 = whole animation

    std::vector<Sel> selection;

    // Unkeyed edits made in Animate mode with auto-key off (shown until keyed or time changes).
    struct Pending { int channels = 0; float rotation = 0, x = 0, y = 0, sx = 1, sy = 1, shx = 0, shy = 0; };
    std::map<int, Pending> pending;
    // Unkeyed mesh deforms, keyed by (slot, attachment): the skin to key into and the vertex offsets.
    struct PendingDeform { std::string skin; std::vector<float> offsets; };
    std::map<std::pair<int, std::string>, PendingDeform> pendingDeform;
    void clearPending() { pending.clear(); pendingDeform.clear(); }

    // ---- history ----
    struct Snapshot { SkeletonData data; std::vector<Sel> selection; std::string label; };
    std::deque<Snapshot> undoStack, redoStack;
    std::optional<Snapshot> openEdit;
    /** Records the current state before a one-shot change. */
    void checkpoint(const std::string& label);
    /** Begins a continuous edit (drag, slider); committed by commitEdit(). */
    void beginEdit(const std::string& label);
    void commitEdit();
    void cancelEdit();
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    void undo();
    void redo();
    std::string undoLabel() const { return undoStack.empty() ? "" : undoStack.back().label; }
    std::string redoLabel() const { return redoStack.empty() ? "" : redoStack.back().label; }

    // ---- pose ----
    /** Rebuilds the runtime skeleton if needed and poses it for the current mode/animation/time. */
    void updatePose(float physicsDelta = 0);
    Animation* currentAnimation();
    float animationDuration();
    void setTime(float t);
    void setMode(Mode m);

    // ---- selection ----
    Sel primary() const { return selection.empty() ? Sel{} : selection.back(); }
    bool isSelected(const Sel& s) const;
    void select(const Sel& s, bool additive = false);
    void clearSelection() { selection.clear(); }
    /** Selects every visible bone and slot (Ctrl+A). */
    void selectAll();
    /** Tree drag: puts `bone` before/after its sibling `target` (same parent). Returns false when they are not siblings. */
    bool moveBone(int bone, int target, bool after);
    /** Tree drag: puts `slot` before/after slot `target` in the draw order, taking on target's bone if it differs. */
    bool moveSlotNextTo(int slot, int target, bool after);
    /** Shift+click in the tree: selects the items from `from` to `to` in `order` (inclusive). Ctrl keeps the existing selection. */
    void selectRange(const Sel& from, const Sel& to, const std::vector<Sel>& order, bool keepExisting);
    std::vector<int> selectedBones() const;

    // ---- object operations (the viewport's right-click menu; DocumentObjects.cpp) ----
    struct ObjectClipboard {
        struct B { BoneData data; std::string parent; };                                  // parent by name
        struct S { SlotData data; std::string bone; std::map<std::string, SlotAttachments> atts; }; // skin name -> attachments
        std::vector<B> bones;
        std::vector<S> slots;
        bool empty() const { return bones.empty() && slots.empty(); }
    };
    struct TransformClipboard { int kind = 0; /* 1 bone, 2 attachment */ float x = 0, y = 0, rotation = 0, scaleX = 1, scaleY = 1; float bone[7] = {0, 0, 0, 1, 1, 0, 0}; };
    struct SlotPropertyClipboard { bool valid = false; Color color, dark; bool hasDark = false; BlendMode blend = BlendMode::Normal; };
    enum class DrawOrderMove { Forward, Backward, ToFront, ToBack };
    struct DeleteInfo { int kind = 0; /* 1 bone, 2 slot */ std::string name, boneName; int bone = -1; int childBones = 0, slots = 0; bool canDeleteBone = false; };
    static ObjectClipboard& clipboard();
    static TransformClipboard& transformClipboard();
    static SlotPropertyClipboard& slotPropertyClipboard();
    /** Copies the selected bones (with their sub-trees) and slots (with all their attachments). */
    bool copySelection();
    /** Pastes copies: bones under their original parents, slots on their original bones; with `special`, everything goes under the selected bone. Returns how many objects were created. */
    int pasteObjects(bool special);
    bool copyTransform();
    int pasteTransform();
    bool copySlotProperty();
    int pasteSlotProperty();
    void moveSlotsInDrawOrder(DrawOrderMove how);
    /** Hide / Show the selection, or everything else; Lock makes objects unpickable in the viewport. */
    void setVisibility(bool others, bool visible);
    void setLocked(bool others, bool locked);
    void showAndUnlockAll();
    /** What deleting the current selection would remove (for the confirmation). */
    DeleteInfo describeDelete() const;
    /** Deletes the selected slot together with the bone it is parented to (and that bone's children and slots). */
    void deleteSlotAndItsBone();
    /** (child, parent) for every selected bone that has a parent: what the viewport's red dashed parent guide draws. A root bone gives none. */
    std::vector<std::pair<int, int>> selectedParentLinks() const;
    std::vector<int> selectedSlots() const;
    /** Slot index for a slot/attachment selection, else -1. */
    int selectionSlot() const;
    void validateSelection();

    // ---- bone edits (route to setup data or animation keys depending on mode) ----
    struct LocalPose { float rotation, x, y, sx, sy, shx, shy; };
    LocalPose bonePose(int bone) const; // current local values shown in the UI
    void setBonePose(int bone, const LocalPose& p, int channels);
    /** Setup mode, after a bone changed: restores children (bone compensation) and the bone's images (image compensation) to their pre-edit world placement. */
    void compensate(int bone, const Skeleton& before);
    /** Pose tool: drag bone `bone` from `grab` to `mouse` (world space). The bone turns about its own joint so
     *  it points at the mouse, and its image and descendants follow; a bone with no parent moves instead.
     *  `start` is the bone's pose and `startOrigin` its world origin when the drag began. */
    void poseDrag(int bone, const LocalPose& start, Vec2 startOrigin, Vec2 grab, Vec2 mouse);
    void keyBone(int bone, int channels);           // writes keys from the current pose
    void keyBoneIfAuto(int bone, int channels);
    bool hasKey(int bone, TimelineType t) const;    // key exists at current time
    /** Keyframe state of a bone (slot = false) or slot (slot = true) in the current animation:
     *  0 = no keys, 1 = keyed at other times, 2 = keyed at the current frame. */
    int keyState(int target, bool slot) const;
    /** Same states for a mesh attachment's deform keys (animated vertices). */
    int deformKeyState(int slot, const std::string& attachment) const; // 3 = pending (moved, not keyed)
    /** Skin that holds `attachment` for `slot` (the active skin, else the default skin). */
    std::string skinNameFor(int slot, const std::string& attachment) const;
    /** Animate mode: sets a mesh's vertex offsets. Keys them with Auto Key, otherwise keeps them pending. */
    void setDeform(int slot, const std::string& attachment, const std::vector<float>& offsets);
    void keyDeform(int slot, const std::string& attachment); // keys a pending deform at the current frame
    void keySlotColor(int slot);
    void keySlotAttachment(int slot);
    void setSlotAttachment(int slot, const std::string& name); // setup or keyed
    void setSlotColor(int slot, const Color& c);
    void deleteKeysAt(float time, const std::vector<int>& bones);

    // ---- structural operations (all undoable) ----
    /** Deletes the deform keys of one attachment (they no longer fit once its vertices change). */
    void dropDeformKeys(int slot, const std::string& attachment);
    /** Trace, Generate, Reset and New remove a mesh's weights and deform keys, as in Spine. Call after checkpoint(). */
    void dropMeshBindings(int slot, Attachment& mesh);
    /** Deletes mesh vertices (hull vertices shrink the hull); one undo step. False when it would leave fewer than 3, or the mesh is weighted. */
    bool deleteMeshVertices(int slot, const std::string& attachment, std::vector<int> vertices);
    int createBone(int parent, Vec2 worldStart, Vec2 worldEnd, const std::string& name = "bone");
    /** Splits a bone into `parts` pieces (nested: each a child of the previous; otherwise all share the bone's parent), equal or Fibonacci lengths.
     *  The last piece takes over the old children. Returns it, or -1 for a zero-length bone. */
    int splitBone(int bone, int parts = 2, bool nested = true, bool fibonacci = false);
    int createSlot(int bone, const std::string& name);
    /** attachmentName (default: the image name) and intoActiveSkin place the region under another name in the active skin, e.g. to fill a placeholder. */
    void addRegionAttachment(int slot, const std::string& imageName, Vec2 worldPos, const std::string& attachmentName = {}, bool intoActiveSkin = false);
    /** Adds a skin placeholder (a named attachment slot that skins fill in) to a slot. Returns its name. */
    std::string createPlaceholder(int slot, const std::string& base = "placeholder");
    bool isPlaceholder(int slot, const std::string& name) const;
    /** Skin bones / skin constraints: a "skin required" bone or constraint is only active while a skin that lists it is active. */
    /** Right click on a bone's eye: shows or hides the bone, all its child bones and the slots on them (one undo step). */
    void setSubtreeVisible(int bone, bool visible);
    void setSkinBone(int skin, int bone, bool in);
    void setSkinConstraint(int skin, const std::string& constraintName, bool in);
    void deleteSelection();
    void duplicateSelection();
    bool renameSelection(const std::string& newName);
    void reparentBones(const std::vector<int>& bones, int newParent); // keeps world pose
    void reparentSlot(int slot, int newBone);                        // keeps world placement
    int createAnimation(const std::string& name);
    int createSkin(const std::string& name);
    int createEvent(const std::string& name);
    /** One bone, or a parent and its child. target < 0 with newTargetAt creates a target bone there (one undo step). Returns -1 when refused. */
    int createIk(const std::string& name, const std::vector<int>& bones, int target, const Vec2* newTargetAt = nullptr);
    int createTransform(const std::string& name, const std::vector<int>& bones, int target);
    int createPath(const std::string& name, const std::vector<int>& bones, int slot);
    int createPhysics(const std::string& name, int bone);
    /** A slider constraint for `animation` (-1 = chosen later in the Properties panel). */
    int createSlider(const std::string& name, int animation);
    void convertToMesh(int slot, const std::string& att);
    /** The Mesh checkbox unchecked: the mesh becomes a region attachment again, placed where the image was. */
    void convertToRegion(int si, const std::string& name);
    /** Adds a linked mesh that reuses `mesh`'s geometry and weights (to the active skin). */
    void createLinkedMesh(int slot, const std::string& mesh);
    void addAttachment(int slot, AttachmentType type, const std::string& name);

    // ---- images and files ----
    std::string imagePathFor(const Attachment& a, int sequenceIndex = -1) const;
    std::string imageFileFor(const std::string& name) const; // cached for a second: it is called per attachment per frame
    mutable std::map<std::string, std::string> imageFileCache;
    mutable std::string imageFileCacheDir;
    mutable long long imageFileCacheStamp = 0;
    std::vector<std::string> imageNames; // relative names (no extension) found in imagesDir
    void scanImages();
    void newProject();
    bool openFile(const std::string& path, std::string& error);    // .osteon or Spine .json
    bool saveProject(const std::string& path, std::string& error);
    /** A copy of the project for Auto-save: writes `path` without touching the document's own path or its unsaved flag. */
    bool writeBackup(const std::string& path, std::string& error);
    /** Opens a backup written by writeBackup as the unsaved project `original` (empty = untitled). */
    bool restoreBackup(const std::string& backup, const std::string& original, std::string& error);
    bool exportJson(const std::string& path, bool pretty, bool nonessential, std::string& error, const std::string& version = "4.2.00",
                    float scale = 1, bool cleanUp = false);
    /** Creates one slot + region attachment per image in imagesDir that has no slot yet. */
    int importImagesAsSlots();
    /**
     * Imports the visible layers of a PSD: one PNG per layer in the output folder, plus a slot and region
     * attachment on the root bone positioned as in the PSD (canvas centred on the origin, y up).
     * Returns the layer count, or -1 with `error` set. `hidden` receives the number of skipped hidden layers.
     */
    int importPsd(const std::string& psdPath, std::string& error, int* hidden = nullptr);

    std::string editorState() const;      // JSON for the "osteon" block
    void applyEditorState(const std::string& json);
    std::vector<std::string> recentFiles;
    void addRecent(const std::string& path);
    void loadRecent();
    void saveRecent();
};

} // namespace osteon
