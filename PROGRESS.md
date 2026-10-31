# Progress and Verification

Status as of 2026-10-29 (second batch). The columns are tracked separately, and a feature counts as done only with evidence.

- **Impl**: code exists and is wired into the app.
- **Unit**: covered by an automated test (`build.bat test` runs 47 engine tests and 15 editor tests, all passing).
- **App**: exercised by hand in the running `Osteon.exe` against the user's images (`C:\Users\Div\Desktop\images`), with screenshots taken during development.
- **vs Spine**: compared with the running Spine 4.2.x-era behaviour or the public format specification. *partial* means the behaviour follows documented semantics but has not been compared side by side.

| Feature | Impl | Unit | App | vs Spine | Notes |
|---|:-:|:-:|:-:|:-:|---|
| Bone world transforms, 5 inherit modes | ✅ | ✅ | ✅ | partial | Applied-pose decomposition round-trips in every mode |
| Skeleton position, scale, flips | ✅ | ✅ | – | partial | |
| Region attachments (offset, rotation, scale, size, color) | ✅ | ✅ | ✅ | ✅ | User's 18-part character renders exactly as exported from Photoshop |
| Mesh attachments (unweighted and weighted) | ✅ | ✅ | ✅ | partial | |
| Linked meshes (create, image, inherit deform) | ✅ | ✅ | – | partial | Created from mesh Properties into the active skin |
| Bounding box, point, path, clipping | ✅ | ✅ (path) | – | partial | Clipping uses a stencil (convex and concave); not checked in the app |
| Slot color, tint black (dark color), 4 blend modes | ✅ | – | – | partial | Shader follows Spine's premultiplied tint-black formula |
| Skins (default fallback, active skin, skin-required bones) | ✅ | – | – | partial | Skin placeholders are not modelled; attachments live per skin |
| IK 1-bone / 2-bone, softness, stretch, compress, bend | ✅ | ✅ | – | partial | Per Spine's IK page: New > IK Constraint then click a bone (target), empty space (new target bone) or a constrained bone (target at its tip); 1 bone or parent+child only, target cannot be a descendant; Properties shows Parent/Child (X = one-bone)/Target/Positive/Compress/Stretch/Uniform/Softness/Mix; Animate mode keys them. Built and unit tested, not yet tried in the running app |
| Transform constraint (absolute/relative, world/local) | ✅ | ✅ | – | partial | |
| Path constraint (all position/spacing/rotate modes) | ✅ | ✅ | – | partial | Sub-agent work; chain-mode tip correction is approximate |
| Physics constraint | ✅ | ✅ (`test_physics_timelines.cpp`, `test_physics.cpp`) | checked by hand: the panel layout in Setup, switching to Animate | ❌ | Original spring model; feel differs from Spine. Per Spine's Physics Constraints page: key buttons for Inertia, Strength, Damping, Mass, Wind, Gravity and Mix (timelines, JSON read/write), a Global checkbox per property (skeleton-wide row), reset keys, Simulate, Deterministic, Reset, Reset All, Limit and FPS, warnings. Not done: Skin..., the Warm Up export slider; keying, Reset and Simulate are untested in the running app |
| Update cache / constraint order | ✅ | ✅ | – | partial | |
| Timelines: rotate, translate, scale, shear (+X/Y), inherit | ✅ | ✅ | ✅ | ✅ | Relative to setup pose, as in Spine |
| Timelines: attachment, RGBA/RGB/alpha/RGBA2/RGB2 | ✅ | ✅ | – | partial | |
| Timelines: deform, draw order, events, IK, transform, path | ✅ | ✅ | – | ✅ | Imported into Spine 4.3.26 and shown in its dopesheet |
| Sequences (frame-by-frame): playback modes, auto frame detection, keying | ✅ | ✅ | – | partial | hold/once/loop/pingpong and reverse variants |
| Curves: linear, stepped, Bezier (absolute 4.x handles) | ✅ | ✅ | ✅ | ✅ | Graph presets and draggable handles checked in the app |
| AnimationState: tracks, queueing, crossfade mixing | ✅ | ✅ | – | partial | Simplified crossfade without Spine's hold/first-mode rules |
| Spine JSON import (3.8, 4.x, Photoshop export) | ✅ | ✅ | ✅ | ✅ | `charachter.json` loads with 18 slots and correct placement |
| Spine JSON export (4.2 runtimes / 4.3 editor) and `.osteon` projects | ✅ | ✅ | ✅ | ✅ | The 4.3 export imports into Spine 4.3.26 (weighted mesh, IK, transform, curves, deform, events) |
| Spine 4.3 JSON import (`constraints` array) | ✅ | ✅ | – | ✅ | Esoteric's public spineboy-pro 4.3 export loads and every animation evaluates |
| Undo / redo (all edits, snapshot based) | ✅ | ✅ | ✅ | ✅ | |
| Setup / Animate mode (Ctrl+Tab, SETUP label) | ✅ | ✅ | ✅ | ✅ | |
| Tools: rotate, translate, scale, shear (drag + numeric) | ✅ | – | ✅ | partial | Shear by drag is an approximation (Shift = Y axis) |
| Pose tool | ✅ | – | – | ❌ | Turns the dragged bone itself about its joint (the root moves); its image and children follow. Spine's pose tool solves more (chains) |
| Axes Local / Parent / World | ✅ | – | – | partial | Affects the gizmo and world-space translate fields only |
| Create tool (drag to make bone chains) | ✅ | ✅ | ✅ | ✅ | |
| Set parent (P), reparent by drag in tree, world pose kept | ✅ | ✅ | ✅ | ✅ | Fixed a bug where a reparented slot drew relative to the wrong bone |
| Tree: hierarchy, search, visibility, draw order drag, context menus | ✅ | – | ✅ | partial | |
| Properties for every selection type | ✅ | – | ✅ (bone, slot, region) | partial | |
| Auto Key, pending (orange) poses, K / L keying, key buttons | ✅ | ✅ | ✅ | ✅ | |
| Dopesheet, per Spine's Dopesheet page: overview row (click selects the animation), bone/slot header rows (fold with the +/- icon or a right-click, click selects the item, folded rows show every key, unfolded rows only shared frames, white = several key types), property rows (click selects the item and its tool), interpolation lines (straight, curved, dotted, none), Sync, Filter (multi-select, red when on, right-click toggles), Lock with Refresh / Select, Key shown, Adjust, Shift / Offset, box select (Ctrl-drag starts on a key), edge handles scale the timing, Shift = no snapping, Ctrl+Shift drag duplicates, double-click deletes a key, Ctrl+A (row, then all), right-drag pans, view follows the playhead, Toolbar / Rows settings | ✅ | ✅ (`test_dopesheet.cpp`: Adjust) | checked by hand: overview and header rows, interpolation line, toolbar, Filter menu | partial | Not done: drag to reorder rows, skin-bone visibility, "Jump to key / frame" settings, Current-button auto-scroll option. Box scaling, duplicate, Lock, Sync, Key shown and the other mouse interactions are untested in the app |
| Graph editor | ✅ | – | ✅ | partial | Shows values per channel. Spine also has a normalised curve view |
| Playback: play/back, step frames/keys, loop, speed, loop range [ ] | ✅ | – | ✅ | ✅ | |
| Ghosting (onion skin) | ✅ | – | – | partial | |
| Preview view (AnimationState with mixing) | ✅ | – | – | partial | |
| Mesh tools, per Spine's "Meshes" page: Modify / Create / Delete / New tools, Trace (Detail, Concavity, Refinement, Alpha threshold, Padding sliders that re-trace live, Refresh), Generate (a quad becomes a grid, click again for more), Reset, Triangles / Dim / Isolate, cyan hull edges, right-click cycles tools, Space/Esc leaves Edit Mesh, double-click deletes a vertex, drag a box in Delete | ✅ | ✅ (`test_mesh_tools.cpp`, `test_mesh_edit.cpp`) | checked by hand: Trace + live sliders, Generate twice, New hull | partial | Trace, Generate, Reset and New drop weights and deform keys as in Spine. Not done: manual (orange) edges and edge-loop select, the Deformed checkbox (editing UVs on the full image), Freeze, the image-resize prompt, Shift snapping. Delete box, double-click delete and right-click cycling are untested in the app |
| Deform keys (drag vertices in Animate mode) | ✅ | – | – | partial | |
| Weights, per Spine's "Weights" page: Bind (click bones), Unbind, Auto (along mesh edges; limited to selected vertices/bones), Smooth (neighbours), Prune (max bones + threshold, live count), Swap, Lock, Update bindings, Remove bone, copy/paste, Direct and Brush modes (Add/Remove/Replace, Strength, Size, Feather), Pies/Overlay/Selected, Ctrl+A | ✅ | ✅ (`test_weights.cpp`) | checked by hand: Bind, colours, pies, overlay, brush, vertex selection | partial | Not done: Weld, dragging bones to set triangle order. Auto's "mesh-topology algorithm" is Osteon's own (graph distance), not Spine's |
| Export: JSON, PNG sequence, animated GIF | ✅ | – | ✅ (JSON, GIF) | partial | GIF output: 521×1269, 16 frames, looping |
| Texture packer (MaxRects, rotation, pow2, `.atlas`) | ✅ | ✅ | – | partial | |
| Metrics, Outline, Problems views | ✅ | – | – | partial | |
| Separate Setup / Animate dock layouts | ✅ | – | ✅ | ✅ | Each layout is saved on exit (`layout_setup.ini`, `layout_animate.ini`) |
| Single instance (second launch opens its file in the running editor) | ✅ | – | ✅ | ✅ | |
| Native install: portable zip and per-user installer (IExpress) | ✅ | – | – | n/a | `OsteonSetup.exe` is built but has not been installed on this PC |

## Unfinished or missing (not claimed as done)
- Binary `.skel` export, PSD import, video export (AVI/MOV/WEBM), APNG/WEBP export, audio and audio events playback.
- Opening Spine's proprietary `.spine` project files. The format is undocumented and compressed, so `Mr dom.spine` cannot be opened. Export JSON from Spine instead.
- Skin placeholders and skin constraints UI.
- Transform constraints in 4.3 support only same-channel property mappings (rotate to rotate, and so on). Cross-channel mappings and value ranges are not modelled.
- Sequence playback has not been checked in the app, because the user's images have no numbered frame sets.
- Viewport handles for IK targets, path vertex editing and constraint-specific gizmos. Graph view lacks Spine's normalised curve mode, and the dopesheet lacks Shift/Offset/Adjust tools.
- Hotkey customisation (the list is fixed; see Help > Hotkeys).
- Side-by-side behavioural comparison against Spine for physics, path chain modes and mixing edge cases.
- Not yet loaded in a Spine *runtime* (game-engine integration). Editor import is verified.

## Bugs found and fixed during verification
1. A slot reparented to a different bone rendered relative to the old bone, because the runtime didn't rebuild. `Skeleton::needsRebuild` now compares slot bones (regression test `doc_reparent_slot_keeps_image_in_place`).
2. Clicking a key in the Graph changed its value. A value now changes only on a real drag, by relative motion.
3. Zoom-to-fit ran before the docked viewport had its final size.
4. Pen taps and synthetic clicks sent in the same frame as a pointer jump were missed by widgets. Such presses are now replayed one frame later.
5. Selecting bones for weight binding hid the mesh from the Mesh Tools and Weights views.
6. A test held a reference to a key across a vector insert (test bug, not an engine bug).
7. Spine rejected Osteon's weighted meshes because influence order varied between vertices. The writer now sorts influences by bone index and permutes deform keys to match (test `json_weighted_influences_written_sorted_with_deform`).
8. A second launch opened a second editor window. Osteon is now single-instance.

## Process and token usage
- REA 6.8.0 with the Ghidra 12.1.4 provider is configured. It was **not** used on the Spine Editor binaries, because the license forbids reverse engineering. Spine 4.3.26 Trial was observed through screenshots of the running UI (main menu, Views menu, toolbar, tree, properties, dopesheet, graph, export dialog), together with the user's Spine hotkey and layout files.
- Sub-agents: 2, both on the session's model. Path and physics constraints used 104,728 tokens; geometry, weights and atlas used 104,969 tokens. They worked in separate files, so there were no conflicting edits.
- Main session context at the time of writing: about 546k tokens (reported by the app). Plan usage: 68% of the 5-hour window and 9% of the weekly limit.

## PSD import (File > Import PSD...)

Implemented and unit tested (engine: 4 tests in `tests/test_psd.cpp` with an in-test PSD writer; editor: 3 tests in `tests/editor/test_psd_import.cpp`). Not yet tried in the running app or with a real Photoshop file.

- Reads 8-bit RGB PSD (and PSB lengths) with raw and RLE channels; layer names from `luni` (else Pascal name); groups from `lsct` dividers; layer opacity and the hidden flag; user masks are skipped.
- Each visible pixel layer becomes `<name>.png` (in `<images folder>/<psd name>/`, or `<psd folder>/<psd name>_images/` when no images folder is set), plus a slot and region attachment on the root bone, centred on the PSD canvas (y up). Layers import bottom to top so draw order matches Photoshop. Layer opacity goes to the attachment alpha. One undo step.
- Hidden layers are skipped (counted in the status line). Group names are read but groups are not recreated as bones or slots.
- Unsupported (clear error): 16/32-bit, CMYK/Grayscale/other colour modes, ZIP-compressed channels. Not applied: blend modes, layer masks, vector/text/smart-object effects (only the rasterised pixels), clipping masks, fill opacity.

## UI refresh
Flat theme (rounded corners, no outlines, softer greys, slim scrollbars), ghost-style icon buttons, grouped toolbar plates in the dopesheet, and a tidier playback row: jump/step buttons around one round Play button, repeat and speed, an Auto Key record toggle, and Frame / Loop fields with the total frame count. Animations panel buttons carry icons. Checked by eye in the running app (Animate mode); no feature was removed or moved to another menu.

## Sliders
All sliders use one modern widget (`sliderFloat` / `sliderInt` in Icons.cpp): a thin track with a round knob and a box to type a value. The Physics constraint panel uses it too (`sliderFloatEdit`, one undo step per edit). Checked by eye in Mesh Tools, the bone Properties and the Physics panel; typing into the boxes is untested in the app.

## Path editing
A selected path can be drawn and edited with the Mesh Tools Create / Modify / Delete tools: click to add a point (drag to pull its Bezier handles), click the curve to insert a point, click the first point to close the path, drag points and handles (Alt moves one handle alone), Delete removes a point or resets a handle. Setup-pose lengths are recomputed on every edit (`computePathLengths`, unit tested). Checked by hand: adding points and a pulled handle; closing, inserting, Modify and Delete are untested in the app.

## Look
New app icon (assets/osteon_icon.png, src/editor/app.ico). Dark theme follows Photoshop's neutral greys with Osteon's dark blue accent; Settings > Theme switches to a Light theme (saved in ui_theme.txt). The viewport toolbar and zoom controls keep the dark look in both. Checked by eye in Setup and Animate modes; some dialogs and rarely used panels have not been reviewed in Light.

## Auto-save and crash recovery
Unsaved work is copied to %APPDATA%\Osteon\recovery every 30 s (Settings > Auto-save). A normal exit deletes the copy; if Osteon dies, the next start asks to restore it. On an unhandled exception Osteon writes crash.log and crash.dmp, saves a recovery copy, and restarts itself (not when the restart itself dies within 15 s). `Osteon.exe --crash-test` crashes on purpose after 25 s to try this. Checked by hand: crash, restart, restore prompt and restore. Found and fixed with it: a crash in Trace refinement (negative outline index; read from the user's own crash dump with tools/dumpinfo.cpp and the build's .pdb).

## Final-mission pass 1 (audit + hardening)
Audit documents: `SPINE_CHECKLIST.md` (first pass; built from the guide's contents page plus code greps, not every subpage), plus the bug audit below.
Fixed (each has a regression test unless noted):
- Loader: invalid weighted vertex data, mesh triangles/hull/edges out of range, negative deform offsets, draw-order keys with holes, physics mass/fps extremes, path with fewer than 3 vertices (viewport), negative skin index, animation index out of range in the preview state.
- Deleting a bone, slot or constraint now re-points the other constraints' timelines by name (before, deleting `ik[0]` moved its keys onto the next constraint).
- Set Parent with bones and slots selected: slots are reparented first (bone indices change when the bone list re-sorts).
- Reparenting bones/slots in Animate mode no longer bakes the animated pose into the setup pose (uses a fresh setup-pose skeleton).
- `checkpoint()` commits an open edit instead of dropping its pre-edit state.
- Image file lookups are cached for 1 s (was up to 4 disk checks per attachment per frame).
- New: Bone / Img compensation toggles in the Setup toolbar (children / images keep their world placement while a bone is edited). Tested at document level (`doc_compensation_keeps_children_and_images_in_place`); not yet checked by eye in the running app.
- New stress test: 300 bones, 150 slots, 200 keyed edits, 100 undo + redo.
Still open from the audit: stale open edits swallowing Ctrl+Z (some sliders), reparentSlot ignoring scale/mirroring, per-frame allocations (ghost/setup skeletons, `updateCacheOrder`), quadratic tree work, physics scrub cost. Open features: Slider constraint, skin placeholders/skin bones, Split bone, 4.3 transform mappings, .skel export, graph normalised view.

### Pass 2
- Undo: a gesture that changed nothing (slider click) is dropped instead of becoming an empty undo step (`doc_unchanged_open_edit_does_not_swallow_undo`).
- Reparenting a slot keeps a region's world size under a scaled bone.
- Measured `updatePose`: 0.057 ms on 500 bones, so the per-frame `updateCacheOrder` is not a bottleneck and was left alone (test `doc_update_pose_cost_on_large_skeleton` guards it).
- New: Split bone (Bone panel button): halves the bone, the new child carries the second half and the old children (`doc_split_bone_keeps_the_pose_and_moves_children`). Not yet checked by eye in the app.

### Pass 3: Slider constraint (Spine 4.3)
- Engine: `SliderConstraintData` (animation by name, loop, additive, frame, mix, optional source bone with local/world, property, range -> frames). Evaluated in the constraint order (`CacheType::Slider`, `applySlider`): poses the animation at the frame (or the bone-mapped frame), blends by mix (Replace, or Add when additive), then refreshes the world transform of the bones the animation drives. Timelines `SliderTime` / `SliderMix` can key frame and mix in other animations.
- Data/IO: own JSON keys in the `slider` array (4.2-style) or the 4.3 `constraints` array; animation `slider` timelines. Deleting a bone or animation, renaming an animation, delete/undo all keep slider references valid.
- Editor: New... > Slider (bone menu) and a Slider button on the Animation panel; Tree > Constraints lists sliders; Properties panel (animation, loop, additive, frame, bone, local, property, range, frames, mix, order, key buttons); dopesheet rows and filters.
- Tests: `slider_*` (engine, 4), `doc_slider_create_rename_delete_and_bone_removal`. Checked in the running app: created from the New menu, Frame slider poses the character live.
- **Unverified against Spine:** the exact 4.3 JSON field names for sliders are Osteon's own guess (Spine's guide page does not document them), so Osteon-written 4.3 slider data may not import into Spine. The time/bone mapping follows the guide's description.

### Pass 4: Skin placeholders
- A placeholder is a name on a slot (`SlotData::placeholders`, editor-only: saved in project files, left out of exports) that skins fill with their own attachment of that name. New... > Skin Placeholder (bone menu, makes a slot with one) and "New Skin Placeholder" on a slot's menu. The Tree lists unfilled placeholders under the slot; the Properties panel fills one with an image into the active (non-default) skin. Delete/rename work on empty placeholders.
- Tests: `doc_skin_placeholders_are_filled_per_skin_and_persist`, `doc_renaming_an_empty_placeholder_follows_into_the_slot`. Created from the New menu in the running app and shown in the tree; filling with an image was not tried by eye.
- Not done: Skin bones / skin constraints (the Skin... button on bones and constraints is still disabled).
