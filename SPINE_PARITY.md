# Spine parity report (Osteon vs Spine 4.3.26 Trial)

Status: partial. Osteon does not yet match Spine element-for-element. This file lists what was compared and what changed.

## Done in this pass
- **Dopesheet transport:** text buttons replaced with icons (first key, previous key, previous frame, play backward, play/pause, next frame, next key, last key, loop). Shown and checked in the app.
- **Timeline rows:** each row shows an icon for its type (rotate, translate, scale, shear, attachment, color, deform, draw order, events, IK, transform, path, physics).
- **Icon set:** added icons for transport, sync, filter, lock, shift, adjust, group open/closed, color and deform (`src/editor/Icons.cpp`).
- Earlier passes: icon toolbar, image hover outline, name chip, gizmo handles, tree icons, icon eye toggles, Set Parent for slots and attachments.

## Spine Animate mode, observed
- Dopesheet toolbar row 1: Sync, copy/paste keys, filter, lock, refresh, ghosting, key-selected, bone-select, then Shift / Offset / Adjust tools and the Current / Loop Start / End fields.
- Transport row: first, previous key, previous frame, play backward, play, next frame, next key, last, loop toggle.
- Rows are grouped by bone with a collapse button (−), and each row has its own type icon.
- Ruler with key markers on the top row; zoom controls at bottom-left.

## Offset tool: unverified
- Osteon's Offset moves the dragged key and every later key in the visible timelines by the same amount.
- A test in Spine did not confirm that. The dragged key moved right as expected, but the key before it appeared to move left and the last key stayed put. The test was too small to measure reliably.
- Treat Offset as unverified until it is checked against Spine on a clean project.

## Not yet matched (known gaps)
- **Dopesheet toolbar:** Sync, Filter, Lock, Ghosting, Shift, Current / Loop Start / End and Offset are in place. Adjust is still disabled. Copy/paste keys and the bone-select toggle are not added to the toolbar.
- **Grouping:** done. Timelines sit under bone or slot headers that fold, with key summaries.
- **Key edit tools:** Shift and Offset exist (Offset is unverified). Adjust (stretch selected keys) is missing.
- **Graph view:** Spine's graph toolbar and normalised view are not matched.
- **Setup mode:** not compared element by element. The tree column layout, properties panels and toolbar spacing still need a side-by-side check.

## How to verify
Open the sample in Osteon (`samples/character_rig.osteon`) and the Spineboy example in Spine, then compare each row of the list above.

## Setup mode comparison (first pass)
Compared Spine 4.3.26 Trial with Osteon's Setup view. Items are listed by where they differ.

| Area | Spine | Osteon | Status |
|---|---|---|---|
| Top chrome | Logo button opens the main menu; folder, undo, redo, Views dropdown and window buttons in one bar; no menu bar | Classic menu bar (File, Edit, Views, Help); mode button at top right | Not matched (layout) |
| Mode label | Cyan icon + "SETUP" at top left, with the TRIAL banner | "SETUP" text in the viewport corner | Partly matched |
| Transform fields | Rotate/Translate/Scale/Shear show as bars; numbers appear only when a bone is selected | Number fields always visible | Not matched |
| Pose / Weights / Create | Icon column, stacked | Icon buttons, stacked | Matched |
| Local / Parent / World | Icon buttons | Icon buttons | Matched |
| Bones / Images / Pixels | Icon toggle buttons | Text toggle buttons | Not matched |
| Visibility grid | Select, eye and tag icons per row | Dots with text labels | Not matched |
| Tree rows | Eye column at the left; hierarchy lines; type icons | Eye at the right; hierarchy lines; type icons | Partly matched |
| Zoom controls | Fit and zoom buttons bottom left | Fit and 100% buttons bottom left | Partly matched |

## Next Setup work, in order
1. Move the tree eye column to the left.
2. Show the transform fields only when a bone is selected.
3. Replace the Bones/Images/Pixels text toggles and the visibility grid with icon buttons.
4. Decide whether to match Spine's top bar layout. This is a larger change, so it needs a decision first.

## Properties panels (this pass)
Colors were sampled from a Spine 4.3.26 screenshot: panel #626262, tree #585858, fields #3b3b3b, borders #2d2d2d, selection #2885be, viewport checker #515151 / #565656.

| Panel | Status |
|---|---|
| Skeleton, Bone, Slot, Region, Mesh, Linked mesh | Spine-style table with icon labels, teal header with duplicate / rename / delete buttons, button row (New..., Parent, Edit Mesh, Weights) |
| IK, Transform, Path, Physics, Skin, Animation, Event | Same table style. Path and Physics now have an Order row |
| Bounding box, Clipping, Path attachment, Point | Theme colors only; layout not restyled |

Captured from Spine and matched: the Bone panel (Inherit, Length, Icon, Color rows; New..., Parent, Split buttons). Not captured from Spine (its tree would not accept clicks from my automation): the Slot, constraint, skin, animation and event panels. Those follow the same style, but their rows come from Osteon's own fields, not from Spine.

Known differences: Spine's Bone panel has Select and Name checkboxes, an icon picker and a Skin... button; Osteon has no per-bone select or name flags, and Split is disabled.

## Dopesheet key tools
Copy, cut, paste, delete, and Linear / Bezier / Stepped curve buttons were added to the dopesheet toolbar. Adjust is still disabled, and Offset is unverified.

## Spineboy check (public 4.3.75-beta export)
Run by editor tests (`tests/editor/test_spineboy.cpp`), with the example's images copied to `samples/spineboy_test` (ignored by git):
- Loads with 67 bones, 52 slots and 11 animations.
- All 231 sampled poses across the 11 animations are finite, including mesh vertices.
- The run cycle swings the foot target through a stride.
- The weighted head mesh can be unbound without changing its shape; the traced hull covers at least 90% of the opaque pixels; rebinding to head and hair bones keeps the shape and weights sum to 1.
- A physics constraint on `hair2` responds to motion and settles. **Order matters:** a constraint that runs before other constraints is overwritten when they recompute the bone, so physics must have the highest order. The editor already gives new constraints the last order.

Open items from this check:
- The public Spineboy export has no physics constraint, so physics was tested by adding one in code.
- Osteon shows `head` translate 27.66, -0.26 (matches the JSON), while Spine's own project showed 24.728, -12.403. I did not find the reason.
- 4.3 transform constraints are read as same-channel copies, so seven constraints produce warnings.
- I did not compare Spineboy's rendering with Spine's side by side.
