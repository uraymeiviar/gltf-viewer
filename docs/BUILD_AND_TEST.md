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
- **`run_regression.py`** runs the selected suites against that baseline: the renderer harnesses
  (`scripts/validate_p8_*.py`, `validate_p4_exit_gate.py`; `--jobs`, default 3, at a time), then the feature and
  ASOBO harnesses one at a time, then the built-in self-checks. They check byte-identical captures and identical
  selections plus each feature's analytic checks. Window screenshots are compared on their viewport area only, so
  side-panel UI changes do not fail them (`validate_p4_exit_gate.py` covers the panels). The script's docstring
  lists what each suite covers. `--only a,b` runs exactly those suites of the tier (names as printed);
  `--no-cache` starts every baseline launch.
- **Tiers**: `--tier full` runs every suite except the on-demand ones (one full run per commit for broad changes;
  about 33 minutes on the CPU-limited maintainer setup, 12 of 24 cores, `--jobs 1`, before the capture sessions and
  on-demand suites of 2026-11-01; an estimated 26 minutes since). The **quick tier** (default) runs the **core** —
  `p8_1`, `p4_exit_gate`, `asobo_common`, `defaults` (about 57 s) and the self-checks; 139 s warm before
  `defaults` joined, 288 s measured with `p8_1`'s baseline cold — plus the suites that the changed
  files **trigger**. Changed files are `git diff --name-only HEAD` plus untracked files under `source/`, `header/`,
  `resources/`, `OpenGL-Renderer/` and `scripts/`; `--changed a,b` replaces them (`--changed ""`: nothing changed).
  The trigger table is `TRIGGERS` in `run_regression.py` (file-name globs per suite group: picking/instancing,
  sun/shadows, ground/ocean, SSAO/ambient/sky occlusion, sky/sun disk/night/time of day, ASOBO/textures,
  settings/UI/control). `docs/`, `scratch/`, `legacy/`, Markdown outside `resources/` and top-level files other than
  `CMakeLists.txt` select nothing. A suite's own `scripts/validate_<suite>.py`, or a script it imports or names
  (recursively; `harness_process.py` and `glider_harness.py` therefore select almost everything), triggers it; a
  script no suite uses and `run_regression.py` itself select only the core. `BROAD` paths (`pbr.frag`,
  `primitive.vert`, `ibl.glsl`, `brdf.glsl`, `Application.*`, `Frame.cpp`, `Scene.cpp`, CMake, `thirdparty/`,
  `HelloCube.glb`, the default HDR) select every suite. A `resources/` file outside the shaders triggers the suites
  whose scripts name it. **Any other file that matches no trigger selects every suite** and prints
  `WARNING: <path> matches no trigger`, so the table can be extended.
- **`--plan`** prints the changed files, the selected suites and why (core, triggered by which paths, full tier,
  `--only`) and the suites left out, then exits without launching anything (no baseline or build needed):
  `python scripts/run_regression.py --plan --changed source/imgui/SkyEnvironment.cpp`.
- **`GLIDER_CANDIDATE_EXE=<path>`** (relative to the repository) replaces `build/bin/Release/Glider.exe` as the
  candidate in `run_regression.py`, `glider_harness.EXE` and every script that used to hardcode it
  (`harness_process.candidate_exe()`). Use it to prove that a known-bad binary from `build/baselines/` fails a suite.
  The candidate has role `current`, so it is never cached; with the variable set, every cache key also holds the
  candidate's SHA-256, so nothing stored in a run against one candidate is replayed for another.
- **Self-checks**: revision, blend, aa-target, cache-invalidation, compute, sampler, VAO, polygon-mode,
  cull/winding and depth; all but cache-invalidation and compute also run with `--selfcheck-force-failure`, which
  must exit 1 with a report.
- **`--tier full`** adds each feature's same-binary A/B switch (for `validate_p8_5.py` also `--always-pick` on the
  former P5.1 cases) and `validate_p8_5.py`'s 10,000-drawable and MSAA/TAA picking cases.
- **Shared defaults suite** (`validate_defaults.py`, core): renders HelloCube (1280x720, AA none) and the E195
  `exterior_fuselage_nose` / `cockpit_main_displays` reference views once per binary and runs every comparison
  that sun, shadows, ground, ssao, time_of_day, sky, sun_disk, night, ambient, sky_occlusion, ocean, sun_source and
  asobo_sky_portal made against those baseline renders — including their "settings changed while the feature is
  off" variants, rendered with the suite's own helper and flags — with the same wording prefixed by the suite name
  (`FAIL night: HelloCube (night_settings): byte-identical to <baseline>`). Those suites no longer render them;
  comparisons against any other baseline render (fixtures, sun or plane on, procedural sky, explicit
  `--environment hdr` in `validate_emissive_units.py`) stay in the suite.
- **On-demand parts** (`ON_DEMAND` in `run_regression.py`): slow sections that are acceptance evidence rather than
  equivalence run only with their suite flag. `night --sweep`: the two evening exposure sweeps (Curve and Auto,
  5-minute steps) and the minute-by-minute emissive day/night blend (about 90 samples; the default run places about
  17 samples by sun elevation: both sides of the band, 11 inside it, two bracketing the threshold).
  `area_maximize --keys`: the whole keys-and-menus section (the default run keeps one Ctrl+Space round trip over the
  viewport). `sky_occlusion --da62`: the DA62 SDK cockpit in the real-asset bake checks (its HDR-default
  byte-identity check always runs). The quick tier passes the flag when a changed path matches the part's
  patterns (taken from the suite's trigger group: night `NightSky.*`, `SkyEnvironment.*`, `EnvironmentState.cpp`,
  `tonemapping.glsl`; area_maximize `AreaLayout.cpp`, `CommandsArea.cpp`, `Menus.cpp`, `Application.cpp`;
  sky_occlusion `SkyOcclusion.*`, `sky_occlusion*`; each also its own `validate_<suite>.py`); `--on-demand a,b`
  selects those suites with their flags in any tier, `--with-on-demand` passes every flag of the selected suites.
  `--tier full` does not run them otherwise. `--plan` lists which flags are passed and why.
- **On-demand suites** (`ON_DEMAND_SUITES`): `p8_followup` (a same-binary A/B of shared traversal; `p8_1`'s split
  case compares the multi-viewport image with the baseline), `texture_dds`, `texture_ktx2`, `asobo_model`,
  `asobo_support`, `asobo_normal` and `asobo_occlusion` (phase acceptance of one or two commits; `asobo_common`'s
  E195 report covers the parsing) are not in `--tier full`. They keep their triggers, so the quick tier runs them
  when their paths or their own scripts change; `--on-demand <suite>` or `--with-on-demand` (with `--tier full`:
  every suite and every on-demand part) selects them in any tier. `--plan` lists each as
  `on-demand suite: <name> selected (...)` or `... not run`.
- **Capture sessions** (`glider_harness.capture_run`): the fixture captures of `asobo_uv_options`,
  `asobo_decal_modes`, `asobo_dither`, `asobo_draworder`, `asobo_fresnel_ghost` and `asobo_occlusion` go through
  one helper that takes the capture's CLI argv. For the candidate, a capture whose flags the session can replay
  runs in a control-pipe session started with the same startup flags (started at the second consecutive capture
  with those flags, one Glider at a time, closed before any other Glider starts): `open_model <fixture>
  startup_state=true` restores the state a fresh start with those flags gives (the `reset` state plus the replayed
  `--select-node` / `--camera-*` flags), `--debug-view`, `--asobo-disable`, `--asobo off`, `--set-asobo-param`,
  `--windshield-icing` and `--without-textures` go through the registry, then `capture` settles the remaining
  frames. Every session capture of those suites was hashed against its CLI launch (byte-identical, 185 of 185 on
  2026-11-01). Baseline launches, the candidate's side of every baseline comparison, captures whose profile report
  or launch log is read, and flags outside the replayable set stay CLI launches. `asobo_anisotropy` and
  `asobo_sss` are routed through the helper but keep CLI launches (`SESSIONS = False`: identical, but small `--env`
  launches are not slower than a session). `asobo_normal` runs in sessions too and opens missing -> DirectX ->
  OpenGL in one Glider of its own, requiring the OpenGL capture to equal its CLI launch (up to dc456a6
  `Gltf::reset` kept the previous file's asset extensions, so the DirectX green flip leaked). `GLIDER_HARNESS_SESSIONS=0` turns
  sessions off; `GLIDER_HARNESS_SESSION_PROOF=<file>` launches each session capture's CLI argv as well when the
  harness exits and appends one JSON line per capture (session hash, CLI hash, equal) to `<file>`.
- **GPU / CPU timing checks** never compare a measured time with an absolute limit in the regression (the machine
  and its GPU are shared). The procedural sky's scheduling runs under the test hook `GLIDER_TEST_SKY_FIXED_COST`
  (constant cost per unit, [CLI reference](CLI_REFERENCE.md#test-hooks--environment-variables)): swaps and fast
  jobs must equal the baseline's under the same hook (a baseline without the hook: ≥ 40), with no incomplete swap
  and no planned frame above the stated 4 ms. Measured times (sky extra GPU per frame while time plays and after it
  stops, ocean background slices, sky-occlusion largest bake frame) are compared with the baseline measured in the
  same conditions: interleaved candidate, baseline, candidate, baseline (ocean: three pairs), never cached
  (`harness_process.uncached()`), the best run of each: candidate ≤ 1.25 × baseline + 0.5 ms
  (`glider_harness.relative_timing`). The absolute limits (sky 4 ms and no frame over, ocean median 3 ms / largest
  4.5 ms, sky occlusion 200 ms per bake frame) and the sky-occlusion lookup's per-frame cost run with each harness's
  `--bench`.
- **`validate_p5_1.py` is superseded** (not registered): its candidate counters — no unserved click, one picking
  render per viewport click, no picking render on idle frames — plus the focus double-click, resize clicks,
  split-viewport node check and `--always-pick` A/B run in `validate_p8_5.py`.
- **`meshopt` suite** (`validate_meshopt.py`; triggers: the meshopt sources, the glTF buffer/bufferView/JSON loader
  files, `Gltf.cpp`, `Extensions.*`, `GltfValidator.*`, `meshopt_*`): runs `build/tools/Release/meshopt_fixture_gen.exe`
  into `build/meshopt/fixtures` (EXT and KHR GLBs, a `.gltf` with a uri-less fallback buffer, a used-not-required
  file with an uncompressed fallback, a damaged stream; about 360 KB, generated, never committed) and requires every
  compressed file to render byte-identical to its plain twin (same decoded bytes, no extension), the damaged and
  the `GLIDER_TEST_DISABLE_MESHOPT=1` required files to be refused with their error, the validator notices for
  unsupported required/used extensions, and the plain twins to equal the baseline.
- **Per-suite summary**: the run ends with each suite's wall time and Glider launches (current build, baseline,
  baseline served from the cache, baseline started, repeated argument sets, control-pipe/follow sessions and their
  summed seconds), slowest first, and writes `build/regression/summary.json` (with the candidate and why each suite
  was selected). Every launch of a suite is listed in `build/regression/<suite>.launches.jsonl`
  (`GLIDER_HARNESS_LAUNCH_LOG`); `Popen` sessions are logged when the process exits, with their real duration and
  exit code.
- **Baseline render cache**: a baseline binary never changes for its hash, so `scripts/harness_process.py` keeps what
  each baseline launch wrote in `build/regression_cache/<key>/` and replays it on the next run instead of starting
  the baseline. It applies to every executable under `build/baselines/` and never to the current build or to
  control-pipe sessions (`Popen`). The key is the SHA-256 of the executable, the working directory and exact
  argument list (an output path counts only by position and suffix, so the same launch writing elsewhere shares
  the key; a replay copies the files to the new paths and rewrites the stored paths in reports and stdout), the content of every argument that names a file (models and every file a model names through a
  `uri`, HDRs, UI scripts and the files their `reload` lines load), the `GLIDER_*` and graphics-driver environment
  variables plus any variable the harness changed, the display adapters' driver versions, system DPI and screen
  size, and how stdout/stderr are captured. Any change is a miss. The value is the output files (`--capture-viewport`,
  `--dump-frame`, `--profile-report`, `--asobo-report`, `--ui-report`, UI-script `shot` files, ...), stdout and
  stderr. Launches that are not fully understood start normally: no `--isolated`, no known output flag, a flag that
  writes something else (benchmark, idle stats, loading series, self-checks), `--search-path` or another directory
  argument, a model URI that does not resolve, UI scripts with commands outside the known set or keyboard shortcuts
  (clipboard), or uncaptured output. A result is stored only after exit code 0, with every output rewritten and the
  executable and inputs unchanged. Entries whose executable is gone or changed, and the entries of all but the 4
  most recently used executables (`GLIDER_HARNESS_CACHE_EXES`), are pruned at the start of each run;
  `GLIDER_RENDER_CACHE=0` (or `--no-cache`) turns the cache, the memo and seeding off.
- **In-run memo** (current build): within one `run_regression.py` invocation, a candidate launch that the cache
  rules accept (same eligibility, same key, which holds the candidate's SHA-256) is stored in
  `build/regression_memo/run-<pid>-<time>/`; an identical launch later in the run, in any suite, replays it. The
  directory is deleted when the run ends, so nothing crosses invocations or executables. A check that launches the
  same arguments twice to prove determinism (ocean's clock and live runs, sun's default-HDR detection) wraps the
  second launch in `harness_process.fresh()`. The summary's `memo` column counts replays; `--no-memo` turns it off.
- **Baseline seeding**: each stored memo entry is also copied to `build/regression_cache/` under its key, marked
  seeded, unless an entry exists (not with `GLIDER_CANDIDATE_EXE`). `save_baseline.py` copies the same bytes after
  the commit, so the first run against the new baseline replays them (`seed` column, log `hit, seed`) instead of
  starting the baseline; the entry then becomes an ordinary baseline entry. A rebuilt exe simply misses.
- **Harness helpers**: `scripts/glider_harness.py` holds the pieces harnesses share (baseline lookup, `run`,
  `capture`, `capture_run`, `start_pipe_session`, `gl_errors_zero`, `read_json`, `digest`/`same`, `viewport_sha`); its docstring lists
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
libraries are fetched by CMake `FetchContent` (`cmake/standalone_dependencies.cmake`). meshoptimizer (`v1.3`, MIT;
EXT/KHR_meshopt_compression) is fetched there too: only its three codec sources are compiled (`meshopt_codec`,
decoders used by Glider), plus the dev-only fixture writer `meshopt_fixture_gen` (`scripts/meshopt_fixture_gen.cpp`,
output `build/tools/Release/`), which the Glider target builds as a dependency and which is never shipped. The glTF, Khronos and ASOBO
JSON schemas live in `resources/schemas`. The UX3D libraries' own unit tests (`UX3D_RUNTIME_TESTING`,
`UX3D_TOOLS_TESTING`) and the editor's `Operations` library are not built.
