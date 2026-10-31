// Runtime pose built from SkeletonData: world transforms, constraints, slot state.
#pragma once
#include "Model.h"

namespace osteon {

struct Skeleton;

/** World transform of a bone or of the skeleton's virtual root. */
struct WorldMatrix {
    float a = 1, b = 0, c = 0, d = 1, worldX = 0, worldY = 0;
};

struct Bone : WorldMatrix {
    int index = 0, parent = -1;
    std::vector<int> children;
    // Local pose (set by setup pose / animations / tools).
    float x = 0, y = 0, rotation = 0, scaleX = 1, scaleY = 1, shearX = 0, shearY = 0;
    Inherit inherit = Inherit::Normal;
    // Applied pose (local pose after constraints).
    float ax = 0, ay = 0, arotation = 0, ascaleX = 1, ascaleY = 1, ashearX = 0, ashearY = 0;
    bool sorted = false, active = true;

    void update(Skeleton& sk) { updateWorldTransform(sk, x, y, rotation, scaleX, scaleY, shearX, shearY); }
    void updateWorldTransform(Skeleton& sk, float x, float y, float rotation, float scaleX, float scaleY, float shearX, float shearY);
    /** Recomputes the applied local pose from the current world transform. */
    void updateAppliedTransform(Skeleton& sk);
    Vec2 worldToLocal(Vec2 world) const;
    Vec2 localToWorld(Vec2 local) const;
    /** Converts a world point to this bone's parent space (where x/y live). */
    Vec2 worldToParent(const Skeleton& sk, Vec2 world) const;
    Vec2 parentToWorld(const Skeleton& sk, Vec2 p) const;
    float worldRotationX() const { return atan2Deg(c, a); }
    float worldRotationY() const { return atan2Deg(d, b); }
    float worldScaleX() const { return std::sqrt(a * a + c * c); }
    float worldScaleY() const { return std::sqrt(b * b + d * d); }
    /** World-space direction (degrees) -> local rotation value. */
    float worldToLocalRotation(float worldRotation) const;
    float localToWorldRotation(float localRotation) const;
};

struct Slot {
    int index = 0, bone = 0;
    Color color, dark;
    bool hasDark = false;
    std::string attachment;  // current attachment name
    std::vector<float> deform; // per-attachment deform (owned by the slot, cleared on change)
    std::string deformFor;   // attachment the deform belongs to
    int sequenceIndex = -1;
};

struct IkConstraint { float mix = 1, softness = 0; int bendDirection = 1; bool compress = false, stretch = false; bool active = true; };
struct TransformConstraint { float mixRotate = 1, mixX = 1, mixY = 1, mixScaleX = 1, mixScaleY = 1, mixShearY = 1; bool active = true; };
struct PathConstraint {
    float position = 0, spacing = 0, mixRotate = 1, mixX = 1, mixY = 1; bool active = true;
    std::vector<float> spaces, positions, world, curves, lengths, segments; // scratch
};
struct PhysicsConstraint {
    float inertia = 1, strength = 100, damping = 1, massInverse = 1, wind = 0, gravity = 0, mix = 1;
    bool active = true, reset = true;
    // Simulation state.
    float ux = 0, uy = 0, cx = 0, cy = 0, tx = 0, ty = 0;
    float xOffset = 0, xVelocity = 0, yOffset = 0, yVelocity = 0;
    float rotateOffset = 0, rotateVelocity = 0, scaleOffset = 0, scaleVelocity = 0;
    float remaining = 0, lastTime = 0;
};

struct SliderConstraint { float time = 0, mix = 1; bool active = true; };

enum class Physics { None, Reset, Update, Pose };

struct Skeleton {
    const SkeletonData* data = nullptr;
    std::vector<Bone> bones;
    std::vector<Slot> slots;
    std::vector<int> drawOrder;
    std::vector<IkConstraint> ik;
    std::vector<TransformConstraint> transforms;
    std::vector<PathConstraint> paths;
    std::vector<PhysicsConstraint> physics;
    std::vector<SliderConstraint> sliders;
    int skin = 0; // index into data->skins
    float x = 0, y = 0, scaleX = 1, scaleY = 1;
    float time = 0; // physics clock
    Color color;

    enum class CacheType { Bone, Ik, Transform, Path, Physics, Slider };
    struct CacheItem { CacheType type; int index; };
    std::vector<CacheItem> updateCache;

    Skeleton() = default;
    explicit Skeleton(const SkeletonData& d) { build(d); }

    /** (Re)creates runtime objects from data and resets to the setup pose. */
    void build(const SkeletonData& d);
    /** True if the data's structure changed since build (counts differ). */
    bool needsRebuild(const SkeletonData& d) const;
    void updateCacheOrder();
    void setToSetupPose() { setBonesToSetupPose(); setSlotsToSetupPose(); }
    void setBonesToSetupPose();
    void setSlotsToSetupPose();
    void updateWorldTransform(Physics physics = Physics::Pose);
    void update(float delta) { time += delta; }

    const WorldMatrix& parentMatrix(int bone) const;
    const Attachment* attachmentFor(const Slot& slot) const;
    /** Computes world vertices for any attachment on `slot` (region: 4 corners BL,TL,TR,BR). */
    bool worldVertices(const Slot& slot, const Attachment& a, std::vector<float>& out) const;
    /** Axis-aligned bounds of all visible attachments. Returns false if empty. */
    bool bounds(float& minX, float& minY, float& maxX, float& maxY) const;

    WorldMatrix rootMatrix; // virtual parent of the root bone
};

// Constraint solvers (Constraints.cpp, PathConstraint.cpp, PhysicsConstraint.cpp).
void applyIk(Skeleton& sk, int index);
void applyIk1(Skeleton& sk, Bone& bone, float targetX, float targetY, bool compress, bool stretch, bool uniform, float alpha);
void applyIk2(Skeleton& sk, Bone& parent, Bone& child, float targetX, float targetY, int bendDir, bool stretch, bool uniform, float softness, float alpha);
void applyTransform(Skeleton& sk, int index);
float transformBoneProp(const Bone& b, int prop, bool local); // one of Rotate, X, Y, ScaleX, ScaleY, ShearY (local or world)
void applyPath(Skeleton& sk, int index);
void applyPhysics(Skeleton& sk, int index, Physics mode);
/** Slider constraint: poses the bones its animation drives, at the frame set by its time or its source bone (Animation.cpp). */
void applySlider(Skeleton& sk, int index);
/** Bones that an animation has bone timelines for. */
std::vector<int> animatedBones(const Animation& a, int boneCount);
/** Restarts the simulation of one physics constraint (index >= 0) or all of them (-1): offsets and velocities go to zero. */
void resetPhysics(Skeleton& sk, int index = -1);

} // namespace osteon
