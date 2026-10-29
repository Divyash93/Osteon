# Osteon Architecture

Osteon is a native Windows application: C++20 built with MSVC/CMake, Win32 windowing, Direct3D 11 rendering and Dear ImGui (docking branch) for the UI. The animation engine is a separate static library with no UI or OS dependencies, so it is unit-tested on its own.

> History: the project started as an Electron/JavaScript prototype. On request it moved to a native desktop architecture. The three JS prototype files are kept in `prototype/js-engine/` and were ported to C++.

```
src/engine/  (static library osteon_engine, pure C++)
  MathUtil      angles, Vec2, Color, Bezier curve evaluation, geometry helpers
  Model         setup-pose data: BoneData, SlotData, Skin, Attachment (all types), constraint data,
                Timeline/Key/Animation, SkeletonData with index-safe structural edits
  Skeleton      runtime pose: Bone world transforms (5 inherit modes), applied-pose decomposition,
                update cache ordering, attachment world vertices (weighted + deform)
  Constraints   IK (1/2 bone, softness, stretch, compress), Transform (abs/rel, world/local)
  PathConstraint, PhysicsConstraint   path following and spring/damper physics
  Animation     timeline evaluation (linear/stepped/Bezier), MixBlend, events, AnimationState crossfades
  SkeletonJson  read Spine 3.8/4.x JSON (incl. Photoshop exports), write Spine 4.2 JSON
  Geometry      triangulation, hull tracing, interior points, auto weights, MaxRects packing, .atlas writer
src/editor/  (Osteon.exe)
  main.cpp      Win32 window, D3D11 device/swapchain, ImGui setup, theme, file drop, input fixes
  Gfx           D3D11 renderer: premultiplied textures (WIC), 4 blend modes, tint black, stencil clipping,
                offscreen targets; WIC PNG/GIF encoding
  Document      editor model: data + runtime pose, selection, Setup/Animate routing, auto-key and
                pending poses, snapshot undo/redo, structural commands, project I/O, recent files
  App           frame loop, dock layout, menus, hotkeys, commands
  Viewport      camera, rendering, ghosting, overlays/gizmos, picking, tools, mesh and weight editing
  Panels        Tree, Properties, Animations, Skins, Events, Mesh Tools, Weights, Metrics, Outline, Problems
  Timeline      playback bar, dopesheet, graph editor, preview
  Dialogs       Export, Texture Packer, Settings, Hotkeys, About
tests/          osteon_tests (engine), osteon_editor_tests (document model)
```

## Key decisions
- **Value-semantics data.** References use indices, never pointers, and structural edits remap every reference (`removeBone`, `moveSlot`, `sortBones`). This makes a `SkeletonData` cheap to copy, and undo/redo is a full snapshot that is correct for every command by construction.
- **Data vs pose.** Setup mode edits `SkeletonData`. Animate mode edits the runtime pose: with Auto Key, changes are written straight into timelines as values relative to the setup pose (Spine's convention); without it they are held as *pending* until keyed (K) or the frame changes.
- **Interop.** The project file is Spine 4.2 JSON plus an `osteon` editor block, so any Spine JSON runtime loads Osteon projects. Spine JSON 3.8 and 4.x import directly, including 3.x curve and constraint field conversions.
- **Rendering.** Attachments render with D3D11 into an offscreen target that is shown inside the docked Viewport. Bones, gizmos and meshes are drawn as an ImGui overlay. Exports reuse the same renderer with a transparent target and readback.
- **Distribution.** The static CRT and system-provided `d3dcompiler_47`/WIC make `Osteon.exe` a single file.
