# Developer & Agent Onboarding Guide (`AGENTS.md`)

## 1. Project Mission & Identity

Glider is a lightweight **viewer-only / visual inspector** for glTF and GLB, forked from UX3D Gestaltor.
It uses C++20, Dear ImGui Docking, GLFW and OpenGL 4.6 (fallbacks 4.5, 4.3, 4.1).
**One standalone `Glider.exe`** embeds runtime assets with Zstandard: no external asset folders or runtime DLL dependencies.

## 2. Directory Structure

- `header/imgui/`, `source/imgui/`: panels, helpers and runtime systems.
- `source/imgui/application/`: startup, CLI, frames, scenes, menus, panels, viewports and diagnostics.
- `header/renderViewInput/`, `source/renderViewInput/`: camera navigation.
- `OpenGL-Renderer/`: core PBR renderer and UX3D tools.
- `resources/`: embedded source assets; generated VFS/archive outputs are not committed.
- `scripts/`: resource packing, validation, regression and release tooling.
- `thirdparty/`, `cmake/`: dependencies and build configuration.
- `legacy/`: archived Qt editor and assets, retained as reference.
- `docs/`: topic references, feature plans and validation evidence.

## 3. Where to Read Next

- [Architecture](docs/ARCHITECTURE.md): read for VFS, render passes, picking, camera, theme, docking/settings storage, validator UI, object actions, animation or ground/ocean; includes the detailed source map.
- [CLI reference](docs/CLI_REFERENCE.md): read when adding or using flags, capture/diagnostic routes, registry keys or test environment variables.
- [Build and test](docs/BUILD_AND_TEST.md): read before configuring/building, selecting regression checks, updating embedded assets, changing dependencies or publishing releases.
- [Settings scopes](docs/SETTINGS_SCOPES.md): read for renderer quality, scene effects, viewport overrides, compatibility keys and persistence ownership.
- [Live script control](docs/LIVE_SCRIPT_CONTROL.md): read for command files and scripted UI checks on a running instance.
- [Agent control plan and implementation](docs/AGENT_CONTROL_PLAN.md): read for named-pipe/MCP control, shared commands, probes or registering a setting (§7.2).
- [Quality presets and telemetry](docs/QUALITY_PRESETS_AND_TELEMETRY.md): read for editable profiles, Auto selection and Statistics telemetry.
- [Loading and adaptive quality](docs/LOADING_AND_ADAPTIVE_QUALITY.md): read for loading progress, memory protection and quality adaptation.
- [Texture-free loading](docs/LOADING_SPEED_AND_TEXTURE_FREE_OPEN.md): read for import performance and opening without textures.
- [ASOBO extension journal](docs/ASOBO_EXTENSIONS_JOURNAL.md): start here for MSFS support; follow its reference, semantics and phase 0–7 docs for the relevant extension.
- [Renderer optimization journal](docs/RENDERER_OPTIMIZATION_JOURNAL.md): read for performance work and the relevant phase's correctness gates and evidence.
- [ImGui migration plan](docs/MIGRATION_PLAN_IMGUI.md): read for historical Qt migration context.

## 4. Build & Development Workflows

On Windows, configure and build with:

```cmd
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Glider
```

Output: `build/bin/Release/Glider.exe`. Prerequisites and complete workflows: [BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md).

After each commit, build and save its binary as the clean baseline with `python scripts/save_baseline.py` (tracked files must match the commit). Compare subsequent uncommitted changes with `python scripts/run_regression.py` (quick tier). Each change runs its own focused check plus the closest **1–3 validators**; run `python scripts/run_regression.py --tier full` **once per commit for broad changes**. See [§4.5](docs/BUILD_AND_TEST.md#45-regression-testing) for comparisons, scheduling and baseline caching. Benchmarks are separate timing evidence: use the harness's `--bench` and never run benchmarks in parallel. Docs-only edits use documentation checks.

Portable settings: persisted preferences use `SettingsFile` and one `settings.ini` next to the executable (fallback through the store to `%APPDATA%/Glider`, then `%TEMP%/Glider`). Save only non-default values, omit empty sections and never rewrite unchanged content; pristine defaults create no file. Preserve one-time legacy migration and safe in-memory fallback. Sections include `[GltfExtensions]` (switched-off glTF extensions, edited in File → Settings → glTF Extensions; quality is File → Settings → Quality). Use `GLIDER_DATA_DIR=<dir>` for test storage without migration. **`--isolated` reads and writes no persistent state.** Details: [architecture §3.6](docs/ARCHITECTURE.md#36-docking--layout-persistence).

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
   - `[GltfExtensions]` is written only by `GltfExtensionSettings`; command-line flags never reach it.
   - All filesystem operations on settings must use non-throwing `std::filesystem` overloads with `std::error_code` and check streams. Unreadable/corrupt files must fall back to defaults with a warning; unwritable files must keep state in memory with a warning (no repeated per-frame log spam).
   - `--isolated` must never read or write any persistent state to disk.
