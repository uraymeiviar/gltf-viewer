# Build & Development Workflows (Glider)

Moved from AGENTS.md §§4.1, 4.3–4.6; original subsection numbers are retained. Flags: [CLI_REFERENCE.md](CLI_REFERENCE.md). Guardrails: [AGENTS.md](../AGENTS.md#5-critical-rules--guardrails-for-ai-agents).

### Proportional testing

For each change, run its own focused check plus the closest 1–3 validators. Run the full tier once per commit for broad changes. Use the baseline/regression workflow in §4.5 below. Broaden or repeat checks only for new changes, failures or unresolved concerns. Documentation-only edits use documentation checks.

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
- **`run_regression.py`** (quick tier) runs the renderer harnesses (`scripts/validate_p8_*.py`,
  `validate_p4_exit_gate.py`; `--jobs`, default 3, at a time), then the feature and ASOBO harnesses one at a time,
  then the built-in self-checks, against that baseline. They check byte-identical captures and identical
  selections. Window screenshots are compared on their viewport area only, so side-panel UI changes do not fail
  them (`validate_p4_exit_gate.py` covers the panels). The script's docstring lists what each suite covers.
  `--only a,b` runs a subset (suite names as printed); `--no-cache` starts every baseline launch.
  On the CPU-limited maintainer setup (12 of 24 cores, `--jobs 1`) the quick tier takes about 43 minutes (2601 s) with an empty
  cache and about 35 minutes (2106 s) warm; the current build's launches (1,092 of 1,332) dominate.
- **Self-checks**: revision, blend, aa-target, cache-invalidation, compute, sampler, VAO, polygon-mode,
  cull/winding and depth; all but cache-invalidation and compute also run with `--selfcheck-force-failure`, which
  must exit 1 with a report.
- **`--tier full`** adds each feature's same-binary A/B switch, `validate_p5_1.py`, and `validate_p8_5.py`'s
  10,000-drawable and MSAA/TAA picking cases.
- **Per-suite summary**: the run ends with each suite's wall time and Glider launches (current build, baseline,
  baseline served from the cache, baseline started, repeated argument sets), slowest first, and writes
  `build/regression/summary.json`. Every launch of a suite is listed in `build/regression/<suite>.launches.jsonl`
  (`GLIDER_HARNESS_LAUNCH_LOG`).
- **Baseline render cache**: a baseline binary never changes for its hash, so `scripts/harness_process.py` keeps what
  each baseline launch wrote in `build/regression_cache/<key>/` and replays it on the next run instead of starting
  the baseline. It applies to every executable under `build/baselines/` and never to the current build or to
  control-pipe sessions (`Popen`). The key is the SHA-256 of the executable, the working directory and exact
  argument list, the content of every argument that names a file (models and every file a model names through a
  `uri`, HDRs, UI scripts and the files their `reload` lines load), the `GLIDER_*` and graphics-driver environment
  variables plus any variable the harness changed, the display adapters' driver versions, system DPI and screen
  size, and how stdout/stderr are captured. Any change is a miss. The value is the output files (`--capture-viewport`,
  `--dump-frame`, `--profile-report`, `--asobo-report`, `--ui-report`, UI-script `shot` files, ...), stdout and
  stderr. Launches that are not fully understood start normally: no `--isolated`, no known output flag, a flag that
  writes something else (benchmark, idle stats, loading series, self-checks), `--search-path` or another directory
  argument, a model URI that does not resolve, UI scripts with commands outside the known set or keyboard shortcuts
  (clipboard), or uncaptured output. A result is stored only after exit code 0, with every output rewritten and the
  executable and inputs unchanged. Entries whose executable is gone or changed are pruned at the start of each run;
  `GLIDER_RENDER_CACHE=0` (or `--no-cache`) turns the cache off.
- **Harness helpers**: `scripts/glider_harness.py` holds the pieces harnesses share (baseline lookup, `run`,
  `capture`, `start_pipe_session`, `gl_errors_zero`, `read_json`, `digest`/`same`, `viewport_sha`); its docstring lists
  the API. Launches go through `harness_process`, so new harnesses get the focus rules and the cache by using it.
- **Harness launches never request keyboard focus**: they go through `scripts/harness_process.py`, which sets
  `GLIDER_NO_FOCUS=1` and starts Glider with `SW_SHOWNOACTIVATE`. Baselines older than the marker may still take
  focus when they are not served from the cache. `scripts/check_no_focus.py` checks the current build.
- **On-demand validators** (not in the regression; fast checks or ones that need special assets; run the one for the
  area you change): `validate_gl46.py`, `validate_live_control.py`, `validate_loading.py`,
  `validate_settings_storage.py`, `validate_tiff_images.py`, `validate_wireframe_overlay.py`,
  `validate_extension_editing.py`, `validate_uri_warnings.py`, `validate_decal_visibility.py`,
  `validate_extreme_emissive.py`, `validate_asobo_base_vertex_picking.py`, `validate_asobo_decal.py`,
  `validate_asobo_primitive_ranges.py`, `validate_asobo_windshield_layers.py`, `validate_pmdg777_overhead.py`
  (uses `validate_msfs2020_materials.py`, also kept), `validate_asobo_glass.py`, `validate_p6_1.py --only reports`
  (validation reports against `--validation-full-read`) and `validate_p5_4.py` (event-driven idle loop).
  `validate_wireframe_overlay.py`, `validate_decal_visibility.py`, `validate_pmdg777_overhead.py` and
  `validate_asobo_glass.py` compare some images against pinned older binaries and fail those comparisons since
  later intended render changes (glass: the HDR background projection fix `0e4fcfc`); their own feature checks pass.
  `validate_p5_4.py`'s idle-CPU comparison is noise-limited on a loaded machine; its frame-count checks pass.
- **Retired** (refactor stage 3, see git history): the one-off phase checks `validate_p3_*.py`, `validate_p4_1.py` …
  `validate_p4_7.py`, `validate_p5_2.py`, `validate_p5_3.py`, `validate_p5_5.py`, `validate_p6_2.py`, `validate_p7.py`,
  `validate_a2_1.py`, `validate_aa_split.py` and `validate_hdr.py`; the regression suites above cover their features.
- **Benchmarks** never run here. They back timing claims only: pass `--bench` to the harness concerned, and do not
  run them in parallel.

### 4.6 Third-party code

There are no git submodules. GLM is vendored as headers in `thirdparty/glm` (version in its README); the other
libraries are fetched by CMake `FetchContent` (`cmake/standalone_dependencies.cmake`). The glTF, Khronos and ASOBO
JSON schemas live in `resources/schemas`. The UX3D libraries' own unit tests (`UX3D_RUNTIME_TESTING`,
`UX3D_TOOLS_TESTING`) and the editor's `Operations` library are not built.
