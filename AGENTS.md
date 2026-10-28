# Developer & Agent Onboarding Guide (`AGENTS.md`)

This document serves as the comprehensive technical onboarding guide and architectural reference for AI agents and human contributors working on the **Glider glTF Viewer** codebase.

---

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
│   │   ├── GridRenderer.h        # Infinite fading ground grid
│   │   ├── GroundSettings.h      # A3.6 ground choice (grid / plane), height modes, material presets
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
│   │   │   ├── Diagnostics.cpp   # Captures, profiling/scenario reports & UI test hooks
│   │   │   └── ViewportImage.cpp # Viewport menu capture / save / copy (PNG file, clipboard)
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

- **Profile**: OpenGL 4.6 Core Profile (via GLAD and GLFW). `Application::run` requests 4.6 and, if context creation fails, 4.5, then 4.3, then 4.1, and logs which version it obtained. Compute shaders, SSBOs and image load/store need 4.3+; query `ux3d::imgui::glCaps()` (`header/imgui/GlCaps.h`: `major`, `minor`, `compute`, `maxComputeWorkGroupInvocations`) before using them. The profile and benchmark reports carry the same values in their `gl` object. The vendored loader in `thirdparty/glad` is glad 0.1.36 generated for `gl=4.6` core with all extensions (command in `thirdparty/glad/README.md`). Every existing shader (and the ImGui backend) deliberately stays at `#version 410`, so the 4.6 context changes no rendered pixel (`scripts/validate_gl46.py` compares captures byte for byte with the baseline).
- **Core Pipeline**:
  - `ux3d::renderer::GltfRenderLayer`: Manages the active scene, geometry buffers, skins, and reverse mapping from drawables to glTF nodes. Its traversal applies local-view isolation (one node plus optional descendants) and inherited visibility (hiding a node hides its whole subtree) without changing the glTF visibility flags. All scene materials and the background skybox render in linear HDR space (`LINEAR_OUTPUT=1`) into a multisample-capable `GL_RGBA16F` target (`vp->sceneFbo`).
  - `ux3d::renderer::CommandGenerator`: Manages PBR material uniforms, punctual lights, IBL parameters, and pass bucketing (`DecalPrepass`, `DecalAccumulation`, `Opaque`, `Grid`, `Transparent2`, `TransmissiveBlit`, `Transmissive`, `Transparent`). The glass refraction copy (`TransmissionTarget`, `GL_RGBA16F` with regenerated mip chain) and windshield SSR capture directly from `vp->sceneFbo` without duplicate linear material passes.
  - `ux3d::imgui::AntiAliasing`: Executes the per-viewport HDR and LDR post-processing chain:
    `scene (HDR, MSAA) -> resolve -> TAA in HDR (reversible Karis luminance weighting c/(1+luma) before neighborhood clamp/blend and c/(1-luma) after) -> auto-exposure metering (64x64 centre-weighted log-average luminance with outlier rejection) -> physically based bloom (Jimenez 2014 13-tap Karis downsample + 3x3 tent additive upsample) -> post tone mapping (exposure * Khronos PBR Neutral / ACES Hill / ACES Narkowicz + linear-to-sRGB) -> FXAA (LDR) -> unjittered overlays & wireframe (vp->overlayFbo) -> ImGui`. Debug Render Modes (`--debug-view`) bypass bloom and tone mapping. Both Wireframe shading mode and Wireframe Overlay draw lines after tone mapping into the LDR `vp->overlayFbo` without feeding bloom, auto-exposure, or TAA history.
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
- **Single Portable Settings File**: Every persisted preference lives in one `settings.ini` next to `Glider.exe`: window state `[Window]`, recent files `[RecentFiles]`, search paths `[SearchPaths]`, hierarchy/resource display `[SceneHierarchy]`/`[Resources]`, render quality `[RenderQuality]`, the viewport capture folder `[Captures]` (`folder`, stored only when set; see §3.8), the four quality presets `[QualityProfiles]` (`profile0.*` … `profile3.*`, `selection`; 0 = Auto, 1–4 = profiles) and the ImGui docking layout `[Layout]`. No settings folder (no `GliderData/`) is created next to the executable, so `Glider.exe` still runs as a single file.
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
- **Extension inspector:** every material parameter is editable in memory; nothing is written to disk. Extensions are never added or removed: only extensions already present on a material are listed. Rendered Khronos material extensions have a per-material, per-viewport display switch (default on); unsupported extensions have no switch. Switching off affects only rendering and leaves the loaded extension and its parameters intact. Typed ASOBO parameters and their existing viewport switches retain their behaviour. Existing non-ASOBO vendor values use numeric drags, checkboxes, colour/number arrays and schema enum combos; strings without an enum remain read-only, and keys cannot be added or removed. Vendor edits go through `ExtensionInspector::setExtensionParameter` and mark `SceneChange::Extension` plus `Material` for material extensions. Display switches are session state and are not persisted.
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

**Viewport Settings → Camera & Helpers → Ground** chooses what a viewport draws on the ground: **Grid** (the default, the editor grid overlay, byte-identical to before), **Plane**, and a disabled *Ocean (A6.8)* entry for the later ocean. Per viewport, in `ViewportSettingsState::ground` (`header/imgui/GroundSettings.h`).
- **Plane:** `GltfResourceManager::getGroundTuple()` is an Opaque-pass drawable (a screen quad, `ground.vert` / `ground.frag`) that `Viewports.cpp` appends to the scene while the viewport shows the plane in a shaded view (debug views and wireframe keep the sky). The fragment shader intersects each pixel's ray with the plane `y = h` (the `GridRenderer` ray path), writes `gl_FragDepth` (1 beyond the far plane), shades a dielectric (F0 0.04) with albedo, roughness, optional procedural detail (four noise octaves and slab joints that fade to their mean with the pixel footprint) from the IBL (`getIBLRadianceLambertian` / `GGX`, the HDR's own lower hemisphere) and the A3.1 sun with `sunShadowVisibility` (A3.2), and fogs towards the environment's horizon (`1 - exp(-k d)`, 98% at the haze distance, automatic 0.9 x far plane). Height modes: **Model base** (`Application::getCachedModelBaseHeight`: lowest world Y of the POSITION boxes of the visible nodes' static pose, cached per scene generation and Transform/Visibility/Material revisions), **Y = 0**, **Custom**.
- Not pickable: the drawable is removed from the scene before the picking passes and never enters the overlay depth, so clicks fall through to the background. **Show grid on ground** draws the grid on the plane at the plane's height (the overlay depth is then rebuilt from the opaque model only).
- The shadow cascades also cover the plane and the model's footprint on it (`updateSunShadows` `groundHeight`).
- `ViewportInstance::groundAlbedo` / `groundPlaneActive` / `groundPlaneHeight` hold the last render's ground; A6.2 reads the albedo as its ground albedo.


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
- `--without-textures`: Open the startup model (and `--reload-model`, if supplied) with geometry and material parameters, intentionally skipping image reads, decoding, previews, GPU image uploads and generated normal-map tangents. Equivalent to **File → Open without texture ...**. Texture references remain inspectable; quality changes keep them omitted. A normal File → Open / Open Recent loads textures again. This is a per-open choice, not a saved preference. GLB/shared buffers still have to be read for geometry. See `docs/LOADING_SPEED_AND_TEXTURE_FREE_OPEN.md`.
- `--aa <none|msaa2|msaa4|msaa8|fxaa|taa>`: Legacy AA preset, setting both MSAA and Post AA (e.g. msaa4 = 4x + None, taa = Off + TAA, none = Off + None).
- `--msaa <0|2|4|8>` and `--post-aa <none|fxaa|taa>`: Independent per-viewport geometry and post AA overrides, applied after `--aa` regardless of argument order. New viewports default to MSAA Off + TAA.
- `--isolate-node <name>`: Isolate a node (without descendants), select it and frame the camera on startup.
- `--select-node <name>`: Automatically selects a node by name and frames camera on startup.
- `--frame-node <name|#index>`: Frame a node on startup using the same bounds and distance logic as **Object -> Focus (`F`)**. Selects the node. Node lookup matches exact, case-insensitive, unique substring, or `#index` (`--select-node`/`--isolate-node` keep exact names). Nodes with empty geometry bounds frame their world position at a default distance without error.
- `--frame-view <front|back|left|right|top|bottom|iso>`: Preset viewing direction for `--frame-node`.
- `--frame-margin <x>`: Distance multiplier for `--frame-node` (default 1.0; 2.0 frames twice as far away).
- `--report-node <name|#index>`: Repeatable. Adds `nodes` array to `--profile-report` with `index`, `name`, `visible`, `screenRect` `[x0, y0, x1, y1]` in capture-image coordinates, `onScreen`, `clipped`, `behindCamera`, and world-space `center`.
- `--report-pick <x,y>`: Repeatable. Adds `picks` array to `--profile-report` with GPU-picked node, mesh, primitive, and material under pixel `(x, y)` in capture-image coordinates (or `null` if background).
- `--ui-script-follow <file>`: Watches `<file>` for appended commands, executes them between frames on the main thread, and writes structured JSONL results to `<file>.out.jsonl`. Wakes at least every 250 ms when idle. Enforces non-persistent isolated mode and displays `Agent script: <file>` in the status bar. See [`docs/LIVE_SCRIPT_CONTROL.md`](docs/LIVE_SCRIPT_CONTROL.md). Script lines run through the shared command registry, so every registry command (below) also works in `--ui-script` files.
- `--control-pipe`: Opens the agent control pipe `\\.\pipe\glider-<pid>` (current user only, local only, one client at a time; also **File → Settings → Agent Control**, session only). Line-delimited JSON `{"id","cmd","args"}`; `describe` lists the commands, which come from the same registry as the CLI flags and `--ui-script` (`header/imgui/CommandRegistry.h`, `source/imgui/application/Commands.cpp`). The status bar shows "Agent connected" while a client is attached; pipe commands never write `settings.ini` unless the client calls `set_persist(true)`. Clients: `scripts/glider_mcp.py` (dependency-free MCP stdio server; `--pid`, `--launch`, `--exe`) and `scripts/glider_ctl.py`. Runtime-safe viewport/environment flags (`--ssr`, `--asobo-disable`, `--exposure`, `--wireframe-*`, `--debug-view`, `--set-asobo-param`, `--select-node`, ...) apply through the registry's settings, so a pipe `set_viewport_setting` / `set_environment` / `set_material_param` renders exactly like the flag; a new setting is added by registering it once (`docs/AGENT_CONTROL_PLAN.md` §7.2). Readback: `probe` (linear HDR before tone mapping, LDR, depth, world position, pick) and `probe_region`. Checked by `scripts/validate_agent_control.py`.
- `--agent-guide [<out.md>]` / `--agent-guide --format skill <dir>`: Writes the embedded agent guide (`resources/agent/GLIDER_AGENT_GUIDE.md`) followed by the command and settings tables generated from the registry, then exits without opening a window (no path: redirected stdout or the parent console). `skill` writes `<dir>/glider-control/SKILL.md`.
- `--debug-view <name>`: Selects a viewport Render Mode at startup, using the View menu's names with spaces,
  underscores and hyphens ignored (`occlusion`, `roughness`, `shadingnormal`, `geometrybitangent`,
  `normaltexture`, `decalprepassdepth`, `decalprepassnormal`, `ssr`, `ssrray`, ...).
  SSR shows hit U/V in red/green and confidence in blue (black = miss); SSR Ray shows the view-space
  reflected direction mapped to RGB (magenta = rejected physical-pane hemisphere). Unknown names log the value and leave the
  default shaded view.
- `--select-viewport <id>`: Selects a viewport to inspect its settings.
- `--dump-frame <output.jpg>`: Renders 8 warm-up frames and dumps the window framebuffer to disk as `.jpg` or `.png`.
- `--decal-tolerance <mm>`: Per-viewport decal depth tolerance along the receiver normal, 0–10 mm (default 1.2 mm); applies only to the decal buffer. Also editable in Viewport Settings → MSFS Extensions.
- `--decal-prepass-check <out.json>`: A2.1 diagnostic. After the warm-up frames it reads the decal prepass
  depth/G0 and the forward depth/Geometry Normal, writes the maximum differences and viewport/buffer sizes,
  then exits. It forces a single-sample target and redraws every frame. Files without a decal material report
  `decalBuffer.active: false` instead of failing.
- `--dump-next-frame <output.png>`: With `--dump-frame`, also dump the immediately following window frame (overlay steadiness diagnostic); `--exit-after-dump` waits for both.
- `--exit-after-dump`: Exits the application immediately after dumping the frame.
- `--quality <high|balanced|performance>`: Render-quality preset for this run (default High, the original quality). Finer overrides: `--ibl-size`, `--ibl-irradiance-size`, `--ibl-lut-size`, `--ibl-precision <32|16>`, `--ibl-samples`, `--grid-mode <ray|plane>`, `--render-scale`. The UI equivalents are **Viewport Settings → Quality** (preset, ground grid, render scale) and **Scene Inspector → IBL Filter Settings** (the IBL filter sizes and samples, shown while IBL is enabled); both are renderer-wide and saved in the portable `settings.ini` (`[RenderQuality]`; `%APPDATA%/Glider/settings.ini` on fallback).
- `--auto-quality <on|off>`: Auto quality defaults to on for interactive sessions and is saved by **Viewport Settings → Quality → Auto quality**. It starts with cheap effective settings and steps toward the configured choices using measured CPU/GPU frame time and RAM/VRAM headroom. The configured choices are never replaced by the automatic values. Explicit CLI IBL/scale/AA/SSR/bloom overrides and manual UI edits pin their setting group. `--isolated`, `--no-persist`, `--dump-frame`, `--capture-viewport`, `--fixed-frame-time`, and `--ui-script` disable adaptation unless `--auto-quality on` is explicit. Off preserves the configured rendering path; loading memory protection remains active. See `docs/LOADING_AND_ADAPTIVE_QUALITY.md`.
- `--capture-loading <window.png>`: Save one loading-progress window frame during image decoding. Ordinary capture/dump frame numbers count model frames, excluding loading frames. `--profile-report` now also includes `startup.loadStagesMs`, progress-frame count, texture estimates/reductions, sampled DXGI GPU usage, and adaptive status/effective settings.
- Loading diagnostics also include `lastLoadWallMs`, `lastLoadToFirstModelFrameMs`, `loadingProgressUiMs`, image CPU/GPU payload bytes and `texturesIntentionallyOmitted`. `Shader link` and `Tangent generation` are separate stages. Normal interactive opens log import completion and first model frame readiness separately. `GLIDER_TEST_OPEN_FILE=<path>` supplies the native-open result only while a UI script is active, for focused menu checks; it has no effect in ordinary sessions.
- `GLIDER_TEST_VRAM_MB=<MiB>` (environment): caps the reported process-local video-memory budget for constrained tests. `GLIDER_TEST_QUEUE_LOAD=<model>` queues one replacement during image decoding. `GLIDER_TEST_TEXTURE_ALLOCATION_FAILURE=1` injects one texture-allocation fallback branch without inserting a GL error; `GLIDER_TEST_LOAD_FAILURE=upload` exercises safe failure of model upload. These hooks do not change persistent preferences.
- `--hdr-rotation <deg>`, `--exposure <ev>`, `--tone-mapping <neutral|hill|narkowicz>`: Environment rotation,
  exposure and tone mapping for this run. Environment settings belong to the glTF scene (one set per scene, shared
  by every viewport) and are edited in the Inspector when the scene row is selected (`SceneInspector`).
- `--ssr <on|off>`: per-viewport windshield-v3 screen-space reflections (default on).
- `--ssr-quality <low|medium|high>`: 256/512/1024 screen-space sample budgets, maximum pixel stride 2/1/1, with five refinement steps (default medium).
- `--interior-sky-reflection <0..1>`: windshield IBL fallback strength inside the combined environment-occluder bounds; without occluders it applies to every windshield (default 0.5). These three controls are in Viewport Settings → MSFS Extensions.
- `--windshield-icing <0..1>`: icing state (0.0 = clean [default], 1.0 = fully iced): windshield icing coverage and the coverage of `frosted` geometry decals. Viewport Settings → MSFS Extensions → Icing.
- `--clear-color <r,g,b>`: scene clear colour for this run; disables the background skybox (capture/test hook).
- `--hide-grid`: hide the active viewport's ground grid for review captures.
- `--ground <grid|plane>`: what the active viewport draws on the ground (A3.6): the editor grid (default, unchanged) or an infinite ground plane in the linear HDR scene target (ray intersection like `--grid-mode ray`, writes depth, lit by the IBL and by the sun with the A3.2 shadows, fogged into the environment's horizon; shaded views only). `--ground-height <model-base|zero|number>` (default model-base: the scene's lowest point from the glTF node transforms, visible nodes only, ASOBO invisible/occluder materials ignored, never moved by animation; a number is a custom height in scene units), `--ground-preset <concrete|grass|sand|grey>` (sets colour, roughness, detail and slab joints), `--ground-color <r,g,b>` (linear albedo, also the average albedo A6.2 reads), `--ground-roughness <0..1>`, `--ground-detail <0..1>`, `--ground-detail-scale <units>`, `--ground-joints <units>` (slab joint spacing, 0 = none), `--ground-haze <units>` (fog visibility distance, 0 = automatic 0.9 x far plane), `--ground-grid <on|off>` (draw the grid on the plane, at its height). Later flags override earlier ones (a preset after a colour replaces the colour). Registry keys: `ground`, `ground_height_mode`, `ground_height`, `ground_preset`, `ground_color`, `ground_roughness`, `ground_detail`, `ground_detail_scale`, `ground_joint_spacing`, `ground_haze`, `ground_grid`; `get_state` carries a `ground` object.
- `--ssao <on|off>`: per-viewport screen-space ambient occlusion of the sky light (A3.5, default off; Auto quality never changes it). GTAO from the A2.1 depth/normal prepass (the decal buffer when decals are active, otherwise the viewport's own prepass target), computed before the opaque pass with a depth-aware 5 x 5 denoise (and a depth-aware upsample at half resolution); `pbr.frag` multiplies only the IBL diffuse (multi-bounce) and, through Lagarde's specular occlusion, the IBL specular, sheen and clearcoat of OPAQUE/MASK surfaces, as `min(authored occlusion, SSAO)`; the sun, punctual lights, emissive, BLEND/transmission, the background and the ground plane are unchanged. `--ssao-radius <x>` (world units, default 0.5), `--ssao-intensity <0..4>` (exponent, default 1), `--ssao-quality <low|medium|high>` (2 x 4, 3 x 6, 4 x 8 samples per side), `--ssao-resolution <full|half>`; debug view `ssao` (raw visibility). Viewport Settings → Render Modes → Ambient Occlusion; registry keys `ssao`, `ssao_radius`, `ssao_intensity`, `ssao_quality`, `ssao_resolution`; `get_state` / `--profile-report` carry an `ssao` object (per viewport: active, target size, memory, GPU time). Fragment shaders only (`#version 410`): works on every 4.1+ context. See `docs/ASOBO_EXTENSIONS_PHASE_3.md` A3.5.
- `--show-invisible-materials`: Also draw materials marked invisible by `ASOBO_material_invisible` and environment occluders (`ASOBO_material_environment_occluder`); both are hidden by default.
- `--asobo off|on`, `--asobo-disable <extension>` (repeatable): MSFS (ASOBO) extensions the active viewport applies (Viewport Settings → MSFS Extensions; all on by default; off renders the file as if it had none). Every `Rendered`/`Partial` row of the support table with a viewport switch can be disabled, e.g. `--asobo-disable ASOBO_material_geometry_decal` or `--asobo-disable ASOBO_material_detail_map`.
- `--msfs-emissive-scale <x>`: Scene environment MSFS emissive scale factor (default 0.001 / `1/1000`). For assets declaring ASOBO extensions (`ASOBO_*`) in viewports with ASOBO enabled, scales authored `emissiveFactor` ($E = \text{emissiveFactor} \times \text{emissiveTexture} \times \text{msfsEmissiveScale}$) so MSFS default exporter scale 1000 maps to 1.0, and high-intensity cockpit screens (e.g. 3000) bloom. Fall back to [0, 1] clamped factor when ASOBO is disabled. Also editable in Scene Inspector → Environment.
- `--msfs-emissive on|off`: Same-binary A/B switch; `off` renders ASOBO assets with the clamped core emissive factor (the behaviour before the MSFS mapping). Default on.
- `--sun on|off`, `--sun-mode manual|hdr`, `--sun-dir <az,el>`, `--sun-color <r,g,b>`, `--sun-intensity <x>`,
  `--sun-angular-radius <deg>`: A3.1 sun (scene environment; Scene Inspector → Environment → Sun; the same keys
  `sun`, `sun_mode`, `sun_dir`, `sun_color`, `sun_intensity`, `sun_angular_radius` through `set_environment`).
  Directional light through the punctual-light BRDF. Azimuth around +Y from +Z towards +X, elevation above the
  horizon; intensity is illuminance in the HDR's radiance units x sr (a white Lambert surface facing the sun
  reflects colour x intensity / pi). `hdr` mode takes the HDR's brightest region (direction after
  `--hdr-rotation`, colour, integrated illuminance x HDR intensity). While the sun is on, the HDR's own sun is
  removed from the IBL and background (one re-filter); off (default) is byte-identical. `get_state` and
  `--profile-report` include a `sun` object. See `docs/ASOBO_EXTENSIONS_PHASE_3.md` (A3.1).
- `--shadows on|off`, `--shadow-resolution <1024|2048|4096>`, `--shadow-cascades <2..4>`, `--shadow-distance <m>`,
  `--shadow-filter <hard|pcf|soft>`, `--shadow-split-lambda <0..1>`, `--shadow-normal-bias <texels>`,
  `--shadow-slope-bias <x>`, `--shadow-constant-bias <x>`: A3.2 cascaded shadow maps of the sun (drawn only while
  the sun is on; default on, 2048 x 3 cascades, 200 m, PCF, lambda 0.75, biases 1 / 2 / 1). Renderer-wide
  (Viewport Settings → Quality → Sun Shadows; the quality fields are part of the four presets in
  `[QualityProfiles]`, all of them in `[RenderQuality]`; Auto starts at 1024 x 2 and steps up to the configured
  values); registry keys `shadows`, `shadow_resolution`, `shadow_cascades`, `shadow_distance`, `shadow_filter`,
  `shadow_split_lambda`, `shadow_normal_bias`, `shadow_slope_bias`, `shadow_constant_bias` (saved preferences:
  `set_persist(true)` over the pipe). Each viewport keeps its own map, fitted to its frustum (size² x cascades x
  4 bytes: 48 MiB at the default), released while the sun or its shadows are off. `--debug-view shadowcascades`
  and `shadowvisibility` show the cascades and the visibility term; `sun.shadows` in `get_state` and
  `--profile-report` lists each viewport's cascades. Sun off and shadows off are byte-identical to before. See
  `docs/ASOBO_EXTENSIONS_PHASE_3.md` (A3.2).
- `--time-of-day <day|night>`: Scene environment time of day (day=0 [default], night=1). Controls `ASOBO_material_emissive` day/night multipliers (`emissiveDayMultiplier` during day, `emissiveNightMultiplier` at night). Also editable in Scene Inspector → Environment.
- `--set-asobo-param <material> <extension> <param> <value>` (repeatable): edits a material's ASOBO extension parameter in memory after loading (by material name or index), using the same code path as the Inspector UI (`ExtensionInspector::setMaterialAsoboParameter` + `GltfRenderLayer::updateMaterialStates`).
- `--quality-preset <auto|low|med|mid|high|ultra>`: select the editable renderer-wide preset. Interactive startup defaults to Auto, which adapts toward Ultra. The viewport dropdown has Auto/Low/Mid/High/Ultra; **File → Settings → Quality Presets** edits and resets the four definitions. They are saved in the portable `settings.ini` (`[QualityProfiles]`; same fallback and isolated rules as other settings). Texture-cap changes refresh images without reopening the model. Legacy `--quality` retains its existing meanings. See `docs/QUALITY_PRESETS_AND_TELEMETRY.md`.
- `--show-telemetry`: show status-bar VRAM usage/budget and FPS in deterministic tooling runs. Normal interactive runs show these automatically. Existing isolated/capture harnesses keep their previous chrome unless this flag or `--quality-preset` explicitly requests the new controls. Clicking either readout opens and focuses the Statistics panel.
- `--search-path <dir>` (repeatable): extra texture folder for this run, relative to the model; works with `--isolated`, which ignores `settings.ini`. In **File → Settings → Search Paths**, folders may use case-insensitive wildcards: `../texture*` matches sibling texture folders, `../*` searches immediate sibling folders, and `../**` searches recursively. Recursive expansion skips symlinks/junctions; reload the model to rescan newly created folders.
- Search Paths starts with the ordinary, removable `../texture*` list item when `[SearchPaths]` is absent. Removing every item persists `[SearchPaths] count=0`, so the default stays removed after restarting. Existing saved lists are authoritative; restoring the default-only list removes the override section, consistent with saving only non-default values. The model's own folder remains the separate implicit lookup location.
- `--compute-selfcheck <out.json>`: GL 4.6 smoke test. On a 4.3+ context it dispatches a `#version 430` compute shader that writes a known pattern into an `r32ui` image with `imageStore`, reads it back and reports PASS/FAIL (exit code 0/1); below 4.3 it reports "skipped" and succeeds. Registered in `scripts/run_regression.py`.
- `--validate-file <gltf> --validation-report <out.json>`: validate headlessly and exit; files the loader accepts are validated with their model, so the model-based checks (including ASOBO) run.
- `--asobo-report <out.json>`: After loading (and any `--reload-model`), write the typed ASOBO extension model and its diagnostics, then exit (`scripts/validate_asobo_model.py`). The model lives in `gltf/extensions/AsoboExtensions.h`; its table is generated by `scripts/generate_asobo_specs.py` from `resources/schemas/asobo` plus the exporter data in `docs/ASOBO_EXTENSIONS_SEMANTICS.md`.
- `--disable-shared-traversal`: Every viewport traverses the scene itself (A/B reference).
- `--render-mode <shaded|wireframe>`: viewport shading mode for captures and profiling.
- `--wireframe-overlay`: draw mesh edges over the shaded viewport.
- `--wireframe-color <r,g,b[,a]>`: wireframe line RGB colour (0..1) with optional opacity (default 1,1,1,1).
- `--wireframe-blend <normal|additive|invert|xor>`: blend mode for wireframe lines drawn into the LDR overlay target (default normal).
- `--disable-wireframe-cache`: same-binary A/B reference; copy wireframe materials per drawable on every redraw instead of reusing per-viewport variants. `wireframeMaterialCopies` in profile frame counters reports actual copies.
- `--disable-pick-culling`, `--disable-instancing`: Same-binary A/B references for the Phase 8 hybrid picking and instancing (see `docs/RENDERER_OPTIMIZATION_PHASE_8.md`).
- `GLIDER_NO_FOCUS=1` (environment): shows and renders the window normally but creates it without keyboard focus (`GLFW_FOCUSED` / `GLFW_FOCUS_ON_SHOW` false); unset behaviour is unchanged.

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
- **`run_regression.py`** (quick tier, about 2 minutes) runs the renderer harnesses
  (`scripts/validate_p8_*.py`, `validate_p4_exit_gate.py`) three at a time, then the built-in self-checks and the ASOBO
  harnesses (`validate_asobo_*.py`) one at a time, against that baseline. They check byte-identical captures and
  identical selections. Window screenshots are compared on their viewport area only, so side-panel UI changes do
  not fail them (`validate_p4_exit_gate.py` covers the panels). The script's docstring lists what each suite covers.
- **`--tier full`** adds each feature's same-binary A/B switch, `validate_p5_1.py`, and `validate_p8_5.py`'s
  10,000-drawable and MSAA/TAA picking cases.
- **Harness launches never request keyboard focus**: they go through `scripts/harness_process.py`, which sets `GLIDER_NO_FOCUS=1` for the current build and any baseline that carries the marker. Older baselines are only started with `SW_SHOWNOACTIVATE` and may still take focus until the next commit's baseline carries the marker; the pinned references (`167a5e0`, `a9c0a29`) are cached under `build/regression_cache/` after their first render, keyed by the exe, the exact arguments and the model's input files, so repeat runs skip the launch. `scripts/check_no_focus.py` checks the current build.
- **Benchmarks** never run here. They back timing claims only: pass `--bench` to the harness concerned, and do not
  run them in parallel.

### 4.6 Third-party code

There are no git submodules. GLM is vendored as headers in `thirdparty/glm` (version in its README); the other
libraries are fetched by CMake `FetchContent` (`cmake/standalone_dependencies.cmake`). The glTF, Khronos and ASOBO
JSON schemas live in `resources/schemas`. The UX3D libraries' own unit tests (`UX3D_RUNTIME_TESTING`,
`UX3D_TOOLS_TESTING`) and the editor's `Operations` library are not built.

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
6. **Portable & Robust Settings Storage**:
   - Never hardcode `%APPDATA%` or read/write settings in the working directory `.` directly.
   - All persisted preferences go through `ux3d::imgui::SettingsFile` (`settings.ini` sections); do not add a new per-feature state file or a `GliderData`-style folder.
   - All filesystem operations on settings must use non-throwing `std::filesystem` overloads with `std::error_code` and check streams. Unreadable/corrupt files must fall back to defaults with a warning; unwritable files must keep state in memory with a warning (no repeated per-frame log spam).
   - `--isolated` must never read or write any persistent state to disk.
