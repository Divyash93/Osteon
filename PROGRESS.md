# Progress and Verification

Status as of 2026-10-29. The columns are tracked separately, and a feature counts as done only with evidence.

- **Impl**: code exists and is wired into the app.
- **Unit**: covered by an automated test (`build.bat test` runs 42 engine tests and 13 editor tests, all passing).
- **App**: exercised by hand in the running `Osteon.exe` against the user's images (`C:\Users\Div\Desktop\images`), with screenshots taken during development.
- **vs Spine**: compared with the running Spine 4.2.x-era behaviour or the public format specification. *partial* means the behaviour follows documented semantics but has not been compared side by side.

| Feature | Impl | Unit | App | vs Spine | Notes |
|---|:-:|:-:|:-:|:-:|---|
| Bone world transforms, 5 inherit modes | ✅ | ✅ | ✅ | partial | Applied-pose decomposition round-trips in every mode |
| Skeleton position, scale, flips | ✅ | ✅ | – | partial | |
| Region attachments (offset, rotation, scale, size, color) | ✅ | ✅ | ✅ | ✅ | User's 18-part character renders exactly as exported from Photoshop |
| Mesh attachments (unweighted and weighted) | ✅ | ✅ | ✅ | partial | |
| Linked meshes | ✅ | – | – | partial | Load, save and render only; no UI to create them |
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
| Timelines: deform, draw order, events, IK, transform, path | ✅ | ✅ | – | partial | Sequence timelines are stored but not played |
| Curves: linear, stepped, Bezier (absolute 4.x handles) | ✅ | ✅ | ✅ | ✅ | Graph presets and draggable handles checked in the app |
| AnimationState: tracks, queueing, crossfade mixing | ✅ | ✅ | – | partial | Simplified crossfade without Spine's hold/first-mode rules |
| Spine JSON import (3.8, 4.x, Photoshop export) | ✅ | ✅ | ✅ | ✅ | `charachter.json` loads with 18 slots and correct placement |
| Spine 4.2 JSON export / `.osteon` project save and open | ✅ | ✅ | ✅ | partial | Stable byte-for-byte round trip. Not yet loaded into Spine itself |
| Undo / redo (all edits, snapshot based) | ✅ | ✅ | ✅ | ✅ | |
| Setup / Animate mode (Ctrl+Tab, SETUP label) | ✅ | ✅ | ✅ | ✅ | |
| Tools: rotate, translate, scale, shear (drag + numeric) | ✅ | – | ✅ | partial | Shear by drag is an approximation (Shift = Y axis) |
| Pose tool | ✅ | – | – | ❌ | Rotates the parent so the bone follows the mouse; Spine's pose tool solves more |
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
| Weights: bind (auto weights), unbind, paint brush, set, prune, normalize | ✅ | ✅ | – | partial | |
| Export: JSON, PNG sequence, animated GIF | ✅ | – | ✅ (JSON, GIF) | partial | GIF output: 521×1269, 16 frames, looping |
| Texture packer (MaxRects, rotation, pow2, `.atlas`) | ✅ | ✅ | – | partial | |
| Metrics, Outline, Problems views | ✅ | – | – | partial | |
| Native install: portable zip and per-user installer (IExpress) | ✅ | – | – | n/a | `OsteonSetup.exe` is built but has not been installed on this PC |

## Unfinished or missing (not claimed as done)
- Binary `.skel` export, PSD import, video export (AVI/MOV/WEBM), APNG/WEBP export, audio and audio events playback.
- Opening Spine's proprietary `.spine` project files. The format is undocumented and compressed, so `Mr dom.spine` cannot be opened. Export JSON from Spine instead.
- Separate dock layouts for Setup and Animate (Spine keeps two). Osteon has one layout.
- Skin placeholders, skin constraints UI, linked-mesh creation, sequence (frame-by-frame) playback.
- Viewport handles for IK targets, path vertex editing and constraint-specific gizmos. Graph view lacks Spine's normalised curve mode, and the dopesheet lacks Shift/Offset/Adjust tools.
- Hotkey customisation (the list is fixed; see Help > Hotkeys).
- Side-by-side behavioural comparison against Spine for physics, path chain modes and mixing edge cases.
- Exported JSON has not yet been loaded in Spine or a Spine runtime to confirm compatibility end to end.

## Bugs found and fixed during verification
1. A slot reparented to a different bone rendered relative to the old bone, because the runtime didn't rebuild. `Skeleton::needsRebuild` now compares slot bones (regression test `doc_reparent_slot_keeps_image_in_place`).
2. Clicking a key in the Graph changed its value. A value now changes only on a real drag, by relative motion.
3. Zoom-to-fit ran before the docked viewport had its final size.
4. Pen taps and synthetic clicks sent in the same frame as a pointer jump were missed by widgets. Such presses are now replayed one frame later.
5. Selecting bones for weight binding hid the mesh from the Mesh Tools and Weights views.
6. A test held a reference to a key across a vector insert (test bug, not an engine bug).

## Process and token usage
- REA 6.8.0 with the Ghidra 12.1.4 provider is configured. It was **not** used on the Spine Editor binaries, because the license forbids reverse engineering. Spine 4.3.26 Trial was observed through screenshots of the running UI (main menu, Views menu, toolbar, tree, properties, dopesheet, graph, export dialog), together with the user's Spine hotkey and layout files.
- Sub-agents: 2, both on the session's model. Path and physics constraints used 104,728 tokens; geometry, weights and atlas used 104,969 tokens. They worked in separate files, so there were no conflicting edits.
- Main session context at the time of writing: about 546k tokens (reported by the app). Plan usage: 68% of the 5-hour window and 9% of the weekly limit.
