// Mesh geometry tools: triangulation, image hull tracing, automatic weights, atlas packing.
#pragma once
#include "Skeleton.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace osteon {

// ---- Triangulation ------------------------------------------------------------------------

/** Ear clipping of a simple polygon [x,y]*n (either winding). Returns index triples. */
std::vector<int> triangulate(const std::vector<float>& polygon);

/**
 * Constrained Delaunay triangulation: the first hullCount points are the hull polygon (its
 * edges are never flipped), the rest are interior points inserted into it. Points that
 * duplicate an existing point or lie outside the hull are left unused.
 */
std::vector<int> triangulateMesh(const std::vector<float>& points, int hullCount);

/** Signed polygon area (positive when x*y' - x'*y sums positive). */
float polygonArea(const std::vector<float>& polygon);
bool pointInPolygon(const std::vector<float>& polygon, float x, float y);

// ---- Image hull ---------------------------------------------------------------------------

/**
 * Traces the outline of the largest 8-connected component of pixels with alpha > threshold,
 * simplifies it (Douglas-Peucker) to at most maxVertices and offsets it outward by padding.
 * Pixel coordinates (x right, y down); output has positive polygonArea (clockwise on screen).
 */
std::vector<float> traceHull(const uint8_t* rgba, int w, int h, int alphaThreshold, int maxVertices, float padding);

/** Trace settings, as Spine's Mesh Tools: Detail, Concavity and Refinement run 0-100. */
struct TraceOptions {
    float detail = 40;       // how many vertices the hull gets (traceVertexBudget)
    float concavity = 50;    // priority for vertices in concave areas of the outline
    float refinement = 50;   // effort spent sliding vertices to hug the outline
    int alphaThreshold = 10; // pixels with alpha <= this are ignored
    float padding = 2;       // pixels between the image and the hull
    int seed = 0;            // Refresh: a different seed gives slightly different results
};
/** The vertex count Detail aims for. */
int traceVertexBudget(float detail);
std::vector<float> traceHull(const uint8_t* rgba, int w, int h, const TraceOptions& options);

/** Grid points (spacing apart) strictly inside the hull and at least spacing/2 from its edges. */
std::vector<float> generateInteriorPoints(const std::vector<float>& hull, float spacing);

/**
 * One click of Generate on a mesh (local vertices, first hullLength are the hull): a plain four-corner quad gets a
 * row-and-column grid; otherwise points are added where the mesh is coarsest. Returns the new points only.
 */
std::vector<float> generateVertices(const std::vector<float>& vertices, int hullLength, const std::vector<int>& triangles);

/** Least-squares affine map dst = M * (src, 1) over n matching points (src and dst are [x,y]*n); cx and cy are the two rows. */
bool fitAffine(const std::vector<float>& src, const std::vector<float>& dst, int n, double cx[3], double cy[3]);

/**
 * Path attachments store [handle in, point, handle out] per point. Cumulative setup-pose length at the end of each Bezier curve
 * (one per curve: every point's segment to the next, plus the closing curve of a closed path).
 */
std::vector<float> computePathLengths(const std::vector<float>& vertices, bool closed);
/** Splits the curve after point `curve` of a path at t (0-1), inserting a new point whose handles keep the shape. Returns its index. */
int splitPathCurve(std::vector<float>& vertices, bool closed, int curve, float t);

/** UV at local point (x, y) from the triangle that contains it. False when outside every triangle. */
bool interpolateUv(const std::vector<float>& vertices, const std::vector<float>& uvs, const std::vector<int>& triangles, float x, float y, float* uv);

// ---- Weights ------------------------------------------------------------------------------

/** Number of vertices of a vertex attachment, from its bones/vertices arrays. */
int countVertices(const Attachment& mesh);
/** World positions [x,y]*n of a vertex attachment in the skeleton's current pose (no deform). */
std::vector<float> meshWorldVertices(const Attachment& mesh, const Skeleton& sk, int slotBone);

/**
 * Converts to weighted form over `bones` with inverse-distance^falloff weights to each bone
 * segment (max 4 influences, normalised). World positions are preserved.
 */
void bindWeights(Attachment& mesh, const Skeleton& sk, int slotBone, const std::vector<int>& bones, float falloff = 2);
/** Converts back to unweighted vertices relative to slotBone, preserving world positions. */
void unbindWeights(Attachment& mesh, const Skeleton& sk, int slotBone);
/**
 * Rescales each vertex's weights to sum to 1. World positions are kept when every influence's
 * local coordinates map to the same world point (always true right after bind/setVertexWeight).
 */
void normalizeWeights(Attachment& mesh);
/**
 * Prune: drops weights below `threshold`, then (when maxBones > 0) all but each vertex's `maxBones` strongest, and
 * gives the removed weight to the bones that stay. Each vertex always keeps its strongest bone.
 */
void pruneWeights(Attachment& mesh, float threshold, int maxBones = 0);
/** How many bone influences a prune with these settings would remove (for the live preview). */
int pruneCount(const Attachment& mesh, float threshold, int maxBones = 0);
/**
 * Sets one bone's weight on a vertex, scaling its other influences to fill the remainder;
 * adds the bone if missing, drops influences that reach 0. World position is preserved.
 * `locked` bones keep their weights: they take no part in the redistribution (and a locked `bone` is left alone).
 * A bone that is the only free influence stays at 100% of what the locked bones leave.
 */
void setVertexWeight(Attachment& mesh, const Skeleton& sk, int vertex, int bone, float weight, const std::vector<int>& locked = {});

struct VertexWeightEdit { int vertex; int bone; float weight; };
/** Several setVertexWeight edits in one pass (brush strokes), applied in order. */
void setVertexWeights(Attachment& mesh, const Skeleton& sk, const std::vector<VertexWeightEdit>& edits, const std::vector<int>& locked = {});

/** The weight `bone` has on `vertex` (0 when unbound or the mesh is not weighted). */
float boneWeightAt(const Attachment& mesh, int vertex, int bone);
/** The bones a weighted mesh is bound to, sorted. */
std::vector<int> boundBones(const Attachment& mesh);

/**
 * Auto: weights from the mesh's own connectivity, measured along its edges, so a gap in the mesh stops a bone's
 * influence. `vertices` limits it to those vertices (all when empty); `onlyBones` recomputes just those bones
 * (the others share what is left). On an unweighted mesh this is the first binding.
 */
void autoWeights(Attachment& mesh, const Skeleton& sk, int slotBone, const std::vector<int>& bones, const std::vector<int>& vertices = {},
                 const std::vector<int>& onlyBones = {}, float falloff = 2);
/** Binds more bones to a mesh: each new bone takes the weight auto would give it, the existing bones share the rest. */
void addBonesToBinding(Attachment& mesh, const Skeleton& sk, int slotBone, const std::vector<int>& newBones, float falloff = 2);
/** Smooth: averages each vertex's weights with its neighbours' (`vertices` empty = all). Repeat to spread further. */
void smoothWeights(Attachment& mesh, const Skeleton& sk, const std::vector<int>& vertices = {}, float amount = 0.5f);
/** Exchanges the weights of two bones. */
void swapBoneWeights(Attachment& mesh, const Skeleton& sk, int boneA, int boneB);
/** Update bindings: the vertices stay where they are now, and that becomes their bind position for the bones. */
void updateBindings(Attachment& mesh, const Skeleton& sk);
/** Unbinds one bone; a vertex left with none follows the slot's bone, and a mesh left with none is unweighted. */
void removeBoneWeights(Attachment& mesh, const Skeleton& sk, int slotBone, int bone);

using WeightSet = std::vector<std::vector<std::pair<int, float>>>; // per vertex: (bone, weight)
/** Copy the weights of `vertices` (in that order). */
WeightSet copyWeights(const Attachment& mesh, const std::vector<int>& vertices);
/** Paste onto the same number of vertices; returns false when the counts differ. World positions are kept. */
bool pasteWeights(Attachment& mesh, const Skeleton& sk, const std::vector<int>& vertices, const WeightSet& set);

// ---- Atlas packing ------------------------------------------------------------------------

/** w,h are the unrotated size; a rotated rect occupies h x w on its page. */
struct PackRect {
    int id, w, h, x = 0, y = 0, page = 0;
    bool rotated = false;
};

/**
 * MaxRects (best short side fit) multi-page packer. Rects that cannot fit an empty page get
 * page = -1. With pow2, maxW/maxH are rounded down to powers of two. Returns the page count.
 */
int packRects(std::vector<PackRect>& rects, int maxW, int maxH, int padding, bool allowRotate, bool pow2);
/** Used extent of a page (optionally rounded up to powers of two). */
std::pair<int, int> packedPageSize(const std::vector<PackRect>& rects, int page, bool pow2);

/** libGDX / Spine 4.x .atlas text. names is parallel to rects. */
std::string writeAtlas(const std::vector<std::string>& pageFiles, const std::vector<std::pair<int, int>>& pageSizes,
                       const std::vector<PackRect>& rects, const std::vector<std::string>& names);

// ---- Atlas reading (texture unpacker) -----------------------------------------------------

struct AtlasPage { std::string file; int w = 0, h = 0; };
struct AtlasRegion {
    std::string name;
    int page = 0;                 // index into the pages
    int x = 0, y = 0, w = 0, h = 0; // on the page; w,h are the unrotated size
    bool rotated = false;         // stored 90 degrees clockwise (h x w on the page)
    int origW = 0, origH = 0, offX = 0, offY = 0; // trimmed regions: original size and offset from the bottom-left
    int index = -1;               // frame index for sequences
};

/** Reads libGDX / Spine .atlas text (the 4.x "bounds:" layout and the older "xy:/size:" layout). False when no region was found. */
bool parseAtlas(const std::string& text, std::vector<AtlasPage>& pages, std::vector<AtlasRegion>& regions);

/**
 * Cuts a region out of its page (RGBA, pw x ph) and, for trimmed regions, places it on a transparent canvas of the original size.
 * Returns the image size in outW/outH; empty when the region lies outside the page.
 */
std::vector<uint8_t> extractAtlasRegion(const uint8_t* pageRgba, int pw, int ph, const AtlasRegion& r, int& outW, int& outH);

} // namespace osteon
