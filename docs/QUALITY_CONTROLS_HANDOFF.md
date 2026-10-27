# Quality controls handoff — 2026-10-27

Later continuation note: the underscore probe failure was traced to `UiScenarioHarness`
converting every underscore in a tag to a space. It now prefers exact registered tags,
then falls back to legacy space aliases. The newer texture-free File-menu checks exercise
that correction. The older persistence script was not rerun here; current storage is the
single `settings.ini` described in AGENTS, rather than this historical journal's JSON file.
See `LOADING_SPEED_AND_TEXTURE_FREE_OPEN.md` for the later loading follow-up and verification.

This is a continuation journal for the loading/adaptive-quality task and its later UI requests. All changes are uncommitted. Never commit; use only `git status`, `git diff`, or `git log`. Never copy/rename executables or delete `build`. The user will run regressions: **do not run regression suites**. Do not touch the unrelated existing `scratch/` directory.

## Current state

The original loading work was completed and measured before the later UI requests. Its detailed results are in `C:\Users\uray\.claude\codex-runs\gltf-viewer\loading-perf.report.md`; architecture is in `docs/LOADING_AND_ADAPTIVE_QUALITY.md`. That report's checks predate the newer quality UI changes.

The follow-up is implemented and the focused checks listed below now pass end to end. It compiles with `cmake --build build --config Release --target Glider`. The latest binary is `build/bin/Release/Glider.exe` and is newer than the last source edit; the build log is `build/review/loading/presets-build.log`. Executable launches work on this machine, so the original launch-failure restriction was not triggered.

Implemented follow-up:

- Status bar GPU usage/budget progress and FPS, detailed tooltips; click either readout to open and focus the Statistics panel; hide adaptive status once target reached.
- Statistics Overview/Textures/Meshes tabs: OS GPU budget/usage/capacity/shared usage, RAM/commit/process peaks, CPU frame/p95/process utilization, actual rendered GPU timing, presentation/render FPS, asset payload estimates and per-texture/mapped mesh buffer payloads. Inspector shows texture/mesh GPU payload too.
- Viewport Auto/Low/Mid/High/Ultra selector; default Auto targets editable Ultra. Profiles are global because images and IBL are shared. Deterministic old captures retain legacy settings/chrome unless new controls are explicitly requested.
- File > Settings replaces Search Paths, with Search Paths and Quality Presets tabs. Editable four profiles, per-profile reset and Use this preset. All preferences (search paths, hierarchy/resource display, window state, recent files, render quality, the four profiles, and the ImGui layout) are stored in one portable `settings.ini` next to `Glider.exe`, written only when a value differs from its default. Isolated mode never persists.
- Low: 512 max, no AA/skybox/IBL/bloom/SSR, plane grid, unlit textured preview so unlit scenes remain visible. Med: 1024 max, MSAA2 with no post-AA, IBL cube/samples512, ray grid, no SSR/bloom. High: 4096, MSAA2+TAA, full IBL, medium SSR, bloom. Ultra: uncapped, MSAA4+TAA, high SSR, full IBL/bloom. Med's conflicting “no AA / MSAA2” was interpreted as no post-AA plus MSAA2.
- Live texture-cap changes re-decode/re-upload images only, preserving model/selection/edits/animation. Recover full resolution when switching upward.
- Wildcard search folders `../texture*`, `../*`, `../**`, case-insensitive matching and cached expansion; skip symlinks/junctions. Reload invalidates cache.
- New CLI `--quality-preset auto|low|med|mid|high|ultra`, `--show-telemetry`; existing `--quality` semantics preserved.

## Focused verification — all groups pass

`& C:\ProgramData\anaconda3\python.exe scripts/check_quality_controls.py` generates small texture fixtures in `build/review/quality_controls/` and exercises the actual application controls, including a private `GLIDER_DATA_DIR`. Latest output (`build/review/loading/quality-controls-check.log`):

```text
PASS low 512 none
PASS med 1024 msaa2
PASS high 4096 msaa2+taa
PASS ultra 0 msaa4+taa
PASS wildcard_immediate 512 none
PASS wildcard_recursive 512 none
PASS relative_model 512 none
PASS Settings tabs / live Low -> Ultra texture refresh / statistics screenshots
PASS status bar FPS/VRAM click opens the Statistics panel
PASS saved profile override and reset (single settings.ini in a private directory)
PASS settings.ini stores only non-default values
```

All preset runs completed with zero automation GL errors. All three wildcard forms resolved their textures. Settings tabs, live texture refresh and the Statistics tabs rendered correctly.

### Probe failure root cause (resolved)

The previous blocker, `probe not visible: clickp quality.texture_max 1`, was a harness addressing bug, not an application failure. `UiScenario` converts `_` to a space in `clickp`/`hoverp` tags, so `quality.texture_max` addressed a label named `quality.texture max`. Fixes:

- Renamed the tag to `quality.texture.max` so it survives the conversion.
- Recorded `UiProbe::item` after submitting the size combo (the real combo rectangle) instead of a manually captured pre-combo `UiProbe::rect`.

No assertions were weakened. With the probe reachable, persistence was confirmed: Low's texture maximum 1024 is written as `[QualityProfiles] profile0.textureMaxSize=1024` in the private `settings.ini`, and the per-profile reset removes the override again (the default is 512), leaving Med at its default 1024 with no stored key.

### Auto quality verification

Run with `--auto-quality on` and the small fixture. The controller stepped from level 1 to 6 in about 2.5 s, logged `configured quality reached (step 6/6, 0.6 ms)`, the toolbar adaptive caption disappeared, and the Auto selector remained active. Captures: `build/review/quality_controls/auto_reached.png`.

### Regression found while testing (fixed)

With a relative model path, wildcard-expanded search folders were cached as relative paths; `resolveAssetPath` then prepended the model directory a second time, so textures loaded as the error image. `expandSearchPathsLocked` in `source/imgui/AppSettings.cpp` now stores `std::filesystem::absolute(...).lexically_normal()` results, falling back to the relative path on an `std::error_code`. A `relative_model` case was added to the focused harness and now passes.

### Layout fixes

The Quality Presets editor controls use `ImGui::PushItemWidth(150)` / `PopItemWidth()`, so each row reads `[value ▾] Label` instead of a full-width control with a far-right label. Screenshots re-inspected at 1600x900: `statistics.png`, `texture_memory.png`, `mesh_memory.png`, `settings_profiles.png`, `settings_search.png`, `auto_reached.png`.

### Single settings.ini (replaces the GliderData folder)

Every persisted preference now lives in one `settings.ini` next to `Glider.exe`; no
`GliderData/` folder is created. `SettingsFile` (new `header/imgui/SettingsFile.h`,
`source/imgui/SettingsFile.cpp`) owns the ordered sections `[SearchPaths]`,
`[SceneHierarchy]`, `[Resources]`, `[Window]`, `[RecentFiles]`, `[RenderQuality]`,
`[QualityProfiles]` and `[Layout]`. Writers compare each value with its built-in default,
drop empty sections, and `save()` skips identical content, so a pristine session writes no
file and later runs only write real changes. The window/layout baseline is captured three
frames after startup (or after a reset) so ImGui's own normalization is not mistaken for a
user change; Ctrl+L removes the `[Layout]` section immediately.

First launch after an upgrade merges `GliderData/`, `%APPDATA%/Glider`,
`%APPDATA%/Gestaltor` and the pre-GliderData bare files: list sections (`[SearchPaths]`,
`[RecentFiles]`, `[Layout]`, `[QualityProfiles]`) come from the newest source as a whole,
scalar sections fill only missing keys. The consolidated file starts with
`; Glider settings file (schema 1)`; once it exists, legacy files are never read again, so
resetting a value to its default does not resurrect the old one. `GLIDER_DATA_DIR=<dir>`
points the store at `<dir>/settings.ini` and disables migration. An unwritable exe directory
falls back to `%APPDATA%/Glider` then `%TEMP%/Glider`; an unreadable file is left untouched
and changes stay in memory.

Verified with `scripts/validate_settings_storage.py` (all groups pass): a pristine session
writes no file; `--quality-preset low` writes only `[QualityProfiles]`/`[RenderQuality]`
non-defaults; the exe→AppData fallback logs and writes in the new format while leaving the
exe directory untouched; corrupt files warn once and are replaced (or removed when empty);
unreadable files stay byte-identical; `--isolated` neither reads nor writes. Migration from
the existing `build/bin/Release/GliderData` + `%APPDATA%/Glider` layout was run on this
machine: search paths came from the newer GliderData file (`../texture*`), the older bare
`<exeDir>/settings.ini` paths did not shadow them, window state, recent files, layout and
profile values were consolidated, and a second launch left the file byte-identical. Removing
a section from the consolidated file does not re-import it from the legacy folders.

### Heavy-fixture preset refresh (re-run)

`build/review/heavy/heavy.gltf` (115 MB geometry, sixteen 8192x8192 PNGs) was re-run with
`--quality-preset high`, then Low and Ultra were applied through the Settings dialog.
Evidence: `build/review/quality_controls/reload_ui_full.log` / `.json` (959 UI frames,
`failed=false`; `GLIDER_DATA_DIR` pointed at a private `overlay_settings` directory).

- High startup: `texture estimate 1365.3 MiB, budget 7043.1 MiB, maximum dimension 4096`;
  all sixteen images resident at 4096; VRAM 1.96 GiB.
- Use Low: `texture estimate 21.3 MiB ... maximum dimension 512`; sixteen residents at 512;
  the bottom strip showed `Refreshing textures for Low 15/16 Image decode`; VRAM 1.96 → 0.22 GiB.
- Use Ultra: `texture estimate 5461.3 MiB, budget 6982.6 MiB, unlimited (active preset)`;
  the strip showed `Refreshing textures for Ultra 1/16 ... 15/16 GPU upload Unlimited`; VRAM
  reached 5.92 GiB, i.e. the full 8K payload, with no 512/4096 clamp and no extra full-screen
  load screen.
- Both refreshes kept the last presented frame, the docked panels and the status bar visible
  under a bottom strip; the full loading overlay appeared only for the initial model load.
- Window screenshots every 300 ms: `build/review/quality_controls/overlay/frame_000..059.png`
  (`capture_overlay.ps1`: 61 frames, exit 0; `overlay_ui.json`: 238 frames, `failed=false`),
  including `frame_020.png` (Low strip) and `frame_055.png` (Ultra upload, VRAM 5.92 GiB in
  `frame_056.png`+).

## Remaining limitations

- A compressed KTX2 chain without a small enough authored mip can still exceed the selected cap. This is documented rather than silently fixed; a decode-and-resize fallback using `imageformats::decodeKTX2Preview` could change signed normal-map handling and was not implemented. DDS and STB-decoded images do have a generic resize path, and KTX2 trims authored levels when a level at or below the cap exists.
- The generated heavy fixture was re-run for the preset refresh; the maintainer's E195 cockpit/cabin/wings models were measured during the original loading task and were not re-run after this follow-up. Their artifacts remain under `build/review/heavy/`.
- No regression suites were run for the follow-up, at the user's request. The original loading validation results apply to the earlier work only.

## Files and implementation entry points

- `source/imgui/RenderQuality.cpp`, `header/imgui/RenderQuality.h`: profiles, persistence, adaptive controller, effective viewport settings, GPU sample ring and UI editor. `drawProfilesEditor`, `sizeCombo`, `effectiveViewport` are relevant functions.
- `header/imgui/SettingsFile.h`, `source/imgui/SettingsFile.cpp`: the single INI store, legacy migration, schema marker and atomic write/fallback.
- `source/imgui/application/Settings.cpp`, `AppSettings.cpp`: window/recent/layout and search/hierarchy/resource sections of `settings.ini`.
- `source/imgui/SearchPathsPanel.cpp`: Settings tabs and scrolling editor with pinned Close button.
- `source/imgui/AppSettings.cpp`: wildcard expansion (now absolute), cache invalidation and resource resolution.
- `header/imgui/MemoryInfo.h`, `RuntimeTelemetry.h`, `ResourceMemory.h`: OS/process samples, rolling frame metrics, mapped GL payload estimates.
- `source/imgui/StatusBar.cpp`, `StatisticsPanel.cpp`, `ElementInspector.cpp`: display. The VRAM bar and FPS text in the status bar open and focus Statistics (`StatisticsPanel::requestFocus`).
- `source/imgui/application/Frame.cpp`, `Scene.cpp`, `Viewports.cpp`: telemetry, texture-only refresh, toolbar and adaptive status.
- Renderer GL Texture/Buffer classes: resident payload counters; Image decode/resize and compressed mip selection implement texture limits.
- `scripts/check_quality_controls.py`: focused smoke/persistence tests; not a regression runner.

## Documentation

- `AGENTS.md` §3.6 documents the single `settings.ini`, migration, fallback and isolated rules; §4.2 documents the new flags, storage and wildcard rules.
- `docs/QUALITY_PRESETS_AND_TELEMETRY.md` describes presets, Settings, live resize, telemetry, and the KTX2 cap edge.
- `docs/LOADING_AND_ADAPTIVE_QUALITY.md` was corrected: DDS and STB-decoded images resize on CPU, compressed KTX2 uploads trim authored mip levels when possible, and the remaining compressed cap edge is called out.
- The loading design links to the follow-up doc and this journal.

## Build and verification commands

```cmd
cmake --build build --config Release --target Glider > build/review/loading/presets-build.log 2>&1
& C:\ProgramData\anaconda3\python.exe scripts/check_quality_controls.py > build/review/loading/quality-controls-check.log 2>&1
& C:\ProgramData\anaconda3\python.exe scripts/validate_settings_storage.py > build/review/loading/settings-storage-check.log 2>&1
```

Build outputs are redirected rather than piped, so failures still surface in the logs.

## Final checkpoint

- Latest Release build succeeded; `build/bin/Release/Glider.exe` (9:25) is newer than every source edit. Only docs and harness scripts changed after it.
- `scripts/check_quality_controls.py` passes all eleven checks, including private-directory persistence/reset, the no-default-values assertion, and the status-bar VRAM/FPS click opening Statistics.
- `scripts/validate_settings_storage.py` passes pristine, changed-value, fallback, corrupt/unreadable, isolated and optional baseline groups; the portable `settings.ini` was restored byte-identically after the ACL fallback test.
- Heavy fixture Low→Ultra refresh verified: High 1.96 GiB/4096, Low 0.22 GiB/512, Ultra 5.92 GiB/8192 with `unlimited (active preset)`; the scene stayed on screen under the bottom progress strip in all 61 window captures.
- Migration from the real `GliderData`/AppData layout was consolidated into one `settings.ini`; a second launch left it byte-identical and removing a section did not re-import legacy values.
- Probe root cause fixed without reducing coverage; relative-model wildcard path bug fixed and covered by a new case.
- `git diff --check` passed (exit 0; only CRLF normalization warnings).
- Trailing whitespace checked on the untracked documentation files; none found.
- `git status --short` shows only the intended modified sources plus the new untracked headers, docs and harness scripts; the unrelated `scratch/` directory was not touched. Nothing was committed or pushed.
