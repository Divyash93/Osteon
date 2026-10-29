# Implementation Plan

Build from the bottom up. A phase is done only when its tests pass, not when its UI exists.

1. **Engine core**: math, data, skeleton world transforms (5 inherit modes), attachments (region and mesh vertex computation, weighted), JSON read/write. *Tests:* transform chains, inherit modes, round-trip of the user's `charachter.json`.
2. **Animation**: curves (linear, stepped, Bezier), bone, slot, deform, draw order and event timelines, Animation.apply with mix/blend, AnimationState with crossfade. *Tests:* interpolation values, curve sampling, attachment switching, events firing across loops.
3. **Constraints**: IK (1 and 2 bone, bend direction, mix, softness, stretch), transform constraints. *Tests:* tip reaches target, mix 0 means no effect.
4. **Geometry**: triangulation, auto weights, atlas packing. *Tests:* triangle count is n-2, weights sum to 1, packing has no overlap.
5. **Editor shell**: Electron window, menu, docking layout, viewport camera, WebGL renderer, Setup/Animate mode switch.
6. **Setup tools**: select, translate, rotate, scale, shear (local/parent/world axes), create bone, set parent, import images to slots, tree, properties, draw order, skins, undo/redo.
7. **Animate tools**: animations list, dopesheet, graph editor (Bezier handles), auto-key, key buttons, playback, loop range, ghosting.
8. **Mesh tools**: convert region to mesh, add/move/delete vertices, auto-triangulate, bind bones, weights tool and auto weights, deform keys.
9. **I/O**: save/open project, import Spine JSON, export Spine JSON, texture atlas, PNG sequence, GIF.
10. **Verification**: run every checklist item in the app against the user's images, compare with Spine behaviour, and update `PROGRESS.md`.

Sub-agents are used only for modules that do not depend on each other and have frozen interfaces (geometry/weights/atlas, GIF encoder, constraint tests). The core engine and the editor wiring stay in the main session to avoid conflicting edits.
