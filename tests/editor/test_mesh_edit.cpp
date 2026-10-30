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
