# Osteon

**A free, open-source, native 2D skeletal animation editor for Windows.**

Osteon lets you rig and animate 2D characters the way professional skeletal animation tools do: build a bone hierarchy, attach your artwork, deform it with meshes and weights, constrain it with IK, transform, path and physics constraints, and key everything on a dopesheet and graph editor. It reads and writes the publicly documented Spine-format JSON, so animations can move between Osteon and other tools that speak that format.

It is one small native executable (C++20, Win32, Direct3D 11, Dear ImGui). It needs no browser, runtime or installer dependencies, and it starts instantly.

Author: Divyash ([github.com/Divyash93](https://github.com/Divyash93)).

## What it can do

- **Rigging:** bones with all five inherit modes, slots, skins, draw order, bone icons, multi-selection, set parent, a Pose tool and a Create tool.
- **Art:** region attachments, meshes (Modify / Create / Delete / New tools, Trace with Detail / Concavity / Refinement, Generate, Apply), linked meshes, bounding boxes, clipping, points, and Bezier paths drawn with Create / Modify / Delete. Sequences for frame-by-frame images. Photoshop **PSD import** turns layers into slots.
- **Weights:** Bind, Auto, Smooth, Prune, Swap, locks, Direct and Brush painting, pies and overlays.
- **Constraints:** IK (1 and 2 bone, softness, stretch, compress), transform, path and physics constraints, with keyable properties, Global rows, Simulate and Reset.
- **Animation:** Setup and Animate modes, Auto Key, a dopesheet with overview, bone and property rows, filters, lock, sync, Adjust, box scaling, copy and paste, a graph editor with Bezier handles, zoom on both, ghosting, a preview with mixing.
- **Interface:** dockable panels, a Photoshop-style dark theme (and a Light theme), modern sliders with typed values, rulers, a zooming checkerboard, undo and redo for every edit.
- **Safety:** Auto-save with crash recovery (a recovery copy of unsaved work, offered back after an unexpected close) and a crash report for bug reports.
- **Output:** `.osteon` projects, Spine JSON (4.2 and 4.3 layouts), PNG sequences, animated GIFs and a MaxRects texture atlas packer.

Osteon is early software (version 0.1.0). It is already usable for real rigs, but expect rough edges. `PROGRESS.md` lists what is done, what is checked and what is still missing, honestly.

## Open source, and where it is going

Osteon is open source and meant to grow with its community. **The plan is to build official runtimes for the popular game engines together**, so that animations made in Osteon play natively in games. The runtimes are not written yet. Likely first targets, in no particular order:

- Unity, Godot and Unreal Engine
- Web (PixiJS, Phaser, plain WebGL / Canvas)
- libGDX, Cocos, SDL-based and custom C / C++ engines

The engine in `src/engine/` is deliberately separate from the editor (no UI code), which makes it the natural starting point for a C++ runtime and a reference for ports to other languages. If you want to help with a runtime, an importer, a bug or a feature, you are welcome: open an issue or a pull request, and say which engine you care about.

## Run

- **Installer:** `dist\OsteonSetup.exe` installs for the current user, with no admin rights needed. It adds Start Menu and Desktop shortcuts, a `.osteon` file association and an uninstall entry in *Installed apps*.
- **Portable:** unzip `dist\Osteon-0.1.0-win64.zip` and run `Osteon.exe`. You can pass a project or JSON file on the command line, or drag one onto the window.

Requirements: Windows 10/11 x64 and a Direct3D 11 GPU. Without a suitable GPU it falls back to the WARP software renderer.

## Build from source

Prerequisite: Visual Studio 2022 with the "Desktop development with C++" workload. CMake and Ninja come with it.

```bat
build.bat            :: Release build -> build\Osteon.exe
build.bat test       :: build, then run the engine and editor test suites
```

Or open the folder in Visual Studio ("Open Folder"), which uses `CMakeLists.txt` directly.

To package the portable zip and installer (this runs the tests first):

```powershell
powershell -ExecutionPolicy Bypass -File package.ps1
```

## Quick start with your own PNG parts

1. **File > Import Data...** and choose a Spine JSON (e.g. a Photoshop-to-Spine export). Alternatively, **File > Set Images Folder...** creates one slot per image.
2. Press **N** (Create) and drag in the viewport to create bones. A bone created while another is selected becomes its child.
3. Select an image, press **P** and click a bone to parent it. Rotate (**C**), translate (**V**), scale (**X**) and shear (**Z**) with the toolbar or by dragging.
4. Press **Ctrl+Tab** for Animate mode. With **Auto Key** on, every change is keyed at the current frame. Scrub on the dopesheet ruler, and press **D** to play.
5. Ease keys in the **Graph** view (Linear, Stepped, Bezier presets, draggable handles).
6. **Ctrl+S** saves an `.osteon` project. A project is Spine 4.2 JSON plus an editor block, so runtimes can read it directly. **Ctrl+E** exports JSON, PNG sequences or GIFs. **Ctrl+P** packs a texture atlas.

All hotkeys are listed under **Help > Hotkeys (F1)**.

## Repository layout

| Path | Contents |
|---|---|
| `src/engine/` | Animation engine (no UI): data model, skeleton, constraints, animation, JSON, geometry |
| `src/editor/` | Win32/D3D11/ImGui editor |
| `tests/` | Engine tests (`osteon_tests`) and editor document tests (`tests/editor`, `osteon_editor_tests`) |
| `third_party/` | Dear ImGui (docking branch, MIT) and nlohmann/json 3.11.3 (MIT) |
| `installer/` | Per-user install and uninstall scripts |
| `prototype/js-engine/` | The original JavaScript prototype, kept for reference |
| `FEATURE_AUDIT.md`, `IMPLEMENTATION_PLAN.md`, `ARCHITECTURE.md`, `PROGRESS.md` | Audit, plan, design and status |

## Contributing

Issues and pull requests are welcome. Please run `build.bat test` before sending a change, and keep new behaviour covered by a test where it can be (`tests/` for the engine, `tests/editor/` for document logic). `ARCHITECTURE.md` explains how the engine and editor fit together.

## License

Osteon is released under the MIT License (see `LICENSE`). Third-party code in `third_party/` keeps its own licenses.

## Licensing note

Osteon is an independent implementation and is not affiliated with Esoteric Software. The Spine Editor was examined only as a running program and through its public documentation and file format; its code was not decompiled (Spine license §12(c)). Osteon reads and writes the publicly documented Spine JSON format. If you ship Spine *runtimes* in a product, the Spine Runtimes License applies to those runtimes.
