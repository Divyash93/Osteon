# Runtime research notes

Notes from reading **public documentation only** (Unity and Godot manuals and API docs, and the public feature descriptions of Esoteric Software's spine-unity and spine-godot runtimes). No Spine Runtimes source code was read or copied; the Osteon2D runtimes were written from scratch against Osteon2D's own engine (`src/engine`). The Spine Runtimes are under a separate licence (Spine Runtimes License), which is why they are not reused.

## What a skeletal-animation runtime has to do

1. **Load** skeleton data (bones, slots, skins, attachments, constraints, events, animations) and an atlas.
2. **Pose**: apply animation timelines (bone, slot colour, attachment, deform, draw order, events, constraint mixes) to a local pose, then compute world transforms in an order that respects IK and transform constraints.
3. **Mix**: tracks, queueing, crossfades between entries (first track blends from the setup pose, upper tracks replace).
4. **Render**: turn the visible attachments in draw order into textured triangles (region quads, meshes, weighted meshes with deform), with slot colour, blend mode and clipping, batched by texture and blend mode.
5. **Integrate**: events, bone access (followers), skin switching, flipping, update modes.

## Engine integration patterns found in the docs

| | Unity | Godot 4 |
|---|---|---|
| Component | `MonoBehaviour` plus `MeshFilter` and `MeshRenderer`; data in a `ScriptableObject` | `Node2D` subclass; resources loaded from file paths |
| Geometry | `Mesh` with sub meshes (one material per atlas page and blend mode); vertices rewritten each frame | `RenderingServer.canvas_item_add_triangle_array` per batch, or `ArrayMesh` |
| Blending | material blend factors (shader properties) | `CanvasItemMaterial` blend modes (mix, add, sub, mul, premultiplied) |
| Import | `ScriptedImporter` for custom extensions, asset menu items | custom resource importers; non-resource files need export filters |
| Followers | transform that copies a bone after the skeleton updated | node that copies a bone from a signal |
| UI rendering | `Graphic` on a Canvas (single material) | Control or `Node2D` in a `SubViewport` |
| Timeline / state machines | Timeline tracks, Mecanim | `AnimationPlayer` / `AnimationTree` |

## Feature list of the existing Spine runtimes (from their public descriptions) and what Osteon2D implements

| Feature | Osteon2D Unity | Osteon2D Godot |
|---|---|---|
| Skeleton component with animation, skin, loop, time scale | yes | yes |
| Update modes (update, late update, fixed, manual) | yes | yes |
| Bone followers | yes | yes |
| Events | yes | yes |
| Animation completion callback | not yet | `animation_completed` signal |
| Custom skins (mixing skins at run time) | not yet | not yet |
| Blend modes, tint black, clipping | yes | blend modes, clipping (tint black not yet) |
| Premultiplied alpha | builder option | n/a (straight alpha) |
| Mesh-based rendering with batching by page and blend mode | yes | yes |
| Editor preview | yes | yes |
| Graphic (UI) component, Mecanim / Timeline | not yet | not yet |
| Slot followers, bounding box followers, ragdoll | not yet | not yet |
| Path and physics constraints | not yet | not yet |
| Runtime skeleton loading from files | yes (data asset) | yes (paths) |

"Not yet" items are the roadmap, not claims of parity.
