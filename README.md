<p align="center">
  <img src="resources/Glider_AppIcon_256.ico" width="96" alt="Glider Logo" />
</p>

<h1 align="center">Glider glTF Viewer</h1>

<p align="center">
  A lightweight, high-performance, standalone <b>glTF 2.0 Visual Inspector & Viewer</b> based on <a href="https://github.com/ux3d/Gestaltor">UX3D Gestaltor</a>.
</p>

<p align="center">
  <a href="https://github.com/uraymeiviar/gltf-viewer/releases"><img src="https://img.shields.io/github/v/release/uraymeiviar/gltf-viewer?include_prereleases&color=21AB6B&label=Release" alt="Latest Release" /></a>
  <img src="https://img.shields.io/badge/UI-Dear%20ImGui-blue" alt="Dear ImGui" />
  <img src="https://img.shields.io/badge/Architecture-100%25%20Standalone%20Single%20Binary-success" alt="Standalone" />
  <img src="https://img.shields.io/badge/Compression-Zstandard%20VFS-orange" alt="Zstandard" />
</p>

---

![Glider Viewer: E195 cockpit with HDR, bloom and auto-exposure](docs/images/e195_cockpit_hdr.jpg)

![Glider Viewer: windshield screen-space reflections and glass](docs/images/e195_cockpit_windshield.jpg)

## Overview

**Glider** modernizes and streamlines 3D asset inspection by stripping out the legacy, heavyweight Qt framework and replacing the entire UI layer with **Dear ImGui (Docking)** and **GLFW**. Designed as a fast, dedicated **"viewer only" / visual inspector** for glTF and GLB 3D assets, Glider provides rich diagnostics, multi-camera views, and high-fidelity PBR rendering in a single portable executable.

### Key Highlights & Features

- **100% Standalone Single Executable**: All runtime resources—including GLSL shaders, the default IBL HDR environment map (`default_hdr_env4k.hdr`), validation schemas, UI fonts (`fss-ui.ttf` and `fss-ui-mono.ttf`), application icons, and default models—are compressed with **Zstandard (`zstd`)** level 19 and embedded directly inside `Glider.exe`. No external `resources` folder, assets directory, or external DLLs are required to run.
- **Multi-Viewport Architecture**:
  - Open multiple concurrent 3D viewports docked as tabbed views or side-by-side splits.
  - Viewports are first-class items listed in the **Scene Hierarchy** tree; clicking a viewport opens its individual settings directly in the **Inspector**.
  - Attach any viewport to a scene camera object via the toolbar dropdown (`[ Free Look | <camera-list> ]`) or create a new viewport pinned to a camera from the **Viewport** menu.
  - Clean viewport toolbar with perspective/isometric toggle, floor grid toggle, bounding box toggle, turntable auto-rotate toggle, camera selector, and shading mode dropdown.
- **Comprehensive Image & Texture Inspection**:
  - Proportional image preview in the **Element Inspector** constrained to the inspector's width with a crisp 1px border.
  - Displays complete image metrics: dimensions, aspect ratio, exact channel count (Grayscale, RG, RGB, RGBA), and bit depth (e.g., 8-bit, 16-bit per channel / 48-bit, 64-bit total).
- **HDR Rendering Pipeline**:
  - The scene renders linear into a 16-bit floating-point target; tone mapping (Khronos PBR Neutral, ACES Hill, ACES Narkowicz) is one final pass, so transparents and glass blend in linear light.
  - **Physically based bloom** (Jimenez, *Call of Duty: Advanced Warfare*): bright and emissive surfaces glow (`KHR_materials_emissive_strength` up to 1000); resolution independent and stable under TAA.
  - **Auto-exposure**: centre-weighted luminance metering with outlier rejection, smooth adaptation (separate up/down speeds), EV compensation and min/max limits; or manual EV.
  - Settings live with the scene's environment (Scene Inspector → Environment). See [`docs/HDR_PIPELINE.md`](docs/HDR_PIPELINE.md).
- **Anti-Aliasing**: geometry MSAA (Off, 2x, 4x, 8x) and post AA (None, FXAA 3.11, TAA) chosen independently per viewport. TAA runs in HDR with 8-phase Halton(2, 3) jitter, neighbourhood clamping and luminance weighting; overlays (grid, gizmos, bounds) stay steady under TAA. New viewports default to MSAA Off + TAA.
- **Microsoft Flight Simulator (ASOBO) Extensions**: Glider renders the `ASOBO_*` glTF extensions a file declares (MSFS 2020/2024 exports), reading only the glTF itself:
  - **Decals**: `ASOBO_material_geometry_decal` / `ASOBO_material_blend_gbuffer` through a decal buffer with per-channel blending and a configurable depth tolerance.
  - **Glass**: `ASOBO_material_glass_v2` with reflections independent of opacity and refraction through a parallel slab (`glassWidth`), frosted by roughness.
  - **Windshields**: `ASOBO_material_windshield_v3` with screen-space reflections (on by default; quality Low/Medium/High) that reflect the cockpit interior, with an interior sky-reflection strength.
  - Draw order, invisible materials and environment occluders, normal-map convention, vertex encoding of optimized assets, and more; each extension can be switched off for the whole app (File → Settings → glTF Extensions).
  - **Live tuning**: ASOBO extension parameters (e.g. glass width, SSR attenuation, decal factors) are editable in the Inspector in memory, without changing the file.
- **Built-in In-Process glTF 2.0 Validator**:
  - High-performance, native C++20 validation engine running directly in-process with zero external subprocesses or tools needed.
  - **Tier 1 (JSON Schema Validation)**: Evaluates loaded assets against official Khronos glTF 2.0 and extension schemas embedded inside the executable.
  - **Tier 2 (Semantic & Buffer Integrity Checks)**: Verifies buffer view alignments, accessor strides/bounds, min/max values, geometry attribute parity, vertex index bounds, degenerate triangles, animation monotonicity, and scene hierarchy cycle detection.
  - **Interactive Validation Panel**: Filterable issues table (Errors 🔴, Warnings 🟡, Infos 🔵), text search, single-click Inspector selection, double-click 3D viewport framing (`F`), and one-click GitHub-flavored Markdown report export.
  - Viewport toolbar status pill indicator (`[ ✓ glTF Valid ]` / `[ 🔴 N Errors ]`).
- **Streamlined Menu Navigation**:
  - **File**: Open, Re-open Recent, Save As (glTF/GLB), Search Paths, Exit.
  - **Viewport**: New Viewport, New Viewport From Camera, Grid/BBox/Turntable toggles, Camera Detach, Orientation Presets, Shading Mode, and Camera Reset / Fit to Scene.
  - **Panel**: Toggle Scene Hierarchy, Resources, Inspector, Statistics, Validation, Logger, and Reset Default Layout (`Ctrl+L`).
- **Blender-Style Viewport Controls**:
  - Middle Drag: Orbit / Rotate view
  - Shift + Middle Drag: Pan view
  - Ctrl + Middle Drag / Mouse Wheel: Smooth Zoom
  - Focus (`F`): Instantly frame and focus camera on the selected node or active scene
  - 3D Orientation Gizmo & Compass: Clickable view axes (Front, Back, Right, Left, Top, Iso)
- **Native GPU Color Picking**:
  - Direct 3D mesh selection in viewport with sub-pixel precision.
  - Highlights selected objects using **Oriented Bounding Boxes (OBB)** matching Blender's local bounding box convention.
  - Local coordinate gizmo indicator for selected nodes with Blender ($Z$-up) or glTF ($Y$-up) axis conventions.
- **Advanced glTF 2.0 PBR Rendering**:
  - Full support for glTF 2.0 Core specification and PBR metallic-roughness materials.
  - Extension support: Specular-Glossiness, Clearcoat, Sheen, Transmission, Volume, IOR, Iridescence, Material Variants, and Punctual Lights.
  - Real-time Image-Based Lighting (IBL) with spherical harmonics and environment radiance filtering supporting both Radiance HDR (`.hdr`) and OpenEXR (`.exr`) panoramic environment maps.
  - Infinite fading ground grid and 30+ render debug modes (Wireframe, Normal, Roughness, Metallic, Base Color, UV, Clearcoat, Sheen, etc.).
  - Window state and recent file history persistence across sessions.

---

## Download & Run

Pre-compiled standalone binaries are available on the [**Releases**](https://github.com/uraymeiviar/gltf-viewer/releases) page.

Simply extract `Glider.exe` anywhere and run. No installation or setup is needed. Glider is fully portable: all settings, layout, and session state live in a single `settings.ini` next to `Glider.exe`, written only when a value differs from its default (with automatic fallback to `%APPDATA%/Glider` if the directory is unwritable).

```bat
Glider.exe [path_to_model.gltf / path_to_model.glb]
```

### Command-Line Arguments & Automation

`Glider.exe` supports CLI flags for batch testing and automated image verification:
- `[model.gltf / model.glb]`: Path to glTF or GLB file to open on startup (defaults to embedded `HelloCube.glb`).
- `--aa <none|msaa2|msaa4|msaa8|fxaa|taa>`: Legacy AA preset; `--msaa <0|2|4|8>` and `--post-aa <none|fxaa|taa>` set geometry and post AA independently.
- `--exposure <ev>`, `--auto-exposure on|off`, `--exposure-compensation <ev>`, `--tone-mapping <neutral|hill|narkowicz>`: Exposure and tone mapping.
- `--bloom on|off`, `--bloom-intensity <x>`: Bloom.
- `--ssr on|off`, `--ssr-quality <low|medium|high>`: Windshield screen-space reflections.
- `--asobo on|off`, `--asobo-disable <extension>`: MSFS (ASOBO) extensions applied by the viewport.
- `--set-asobo-param <material> <extension> <param> <value>`: Edit an ASOBO extension parameter in memory after loading.
- `--debug-view <name>`: Start in a render debug mode (e.g. `roughness`, `shadingnormal`, `ssr`).
- `--select-node <name>`: Automatically selects a node by name and frames camera on startup.
- `--select-viewport <id>`: Selects a viewport to inspect its settings.
- `--dump-frame <output.jpg>`: Renders warm-up frames and dumps the window framebuffer to disk as `.jpg` or `.png`.
- `--exit-after-dump`: Exits the application immediately after dumping the frame.

The complete list is in the [CLI reference](docs/CLI_REFERENCE.md).

---

## Building from Source

### Prerequisites

- **CMake 3.20+**
- **C++20 compliant compiler** (MSVC 2019/2022 on Windows, GCC 11+ or Clang 13+ on Linux/macOS)
- **Python 3** (with `zstandard` module: `pip install zstandard` — used by the resource packer)

### Windows (Visual Studio / MSVC)

```cmd
:: Configure CMake (Release)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

:: Build the standalone Glider executable
cmake --build build --config Release --target Glider
```

The output executable will be generated at:
```
build/bin/Release/Glider.exe
```

---

## Developer & Agent Guide

For detailed technical architecture, subsystem breakdowns, and onboarding guidelines for human contributors and AI coding agents, please refer to:
👉 [**Developer & Agent Onboarding Guide (`AGENTS.md`)**](AGENTS.md)

---

## License

Glider is licensed under the Apache 2.0 License. See the [LICENSE](LICENSE) file for details.
glTF is a trademark of the Khronos Group Inc.
