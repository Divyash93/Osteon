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
    void addAttachment(int slot, AttachmentType type, const std::string& name);

    // ---- images and files ----
    std::string imagePathFor(const Attachment& a) const;
    std::string imageFileFor(const std::string& name) const;
    std::vector<std::string> imageNames; // relative names (no extension) found in imagesDir
    void scanImages();
    void newProject();
    bool openFile(const std::string& path, std::string& error);    // .osteon or Spine .json
    bool saveProject(const std::string& path, std::string& error);
    bool exportJson(const std::string& path, bool pretty, bool nonessential, std::string& error);
    /** Creates one slot + region attachment per image in imagesDir that has no slot yet. */
    int importImagesAsSlots();

    std::string editorState() const;      // JSON for the "osteon" block
    void applyEditorState(const std::string& json);
    std::vector<std::string> recentFiles;
    void addRecent(const std::string& path);
    void loadRecent();
    void saveRecent();
};

} // namespace osteon
