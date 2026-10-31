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

### Pass 5: Skin bones and skin constraints
- A bone or constraint marked "Skin required" is now active only while a skin that lists it is active (bones already worked in the engine; constraints of every type, sliders included, now follow the same rule). The Skin... button (bone, IK, transform, path, physics, slider panels) ticks the skins that include the item; the Skin panel lists each skin's bones and constraints with remove buttons and "Add selected bones".
- Skin constraints are now written to JSON (they were read but dropped on save): per-type arrays for 4.2 layout, one `constraints` name list for 4.3 (the 4.3 key is Osteon's guess). Renaming or deleting a constraint updates the skins.
- Tests: `skin_required_bone_is_active_only_in_skins_that_list_it`, `skin_bones_follow_bone_deletion_and_reordering`, `skin_required_constraint_follows_skin_rename_delete_and_round_trips`. In the running app the Skin... button and popup show; ticking a skin was not tried (the sample has no skins).

### Pass 6: JSON export Scale and Animation clean-up
- File > Export > JSON: "Scale" multiplies every distance in the exported data (bones, attachment positions/sizes/vertices, path lengths, translate and deform keys incl. Bezier handles, IK softness, constraint offsets, skeleton bounds; angles, colors, UVs untouched) and "Animation clean-up" drops keys that repeat the value of both neighbours (never a Bezier key). The project itself is not changed. Tests: `export_scale_*`, `animation_clean_up_*`.
- Not covered: Fixed-mode path constraint timelines and physics values are not scaled; clean-up does not remove whole redundant timelines. The dialog controls were not checked by eye.

### Pass 7: Texture Unpacker
- File > Texture Unpacker...: reads a libGDX/Spine `.atlas` (4.x `bounds:` layout and the older `xy:/size:` layout), loads its page PNGs and writes one PNG per region (rotated regions turned back, trimmed regions put back on their original canvas, names with `/` become sub-folders, sequence regions get `_index`).
- Engine: `parseAtlas`, `extractAtlasRegion`. Tests: parse of both layouts, pack -> unpack returns identical pixels (with rotation), trimmed placement.
- Not checked: the dialog itself in the running app, and real atlases from Spine's packer (none on this machine); rotation direction follows Osteon's own packer (90 degrees clockwise).

### Pass 8: Event audio playback
- Events with an Audio path now play during forward playback in Animate mode as the playhead passes their keys (loop wraps included), at the event's volume; Settings > "Play event audio during playback" turns it off. Relative paths resolve against the skeleton's audio folder, else the project folder. Windows MCI is used (wav and mp3 and whatever the system decodes); at most 8 sounds are kept open.
- Engine: `eventsBetween` (tested incl. loop wrap and paused/backward cases). Player: `src/editor/Audio.cpp`, smoke-tested with a generated wav in a standalone program (plays, missing file reports failure). Not checked inside the running editor with a real animation; balance/pan is ignored.

### Pass 9: Fuzzing and AddressSanitizer
- New `tests/test_fuzz_loader.cpp`: mutates a rich skeleton's JSON (4.2 and 4.3 layouts: values replaced by -1/0/huge/strings/null, keys and array items removed or duplicated) and requires that it either loads or is rejected, and that anything loaded poses, animates, runs constraints and computes vertices without crashing. 400 mutations per layout in the normal run.
- Found and fixed with it: an uncaught JSON exception for constraints of an unexpected shape (crashed the process; the loader now has an exception barrier), and an impossible vertex count that tried to allocate gigabytes (now emptied with a warning).
- How to run under AddressSanitizer: build with `-DCMAKE_CXX_FLAGS="/fsanitize=address /Zi"` in a separate build folder (`build_asan`, git-ignored) and run both test exes. All 91 engine + 69 editor tests are clean, and one-off runs of 8,000 and 20,000 mutations per layout (different seeds) found nothing further.

### Pass 10: Hotkey remapping
- Help > Hotkeys now has "Remap a key": click "Shortcut key", press the key whose shortcut you want to move, click "New key", press the key that should trigger it, Apply. Remaps are listed with Reset buttons and saved in `%APPDATA%\Osteon\hotkeys_remap.txt`. Applies to the editor's global shortcuts (tools, play, keys, delete etc.); the original key keeps its own shortcut unless it is remapped too. Shortcuts handled inside individual panels (and modifier combos' modifier part) are not remappable.
- Checked in the running app: remapped V (Translate) to M, and M then selected Translate. No unit test (needs a live ImGui context).
- Decided not to do: binary `.skel` export. The only spec available (Esoteric's public page) is for an older format and is internally inconsistent; a writer built from it would not load in real runtimes, and there is no way here to verify against one.

### Pass 11: Bones page review (root bone and descendants)
Compared with Spine's Bones page and fixed what differed:
- **Hiding a bone hid its whole subtree and all their images** (the eye on `root` made the entire character vanish). Spine hides only that bone's drawing; children and images still draw. Now the eye affects that bone only, and a **right click on the eye** shows/hides the bone with all its child bones and the slots on them (one undo step). Checked in the running app on the sample: root hidden leaves everything else; right click hides all.
- **New > Bone** now starts at the parent's tip (it started at the origin, so a bone under a long bone overlapped it).
- **Split** now matches the page: a popup with the number of pieces, **Nested** (each a child of the previous, or all sharing the original's parent) and **Fibonacci** lengths; the last piece takes the old children and everything keeps its world pose.
- Bone **Select** (viewport picking) and **Name** (always draw the name) checkboxes added to the bone panel; saved with project files.
- The **root can be renamed** (the menu item was disabled); delete, duplicate and re-parent stay refused for it.
- New `tests/editor/test_root_bone.cpp` (13 tests): new bones under a rotated/scaled/mirrored root, moving the root carries descendants and images, re-parenting under a transformed root, root restrictions, deleting subtrees, Animate-mode edits, subtree visibility, Split variants.
- Not changed: "bones affected by IK cannot disable inheritance", the bone-length tool, multi-bone color editing.

### Pass 12: Hierarchy guide (dashed red line to the ancestors)
- With a bone selected, a dashed red line runs from its origin to its parent's origin and on up through every ancestor to the root, with an arrow in the middle of the first link pointing towards the parent (as in the Spine screenshot the user supplied). Works for several selected bones; each link is drawn once. Drawn with the bones (Bones view toggle hides it). Checked in the running app on the sample (hand bone: line up the arm, spine and to the root). No unit test (drawing only).

### Pass 13: Parent guide refined (single link) and double-click to clear
- The red dashed guide now shows **only the selected bone's own parent link** (child origin to parent origin, arrow pointing at the parent), not the chain up to the root. A root bone or no selection shows nothing; selecting another bone moves the guide to it. It is computed from `Document::selectedParentLinks()` (the real parent indices and world transforms), is screen-space dashed so it reads at any zoom, and is drawing only: it never changes bones, animation, skinning or exported data.
- **Double-click on empty viewport** clears the selection (and so the guide) without touching bones, parenting or transforms; clicking a bone brings its guide back.
- Tests: `parent_guide_links_follow_the_selection_and_the_hierarchy`, `parent_guide_selection_changes_never_alter_the_hierarchy_through_undo_and_save` (root none, one child, several generations, switching selection, re-parenting, clear, undo/redo, save/load). Checked in the running app on a fresh project (root, three chained bones): guide on bone3 only along bone2, on bone2 only along bone1, double-click cleared selection and guide with all four bones intact.
- Not covered: a child that starts exactly at its parent's origin (e.g. the first bone under a zero-length root) has a zero-length link, so no line is visible; the viewport pixel output itself has no automated test.

### Pass 14: Export dialog redesign, video export (AVI, MOV), JPEG, soft window shadows
- **Export dialog** rebuilt after Spine's: format list on the left (Data: JSON, Binary, HTML; Image: GIF, PNG, APNG, WEBP, AWEBP, JPEG; Video: AVI, MOV, WEBM; **no PSD**), options on the right (Export type, Output file with default and browse buttons, Warm up, Render, Smoothing, Multisample AA, Viewport Crop, Size Scale/Width/Height, Frames Range, Encoding, Background, Compression, Audio), Defaults / Preview (disabled, as in Spine), Save / Load of the settings, Open after export, Export, Cancel. Unavailable formats (Binary, HTML, APNG, WebM, and WebP where Windows has no WebP codec) are greyed with a tooltip saying why.
- **Video export:** AVI (uncompressed 24-bit or MJPEG) and QuickTime MOV (PNG with alpha, or JPEG), written by hand in `VideoWriter.cpp`, no external encoder. Verified: structure tests parse the files; Windows Media Foundation opens and decodes every frame of uncompressed AVI, MJPEG AVI and JPEG MOV; an AVI exported from the sample character through the real dialog (537x1269, 16 frames) decoded fully. PNG-in-MOV opens (size and codec read) but Windows has no PNG video decoder, so it needs QuickTime, ffmpeg, After Effects etc. to play. No audio is muxed.
- Real options: Warm up (physics steps before the first frame), Smoothing (nearest vs bilinear sampling in the renderer), Multisample AA (2x / 4x supersampling, alpha-weighted), Crop, Size by scale / pixel width / pixel height, Frames range, Export type (current pose / animation / all animations), Compression (JPEG quality), Background colour where the format has no alpha. Render shows Images only; Bones, Others, Selection and Titles are greyed (exports render images only).
- **JPEG** image-sequence export added (quality from Compression).
- **Soft glowing shadows** around all floating windows (Export, Texture Packer, Unpacker, Settings, Hotkeys, About): layered fading rings drawn outside the window (`windowShadow`). Docked panels have none.
- Tests: `avi_*`, `video_*`, `jpeg_file_*` (writers' structure, real encoded frames, rejection of bad input, JPEG round trip and quality).

### Pass 15: WebP export and the Spine-style Settings dialog
- **WEBP / AWEBP export** (Export dialog): lossless WebP written by Osteon's own VP8L encoder (`src/engine/WebP.cpp`: subtract-green transform, Huffman-coded ARGB, no external codec); WEBP writes a numbered sequence, AWEBP one animated file (VP8X/ANIM/ANMF). Verified against libwebp through Pillow: six test images (gradient, flat colour, alpha noise, transparent shape, 1x1, 640x480) decode **pixel-identical**, and a 5-frame animation decodes to 5 distinct frames. Windows has no WebP codec here, which is why the encoder is Osteon's own. Files are lossless, so no quality setting. Unit tests check the RIFF/VP8L/VP8X/ANMF structure.
- **Settings dialog** rebuilt after Spine's: page list (Application > Files, General, User interface; Viewport; Behavior > Dopesheet), rows of title + description + control, Credits / Defaults / Save / Cancel. Edits are made on a copy; Save applies them and writes `%APPDATA%\Osteon\settings.txt`, Cancel or closing discards. Real settings only: auto-save and its interval, event audio, layout reset, theme, default timeline FPS for new projects, tree indentation, dopesheet row height, canvas colours, axes, rulers, image names, selectable bones/images/others, ghosting (frames, step, opacity). Spine's Launcher, Font, Language, Interface scale, toolbar-position/labels, Fewer-ticks, Tree-colors and Graph pages are not included because Osteon has nothing behind them.
- Checked in the running app: the dialog opens with F12, shows the pages, Save writes settings.txt.

### Pass 16: Export / Settings dialog clean-up
- The grey strip along the bottom was a horizontal scrollbar: the content was wider than the window (Preview button and the "Bilinear 80%" text ran past the edge). Fixed the widths (items leave room for the text/buttons after them, Defaults/Preview are measured from the options area's right edge) and switched the scrollbars off. The black title bar is replaced by a plain header row ("Export" / "Settings" and a close button) on both dialogs. Checked in the running app on the JSON and MOV pages.

### Pass 17: Shadow rings removed, button margins
- The layered shadow rings around the floating dialogs showed as dark/grey bands above and below the window; removed them (all dialogs are flat again; the `windowShadow` helper stays unused in Icons.cpp). Defaults/Preview and the close button now keep a margin from the panel edge. Checked in the running app.

### Pass 18: Dialog padding and icons
- Export and Settings panels now have real inner padding (nothing touches a panel edge); the label column is measured from the padded content edge, so labels no longer get clipped; Defaults/Preview keep a margin from the panel edge. The close button is the red cross icon; Save, Load, Export, Cancel, Credits and Defaults buttons carry icons (Cancel and close use the red cross). Checked in the running app.

### Pass 19: Start-up splash screen
- On start Osteon shows a splash over the (dimmed) editor: the supplied blank card with the animated logo (the **SVG** was used: it is vector-quality and was rendered to PNG frames with headless Chrome at 1024 px, where the supplied GIF is only 150 px), the name, version, description, author and a clickable GITHUB link. It plays once, fades out by itself after 4 seconds, and closes earlier on a click on the empty area around the card (a click on the card does nothing, a click on GITHUB opens the author's page) or on Esc / Enter / Space. It is shown once per run, skipped on the automatic restart after a crash, delayed the "Restore unsaved work?" question until it is gone, and can be switched off in Settings > General.
- Pipeline: `tools/make_splash_assets.py` renders the SVG frames, places them over the logo position of the reference card, bakes the text into the supplied blank background and writes `assets/splash/splash.pak`, embedded in `Osteon.exe` as a resource (resource 301). Re-run it after changing the version or the text; `assets/splash/preview.png` shows the result. Originals are kept in `assets/branding`.
- Code: `Splash.cpp` (draw, input), `SplashData.cpp` (pack parser, area resize, timing; unit tested), `Gfx::createTexture`, `decodeImageMemory`. Tests: `splash_*` (pack parses and every image decodes, malformed packs rejected, resize keeps transparent edges clean, clock fade/auto-close/dismiss). Checked in the running app: appears, animates, auto-closes at 4 s, closes on a click outside the card, stays on a click on the card.
- Text notes: the version shown is the real one (0.1.0, from `app.rc`), not the 2.0.10 of the mock-up; the author line reads "Divyash Vishwakarma".

### Pass 20: Branded setup wizard
- `dist\OsteonSetup.exe` is now a real setup wizard (`installer/setup.cpp`, native Win32, no installer framework) instead of the plain IExpress extractor, laid out like the supplied mock-up: the Osteon banner on top, white body, grey button bar with Back / Next / Cancel. Pages: Welcome, Choose folder (default `%LOCALAPPDATA%\Programs\Osteon`, Browse, desktop-shortcut and .osteon-association options), Installing (progress), Finished ("Start Osteon now"). Osteon.exe, the README and the banner are embedded in it; it needs no administrator rights.
- The same file is the uninstaller (copied to the install folder as `Uninstall.exe`, registered in Installed apps; asks first, keeps projects and settings). Command line: `/S` silent, `/D=<folder>`, `/NOREG` (no shortcuts or registry, for tests), `/uninstall`.
- Built by `installer/build_setup.bat` from `package.ps1`. The old `install.ps1` / `uninstall.ps1` still ship in the portable zip.
- Checked: silent install into a temp folder (Osteon.exe byte-identical to the build, README and Uninstall.exe present), silent uninstall (folder removed), and the GUI clicked through Welcome, Folder, Installing and Finished into a temp folder with /NOREG (screenshots looked right). **Not done:** a real install on this PC (shortcuts, registry entries, .osteon association) was never run, because it would replace the installed Osteon; those code paths are untested.

### Pass 21: Splash stays until dismissed
- The splash no longer closes by itself: it stays until the user clicks the empty area around the card (or presses Esc / Enter / Space); a click on GITHUB still opens the page. `SplashClock::lifetime` is now 0 (stay open); a value above 0 would restore the timed close. Checked in the running app: still open after 7 s, closed on a click outside the card.

### Pass 22: Named layouts (save, load, rename, delete)
- Views > Layouts: the saved layouts are listed (click one to load it), plus "Save Layout As..." and "Manage Layouts...", which open the Layouts dialog: a name field with Save (an existing name is replaced), and a list with Load, Rename (inline, Enter to confirm, Esc to cancel) and Delete (click twice) per layout. A layout stores which of the 13 panels are open and the ImGui dock layout, for the current mode (Setup or Animate). Files: `%APPDATA%\Osteon\layouts\<name>.layout`.
- Names are trimmed, up to 48 characters, no `< > : " / \ | ? *`, no reserved Windows names; names compare without regard to case (saving "ANIMATE" over "animate" replaces it, renaming to another layout's name is refused, a change of case only is allowed).
- Code: `Layouts.cpp` (format, validation, files; unit tested: `layout_*`), `LayoutDialog.cpp`. Checked in the running app: save, rename, load (a hidden Tree came back), delete.

### Pass 23: Right-click object menu, delete confirmation, custom title bar
- **Right-click menu** (viewport, no drag): on an image Mesh / Replace Image / Set Parent / Copy / Paste / Paste Special / Duplicate / Delete Slot / Delete Bone / Rename / Copy and Paste Transform / Copy and Paste Slot Property / Bring Forward, Send Backward, Bring to Front, Send to Back / Hide, Hide Others / Lock, Lock Others / Show & Unlock All (no "Switch Display"); on a bone the subset that applies; on empty space just Show & Unlock All and Paste. Copy/Paste copies bones with their sub-trees and slots with all attachments (Paste Special puts them under the selected bone); Copy Transform works on bones and on images (x, y, rotation, scale); Lock makes an object unpickable in the viewport (still selectable in the tree; saved in project files); hotkeys Ctrl+C/V, Ctrl+Shift+C/V, Ctrl+= and Ctrl+- (Setup mode). `DocumentObjects.cpp`, `ObjectMenu.cpp`; 9 tests (`test_object_ops.cpp`).
- **Delete confirmation**: Delete / the menu / the tree ask first when other objects hang on what is deleted. A bone with children or images: "Deleting the bone X also deletes N child bones and M images". An image on a bone other than the root: "Delete image only" or "Image and bone" (the bone plus its children and images), and Delete Bone in the menu. Images on the root delete directly (nothing else could go).
- **Title bar**: the native caption is gone. A dark slate bar carries the app logo, File / Edit / Window / Help, white quick icons (New, Open, Save | Undo, Redo | Import data, Export | Texture packer, Settings), the project name in the middle and Minimize / Maximize / Close at the right (close turns red on hover). The empty part drags the window (double-click maximizes), the edges resize it, and it keeps its shadow (`Header.cpp`, `WM_NCCALCSIZE` / `WM_NCHITTEST` in `main.cpp`). "Views" is now called "Window", as in the reference.
- Checked in the running app: the image and bone menus, the bone delete dialog (and Cancel), restore, drag by the bar, double-click maximize. **Not checked in the app:** the image-on-a-bone delete dialog (the sample has all images on the root; covered by `describe_delete_*` / `delete_slot_and_its_bone_*` tests), Replace Image, Paste / Paste Special from the menu, and Windows snap layouts on the maximize button.
