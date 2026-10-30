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
| IK 1-bone / 2-bone, softness, stretch, compress, bend | ✅ | ✅ | – | partial | No IK target handle in the viewport yet; edit it in Properties |
| Transform constraint (absolute/relative, world/local) | ✅ | ✅ | – | partial | |
| Path constraint (all position/spacing/rotate modes) | ✅ | ✅ | – | partial | Sub-agent work; chain-mode tip correction is approximate |
| Physics constraint | ✅ | ✅ | – | ❌ | Original spring model; feel differs from Spine |
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
| Dopesheet: scrub, select, box select, move, copy/paste, delete, curve menu | ✅ | – | ✅ (keys, scrub) | partial | |
| Graph editor | ✅ | – | ✅ | partial | Shows values per channel. Spine also has a normalised curve view |
| Playback: play/back, step frames/keys, loop, speed, loop range [ ] | ✅ | – | ✅ | ✅ | |
| Ghosting (onion skin) | ✅ | – | – | partial | |
| Preview view (AnimationState with mixing) | ✅ | – | – | partial | |
| Mesh tools: convert, trace from alpha, generate interior, add/move/delete vertex, reset | ✅ | ✅ (geometry) | ✅ (convert, trace, generate) | partial | Topology edits require an unweighted mesh |
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
