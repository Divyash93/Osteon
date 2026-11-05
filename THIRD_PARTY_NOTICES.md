# Third-party notices

Osteon2D itself is released under the MIT License (see `LICENSE`). It includes, or is built with, the following third-party components. Each keeps its own licence.

| Component | Where | Licence | Copyright |
|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) | `third_party/imgui/` | MIT (`third_party/imgui/LICENSE.txt`) | (c) 2014-2026 Omar Cornut |
| [nlohmann/json](https://github.com/nlohmann/json) 3.11.3 | `third_party/nlohmann/json.hpp` | MIT (SPDX header in the file) | (c) 2013-2022 Niels Lohmann |
| `stb_*` single-file libraries bundled with Dear ImGui | `third_party/imgui/` | MIT or public domain, as stated in each file | Sean Barrett and contributors |

## Not bundled

- **Segoe UI** is loaded from the user's own Windows installation at run time. It is not distributed with Osteon2D.
- **Windows, Direct3D 11, WIC and the Windows SDK** are system components provided by Microsoft.

## Trademarks and file formats

- "Spine" is a trademark of Esoteric Software LLC. Osteon2D is not affiliated with or endorsed by Esoteric Software. Osteon2D reads and writes the publicly documented Spine JSON format; it contains no Spine Editor or Spine Runtimes code. If you ship Spine runtimes with a product, the [Spine Runtimes License](https://esotericsoftware.com/spine-runtimes-license) applies to them.
- "Photoshop" and "PSD" are trademarks of Adobe Inc. Osteon2D reads the documented PSD file format and is not affiliated with Adobe.
