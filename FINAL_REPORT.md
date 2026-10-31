# Osteon: development report (final-mission passes 1 to 10)

Status: **not 100% complete, and not claimed to be.** Osteon is a working editor with a large part of Spine's
functionality; the items under "Remaining limitations" are real gaps. Details per pass are in `PROGRESS.md`;
the feature table is `SPINE_CHECKLIST.md` (a first-pass table built from the guide's contents page, the existing
docs and code searches, not an item-by-item audit of every guide subpage).

## Build and tests
- Clean build with MSVC 2022 (Release): OK.
- Automated tests: **91 engine + 69 editor, all passing.** Also run under AddressSanitizer with no findings
  (including 8,000 and 20,000 fuzz mutations per JSON layout in one-off runs).
- Packaging: `dist/OsteonSetup.exe`, `dist/Osteon-0.1.0-win64.zip`, symbols `dist/Osteon-0.1.0.pdb`. The installer
  was built but not run on this PC.

## Before this mission (already present)
Setup/Animate modes, bones and slots, region/mesh/linked mesh/bounding box/clipping/path/point attachments, weights,
mesh tools, IK/transform/path/physics constraints, skins, events, dopesheet and graph, JSON read (3.8, 4.x) and write
(4.2, 4.3), PNG/GIF export, texture packer, PSD import, auto-save and crash recovery.

## Added in this mission
| Feature | Verified how |
|---|---|
| Bone and image compensation (Setup toolbar) | unit test; toolbar seen in the app |
| Split bone | unit test; used in the app on the sample character |
| Slider constraint (4.3): engine, timelines, JSON, UI | engine + editor tests; created and driven in the app |
| Skin placeholders | tests; created from the New menu in the app |
| Skin bones and skin constraints (all constraint types honour "skin required"; skin constraints now saved) | tests; Skin... popup seen in the app |
| JSON export Scale and Animation clean-up | unit tests; dialog not checked by eye |
| Texture Unpacker (atlas reader, region extraction) | tests (pack then unpack gives identical pixels); dialog not checked by eye; no Spine-made atlas available |
| Event audio playback | unit test of event timing; player smoke-tested alone; not exercised inside the editor |
| Hotkey remapping | used in the app (V remapped to M) |

## Bugs found and fixed
- Loader: invalid weighted vertex data, mesh triangles/hull/edges out of range, negative deform offsets, draw-order
  keys with holes, extreme physics mass/fps, impossible vertex counts (multi-GB allocation), uncaught JSON exceptions
  (process crash), path with fewer than 3 points, negative skin index, animation index out of range in the preview.
- Deleting a bone, slot or constraint moved other constraints' animation keys onto the wrong constraint.
- Set Parent with bones and slots selected attached slots to the wrong bone.
- Reparenting in Animate mode baked the animated pose into the setup pose; slot reparent ignored scale.
- A click on a slider could leave an empty undo step that swallowed the next Ctrl+Z.
- Mesh Tools Trace crashed on some outlines (out-of-bounds read; found from a real crash dump).
- Skin constraints were read but dropped on save.

## Intentionally not done
- **Binary `.skel` export:** the only public spec is for an older format and contradicts itself; output built from it
  would not load in real runtimes and cannot be verified here.
- Spine's proprietary `.spine` project format, video/APNG/WEBP export, Spine's runtime licensing.

## Remaining limitations
- 4.3 JSON field names Osteon writes for sliders and skin constraints are its own guesses (Spine documents neither);
  they round-trip in Osteon but may not import into Spine.
- 4.3 transform-constraint cross-channel mappings and ranges are read only as same-channel copies.
- Graph view has no normalised curve view; Weld and manual mesh edges are missing; PSD import handles 8-bit RGB only
  (no groups/masks/blend modes) and was not tried on a wide range of files.
- Per-frame allocations (ghost/preview skeletons), quadratic tree work and physics re-simulation when scrubbing remain.
  Measured: posing a 500-bone skeleton costs about 0.06 ms, so these are not current bottlenecks.
- Several new dialogs (export options, unpacker) and event audio were not exercised end to end in the running app.

## Performance and stability observations
- 300 bones, 150 slots, 200 keyed edits, 100 undo + 100 redo: well under the 5 s test budget, all poses finite.
- Image-file lookups are cached (they were up to four disk checks per attachment per frame).
- Crash handling: report (`crash.log`, `crash.dmp`), recovery copy and restart; symbols are kept per release.

## Files modified (main ones)
Engine: `Model.*`, `Skeleton.*`, `Animation.*`, `SkeletonJson.*`, `Geometry.*`. Editor: `Document.*`, `Panels.cpp`,
`Viewport.cpp`, `Timeline.cpp`, `Dialogs.cpp`, `App.*`, `Audio.*`, `main.cpp`. Tests: `tests/test_*.cpp`,
`tests/editor/test_*.cpp`. Docs: `PROGRESS.md`, `SPINE_CHECKLIST.md`, this file.
