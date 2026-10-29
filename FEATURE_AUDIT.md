# Feature Audit — Spine 4.3.26 Trial (reference for Osteon)

## Method and licensing constraint

The installed Spine Editor is governed by the Esoteric Software license (`C:\Program Files\Spine Trial\license.pdf`).
Section 12(c) forbids reverse engineering, disassembling or decompiling the Spine Editor. Osteon is therefore a
**clean-room** implementation. Every row below comes from one of these sources, never from the editor's code:

| Tag | Source |
|-----|--------|
| **O** | Observed in the running editor (Spine 4.3.26 Trial, launcher 4.3.08), via screenshots on 2026-10-29 |
| **C** | The user's own Spine config files: `~/Spine/hotkeys.txt` (3.8.75), `~/Spine/prefs.json`, `~/SpineTrial/settings/views-3.json` |
| **D** | Esoteric's public user guide and the public JSON export format specification |
| **U** | The user's own project data: `images/charachter.json` (Photoshop-to-Spine export) |

Spine binaries were not opened with Ghidra or REA disassembly. The editor is a Java application whose payload is `~/SpineTrial/updates/4.3.26` (14.5 MB). It has a bundled JRE in `launcher/2`.

## Shell / window (O)
- Title bar: logo/main menu, Open, Undo, Redo, project name, Views menu, window buttons.
- Start screen: New Project, Open Project, Recent/Examples, News, Tips, Learn, Changelog.
- Main menu (O): New Project (Ctrl+Shift+N), Open Project (Ctrl+O), Recent Projects ▸, Save (Ctrl+S), Save As (Ctrl+Shift+S), Package Project, New Skeleton (Ctrl+N), Import Project, Import Data, Import PSD, Export (Ctrl+E), Texture Packer (Ctrl+P), Texture Unpacker, Settings (F12).
- Views menu (O): Color (Alt+C), Mesh Tools (Alt+E), Skins (Alt+S), Weights (Alt+W), Animations (Alt+A), Metrics (Alt+M), Outline (Alt+O), Preview (Alt+R), Problems (Alt+B), Tree (Alt+T). C adds: Audio, Dopesheet (Alt+D), Ghosting (Alt+H), Graph (Alt+G), Playback (Alt+P), Timeline (Alt+L).
- Docking layout: separate layouts are kept for Setup and Animate (C: `panesSetup` and `panesAnimate`). Setup shows Editor | Tree. Animate shows Graph+Dopesheet (bottom) and Editor | Tree.

## Viewport (O/C)
- Checkerboard background. World axes cross at the origin. The mode label (SETUP / ANIMATE) is at the top-left and toggles on click or Ctrl+Tab.
- Bones are drawn as tapered shapes, coloured by bone colour. Attachments are drawn in draw order. The selected attachment gets a cyan bounding box.
- A breadcrumb bar shows the selection path (bone ▸ bone ▸ slot ▸ attachment).
- Zoom slider at bottom-left. Zoom To Fit (Ctrl+Shift+F), Zoom 100% (Ctrl+F), Rulers (Ctrl+Shift+R).
- Pan: right-drag or J. Zoom: wheel or U.

## Toolbar (O)
- Pose, Weights, Create | Rotate, Translate, Scale, Shear. Each transform row has numeric fields (rotate: 1 field; translate/scale/shear: X,Y) and a key button in Animate mode.
- Axes: Local, Parent, World. Compensation toggles (bone / image).
- Options: Bones, Images, Pixels. Visibility/selection grid for Bones, Images, Others (select, show, names).
- Animate mode adds **Auto Key**.
- Hotkeys (C): Shear Z, Scale X, Rotate C, Translate V, Pose B, Create N, Weights G. Nudge uses the arrows (Shift = 1 px).

## Tree (O)
- Search box, filter toggles (bones, slots, attachments, constraints…), and visibility and key columns.
- Nodes: skeleton ▸ root (bone hierarchy with slots and attachments), Constraints, Draw order, Skins, Events, Animations, Images, Audio.
- Icons mark bones, slots, region, mesh, IK, transform, path and physics. Key indicators appear per row in Animate mode.
- Properties panel under the tree, which depends on the selection. Region shows: Image path, Options (Select, Export, Name, Mesh, Sequence), Color, plus New… and Parent buttons.
- Bone properties (D): name, length, transform, inherit (rotation, scale, reflection), color, icon, skin-required.
- Slot properties (D): colour/dark colour (tint black), blend mode (normal, additive, multiply, screen), attachment.
- Hotkeys (C): F2 rename, Ctrl+D duplicate, Del delete, H visibility, P set parent, +/- draw order, F4 replace.

## Animate mode (O)
- Graph and Dopesheet tabs. Transport: first, previous key, play back, play, next key, last, loop.
- Dopesheet: hierarchical rows (animation ▸ bone ▸ Rotate/Translate/…; Events), a frame ruler, diamond keys, and loop start/end.
- Dopesheet toolbar (O): Sync, copy/paste, filter, lock, Shift, Offset, Adjust, Current frame field, Loop Start, End.
- Graph (O): value curves per channel with Bezier handles. Toolbar: Shift, Offset, Frame, Auto, Store presets.
- Hotkeys (C): Play D, Back A, Q/E first and last key, W/S next and previous key, R/F frame step, K key edited, L key active, Ctrl+Shift+A auto key, [ ] loop start and end, I ghosting, Y/T next and previous animation.

## Export dialog (O)
- Data: JSON, Binary, HTML. Image: GIF, PNG, APNG, WEBP, AWEBP, PSD, JPEG. Video: AVI, MOV, WEBM.
- JSON options: output folder, extension, format (JSON), pretty print, version, nonessential data, animation clean-up, warnings, export all, texture atlas pack. Buttons: Save, Load, Open after export, Export, Cancel.

## Data model (D, U)
- Skeleton: bones (parent, length, x, y, rotation, scaleX, scaleY, shearX, shearY, inherit, color), slots (bone, color, dark, attachment, blend), IK, transform, path and physics constraints, skins → slot → attachment, events, animations.
- Attachments: region, mesh (uvs, triangles, vertices weighted or unweighted, hull, edges), linkedmesh, boundingbox, path, point, clipping.
- Animation timelines: bone (rotate, translate/x/y, scale/x/y, shear/x/y, inherit), slot (attachment, rgba, rgb, alpha, rgba2), deform, drawOrder, events, ik, transform, path, physics. Curves: linear, stepped, and cubic Bezier, with 4 floats per value.
- U: the user's file is a Photoshop-to-Spine export with 18 slots on root and region attachments with x/y/width/height. Its images live in `C:/Users/Div/Desktop/images/`.

## Interop findings from importing Osteon output into Spine 4.3.26 (O, black-box)
- **Version gate:** Import Data rejects JSON whose `skeleton.spine` differs from the editor's own version ("The Spine version must be the same version that exported the data file"). The Trial always runs the latest, so Osteon offers a 4.3 export target.
- **Weighted meshes:** the importer fails (`Index -2 out of bounds`) unless each vertex's influences are listed in ascending bone order. Osteon now writes them sorted and reorders deform keys to match.
- **4.3 layout** (from Esoteric's public 4.3 example export): IK, transform, path and physics constraints share one `constraints` array in evaluation order, each with a `type`. Transform constraints use `source` and a `properties` map. Osteon reads this layout and can write it.
- Verified: Osteon's 4.3 export of the user's character imports into Spine 4.3.26. That covers bones, region attachments, a 2-bone weighted mesh, IK and transform constraints, rotate/translate Bezier keys, stepped RGBA keys, deform, draw order and events, and the `idle` animation shows in Spine's dopesheet.
- The Trial cannot export ("export is not available in the trial version"), so Spine-to-Osteon round trips were tested with Esoteric's public 4.3 example JSON instead.

## Out of scope (stated, not hidden)
- Spine binary (.skel) export, the proprietary `.spine` project format (`Mr dom.spine` is compressed and undocumented), PSD import, video export, audio, and the Spine runtimes license model.
- See `PROGRESS.md` → *Unfinished* for the live list.
