# Osteon2D runtimes

Runtimes play animations made in Osteon2D inside a game engine. They read the **Spine 4.2 JSON** that Osteon2D exports (File > Export > JSON), or the `.osteon` project file directly, together with the **libGDX-style `.atlas`** and page PNGs written by Osteon2D's Texture Packer.

| Engine | Folder | Language | Status |
|---|---|---|---|
| Unity 2021.3+ | [`unity/com.osteon2d.runtime`](unity/com.osteon2d.runtime) | C# (Unity package) | Core verified against the editor engine; Unity layer type-checked, **not yet run inside Unity** |
| Godot 4.3+ | [`godot`](godot) | GDScript (addon) | Core verified headless in Godot 4.7; sprite node renders correctly in a real window |

Both runtimes were **written independently for Osteon2D** from the public documentation of the JSON and atlas formats. They contain no Esoteric Software code and do not use the Spine Runtimes.

## What they can do

| Feature | Unity | Godot |
|---|:-:|:-:|
| Spine 4.x JSON / `.osteon` loading, libGDX atlas (incl. rotated regions) | yes | yes |
| Bones with all five inherit modes, applied pose, constraint ordering | yes | yes |
| IK (one and two bone, softness, stretch, compress), transform constraints | yes | yes |
| Region, mesh, linked mesh, weighted meshes, deform (FFD) | yes | yes |
| Skins (incl. skin-required bones and constraints) | yes | yes |
| Slot colours and attachment switching, draw order, sequences | yes | yes |
| Animation tracks, queueing, crossfade mixing (default and per pair) | yes | yes |
| Events (int, float, string) with callbacks / signals | yes | yes |
| Clipping | yes | yes |
| Blend modes (normal, additive, multiply, screen) | yes (all four) | yes (screen drawn as additive) |
| Tint black (two colour) | yes (shader `Osteon2D/Tint Black`) | not yet |
| Bone follower | yes | yes |
| Editor preview (animates in the editor) | yes | yes |
| Flip, time scale, update modes (update, late, fixed, manual) | yes | yes |
| Path constraints, physics constraints, slider constraints, Spine 4.3 transform property links | not yet | not yet |
| UI (Canvas) rendering, Timeline / Mecanim integration | not yet | not yet |
| Binary `.skel` loading | not yet (the editor exports `.skel`, see below) | not yet |

## How they were verified

`tools/golden_dump.cpp` (target `osteon_golden`) dumps bone matrices, draw order, slot colours and world vertices from the C++ editor engine for a set of animations, times, skins and crossfades. The runtimes are compared against that data:

| Check | Command | Result |
|---|---|---|
| C# core vs C++ engine | `runtimes\tests\run_core_test.bat` | 1696 checks, 0 mismatches on the sample rig; also 19 242 and 15 022 checks, 0 mismatches, on the CyberCoat project (meshes, weights, deform, IK, events, clipping, 4 skins, crossfades) |
| GDScript core vs C++ engine | `runtimes\tests\run_godot_test.bat <godot console exe>` | the same suites, 0 mismatches (Godot 4.7.2 headless) |
| Unity layer | `runtimes\tests\run_unity_compile_check.bat` | compiles against stand-ins for the Unity APIs it uses. This catches type errors only. |

What is **not** verified: the Unity package has not been opened in Unity yet (no Unity install was available), so the shader, importer and MeshRenderer wiring are untested in the real editor. Please report problems.

## Binary `.skel` export (experimental)

File > Export > Binary writes a compact `.skel` (a 4.2 style layout: big-endian numbers, variable length integers, shared string table, flags bytes) next to the JSON option. Typical size is a quarter of the JSON (the sample rig: 7.8 KB JSON, 2.0 KB binary).

How far to trust it: the writer and a matching reader (`src/engine/SkeletonBinary.cpp`) are covered by round-trip tests (every attachment type, weighted meshes, deform, all timeline kinds with Bezier and stepped curves, constraints, skins, events, draw order, sequences, physics) and by a fuzz test of the reader. The layout was written from the public description of the binary format and the 4.2 arrangement as far as it is documented, and **it has not been loaded by Esoteric's own runtimes or editor**, so treat compatibility with them as unverified. Osteon2D's Unity and Godot runtimes do not read `.skel` yet; use the JSON for them.

Left out of the file (a warning is shown in the status bar): slider constraints (Spine 4.3 only) and the property links of 4.3 transform constraints (the same-channel copy is written instead). Colours are stored as bytes, as in the real format.

## Unity quick start

1. Package Manager > Add package from disk > `runtimes/unity/com.osteon2d.runtime/package.json` (or copy the folder into `Packages/`).
2. Export from Osteon2D: File > Export > JSON, and Tools > Texture Packer (writes `name.atlas` + PNG pages). Put the files in one folder of your project.
3. Select the skeleton file in the Project window: right click > Create > Osteon2D > Skeleton Data From Selection.
4. GameObject > Osteon2D > Skeleton Animation, assign the data asset and an animation name. Press Play.

```csharp
var anim = GetComponent<OsteonSkeletonAnimation>();
anim.State.SetMix("Run", "Jump", 0.1f);
anim.Play("Run");                                   // track 0, looping
anim.State.AddAnimation(0, "Idle", true, 0);        // queued
anim.AnimationEvent += (track, e) => Debug.Log(e.name + " " + e.intValue);
anim.SetSkin("Viper");
```

Add `OsteonBoneFollower` to any object to attach it to a bone.

## Godot quick start

1. Copy `runtimes/godot/addons/osteon2d` into your project's `addons/` folder and enable the plugin (optional: the nodes register themselves).
2. Put the exported JSON (or `.osteon`), `.atlas` and page PNGs in your project.
3. Add an **Osteon2DSprite** node, set `skeleton_path` and `atlas_path`, pick a `skin` and an `animation`.
4. For exported games, add `*.osteon, *.json, *.atlas` to Project > Export > Resources > "Filters to export non-resource files".

```gdscript
$Osteon2DSprite.play("Run")
$Osteon2DSprite.add_animation("Idle", true)
$Osteon2DSprite.set_mix("Run", "Jump", 0.1)
$Osteon2DSprite.animation_event.connect(func(track, name, i, f, s): print(name))
$Osteon2DSprite.set_skin("Viper")
```

Add an **Osteon2DBoneFollower** node, assign the sprite and a bone name, to attach nodes to a bone.

## Licence

MIT, like the rest of Osteon2D. "Spine" is a trademark of Esoteric Software LLC; Osteon2D is not affiliated with it.
