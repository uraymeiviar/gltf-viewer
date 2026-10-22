# Phase 1 — Local CPU Optimizations

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Complete. 

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-3"></a>

### 5.3 Phase 1 — Local CPU Work with Stable Render Semantics (R1)

**Entry:** Phase 0. Each task is independent unless noted.

- [x] **P1.1 — Cache immutable GL capabilities ([§3.2](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-2)).** Cache extension support/max anisotropy per
  context. Keep existing sampler binding behavior; sampler objects belong to P3.2.
- [x] **P1.2 — Linear mapping bookkeeping ([§3.8](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-8)).** Replace quadratic removal tracking without changing
  create/update/destroy callbacks. Verify additions, removals, duplicates as permitted by the API, empty
  mappings and reload. Measure synchronization scaling across increasing scene sizes.
- [x] **P1.3 — Reuse skin scratch memory ([§3.17](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-17)).** Preserve joint texture format and upload frequency.
  Verify size changes and identical packed matrix contents; measure allocations, not an assumed GPU win.
- [x] **P1.4 — Cache Statistics ([§3.21](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-21)).** Invalidate on load/reload/model revision; confirm displayed and
  copied values and embedded-file handling. Verify zero steady-state metadata queries/aggregate scans.
- [x] **P1.5 — Incremental log counts ([§3.23](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-23)).** Verify append/Clear and all severities with Logger closed
  and open. Leave clipping and retention policy for later tasks.
- [x] **P1.6 — Gate hot-path GL error checks ([§3.1](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-1)).** Preserve Debug diagnostics and coarse resource/pass
  checks. Compare Release with the gate on/off. Treat reduced diagnostic coverage as an explicit tradeoff,
  not proof the candidate has no GL errors; run the diagnostic configuration before acceptance.
- [x] **P1.7 — Remove measured allocation overhead ([§3.5](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-5), [§4.3](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-3)).** Reserve known temporary list capacities
  or replace recursive type-erased traversal only where profiling shows a cost. Preserve traversal order;
  do not introduce persistent command caches in this task.

**Exit gate:** affected outputs/behavior match, the intended operation counts fall, and timings or
allocation metrics improve beyond noise without regressions in controls. No rendering API redesign.

**Phase 1 status: accepted for the current supported scope (2026-10-21).** All seven tasks are
recorded in Sections 12.8-12.14. P1.1, P1.7 and the targeted P1.3-P1.6 metrics meet their stated
per-task gates; P1.2 is behaviorally correct with a neutral steady-state effect; total-frame benefits
for P1.4-P1.6 remain INCONCLUSIVE where recorded. The cumulative review in [§12.15](#section-12-15) now covers the
supported S6 per-viewport settings and picking cases and records S9 scene switching and viewport
destruction as not applicable to the current viewer. The independent reviewer acceptance is recorded
at the end of [§12.15](#section-12-15); no blocking Phase 1 finding remains. **Phase 2 has started:** P2.1 and P2.2 are
implemented and validated in [§12.16](RENDERER_OPTIMIZATION_PHASE_2.md#section-12-16)-[§12.17](RENDERER_OPTIMIZATION_PHASE_2.md#section-12-17), remain uncommitted for review, and P2.3 is implemented
with its evidence and remaining validation limits recorded in [§12.20](RENDERER_OPTIMIZATION_PHASE_2.md#section-12-20). P2.3 is awaiting independent
review; P2.4 has not started. The P2.1/P2.2 review findings and their fixes are recorded in [§12.18](RENDERER_OPTIMIZATION_PHASE_2.md#section-12-18);
that section supersedes the startup-only invalidation claims in [§12.17](RENDERER_OPTIMIZATION_PHASE_2.md#section-12-17).

<a id="section-12-8"></a>

### 12.8 P1.1 - Cache immutable GL capabilities (2026-10-21)

```text
Task ID / title: P1.1 - Cache immutable GL capabilities (R1, Phase 1)
Status: PASS (paired steady-state timing)
Source state: no commit. Changed relative to the corrected Phase 0 baseline:
  header/imgui/GladGL.h, source/imgui/GladGL.cpp,
  OpenGL-Renderer/renderer/include/renderer/gl/GL.h,
  OpenGL-Renderer/renderer/source/gl/Sampler.cpp.
Build: cmake --build build --config Release --target Glider (MSVC Release, x64)
Executable: G:\temp\uray-codex\opencode\glider-p1\builds\p1_1_candidate.exe
  SHA-256 D7BDEC7C9A16889F2290FF9C5E9EF1CE49332932CA87D65D1E6C8DCA35D6531B
Baseline executable: G:\temp\uray-codex\opencode\glider-p1\builds\base_p1_0.exe
Evidence root: G:\temp\uray-codex\opencode\glider-p1\p1_1\
```

**Change.** `ux3d::imgui::GladGL` now caches whether anisotropic filtering is supported and the maximum
supported anisotropy for each GL context. `ux3d::renderer::gl::GL` exposes that cached query, and
`Sampler::commit` uses it instead of issuing extension/query calls while binding samplers. Sampler
object creation, sampler state and binding behavior are otherwise unchanged; sampler objects remain
P3.2 work.

**Measurement.** Workload `fixtures/p1_cubes_512.glb`, 512 cubes, 1280x720, vsync off, no AA,
`--profile`, 120 warmup and 600 measured frames, five alternating baseline/candidate pairs. Captures
were taken at frame 15 and kept outside the measured interval. The harness was
`G:\temp\uray-codex\opencode\glider-p1\bench_harness.py`.

| Metric | Baseline | Candidate | Result |
| :--- | ---: | ---: | :--- |
| CPU frame median, five paired runs | 14.42075, 14.5383, 14.366, 14.3253, 14.3494 ms | 14.18425, 13.89745, 13.87705, 14.02865, 13.653 ms | median paired delta **-3.40%**; improved **5/5** |
| Baseline run-median noise | - | - | **1.48%** |
| `draw.submit` stage | - | - | median paired delta **-9.71%** |
| `glCalls` (representative run) | 99,414 | 91,220 | **-8,194 calls/frame** |

The CPU delta exceeds both 3% and the measured baseline noise, and all five pairs improved. The
reported timing is CPU-only; no GPU speedup is claimed.

**Correctness and coverage.** Baseline and candidate frame-15 viewport captures were pixel-identical:
SHA-256 `4FF7B4A64836DFBB1925E7FD564B4D48467FF7CF65732C3BC35C8666A7E93776` for both. The image was
opened and inspected: expected HelloCube-like test scene, grid, camera and lighting unchanged. The
cached values are context-owned and immutable for the lifetime of the context; no sampler object or
format was introduced.

**Coverage gap.** No dedicated unaffected control was built for P1.1. The 512-cube scene exercises the
sampler path, so it cannot serve as an independent control for this change. Later 512 controls also
include P1.1. This does not invalidate the paired result, but it means the evidence isolates the
mechanism by code inspection plus the GL-call reduction rather than by a separate unaffected workload.

**Decision.** Accept P1.1. Rollback boundary: remove the per-context capability fields and accessors,
and restore the direct anisotropy queries in `Sampler::commit`.

<a id="section-12-9"></a>

### 12.9 P1.2 - Linear mapping bookkeeping (2026-10-21)

```text
Task ID / title: P1.2 - Linear mapping bookkeeping (R1, Phase 1)
Status: PASS (corrected behavior test and scaling); steady-state control neutral within noise
Source state: no commit. Changed relative to P1.1:
  OpenGL-Renderer/renderer/include/renderer/Mapping.h,
  OpenGL-Renderer/renderer/include/renderer/gltf/GltfMapping.h.
Executable: G:\temp\uray-codex\opencode\glider-p1\builds\p1_2_candidate.exe
  SHA-256 D8228A1FE1406E4B550D48E44545C32097D8FCA625940F684E849F2398220128
Evidence roots: G:\temp\uray-codex\opencode\glider-p1\mapping_bench\ and ...\p1_2\
```

**Change.** `Mapping` and `GltfMapping` no longer maintain the quadratic removal bookkeeping that
searched a container for every removed key. Additions, updates, removals, duplicate handling,
per-key create/update/destroy callback order and map-order destruction are preserved; the deferred
callback path in `GltfMapping` keeps its existing callback semantics.

**Behavior check.** The header-only mapping test prints callback sequences for both `Mapping` and
`GltfToRendererMapping`, and now asserts callback order, map size and the exact map contents
immediately after every operation. The original test had two defects. Its recreation condition was
`idOf(key)==2 && value!=102`, while creation always wrote `idOf(key)+100`; for `idOf(key)==2` creation
wrote exactly `102`, so the branch was unreachable. It also did not cover duplicate input in the
deferred `GltfToRendererMapping` work list. The corrected test
mutates the stored value to `999` to force destruction and recreation, and adds the deferred
`duplicate`, `duplicateNew`, `restore`, `recreate` and `clear` cases. Baseline and candidate behavior
logs are identical (13 lines, including map sizes and contents); both executables exit `0`:

```text
MAPPING add events=C0;C1;C2; mapSize=3 map=0:1,1:2,2:3
MAPPING upd events=U0=1;U1=2;U2=3; mapSize=3 map=0:1,1:2,2:3
MAPPING duplicate events=U0=1;U0=1;U2=3;D1=2; mapSize=2 map=0:1,2:3
MAPPING recreate events=D0=1;C0;D0=4;C0;D2=3;C2; mapSize=2 map=0:5,2:6
MAPPING clear events=D0=5;D2=6; mapSize=0 map=
GLTFMAP add events=C0;C1;C2;C3; mapSize=4 map=0:100,1:101,2:102,3:103
GLTFMAP upd events=U0=100;U1=101;U2=102;U3=103; mapSize=4 map=0:100,1:101,2:102,3:103
GLTFMAP duplicate events=D1=101;D3=103;U0=100;U0=100;U2=102; mapSize=2 map=0:100,2:102
GLTFMAP change events=C3;C4;U0=100;U2=102; mapSize=4 map=0:100,2:102,3:103,4:104
GLTFMAP duplicateNew events=D0=100;D2=102;D3=103;D4=104;C5;C5;C6; mapSize=2 map=5:105,6:106
GLTFMAP restore events=D5=105;D6=106;C2;C7; mapSize=2 map=2:102,7:107
GLTFMAP recreate events=D2=999;C2;U7=107; mapSize=2 map=2:102,7:107
GLTFMAP clear events=D2=102;D7=107; mapSize=0 map=
```

The earlier recreation record (`mapSize=0`) is superseded: it described a sequence that never entered
the recreation condition. The corrected `Mapping` recreation case now leaves the two recreated entries
`0:5,2:6`; the deferred mapping destroys key `2`'s sentinel value, creates it again with `102`, then
updates the existing key `7`. No implementation change was required.

**Scaling.** Median of the focused benchmark (milliseconds):

| Keys | `Mapping` baseline | `Mapping` candidate | `GltfToRendererMapping` baseline | `GltfToRendererMapping` candidate |
| ---: | ---: | ---: | ---: | ---: |
| 256 | 0.018090 | 0.019898 | 0.013753 | 0.011540 |
| 512 | 0.082397 | 0.049427 | 0.036455 | 0.023283 |
| 1024 | 0.240610 | 0.089950 | 0.124975 | 0.065680 |
| 2048 | 1.675650 | 0.208508 | 0.502842 | 0.121233 |
| 4096 | 7.329833 | 0.462825 | 1.823467 | 0.288242 |

At 4096 keys the reduction is about 15.8x for `Mapping` and 6.3x for the deferred glTF mapping. The
small 256-key result is within run noise and is not used as a win.

**Steady-state control.** 512-cube workload, 120/600 frames, five alternating pairs, P1.1 build versus
P1.2 build. CPU medians were 13.9122, 13.9507, 13.88015, 13.9986, 14.11885 ms (baseline) and 13.87845,
14.0087, 13.99515, 14.0131, 13.84375 ms (candidate). Median paired delta **+0.10%**, baseline noise
**1.71%**, improved 2/5. This is neutral within noise, as expected for a synchronization-path change.

**Correctness.** P1.2 frame-15 capture is pixel-identical to P1.1: SHA-256
`4FF7B4A64836DFBB1925E7FD564B4D48467FF7CF65732C3BC35C8666A7E93776`. The image was inspected.

**Limitation.** App startup `rendererUploadMs` was measured but is unusable as a gate: at 2048 keys the
baseline noise was 49.61% (median delta -3.56%, improved 3/5), and at 512 keys 56.09% (median delta
-1.18%, improved 3/5). Those startup results are **INCONCLUSIVE** and are not claimed as a benefit. The
mapping microbenchmark is the primary metric for this task.

**Decision.** Accept P1.2 on behavior plus scaling; the steady-state effect is neutral. Rollback
boundary: restore the previous quadratic removal tracking in the two mapping headers.

<a id="section-12-10"></a>

### 12.10 P1.3 - Reuse skin scratch memory (2026-10-21)

```text
Task ID / title: P1.3 - Reuse skin scratch memory (R1, Phase 1)
Status: PASS (focused allocation test and skinned output)
Source state: no commit. Changed relative to P1.2:
  OpenGL-Renderer/renderer/include/renderer/Skin.h,
  OpenGL-Renderer/renderer/source/Skin.cpp.
Executable: G:\temp\uray-codex\opencode\glider-p1\builds\p1_3_candidate.exe
  SHA-256 B0075D791B2396D151C15BE7058704EAF6F755884DBDC884E50A25F73D43E93C
Evidence roots: ...\skin_alloc\, ...\p1_3\, and ...\p1_3\steady\
```

**Change.** `Skin` keeps a persistent `core::Vector<float> m_jointTextureScratch`. `computeJoints`
checks the null root and joint/IBM count mismatch before sizing the scratch, then reuses the scratch
for every upload. The RGBA32F joint texture, its dimensions and the per-call texture upload frequency
are unchanged. The scratch is transient and is not copied by the existing skin copy/move constructors;
it starts empty and may retain capacity up to the largest joint count seen.

**Allocation measurement.** A focused test uses the same `core::Vector<float>` and allocation pattern
as the old local vector and the new persistent scratch. Six hundred calls per joint count:

| Joints | Width | Floats | Baseline allocations / bytes | Candidate allocations / bytes |
| ---: | ---: | ---: | ---: | ---: |
| 2 | 4 | 64 | 600 / 153,600 B | 1 / 256 B |
| 32 | 16 | 1,024 | 600 / 2,457,600 B | 1 / 4,096 B |
| 128 | 32 | 4,096 | 600 / 9,830,400 B | 1 / 16,384 B |
| 1024 | 91 | 33,124 | 600 / 79,497,600 B | 1 / 132,496 B |

Five size changes (`2 -> 128 -> 2 -> 1024 -> 2`) produced 5 allocations / 149,648 B for the
baseline-style pattern and 3 allocations / 149,136 B for the persistent scratch, leaving capacity
33,124. The early-return path performs 0 allocations in both patterns. This is a focused container
measurement, not direct instrumentation of `Skin::computeJoints`; see the limitation below.

**Output check.** `simple_skin.gltf` was rendered with `--anim-time 0.5` from the P1.2 and P1.3 builds.
The frame-15 captures were byte-identical: SHA-256
`E08C555390663C010E7C94A29EF3E771EF87878D558BF63F99F905EF3F006CE1` for both. The image was opened
and inspected: the deformed skinned mesh and lighting were unchanged. The packing loop itself was not
modified, so this exercises non-identity joint matrices through rendering rather than reading back the
joint texture.

**Control.** 512-cube workload, 120/600 frames, five alternating pairs, P1.2 versus P1.3. CPU medians
were 14.0475, 14.44785, 13.87065, 13.5958, 13.6513 ms (baseline) and 13.61645, 13.73085, 13.7711,
13.56525, 13.85445 ms (candidate). Median paired delta **-0.72%**, baseline noise **6.14%**, improved
4/5. This is no regression, but the run is inside host noise and is not a claimed steady-state win.
P1.3's intended benefit is allocation removal, not a GPU or frame-time win.

**Limitations.** Allocation counts come from the focused `core::Vector` simulation, not a heap hook
inside `Skin::computeJoints`. A direct joint-texture readback was not performed; identical rendered
output at a non-zero animation time and unchanged packing code are the content evidence. The focused
model has only two joints, so the larger widths are covered by the container test rather than by
loading a large skinned asset.

**Decision.** Accept P1.3. Rollback boundary: remove the scratch member and restore the local
allocation before the null-root/count checks.

<a id="section-12-11"></a>

### 12.11 P1.4 - Cache Statistics and file metadata (2026-10-21)

```text
Task ID / title: P1.4 - Cache Statistics (R1, Phase 1)
Status: PASS (targeted panel timing, value/clipboard/reload checks)
Source state: no commit. Changed relative to P1.3:
  header/imgui/StatisticsPanel.h, source/imgui/StatisticsPanel.cpp,
  source/imgui/main.cpp (cache invalidation plus test controls).
Candidate executable: G:\temp\uray-codex\opencode\glider-p1\builds\p1_4_candidate.exe
  SHA-256 DB1997E83F3A4D7DED9D8360E6947F2C5B55FE54665A15B8B93DE6D7F71AFC99
No-cache baseline executable: G:\temp\uray-codex\opencode\glider-p1\builds\p1_4_nocache_baseline.exe
  SHA-256 1936AD982FABB66D5DE3AAD183DA906C100754BAACF6FDA1F6E5E3B6025469DC
  (HEAD StatisticsPanel implementation plus a no-op invalidate; same P1.1-P1.3 engine and main flags.)
Evidence root: G:\temp\uray-codex\opencode\glider-p1\p1_4\
```

**Change.** `StatisticsPanel` builds one snapshot containing aggregate counts, byte estimates,
formatted row strings, file metadata and the clipboard text. The snapshot is keyed by the current
model object and path and is rebuilt only after `StatisticsPanel::invalidate()`, an explicit
**Refresh** button, or a key/path change. Invalidation is called on load, reload, new-empty-model and
successful save. The panel resolves embedded files through `EmbeddedVFS` before trying disk, reports a
separate **File Source** row (`Embedded`, `Disk`, `Unavailable`, `Unsaved`), and keeps the existing
byte formatting. Displayed rows and **Copy to Clipboard** are both sourced from the same snapshot.
`main.cpp` adds `--show-statistics` and `--show-logger` test controls; the latter is used by P1.5.

**Measurement.** Large statistics fixture `fixtures/p1_stats_4096.glb` (4096 meshes, one visible
cube), 1280x720, vsync off, no AA, `--profile --show-statistics`, 120 warmup and 600 measured frames,
five alternating no-cache/candidate pairs. Captures and report writes stayed outside the interval.

| Metric | Baseline (no cache) | Candidate (cached) | Result |
| :--- | ---: | ---: | :--- |
| `ui.panels` median per run | 0.1166, 0.11485, 0.11325, 0.1114, 0.1125 ms | 0.02745, 0.0259, 0.026, 0.0261, 0.0264 ms | median paired delta **-76.57%**; improved **5/5** |
| `ui.panels` baseline noise | - | - | **4.59%** |
| Total CPU median | 0.54725, 0.5395, 0.5164, 0.3441, 0.3515 ms | 0.2576, 0.43925, 0.43025, 0.4451, 0.4333 ms | median paired delta -16.68%; **INCONCLUSIVE** (noise 39.34%/43.27%, improved 3/5) |

The targeted `ui.panels` stage is the primary metric because the 4096-mesh scene's total CPU frame is
only about 0.5 ms and is dominated by host scheduling noise. The targeted delta is far above 3% and the
baseline stage noise, and all five pairs improved.

**Value and clipboard checks.** With the embedded default model, the panel showed `File Size 29.25 KB`,
`File Source Embedded`, `Total Buffer Data 26.98 KB`; the physical asset is 29,952 B. Clicking
**Copy to Clipboard** produced (exact, 310 bytes):

```text
File: H:\git-repos\gltf-viewer\resources\sampleGltf\HelloCube.glb
File Source: Embedded
File Size: 29.25 KB
Scenes: 1
Nodes: 4
Meshes: 1
Primitives: 1
Vertices: 681
Triangles: 972
Materials: 1
Textures: 0
Images: 0
Animations: 0
Skins: 0
Cameras: 1
Lights: 2
Accessors: 4
Buffer Views: 4
Buffers: 1 (26.98 KB)
```

Opening `fixtures/p1_stats_4096.glb` in the same process (native Open dialog, same clipboard action)
changed the copied first lines to `File: ...\p1_stats_4096.glb`, `File Source: Disk`,
`File Size: 470.23 KB`; the file is 481,512 B. A same-path reload test overwrote a copy of that asset
from 4096 to 4097 meshes while the app was running and reopened it. Before reload the clipboard showed
`File Size: 470.23 KB` and `Meshes: 4096`; after reload it showed `File Size: 470.34 KB` and
`Meshes: 4097`. This confirms invalidation on a different asset and on same-file reload, with no stale
values. The before/after clipboard files are `clipboard.txt`, `clipboard_after_open.txt` and
`clipboard_after_reload.txt` in the evidence root.

**Visual/UI check.** Window dumps were opened for the embedded model and the 4096-mesh disk asset.
The candidate shows the new **Refresh** button, `File Size`, and `File Source` rows; additive UI
differences are expected. Counts and byte values otherwise match the baseline panel. The 3D viewport
capture is unaffected by this UI-only change.

**Steady-state work.** On a cache hit, `render` only iterates the cached row vector and draws ImGui; it
performs no aggregate mesh/buffer/image traversal and no `std::filesystem::file_size` call. The panel
was closed for the 512 control and opened for this paired test. There is no explicit query counter in
the release build; the zero-work claim is supported by the code path plus the stable 76.57% `ui.panels`
reduction. This is retained as a measurement limitation, not a blocker.

**Decision.** Accept P1.4 on the targeted panel result and the value/reload checks; total CPU remains
**INCONCLUSIVE**. Rollback boundary: restore the per-frame `StatisticsPanel` traversal and remove the
invalidation calls/flags. Do not extend this cache to other panels in Phase 1.

<a id="section-12-12"></a>

### 12.12 P1.5 - Incremental logger severity counts (2026-10-21)

```text
Task ID / title: P1.5 - Incremental log counts (R1, Phase 1)
Status: PASS (counters, Copy/Clear, closed/open, targeted timing)
Source state: no commit. Changed relative to P1.4:
  header/imgui/StatusBar.h, source/imgui/StatusBar.cpp, source/imgui/main.cpp.
Candidate executable: G:\temp\uray-codex\opencode\glider-p1\builds\p1_5_candidate.exe
  SHA-256 431C975A6C98D8A98176E37DEEDADA5BA8DA448AFB82183530498EA64A0036C3
Legacy-count baseline: G:\temp\uray-codex\opencode\glider-p1\builds\p1_5_legacy_baseline.exe
  SHA-256 BC987BFC16F93F706BF1385FD9D6A423752BC07065481954A412BBDD4D97574C
  (HEAD StatusBar scan, same ConsoleLogStore/test-control main and P1.1-P1.4 engine.)
Evidence root: G:\temp\uray-codex\opencode\glider-p1\p1_5\
```

**Change.** `ConsoleLogStore` owns the retained `LogEntry` vector and a `LogSeverityCounts`
(`errors`, `warnings`, `info`). Append classifies `Error`/`Fatal` as errors, `Warning` as warnings and
all other levels as info, matching the old status-bar scan. Clear resets both the vector and counters.
`StatusBar::render` receives the counters instead of the entry vector, so it no longer scans the full
log every frame, including while the Logger is closed. Display, auto-scroll, wrapping and Copy still
read the entries vector; retention/clipping policy is unchanged and remains Phase 2 work.

**Measurement.** 512-cube workload, 1280x720, vsync off, no AA, `--profile --log-test 100000`,
120 warmup and 600 measured frames, five alternating legacy-scan/candidate pairs. The Logger was
closed. The test control appends 100,000 deterministic entries cycling Info/Warning/Error/Fatal
without echoing them to stderr.

| Metric | Legacy scan | Incremental counters | Result |
| :--- | ---: | ---: | :--- |
| `ui.statusBar` per-run medians | 0.1324, 0.13025, 0.1315, 0.1342, 0.1336 ms | 0.01435, 0.0148, 0.0145, 0.0142, 0.0146 ms | median paired delta **-89.07%**; improved **5/5** |
| `ui.statusBar` baseline noise | - | - | **2.98%** |
| Total CPU medians | 14.13435, 14.0136, 13.80175, 14.2499, 13.9811 ms | 13.8901, 14.0668, 13.98505, 13.63955, 13.78775 ms | -1.38%, baseline noise 3.20%, improved 3/5 -> **INCONCLUSIVE** for total frame time |
| Viewport frame 15 | - | - | identical SHA-256 `4FF7B4A64836DFBB1925E7FD564B4D48467FF7CF65732C3BC35C8666A7E93776` |

The targeted status-bar stage exceeds both 3% and its measured baseline noise, and every pair improved.
The full-frame effect is below host noise because the 512 scene is CPU-heavy; no overall frame-time
claim is made.

**Counter and UI checks.** With four test entries and the Logger open, the status bar showed
`2` errors (Error + Fatal), `1` warning and `12` info; the Logger showed `Messages: 15` and the four
test lines. With the Logger closed, the same `2 / 1 / 12` counts were shown in a window dump. With
100,000 test entries and the Logger closed, the dump showed exactly `50000` errors, `25000` warnings
and `25011` info (25,000 test info plus 11 startup info entries). The images were opened and inspected.

**Copy/Clear checks.** In the four-entry run, **Copy** produced 738 UTF-8 bytes in 15 non-empty lines,
including exactly:

```text
Log test #0 [Info]
Log test #1 [Warning]
Log test #2 [Error]
Log test #3 [Fatal]
```

Clicking **Clear** emptied the Logger, reset the status pill to `0 / 0 / 0`, and a following **Copy**
produced an empty clipboard value. This covers append, all severities, closed/open display and Clear.
No clipping, retention limit, ring buffer or truncation indicator was added.

**Limitation.** The timing run uses the Logger closed; opening it with 100,000 entries would measure
the existing row-rendering path, which P1.5 deliberately does not optimize. The all-severity UI check
uses four entries rather than 100,000 to keep the opened panel inspectable. Counts for the 100,000-entry
closed case are verified from the window dump, not from a per-entry assertion log.

**Decision.** Accept P1.5 on exact counters, Copy/Clear behavior and the targeted status-bar result.
Rollback boundary: restore the StatusBar vector scan and pass the entries vector again; the log store
and test controls can be removed independently. Do not start Logger clipping here.

<a id="section-12-13"></a>

### 12.13 P1.6 - Gate hot-path GL error checks (2026-10-21)

```text
Task ID / title: P1.6 - Gate hot-path GL error checks (R1, Phase 1)
Status: PASS (targeted draw.submit); total-frame benefit INCONCLUSIVE
Source state: no commit. Changed relative to P1.5:
  CMakeLists.txt
  OpenGL-Renderer/renderer/CMakeLists.txt
  OpenGL-Renderer/renderer/include/renderer/gl/Util.h
  OpenGL-Renderer/renderer/source/gl/Uniform.cpp
  OpenGL-Renderer/renderer/source/gl/Sampler.cpp
  OpenGL-Renderer/renderer/source/gl/Texture.cpp
  OpenGL-Renderer/renderer/source/gl/UniformBlock.cpp
  OpenGL-Renderer/renderer/source/CommandExecutor.cpp
Release default OFF candidate: G:\temp\uray-codex\opencode\glider-p1\builds\p1_6_candidate_off.exe
  SHA-256 D218FAF2C81429532D737E537309A75688558861D008BEF7C0FF353A7B2F3A8B
Release diagnostic ON: G:\temp\uray-codex\opencode\glider-p1\builds\p1_6_diagnostic_on.exe
  SHA-256 4FF23FB7854A99A47A7B6538649E0FD782BF446B687725A4A44A253C94F2A7B2
Debug (hot checks active at P1.6; later overwritten by the P1.7 Debug build):
  build/bin/Debug/Glider.exe
  SHA-256 3F841685A5D00D4586BE96779546F49927A344A5D26BECAA3CAB32854BDCD8F4
Evidence root: G:\temp\uray-codex\opencode\glider-p1\p1_6\
```

**Change.** Added the CMake option `GLIDER_GL_DEBUG_CHECKS` (default OFF) and the
`GLIDER_HOT_GL_CHECK` compile-time gate. The gate expands to `checkGLError` in Debug and in an explicitly
requested diagnostic Release build, and to a no-op in the default Release. It is applied to
`Uniform::commit`, `Sampler::commit`, `Texture::bind`, `UniformBlock::commit`, and the per-draw
executor sites for pre-draw validation, draw submission, VAO binding, blend setup, culling setup and
polygon-mode setup. Coarse checks remain unconditional: `Renderer::draw`, clear and depth setup,
texture upload, and the resource/pass checks in the renderer and editor layers. No GL state or draw
membership was changed.

**Measurement.** Workloads `fixtures/p1_cubes_512.glb` and `fixtures/p1_cubes_2048.glb`, 1280x720,
vsync off, no AA, `--profile`, 120 warmup and 600 measured frames, five alternating pairs. The baseline
was the diagnostic ON build; the candidate was the default OFF build. Negative deltas mean the OFF
candidate was faster.

| Workload | Total CPU delta | Baseline CPU noise | Improved pairs | `draw.submit` delta | `draw.submit` noise | Improved pairs |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| 512 cubes | +0.42% | 2.35% | 1/5 | **-3.61%** | 1.79% | **5/5** |
| 2048 cubes | -0.74% | 3.52% | 3/5 | **-3.08%** | 0.78% | **5/5** |

The targeted `draw.submit` stage exceeds both 3% and its measured baseline noise on both workloads,
with all five pairs improving. The whole-frame result does not: 512 cubes is slightly slower within
noise and 2048 cubes is a 0.74% change with 3.52% baseline noise. `commands.generate` moved by +1.31%
and +0.22% respectively, consistent with no intended effect and within host variation. No overall
frame-time speedup is claimed.

**Correctness and diagnostics.** The 512-cube frame-15 captures from the ON and OFF builds were
pixel-identical: SHA-256 `4FF7B4A64836DFBB1925E7FD564B4D48467FF7CF65732C3BC35C8666A7E93776` for
both. The image was opened and inspected. The Release diagnostic build ran the 512- and 2048-cube
workloads without a GL-error log, and the Debug build ran the 512-cube workload without one. In the
saved default-OFF P1.6 binary, the hot-site message strings (`Failed to bind vertex array`,
`Failed to commit uniform`, etc.) are absent while the coarse messages (`Failed to clear`,
`Failed to set depth test`) remain; the saved diagnostic-ON P1.6 binary contains both sets. This
confirms that the gate was compiled out of the default Release. The diagnostic build remains the
full-coverage configuration.

**Tradeoff and limits.** The default Release now detects errors only at coarse boundaries rather than
immediately at the offending command. An error raised inside a frame can therefore be reported later
and with a broader message, or not observed if the process exits before the next coarse check. The
clean diagnostic runs exercise the scenes used here, not every render state. Reduced Release coverage
is an explicit diagnostic tradeoff, not proof that the candidate produces no GL errors.

**Decision.** Accept P1.6 on the targeted `draw.submit` result and the diagnostic checks; the total
frame-time benefit remains **INCONCLUSIVE**. Rollback boundary: remove the CMake option and gate macro,
and restore direct `checkGLError` calls at the listed hot sites. Coarse checks and Debug behavior must
stay intact.

<a id="section-12-14"></a>

### 12.14 P1.7 - Remove measured allocation overhead (2026-10-21)

```text
Task ID / title: P1.7 - Remove measured allocation overhead (R1, Phase 1)
Status: PASS (full-frame and commands.generate)
Source state: no commit. Changed relative to P1.6:
  OpenGL-Renderer/renderer/include/renderer/Defines.h
  OpenGL-Renderer/renderer/source/Defines.cpp
  OpenGL-Renderer/renderer/include/renderer/CommandMapping.h
  OpenGL-Renderer/renderer/source/CommandMapping.cpp
  OpenGL-Renderer/renderer/source/CommandGenerator.cpp
Measured candidate hash (paired_512 / paired_2048):
  SHA-256 DB01EB845C302F2F60682E79199DFCE2C00F726ABD53E6BF1F1AE6A03C736666
  (the candidate executable was later overwritten in the builds directory by the
  final relink; this hash identifies the measured link)
Final Release OFF build (paired_512_final2):
  H:\git-repos\gltf-viewer\build\bin\Release\Glider.exe
  G:\temp\uray-codex\opencode\glider-p1\builds\p1_7_final_46F3.exe
  SHA-256 46F37730934B12E3902770264B5B4E76F2711239BF4D58C2BAC9F5568E41BDF7
  (same source and OFF configuration; a PE timestamp changed on relink after the
  diagnostic builds, so the file hash differs from the measured candidate)
Baseline: G:\temp\uray-codex\opencode\glider-p1\builds\p1_6_candidate_off.exe
  SHA-256 D218FAF2C81429532D737E537309A75688558861D008BEF7C0FF353A7B2F3A8B
Release diagnostic ON (P1.7): G:\temp\uray-codex\opencode\glider-p1\builds\p1_7_diagnostic_on.exe
  SHA-256 D933EC6458362B662657F9900440C860554EE8A8B5038770DBAF50A56DD7F6EC
Debug (P1.7): G:\temp\uray-codex\opencode\glider-p1\builds\p1_7_debug.exe
  SHA-256 F508F197980A564A1E388CD94C4E8922C11DA7BEE46E0775EBCE29BBC4FFDC9C
Evidence roots: G:\temp\uray-codex\opencode\glider-p1\p1_7\paired_512\,
  ...\paired_2048\, ...\paired_512_final2\, ...\alloc_bench.cpp and ...\alloc_bench.exe
```

**Change.** `Defines` gained `reserve`, and the temporary per-drawable fragment define set reserves 64
entries before its fixed debug, material and feature keys are inserted. `CommandMapping::makeCommand`
now reserves the command's uniform-block, uniform and sampler vectors from known upper bounds:
mesh and material lists, active-camera uniforms, environment uniforms, active lights and the fixed
extras. `CommandMapping::update` now fills a caller-owned vector instead of returning a fresh one;
`CommandGenerator::m_commands` therefore keeps its outer capacity across frames, and commands are moved
into it. This is capacity reuse only: command contents, shader selection, pass order and draw order are
still regenerated every frame, and no persistent command or scene cache was added.

**Measurement.** Five alternating pairs at 120 warmup and 600 measured frames per run. Negative deltas
mean the P1.7 candidate was faster. `commands.generate` is the stage that contains the changed
allocations; total CPU is reported separately so the targeted and whole-frame results are not conflated.

| Workload / run | Total CPU delta | Baseline CPU noise | Improved pairs | `commands.generate` delta |
| :--- | ---: | ---: | ---: | ---: |
| 512 cubes, first run | **-6.89%** | 2.48% | **5/5** | **-12.62%** |
| 2048 cubes | **-5.10%** | 0.76% | **5/5** | **-10.52%** |
| 512 cubes, final relink | **-6.40%** | 3.89% | **5/5** | **-11.40%** |

The first 512 run used candidate medians of 12.93185, 12.98655, 12.8347, 12.92505 and 12.7423 ms.
The final relink repeat used 12.7722, 12.9985, 12.8982, 12.942 and 12.59375 ms. The 2048 result shows
the same direction and magnitude at four times the drawable count. All three runs exceed both 3% and
their measured baseline noise with five of five pairs improving; no other stage is claimed as a
mechanism.

**Allocation evidence.** A focused benchmark compiled the real `Defines.cpp` and simulated one command's
known containers per drawable: 31 debug defines, mesh and material defines, five feature defines, 30
uniform pointers, 10 sampler pointers and one uniform-block pointer, repeated 512 times. With global
allocation counting:

| Case | Allocations | Allocated bytes | Per drawable |
| :--- | ---: | ---: | ---: |
| Baseline capacities | 50,688 | 3,203,072 | 99 allocations / 6,256 B |
| Reserved capacities | 43,008 | 2,691,072 | 84 allocations / 5,256 B |
| Change | **-7,680 (-15.1%)** | **-512,000 (-16.0%)** | -15 / -1,000 B |

The benchmark is a focused allocation simulation, not a full-application heap profile. The repeated
`commands.generate` reduction and the full-frame result are the primary evidence; the allocation counts
show that the intended container growth was removed.

**Correctness.** The 512-cube frame-15 capture is pixel-identical to P1.6: SHA-256
`4FF7B4A64836DFBB1925E7FD564B4D48467FF7CF65732C3BC35C8666A7E93776`. A skinned control,
`build/_deps/assimp-src/test/models/glTF2/simple_skin/simple_skin.gltf` at `--anim-time 0.5`, is also
identical between P1.6 and P1.7: SHA-256
`E08C555390663C010E7C94A29EF3E771EF87878D558BF63F99F905EF3F006CE1`. The skin image was opened and
inspected. A textured `BoxTextured.glb` control also matched P1.6 at frame 10: SHA-256
`9EEBF93F698D76003A4A1E9129188487EB6D046D565932BA1CEBE48EAA25CCE3` for both; the image was viewed
and shows the expected texture, lighting and IBL background. The P1.7 diagnostic-ON and Debug builds
produced the same 512 frame hash `4FF7B4A64836DFBB1925E7FD564B4D48467FF7CF65732C3BC35C8666A7E93776`,
with no GL-error logs.

**Limitations.** Reserving the temporary `Defines` map changes its bucket layout, and `Defines::hash`
hashes in iteration order; shader variant keys therefore change between P1.6 and P1.7 even though the
rendered output is identical. There is no persistent shader cache in this checkout, so this costs only
a one-time variant selection per process; a future disk shader cache would see a one-time invalidation.
The allocation benchmark exercises the changed containers, not every allocation in command generation.
The final executable was relinked after the diagnostic builds, so its PE-level hash differs from the
measured candidate; the final 512 repeat and the source/configuration checks cover that relink.

**Decision.** Accept P1.7 on the full-frame and `commands.generate` results, with identical rendered
output and a measured allocation reduction. Rollback boundary: remove `Defines::reserve` and the
capacity reservations, restore `CommandMapping::update` to return a vector, and restore the
`CommandGenerator::m_commands` assignment. Do not convert this capacity reuse into a persistent command
cache in Phase 1.

<a id="section-12-15"></a>

### 12.15 Cumulative Phase 1 regression review (2026-10-21)

```text
Review status: ACCEPTED FOR CURRENT SUPPORTED SCOPE (independent review, 2026-10-21)
Final Release OFF: H:\git-repos\gltf-viewer\build\bin\Release\Glider.exe
  SHA-256 46F37730934B12E3902770264B5B4E76F2711239BF4D58C2BAC9F5568E41BDF7
P1.0 baseline: G:\temp\uray-codex\opencode\glider-p1\builds\base_p1_0.exe
  SHA-256 57B68AC8C95BDF7D726F63B475D9FDC79C3A7C1ECD7428F833D8C384FE273F1D
Release diagnostic ON: G:\temp\uray-codex\opencode\glider-p1\builds\p1_7_diagnostic_on.exe
  SHA-256 D933EC6458362B662657F9900440C860554EE8A8B5038770DBAF50A56DD7F6EC
Final configuration: GLIDER_GL_DEBUG_CHECKS:BOOL=OFF (build/CMakeCache.txt)
Evidence root: G:\temp\uray-codex\opencode\glider-p1\cumulative\
Key fixtures:
  s5_materials_cameras.glb, 4056 bytes, FNV-1a64 0x21950f0bf2e04543
  p1_1_no_sampler.glb, 1916 bytes, FNV-1a64 0x2dbfa95941e16158
  one_sided_plane_back_camera.glb, 1260 bytes, SHA-256 A00DE1DA...0A2C8A,
    FNV-1a64 0xff1897fea3a7f8c2 (one single-sided quad viewed from its back face)
Follow-up UI evidence manifest:
  mv_followup_evidence.txt (capture paths and SHA-256 hashes)
```

**Reason for this review.** The per-task records did not by themselves establish the cumulative
scenario union. The earlier statement that the Phase 1 exit gate was met is withdrawn. This review ran
S0, S5, S6 and the executable parts of S9 on the final Release OFF link, repeated the safety-relevant
cases with the diagnostic ON build, and used the P1.0 build as a behavior control. Images were opened
and inspected; no GL-error strings appeared in any recorded stderr log.

**Results.** Hashes are SHA-256 of the raw viewport PNG unless marked as a window capture.

| Scenario | Final OFF | Control / diagnostic | Result |
| :--- | :--- | :--- | :--- |
| S0 default `HelloCube`, `--select-node "Hello Cube"` | `CFEBD4E6222C7A21420B5BC483352B1C7D6930AB7F5B1ECA7D8F34D18DCC76F4` | P1.0 identical; diagnostic ON identical | PASS |
| S5 opaque/mask/blend/transmission, two samplers over one image | `C2AF965D6A8E4A40F019F7FE935B26D6353637FD34DF769CDE0E192D478001EF` | P1.0 and diagnostic ON identical | PASS for run |
| S6 two viewports, camera 2 selected | `04B5E15887D3F613826E58FD7C62249A36BEF85BECB9F62621590BE2DA2A1EB5` | P1.0 and diagnostic ON identical | PASS (settings/picking follow-up below) |
| S6 full window, two visible viewport tabs | `C1F59164C12139BCE11CBAEC5B99E9F67A564593E04349B6007643515513A54F` | P1.0 and diagnostic ON identical | PASS (settings/picking follow-up below) |
| S6 one close/reopen cycle plus `Ctrl+L` | `A35BFC4B3327BC59109F0804E506EB02DB90C754C5C9F63893D2BE8B8B6FFEBC` | P1.0 and diagnostic ON identical | PASS |
| S6 per-viewport settings (shading, AA, culling) | full-window UI captures (see below) | P1.0 culling control matched | PASS for supported settings |
| S6 viewport-2 picking | full-window capture plus Logger (`Picked: Mask`, `Picked: Transmission`) | P1.0 selection matched | PASS for supported picking |
| S9 three close/reopen cycles | `04B5E15887D3F613826E58FD7C62249A36BEF85BECB9F62621590BE2DA2A1EB5` | P1.0 identical | PASS |
| S9 in-process A → B → A | `D8AEEFE00489A649C36D621086BF1531392C1FA105BBB8EA00F3F4A0C6A20EA8` | P1.0 and diagnostic ON identical | PASS for load sequence |
| S9 standalone A control | `E470939E13A9843CC72A3C4207F7F49CA6CC642467B1722EB96F1DBDE2A17BF3` | P1.0 identical | PASS as control |
| P1.1 untextured/no-sampler control | `7CF21C8157DA26F1CE8D1A758E39866217185BFF5B593C3AD80338C3A516AFAE` | P1.0 and diagnostic ON identical | PASS |

**S5 material/pass coverage.** The current fixture has four cubes sharing one image through two
distinct samplers and exercises opaque, alpha-mask, alpha-blend and `KHR_materials_transmission`
materials. A completed frame contains the expected pass sequence
`Opaque` ×3, `Transparent`, `TransmissiveBlit`, `Opaque`, `Picking/Opaque`, `Transmissive`,
`Picking/Transmissive`, `Transparent`, `Picking/Transparent`. The final and diagnostic captures are
byte-identical. The previously recorded S5 hash `09769F88…` was produced before the fixture's camera
nodes were edited and is retired; it must not be compared with the current fixture.

**S6 multi-viewport coverage.** The 960×640 run creates viewport 2 as
`Camera: CameraPerspective` with FXAA/1 sample; the scenario report confirms viewport ID 2 and the
viewport list shows `Viewports (2)`. A 1280×720 full-window capture shows both tabs and the hierarchy
entries. Resizing at 960×640 and 1280×720 was exercised. `Ctrl+3` toggled the active second viewport;
the profile probe shows viewport 2 absent for frames 21-71 while viewport 1 rendered, then viewport 2
returned. Three repeated close/reopen cycles produced three gaps in the final build
(36-69, 104-137, 172-205) and the P1.0 build (32-64, 100-133, 169-200). The final captures match the
P1.0 and diagnostic builds. This is a viewport visibility toggle, not viewport destruction/recreation.

**S6 per-viewport settings (follow-up).** At 1600x1000, viewport 2 was switched through the Inspector's
`Render Modes` tab to `Wireframe`, `Off (No AA)` and `Force Double-Sided (Disable Backface Culling)`.
Viewport 1 remained `None / Shaded (Default)`, `FXAA (Fast Approximate)` with force double-sided off.
The two viewport settings survived switching tabs in both directions; the viewport-2 toolbar remained
`Wireframe` while viewport 1 remained `Default Shaded`. The UI captures are
`mv_settings_probe3\p2_off_wire_doublesided.png`, `mv_settings_probe3\p2_confirm.png` and
`mv_settings_probe3\p1_fxaa_shaded_culled.png`; the last capture's Inspector shows viewport 1's
independent FXAA/shaded/culling-off state even though its free camera had not yet been framed. A
separate `mv_viewport1_frame_probe\p1_framed.png` shows the fitted viewport-1 scene in the same shaded
mode. These are UI/window captures, not deterministic raw viewport PNGs, and are used here only to
verify the setting values and viewport identity. Source confirms the per-viewport use: AA is selected
from `vp->settings.antiAliasingIdx` (`source/imgui/main.cpp:2959-2960`) and debug mode/culling defines
come from `vp->settings.renderMode` and `vp->settings.forceDoubleSided` (`3062-3091`), inside the
per-viewport render loop.

**S6 culling effect (follow-up).** The new `one_sided_plane_back_camera.glb` fixture places a
single-sided quad between the scene and a glTF camera that views its back face. In viewport 2, the quad
was absent with force double-sided off (`mv_culling_probe\initial_culled.png` and
`mv_culling_probe\force_double_sided_off.png`), appeared as the expected red surface with the option on
(`mv_culling_probe\force_double_sided_on.png`), and disappeared again when the option was unchecked.
The P1.0 control produced the same culled/visible/culled sequence
(`mv_culling_control_p1_0\initial_culled.png`, `...force_double_sided_on.png`,
`...force_double_sided_off.png`) and was inspected for the same result. This closes the supported
per-viewport culling check; it does not claim viewport destruction.

**S6 viewport-2 picking (follow-up).** With viewport 2 active and the Logger opened (`Ctrl+5`), a click
on the opaque/mask geometry logged `Picked: Mask` and selected `Node ID: 1` / `Mask` in the Inspector;
a second click selected `Node ID: 3` / `Transmission` and logged `Picked: Transmission`. The captures
are `mv_pick_probe2\before_pick.png`, `mv_pick_probe2\after_pick_left.png` and
`mv_pick_probe2\after_pick_right.png`. Selecting a node after the click blanked the attached-camera
viewport image in this fixture; the P1.0 control reproduced the same post-selection blanking and the
same Inspector/log selection (`mv_pick_control_p1_0\after_pick.png`), so this is recorded as existing
selection behavior, not a Phase 1 regression. The picking identity and selection result are correct.
No per-viewport picking code was changed.

**S9 load and viewport-cycle coverage.** With synthetic keyboard input, the process loaded A
(`s5_materials_cameras.glb`), then B (`p1_1_no_sampler.glb`), then A again; the stderr log shows all
three loads. The final A capture is byte-identical across final, P1.0 and diagnostic builds. The
standalone A capture is also byte-identical across final and P1.0, but differs from the A-after-reload
capture in both builds; the load-sequence difference is therefore pre-existing and not a Phase 1
regression. Three viewport close/reopen cycles also match P1.0 byte-for-byte. The images were
inspected and show the same scene content.

**Diagnostic configuration.** S0, S5, S6 (single viewport, full window and cycle), S9 A→B→A and the
P1.1 control were rerun with `p1_7_diagnostic_on.exe`; every corresponding final/diagnostic PNG was
byte-identical and every stderr log was clean. The default final build still has
`GLIDER_GL_DEBUG_CHECKS=OFF`; reduced
Release diagnostic coverage remains the P1.6 tradeoff and is not proof that the candidate can never
emit a GL error.

**Unsupported cases and applicability.** Two planned cases are not features of this viewer, so they
are recorded as not applicable rather than as untested product requirements:

- **S9 scene switch: not applicable.** `loadGltfModel` always selects `getDefaultScene()` and falls
  back to the first scene (`source/imgui/main.cpp:650-652`). The Resources panel can select a
  `Scene` for inspection (`source/imgui/ResourcesPanel.cpp:236-238`), but no UI or CLI path reassigns
  `activeScene` or calls `updateScene` with another scene; `ViewportSettingsPanel` only uses the
  default scene for its camera reset (`ViewportSettingsPanel.cpp:328-334`). The applicable S9 coverage
  is therefore the model A→B→A reload sequence already recorded above, not an in-file scene switch.
- **Viewport destruction/recreation: not applicable.** The viewport close button and `Ctrl+3` only
  toggle `ViewportInstance::isOpen` (`source/imgui/main.cpp:2433-2436`, `2701-2709`); no path erases a
  viewport or destroys/recreates its GPU resources while the application is running. The supported
  lifecycle is the close/reopen visibility cycle already tested, and that is what the S6/S9 evidence
  covers. Resource-release-on-destruction is reserved for a future feature, not a Phase 1 scenario.

**Supported coverage is complete.** All required scenarios that the current viewer actually implements
now have final-build evidence: S0, S5, S6 (including per-viewport shading, AA, culling and viewport-2
picking), the S6 close/reopen cycle, S9 A→B→A reload and repeated viewport cycles, and the P1.1 control.
No supported S6/S9 scenario remains NOT RUN. The only remaining caveat is the pre-existing
post-selection blanking of the attached-camera viewport described above; it reproduced identically on
P1.0 and does not change picking identity or selection.

**Gate decision.** Phase 1 is **accepted for the current supported scope** after independent review
on 2026-10-21. The reviewer inspected the follow-up settings, culling and picking captures, compared
the baseline culling/selection evidence, checked the source for unsupported lifecycle operations,
and rebuilt Release successfully. The executable remains SHA-256 `46F37730934B12E3902770264B5B4E76F2711239BF4D58C2BAC9F5568E41BDF7`.
The corrected mapping test was independently rerun in the preceding review: both executables exited
zero and all 13 behavior records matched. No blocking Phase 1 finding remains. This acceptance uses
the recorded UI evidence; the reviewer did not independently repeat the interactive sequences or
the full performance series in this final review. The attached-camera post-selection blanking remains
a separate pre-existing issue, reproduced in P1.0. Scene switching and runtime viewport destruction
remain not applicable until those features exist. Phase 1 is ready to commit upon user instruction;
no commit or Phase 2 implementation was performed during this review.

