# Architecture & Subsystems (Glider)

Moved from AGENTS.md; original section numbers are retained. Start with [AGENTS.md](../AGENTS.md). For setting ownership and compatibility keys use [SETTINGS_SCOPES.md](SETTINGS_SCOPES.md); follow the linked feature phase docs for implementation history and acceptance evidence.

## 1. Project Mission & Identity

- **Origin**: Forked from [UX3D Gestaltor](https://github.com/ux3d/Gestaltor) (originally a full-featured glTF 2.0 visual editor built with Qt 5/6).
- **Current Role**: A lightweight, high-performance, **viewer-only / visual inspector** for glTF and GLB 3D assets.
- **Key Tenet**: **100% Standalone Single Executable**. All runtime assets (GLSL shaders, IBL HDR environment map, UI fonts, icons, default sample models) are compressed with Zstandard and embedded directly into `Glider.exe`. The application runs with **zero external folders, assets, or runtime DLL dependencies**.
- **UI Framework**: Replaced the entire heavyweight Qt framework with **Dear ImGui (Docking branch)** and **GLFW** with an OpenGL 4.6 Core Profile context (falls back to 4.5, 4.3, then 4.1).

---

## 2. Directory Structure

```text
gltf-viewer/
├── header/
│   ├── imgui/               # Dear ImGui panel and helper headers
│   │   ├── AnimationPlayer.h
│   │   ├── AntiAliasing.h
│   │   ├── BoundingBoxRenderer.h
│   │   ├── CaptureSettings.h      # Viewport capture folder setting, file naming, Settings → Captures tab
│   │   ├── ElementInspector.h
│   │   ├── EmbeddedVFS.h         # In-memory virtual file system interface
│   │   ├── Framebuffer.h         # Offscreen FBO & MSAA wrapper
│   │   ├── GladGL.h              # GL function wrapper
│   │   ├── GltfExtensionSettings.h # Global glTF extension switches (app setting)
│   │   ├── GridRenderer.h        # Infinite fading ground grid
│   │   ├── GroundSettings.h      # A3.6 ground choice (grid / plane), height modes, material presets
│   │   ├── IconsMaterialDesignIcons.h  # MDI icon glyph macros
│   │   ├── MaterialInspector.h
│   │   ├── ObjectActions.h        # Object menu actions and isolation helpers
│   │   ├── ResourcesPanel.h
│   │   ├── SettingsScopes.h       # Settings scopes: SceneEffectSettings, EffectOverride (Scene / Off / On)
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
│   │   │   ├── CommandLine.cpp   # CLI parsing (Application::configure), self-check dispatch & startup overrides
│   │   │   ├── CommandLineHelpers.cpp # Argument parsers, node lookup by name/#index, --asobo-report
│   │   │   ├── Commands.cpp      # Agent channel core: shared actions, script/pipe dispatch, control pipe, --agent-guide
│   │   │   ├── CommandsInternal.h # Private: includes and file-local helpers shared by Commands*.cpp
│   │   │   ├── CommandsHandlers.cpp # registerAgentCommands: every registry command
│   │   │   ├── CommandsSettings.cpp # registerAgentSettings (calls the areas in order), scene effects, compatibility keys
│   │   │   ├── CommandsSettingsQuality.cpp # Renderer-wide quality keys (AA, effect quality, IBL, scale, A3.2 shadows)
│   │   │   ├── CommandsSettingsViewport.cpp # Viewport keys plus scene ground and compatibility forwarding
│   │   │   ├── CommandsSettingsEnvironment.cpp # Scene environment keys (HDR, exposure, sun, sky, time of day, night)
│   │   │   ├── CommandsState.cpp # get_state / profile JSON: session, quality, effects, SSAO, ambient, sun
│   │   │   ├── CommandsProbe.cpp # probe / probe_region readback
│   │   │   ├── EnvironmentState.cpp # Effective sun, moon, exposure plan, night state, procedural sky, time-of-day clock
│   │   │   ├── Frame.cpp         # Frame orchestration, keyboard input & idle tracking
│   │   │   ├── Scene.cpp         # Model loading, resource resolution & scene synchronization
│   │   │   ├── Settings.cpp      # Window state, recent files & settings paths
│   │   │   ├── Bounds.cpp        # Bounds caching, focus helpers & coordinate gizmo
│   │   │   ├── Menus.cpp         # Dock layout, menus & viewport creation
│   │   │   ├── Panels.cpp        # Hierarchy, resources, inspector & auxiliary panels
│   │   │   ├── Console.cpp       # Log storage/layout, logger UI & logger self-check
│   │   │   ├── Ocean.cpp         # A6.8 ocean ground: clock, material values of the draw, get_state object
│   │   │   ├── Viewports.cpp     # drawViewports: viewport windows, scene render, overlays, compass & mouse picking
│   │   │   ├── ViewportToolbar.cpp # Viewport toolbar strip (projection, grid, bbox, turntable, variants, camera, shading)
│   │   │   ├── ViewportEffects.cpp # A2.1 decal buffer, A3.2 sun shadow cascades, A3.9 sky-occlusion volume (+ state JSON)
│   │   │   ├── Diagnostics.cpp   # Self-check reports, decal prepass check, capture/hash helpers, report naming, GPU report
│   │   │   ├── DiagnosticsFrame.cpp # finishFrame (dumps, captures, profile/benchmark reports, agent pump) & scenario report
│   │   │   ├── DiagnosticsPicking.cpp # Camera JSON, node screen rectangles (--report-node), pick at pixel
│   │   │   └── ViewportImage.cpp # Viewport menu capture / save / copy (PNG file, clipboard)
│   │   ├── AntiAliasing.cpp      # Per-viewport AA: targets, TAA jitter/history, FXAA/TAA passes
│   │   ├── AntiAliasingBloom.cpp # Bloom, A6.7 lens flare, auto-exposure meter, tone-mapping composite
│   │   ├── AntiAliasingSelfCheck.cpp # AA render-target allocation self-check
│   │   ├── AntiAliasingShaders.h # Private: quad + GLSL sources of the post passes, compile/link helpers
│   │   ├── AmbientOcclusion.cpp  # A3.5 GTAO pass (class in header/imgui/AmbientOcclusion.h)
│   │   ├── ElementInspector.cpp  # ElementInspector::render (node, mesh, texture, accessor, ... views)
│   │   ├── ElementInspectorImages.h/.cpp # Image preview cache, header details, normal-map detection, preview widget
│   │   ├── SceneHierarchyPanel.cpp # Hierarchy panel state, label/visibility/search cache, row model
│   │   ├── SceneHierarchyInternal.h # Private: row/cache types and shared panel state of SceneHierarchy*.cpp
│   │   ├── SceneHierarchyRender.cpp # Hierarchy UI: eye buttons, rows, SceneHierarchyPanel::render
│   │   ├── SceneHierarchySelfCheck.cpp # Hierarchy cache-invalidation self-check
│   │   ├── SkyEnvironment.cpp    # A6.2/A6.3 procedural sky LUTs & time-sliced IBL (class in header/imgui/SkyEnvironment.h)
│   │   ├── SkyOcclusion.cpp      # A3.9 sky-visibility volume bake (class in header/imgui/SkyOcclusion.h)
│   │   ├── UiScenarioHarness.cpp # Scripted UI scenario harness (class in header/imgui/UiScenarioHarness.h)
│   │   ├── EmbeddedResources.cpp # Generated at build time (not committed): C++ byte array of the compressed VFS
│   │   ├── EmbeddedVFS.cpp       # VFS decompression & path indexing
│   │   ├── GltfExtensionSettings.cpp # Global glTF extension switches, vendor groups, persistence, Settings tab
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
│   │   └── source/gltf/     # GltfRenderLayer.cpp (construction, sync entry points, environment, cameras, lights),
│   │                        # GltfRenderLayerMaterials.cpp (material mapping), GltfRenderLayerMeshes.cpp (meshes,
│   │                        # MikkTSpace tangents), GltfRenderLayerNodes.cpp (transform hierarchy, skins, animations),
│   │                        # GltfRenderLayerTraversal.cpp (traversal, culling, renderer scene), GltfRenderLayerInternal.h
│   └── Tools/               # gltf parser, filesystem, core utilities
│
├── legacy/                  # Archived Qt editor code & obsolete Qt assets (reference only)
│   ├── editor/              # Old Qt widgets and dialogs
│   └── resources/           # Old Qt PNG icon slots, QRC files, and stylesheets
│
├── docs/                    # Technical documentation & project assets
│   ├── images/              # README screenshots & assets
│   ├── MIGRATION_PLAN_IMGUI.md  # Architectural Qt-to-Dear-ImGui migration plan
│   ├── SETTINGS_SCOPES.md   # Which settings are global quality, scene effects or viewport view (and their keys)
│   ├── MSFS_GLTF_JOURNAL.md # Development journal tracking MSFS extension support
│   ├── ASOBO_EXTENSIONS_JOURNAL.md # Phased plan for rendering every ASOBO (MSFS) glTF extension (+ _REFERENCE, _PHASE_0..7)
│   └── RENDERER_OPTIMIZATION_JOURNAL.md # Comprehensive render loop audit and optimization roadmap
├── CMakeLists.txt           # Main build configuration (Qt removed, standalone ImGui)
├── AGENTS.md                # This developer/agent onboarding guide
└── README.md                # User-facing repository overview and download guide
```

---

## 3. Core Architecture & Subsystems

> **Flagged legacy fact:** the VFS diagram below still labels the binary `Gestaltor.exe`. The current output is `Glider.exe` (`CMakeLists.txt:12`). The original diagram label is retained here to make the stale claim explicit during this documentation move.

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

> **Flagged legacy fact:** the profile paragraph below says every existing shader stays at `#version 410`. Ocean compute shaders are compiled with `#version 430` (`source/imgui/OceanSimulation.cpp:110`); the 4.1-compatible graphics path and the optional 4.3+ compute path must be distinguished. The original claim is retained for review, not as a guarantee about compute shaders.

### 3.2 Rendering Engine (`OpenGL-Renderer`)

- **Profile**: OpenGL 4.6 Core Profile (via GLAD and GLFW). `Application::run` requests 4.6 and, if context creation fails, 4.5, then 4.3, then 4.1, and logs which version it obtained. Compute shaders, SSBOs and image load/store need 4.3+; query `ux3d::imgui::glCaps()` (`header/imgui/GlCaps.h`: `major`, `minor`, `compute`, `maxComputeWorkGroupInvocations`) before using them. The profile and benchmark reports carry the same values in their `gl` object. The vendored loader in `thirdparty/glad` is glad 0.1.36 generated for `gl=4.6` core with all extensions (command in `thirdparty/glad/README.md`). Every existing shader (and the ImGui backend) deliberately stays at `#version 410`, so the 4.6 context changes no rendered pixel (`scripts/validate_gl46.py` compares captures byte for byte with the baseline).
- **Core Pipeline**:
  - `ux3d::renderer::GltfRenderLayer`: Manages the active scene, geometry buffers, skins, and reverse mapping from drawables to glTF nodes. Its traversal applies local-view isolation (one node plus optional descendants) and inherited visibility (hiding a node hides its whole subtree) without changing the glTF visibility flags. All scene materials and the background skybox render in linear HDR space (`LINEAR_OUTPUT=1`) into a multisample-capable `GL_RGBA16F` target (`vp->sceneFbo`).
  - `ux3d::renderer::CommandGenerator`: Manages PBR material uniforms, punctual lights, IBL parameters, and pass bucketing (`DecalPrepass`, `DecalAccumulation`, `Opaque`, `Grid`, `Transparent2`, `TransmissiveBlit`, `Transmissive`, `Transparent`). The glass refraction copy (`TransmissionTarget`, `GL_RGBA16F` with regenerated mip chain) and windshield SSR capture directly from `vp->sceneFbo` without duplicate linear material passes.
  - `ux3d::imgui::AntiAliasing`: Executes the per-viewport HDR and LDR post-processing chain:
    `scene (HDR, MSAA) -> resolve -> TAA in HDR (reversible Karis luminance weighting c/(1+luma) before neighborhood clamp/blend and c/(1-luma) after) -> auto-exposure metering (64x64 centre-weighted log-average luminance with outlier rejection; read back asynchronously interactively, synchronously in capture runs) -> physically based bloom (Jimenez 2014 13-tap Karis downsample + 3x3 tent additive upsample) -> post tone mapping (exposure * Khronos PBR Neutral / ACES Hill / ACES Narkowicz + linear-to-sRGB) -> FXAA (LDR) -> unjittered overlays & wireframe (vp->overlayFbo) -> ImGui`. Debug Render Modes (`--debug-view`) bypass bloom and tone mapping. Both Wireframe shading mode and Wireframe Overlay draw lines after tone mapping into the LDR `vp->overlayFbo` without feeding bloom, auto-exposure, or TAA history.
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
- **Settings scopes** (`docs/SETTINGS_SCOPES.md`): global Quality owns effect levels (SSR / SSAO off-low-mid-high,
  shadow map and IBL cube off-size), AA, filter / shadow tuning, sky occlusion, ocean mode / animation, bloom,
  lens mode / intensity, lights and texture cap; every editable preset carries these and Auto adapts them.
  File > Settings > Quality and the status-bar preset combo expose it. Scene (`SceneEnvironmentState`) owns
  appearance, background and ground / ocean parameters, with one ocean simulation and clock shared by its
  viewports. Viewport owns camera, overlays and render mode: Shaded + post effects (default), Shaded without
  post AA / bloom / lens / SSR / SSAO, and debug views. Its toolbar gear selects the viewport in the Inspector.
  The grid overlays the scene ground height (Y=0 without ground). Deprecated viewport effect overrides are warned
  no-ops. `GltfExtensionSettings` owns switches plus ASOBO interior sky reflection and decal tolerance, saved in
  `[GltfExtensions]` with independent effective / saved values. Old-scope registry keys forward to their new owners.
  Resolved viewport mirrors keep renderer reads centralized in `Application::resolveAllViewportSettings`.
- **Single Portable Settings File**: Every persisted preference lives in one `settings.ini` next to `Glider.exe`: window state `[Window]`, recent files `[RecentFiles]`, search paths `[SearchPaths]`, hierarchy/resource display `[SceneHierarchy]`/`[Resources]`, render quality `[RenderQuality]`, the glTF extension switches `[GltfExtensions]` (one key per switched-off extension name, value `0`; `ASOBO=0` is the group's master switch; see `docs/SETTINGS_SCOPES.md`), the viewport capture folder `[Captures]` (`folder`, stored only when set; see §3.8), the four quality presets `[QualityProfiles]` (`profile0.*` … `profile3.*`, `selection`; 0 = Auto, 1–4 = profiles; besides AA, SSR quality and the IBL / shadow fields also `ssaoQuality`, `ssaoResolution`, `skyOcclusionQuality`, `oceanQuality`) and the ImGui docking layout `[Layout]`. No settings folder (no `GliderData/`) is created next to the executable, so `Glider.exe` still runs as a single file.
  - **Only Non-Default Values**: writers compare every value with its built-in default and skip equal ones, empty sections are not serialized, and a pristine run creates no file at all. `SettingsFile::save()` compares the serialized content and never rewrites an unchanged file; a file whose values all match the defaults is removed.
  - **Fallback**: If the executable directory is not writable (e.g. under Program Files), Glider writes `%APPDATA%/Glider/settings.ini` (or `%TEMP%/Glider/settings.ini`), logging the fallback once. If every location is unwritable, changes are kept in memory for the session without crashing.
  - **One-Time Migration**: On first launch without a new-format file, existing state from `<exeDir>/GliderData/` (or legacy `<exeDir>/settings.ini`, `%APPDATA%/Glider/`, `%APPDATA%/Gestaltor/`) is merged in memory: list sections (`[SearchPaths]`, `[RecentFiles]`, `[Layout]`, `[QualityProfiles]`) come from the newest source as a whole, scalar sections fill only missing keys. The first save writes the consolidated file with a `; Glider settings file (schema 1)` marker; once that file exists, legacy files are never consulted again, so values the user resets to their defaults are not resurrected.
  - **Robust Non-Throwing I/O**: All filesystem operations use non-throwing `std::filesystem` overloads with `std::error_code` and explicit stream checking. Unreadable or corrupt files fall back to defaults with a single warning line; an unreadable file is never overwritten (changes stay in memory); unwritable files/directories keep modified state in memory with a single warning line (never spamming every frame).
  - **Test Hook**: Environment variable `GLIDER_DATA_DIR=<dir>` points the store at `<dir>/settings.ini` and disables legacy migration, keeping test runs away from user state.
  - **`--isolated`**: Disables reading or writing any persistent settings to disk (leaves user settings completely untouched).

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
  - External resource URI portability checks (warnings for absolute paths, infos for external URLs).
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
- **Viewport image items:** the Viewport menu body ends with three items that act on the viewport the menu belongs to (disabled while a model is loading or the viewport has no image yet; no shortcuts). They share one path with the `capture` command and `--capture-viewport` (`Application::readTexturePixels`, `captureViewportTexture`, `saveImage` in `Diagnostics.cpp`; `source/imgui/application/ViewportImage.cpp`), so all routes produce identical pixels for the same frame: the final displayed image after tone mapping and AA with the in-viewport overlays drawn into the viewport texture (grid, wireframe, selection and scene boxes), at the viewport's pixel size, without ImGui chrome (the orientation compass and the node coordinate gizmo are ImGui draw-list overlays and are not part of the image).
  - **Capture Viewport** saves a PNG without a dialog as `Glider_<model stem>_<yyyyMMdd-HHmmss>.png` (`_2`, `_3`, ... if it exists) in the capture folder, created on demand, and logs one `[Capture]` line with the full path (an error line if the folder cannot be created or written; nothing is written elsewhere).
  - **Save Viewport View...** opens the native Save As dialog (PNG filter, the proposed name above, starting in the capture folder) and writes the PNG there; a name without `.png` gets it appended. Scripted runs answer the dialog through `GLIDER_TEST_SAVE_FILE` (empty cancels), like `GLIDER_TEST_OPEN_FILE` for File → Open.
  - **Copy Viewport View** puts a 24-bit bottom-up `CF_DIB` on the Windows clipboard (alpha dropped) plus the registered `PNG` format (full quality, alpha kept) and logs one line.
  - **Capture folder:** **File → Settings → Captures** edits it (text field, Browse..., Reset to default; the default `<Pictures>\Glider` from `SHGetKnownFolderPath(FOLDERID_Pictures)` is shown). It is stored in `settings.ini` as `[Captures] folder=` only when it differs from the default (`ux3d::imgui::CaptureSettings`); an unset value means the default. `--isolated` never reads or writes it: edits stay in memory for the session. `get_state` reports `captures.default/configured/effective`. `scripts/validate_viewport_image.py` covers it.
- **Isolation** is a render filter in `GltfRenderLayer::traverseScene`: only the root node and, optionally, its descendants are collected (lights included), and the hierarchy list is filtered to that isolated node (+ descendants when isolating with children). It never writes node visibility flags, so Exit Isolate restores the scene exactly; loading a model clears it. The Scene Hierarchy shows an **Exit Isolate** button above the list while isolation is active.
- **Hierarchy rows:** hiding a node greys its whole subtree (the renderer inherits visibility the same way); the selected row is light green. Long labels scroll horizontally (the list has a horizontal scrollbar), and the pinned eye buttons have an opaque background and reserved space so they never overlap the label.
- The **Camera & Helpers** tab keeps two camera-preset buttons per row and edits Zoom, Rotation, Look Target, FoV and the Near/Far clipping planes with drag fields; the camera actions are disabled while attached to a glTF camera.
- **Node inspector:** the object name is read-only (renaming is disabled in the viewer). The Transform header has **Local** and **Global** tabs; Global shows world-space translation/rotation/scale and converts edits back into local TRS through the parent's inverse world transform.
- **Material inspector texture slots:** instead of a texture combo, an assigned slot shows a clickable 64x64 preview and texture-name button. Clicking either opens the Texture inspector with a **Back to Material** button at the top. The **TEXCOORD_n** button opens the matching Accessor inspector, which lists the primitives using that accessor and also offers **Back to Material**.
- **Extension inspector:** every material parameter is editable in memory; nothing is written to disk. Extensions are never added or removed: only extensions already present on a material are listed. Rendered Khronos material extensions have a per-material, per-viewport display switch (default on); unsupported extensions have no switch. Switching off affects only rendering and leaves the loaded extension and its parameters intact. Typed ASOBO parameters keep their behaviour; the per-extension ASOBO checkbox edits the global app switch (File → Settings → glTF Extensions), not a viewport. Existing non-ASOBO vendor values use numeric drags, checkboxes, colour/number arrays and schema enum combos; strings without an enum remain read-only, and keys cannot be added or removed. Vendor edits go through `ExtensionInspector::setExtensionParameter` and mark `SceneChange::Extension` plus `Material` for material extensions. Display switches are session state and are not persisted.
- **File → Close** reloads the embedded default sample model, resetting the application to its initial scene.
- **Wireframe Shading & Overlay:** Wire lines for both **Wireframe** shading mode and the **Wireframe Overlay** are rendered into the LDR `vp->overlayFbo` after tone mapping and post-AA using unjittered camera projection. Lines never feed bloom, auto-exposure or TAA history, avoiding blown-out white haze on bright skies. Depth testing against the scene depth preserves occlusion (hidden lines remain hidden). Per-viewport settings in **Viewport Settings → Wireframe** configure line colour (RGB), opacity, and blend modes:
  - **Normal**: standard alpha blending (`glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`).
  - **Additive**: additive glow blending (`glBlendFunc(GL_SRC_ALPHA, GL_ONE)`).
  - **Invert**: contrast inversion (`glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA)` yielding `mix(dst, 1 - dst, alpha)`), visible on any background.
  - **XOR**: bitwise logic operation (`glEnable(GL_COLOR_LOGIC_OP); glLogicOp(GL_XOR)`), protected with `glColorMask` to prevent target alpha corruption.
  Materials are cached per viewport in `vp->wireMaterials`, ensuring 0 material copies per frame after frame 1.

### 3.9 Animation Menu

`Animation` (right of Object) exposes the playback controls that the status bar also offers:
- **Play / Stop** (`Alt+A`) toggles playback; the label follows the player state.
- **Rewind** (`Shift+Left`) seeks to 0 and **FForward** (`Shift+Right`) seeks to the clip end; both reset TAA history.
- **Active** opens a submenu with the same checklist as the status-bar animation popup — `AnimationPlayer::renderActiveAnimationsContent` is shared by both, including Select all non-conflicting and the Skinning/Morphing settings.
- All items are disabled when the model has no animations; the shortcuts are ignored while typing or while a menu/popup is open.

### 3.10 Ground Plane (A3.6)

**Scene Inspector → Ground** chooses **Off** (default), **Plane**, or **Ocean** (OpenGL 4.3+). The
`SceneEnvironmentState::ground` surface is shared by all views; `ViewportSettingsState::ground` is a resolved
mirror. The independent **Show ground grid** viewport overlay draws at that surface's height, or Y=0 without it.
- **Plane:** `GltfResourceManager::getGroundTuple()` is an Opaque-pass drawable (a screen quad, `ground.vert` / `ground.frag`) that `Viewports.cpp` appends to the scene while the viewport shows the plane in a shaded view (debug views and wireframe keep the sky). The fragment shader intersects each pixel's ray with the plane `y = h` (the `GridRenderer` ray path), writes `gl_FragDepth` (1 beyond the far plane; the background draws first in the Opaque pass while a ground surface is drawn, `Material::drawFirstInPass`, so the plane and the ocean continue past the far plane to the horizon), shades a dielectric (F0 0.04) with albedo, roughness, optional procedural detail (four noise octaves and slab joints that fade to their mean with the pixel footprint) from the IBL (`getIBLRadianceLambertian` / `GGX`, the HDR's own lower hemisphere) and the A3.1 sun with `sunShadowVisibility` (A3.2), and fogs towards the environment's horizon (`1 - exp(-k d)`, 98% at the haze distance, automatic 0.9 x far plane but at least 100 x the camera height above the ground, `GroundSettings::hazeVisibility`). Height modes: **Model base** (`Application::getCachedModelBaseHeight`: lowest world Y of the POSITION boxes of the visible nodes' static pose, cached per scene generation and Transform/Visibility/Material revisions), **Y = 0**, **Custom**.
- Not pickable: the drawable is removed from the scene before the picking passes and never enters the overlay depth, so clicks fall through to the background. **Show ground grid** draws the overlay at the scene surface height (the overlay depth is then rebuilt from the opaque model only).
- The shadow cascades also cover the plane and the model's footprint on it (`updateSunShadows` `groundHeight`).
- `ViewportInstance::groundAlbedo` / `groundPlaneActive` / `groundPlaneHeight` hold the last render's ground; A6.2 reads the albedo as its ground albedo.
- **Ocean (A6.8; `docs/ASOBO_EXTENSIONS_PHASE_6.md` A6.8):** a Tessendorf FFT sea (`header/imgui/OceanSimulation.h`, `source/imgui/OceanSimulation.cpp`, `resources/shaders/ocean*.{comp,vert,frag}`, `source/imgui/application/Ocean.cpp`). JONSWAP spectrum with the Horvath 2015 directional spread, 2 / 3 / 4 cascades (Low / Medium / High) of different patch sizes whose wave-number bands tile the spectrum, h0 on the CPU (hashed Box-Muller, deterministic). **Looping** (default): every cascade's dispersion is quantised to a multiple of 2 pi m / T (loop period T, default 30 s), F frames (32 / 48 / 64) are computed once with GL 4.3 compute shaders (spectrum, row and column FFT, displacement + Jacobian, foam persistence over two loop turns, compute mip chain) into a `GL_RGBA16F` array (12 / 101 / 176 MiB), rendering interpolates the two nearest frames; a setting change recomputes in the background (timestamp-timed slices within 3 ms of GPU per frame) and swaps when complete. **Live**: one layer per cascade (2 x 128^2, 3 x 256^2, 4 x 512^2) recomputed every frame at the exact dispersion (the phase in double precision), foam accumulating frame to frame; a mode or setting change builds the set at once (about 3 / 13 / 120 ms of CPU for the spectrum); measured GPU cost 0.05 / 0.08 / 0.24 ms per frame (`get_state` `ground.ocean.live`). Time: a per-scene clock advanced once by the frame time while Quality ocean animation plays; capture runs move it only by `--fixed-frame-time` steps; `--ocean-time` pins it.
  - **Mesh and shading:** a camera-centred polar grid (256 x 192, rings growing with distance, reaching 2000 camera heights, at least 100 km) displaced in the vertex shader (one mip coarser than the ring spacing, so waves shorter than four vertices stay out of the geometry and do not swim when the camera moves), sea level = the Height mode above, Opaque pass in linear HDR, writes its depth (hulls and floats intersect the waves). Per-pixel normals from the displaced field at the footprint's mip; the slope variance the mips and the finest cascade lose is added to the GGX roughness (Cox-Munk total). Fresnel (water IOR 1.33, F0 0.02) between the sky / IBL reflection (a mirrored lower hemisphere above the horizon) and the water body (colour x sky diffuse + the sun-driven part, lighter at crests), the A3.1 sun's GGX highlight with the A3.2 shadow visibility (a shadow removes the sun's direct specular and two thirds of the sun-driven body light; the sky reflection and sky-lit body stay), the plane's horizon fog. Only sky / IBL reflections: there is no ocean SSR. Not pickable (removed before picking, never in the overlay depth); shaded views only, like the plane. TAA: the plane's reprojection (no motion vectors).
  - **Foam:** whitecaps where the summed displacement Jacobian J = 1 + sum of the cascades' (J_c - 1) is low; the stored J persists (min(J, J_prev + 0.6 / s x dt)) so foam fades after a fold. The threshold is chosen so the whitecap fraction follows Monahan and O'Muircheartaigh (1980), W = 3.84e-6 U^3.41 (0.02% at 3 m/s, 1% at 10, 10% at 20), at foam amount 0.5 (the amount scales the coverage with its square), at every distance (the Jacobian's spread narrows with the slope variance the footprint resolves); a noise fixed to the undisplaced water coordinate gives it a lacy edge.
  - **Model interiors:** with the A3.9 sky-visibility volume (the default with the procedural sky) water where less than 5% of the sky is visible is discarded; without it, while the camera is inside the model's bounds, water inside those bounds is discarded (limit: from a cockpit the water under the wings is hidden too). Its average albedo (0.066 + the water colour) is the ground albedo A6.2 reads; the shadow cascades cover it like the plane.
  - **Settings** (Viewport Settings, Camera & Helpers, Ground; shown when Ocean is chosen, all per viewport, never persisted): Mode (Looping / Live), Quality (read-only here: the simulation quality is renderer-wide, File → Settings → Quality → Current Settings → Effects, registry `ocean_quality`), Wind speed, Wind from, Choppiness, Swell, Foam amount, Water colour, Loop period, Fetch, Animate, Reset. Wind direction is the direction the wind comes from, in the sun's azimuth convention (around +Y from +Z towards +X); the waves travel the other way. The controls carry `UiProbe` tags (`vp.ocean.*`, `vp.ground.*`, `vp.tab.camera`) for `--ui-script`.


---
