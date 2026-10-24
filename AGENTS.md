# Developer & Agent Onboarding Guide (`AGENTS.md`)

This document serves as the comprehensive technical onboarding guide and architectural reference for AI agents and human contributors working on the **Glider glTF Viewer** codebase.

---

## 1. Project Mission & Identity

- **Origin**: Forked from [UX3D Gestaltor](https://github.com/ux3d/Gestaltor) (originally a full-featured glTF 2.0 visual editor built with Qt 5/6).
- **Current Role**: A lightweight, high-performance, **viewer-only / visual inspector** for glTF and GLB 3D assets.
- **Key Tenet**: **100% Standalone Single Executable**. All runtime assets (GLSL shaders, IBL HDR environment map, UI fonts, icons, default sample models) are compressed with Zstandard and embedded directly into `Glider.exe`. The application runs with **zero external folders, assets, or runtime DLL dependencies**.
- **UI Framework**: Replaced the entire heavyweight Qt framework with **Dear ImGui (Docking branch)** and **GLFW** with OpenGL 4.1 Core Profile.

---

## 2. Directory Structure

```text
gltf-viewer/
├── header/
│   ├── imgui/               # Dear ImGui panel and helper headers
│   │   ├── AnimationPlayer.h
│   │   ├── AntiAliasing.h
│   │   ├── BoundingBoxRenderer.h
│   │   ├── ElementInspector.h
│   │   ├── EmbeddedVFS.h         # In-memory virtual file system interface
│   │   ├── Framebuffer.h         # Offscreen FBO & MSAA wrapper
│   │   ├── GladGL.h              # GL function wrapper
│   │   ├── GridRenderer.h        # Infinite fading ground grid
│   │   ├── IconsMaterialDesignIcons.h  # MDI icon glyph macros
│   │   ├── MaterialInspector.h
│   │   ├── ObjectActions.h        # Object menu actions and isolation helpers
│   │   ├── ResourcesPanel.h
│   │   ├── StatisticsPanel.h
│   │   ├── StdFileSystem.h       # IFileSystem hook for EmbeddedVFS + disk
│   │   ├── Theme.h               # FSS Aviation UI theme & font loader
│   │   ├── ViewportSettingsPanel.h
│   │   └── zstd.h                # Standalone Zstd header (linked via ktx.lib)
│   └── renderViewInput/
│       └── CameraActor.h         # Blender-style camera orbit, pan, zoom, frame focus
│
├── source/
│   ├── imgui/               # Dear ImGui panel and runtime implementations
│   │   ├── main.cpp              # Thin main/WinMain entry points
│   │   ├── application/          # Application orchestration, split by responsibility
│   │   │   ├── Application.h/.cpp # Private application state, startup, loop & shutdown
│   │   │   ├── CommandLine.cpp   # CLI parsing, self-check dispatch & startup overrides
│   │   │   ├── Frame.cpp         # Frame orchestration, keyboard input & idle tracking
│   │   │   ├── Scene.cpp         # Model loading, resource resolution & scene synchronization
│   │   │   ├── Settings.cpp      # Window state, recent files & settings paths
│   │   │   ├── Bounds.cpp        # Bounds caching, focus helpers & coordinate gizmo
│   │   │   ├── Menus.cpp         # Dock layout, menus & viewport creation
│   │   │   ├── Panels.cpp        # Hierarchy, resources, inspector & auxiliary panels
│   │   │   ├── Console.cpp       # Log storage/layout, logger UI & logger self-check
│   │   │   ├── Viewports.cpp     # Viewport rendering, overlays & mouse picking
│   │   │   └── Diagnostics.cpp   # Captures, profiling/scenario reports & UI test hooks
│   │   ├── EmbeddedResources.cpp # Generated at build time (not committed): C++ byte array of the compressed VFS
│   │   ├── EmbeddedVFS.cpp       # VFS decompression & path indexing
│   │   ├── StdFileSystem.cpp     # Hooks file queries to EmbeddedVFS with disk fallback
│   │   ├── Theme.cpp             # FSS Aviation theme colors & in-memory font loader
│   │   └── ... (panels and renderers)
│   └── renderViewInput/
│       └── CameraActor.cpp
│
├── resources/               # Centralized runtime assets (packed into EmbeddedResources.cpp)
│   ├── environments/        # default_hdr_env4k.hdr (default IBL skybox & radiance, 4096x2048)
│   ├── fonts/               # fss-ui.ttf & fss-ui-mono.ttf (embedded MDI icons)
│   ├── sampleGltf/          # HelloCube.glb (default startup model)
│   ├── shaders/             # All runtime GLSL shaders
│   │   └── ibl_sampler/     # Shaders for runtime HDR cubemap filtering
│   ├── Glider_AppIcon_1024.png # Transparent source artwork for the app icon
│   ├── Glider_AppIcon_256.ico  # Multi-size app icon: shell, window, taskbar
│   └── Icon_Application.rc  # Windows icon resource: GLFW_ICON (shell + window)
│
├── scripts/
│   ├── pack_resources.py    # Packs resources/ into compressed Zstandard archive
│   └── create_release.py    # Automates GitHub release packaging and upload
│
├── OpenGL-Renderer/         # Core PBR rendering engine (UX3D)
│   ├── renderer/            # GltfRenderLayer, CommandGenerator, IBLSampler, ShaderCache
│   └── Tools/               # gltf parser, filesystem, core utilities
│
├── legacy/                  # Archived Qt editor code & obsolete Qt assets (reference only)
│   ├── editor/              # Old Qt widgets and dialogs
│   └── resources/           # Old Qt PNG icon slots, QRC files, and stylesheets
│
├── docs/                    # Technical documentation & project assets
│   ├── images/              # README screenshots & assets
│   ├── MIGRATION_PLAN_IMGUI.md  # Architectural Qt-to-Dear-ImGui migration plan
│   ├── MSFS_GLTF_JOURNAL.md # Development journal tracking MSFS extension support
│   ├── ASOBO_EXTENSIONS_JOURNAL.md # Phased plan for rendering every ASOBO (MSFS) glTF extension (+ _REFERENCE, _PHASE_0..7)
│   └── RENDERER_OPTIMIZATION_JOURNAL.md # Comprehensive render loop audit and optimization roadmap
├── CMakeLists.txt           # Main build configuration (Qt removed, standalone ImGui)
├── AGENTS.md                # This developer/agent onboarding guide
└── README.md                # User-facing repository overview and download guide
```

---

## 3. Core Architecture & Subsystems

### 3.1 Embedded Zstandard Virtual File System (`EmbeddedVFS`)

To achieve a **100% standalone single executable**, the project avoids any external runtime disk folder requirements:

```mermaid
flowchart TD
    A["resources/ (shaders, fonts, HDR, glb)"] -->|pack_resources.py| B["EmbeddedResources.cpp (g_embeddedResourcesArchive)"]
    B -->|Compiled into| C["Gestaltor.exe"]
    C -->|main() startup| D["EmbeddedVFS::initialize()"]
    D -->|ZSTD_decompress using ktx.lib| E["In-Memory Archive Buffer (~21 MB decompressed)"]
    E -->|Index File Table| F["std::unordered_map<path, VfsFile>"]
    G["OpenGL-Renderer / ShaderCache / IBLSampler"] -->|StdFileSystem::loadBinary / loadText| F
    H["ImGui Theme"] -->|io.Fonts->AddFontFromMemoryTTF| F
```

- **Packing (`scripts/pack_resources.py`)**: Walks `resources/` (shaders, fonts, environments, sampleGltf), compiles a binary file table with offsets/lengths, and compresses the payload using Zstandard level 19 (ratio ~44%). Outputs `source/imgui/EmbeddedResources.cpp` and `resources/resources.vfs.zst`. Both are build outputs: they are git-ignored and regenerated by CMake before compilation. Only the source assets in `resources/` are committed.
- **Runtime Decompression (`source/imgui/EmbeddedVFS.cpp`)**: On startup, calls `ZSTD_getFrameContentSize()` and `ZSTD_decompress()` (provided by the linked `ktx.lib`). It indexes all virtual files under both relative (`shaders/pbr.frag`) and prefixed (`resources/shaders/pbr.frag`) paths, plus populates virtual directory entries.
- **Filesystem Interception (`source/imgui/StdFileSystem.cpp`)**: Implements `ux3d::filesystem::IFileSystem`. Methods (`isFile`, `isDirectory`, `exists`, `loadBinary`, `loadText`, `listFiles`, `listContents`) query `EmbeddedVFS` first. If an asset is found, it is served directly from memory without hitting the disk.

### 3.2 Rendering Engine (`OpenGL-Renderer`)

- **Profile**: OpenGL 4.1 Core Profile (via GLAD and GLFW).
- **Core Pipeline**:
  - `ux3d::renderer::GltfRenderLayer`: Manages the active scene, geometry buffers, skins, and reverse mapping from drawables to glTF nodes. Its traversal applies local-view isolation (one node plus optional descendants) and inherited visibility (hiding a node hides its whole subtree) without changing the glTF visibility flags.
  - `ux3d::renderer::CommandGenerator`: Manages PBR material uniforms, punctual lights, tone mapping (ACES Hill), exposure EV, and IBL parameters.
  - `ux3d::renderer::IBLSampler`: Loads the default environment `resources/environments/default_hdr_env4k.hdr` (`kDefaultEnvironmentFile` in `SceneEnvironment.h`) from memory, converts equirectangular panorama to cubemap, and computes diffuse Lambertian irradiance, specular GGX reflection cubemap mips, and Charlie sheen LUT.
  - `ux3d::imgui::GridRenderer`: Infinite fading ground plane grid with adaptive grid subdivisions based on camera altitude and zoom.
  - `ux3d::imgui::BoundingBoxRenderer`: Immediate-mode wireframe bounding box renderer supporting both axis-aligned (AABB) and oriented bounding boxes (OBB).

### 3.3 GPU Color Picking & Oriented Bounding Boxes (OBB)

- **Picking FBO**: An offscreen `GL_RGBA8` framebuffer rendered by `ux3d::renderer::PickingRenderLayer`. Each drawable mesh receives a unique 24-bit integer ID encoded as `(R << 16) | (G << 8) | B`.
- **Click Handling**: On mouse click inside the viewport, `glReadPixels(1x1)` queries the mesh ID under the cursor. `currentReverseMapping` resolves the mesh to the corresponding `ux3d::gltf::Node*`.
- **Oriented Bounding Box (OBB)**: Unlike standard AABBs which expand when an object rotates, `getNodeOBB()` in `source/imgui/application/Bounds.cpp` calculates the bounding box in the node's local coordinate space and multiplies it by the node's world transform matrix. This produces a tight, oriented orange bounding box matching Blender's selection behavior.
- **Local Coordinate Gizmo**: `drawNodeCoordinateGizmo()` renders RGB axis indicators ($X$=Red, $Y$=Green, $Z$=Blue) at the selected node's pivot position, respecting Blender coordinate convention ($Z$-up) or glTF ($Y$-up).

### 3.4 Camera Navigation (`CameraActor`)

Implemented in [`CameraActor.cpp`](file:///H:/git-repos/gltf-viewer/source/renderViewInput/CameraActor.cpp), matching standard 3D DCC tools (Blender):
- **Rotate / Orbit**: Middle Mouse Drag.
- **Pan**: Shift + Middle Mouse Drag.
- **Smooth Zoom**: Ctrl + Middle Mouse Drag or Mouse Wheel.
- **Focus Framing (`F`)**: Pressing `F` (Object → Focus, a context menu, or focusing a node in the hierarchy) calls `cameraActor.presentFocusNode()`, centering the view and setting an optimal camera distance derived from geometry bounding sphere radius.
- **Orthographic & Isometric Shortcuts**: Toolbar buttons for Front, Back, Right, Left, Top, and Iso views.

### 3.5 FSS Aviation Theme & Typography

- Defined in [`source/imgui/Theme.cpp`](file:///H:/git-repos/gltf-viewer/source/imgui/Theme.cpp):
  - **Color Palette**: Dark charcoal background (`#1A1A1E`), subtle borders, with emerald green accents (`#21AB6B`) for active states, checkmarks, sliders, and highlights.
  - **Fonts**: Loaded in-memory from `EmbeddedVFS`:
    - `fss-ui.ttf` (16px): UI interface font.
    - `fss-ui-mono.ttf` (15px): Monospace font for logs, tables, and numeric properties.
  - **Icons**: Material Design Icons (MDI) are embedded into the fonts in the unicode private use area (`0xE000` to `0xFD17`). Macros like `ICON_MDI_CUBE_OUTLINE`, `ICON_MDI_FOLDER_OPEN`, etc., are defined in [`IconsMaterialDesignIcons.h`](file:///H:/git-repos/gltf-viewer/header/imgui/IconsMaterialDesignIcons.h).

### 3.6 Docking & Layout Persistence

- Uses Dear ImGui Docking (`ImGuiID dockspaceId = ImGui::GetID("GestaltorMainDockSpace_v3")`).
- Default layout:
  - **Left**: Scene Hierarchy (top) + Resources (bottom).
  - **Center**: 3D Viewport (top) + Logger (bottom splitting viewport only).
  - **Right**: Element Inspector (full height).
  - Optional panels (Viewport Settings, Statistics) dock into the center-bottom tab bar.
- Layout reset shortcut: `Ctrl + L`.
- Window position, dimensions, maximized state, recent file history and `settings.ini` (texture search paths, panel display options) are persisted in `%APPDATA%/Glider/`. A `settings.ini` next to the executable is copied there on first use; the working directory is never read.

### 3.7 Built-in In-Process glTF 2.0 Validator

Replaces the legacy external `gltf_validator.exe` subprocess with a native, 100% in-process C++ validation engine:
- **Tier 1 (JSON Schema Validation)**: Uses `nlohmann_json_schema_validator` to validate the loaded asset against official Khronos `glTF.schema.json` and extension schemas embedded directly in `EmbeddedVFS` (`resources/schemas/`).
- **Tier 2 (Semantic & Buffer Integrity Checks)**: Written in clean standard C++20:
  - BufferView & Accessor alignment (4-byte alignment, component sizes).
  - Buffer and BufferView boundary capacity checks (`offset + length <= buffer.length`).
  - Accessor min/max consistency and component dimension checks.
  - Geometry attribute parity (count match across all attributes on a primitive).
  - Out-of-bounds vertex index checks (`index < vertex_count`).
  - Degenerate (zero-area) triangle detection.
  - Animation timeline monotonicity ($t_{i+1} > t_i$) and interpolation output count checks.
  - Scene hierarchy cycle detection (DFS loop checking) and multi-parent rule checks.
- **Interactive UI (`ValidationPanel`)**:
  - Filterable issues table with Errors (🔴), Warnings (🟡), and Infos (🔵).
  - Quick-search filter by code, JSON pointer, or message.
  - Single-click selects offending element in Element/Material Inspector; double-click frames it in the 3D viewport (`F`).
  - "Copy Report" exports a complete GitHub-flavored Markdown diagnostic report to clipboard.
  - Status bar pill badge in the Viewport Toolbar indicates asset compliance at a glance (`[ ✓ glTF Valid ]` or `[ 🔴 N Errors ]`).
  - Global shortcut: `Ctrl + 8` or **View -> Validation**.

### 3.8 Object Actions, Isolation & Context Menus

`ux3d::imgui::ObjectActions` implements the Object menu and the two object context menus:
- **Object menu items:** Focus (`F`), Hide/Unhide (`H`), Isolate (`/`), Isolate with Childs (`Shift+/`), Select Parent (`[`) and Select First Child (`]`). Items are disabled without a node selection; the last two are disabled when the node has no parent/first child. While isolated, Isolate becomes **Exit Isolate** and Isolate with Childs is hidden.
- **Context menus:** right-clicking a node row in the Scene Hierarchy shows the same items, preceded by **Select** when the row was not selected; both object context menus start with the node's name in the accent color. Right-clicking a viewport row in the hierarchy shows the Viewport menu. Right-clicking the 3D viewport with a node selected shows the six object items; with no node selected it shows the Viewport menu (the same body as the top-level Viewport menu, including New Viewport, Orientation and Shading).
- **Isolation** is a render filter in `GltfRenderLayer::traverseScene`: only the root node and, optionally, its descendants are collected (lights included), and the hierarchy list is filtered to that isolated node (+ descendants when isolating with children). It never writes node visibility flags, so Exit Isolate restores the scene exactly; loading a model clears it. The Scene Hierarchy shows an **Exit Isolate** button above the list while isolation is active.
- **Hierarchy rows:** hiding a node greys its whole subtree (the renderer inherits visibility the same way); the selected row is light green. Long labels scroll horizontally (the list has a horizontal scrollbar), and the pinned eye buttons have an opaque background and reserved space so they never overlap the label.
- The **Camera & Helpers** tab keeps two camera-preset buttons per row and edits Zoom, Rotation, Look Target, FoV and the Near/Far clipping planes with drag fields; the camera actions are disabled while attached to a glTF camera.
- **Node inspector:** the object name is read-only (renaming is disabled in the viewer). The Transform header has **Local** and **Global** tabs; Global shows world-space translation/rotation/scale and converts edits back into local TRS through the parent's inverse world transform.
- **File → Close** reloads the embedded default sample model, resetting the application to its initial scene.

### 3.9 Animation Menu

`Animation` (right of Object) exposes the playback controls that the status bar also offers:
- **Play / Stop** (`Alt+A`) toggles playback; the label follows the player state.
- **Rewind** (`Shift+Left`) seeks to 0 and **FForward** (`Shift+Right`) seeks to the clip end; both reset TAA history.
- **Active** opens a submenu with the same checklist as the status-bar animation popup — `AnimationPlayer::renderActiveAnimationsContent` is shared by both, including Select all non-conflicting and the Skinning/Morphing settings.
- All items are disabled when the model has no animations; the shortcuts are ignored while typing or while a menu/popup is open.


---

## 4. Build & Development Workflows

### 4.1 Building from Source (Windows)

Prerequisites:
- Visual Studio 2019/2022 (MSVC with C++20 support)
- CMake 3.20+
- Python 3 with `pip install zstandard`

```cmd
:: 1. Configure build directory
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

:: 2. Compile standalone Glider.exe
cmake --build build --config Release --target Glider
```

Output binary:
```
build/bin/Release/Glider.exe
```

### 4.2 Command-Line Arguments & Automation

`Glider.exe` supports several CLI flags useful for testing and headless image verification:
- `[model.gltf / model.glb]`: Path to glTF or GLB file to open on startup (defaults to embedded `HelloCube.glb`).
- `--aa <none|msaa2|msaa4|msaa8|fxaa|taa>`: Set anti-aliasing mode on startup.
- `--select-node <name>`: Automatically selects a node by name and frames camera on startup.
- `--select-viewport <id>`: Selects a viewport to inspect its settings.
- `--dump-frame <output.jpg>`: Renders 8 warm-up frames and dumps the window framebuffer to disk as `.jpg` or `.png`.
- `--exit-after-dump`: Exits the application immediately after dumping the frame.
- `--quality <high|balanced|performance>`: Render-quality preset for this run (default High, the original quality). Finer overrides: `--ibl-size`, `--ibl-irradiance-size`, `--ibl-lut-size`, `--ibl-precision <32|16>`, `--ibl-samples`, `--grid-mode <ray|plane>`, `--render-scale`. The UI equivalents are **Viewport Settings → Quality** (preset, ground grid, render scale) and **Scene Inspector → IBL Filter Settings** (the IBL filter sizes and samples, shown while IBL is enabled); both are renderer-wide and saved in `%APPDATA%/Glider/render_quality.txt`.
- `--hdr-rotation <deg>`, `--exposure <ev>`, `--tone-mapping <neutral|hill|narkowicz>`: Environment rotation,
  exposure and tone mapping for this run. Environment settings belong to the glTF scene (one set per scene, shared
  by every viewport) and are edited in the Inspector when the scene row is selected (`SceneInspector`).
- `--show-invisible-materials`: Also draw materials marked invisible by `ASOBO_material_invisible` (hidden by default).
- `--asobo off|on`, `--asobo-disable <extension>` (repeatable): MSFS (ASOBO) extensions the active viewport applies (Viewport Settings → MSFS Extensions; all on by default; off renders the file as if it had none).
- `--search-path <dir>` (repeatable): extra texture folder for this run, relative to the model; works with `--isolated`, which ignores `settings.ini`.
- `--validate-file <gltf> --validation-report <out.json>`: validate headlessly and exit; files the loader accepts are validated with their model, so the model-based checks (including ASOBO) run.
- `--asobo-report <out.json>`: After loading (and any `--reload-model`), write the typed ASOBO extension model and its diagnostics, then exit (`scripts/validate_asobo_model.py`). The model lives in `gltf/extensions/AsoboExtensions.h`; its table is generated by `scripts/generate_asobo_specs.py` from `resources/schemas/asobo` plus the exporter data in `docs/ASOBO_EXTENSIONS_SEMANTICS.md`.
- `--disable-shared-traversal`: Every viewport traverses the scene itself (A/B reference).
- `--disable-pick-culling`, `--disable-instancing`: Same-binary A/B references for the Phase 8 hybrid picking and instancing (see `docs/RENDERER_OPTIMIZATION_PHASE_8.md`).

Example test command:
```cmd
cmd /c "build\bin\Release\Glider.exe --dump-frame frame.jpg --exit-after-dump"
```
*(Note: Because `Glider.exe` is a Win32 subsystem executable, wrap in `cmd /c` when invoking from PowerShell to wait for process exit synchronously).*

### 4.3 Updating Runtime Resources

Edit, add or remove files under `resources/` (shaders, fonts, environments, sampleGltf, schemas, icons), then rebuild:
```cmd
cmake --build build --config Release --target Glider
```
CMake reruns `scripts/pack_resources.py` before compiling whenever a packed resource or the packer changes, and the generated `EmbeddedResources.cpp` is never committed. Python 3 with `pip install zstandard` is required at build time. New files are picked up through `CONFIGURE_DEPENDS` globbing. The default environment's file name is `kDefaultEnvironmentFile` in `header/imgui/SceneEnvironment.h`. Keep it at 4096×2048 or below: the renderer resamples it to 1024-pixel cube faces, and the generated hex array grows by about 6 bytes of source per compressed byte.

### 4.4 Publishing GitHub Releases

The script [`scripts/create_release.py`](file:///H:/git-repos/gltf-viewer/scripts/create_release.py) automatically packages `Glider.exe`, `README.md`, and `LICENSE` into a `.zip` archive, checks authentication via Windows Git Credential Manager, and uploads the release asset to GitHub:
```cmd
python scripts/create_release.py
```

---

### 4.5 Regression Testing

After each commit, build it and save its binary as the baseline. Then check later uncommitted changes against it:

```cmd
python scripts/save_baseline.py
python scripts/run_regression.py
python scripts/run_regression.py --tier full
```

- **`save_baseline.py`** writes `build/baselines/<short sha>.exe`. It refuses when tracked files differ from the
  commit.
- **`run_regression.py`** (quick tier, about 2–3 minutes) runs the renderer harnesses
  (`scripts/validate_p8_*.py`, `validate_p4_exit_gate.py`, `validate_asobo_model.py`, `validate_asobo_common.py`) and the built-in self-checks against that baseline, three
  at a time. They check byte-identical captures and identical selections.
- **`--tier full`** adds each feature's same-binary A/B switch and `validate_p5_1.py`.
- **Benchmarks** never run here. They back timing claims only: pass `--bench` to the harness concerned, and do not
  run them in parallel.

## 5. Critical Rules & Guardrails for AI Agents

1. **Standalone Integrity**:
   - Never add code that assumes `resources/` exists on disk adjacent to `Glider.exe`.
   - All runtime resources must be accessed via `ux3d::imgui::StdFileSystem::instance()` or `EmbeddedVFS::instance()`.
2. **Never Commit or Push Without Instruction**:
   - Only execute `git commit` or `git push` when the user explicitly instructs you to do so.
3. **No Heavy Framework Dependencies**:
   - Do not reintroduce Qt, Intel TBB, or other heavy frameworks. Use modern C++20 standard libraries (`std::thread`, `std::filesystem`, `std::ranges`, etc.) and lightweight C/C++ libraries.
4. **Preserve Legacy Code**:
   - Do not delete old Qt editor widgets in `legacy/`; they serve as design reference for future feature migrations into Dear ImGui panels.
5. **Always Test Visuals with Frame Dumps**:
   - After major viewport, camera, or renderer changes, verify visual correctness by dumping a frame (`--dump-frame`) and viewing the image file before concluding tasks.
