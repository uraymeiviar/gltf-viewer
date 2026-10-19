<p align="center">
  <img src="resources/Gestaltor_AppIcon_256.ico" width="96" alt="Gestaltor Logo" />
</p>

<h1 align="center">Gestaltor glTF Viewer</h1>

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

![Gestaltor Viewer](docs/images/screenshot.png)

## Overview

This fork modernizes and streamlines **Gestaltor** by stripping out the legacy, heavyweight Qt framework and replacing the entire UI layer with **Dear ImGui (Docking)** and **GLFW**. The application is redesigned as a fast, dedicated **"viewer only" / visual inspector** for glTF and GLB 3D assets.

### Key Highlights of this Fork

- **100% Standalone Single Executable**: All runtime resources—including GLSL shaders, IBL HDR environment maps (`footprint_court.hdr`), UI fonts (`fss-ui.ttf` and `fss-ui-mono.ttf`), and default models—are archived and compressed with **Zstandard (`zstd`)** and embedded directly inside `Gestaltor.exe`. No external `resources` folder, assets directory, or external DLLs are required to run.
- **Zero Qt Dependency**: Removed all Qt build pipelines, moc compilers, and Qt runtime dependencies.
- **Dear ImGui Docking UI**: Modern dockable workspace featuring 3D Viewport, Scene Hierarchy, Resource Explorer, Element Inspector, Viewport Settings, Statistics, and Console Logger.
- **FSS Aviation Dark Theme**: Aviation-inspired dark palette with emerald green accents, crisp typography, and embedded Material Design Icons (MDI).
- **Blender-Style Viewport Controls**:
  - Middle Drag: Orbit / Rotate view
  - Shift + Middle Drag: Pan view
  - Ctrl + Middle Drag / Mouse Wheel: Smooth Zoom
  - Focus (F): Instantly frame and focus camera on the selected node or active scene
  - 3D Orientation Gizmo & Compass: Clickable view axes (Front, Back, Right, Left, Top, Iso)
- **Native GPU Color Picking**:
  - Direct 3D mesh selection in viewport with sub-pixel precision.
  - Highlights selected objects using **Oriented Bounding Boxes (OBB)** matching Blender's local bounding box convention.
  - Local coordinate gizmo indicator for selected nodes.
- **Advanced glTF 2.0 PBR Rendering**:
  - Full support for glTF 2.0 Core specification and PBR metallic-roughness materials.
  - Extension support: Specular-Glossiness, Clearcoat, Sheen, Transmission, Volume, IOR, Iridescence, Material Variants, and Punctual Lights.
  - Real-time Image-Based Lighting (IBL) with spherical harmonics and environment radiance filtering.
  - Infinite fading ground grid and multiple render debug modes (Wireframe, Normal, Roughness, Metallic, Base Color, UV, etc.).
  - Window state and recent file history persistence across sessions.
- **Built-in In-Process glTF 2.0 Validator**:
  - High-performance, native C++20 validation engine running directly in-process with zero external subprocesses or tools needed.
  - **Tier 1 (JSON Schema Validation)**: Evaluates loaded assets against official Khronos glTF 2.0 and extension schemas embedded inside the executable.
  - **Tier 2 (Semantic & Buffer Integrity Checks)**: Verifies buffer view alignments, accessor strides/bounds, min/max values, geometry attribute parity, vertex index bounds, degenerate triangles, animation monotonicity, and scene hierarchy cycle detection.
  - **Interactive Validation Panel**: Filterable issues table (Errors 🔴, Warnings 🟡, Infos 🔵), text search, single-click Inspector selection, double-click 3D viewport framing (`F`), and one-click GitHub-flavored Markdown report export.
  - Viewport toolbar status pill indicator (`[ ✓ glTF Valid ]` / `[ 🔴 N Errors ]`).

---

## Download & Run

Pre-compiled standalone binaries are available on the [**Releases**](https://github.com/uraymeiviar/gltf-viewer/releases) page.

Simply extract `Gestaltor.exe` anywhere and run. No installation or setup is needed.

```bat
Gestaltor.exe [path_to_model.gltf / path_to_model.glb]
```

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

:: Build the standalone Gestaltor executable
cmake --build build --config Release --target Gestaltor
```

The output executable will be generated at:
```
build/bin/Release/Gestaltor.exe
```

---

## Developer & Agent Guide

For detailed technical architecture, subsystem breakdowns, and onboarding guidelines for human contributors and AI coding agents, please refer to:
👉 [**Developer & Agent Onboarding Guide (`AGENTS.md`)**](AGENTS.md)

---

## License

Gestaltor is licensed under the Apache 2.0 License. See the [LICENSE](LICENSE) file for details.
glTF is a trademark of the Khronos Group Inc.
