# Osteon Architecture

Osteon is an Electron desktop app written in plain ES modules (no bundler, no framework). It is easy to read and has no build step.

```
electron/        main.js (window, native menu, file dialogs, fs IPC), preload.js (safe `osteon` bridge)
src/engine/      pure, DOM-free animation runtime — unit tested in Node
  math.js          vectors, 2x3 affine, angle helpers, Bezier curve sampling
  data.js          SkeletonData, BoneData, SlotData, Skin, EventData, constraint data
  attachments.js   Region, Mesh, BoundingBox, Point, Clipping, Path; vertex computation (weighted/unweighted)
  skeleton.js      runtime Bone / Slot / Skeleton; world transforms (all inherit modes), setup pose, update cache
  constraints.js   IK (1 and 2 bone, softness, stretch, compress), Transform (absolute/relative, local/world)
  animation.js     Timelines + curves + Animation.apply(skeleton, time, mix, blend)
  state.js         AnimationState: tracks, looping, crossfade mixing, events
  json.js          read/write Spine-format JSON (4.x shape) ↔ SkeletonData
  triangulate.js   ear-clipping triangulation, convex/concave hulls, interior points
  weights.js       automatic bone weights (inverse distance to bone segments), normalisation, pruning
  atlas.js         MaxRects-style bin packer + libGDX/Spine `.atlas` text writer
src/editor/      UI layer, depends on engine
  index.html, style.css
  app.js           boots everything, owns the Document and wires panels
  document.js      project model: SkeletonData + images + editor state (mode, animation, time, selection)
  history.js       snapshot-based undo/redo (structural JSON diffs of the document)
  viewport.js      camera, picking, gizmos, tools (pose/create/rotate/translate/scale/shear/weights), overlay
  render.js        WebGL textured-triangle renderer (regions and meshes, tint, blend modes)
  tree.js          hierarchy panel and search
  properties.js    selection-dependent property editor
  timeline.js      dopesheet, graph editor, transport, auto-key, key operations
  dialogs.js       export, texture packer, settings, mesh tools
  io.js            open/save project (.osteon = JSON), import images folder, import/export Spine JSON, PNG seq, atlas
tests/           node:test suites for the engine and the document/history layer
```

## Key decisions
- **Data vs pose.** `SkeletonData` is the setup pose and the document's source of truth. A runtime `Skeleton` is rebuilt or synced from it. In **Setup** mode, tools edit the data. In **Animate** mode, tools edit the runtime pose and *key* the change into the current animation (auto-key) as a value relative to the setup pose, the same way Spine stores it.
- **Coordinates.** Y-up world and degrees, the same as Spine JSON. The renderer flips Y at the camera.
- **Undo.** Each command snapshots the serialised document before and after (fast enough for skeleton-sized data). This makes undo correct for every operation by construction.
- **Interop.** The project file is Spine-compatible JSON plus an `osteon` editor block. Projects can be opened by anything that reads Spine JSON, and Spine JSON exports (including the user's `charachter.json`) import directly.
- **Rendering.** WebGL draws attachments. A 2D canvas overlay draws bones, gizmos, mesh wireframes, and IK and selection markers.
