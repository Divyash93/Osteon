# Osteon

Osteon is a native Windows 2D skeletal animation editor with a Spine-style workflow: Setup and Animate modes, bones, slots, skins, meshes, weights, IK/transform/path/physics constraints, a dopesheet and graph editor, and Spine-format JSON import and export.

Author: Divyash ([github.com/Divyash93](https://github.com/Divyash93)).

It is a single native executable (C++20, Win32, Direct3D 11, Dear ImGui). It needs no browser, runtime or installer dependencies.

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

## Licensing note

Osteon is an independent implementation and is not affiliated with Esoteric Software. The Spine Editor was examined only as a running program and through its public documentation and file format; its code was not decompiled (Spine license §12(c)). Osteon reads and writes the publicly documented Spine JSON format. If you ship Spine *runtimes* in a product, the Spine Runtimes License applies to those runtimes.
