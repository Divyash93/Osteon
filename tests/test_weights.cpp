// Mesh weights, following Spine's "Weights" page: Auto along the mesh, Smooth, Prune, locks, Swap, Update,
// removing bones, copy/paste and the 100% rule when adding or taking away weight.
#include "Test.h"
#include "Geometry.h"
#include "Skeleton.h"
#include <cmath>

using namespace osteon;

static SkeletonData wSkeleton() {
    SkeletonData d;
    BoneData root; root.name = "root";
    BoneData a; a.name = "a"; a.parent = 0; a.x = 10; a.rotation = 30; a.length = 50;
    BoneData b; b.name = "b"; b.parent = 1; b.x = 50; b.rotation = -45; b.length = 40; b.scaleX = 1.5f;
    BoneData c; c.name = "c"; c.parent = 0; c.x = -20; c.y = 40;
    d.bones = {root, a, b, c};
    return d;
}

// A 5 x 4 grid with its triangles, so every vertex has neighbours.
static Attachment wGrid() {
    Attachment m;
    m.type = AttachmentType::Mesh;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 5; x++) { m.vertices.push_back(x * 25.0f - 10); m.vertices.push_back(y * 20.0f - 15); }
    m.worldVerticesLength = (int)m.vertices.size();
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 4; x++) {
            int a = y * 5 + x, b = a + 1, c = a + 5, d = c + 1;
            m.triangles.insert(m.triangles.end(), {a, b, d, a, d, c});
        }
    return m;
}

static float weightOf(const Attachment& m, int vertex, int bone) { return boneWeightAt(m, vertex, bone); }

static void sumsAreOne(const Attachment& m) {
    for (size_t b = 0, v = 0; b < m.bones.size();) {
        int count = m.bones[b++];
        CHECK(count >= 1);
        float sum = 0;
        for (int i = 0; i < count; i++, b++, v += 3) sum += m.vertices[v + 2];
        CHECK_NEAR(sum, 1, 1e-4);
    }
}

static void sameWorld(const std::vector<float>& a, const std::vector<float>& b) {
    CHECK(a.size() == b.size());
    for (size_t i = 0; i < a.size() && i < b.size(); i++) CHECK_NEAR(a[i], b[i], 1e-3);
}

TEST(weights_auto_follows_the_mesh_not_straight_lines) {
    // A "U": two arms joined at the bottom with a narrow gap between them. Bone A runs up the left arm, bone B only
    // part way up the right arm. The top of the right arm is nearer to A across the gap, but A can only reach it
    // by going all the way round the bottom of the U.
    SkeletonData d;
    BoneData root; root.name = "root";
    BoneData a; a.name = "A"; a.parent = 0; a.x = 10; a.y = 10; a.rotation = 90; a.length = 90;
    BoneData b; b.name = "B"; b.parent = 0; b.x = 60; b.y = 10; b.rotation = 90; b.length = 30;
    d.bones = {root, a, b};
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment m;
    m.type = AttachmentType::Mesh;
    // Vertices 0-3: left arm; 4-7: right arm; 8, 9: the bottom of the U.
    const float pts[] = {0, 0, 20, 0, 0, 100, 20, 100, 50, 0, 70, 0, 50, 100, 70, 100, 0, -20, 70, -20};
    m.vertices.assign(pts, pts + 20);
    m.worldVerticesLength = 20;
    m.triangles = {0, 1, 3, 0, 3, 2, 4, 5, 7, 4, 7, 6, 8, 0, 1, 8, 1, 9, 9, 1, 5, 4, 5, 1};
    bindWeights(m, sk, 0, {1, 2});
    sumsAreOne(m);
    // The top-left corner of the right arm (vertex 6) is 40 from A's tip across the gap, 61 from B's: A is nearer
    // as the crow flies. But A can only reach it by going round the bottom of the U, while B reaches it along that arm.
    CHECK(weightOf(m, 6, 2) > weightOf(m, 6, 1));
    // Top of the left arm (vertex 3) is A's.
    CHECK(weightOf(m, 3, 1) > weightOf(m, 3, 2));
    // Without triangles there is no topology to follow, and plain distance gives the straight-line answer.
    Attachment flat;
    flat.type = AttachmentType::Mesh;
    flat.vertices.assign(pts, pts + 20);
    flat.worldVerticesLength = 20;
    bindWeights(flat, sk, 0, {1, 2});
    CHECK(weightOf(flat, 6, 1) > weightOf(flat, 6, 2));
}

TEST(weights_auto_limited_to_selected_vertices_and_bones) {
    SkeletonData d = wSkeleton();
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment m = wGrid();
    bindWeights(m, sk, 0, {1, 2, 3});
    // Paint vertices 7 and 8 by hand, then run Auto on vertex 7 only: vertex 8 must stay as painted.
    setVertexWeight(m, sk, 7, 1, 1.0f);
    setVertexWeight(m, sk, 8, 1, 1.0f);
    auto world = meshWorldVertices(m, sk, 0);
    autoWeights(m, sk, 0, {1, 2, 3}, {7});
    CHECK_NEAR(weightOf(m, 8, 1), 1.0, 1e-4);
    CHECK(weightOf(m, 7, 1) < 0.999f); // vertex 7 was recomputed
    sumsAreOne(m);
    sameWorld(meshWorldVertices(m, sk, 0), world);
    // Only bone 3 selected: the other bones keep their relative proportions on that vertex.
    float before1 = weightOf(m, 12, 1), before2 = weightOf(m, 12, 2);
    autoWeights(m, sk, 0, {1, 2, 3}, {12}, {3});
    sumsAreOne(m);
    if (before1 > 1e-4f && before2 > 1e-4f) CHECK_NEAR(weightOf(m, 12, 1) / weightOf(m, 12, 2), before1 / before2, 1e-3);
}

TEST(weights_smooth_averages_with_neighbours) {
    SkeletonData d = wSkeleton();
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment m = wGrid();
    bindWeights(m, sk, 0, {1, 2});
    for (int v = 0; v < 20; v++) setVertexWeight(m, sk, v, 2, 0.0f); // everything on bone 1
    setVertexWeight(m, sk, 7, 2, 1.0f);                                // one vertex fully on bone 2
    auto world = meshWorldVertices(m, sk, 0);
    smoothWeights(m, sk, {7});
    CHECK(weightOf(m, 7, 2) < 1.0f && weightOf(m, 7, 2) > 0.3f);       // pulled toward its neighbours
    CHECK_NEAR(weightOf(m, 0, 2), 0.0, 1e-6);                          // vertices outside the selection are untouched
    sumsAreOne(m);
    sameWorld(meshWorldVertices(m, sk, 0), world);
    float once = weightOf(m, 7, 2);
    smoothWeights(m, sk, {7});
    CHECK(weightOf(m, 7, 2) < once);                                   // repeating spreads it further
    smoothWeights(m, sk);                                              // no selection: all vertices
    sumsAreOne(m);
}

TEST(weights_prune_threshold_and_max_bones) {
    SkeletonData d = wSkeleton();
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment m = wGrid();
    bindWeights(m, sk, 0, {0, 1, 2, 3});
    auto world = meshWorldVertices(m, sk, 0);
    int toRemove = pruneCount(m, 0.0f, 2);
    CHECK(toRemove > 0);
    Attachment p = m;
    pruneWeights(p, 0.0f, 2);
    for (size_t b = 0; b < p.bones.size(); b += p.bones[b] + 1) CHECK(p.bones[b] <= 2); // at most 2 bones per vertex
    sumsAreOne(p);
    // The count shown in the preview is what was removed.
    int before = 0, after = 0;
    for (size_t b = 0; b < m.bones.size(); b += m.bones[b] + 1) before += m.bones[b];
    for (size_t b = 0; b < p.bones.size(); b += p.bones[b] + 1) after += p.bones[b];
    CHECK(before - after == toRemove);
    sameWorld(meshWorldVertices(p, sk, 0), world);
    // A high threshold still leaves every vertex its strongest bone.
    Attachment q = m;
    pruneWeights(q, 0.99f);
    for (size_t b = 0; b < q.bones.size(); b += q.bones[b] + 1) CHECK(q.bones[b] >= 1);
    CHECK(pruneCount(wGrid(), 0.1f) == 0); // an unweighted mesh has nothing to prune
}

TEST(weights_locked_bones_keep_their_weights) {
    SkeletonData d = wSkeleton();
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment m = wGrid();
    bindWeights(m, sk, 0, {1, 2, 3});
    setVertexWeight(m, sk, 6, 1, 0.5f);
    setVertexWeight(m, sk, 6, 2, 0.3f);
    float locked = weightOf(m, 6, 1);
    // Raising bone 2 with bone 1 locked: bone 1 must not change, the free bone gives way.
    setVertexWeight(m, sk, 6, 2, 0.45f, {1});
    CHECK_NEAR(weightOf(m, 6, 1), locked, 1e-5);
    CHECK_NEAR(weightOf(m, 6, 2), 0.45, 1e-4);
    sumsAreOne(m);
    // Asking for more than the locked bones leave only gets what is left.
    setVertexWeight(m, sk, 6, 2, 1.0f, {1});
    CHECK_NEAR(weightOf(m, 6, 1), locked, 1e-5);
    CHECK_NEAR(weightOf(m, 6, 2), 1.0 - locked, 1e-4);
    // A locked bone cannot be edited at all.
    float w2 = weightOf(m, 6, 2);
    setVertexWeight(m, sk, 6, 2, 0.1f, {2});
    CHECK_NEAR(weightOf(m, 6, 2), w2, 1e-6);
}

TEST(weights_a_bone_at_100_percent_cannot_be_reduced) {
    SkeletonData d = wSkeleton();
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment m = wGrid();
    bindWeights(m, sk, 0, {1, 2});
    setVertexWeight(m, sk, 4, 1, 1.0f);
    setVertexWeight(m, sk, 4, 1, 0.2f); // nothing else can take the weight
    CHECK_NEAR(weightOf(m, 4, 1), 1.0, 1e-5);
    // Adding to one bone lowers the others so the total stays 100%.
    setVertexWeight(m, sk, 4, 2, 0.25f);
    CHECK_NEAR(weightOf(m, 4, 2), 0.25, 1e-5);
    CHECK_NEAR(weightOf(m, 4, 1), 0.75, 1e-5);
    // Taking weight away from a bone gives it to the others.
    setVertexWeight(m, sk, 4, 2, 0.05f);
    CHECK_NEAR(weightOf(m, 4, 1), 0.95, 1e-5);
}

TEST(weights_swap_remove_update_and_copy_paste) {
    SkeletonData d = wSkeleton();
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment m = wGrid();
    bindWeights(m, sk, 0, {1, 2, 3});
    auto world = meshWorldVertices(m, sk, 0);
    std::vector<float> w1, w2;
    for (int v = 0; v < 20; v++) { w1.push_back(weightOf(m, v, 1)); w2.push_back(weightOf(m, v, 2)); }

    swapBoneWeights(m, sk, 1, 2);
    for (int v = 0; v < 20; v++) { CHECK_NEAR(weightOf(m, v, 1), w2[v], 1e-5); CHECK_NEAR(weightOf(m, v, 2), w1[v], 1e-5); }
    sameWorld(meshWorldVertices(m, sk, 0), world);
    swapBoneWeights(m, sk, 1, 2);

    // Copy one vertex's weights onto another, same count.
    WeightSet set = copyWeights(m, {5});
    CHECK(pasteWeights(m, sk, {14}, set));
    for (int bone : {1, 2, 3}) CHECK_NEAR(weightOf(m, 14, bone), weightOf(m, 5, bone), 1e-5);
    CHECK(!pasteWeights(m, sk, {1, 2}, set)); // different counts: refused
    sameWorld(meshWorldVertices(m, sk, 0), world);

    // Update bindings: after a bone moves, the vertices stay put and the new pose becomes the bind pose.
    sk.bones[2].rotation += 25;
    sk.updateWorldTransform();
    auto moved = meshWorldVertices(m, sk, 0);
    updateBindings(m, sk);
    sameWorld(meshWorldVertices(m, sk, 0), moved);
    sk.bones[2].rotation -= 25;
    sk.updateWorldTransform();
    auto back = meshWorldVertices(m, sk, 0);
    bool differs = false;
    for (size_t i = 0; i < back.size(); i++) differs |= std::fabs(back[i] - moved[i]) > 1e-2f;
    CHECK(differs); // the old bind positions were discarded, so undoing the bone move now moves the vertices

    // Remove bones: a vertex left with none follows the slot's bone; no bones left means no weights.
    removeBoneWeights(m, sk, 0, 3);
    CHECK(m.isWeighted());
    for (int v = 0; v < 20; v++) CHECK_NEAR(weightOf(m, v, 3), 0.0, 1e-6);
    sumsAreOne(m);
    removeBoneWeights(m, sk, 0, 1);
    removeBoneWeights(m, sk, 0, 2);
    CHECK(!m.isWeighted());
    CHECK(boundBones(m).empty());
}

TEST(weights_adding_a_bone_to_a_binding) {
    SkeletonData d = wSkeleton();
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment m = wGrid();
    bindWeights(m, sk, 0, {1, 2});
    auto world = meshWorldVertices(m, sk, 0);
    CHECK(boundBones(m) == std::vector<int>({1, 2}));
    addBonesToBinding(m, sk, 0, {3});
    CHECK(boundBones(m) == std::vector<int>({1, 2, 3}));
    sumsAreOne(m);
    sameWorld(meshWorldVertices(m, sk, 0), world);
    float onBone3 = 0;
    for (int v = 0; v < 20; v++) onBone3 += weightOf(m, v, 3);
    CHECK(onBone3 > 0); // the new bone took some weight
    // On an unweighted mesh it is the first binding.
    Attachment fresh = wGrid();
    addBonesToBinding(fresh, sk, 0, {1, 2});
    CHECK(fresh.isWeighted());
}

TEST(weights_batched_brush_edits_match_single_edits) {
    SkeletonData d = wSkeleton();
    Skeleton sk(d);
    sk.updateWorldTransform();
    Attachment a = wGrid(), b = wGrid();
    bindWeights(a, sk, 0, {1, 2, 3});
    bindWeights(b, sk, 0, {1, 2, 3});
    setVertexWeight(a, sk, 3, 1, 0.8f);
    setVertexWeight(a, sk, 9, 2, 0.4f);
    setVertexWeights(b, sk, {{3, 1, 0.8f}, {9, 2, 0.4f}});
    CHECK(a.bones == b.bones);
    for (size_t i = 0; i < a.vertices.size(); i++) CHECK_NEAR(a.vertices[i], b.vertices[i], 1e-5);
}
