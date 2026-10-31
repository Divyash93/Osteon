# Osteon vs Spine user guide checklist

Method: Spine feature list taken from the guide's page structure and the existing audits (FEATURE_AUDIT.md, SPINE_PARITY.md, PROGRESS.md), then checked with Grep over src/editor and src/engine. Nothing was run. COMPLETE means code present, untested by the checker. Individual guide pages were not each re-fetched; only the table of contents was.

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| **Bones** | | | |
| Create bones by drag (Create tool), chains | COMPLETE | Viewport.cpp, Document::createBone | |
| Reparent, Set Parent, drag in tree | COMPLETE | Document.cpp, Panels.cpp | |
| Inherit modes (5), length, color, icon | COMPLETE | Skeleton.cpp, Panels.cpp boneIconNames | |
| Skin-required bones | COMPLETE | Model.h, Skeleton.cpp | No Skin... button in bone panel |
| Split bone | MISSING | Panels.cpp:776 disabled button | |
| Per-bone Select / Name flags | MISSING | SPINE_PARITY.md | |
| Mirror / flip skeleton or bones | MISSING | no code | |
| **Slots** | | | |
| Slot color, dark color, blend modes | COMPLETE | Gfx.cpp, Document::setSlotColor | |
| Slot attachment setup, draw order +/- and drag | COMPLETE | Panels.cpp tree | |
| **Images / Tools / Keys / Animating** | | | |
| Images folder, auto slots | COMPLETE | App.cpp:225 | |
| Replace image (F4) | PARTIAL | Panels.cpp (no clear F4 handler found) | verify |
| Rotate/Translate/Scale/Shear tools, numeric fields | COMPLETE | Viewport.cpp | Shear drag approximate |
| Pose tool | PARTIAL | Document.cpp:84 | Does not solve chains |
| Axes Local/Parent/World | PARTIAL | Document.h | Gizmo and fields only |
| Bone / image compensation | PARTIAL | Document.h:52 flags only; no use in .cpp | likely not wired |
| Nudge with arrows | COMPLETE | per FEATURE_AUDIT | |
| Auto Key, K / L keying, pending pose | COMPLETE | Document.cpp, Timeline.cpp | |
| Key buttons for every property | COMPLETE | Panels.cpp | |
| Curves linear/stepped/Bezier | COMPLETE | Animation.cpp, Timeline.cpp | |
| **Attachments** | | | |
| Region (offset, rot, scale, color) | COMPLETE | Model.h, Viewport.cpp | |
| Sequences | PARTIAL | Animation.cpp:271 | Not checked in app |
| Mesh: trace, generate, edit, UV | COMPLETE | Geometry.cpp, Panels.cpp | |
| Mesh manual edges, Deformed checkbox, Freeze | MISSING | PROGRESS.md | |
| Linked mesh | COMPLETE | Document.cpp | |
| Bounding box | COMPLETE | Viewport.cpp | Hit-test use only in runtime |
| Clipping | COMPLETE | Gfx.cpp stencil | Not checked in app |
| Path attachment + editing | COMPLETE | Viewport.cpp:388 | Close/insert untested |
| Point attachment | COMPLETE | Viewport.cpp:443 | |
| **Skins** | | | |
| Skins, default fallback, active skin | COMPLETE | Panels.cpp:423 | |
| Skin placeholders | MISSING | Panels.cpp:795 disabled | |
| Skin bones / skin constraints | MISSING | Panels.cpp:1313 disabled | |
| **Constraints** | | | |
| IK 1/2 bone, softness, stretch, compress | COMPLETE | Constraints.cpp | No viewport IK handles |
| Transform constraint | PARTIAL | Constraints.cpp | 4.3 cross-channel mappings and ranges missing |
| Path constraint | COMPLETE | PathConstraint.cpp | Chain tip approximate |
| Physics constraint | PARTIAL | PhysicsConstraint.cpp | Feel differs from Spine; no Skin..., warm up |
| Slider constraint (4.3) | MISSING | Panels.cpp:815 disabled | |
| Constraint ordering | COMPLETE | Skeleton.cpp | |
| **Events** | | | |
| Event defs and keys | COMPLETE | Panels.cpp:1374 | |
| Audio playback | MISSING | no player | |
| **Views** | | | |
| Tree (search, filters, visibility) | COMPLETE | Panels.cpp | |
| Dopesheet | PARTIAL | Timeline.cpp | Row reorder, some settings missing |
| Graph | PARTIAL | Timeline.cpp | No normalised view |
| Timeline / Playback | COMPLETE | App.cpp:494 | |
| Animations view (mix, duplicate) | COMPLETE | Panels.cpp | |
| Preview view | PARTIAL | Animation.cpp AnimationState | Simplified mixing |
| Ghosting | COMPLETE | Timeline.cpp:373 | |
| Outline, Metrics, Problems | COMPLETE | Panels.cpp | |
| Weights (bind, auto, brush, smooth, prune) | COMPLETE | Geometry.cpp | Weld missing |
| Mesh Tools | COMPLETE | Panels.cpp | |
| Skins view | COMPLETE | showSkins | |
| Slot Color view | PARTIAL | Panels.cpp | No dedicated view |
| Audio view | MISSING | | |
| Welcome screen | PARTIAL | Document::loadRecent | No start page |
| **Export / Import** | | | |
| JSON export (4.2 and 4.3) | COMPLETE | Dialogs.cpp | |
| Binary .skel | MISSING | Dialogs.cpp:35 | |
| PNG sequence, GIF | COMPLETE | Dialogs.cpp | |
| APNG/WEBP/video/PSD image export | MISSING | | |
| Animation clean-up, export scale for JSON | MISSING | | |
| Texture packer | COMPLETE | Geometry.cpp | |
| Texture unpacker | MISSING | | |
| Import JSON data | COMPLETE | App.cpp:302 | |
| Import .spine project | MISSING | undocumented format | N/A-ish |
| Import PSD | PARTIAL | Psd.cpp | 8-bit RGB only; no groups or masks |
| Command line interface | PARTIAL | main.cpp | Only crash-test and file open |
| **Settings / misc** | | | |
| Settings (theme, autosave) | PARTIAL | App.cpp | |
| Hotkey customization | MISSING | fixed list | |
| Undo/redo | COMPLETE | Document.cpp | |
| Auto-save / recovery | COMPLETE | App.cpp | |

## Counts (approximate)
COMPLETE 31, PARTIAL 14, MISSING 15, N/A 0 (one row N/A-ish).

## Top 15 MISSING / PARTIAL, ranked
1. Slider constraint
2. Binary .skel export
3. Transform constraint 4.3 mappings and ranges
4. Skin placeholders
5. Skin bones and skin constraints
6. Graph normalised curve view
7. Viewport handles for IK and constraints
8. Split bone
9. Bone and image compensation (wired?)
10. Export clean-up and scale options
11. Hotkey customization
12. Audio events playback
13. PSD import groups, masks, 16-bit
14. Pose tool chain solving
15. Mesh manual edges and Weld
