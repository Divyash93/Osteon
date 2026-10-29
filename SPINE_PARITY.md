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
- **Dopesheet toolbar row 1:** Sync, filter, lock, ghosting toggle, and the Shift / Offset / Adjust tools are not implemented. Osteon has no Current / Loop Start / End fields in that position.
- **Grouping:** Spine groups timelines under bone rows with a collapse button. Osteon lists one row per timeline.
- **Key edit tools:** Shift, Offset and Adjust (move keys relative to time, values, or stretch) are missing.
- **Graph view:** Spine's graph toolbar and normalised view are not matched.
- **Setup mode:** not compared element by element in this pass. The tree column layout, properties panels and toolbar spacing still need a side-by-side check.

## How to verify
Open the sample in Osteon (`samples/character_rig.osteon`) and the Spineboy example in Spine, then compare each row of the list above.
