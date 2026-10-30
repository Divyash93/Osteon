// Setup-pose data model. Value semantics throughout (indices, not pointers), so a whole
// SkeletonData can be copied for undo snapshots and passed between threads safely.
#pragma once
#include "MathUtil.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace osteon {

enum class Inherit { Normal, OnlyTranslation, NoRotationOrReflection, NoScale, NoScaleOrReflection };
enum class BlendMode { Normal, Additive, Multiply, Screen };
enum class AttachmentType { Region, Mesh, LinkedMesh, BoundingBox, Path, Point, Clipping };

const char* inheritName(Inherit i);
Inherit inheritFromName(const std::string& s);
const char* blendName(BlendMode b);
BlendMode blendFromName(const std::string& s);
const char* attachmentTypeName(AttachmentType t);

struct BoneData {
    std::string name;
    int parent = -1;
    float length = 0;
    float x = 0, y = 0, rotation = 0, scaleX = 1, scaleY = 1, shearX = 0, shearY = 0;
    Inherit inherit = Inherit::Normal;
    bool skinRequired = false;
    Color color{0x98 / 255.0f, 0x98 / 255.0f, 0x98 / 255.0f, 1}; // Spine's default bone color
    std::string icon;
    // Editor-only state, persisted in the project file.
    bool visible = true;
};

struct SlotData {
    std::string name;
    int bone = 0;
    Color color;
    bool hasDark = false;
    Color dark{0, 0, 0, 1};
    std::string attachment; // setup-pose attachment name, empty = none
    BlendMode blend = BlendMode::Normal;
    bool visible = true; // editor-only
};

/**
 * Any attachment. Fields irrelevant to a type are ignored. Vertex attachments use
 * `vertices`/`bones` in Spine layout: unweighted = [x,y]*n (bone-local);
 * weighted = per vertex: bones[count, boneIndex*count], vertices[x,y,weight]*count.
 */
struct Attachment {
    AttachmentType type = AttachmentType::Region;
    std::string name;  // attachment name (key in skin)
    std::string path;  // image path, empty = name
    Color color;
    // Region / Point
    float x = 0, y = 0, rotation = 0, scaleX = 1, scaleY = 1;
    float width = 0, height = 0;
    // Vertex attachments
    std::vector<int> bones;     // empty = unweighted
    std::vector<float> vertices;
    int worldVerticesLength = 0;
    // Mesh
    std::vector<float> uvs;     // normalised, v down
    std::vector<int> triangles;
    int hullLength = 0;         // in vertices (not floats)
    std::vector<int> edges;     // pairs of vertex indices (editor/nonessential)
    // Linked mesh
    std::string parentMesh, parentSkin;
    bool inheritTimelines = true;
    // Path
    bool closed = false, constantSpeed = true;
    std::vector<float> lengths;
    // Clipping
    std::string endSlot;
    // Sequence (frame-by-frame images): count 0 = none
    int seqCount = 0, seqStart = 1, seqDigits = 0, seqSetup = 0;

    const std::string& imagePath() const { return path.empty() ? name : path; }
    bool isVertex() const { return type == AttachmentType::Mesh || type == AttachmentType::LinkedMesh || type == AttachmentType::BoundingBox || type == AttachmentType::Path || type == AttachmentType::Clipping; }
    bool isWeighted() const { return !bones.empty(); }
    int vertexCount() const { return worldVerticesLength / 2; }
    /** Floats in a deform key: x,y per vertex (unweighted) or per influence (weighted). */
    int deformLength() const { return isWeighted() ? (int)(vertices.size() / 3) * 2 : (int)vertices.size(); }
    /** Region corners in bone space: BL, TL, TR, BR. */
    void regionCorners(float out[8]) const;
};

using SlotAttachments = std::map<std::string, Attachment>;

struct Skin {
    std::string name;
    std::map<int, SlotAttachments> attachments; // slot index -> name -> attachment
    std::vector<int> bones;                     // skin-required bones
    std::vector<std::string> constraints;       // skin-required constraints
    Color color{0.99f, 0.61f, 0.04f, 1};

    Attachment* get(int slot, const std::string& name);
    const Attachment* get(int slot, const std::string& name) const;
    void set(int slot, const Attachment& a) { attachments[slot][a.name] = a; }
    void remove(int slot, const std::string& name);
};

struct EventData {
    std::string name;
    int intValue = 0;
    float floatValue = 0;
    std::string stringValue, audio;
    float volume = 1, balance = 0;
};

struct IkConstraintData {
    std::string name;
    int order = 0;
    bool skinRequired = false;
    std::vector<int> bones; // 1 or 2
    int target = 0;
    float mix = 1, softness = 0;
    int bendDirection = 1;
    bool compress = false, stretch = false, uniform = false;
};

struct TransformConstraintData {
    std::string name;
    int order = 0;
    bool skinRequired = false;
    std::vector<int> bones;
    int target = 0;
    float mixRotate = 1, mixX = 1, mixY = 1, mixScaleX = 1, mixScaleY = 1, mixShearY = 1;
    float offsetRotation = 0, offsetX = 0, offsetY = 0, offsetScaleX = 0, offsetScaleY = 0, offsetShearY = 0;
    bool relative = false, local = false;
};

struct PathConstraintData {
    std::string name;
    int order = 0;
    bool skinRequired = false;
    std::vector<int> bones;
    int target = 0; // slot index
    enum class Position { Fixed, Percent } positionMode = Position::Percent;
    enum class Spacing { Length, Fixed, Percent, Proportional } spacingMode = Spacing::Length;
    enum class Rotate { Tangent, Chain, ChainScale } rotateMode = Rotate::Tangent;
    float offsetRotation = 0, position = 0, spacing = 0;
    float mixRotate = 1, mixX = 1, mixY = 1;
};

struct PhysicsConstraintData {
    std::string name;
    int order = 0;
    bool skinRequired = false;
    int bone = 0;
    float x = 0, y = 0, rotate = 0, scaleX = 0, shearX = 0, limit = 5000;
    float step = 1.0f / 60, inertia = 1, strength = 100, damping = 1, massInverse = 1, wind = 0, gravity = 0, mix = 1;
};

// ---- Animation data ----------------------------------------------------------------------

enum class CurveType { Linear, Stepped, Bezier };

enum class TimelineType {
    Rotate, Translate, TranslateX, TranslateY, Scale, ScaleX, ScaleY, Shear, ShearX, ShearY, Inherit,
    Attachment, RGBA, RGB, Alpha, RGBA2, RGB2,
    Deform, Sequence, DrawOrder, Event, Ik, Transform, Path, PathMix, Physics
};

/** Number of curve-interpolated values a timeline type carries per key. */
int timelineValueCount(TimelineType t);
const char* timelineName(TimelineType t);
bool isBoneTimeline(TimelineType t);
bool isSlotTimeline(TimelineType t);

struct Key {
    float time = 0;
    float v[7] = {0, 0, 0, 0, 0, 0, 0}; // value(s); meaning depends on timeline type
    CurveType curve = CurveType::Linear;
    std::vector<float> bezier;     // 4 floats (cx1, cy1, cx2, cy2) per value, absolute
    std::string str;               // attachment name / event name
    std::string str2;              // event string override (flag3 = set)
    std::vector<float> deform;     // deform offsets (full length, relative to setup vertices)
    std::vector<int> drawOrder;    // slot indices in draw order (empty = setup order)
    bool flag1 = false, flag2 = false, flag3 = false; // ik: bendPositive, compress, stretch; event: int/float/string overridden
    int intValue = 0;
    float volume = 1, balance = 0;
};

struct Timeline {
    TimelineType type = TimelineType::Rotate;
    int target = -1;             // bone, slot, or constraint index
    std::string skin, attachment; // deform / sequence
    std::vector<Key> keys;       // sorted by time
    int findKey(float time, float eps = 1e-4f) const;
    Key& setKey(float time);     // finds or inserts a key at time
};

struct Animation {
    std::string name;
    std::vector<Timeline> timelines;
    float duration() const;
    Timeline* find(TimelineType type, int target, const std::string& skin = {}, const std::string& att = {});
    Timeline& get(TimelineType type, int target, const std::string& skin = {}, const std::string& att = {});
};

struct SkeletonData {
    std::string name = "skeleton";
    std::vector<BoneData> bones;
    std::vector<SlotData> slots;  // in setup draw order
    std::vector<Skin> skins;      // skins[0] is "default"
    std::vector<EventData> events;
    std::vector<Animation> animations;
    std::vector<IkConstraintData> ik;
    std::vector<TransformConstraintData> transforms;
    std::vector<PathConstraintData> paths;
    std::vector<PhysicsConstraintData> physics;
    float x = 0, y = 0, width = 0, height = 0, fps = 30, referenceScale = 100;
    std::string imagesPath, audioPath, hash, version = "4.2.00";

    int findBone(const std::string& n) const;
    int findSlot(const std::string& n) const;
    int findSkin(const std::string& n) const;
    int findAnimation(const std::string& n) const;
    int findEvent(const std::string& n) const;
    Skin& defaultSkin();
    bool isDescendant(int bone, int ancestor) const;
    std::vector<int> childrenOf(int bone) const;
    std::string uniqueBoneName(const std::string& base) const;
    std::string uniqueSlotName(const std::string& base) const;
    std::string uniqueAnimationName(const std::string& base) const;
    /** Looks an attachment up in `skin` first, then the default skin. */
    const Attachment* findAttachment(int skinIndex, int slot, const std::string& name) const;

    // Structural edits that keep every index reference consistent.
    int addBone(const BoneData& b);
    void removeBone(int index);           // removes descendants, their slots, and references
    void setParent(int bone, int newParent); // keeps world pose is the caller's job
    /** Moves `bone` next to its sibling `target` (before it, or after when `after`). Both must share a parent;
     *  returns false otherwise. Bone indices are remapped everywhere, parents stay before their children. */
    bool moveBone(int bone, int target, bool after);
    int addSlot(const SlotData& s);
    void removeSlot(int index);
    void moveSlot(int from, int to);       // setup draw order
    /** Ensures parents precede children (required by runtimes); remaps all indices. */
    void sortBones();
};

} // namespace osteon
