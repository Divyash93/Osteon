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

/** Grid points (spacing apart) strictly inside the hull and at least spacing/2 from its edges. */
std::vector<float> generateInteriorPoints(const std::vector<float>& hull, float spacing);

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
/** Removes influences below threshold (keeping each vertex's strongest), then normalises. */
void pruneWeights(Attachment& mesh, float threshold);
/**
 * Sets one bone's weight on a vertex, scaling its other influences to fill the remainder;
 * adds the bone if missing, drops influences that reach 0. World position is preserved.
 */
void setVertexWeight(Attachment& mesh, const Skeleton& sk, int vertex, int bone, float weight);

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

} // namespace osteon
