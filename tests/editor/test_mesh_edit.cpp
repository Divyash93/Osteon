// Edit Mesh operations on the document: deleting vertices, dropping weights and deform keys.
#include "../Test.h"
#include "Document.h"
#include "Geometry.h"

using namespace osteon;

static Document meshDoc() {
    Document doc;
    doc.newProject();
    SlotData s; s.name = "s"; s.bone = 0; s.attachment = "m";
    doc.data.slots.push_back(s);
    Attachment m;
    m.type = AttachmentType::Mesh;
    m.name = "m";
    m.width = 100; m.height = 100;
    m.vertices = {-50, -50, -50, 50, 50, 50, 50, -50, 0, 0, 20, 10};
    m.uvs = {0, 1, 0, 0, 1, 0, 1, 1, 0.5f, 0.5f, 0.7f, 0.45f};
    m.hullLength = 4;
    m.worldVerticesLength = 12;
    m.triangles = triangulateMesh(m.vertices, 4);
    m.edges = {0, 1, 1, 2, 2, 3, 3, 0};
    doc.data.defaultSkin().set(0, m);
    doc.updatePose();
    return doc;
}

static Attachment* mesh(Document& doc) { return doc.data.skins[0].get(0, "m"); }

TEST(mesh_delete_interior_vertices_is_one_undo_step) {
    Document doc = meshDoc();
    size_t steps = doc.undoStack.size();
    CHECK(doc.deleteMeshVertices(0, "m", {4, 5}));
    CHECK(mesh(doc)->vertexCount() == 4);
    CHECK(mesh(doc)->hullLength == 4);
    CHECK(doc.undoStack.size() == steps + 1);
    doc.undo();
    CHECK(mesh(doc)->vertexCount() == 6);
}

TEST(mesh_delete_refuses_to_break_the_hull) {
    Document doc = meshDoc();
    CHECK(doc.deleteMeshVertices(0, "m", {0})); // a hull vertex: the hull shrinks to 3
    CHECK(mesh(doc)->hullLength == 3);
    CHECK(!doc.deleteMeshVertices(0, "m", {0})); // would leave a 2-vertex hull
    CHECK(mesh(doc)->hullLength == 3);
}

TEST(mesh_topology_edits_drop_deform_keys) {
    Document doc = meshDoc();
    doc.data.animations.push_back({});
    Animation& an = doc.data.animations[0];
    an.name = "a";
    Timeline t;
    t.type = TimelineType::Deform; t.target = 0; t.attachment = "m";
    an.timelines.push_back(t);
    CHECK(doc.deleteMeshVertices(0, "m", {4}));
    CHECK(doc.data.animations[0].timelines.empty());
}

TEST(mesh_delete_hull_corner_keeps_every_other_point_in_the_mesh) {
    // A rectangle hull with points placed along a rounded outline just inside it (like a traced shape).
    Document doc;
    doc.newProject();
    SlotData s; s.name = "s"; s.bone = 0; s.attachment = "m";
    doc.data.slots.push_back(s);
    Attachment m;
    m.type = AttachmentType::Mesh; m.name = "m"; m.width = 100; m.height = 100;
    m.vertices = {-50, -50, -50, 50, 50, 50, 50, -50,   // hull corners
                  -30, 45, 0, 49, 30, 45, 46, 30, 48, 0, 46, -30, // the outline along the top and right
                  0, 0, -20, -20};                              // inside
    m.uvs.assign(m.vertices.size(), 0.5f);
    m.hullLength = 4;
    m.worldVerticesLength = (int)m.vertices.size();
    m.triangles = triangulateMesh(m.vertices, 4);
    doc.data.defaultSkin().set(0, m);
    doc.updatePose();
    size_t before = doc.data.skins[0].get(0, "m")->vertices.size() / 2;
    CHECK(doc.deleteMeshVertices(0, "m", {2})); // the top-right corner
    Attachment* a = doc.data.skins[0].get(0, "m");
    CHECK(a->vertices.size() / 2 == before - 1); // only that point went
    std::vector<float> hull(a->vertices.begin(), a->vertices.begin() + a->hullLength * 2);
    for (int k = a->hullLength; k < a->vertexCount(); k++) CHECK(pointInPolygon(hull, a->vertices[k * 2], a->vertices[k * 2 + 1]));
    CHECK(a->hullLength > 4); // the outline points joined the hull
    // Every vertex is used by some triangle.
    std::vector<int> used(a->vertexCount(), 0);
    for (int t : a->triangles) used[t] = 1;
    for (int k = 0; k < a->vertexCount(); k++) CHECK(used[k]);
}

TEST(mesh_unchecked_turns_back_into_a_region_where_the_image_was) {
    Document doc;
    doc.newProject();
    SlotData s; s.name = "s"; s.bone = 0; s.attachment = "r";
    doc.data.slots.push_back(s);
    Attachment r;
    r.name = "r"; r.width = 80; r.height = 40; r.x = 30; r.y = -20; r.rotation = 25;
    doc.data.defaultSkin().set(0, r);
    doc.updatePose();
    doc.convertToMesh(0, "r");
    Attachment* m = doc.data.skins[0].get(0, "r");
    CHECK(m->type == AttachmentType::Mesh);
    doc.convertToRegion(0, "r");
    Attachment* b = doc.data.skins[0].get(0, "r");
    CHECK(b->type == AttachmentType::Region);
    CHECK_NEAR(b->width, 80, 1e-2);
    CHECK_NEAR(b->height, 40, 1e-2);
    CHECK_NEAR(b->x, 30, 1e-2);
    CHECK_NEAR(b->y, -20, 1e-2);
    CHECK_NEAR(b->rotation, 25, 1e-2);
    CHECK(b->vertices.empty());
}
