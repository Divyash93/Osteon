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
enum class SelType { None, Skeleton, Bone, Slot, Attachment, Ik, Transform, Path, Physics, Skin, Animation, Event, Image };
enum class Channel { Rotate = 1, Translate = 2, Scale = 4, Shear = 8, All = 15 };

struct Sel {
    SelType type = SelType::None;
    int index = -1;       // bone/slot/constraint/skin/animation/event index
    std::string name;     // attachment or image name
    bool operator==(const Sel& o) const { return type == o.type && index == o.index && name == o.name; }
};

struct Document {
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
    /** Shift+click in the tree: selects the items from `from` to `to` in `order` (inclusive). Ctrl keeps the existing selection. */
    void selectRange(const Sel& from, const Sel& to, const std::vector<Sel>& order, bool keepExisting);
    std::vector<int> selectedBones() const;
    std::vector<int> selectedSlots() const;
    /** Slot index for a slot/attachment selection, else -1. */
    int selectionSlot() const;
    void validateSelection();

    // ---- bone edits (route to setup data or animation keys depending on mode) ----
    struct LocalPose { float rotation, x, y, sx, sy, shx, shy; };
    LocalPose bonePose(int bone) const; // current local values shown in the UI
    void setBonePose(int bone, const LocalPose& p, int channels);
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
    int createBone(int parent, Vec2 worldStart, Vec2 worldEnd, const std::string& name = "bone");
    int createSlot(int bone, const std::string& name);
    void addRegionAttachment(int slot, const std::string& imageName, Vec2 worldPos);
    void deleteSelection();
    void duplicateSelection();
    bool renameSelection(const std::string& newName);
    void reparentBones(const std::vector<int>& bones, int newParent); // keeps world pose
    void reparentSlot(int slot, int newBone);                        // keeps world placement
    int createAnimation(const std::string& name);
    int createSkin(const std::string& name);
    int createEvent(const std::string& name);
    int createIk(const std::string& name, const std::vector<int>& bones, int target);
    int createTransform(const std::string& name, const std::vector<int>& bones, int target);
    int createPath(const std::string& name, const std::vector<int>& bones, int slot);
    int createPhysics(const std::string& name, int bone);
    void convertToMesh(int slot, const std::string& att);
    /** Adds a linked mesh that reuses `mesh`'s geometry and weights (to the active skin). */
    void createLinkedMesh(int slot, const std::string& mesh);
    void addAttachment(int slot, AttachmentType type, const std::string& name);

    // ---- images and files ----
    std::string imagePathFor(const Attachment& a, int sequenceIndex = -1) const;
    std::string imageFileFor(const std::string& name) const;
    std::vector<std::string> imageNames; // relative names (no extension) found in imagesDir
    void scanImages();
    void newProject();
    bool openFile(const std::string& path, std::string& error);    // .osteon or Spine .json
    bool saveProject(const std::string& path, std::string& error);
    bool exportJson(const std::string& path, bool pretty, bool nonessential, std::string& error, const std::string& version = "4.2.00");
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
