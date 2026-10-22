# Phase 0 — Baseline and Instrumentation

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Complete. 

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-2"></a>

### 5.2 Phase 0 — Reproducible Baseline and Instrumentation (R0)

**Entry:** no prerequisites. This phase enables measurement; it is not itself a performance win.

- [x] **P0.1 — Deterministic scenarios.** Record model hashes, camera/projection, window and viewport
  sizes, DPI, layout, settings, animation time and environment. Add explicit scenario controls where
  existing CLI/UI cannot reproduce them. Isolate test settings from the user's persisted layout.
  **Evidence:** [§12.1](#section-12-1).
- [x] **P0.2 - CPU counters/timers.** Measure load/decode/validation/IBL separately and frame stages:
  dirty synchronization, animation/transforms/skins, scene construction, picking, command generation,
  draw submission, overlays, AA and UI. Count GL calls, VAO creations, uploads, traversal/cache rebuilds
  and allocation bytes where relevant. Keep logging outside timed inner loops.
  **Evidence:** [§12.2](#section-12-2).
- [x] **P0.3 — GPU timings.** Add non-overlapping pass timer queries, collecting results several frames
  later only when available. Do not introduce `glFinish` or blocking same-frame query reads. Record
  missing results as unavailable. Label CPU submission time separately from GPU execution time.
  **Evidence:** [§12.3](#section-12-3).
- [x] **P0.4 — Benchmark controls.** Add explicit vsync control and a bounded benchmark mode with warmup,
  measured-frame count and machine-readable output. Implemented controls are documented in [§12.4](#section-12-4).
  **Evidence:** [§12.4](#section-12-4).
- [x] **P0.5 — Capture and baseline bundle.** Support reproducible PNG captures at specified frame/time
  and a fixed viewport region; establish same-build image noise and timing variance. Record first frame
  separately from steady state. Measure instrumentation-on/off overhead.
  **Evidence:** [§12.5](#section-12-5).

**Exit gate:** repeated baseline runs are reproducible, actual measured frame counts are recorded,
GPU samples do not stall collection, and an agent can compare two builds without relying on FPS by eye.

Until this passes, allow source investigation but do not claim measured optimization gains.

**Phase 0 status:** MET at the corrected build. See [§12.6](#section-12-6) for the post-review correction evidence and gate
statement; [§12.5](#section-12-5) retains the original pre-review baseline evidence.

<a id="section-12-1"></a>

### 12.1 P0.1 — Deterministic scenarios

```text
Task ID / title: P0.1 — Deterministic scenarios (R0, Phase 0)
Status: PASS
Source state: base revision 5d009b2b1f7ab63c17130c1e22e88ff7f3a59857
  source/imgui/main.cpp modified (SHA-256 61D6DF121FA64A258CA59335D1F1245479D7FE3D79CCFEB5F8F8F3EEB358666C)
  AGENTS.md pre-existing user modification preserved; docs/RENDERER_OPTIMIZATION_JOURNAL.md pre-existing untracked
  candidate executable SHA-256 4BD5D8C8F3EEEC2ED50FD15866552D949219DE95447153F1C021988321A350BD
  build: cmake --build build --config Release --target Glider (MSVC Release, x64)
Dependencies and gates satisfied: Phase 0 entry has no prerequisites. No renderer optimization depends on this record.
Hypothesis / mechanism: fixed window size, explicit camera state and isolated persistence make the
  eight-frame capture state reproducible; a manifest written on the capture frame records the actual
  state instead of relying on invocation text. Isolated runs must not read or write user window,
  layout, recent-file or settings state.
Primary metric / predeclared thresholds / unaffected control:
  Exact SHA-256 equality of the scenario report and PNG across two repeated runs at the same build
  and machine (zero tolerance for lossless state); all required manifest fields present; no writes to
  persisted user state. Control: a non-isolated run in a temporary CWD must still honor a copied
  imgui.ini while an isolated run forces the default layout.
Environment: Windows 11 Pro 10.0.22631 x64; NVIDIA GeForce RTX 5080/PCIe/SSE2; NVIDIA driver 616.92;
  OpenGL 4.1.0; content scale 1.0; Release build. Other hardware/drivers were not tested.
```

#### Accepted CLI and manifest

New or extended controls used by P0.1:

| Flag | Behavior |
| :--- | :--- |
| `--isolated` / `--no-persist` | Do not load/save `window_state.txt`, `recent_files.txt`, `settings.ini` or `imgui.ini`; force the default dock layout; redirect any settings save to the temporary directory. |
| `--window-size WxH` | Override the client size before GLFW window creation; suppress window-state persistence for that run so the user's saved size is not clobbered. |
| `--scenario-report PATH` | Write the `glider.scenario.v1` manifest on frame 8, matching the existing `--dump-frame` countdown. |
| `--scenario-name NAME` | Stable scenario identifier embedded in the manifest. |
| `--camera-yaw DEG` / `--camera-pitch DEG` | Orbit orientation in degrees. |
| `--camera-distance N` | Orbit distance/zoom from the current target. |
| `--camera-target X,Y,Z` | Orbit target in world space. |
| `--camera-fov DEG` | Perspective vertical FOV override; retained if orthographic is selected. |
| `--camera-projection perspective\|orthographic` | Select the projection type after model load and node framing. |

The manifest records the model container hash and byte count, environment hash and IBL settings,
OpenGL/build strings, client/framebuffer/content-scale values, active viewport FBO size and samples,
camera type/eye/target/up/yaw/pitch/zoom/FOV/near/far/view/projection matrices, viewport settings,
animation time/play state/clip index, selection and layout mode. Hashes are explicitly labeled
`"hashAlgorithm": "fnv1a64"` and `"hashScope": "container-file"`.

Exact commands (run from the repository root; output directory
`G:\temp\uray-codex\opencode\glider-p01\`; run B changes report/frame suffixes):

```cmd
:: Build
cmake --build build --config Release --target Glider

:: S0 HelloCube, 1280x720, MSAA4
cmd /c "build\bin\Release\Glider.exe --isolated --window-size 1280x720 --scenario-name s0-hellocube --aa msaa4 --camera-yaw 30 --camera-pitch 20 --camera-distance 3 --scenario-report G:\temp\uray-codex\opencode\glider-p01\report_a.json --dump-frame G:\temp\uray-codex\opencode\glider-p01\frame_a.png --exit-after-dump"

:: S4-like Fox, animation seek
cmd /c "build\bin\Release\Glider.exe OpenGL-Renderer\Tools\tools\operations\tests\resources\Fox\glb\Fox.glb --isolated --window-size 1024x768 --scenario-name s4-fox-time --aa none --anim-time 0.75 --scenario-report G:\temp\uray-codex\opencode\glider-p01\report_fox_a.json --dump-frame G:\temp\uray-codex\opencode\glider-p01\frame_fox_a.png --exit-after-dump"

:: S0 TAA
cmd /c "build\bin\Release\Glider.exe --isolated --window-size 800x600 --scenario-name s0-hellocube-taa --aa taa --camera-yaw 30 --camera-pitch 20 --camera-distance 3 --scenario-report G:\temp\uray-codex\opencode\glider-p01\report_taa_a.json --dump-frame G:\temp\uray-codex\opencode\glider-p01\frame_taa_a.png --exit-after-dump"

:: S1-like Duck, orthographic control
cmd /c "build\bin\Release\Glider.exe OpenGL-Renderer\Tools\tools\operations\tests\resources\Duck\glTF-Binary\Duck.glb --isolated --window-size 960x640 --scenario-name s1-duck-ortho --aa fxaa --camera-yaw 45 --camera-pitch 30 --camera-distance 2 --camera-target 0,0.5,0 --camera-fov 60 --camera-projection orthographic --scenario-report G:\temp\uray-codex\opencode\glider-p01\report_duck_ortho.json --dump-frame G:\temp\uray-codex\opencode\glider-p01\frame_duck_ortho.png --exit-after-dump"
```

Layout-isolation control (run from a temporary CWD containing a copied `imgui.ini`): the same
command without `--isolated` loads the persisted layout; adding `--isolated` forces the default layout
and leaves the temporary `imgui.ini` untouched.

#### Repeatability results

Two runs under identical arguments, same executable and machine. The report and PNG hashes are
SHA-256; model/environment hashes are FNV-1a 64.

| Scenario | Asset (FNV-1a 64 / bytes) | Window / viewport | AA / camera / animation | Report SHA-256 | PNG SHA-256 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| S0 HelloCube run A | `resources/sampleGltf/HelloCube.glb` (`0x37c8a14a177e01ac`, 29,952) | 1280×720 / 734×616, 4 samples | `msaa4`; yaw 30°, pitch 20°, distance 3; time 0 | `42D86BBBFA83F7D4AF5F28113D20A04B5F106C016517FD9F7E826297EF95DDF6` | `06286C6FED2E7A20EE307D20FF2A5F5972FD7AD72FEBB77AE31523442D72D7C5` |
| S0 HelloCube run B | same | same | same | `42D86BBBFA83F7D4AF5F28113D20A04B5F106C016517FD9F7E826297EF95DDF6` | `06286C6FED2E7A20EE307D20FF2A5F5972FD7AD72FEBB77AE31523442D72D7C5` |
| S4-like Fox run A | `Fox.glb` (`0xd196432e1adedf81`, 162,852) | 1024×768 / 586×664, 1 sample | `none`; default fit; time 0.75 of 3.416667 | `65782AFAAD828F1AB714DA15E5DE9FE93F5D198CA96707F25E1C10E15B0B1138` | `C165576F737946445C42BC8C3F7CD31D196C6C7F278E082BC04574DABCA351D2` |
| S4-like Fox run B | same | same | same | `65782AFAAD828F1AB714DA15E5DE9FE93F5D198CA96707F25E1C10E15B0B1138` | `C165576F737946445C42BC8C3F7CD31D196C6C7F278E082BC04574DABCA351D2` |
| S0 HelloCube TAA run A | HelloCube | 800×600 / 456×496, 1 sample | `taa`; yaw 30°, pitch 20°, distance 3 | `9D4969A0C23EB66DD414F8353C44AC43ED9C039D74ABE535E3DFEDBCF808FE9F` | `6145A713A34E407955897B93C75E6E608A94FED94F3751E3E0C71B61B99577A8` |
| S0 HelloCube TAA run B | same | same | same | `9D4969A0C23EB66DD414F8353C44AC43ED9C039D74ABE535E3DFEDBCF808FE9F` | `6145A713A34E407955897B93C75E6E608A94FED94F3751E3E0C71B61B99577A8` |
| S1-like Duck orthographic | `Duck.glb` (`0xca1bca34ed7ef291`, 120,484) | 960×640 / 549×536, 1 sample | `fxaa`; orthographic; FOV 60°, target 0,0.5,0, distance 2, yaw 45°, pitch 30° | `7C709A5B5B4978F35237C2287CE8B15EAFB76CA4566E1C5A164786E2F93EF701` | `182B14250F8F039413D22F73D24A53C642200C240D5007AA6B6C058C95C98B98` |

The Duck orthographic path was a one-off control for the new projection flag, not a determinism pair.
A perspective-FOV control (`s0-fov45`, HelloCube, 640×480, `--camera-fov 45`) reported
`"type": "perspective"` with `yfovRadians = 0.785398185`; report
`5212196278641DA536B45EFA1619AB309B23C3416E66ADC88A8778CE31F7868F`, PNG
`FB11182E7991669F54DBC8752493109513490C55FB1063CAC37625DE2A59CC2B`.

Common environment hash: `resources/environments/footprint_court.hdr`
(`0x1c7d36e6e0290da3`, 6,050,193 bytes), IBL enabled, rotation 0, intensity 1, blur 0.
Noise: 0 bytes for all paired report/PNG comparisons. No timing metric was collected or claimed;
P0.1 is instrumentation, not a performance result.

Instrumentation overhead: the steady-state hook is one frame-counter increment and a short-circuit
condition per frame. The manifest hashing, scene-bounds query and file I/O happen once on frame 8 after
the scene has rendered, outside any timed loop. No per-frame timers or memory counters were added; those
remain P0.2/P0.3.

#### Isolation and control checks

| Check | Result |
| :--- | :--- |
| User state before/after all isolated runs | `%APPDATA%\Glider\recent_files.txt` (302 bytes, 2026-10-20 16:02:27) and `window_state.txt` (59 bytes, 16:13:35) unchanged; workspace `imgui.ini` `494BDBD7…FB684`, `settings.ini` `4D71B956…6E746`, build-tree `imgui.ini` `F13AA0AE…D6385`, `settings.ini` `CE65F243…2FF5` all unchanged. |
| Persisted-layout control | Temp CWD containing a copied `imgui.ini`: non-isolated run reported `layout: "persisted"`, viewport 598×616, PNG `E774F1A9…085A`; isolated run reported `layout: "default-forced"`, viewport 734×616, PNG `2A326134…9A98`. The isolated run left the temp `imgui.ini` hash `B317DEEA…6FCD` and timestamp unchanged. |
| Standalone resources | All runs used the embedded VFS path; no adjacent `resources/` folder was required. |

#### Visual review

The following PNGs were opened and inspected, not only hashed:

- `G:\temp\uray-codex\opencode\glider-p01\frame_a.png` — S0 HelloCube, MSAA4, fixed window/camera; default dock layout and expected PBR/IBL scene.
- `G:\temp\uray-codex\opencode\glider-p01\frame_fox_a.png` — Fox at animation time 0.75; pose visible and stable.
- `G:\temp\uray-codex\opencode\glider-p01\frame_duck_ortho.png` — Duck in orthographic projection with FXAA; projection and framing respond to the new flags.
- `G:\temp\uray-codex\opencode\glider-p01\frame_taa_a.png` — TAA at frame 8; stable across the paired run.
- `G:\temp\uray-codex\opencode\glider-p01\frame_fov45.png` — perspective FOV 45° control; narrower frame and correct projection.

No visual regression was observed in these scenarios.

#### Coverage gaps / limitations

- The manifest records the **active** viewport's full camera/settings. Other open viewports are not yet
  enumerated; S6 (two or more viewports) was not exercised because the current CLI cannot create a blank
  second viewport headlessly. Extend the manifest before capturing S6 baselines.
- `hashScope: "container-file"` hashes only the top-level `.glb`/`.gltf` file. External buffers/images
  referenced by a `.gltf` are not included; record those separately for such fixtures.
- FNV-1a 64 is an identity checksum, not a cryptographic digest. It is adequate for repeatability
  bookkeeping, not for tamper detection.
- The report is emitted at the fixed frame 8. Configurable capture frame/time and fixed viewport-region
  capture remain P0.5, not P0.1.
- Vsync is not yet controlled or recorded; explicit vsync/benchmark controls remain P0.4.
- `layout` records the persistence mode (`persisted`/`default-forced`) and the resulting viewport size,
  not every dock-node rectangle. `--isolated` is the supported way to make the layout deterministic.
- `--window-size` alone does not force the default layout; use `--isolated` for layout determinism.
- Repeatability was established on one machine/GPU/driver. Cross-driver or cross-machine pixel identity
  is not claimed.
- Release only. No Debug/GL-check run was needed for this instrumentation-only task, but run diagnostics
  before accepting renderer changes.

#### Decision and rollback

P0.1 passes its task-level gate: repeated single-viewport scenarios are byte-reproducible, required
state is recorded, explicit controls cover window/camera/projection, and isolated runs do not touch the
user's persisted state. The Phase 0 exit gate is **not** met because P0.2 (CPU timers), P0.3 (GPU
timings), P0.4 (benchmark controls) and P0.5 (capture bundle) remain open.

Rollback boundary: the new state/report/camera code in `source/imgui/main.cpp` only. No renderer or
resource files were changed. Removing the P0.1 flags and the frame-8 report hook restores the prior
behavior.

Next tasks: P0.2, then P0.3/P0.4/P0.5. Do not use this record to claim a rendering speedup.

Evidence bundle: `G:\temp\uray-codex\opencode\glider-p01\` (reports, PNGs, `isolated_cwd` control).

---

<a id="section-12-2"></a>

### 12.2 P0.2 - CPU counters/timers

```text
Task ID / title: P0.2 - CPU counters/timers (R0, Phase 0)
Status: PASS (task-level instrumentation)
Source state: working tree with P0.1 plus the P0.2 edits below; no commit.
  source/imgui/main.cpp (SHA-256 398B991A157949E3C6A3107BFB9F36C864A7B102A3AB9545E6FEF921B3E8C992)
  source/imgui/Profiling.cpp (439D8B1C4DD32D14C04AA144CA875B4FB9B584A559FCE87F7055DE7CE639607C)
  header/imgui/Profiling.h (7E8414F47EA9260FEDA456B11E607E5C0F9F5CA4C5D33F47AB9735F646BB4BE9)
  source/imgui/GladGL.cpp (C33E5519D13FB8635D9E5E01C378B88894065BEC65D85F2D457D94FF8C170DC7)
  source/imgui/GridRenderer.cpp (41B7F0C7A82AE4A2DD36E55C6D62725C0150634E095EE234B4AAACB435485369)
  source/imgui/BoundingBoxRenderer.cpp (06E664A17525858F73AF48471F2563AE8D04ED1C3BE16DD890B831ED25C4BCB7)
  source/imgui/AntiAliasing.cpp (C7AE1BD946C7B1DC9B3F2047334A98B04FEB1A2271AB68977F4AEB2DCC611199)
  source/imgui/Framebuffer.cpp (A52D8257B5BFE2DF9576848171A4F33BCB4030C81C82E61208AC08DAAA955FFF)
  OpenGL-Renderer/renderer/source/gltf/GltfRenderLayer.cpp (0A73C4B778C7611A6A02010427AB0D0EB84FC1D8BE38208874113DAA9131AC28)
  OpenGL-Renderer/renderer/include/renderer/gltf/GltfRenderLayer.h (6F83EE8A79079081143BDA07ECC668FF23A7A6C86637D47D1269A096ABA7CA44)
  CMakeLists.txt (B94C0319B1F6928675E787A9637114B954C27E2431560992BCB926804FF30049)
  build: cmake --build build --config Release --target Glider (MSVC Release, x64)
  executable SHA-256 A079D5DAB78665429519D0EA79CF547F20B46569607FF1FED78EB5978A355467
Dependencies and gates satisfied: Phase 0 entry; P0.1 scenario controls and frame-8 report are in place.
  This record adds observation only; it does not claim a rendering speedup and does not implement P1+.
Hypothesis / mechanism: the frame cost can be separated into named CPU stages and deterministic GL/resource
  counters if timers wrap the existing stage boundaries and counting is centralized in the renderer's
  gl::GL wrapper, with direct application GL calls counted explicitly. The report must state the counter
  scope rather than imply a driver-level trace. Detailed counting and stage timers are opt-in.
Primary metric / predeclared thresholds / unaffected control:
  Task-local acceptance used for the control runs: steady-state counters identical across repeated S0
  runs; CPU frame time within 5% of the run median; named steady-stage samples within 30% of the run
  median (or within 0.01 ms absolute for sub-0.05 ms stages); first frame recorded separately; PNG
  unchanged across repeats; no logging inside a timed loop. Controls: Fox animation seek and Duck
  orthographic projection, which must still render correctly and exercise different node/draw counts.
Environment: Windows 11 Pro 10.0.22631 x64; NVIDIA GeForce RTX 5080/PCIe/SSE2; NVIDIA driver 616.92;
  OpenGL 4.1.0; content scale 1.0; Release build.
```

#### Implemented instrumentation

- `header/imgui/Profiling.h` / `source/imgui/Profiling.cpp` define the CPU sample model and the
  `glider.profile.v1` writer. Detailed instrumentation is disabled by default. `--profile` enables
  it; `--no-profile` disables it; `--profile-report PATH` enables it and writes the report on frame 8;
  `--exit-after-profile` exits after that write.
- Startup fields are measured separately: embedded-VFS decompression, environment init, environment
  filtering, glTF load/decode, renderer upload, validation wall time and the validator's own reported
  duration, plus a total startup time. Model/environment identity remains the P0.1 FNV-1a/manifest
  contract.
- Per-frame stages: `animation.update`, `ui.newFrame`, `ui.chrome`, `scene.dirtySync`,
  `scene.updateGltf`, `scene.updateScene`, `scene.applyAnimation` (`animation.channels`,
  `animation.transforms`, `animation.skins`), `scene.create`, `picking.build`, `scene.build`,
  `commands.generate`, `draw.submit`, `scene.wireframeOverlay`, `overlays.gridBbox`, `aa.render`,
  `ui.panels`, `ui.statusBar`, `ui.render` and `ui.submit`. Startup and frame-8 report writing are
  outside these timed loops.
- `GladGL` counts every call through the renderer's `gl::GL` wrapper (105 wrappers). It additionally
  classifies draw calls, buffer/texture/renderbuffer/framebuffer/VAO/shader/program creations, buffer
  allocations and uploads, and texture allocations and uploads. The application's direct GL paths
  (scene framebuffer, AA, grid, bounding boxes) increment explicit counters; grid and bounding-box
  draws are classified as application draws.
- `GltfRenderLayer` exposes the node/primitive/drawable traversal counts collected by `createScene`
  and sub-stage times for channel update, transform hierarchy application and skin joint computation.
- The report labels the counter scope: `glCalls` means wrapper calls, not a driver/API trace; uploads
  and allocations are the wrapper requests plus explicit application operations.

#### Startup measurements

All times are milliseconds; S0 is embedded `HelloCube.glb`, 1280x720, MSAA4, default camera. Runs A,
B and C used identical arguments. Counters were exactly equal across all three S0 runs.

| Field | S0 A | S0 B | S0 C | Fox seek | Duck ortho |
| :--- | ---: | ---: | ---: | ---: | ---: |
| VFS decompression | 13.672 | 12.714 | 12.335 | 13.971 | 13.451 |
| Environment init | 36.515 | 37.624 | 36.774 | 37.339 | 36.395 |
| Environment filter | 71.201 | 70.094 | 69.791 | 70.370 | 69.490 |
| glTF load/decode | 0.468 | 0.294 | 0.346 | 7.152 | 1.423 |
| Renderer upload | 0.694 | 0.691 | 0.643 | 4.665 | 1.855 |
| Validation (wall) | 0.934 | 0.923 | 0.899 | 1.315 | 0.957 |
| Total startup | 649.640 | 696.421 | 634.088 | 729.005 | 633.873 |
| Model bytes / nodes / textures | 29,952 / 4 / 0 | same | same | 162,852 / 26 / 1 | 120,484 / 3 / 1 |
| Startup wrapper GL calls | 3,377 | 3,377 | 3,377 | 3,880 | 3,393 |
| Startup VAO creations | 101 | 101 | 101 | 101 | 101 |
| Startup buffer allocation bytes | 55,616 | 55,616 | 55,616 | 240,176 | 204,448 |
| Startup texture allocation bytes | 52,428,776 | 52,428,776 | 52,428,776 | 56,625,432 | 53,477,352 |

The total includes GLFW/window/font/UI initialization that is not split further by this task; it must
not be read as the sum of the named startup fields. The named stages are the comparison points. The
texture-allocation byte count includes the IBL cubemaps and assumes four bytes per texel for the
allocation call; it is an accounting estimate, not a driver memory query.

#### Steady-state S0 results

Frame 8, identical arguments, three runs:

| Sample | Run A | Run B | Run C | Median / spread |
| :--- | ---: | ---: | ---: | :--- |
| CPU frame | 1.532 | 1.526 | 1.526 | 1.526 ms / 0.4% |
| `ui.newFrame` | 0.8795 | 0.8539 | 0.8688 | 0.869 ms / 3.0% |
| `scene.build` | 0.0263 | 0.0307 | 0.0278 | 0.028 ms / 16.8% |
| `picking.build` | 0.0171 | 0.0191 | 0.0173 | 0.017 ms / 11.7% |
| `commands.generate` | 0.1819 | 0.1938 | 0.1619 | 0.182 ms / 19.7% |
| `draw.submit` | 0.2088 | 0.1876 | 0.2028 | 0.203 ms / 10.6% |
| `ui.panels` | 0.0315 | 0.0327 | 0.0323 | 0.032 ms / 3.8% |
| `ui.submit` | 0.1325 | 0.1497 | 0.1493 | 0.149 ms / 12.2% |

The steady frame-8 counters were identical in all three runs: 220 wrapper GL calls, 5 draw calls
(4 renderer, 1 application), 1 scene build with 4 visited nodes and 2 drawables, 1 picking build,
1 command generation, 0 dirty syncs, 0 cache rebuilds and 0 buffer/texture/resource creations. The
4 VAO creations per steady frame are a real observed cost from the current
`CommandMapping.cpp` path that recreates a vertex layout per command every frame (the existing code
comment notes this); P0.2 records it and does not change it.

Frame 1 is recorded separately (`"firstFrame": true`): 850 wrapper GL calls, 5 draws, 1 dirty sync,
2 cache rebuilds, 4 VAO creations, 3 framebuffer and 3 renderbuffer creations, 2 texture creations,
3 texture allocations (12,615,680 bytes), 8 buffer allocations/uploads (55,248 bytes) and 4 shader
compiles / 4 program links. It includes first-use FBO/AA/program setup and must not be compared with
steady frames.

Fox at animation time 0.75: load/decode 7.152 ms, upload 4.665 ms, validation 1.315 ms; steady
frame 8 had 239 wrapper GL calls, 5 draws, 26 visited nodes, 1 scene build and 1 picking build. Duck
orthographic: load/decode 1.423 ms, upload 1.855 ms, validation 0.957 ms; steady frame 8 had 141
wrapper GL calls and 3 draws (the orthographic path intentionally has no background cubemap draw).
Counters and stages responded to the scene/projection controls, confirming they are not fixed to S0.

#### Commands and evidence

```cmd
cmake --build build --config Release --target Glider

:: S0 A/B/C (only the output suffix changes)
cmd /c "build\bin\Release\Glider.exe --isolated --window-size 1280x720 --profile-report G:\temp\uray-codex\opencode\glider-p02\profile_s0_a.json --scenario-report G:\temp\uray-codex\opencode\glider-p02\scenario_s0_a.json --dump-frame G:\temp\uray-codex\opencode\glider-p02\frame_s0_a.png --exit-after-profile --exit-after-dump"

:: Fox, animation seek
cmd /c "build\bin\Release\Glider.exe --isolated --window-size 1280x720 --anim-time 0.75 --profile-report G:\temp\uray-codex\opencode\glider-p02\profile_fox.json --scenario-report G:\temp\uray-codex\opencode\glider-p02\scenario_fox.json --dump-frame G:\temp\uray-codex\opencode\glider-p02\frame_fox.png --exit-after-profile --exit-after-dump OpenGL-Renderer\Tools\tools\operations\tests\resources\Fox\glb\Fox.glb"

:: Duck, orthographic control
cmd /c "build\bin\Release\Glider.exe --isolated --window-size 1280x720 --camera-projection ortho --camera-target 1.435,2.115,-1.54 --camera-distance 8 --profile-report G:\temp\uray-codex\opencode\glider-p02\profile_duck_ortho2.json --scenario-report G:\temp\uray-codex\opencode\glider-p02\scenario_duck_ortho2.json --dump-frame G:\temp\uray-codex\opencode\glider-p02\frame_duck_ortho2.png --exit-after-profile --exit-after-dump OpenGL-Renderer\Tools\tools\operations\tests\resources\Duck\glTF-Binary\Duck.glb"
```

The three S0 PNGs were byte-identical:
`2A3261343CCB8FEFCF331F537E0FF9E3AB9B7978595B7F98CBBBE72502A99A98`. The S0, Fox and Duck
PNGs were opened and inspected: the expected default-layout UI, PBR/IBL scene and selected poses
were visible; no instrumentation-induced visual change was observed.

#### Coverage gaps / limitations

- Counter scope is the renderer's `gl::GL` wrapper plus explicit application paths, not a driver
  trace. GL calls issued directly by third-party code (ImGui/GLFW/GLAD internals) are not counted.
- Texture allocation bytes use `width * height * 4` (and depth for 3D) as an estimate for every
  allocation call, including non-RGBA8 formats and renderbuffer storage. They are not GPU memory
  usage.
- `cacheRebuilds` counts `updateGltf`/`updateScene` invocations, which are the current coarse
  synchronization/rebuild boundary. It is not an internal cache-entry counter.
- Startup total contains unsplit initialization; use the named startup fields for comparison.
- Detailed instrumentation is opt-in, but its on/off overhead is not quantified in this record.
  That is explicitly P0.5. Ordinary runs keep counting disabled.
- The Fox and Duck controls were run once each after the S0 pair; they verify response to different
  scenes, not statistical variance.
- No GPU timing is claimed here. P0.3 will add non-blocking pass queries and will keep CPU
  submission separate from GPU execution.

#### Decision and rollback

P0.2 passes its task-level gate: named CPU stages, startup phases and resource/GL counters are
available in a machine-readable report; repeated S0 runs produced identical counters and stable
steady CPU timings; first-frame costs are retained separately; and the three controls render
correctly. The Phase 0 exit gate is still open because P0.3, P0.4 and P0.5 remain.

Rollback boundary: the P0.2 files listed in the source state. Removing the profiling module, its
call sites and the CMake entries restores the prior behavior; P0.1 stays intact.

Evidence bundle: `G:\temp\uray-codex\opencode\glider-p02\` (`profile_*.json`, `scenario_*.json`,
`frame_*.png`).

### Phase 0 baseline build identity for [§12.3](#section-12-3)-[§12.5](#section-12-5) (historical)

The P0.3-P0.5 records below were all produced from the same working tree and executable. Earlier
P0.2/P0.3 exploratory evidence is superseded for the original Phase 0 gate by this single-build bundle.
This is the pre-review executable: [§12.6](#section-12-6) supersedes it for current working-tree validation, and its
report fields that depend on the corrected counters or capture-finalization logic must not be mixed
with [§12.6](#section-12-6) evidence.

```text
Working tree: no commit. AGENTS.md remains the user's pre-existing modification; the journal remains
  an untracked candidate document. No renderer optimization was implemented in Phase 0.
Build: cmake --build build --config Release --target Glider (MSVC Release, x64)
Report build string: Oct 21 2026 02:33:56
Executable: build/bin/Release/Glider.exe
  SHA-256 BFB7D18AFAE204DB7AEDC006F5D2A3F1593A37C4151C2E27EDDC15C40038D747
Key source hashes:
  source/imgui/main.cpp
    C4E49B4FB2BCBFF4E5567CBF020BC5274A705E082B4B3C76835FD3B2356B190C
  header/imgui/Profiling.h
    3585DD420CE70A723AC57C4679EC2FA6C1AF78BA7F82DB3A53AD9907E42D3103
  source/imgui/Profiling.cpp
    B3124AC83AACCC3BF990ABC6D3FBB8F148AC5F1632B918328108599FFA2B1978
  OpenGL-Renderer/renderer/include/renderer/GpuProfiling.h
    9191FEFFFA2A981B5892EAF3A0A74E1111769C915E46F325DDF74DE7E0FBE24A
  OpenGL-Renderer/renderer/source/GpuProfiling.cpp
    D9566898437A14C6240305090123B33FB8875EF13BB97C2C68A2E5263EBAA8E6
  OpenGL-Renderer/renderer/include/renderer/Renderer.h
    59D760EEDDF3955E95A2CE37122F5938F3169F651C609CA4A2CA6A75126D90F8
  OpenGL-Renderer/renderer/source/Renderer.cpp
    0FD9E3C3F911D7FE7E604BC52A1FAD44C5A34AB07CCA17014BD0D15FFD7B77E8
  OpenGL-Renderer/renderer/source/PickingRenderLayer.cpp
    33F18216396A55B6E0B0E1B3B43FF41BEF5BB45A76FD6499D7053A3941BE4E59
  header/imgui/GladGL.h
    DF5C925C0B15863D05AA17C6EE247AFB2EB7FD961564D713325313D0320195A7
  source/imgui/GladGL.cpp
    165BFBE19A4D62684143E01E31D872F5D2FC4E961F00F46D5D53A3D4A5F1765F
  OpenGL-Renderer/renderer/include/renderer/gl/GL.h
    C76BC96D257E019B11E40FC53FB35AB79A393AF932A913BA9BD6A2294086F42F
  CMakeLists.txt
    B94C0319B1F6928675E787A9637114B954C27E2431560992BCB926804FF30049
  OpenGL-Renderer/renderer/include/renderer/gltf/GltfRenderLayer.h
    6F83EE8A79079081143BDA07ECC668FF23A7A6C86637D47D1269A096ABA7CA44
  OpenGL-Renderer/renderer/source/gltf/GltfRenderLayer.cpp
    0A73C4B778C7611A6A02010427AB0D0EB84FC1D8BE38208874113DAA9131AC28
  source/imgui/AntiAliasing.cpp
    C7AE1BD946C7B1DC9B3F2047334A98B04FEB1A2271AB68977F4AEB2DCC611199
  source/imgui/BoundingBoxRenderer.cpp
    06E664A17525858F73AF48471F2563AE8D04ED1C3BE16DD890B831ED25C4BCB7
  source/imgui/Framebuffer.cpp
    A52D8257B5BFE2DF9576848171A4F33BCB4030C81C82E61208AC08DAAA955FFF
  source/imgui/GridRenderer.cpp
    41B7F0C7A82AE4A2DD36E55C6D62725C0150634E095EE234B4AAACB435485369
Environment: Windows 11 Pro 10.0.22631 x64; NVIDIA GeForce RTX 5080/PCIe/SSE2; NVIDIA driver 616.92;
  OpenGL 4.1.0; content scale 1.0; Release build. No other GPU, driver, OS or display configuration
  was tested.
Evidence root: G:\temp\uray-codex\opencode\glider-final\
  p03\ full-window profile/scenario/PNG controls
  p04\ final vsync/benchmark reports
  p05\ viewport captures, repeatability and overhead reports
  final_summary.txt and bundle.json contain the condensed results and artifact hashes.
```

<a id="section-12-3"></a>

### 12.3 P0.3 - GPU timings

```text
Task ID / title: P0.3 - GPU timings (R0, Phase 0)
Status: PASS (task-level instrumentation)
Source state: historical baseline Phase 0 build identity above. Key P0.3 edits are GpuProfiling.h/.cpp,
  Renderer.h/.cpp, PickingRenderLayer.cpp, gl/GL.h, GladGL.h/.cpp, Profiling.h/.cpp,
  source/imgui/main.cpp and CMakeLists.txt.
Dependencies and gates satisfied: P0.1 deterministic scenarios and P0.2 CPU stages/counters are in place.
  This task adds observation only; it does not claim a rendering speedup and does not implement P1+.
Hypothesis / mechanism: wrapping each render-pass submission in its own GL_TIME_ELAPSED query, and
  polling query availability only from a later submission, exposes per-pass GPU execution time without
  a glFinish or a same-frame blocking result read. A pass that has not completed stays pending and is
  reported as unavailable rather than extending the frame wait.
Primary metric / predeclared thresholds / unaffected control:
  One non-overlapping completed query per pass; no GL_QUERY_RESULT read before GL_QUERY_RESULT_AVAILABLE;
  zero glFinish calls; pending frames explicitly marked available:false/ms:null; steady pass medians
  stable enough to distinguish the tested passes; CPU submission (draw.submit) reported separately.
  Controls: AlphaBlendModeTest (opaque plus transparent passes), Fox animation seek and Duck
  orthographic projection. All controls must render correctly and produce complete GPU pass sets.
Environment: Windows 11 Pro 10.0.22631 x64; NVIDIA GeForce RTX 5080/PCIe/SSE2; NVIDIA driver 616.92;
  OpenGL 4.1.0; content scale 1.0; Release build.
```

#### Implementation and non-blocking collection

- `GpuProfiler` opens one `GL_TIME_ELAPSED` query around each render pass. `Renderer::onDrawScenes`
  calls `beginPass(passName)` before the pass pre-commands and `endPass()` after the pass post-commands;
  `endFrame()` closes the frame. There is no nesting or overlap between pass timers. Picking pass copies
  are renamed `Picking/<original>` so their samples remain distinguishable.
- Queries are pooled and reused only after their result has been accepted. The profiler keeps at most
  eight pending frames. `beginFrame` polls prior frames; a frame is inspected only after at least two
  later submissions (`kCollectionDelayFrames = 2`). The first possible result read is therefore not in
  the frame that submitted the query.
- Readiness is checked with `glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, ...)`. A result is
  read with `glGetQueryObjectui64v(query, GL_QUERY_RESULT, ...)` only after every pass in that frame is
  available. There is no `glFinish`, no `glClientWaitSync` and no same-frame result read in
  `GpuProfiling.cpp`.
- Pending frames remain in `m_pending` and are exposed as `available:false, ms:null`. The report records
  `pendingFrames`, `unavailablePolls` and `droppedFrames`. The frame-8 report therefore contains five
  completed frames (1–5) and three pending frames (6–8); the three pending frames are not silently
  dropped or converted to zero.
- CPU and GPU time are labelled separately: `draw.submit` remains a P0.2 CPU stage around command
  submission, while `gpu.completed[].passes[].ms` is the GPU elapsed-time query result. The report states
  `gpu.cpuSubmissionStage = "draw.submit (CPU, includes command submission only)"`.
- The added query calls are visible in the wrapper counter: S0 steady frame 8 reports 228 wrapper GL
  calls versus P0.2's 220. The eight-call difference is four query start/end calls plus two availability
  and two result reads for the one frame collected in that frame. This is instrumentation cost, not a
  renderer optimization and not a like-for-like performance comparison with P0.2.

#### S0 repeatability

S0 is embedded `HelloCube.glb`, 1280x720, MSAA4, default camera, isolated state, vsync off. Three
identical runs were made on the final build. GPU steady values are medians of completed frames 2–5;
frame 1 is shown separately.

| Sample | S0 A | S0 B | S0 C |
| :--- | ---: | ---: | ---: |
| Frame 8 CPU frame (ms) | 0.924 | 0.865 | 0.674 |
| Frame 8 `draw.submit` (ms) | 0.179 | 0.151 | 0.149 |
| Steady `Opaque` (ms) | 0.0099 | 0.0088 | 0.0096 |
| Steady `Picking/Opaque` (ms) | 0.0056 | 0.0057 | 0.0057 |
| Frame 1 `Opaque` (ms) | 0.0300 | 0.0303 | 0.0301 |
| Frame 1 `Picking/Opaque` (ms) | 0.0153 | 0.0156 | 0.0158 |
| Frame 1 CPU / `draw.submit` (ms) | 6.828 / 1.003 | 7.139 / 1.045 | 7.408 / 1.191 |
| Completed / pending GPU frames | 5 / 3 | 5 / 3 | 5 / 3 |

The steady `Opaque` median across the three runs is 0.0096 ms and the steady `Picking/Opaque` median is
0.0057 ms. Individual steady samples ranged 0.0082–0.0177 ms for `Opaque` and 0.0055–0.0065 ms for
`Picking/Opaque`; at this granularity the timer is showing sub-0.01 ms quantization/noise as well as
work. `unavailablePolls` and `droppedFrames` were zero in all three runs, and frames 6–8 were reported
pending with `available:false, ms:null`.

#### Multi-pass and scene controls

AlphaBlendModeTest produced four pass timers and verified that opaque and transparent paths are timed
separately:

| Pass | Steady median, frames 2–5 (ms) | Frame 1 (ms) |
| :--- | ---: | ---: |
| `Opaque` | 0.0107 | 0.0549 |
| `Picking/Opaque` | 0.0084 | 0.0292 |
| `Transparent` | 0.0040 | 0.0081 |
| `Picking/Transparent` | 0.0042 | 0.0077 |

Its frame 8 CPU frame was 1.161 ms with `draw.submit` 0.420 ms; frame 1 was 8.852 / 1.502 ms. The scene
reported 1,410 wrapper GL calls, 21 draws, 20 VAO creations, 9 visited nodes and 10 drawables, so the
timer set responds to a materially different workload rather than being fixed to S0.

Fox at animation time 0.75 exercised 26 visited nodes: steady `Opaque`/`Picking/Opaque` medians were
0.0050/0.0064 ms (frame 1: 0.0259/0.0169 ms), with 247 wrapper GL calls and 5 draws. Duck in
orthographic projection exercised the no-background path: steady medians were 0.0096/0.0050 ms
(frame 1: 0.0167/0.0083 ms), with 149 wrapper GL calls and 3 draws. Both controls rendered correctly.

Pass output order is not treated as stable. One Fox pending frame listed `Picking/Opaque` before
`Opaque`; samples are aggregated by pass name. The timers remain non-overlapping per pass, but the
report does not promise a canonical list order.

#### Visual and evidence checks

The three S0 full-window PNGs are byte-identical:
`2A3261343CCB8FEFCF331F537E0FF9E3AB9B7978595B7F98CBBBE72502A99A98`. The AlphaBlend, Fox and Duck
control PNGs were also opened and inspected; the expected PBR/IBL scene, transparent materials and
camera/projection poses were visible with no instrumentation-induced visual change.

Evidence bundle: `G:\temp\uray-codex\opencode\glider-final\p03\` (`profile_*.json`,
`scenario_*.json`, `frame_*.png`). The same build appears in the P0.5 800- and 3000-frame runs, where
`gpu.completed` reached 805/3005 frames with 3 pending and no dropped frames; two availability polls
failed in one 800-frame run and were handled as pending, not as a blocking read.

#### Coverage gaps / limitations

- `GL_TIME_ELAPSED` measures the GPU interval between query begin/end; it is not a full GPU trace and
  can include driver/rasterization overhead outside the logical pass.
- Sub-0.01 ms pass times are close to timer resolution and are noisy. Do not compare individual frames;
  use medians over completed frames and repeat the whole run.
- The frame-8 report intentionally has three pending frames because results are not read early. Those
  frames are unavailable in that report; a longer run or a later report frame is required to see them.
- Only one active viewport was exercised. Multi-viewport timing and pass-name collisions were not
  tested in P0.3.
- No optimization was implemented or claimed; P0.3 is instrumentation only.

#### Decision and rollback

P0.3 passes: every tested render pass has a non-overlapping GPU timer, results are accepted only after
a two-submission delay and an availability check, pending/missing results are explicit, CPU submission
is reported separately, and all scene controls render correctly. At this task boundary P0.4 and P0.5
remained; the original Phase 0 gate is recorded in [§12.5](#section-12-5). The corrected-build gate is [§12.6](#section-12-6).

Rollback boundary: the P0.3 query/profiler files and call sites listed in the baseline build identity.
Removing `GpuProfiling.*`, the `Renderer` timer hooks and the report plumbing restores the P0.2-only
state without changing P0.1 or P0.2 behavior.

<a id="section-12-4"></a>

### 12.4 P0.4 - Benchmark controls

```text
Task ID / title: P0.4 - Benchmark controls (R0, Phase 0)
Status: PASS (task-level instrumentation)
Source state: same historical baseline build identity as [§12.3](#section-12-3). P0.4 edits are in source/imgui/main.cpp and
  Profiling.h/.cpp (benchmark model/writer, pre-scan vsync control, report integration).
Dependencies and gates satisfied: P0.1 scenario controls and P0.2/P0.3 reports exist. This task changes
  measurement control only; it does not optimize rendering.
Hypothesis / mechanism: an explicit swap-interval choice and a fixed warmup/measured frame budget make
  run length and presentation pacing observable instead of relying on FPS by eye. A JSON report records
  the requested and actual counts, first-frame timing separately, and whether the run completed.
Primary metric / predeclared thresholds / unaffected control:
  Vsync off must produce materially shorter intervals than vsync on at the same scene/window; both runs
  must record exactly the requested measured-frame count with complete:true; the first frame must be
  present as a separate sample; schema fields must identify build, vsync, warmup, requested/recorded
  counts, measured frame numbers and per-frame timings. Control: S0 at 1280x720, 8 warmup frames and
  60 measured frames for each vsync state.
Environment: Windows 11 Pro 10.0.22631 x64; NVIDIA GeForce RTX 5080/PCIe/SSE2; NVIDIA driver 616.92;
  OpenGL 4.1.0; content scale 1.0; Release build.
```

#### Accepted CLI and report schema

- `--vsync on|off` is pre-scanned before GLFW window creation so `glfwSwapInterval(1)` or
  `glfwSwapInterval(0)` is applied before the first presented frame. An invalid value fails before
  startup. The requested state is written to `scenario.v1` as `"vsync"`, to `profile.v1` as
  `"vsyncRequested"`, and to `benchmark.v1` as `"vsync": { "control": "glfwSwapInterval", "requested": ... }`.
- `--benchmark` enables bounded measurement without requiring detailed profiling. `--benchmark-warmup N`
  defaults to 8 frames; `--benchmark-frames N` defaults to 120 and rejects zero; `--benchmark-output PATH`
  writes `glider.benchmark.v1` (default `glider-benchmark.json` if omitted); `--exit-after-benchmark`
  closes the window immediately after the report is written.
- The loop counts frames from 1. Frames up to the warmup count are rendered but not retained in
  `measuredFrames`. The first rendered frame is copied to `firstFrame` whenever benchmarking is enabled,
  even when detailed instrumentation is off. The report is written once, at the first frame where the
  measured buffer reaches the requested count. Frames beyond that budget are not appended.
- `glider.benchmark.v1` contains `schema`, build/GL identity, `isolated`, `vsync`, window/viewport and AA
  state, `warmupFrames`, `measuredFramesRequested`, `measuredFramesRecorded`, `complete`,
  `measuredFrameNumbers`, `firstFrame`, `measuredFrames`, `timing` (`cpuFrameMs`, `frameIntervalMs` with
  count/mean/median/min/max/p95/population stddev) and optional `gpu`. `complete` is the exact count
  comparison, not an inferred success flag.
- `--profile` still controls detailed P0.2 stages/counters and P0.3 GPU queries. A benchmark without it
  remains lightweight (`detailedInstrumentation:false`, `gpu.enabled:false`, an empty `stages` list and
  zeroed counters); a benchmark with it records the same detailed data and GPU samples.

#### Final S0 vsync control

Both runs used isolated S0, 1280x720, default camera/MSAA4, warmup 8, measured 60, frame range 9–68.
Timing is milliseconds from the report's `timing` object.

| Mode | CPU median | CPU mean | Interval median | Interval mean | CPU p95 | Interval p95 | Complete |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | :--- |
| `--vsync off` | 0.6035 | 0.6399 | 0.7238 | 0.7736 | 0.8544 | 1.0214 | true, 60/60 |
| `--vsync on` | 0.9050 | 0.9386 | 17.2330 | 23.7777 | 1.1999 | 48.5011 | true, 60/60 |

The first frame was retained separately: CPU 7.0118 ms off and 6.3945 ms on, with interval 0 by
definition. The two runs differ by a factor of about 23.8 in median frame interval, so the swap-interval
control is effective. The vsync-on interval is not a hard 16.667 ms on this desktop: the compositor
produced a 9.67–48.92 ms range and the p95 was 48.50 ms. The correct interpretation is requested
swap-interval control and visible presentation pacing, not a claim that every vsync-on frame lands on
one refresh boundary.

The 60-frame runs are deliberately short controls. The final P0.5 800-frame paired runs and 3000-frame
runs confirm the same schema/count behavior at larger budgets: 800 requested/recorded and 3000
requested/recorded, all `complete:true`.

#### Evidence and limitations

```cmd
:: final P0.4 control (swap off; repeat with --vsync on and *_on.json)
cmd /c "build\bin\Release\Glider.exe --isolated --window-size 1280x720 --vsync off --benchmark --benchmark-warmup 8 --benchmark-frames 60 --benchmark-output G:\temp\uray-codex\opencode\glider-final\p04\bench_vsync_off.json --exit-after-benchmark"
```

Evidence bundle: `G:\temp\uray-codex\opencode\glider-final\p04\` (`bench_vsync_off.json`,
`bench_vsync_on.json`). The same build's benchmark reports are also under `p05\` for the paired runs.

- The benchmark itself adds a clock read and sample buffering per frame; P0.5 measures the incremental
  cost of detailed `--profile` instrumentation, not the absolute cost of benchmark mode versus no
  benchmark.
- Vsync-on results are compositor/desktop dependent; do not compare their absolute intervals across
  machines or use them as a GPU throughput metric.
- The 60-frame control is not a variance study. Use the P0.5 800/3000-frame results for baseline
  uncertainty.
- No multi-window or multi-viewport synchronized benchmark was added. The bounded mode measures the
  active app loop and records one viewport's state.
- No renderer optimization or FPS improvement is claimed.

#### Decision and rollback

P0.4 passes: vsync is explicit and observable, the benchmark has a fixed warmup and measured budget,
the first frame is separate, the report is machine-readable and count-complete, and the final vsync
control shows the expected off/on separation. At this task boundary P0.5 remained; the original Phase 0
gate is recorded in [§12.5](#section-12-5). The corrected-build gate is [§12.6](#section-12-6).

Rollback boundary: remove the P0.4 CLI branches, benchmark globals/loop block and benchmark writer/model
from `main.cpp` and `Profiling.*`; P0.1–P0.3 remain functional with their own report paths.

<a id="section-12-5"></a>

### 12.5 P0.5 - Capture and baseline bundle

```text
Task ID / title: P0.5 - Capture and baseline bundle (R0, Phase 0)
Status: PASS (task-level instrumentation)
Source state: same historical baseline build identity as [§12.3](#section-12-3). P0.5 edits are the viewport-capture helper,
  capture CLI and report fields in source/imgui/main.cpp, Profiling.h/.cpp.
Dependencies and gates satisfied: P0.1 scenario state, P0.2 timing, P0.3 GPU timing and P0.4 benchmark
  controls are available. This task records a baseline; it does not implement an optimization.
Hypothesis / mechanism: reading the final active-viewport texture after AA, rather than the whole window,
  gives a 3D-only capture region independent of window chrome and UI overlays. The actual viewport size
  is recorded and can change with the dock layout. Selecting by absolute frame or elapsed time and
  recording the actual selected frame/time makes static S0 captures reproducible. Capturing frame 1
  separately prevents the unsettled first layout from contaminating steady-state evidence.
Primary metric / predeclared thresholds / unaffected control:
  Byte-identical PNGs for repeated fixed-frame S0 captures at one build (zero differing pixels);
  first-frame and steady captures recorded separately; metadata identifies source/region/frame/time/
  dimensions/hashes; repeated timing runs quantify run-to-run variance; paired profiling-on/off runs
  quantify instrumentation overhead. Controls: time-selected capture, TAA capture, AlphaBlend/Fox/Duck
  full-window controls. All images must be visually correct and steady captures byte-stable.
Environment: Windows 11 Pro 10.0.22631 x64; NVIDIA GeForce RTX 5080/PCIe/SSE2; NVIDIA driver 616.92;
  OpenGL 4.1.0; content scale 1.0; Release build.
```

#### Capture controls and fixed region

- `--capture-viewport PATH` enables a PNG capture of the active viewport's display texture. It is separate
  from `--dump-frame`, which still captures the whole window.
- `--capture-frame N` selects an absolute frame. If a capture path is supplied without a frame or time,
  the default is frame 8. Frame numbers start at 1; zero is rejected.
- `--capture-time SECONDS` selects the first frame whose elapsed wall-clock time since the first frame is
  at least the requested value. It does not change animation time (`--anim-time` remains the animation
  seek control). The report records the actual selected frame and elapsed time, so a time-selected run
  is reproducible in content but not necessarily in frame index.
- `--capture-first-frame PATH` writes a separate frame-1 viewport PNG. The steady capture and first-frame
  capture have independent paths and independent records; neither overwrites the other.
- The steady capture is taken after the AA pass and before `ImGui::Image` submits the viewport. It is the
  raw viewport texture, including the 3D scene, ground grid and origin axes, but excluding ImGui overlays
  such as the orientation compass. The observed steady region on S0 is 734x616 inside the 1280x720
  window; the report records the actual size, so a different dock layout changes dimensions rather than
  silently cropping to a hard-coded rectangle.
- Each `capture`/`firstFrameCapture` object records `valid`, `firstFrame`, `source: "viewport-texture"`,
  `region: "active-viewport-texture"`, `viewportId`, `path`, frame, width/height, elapsed `timeSeconds`,
  `readbackMs`, `writeMs`, FNV-1a 64 `rgbaHash` over the raw RGBA bytes, `fileBytes` and `flipVertically`.
  The raw readback is vertically flipped for PNG output through the existing image writer.
- Capture directories are created if needed. A failed readback or file-size check leaves `valid:false`
  and logs the failure; it is not silently reported as a successful image.

#### Same-build image repeatability

All steady captures below used isolated S0, 1280x720, MSAA4, vsync off, frame 8, directly after AA. The
PNG SHA-256 values were computed outside the app; `rgbaHash` is the in-report FNV-1a 64 of the raw pixels.

| Capture | Frame / time | Size | File bytes | PNG SHA-256 | Raw RGBA hash |
| :--- | :--- | :--- | ---: | :--- | :--- |
| Steady A | 8 | 734x616 | 662,573 | `19B22DE8E7F221A7A5411DF730A5D002581E15AFC142EEC117F9F1551581C087` | `5549649347244494908` |
| Steady B | 8 | 734x616 | 662,573 | same as A | same as A |
| Steady C | 8 | 734x616 | 662,573 | same as A | same as A |
| Steady D | 8 | 734x616 | 662,573 | same as A | same as A |
| Steady E | 8 | 734x616 | 662,573 | same as A | same as A |
| First frame A | 1 | 1280x616 | 1,232,959 | `B27F59858229688E30194BF10BB982F10D33D439F4F4415485BE08F7B4FF6BFF` | `7596761716438974563` |
| First frame B | 1 | 1280x616 | 1,232,959 | same as first A | same as first A |
| Time-selected | 2 at 0.010 s | 734x616 | 662,573 | same as steady A | same as steady A |
| TAA 1 | 8 | 734x616 | 638,060 | `1E94CBE8BF9E86B1CAA210BFDABB192E2A87603A0313C01F421D0AB52B4D0209` | `968922005964765559` |
| TAA 2 | 8 | 734x616 | 638,060 | same as TAA 1 | same as TAA 1 |
| TAA 3 | 8 | 734x616 | 638,060 | same as TAA 1 | same as TAA 1 |

Five independent steady runs produced one unique PNG hash and one unique raw-pixel hash: zero image
noise was observed for deterministic MSAA4 S0 at frame 8. Three independent TAA runs were also
byte-identical at frame 8, so the tested S0 temporal-AA state is deterministic at this capture point;
TAA under motion and other scenes were not covered. The time-selected run landed on frame 2 at 0.010 s
and produced the same steady image as frame 8, confirming that the docked 734x616 layout is already
stable by frame 2 on this run.

The first-frame images are deliberately different evidence: frame 1 is 1280x616 because the docking
layout has not settled, and its camera framing shows the wider pre-dock aspect. It is byte-identical
across two runs but must not be compared with the steady 734x616 images. The full-window `--dump-frame`
S0 controls remain `2A3261343CCB8FEFCF331F537E0FF9E3AB9B7978595B7F98CBBBE72502A99A98` in P0.3.

#### Capture overhead

Capture is a one-off readback plus PNG encode, not a steady-frame operation. Across the five steady
captures, `glGetTexImage` readback took 0.383–1.142 ms (median 0.743 ms) and PNG writing took
56.585–59.389 ms (median 57.342 ms). The frame-8 CPU sample in those runs was 59.627–62.568 ms because
the encode occurs inside the capture frame; frame 9's interval also contains the write. These runs were
therefore used for image evidence, not steady timing. The first-frame captures took 1.122–1.200 ms of
readback and 101.058–102.164 ms of write for 1,232,959 bytes. The time-selected capture took 0.721 ms
readback and 66.644 ms write.

Consequence for benchmarking: do not place a viewport capture inside a measured benchmark frame. The
P0.5 timing runs below use no capture. If capture and benchmark are combined, place the capture before
the measured interval (or extend warmup past the capture and its following interval) and do not treat
the capture frame as steady state.

#### Timing variance and instrumentation overhead

Timing variance was measured with 16 interleaved pairs of S0 runs, each 800 measured frames after 8
warmup frames, vsync off, alternated off/profile order to reduce order bias. The profile mode enabled
P0.2 stages/counters and P0.3 GPU queries.

| Mode | CPU median across runs (range, spread) | Interval median across runs (range, spread) |
| :--- | :--- | :--- |
| No detailed profile | 0.6275 ms (0.5811–0.6786, 15.5%) | 0.7558 ms (0.7013–0.8333, 17.5%) |
| `--profile` | 0.6398 ms (0.5983–0.6877, 14.0%) | 0.7837 ms (0.7278–0.8501, 15.6%) |

Paired profile-minus-baseline deltas over those 16 pairs:

| Delta | Median | Mean | Min | Max | Positive pairs |
| :--- | ---: | ---: | ---: | ---: | ---: |
| CPU frame (ms) | +0.0126 | +0.0130 | -0.0635 | +0.1066 | 9 / 16 |
| Frame interval (ms) | +0.0229 | +0.0206 | -0.0828 | +0.1488 | 9 / 16 |

Two 3000-frame pairs with opposite order gave CPU median/interval deltas of -0.0361/-0.0215 ms
(off then profile) and +0.0534/+0.0588 ms (profile then off); averaging the pairs gives roughly
+0.009/+0.019 ms. The detailed instrumentation is therefore small in central tendency (about
0.01–0.02 ms, roughly 2–3% of this frame) and is not distinguishable from host scheduling/frequency
noise in a single run. The measured single-run spread is 14–18%, so a single-run claim below roughly
0.1 ms or below 15% is not trustworthy on this host; paired repetitions are required.

First frames remained separate: across 32 paired-run first samples, CPU was 6.9871 ms median
(6.0294–7.9190 ms), versus roughly 0.63–0.64 ms steady. The 3000-frame first samples were 5.9272–8.0813 ms.
The 16 profile runs completed 805 GPU frames each with 3 pending and 0 dropped; one run recorded two
failed availability polls, which were retained as pending/unavailable rather than blocking.

#### Commands and evidence

```cmd
:: steady viewport capture at a fixed frame, plus a separate first-frame capture
cmd /c "build\bin\Release\Glider.exe --isolated --window-size 1280x720 --vsync off --capture-viewport G:\temp\uray-codex\opencode\glider-final\p05\viewport_s0_a.png --capture-frame 8 --capture-first-frame G:\temp\uray-codex\opencode\glider-final\p05\viewport_s0_first_a.png --profile-report G:\temp\uray-codex\opencode\glider-final\p05\profile_s0_capture_a.json --exit-after-profile"

:: elapsed-time capture (selects frame 2 at 0.010 s for this build)
cmd /c "build\bin\Release\Glider.exe --isolated --window-size 1280x720 --vsync off --capture-viewport G:\temp\uray-codex\opencode\glider-final\p05\viewport_s0_time.png --capture-time 0.01 --profile-report G:\temp\uray-codex\opencode\glider-final\p05\profile_s0_time_capture.json --exit-after-profile"

:: TAA capture
cmd /c "build\bin\Release\Glider.exe --isolated --window-size 1280x720 --vsync off --aa taa --capture-viewport G:\temp\uray-codex\opencode\glider-final\p05\viewport_s0_taa_1.png --capture-frame 8 --profile-report G:\temp\uray-codex\opencode\glider-final\p05\profile_s0_taa_1.json --exit-after-profile"
```

The timing/overhead matrix used the P0.4 benchmark form with no capture, for example:
`--isolated --window-size 1280x720 --vsync off [--profile] --benchmark --benchmark-warmup 8
--benchmark-frames 800 --benchmark-output ... --exit-after-benchmark`.

Evidence bundle: `G:\temp\uray-codex\opencode\glider-final\p05\` (`profile_s0_capture_a..e.json`,
`viewport_s0_a..e.png`, first-frame PNGs, `profile_s0_time_capture.json`, `profile_s0_taa_*.json`,
`bench_pair_01..16_*.json`, `bench_long_*.json`, `bench_long2_*.json`), plus `p03\`, `p04\`,
`final_summary.txt` and `bundle.json` at the evidence root. The steady viewport, first-frame, TAA and
P0.3 full-window controls were opened and inspected; the expected scene, grid, selection axes and
transparent/opaque material results were visible. The steady viewport capture intentionally excludes
the orientation-compass overlay because it is read before `ImGui::Image`.

#### Coverage gaps / limitations

- `glGetTexImage` is a synchronous client readback and can stall the GPU; its measured cost is reported
  separately. It is not a zero-cost screenshot path and must stay outside steady/measured frames.
- PNG encoding is CPU/storage dependent (about 57 ms for the steady 734x616 image here). Different
  compression/library versions or disks will change `writeMs` and file bytes, though not the raw-pixel
  hash for the same scene.
- The first-frame viewport is 1280x616, not the steady 734x616, because the dock layout is still settling.
  This is retained as separate evidence rather than coerced into the steady region.
- Image determinism was established for S0 MSAA4 and S0 TAA at frame 8. Other scenes, animation motion,
  multi-viewport layouts and other AA modes were not exhaustively repeated.
- Timing variance is host-noise limited (14–18% across run medians). No power/thermal/frequency
  instrumentation was added, so the reason for the spread is not attributed to a specific component.
- Capture does not include ImGui overlays; use `--dump-frame` when a whole-window image is required.
- No renderer optimization or image-quality/performance tradeoff was implemented.

#### Decision, Phase 0 gate and rollback

P0.5 passes: fixed viewport captures are reproducible at frame and elapsed-time selectors, first-frame
and steady evidence are separate, five steady MSAA captures and three TAA captures were byte-identical,
capture metadata is machine-readable, timing variance is quantified and paired instrumentation overhead
is measured (about 0.01–0.02 ms central, below single-run noise).

**Phase 0 exit gate: MET for the original baseline build.** P0.1-P0.5 provide deterministic scenario
state, CPU/startup counters, deferred non-blocking GPU pass timers, explicit vsync/benchmark controls,
reproducible viewport captures, same-build image determinism, measured timing variance and an
instrumentation-overhead estimate. The gate authorizes P1 measurement work; it does **not** establish
any rendering speedup. The pre-review gate is retained here for history; [§12.6](#section-12-6) re-records the gate at
the corrected build.

Rollback boundary: remove the P0.5 capture helper/CLI/report fields from `source/imgui/main.cpp` and
`Profiling.*`; P0.1–P0.4 remain intact. The evidence bundle is external and does not affect the
standalone executable or `EmbeddedVFS`/`StdFileSystem` behavior.

<a id="section-12-6"></a>

### 12.6 P0.6 - Measurement/reporting corrections (Phase 0 post-review)

```text
Task ID / title: P0.6 - Review corrections for Phase 0 measurement/reporting (R0, Phase 0)
Status: PASS (focused correction validation)
Source state: no commit. Seven files changed relative to the historical [§12.3](#section-12-3)-[§12.5](#section-12-5) baseline:
  source/imgui/main.cpp, header/imgui/Profiling.h, source/imgui/Profiling.cpp,
  OpenGL-Renderer/renderer/include/renderer/GpuProfiling.h,
  OpenGL-Renderer/renderer/source/GpuProfiling.cpp, source/imgui/GladGL.cpp and
  source/imgui/Framebuffer.cpp.
Build: cmake --build build --config Release --target Glider (MSVC Release, x64)
Report build string: Oct 21 2026 03:46:56
Executable: build/bin/Release/Glider.exe
  bytes 12,904,448
  SHA-256 57B68AC8C95BDF7D726F63B475D9FDC79C3A7C1ECD7428F833D8C384FE273F1D
Changed source hashes:
  source/imgui/main.cpp
    EEB1295061C3117CC9F8FDC70AE175A701C251DC8DBCD050E943B6F23143D02C
  header/imgui/Profiling.h
    CFF2C55364506F754C5454F1533570E306657A6D27F188E52927E57BA7A4AB26
  source/imgui/Profiling.cpp
    723D37792161B95336E4A7FC10021E9D7B2B41925F40D71A2C50D927D4D5F8FE
  OpenGL-Renderer/renderer/include/renderer/GpuProfiling.h
    E564E3B8B7FB964E683101AE7CDEEDF617C98A494646AED7007466D82B573B20
  OpenGL-Renderer/renderer/source/GpuProfiling.cpp
    57906B12FAF3F4B1ABF3626E2006E65D0B28CCE861AA2FF330DADE38C7A4F722
  source/imgui/GladGL.cpp
    03745CDF6B0C49619910CD1F0F3E7BD45AB945448583AFCCE87A74326892A188
  source/imgui/Framebuffer.cpp
    A72567B4FA1F9D41D78F6E50501FF525C0CF4FD8A7D301E2A60C1B15631B69F0
Other key source hashes are unchanged from the historical baseline block above.
Environment: Windows 11 Pro 10.0.22631 x64; NVIDIA GeForce RTX 5080/PCIe/SSE2; NVIDIA driver 616.92;
  OpenGL 4.1.0; content scale 1.0; Release build. No other GPU, driver, OS or display configuration
  was tested.
Evidence root: G:\temp\uray-codex\opencode\glider-p0-final2\
  Each case has stdout.log and stderr.log; reports and PNGs are in the case directories.
  bundle.json contains the condensed build hashes and test results.
```

#### Defect corrections

1. **Late frame/time captures were finalized too early.** The profile report previously wrote at
   `SCENARIO_REPORT_FRAME` (8) even when a requested steady capture was scheduled later, leaving
   `capture.valid=false` in otherwise successful runs. The report now waits for at least frame 8 and for
   `s_captureAttempted`; a deferral message is logged. `captureRequested` is recorded separately.
2. **Output failures did not affect the process exit status.** `saveImage` now returns success/failure,
   report writes are tracked as attempted and successful, and a persistent failure flag makes the
   process return 1 for profile, benchmark, scenario, capture or dump failures. The capture-failure case
   still writes a complete profile report with `capture.valid=false` when the report path is valid, but
   exits 1. JSON writes now explicitly flush and close before reporting success.
3. **Storage accounting used a blanket 4 bytes per texel.** Texture storage is estimated from the
   internal format, dimensions and mip level; uploads from the transfer format/type/dimensions; and
   renderbuffers from internal format, dimensions and sample count. Unsupported formats increment
   `textureAllocationUnknownCalls`, `textureUploadUnknownCalls` or
   `renderbufferAllocationUnknownCalls`, and the report exposes matching `*BytesComplete` flags instead
   of silently charging RGBA8. Application framebuffer color textures and depth/stencil renderbuffers
   are counted separately; the 8x MSAA sample multiplier is applied to the multisampled renderbuffers.
4. **CPU/GPU sample retention was unbounded.** CPU frame samples and GPU completed frames are capped at
   512. The reports expose the limit, retained count, dropped count and first/last retained frame under
   `frameRetention` and `gpu.retention`.

#### Focused correction evidence

| Case | Result |
| :--- | :--- |
| Late frame capture, `--capture-frame 12 --exit-after-profile` | exit 0; `reportFrame=12`; `capture.valid=true`; `capture.frame=12`; PNG 662,573 B, SHA-256 `19B22DE8E7F221A7A5411DF730A5D002581E15AFC142EEC117F9F1551581C087`; stdout logs the deferral. The final2 PNG was visually inspected: expected scene, grid, selection axes and cube. |
| Late time capture, `--capture-time 0.05 --exit-after-profile` | exit 0; `reportFrame=71`; `capture.valid=true`; `capture.frame=71`; `capture.time=0.0505779 s`; PNG 662,573 B. |
| Profile report path under a file | exit 1; no report created; stderr reports the write failure and final failure status. |
| Benchmark report path under a file | exit 1; no report created; stderr reports the failed attempt and final failure status. |
| Scenario report path under a file | exit 1; profile report exists; scenario report absent; stderr reports the failed scenario write and final failure status. |
| Capture path under a file, report path valid | exit 1; report finalized at frame 9; `captureRequested=true`; `capture.valid=false`; `capture.frame=9`; no PNG. |
| Valid `--dump-frame` / dump path under a file | exit 0 with a 747,425 B PNG / exit 1 with no image. |
| Successful benchmark | exit 0; `complete=true`; requested/recorded 20/20; measured frames 3-22. |
| First-frame capture regression check | exit 0; `reportFrame=8`; `hasFirstFrameCapture=true`; `firstFrameCapture.valid=true`, frame 1, 1,231,549 B; `captureRequested=false` and steady `capture.valid=false`. |
| Memory accounting, no AA vs 8x MSAA | no AA frame 1: `textureAllocationBytes=6,307,840`, `renderbufferAllocationBytes=6,307,840`; 8x MSAA frame 1: `textureAllocationBytes=6,307,840`, `renderbufferAllocationBytes=53,616,640` (17/2 = 8.5x the two single-sample depth units); unknown texture/renderbuffer calls 0. Startup changed from the historical 52,428,776 B / 8,388,608 B upload to 201,326,496 B / 25,165,824 B, consistent with wide-format IBL storage rather than uniform RGBA8. |
| Retention at frame 700 | exit 0; `reportFrame=700`; `capture.valid=true`; CPU limit 512, retained 512, dropped 188, range 189-700; GPU limit 512, retained 512, dropped 185, range 186-697, pending 3. |

#### Decision, gate and limits

The four reviewed defects are corrected and pass the focused checks. **Phase 0 gate: MET at the
corrected build.** The gate authorizes P1 measurement work; it does not establish a rendering speedup.

The historical [§12.3](#section-12-3)-[§12.5](#section-12-5) timing matrix, 16-pair overhead estimate, 800/3000-frame benchmark results,
PNG images and counter values remain the pre-review baseline. They were not regenerated after this
correction build. Because this pass changed measurement/reporting rather than rendering, a full timing
matrix rerun was intentionally not performed; instrumentation-sensitive counters and late-capture
behavior must use the [§12.6](#section-12-6) build or later. No commit or push was performed.

Rollback boundary: revert the seven changed files above to the historical baseline block. The P0.1-P0.5
scenario, timing, capture and report formats remain otherwise intact; the evidence directory is
external and does not affect the standalone executable or `EmbeddedVFS`/`StdFileSystem` behavior.

<a id="section-12-7"></a>

### 12.7 Independent re-review before Phase 1 (2026-10-21)

**Result: PASS for the four [§12.6](#section-12-6) corrections; no remaining blocking findings in this focused re-review.**
Release build and `git diff --check` passed. The executable hash is unchanged from [§12.6](#section-12-6)
(`57B68AC8C95BDF7D726F63B475D9FDC79C3A7C1ECD7428F833D8C384FE273F1D`).

Independent evidence is under `build/phase0-rereview/` (local build artifacts, not committed).
All runs used `--isolated --window-size 1280x720 --vsync off`:

- Frame-12 viewport capture with `--exit-after-profile`: exit 0, report and capture both frame 12,
  capture valid. PNG was viewed and exactly matched the historical steady baseline SHA-256
  `19B22DE8E7F221A7A5411DF730A5D002581E15AFC142EEC117F9F1551581C087`.
- Time capture at 0.5 seconds: exit 0, report and capture both frame 1108, valid capture at/after
  0.5 seconds. A separate 0.05-second run captured during frame 1 and correctly reported that actual
  frame/size at frame 8; wall-time selectors do not guarantee a settled layout. Prefer fixed frames
  for deterministic visual comparisons.
- Benchmark and profile output paths beneath an existing regular file: both exit 1. Invalid capture
  destination: exit 1 with a valid JSON report recording `captureRequested=true`, `capture.valid=false`.
- No AA and 8x MSAA at steady 734x616 dimensions: frame-2 texture allocations were 3,617,152 bytes
  in both runs; renderbuffer allocations were 3,617,152 and 30,745,792 bytes respectively. Both match
  the explicit attachment/sample calculation, including picking depth and multisampled scene depth/color.
- Frame-700 capture/report: retained CPU frames 189–700 (512 retained, 188 dropped); retained completed
  GPU frames 186–697 (512 retained, 185 dropped), with 698–700 pending. Retention metadata matched data.
- Detailed benchmark with 8 warmup and 20 measured frames: exit 0, `complete=true`, 20 recorded samples.

This re-review does not establish a performance improvement or regenerate the historical overhead
matrix. Use the corrected executable for both sides of Phase 1 comparisons and the journal's noise
criteria. GPU results remain delayed and bounded; allocation bytes are estimates, not live driver VRAM.
No Phase 1 optimization was implemented. The pre-existing AGENTS.md modification is outside this commit.

