# Renderer Optimization — Shared Validation and Execution Rules

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

Apply the following current scope rule when reading the original protocol below: choose checks for the changed behavior, reuse unaffected passing evidence, and stop when those checks pass. Related tasks may be batched safely with separate task IDs/results. Broad matrices and repeated timing trials are required only for a specific regression risk or a timing claim, not every repair. Documentation corrections alone do not require renderer reruns. This supersedes older blanket one-task-only and repeat-measurement wording.

<a id="section-5"></a>

## 5. Risk-Ordered Implementation Plan

**Planning baseline: every task below is unimplemented unless [§12](RENDERER_OPTIMIZATION_REFERENCE.md#section-12) records a PASS for it.** This section
supersedes the earlier phase order and the follow-up audit order. The table in [§1](RENDERER_OPTIMIZATION_REFERENCE.md#section-1) is an opportunity
inventory, not an execution order;
its effort/risk estimates are preliminary. Use the risk assignments and gates below when scheduling work.
No phase authorizes a commit, push, merge or publication without the user's explicit instruction.

<a id="section-5-1"></a>

### 5.1 Execution Rules and Risk Model

Risk means the chance and reach of a correctness regression, not implementation effort or expected speedup:

| Risk | Typical change | Required evidence |
| :--- | :--- | :--- |
| R0 — observation | Timers, counters, reproducible capture | Instrumentation overhead and baseline repeatability |
| R1 — local | Remove redundant CPU work without changing rendering order or resource ownership | Focused behavior checks plus measurable cost/counter change |
| R2 — lifetime / invalidation | Persistent caches, GL state, resource reuse | Transition tests, rebuild/load tests and visual comparisons |
| R3 — scheduling / visibility / concurrency | Skip work, move it across frames/threads, change draw membership | Deterministic interaction/animation sequences and stale-state/lifetime tests |
| R4 — algorithm / quality | New shader data paths, batching, lower precision/resolution | Broad visual coverage, hardware coverage and explicit quality acceptance |

Execute **one task ID and one mechanism at a time**. Split multi-part tasks into separately measured
substeps; never combine a precision change, a resolution change and a sample-count change into one result.
An independently testable later task can proceed once its listed dependencies pass; phase numbering is
not a reason to implement unrelated features. Do not proceed to a dependent task with a failed gate.
A missing asset, unsupported measurement or unavailable interaction driver is **NOT RUN**, not a pass.

For every task, the executing agent must:

1. Re-read the cited source and current repository instructions; verify the observation still applies.
   Record existing local changes and preserve unrelated work. Establish a baseline from the actual
   starting tree, including any uncommitted changes, rather than assuming HEAD describes it.
2. Declare the hypothesis, affected behavior, test cases, primary metric and acceptance threshold
   **before editing**. Consult [§6](#section-6); state which unaffected workloads will serve as controls.
3. Capture baseline measurements and images, then make one bounded change. Use a temporary comparison
   switch or preserved baseline executable when helpful; avoid maintaining two permanent implementations.
4. Build, run focused behavior/visual checks, then paired baseline/candidate measurements using the same
   instrumentation. Capture source diff and evidence in the task record ([§6.6](#section-6-6)).
5. Mark PASS only when correctness and measurement gates pass. A plausible optimization with no
   repeatable benefit is INCONCLUSIVE or DEFERRED, not completed on the strength of intuition.
6. On regression, stop dependent work and isolate/fix or revert only that task's edits. Do not reset
   the user's tree. Preserve the failing artifact and measurements. Update the journal with the result.

<a id="section-6"></a>

## 6. Agent Measurement and Validation Protocol

No benchmarks or runtime validation were performed for the original planning revision. P0.1 has since
been implemented and validated ([§12.1](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-1)); all other findings remain source-based hypotheses. The following
is the required protocol for future implementation work.

<a id="section-6-1"></a>

### 6.1 Reproducible Scenario Matrix

Record exact assets/hashes and scenario settings. Examples below are fixture classes, not claims that
those external models are available in this checkout. Create small deterministic fixtures where useful;
if a required asset is unavailable, record the coverage gap rather than silently substituting a cube.

| ID | Scenario | Required coverage |
| :--- | :--- | :--- |
| S0 | Embedded HelloCube | Startup, idle, standalone launch, basic selection |
| S1 | Representative PBR object (e.g. DamagedHelmet) | Texture/sampler bindings, shading, all supported AA modes |
| S2 | 1k–5k primitives with repeatable camera path | Command/VAO cost, mapping scaling, selection, wireframe |
| S3 | Wide scene with most geometry outside view | Culling, camera travel, perspective/orthographic and near-plane edges |
| S4 | Skins, morphs, many clips and pointer animation | Play/pause/seek/stop, active clip changes, animated parents, material animation |
| S5 | Opaque, masked, blended and transmissive materials | Pass order, shared textures with distinct samplers, overlays |
| S6 | Two or more viewports | Different shading/culling/AA/cameras, resize, close/reopen, selection |
| S7 | Large GLB and supported image/HDR/EXR formats | Read/decode/upload/validation, peak RAM, first frame, replacement/failure |
| S8 | Long hierarchy/resource/issue/log lists | Panels open/closed, search, scrolling, wrapping, Copy, expansion and focus |
| S9 | Load A → B → A; scene switch; repeated viewport cycles | Cache invalidation, pointer reuse, resource release and stale results |

Every task runs S0 and its affected scenario rows, plus at least one unaffected control. Renderer/state
changes include S1/S5/S6/S9; animation changes include S4/S9. Build cumulatively at phase exit and run
the union of affected scenarios to catch interactions. Missing required coverage prevents a full PASS.

<a id="section-6-2"></a>

### 6.2 Existing Commands and Proposed Harness

Build using the repository workflow:

```cmd
cmake --build build --config Release --target Glider
```

Current screenshot controls (replace model/output placeholders with real paths):

```cmd
cmd /c "build\bin\Release\Glider.exe --aa none --dump-frame baseline_cube.png --exit-after-dump"
cmd /c "build\bin\Release\Glider.exe <model.glb> --aa msaa4 --dump-frame baseline_msaa4.png --exit-after-dump"
cmd /c "build\bin\Release\Glider.exe <model.glb> --aa taa --dump-frame baseline_taa.png --exit-after-dump"
```

Use PNG for comparison. The current dump captures the **window** after an eight-frame countdown;
it is a smoke test, not proof of TAA convergence or reproducible animated state. P0 must provide or
record explicit capture frame/time, viewport region and settings. Existing `--select-node` and
`--select-viewport` can help, but do not replace deterministic interaction sequences.

`--benchmark`, `--benchmark-warmup`, `--benchmark-frames`, `--benchmark-output`,
`--exit-after-benchmark`, `--vsync on|off`, `--capture-viewport`, `--capture-frame`,
`--capture-time` and `--capture-first-frame` are implemented (see [§12.4](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-4)–[§12.6](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-6)).
General scripted interaction playback remains future work. Profile reports wait for requested viewport
captures; do not combine an earlier benchmark/dump exit with a later capture. Keep report writes and
capture readbacks outside timed intervals. Detailed CPU/GPU history is capped at 512 entries; use the
reported retention ranges, intersect GPU frame IDs with the measured CPU range, and never assume a
long benchmark retains every GPU sample. The requested benchmark CPU sample vector remains separate.

<a id="section-6-3"></a>

### 6.3 Timing and Memory Procedure

1. Record CPU/GPU, driver, OS, executable/build configuration, source state, viewport dimensions,
   DPI, AA, vsync, model/environment hashes and settings. Use the same machine and power conditions.
2. Establish same-build repeatability first. For steady state, use at least five baseline/candidate
   pairs in alternating order, initially 120 warmup frames and 600 measured frames per run. Increase
   warmup if shaders/IBL/uploads are still active; document the change. Keep startup out of these runs.
3. Report per-run median and p95 CPU frame/stage time and GPU pass time, then paired deltas across
   runs. Retain raw samples; do not pool all frames into a single average that hides run-to-run noise.
4. Measure load/startup separately with fresh processes. Label warm filesystem/driver-cache runs;
   do not call a fresh process a cold-cache test. Record process start → first usable frame and
   validation/filter completion separately. Use at least five runs per build for startup comparisons.
5. Record application-owned texture/buffer byte estimates, live object counts and peak process RAM.
   Driver VRAM is optional and must identify its measurement source; unavailable is not zero. For
   lifetime tasks, run at least ten load/close cycles and inspect retained counts after each cycle.
6. Define idle tests separately with vsync at the user's normal setting: time to quiescence, frames
   rendered during a fixed 30-second idle window and input-to-visible-response latency. Compare GPU
   utilization/power only if reliable collection is available. Do not use capped FPS to judge CPU gains.

Timers/counters used for baseline and candidate must be identical. Delayed GPU query collection,
logging, screenshot readback and asset generation must not contaminate timed steady-state frames.

<a id="section-6-4"></a>

### 6.4 Correctness and Visual Gates

- Build the affected target in Release and run diagnostics in a supported Debug/GL-check configuration.
  Record exact commands, exit codes and errors. Check standalone launch from a directory without an
  adjacent resources folder after resource/build changes; retain the OpenGL 4.1/no-Qt/no-TBB constraints.
- Capture matching PNGs and **view the actual images** after major viewport/camera/renderer work.
  Compare fixed viewport regions; separate dynamic UI text/cursor from rendering instead of masking
  arbitrary pixels. Record capture coordinates and any excluded UI region.
- Establish image variability with repeated baseline captures. For deterministic lossless changes,
  require exact viewport pixels when the baseline is exact. Otherwise declare a bounded tolerance from
  baseline variability before implementation and report maximum channel error, mean error and fraction
  of changed pixels. Inspect difference images for localized errors; averages alone can hide missing meshes.
- TAA tests must use matching history reset, jitter sequence, frame count and animation time. Capture
  both transition frames and settled output. A single eight-frame dump is insufficient for redraw changes.
- Validate interactions numerically where possible: selected node IDs at known clicks, revisions and
  rebuild counts, conflict results, report contents, list counts, animation poses and resource ownership.
  Frame similarity alone cannot prove input timing, off-screen list behavior or safe cache lifetimes.
- R4 quality changes require task-specific tolerances and visual approval established before work.
  Do not broaden an image threshold after seeing a regression just to turn the gate green.

<a id="section-6-5"></a>

### 6.5 Pass / Fail / Inconclusive Rules

Choose the primary metric to match the task: CPU/GPU time, peak RAM/live bytes, allocations, startup
latency or idle work. A memory optimization does not need an FPS win, but still must avoid time regressions.
Do not carry over earlier blanket promises of 40–50% fewer total draws or 5–10× less total VRAM.

Default timing decision rule, unless a different budget is recorded before work:

- Compute baseline run noise as the range of the five baseline per-run values divided by their median.
  If noise exceeds 5%, stabilize the environment or increase repetitions before deciding a timing win.
- Require median paired improvement greater than both 3% and the observed baseline noise, with improvement
  in at least four of five pairs. This is an operational gate, not a claim of statistical significance.
- A >5% control-workload or p95 regression that also exceeds its baseline noise requires investigation
  and a rerun; persistent regression fails unless an explicit, pre-agreed tradeoff budget covers it.
  State absolute milliseconds as well as percentages so tiny denominators do not mislead.
- For deterministic counts/bytes, require the predicted reduction and correct invalidation/lifetime
  behavior; report timing as unchanged if it is within noise. Do not translate fewer calls into invented ms.
- Any correctness failure is FAIL regardless of speed. Missing fixtures/tools are NOT RUN. Results
  inside noise are INCONCLUSIVE. Optional complexity without demonstrated benefit is DEFERRED.

A task passes only after its specific phase/task gate and applicable [§6](#section-6) checks pass. A phase passes
only when selected constituent tasks and cumulative scenario checks pass; deferred optional tasks
remain explicitly unchecked. Stopping work or exhausting time never means a gate passed.

<a id="section-6-6"></a>

### 6.6 Per-Task Evidence Record (Template)

Append a completed record or link a reproducible local evidence bundle when implementing a task:

```text
Task ID / title:
Status: PLANNED | IN PROGRESS | PASS | FAIL | INCONCLUSIVE | NOT RUN | DEFERRED
Source state: base revision + working-tree diff/hash; baseline/candidate executable hashes
Dependencies and gates satisfied:
Hypothesis / mechanism / expected counter change:
Primary metric / predeclared thresholds / unaffected control:
Environment and exact build/run commands:
Scenario IDs, asset hashes, settings, action sequence, frame/time:
Baseline and candidate: per-run values, median/p95, noise, paired deltas, units
Memory/counters: predicted vs observed; measurement source
Correctness: diagnostic result, interaction assertions, image metrics and viewed artifact paths
Coverage gaps / limitations / known tradeoffs:
Decision and rationale:
Rollback boundary / remaining follow-up tasks:
```

Keep baseline/candidate PNGs, difference images, raw timings and machine-readable counters under a
clearly identified task output directory. Do not commit large assets, captures or generated logs by
default. Preserve enough source/build context to reproduce a baseline after further edits. Update the
checkbox only on PASS; other outcomes belong in the record and remain open in the task list.

---
<a id="section-7"></a>

## 7. Guardrails and Non-Goals

* **Do not** introduce Qt, TBB or other heavy frameworks. Use C++20 standard library facilities.
* **Do not** delete or rewrite `legacy/` Qt code; it remains reference material.
* **Do not** add runtime dependencies on `resources/` on disk. All assets must continue to load via
  `EmbeddedVFS`.
* **Do not** trade correctness for speed: debug builds must still surface GL errors, and all reference
  frames must match.
* **Not in scope yet**: physically based rendering changes, new features, shader quality upgrades.
* **The journal began as a planning document. P0.1 ([§12.1](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-1)) is the first implemented task; no renderer
  optimization has been implemented.**

---

<a id="section-8"></a>

## 8. Open Questions

1. **Target hardware floor**: should the primary target be integrated GPUs (Intel Iris / AMD Vega) or
   discrete GPUs? This affects the priority of fill-rate, MSAA and IBL memory work.
2. **Typical scene size**: what is the expected maximum drawable count in real use? This determines
   whether command caching or frustum culling should come first.
3. **Selection latency tolerance**: is one frame of latency for picking acceptable? If yes, on-demand
   picking is trivial; if not, a 1×1 scissored pass in the same frame is safer.
4. **IBL quality bar**: is a 512² RGBA16F environment acceptable visually, or must 1024² be preserved?
5. **Startup budget**: how long may first-frame IBL filtering take? If it must be instant, pre-baking
   the environment into the resource archive is the only clean solution.
6. **Profiling access**: can we run on the target hardware with GPU timer queries, or should we rely on
   CPU timing and counters only?
7. **Shader cache**: is writing a shader binary cache to `%APPDATA%/Glider` acceptable under the
   standalone policy? (Window state is already persisted there.)

---

<a id="section-9"></a>

## 9. Appendix A — Per-Drawable GL Call Budget (Current)

Approximate values for one typical opaque drawable with a standard PBR material:

| Category | Calls | Notes |
| :--- | ---: | :--- |
| Program | 1 `glUseProgram` | Always, no state cache |
| Uniforms | ~25–35 `glUniform*` + ~25–35 `glGetError` | Camera 6, env 4–5, lights N, material ~10, mesh ~5, exposure 1 |
| Samplers | ~10–18 × (`glActiveTexture` + `glBindTexture` + 5 `texParameteri` + `hasExtension` + `glGetFloatv` + `glGetError` × 2 + `glUniform1i`) | Includes env maps and LUTs |
| Uniform blocks | 0–1 `glBindBufferBase` + `glGetError` | Skinning / morph |
| Geometry | 1 `glBindVertexArray` (new VAO created) + 1 buffer bind + 1 draw | VAO is created and deleted every frame |
| Render state | `glPolygonMode` + `glFrontFace` + `glEnable/glDisable(GL_CULL_FACE)` + `glCullFace` + 4 `glGetError` | Unconditional |
| **Total** | **≈ 80–150 driver calls + 1 VAO alloc/free** | per drawable per frame |

With picking enabled, this budget applies twice per drawable.

---

<a id="section-10"></a>

## 10. Appendix B — Relevant Files

| Area | File |
| :--- | :--- |
| Main loop, frame orchestration, picking readback | [`source/imgui/main.cpp`](file:///H:/git-repos/gltf-viewer/source/imgui/main.cpp) |
| Command generation and per-drawable rebuild | [`renderer/source/CommandMapping.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/CommandMapping.cpp) |
| Pass sorting and command grouping | [`renderer/source/CommandGenerator.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/CommandGenerator.cpp) |
| Pass execution order | [`renderer/source/Renderer.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/Renderer.cpp) |
| Per-command GL submission | [`renderer/source/CommandExecutor.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/CommandExecutor.cpp) |
| GL error checks | [`renderer/source/gl/Util.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/gl/Util.cpp) |
| Uniform / sampler / texture / block commits | [`renderer/source/gl/`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/gl/) |
| VAO creation | [`renderer/source/gl/VertexLayout.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/gl/VertexLayout.cpp) |
| Shader variant cache | [`renderer/source/gl/ShaderCache.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/gl/ShaderCache.cpp) |
| Scene construction and meshes | [`renderer/source/gltf/GltfRenderLayer.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/gltf/GltfRenderLayer.cpp) |
| Mapping algorithm | [`renderer/include/renderer/Mapping.h`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/include/renderer/Mapping.h) |
| GPU picking layer | [`renderer/source/PickingRenderLayer.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/PickingRenderLayer.cpp) |
| IBL filtering and memory | [`renderer/source/filtering/IBLSampler.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/filtering/IBLSampler.cpp) |
| Scene FBO / MSAA | [`source/imgui/Framebuffer.cpp`](file:///H:/git-repos/gltf-viewer/source/imgui/Framebuffer.cpp) |
| FXAA / TAA | [`source/imgui/AntiAliasing.cpp`](file:///H:/git-repos/gltf-viewer/source/imgui/AntiAliasing.cpp) |
| Grid | [`source/imgui/GridRenderer.cpp`](file:///H:/git-repos/gltf-viewer/source/imgui/GridRenderer.cpp) |
| Bounding boxes & OBB | [`source/imgui/BoundingBoxRenderer.cpp`](file:///H:/git-repos/gltf-viewer/source/imgui/BoundingBoxRenderer.cpp) |
| Animation player & channel sampling | [`source/imgui/AnimationPlayer.cpp`](file:///H:/git-repos/gltf-viewer/source/imgui/AnimationPlayer.cpp) |
| Skeletal skinning & joint matrices | [`OpenGL-Renderer/renderer/source/Skin.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/renderer/source/Skin.cpp) |
| Image loading & texture decoding | [`OpenGL-Renderer/Tools/tools/gltf/source/Image.cpp`](file:///H:/git-repos/gltf-viewer/OpenGL-Renderer/Tools/tools/gltf/source/Image.cpp) |
| Scene hierarchy tree rendering | [`source/imgui/SceneHierarchyPanel.cpp`](file:///H:/git-repos/gltf-viewer/source/imgui/SceneHierarchyPanel.cpp) |

---

<a id="section-11"></a>

## 11. Changelog

| Date | Change |
| :--- | :--- |
| 2026-10-20 | Initial audit journal created. No code changes. |
| 2026-10-20 | Architectural review & refinements: added multi-viewport concurrency analysis, dynamic `u_debugMode` uniform proposal, program-independent mesh VAO caching, shared 1×1 picking FBO design, selection `isSceneDirty` quick win, and custom OpenEXR memory implications. |
| 2026-10-20 | Comprehensive deep-dive optimizations added: [§3.15](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-15) animation playback scene rebuild decoupling & inactive clip filtering, [§3.16](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-16) parallel CPU texture decoding on file load, [§3.17](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-17) skinning joint vector allocations & UBO migration, [§3.18](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-18) selected node OBB subtree caching, and [§3.19](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-19) ImGui hierarchy panel label formatting and virtualization. |
| 2026-10-20 | Follow-up source audit: added eight unimplemented task groups ([§3.20](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-20)–[§3.27](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-27)), priority entries and measurement/acceptance criteria covering AA allocations, Statistics, resource/validation lists, logging, animation conflict indexing, dirty subtrees/skins, validation input reuse and duplicate load synchronization. Corrected unmeasured CPU/GPU-bound wording, inactive-animation channel claims, animation-pointer scope, and joint-storage accounting / OpenGL 4.1 constraints. Documentation only; no benchmarks or implementation. |
| 2026-10-20 | Replaced the implementation order with risk levels R0–R4 and phases P0–P8. Added stable task IDs, dependencies, phase gates, per-task rollback/evidence rules, a scenario matrix, paired timing/noise criteria, deterministic image checks and explicit PASS/FAIL/INCONCLUSIVE/NOT RUN outcomes. Separated capability caching from sampler objects, scratch reuse from skin data redesign, full-size on-demand picking from 1×1 picking, redraw skipping from event waits, and IBL quality experiments from transparent optimizations. Planning only; no runtime changes or measurements. |
| 2026-10-20 | Implemented and validated **P0.1 - Deterministic scenarios** ([§12.1](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-1)). Added `--isolated`, fixed `--window-size`, explicit camera/projection flags and a `glider.scenario.v1` JSON manifest. Repeated S0, S4-like and TAA runs produced byte-identical reports and PNGs; isolated runs left persisted window/layout/settings state unchanged. No renderer optimization was implemented. |
| 2026-10-21 | Implemented and validated **P0.2 - CPU counters/timers** ([§12.2](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-2)). Added opt-in `glider.profile.v1` reports with startup phases, named frame stages, renderer-wrapper GL/resource counters and explicit application counters. Three S0 runs produced identical steady counters and byte-identical PNGs; first-frame setup is recorded separately. No renderer optimization was implemented. |
| 2026-10-21 | Implemented and validated **P0.3 - GPU timings** ([§12.3](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-3)). Added `GpuProfiler` with one non-overlapping `GL_TIME_ELAPSED` query per render pass, deferred availability polling at least two submissions later, no `glFinish`, and explicit pending/unavailable samples. S0, a four-pass transparent control and Fox/Duck controls were rerun on the historical baseline build; CPU submission (`draw.submit`) is reported separately from GPU pass time. |
| 2026-10-21 | Implemented and validated **P0.4 - Benchmark controls** ([§12.4](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-4)). Added pre-scanned `--vsync on|off`, bounded `--benchmark` with warmup/measured counts, `glider.benchmark.v1` JSON, first-frame separation and `--exit-after-benchmark`. Final 60-frame S0 controls measured 0.724 ms median interval with vsync off versus 17.233 ms with vsync on. |
| 2026-10-21 | Implemented and validated **P0.5 - Capture and baseline bundle** ([§12.5](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-5)). Added raw viewport-texture PNG capture at a fixed frame or elapsed time, separate first-frame capture, capture timing/hash metadata and a final single-build evidence bundle. Five steady S0 captures and three TAA captures were byte-identical; instrumentation overhead was below host noise at roughly 0.01-0.02 ms in paired medians. Phase 0 exit gate recorded as met for the pre-review baseline; no renderer optimization was implemented. |
| 2026-10-21 | Applied **P0.6 - Phase 0 measurement/reporting corrections** ([§12.6](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-6)) after review found four P2 defects. Profile reports now wait for a requested capture; report/capture/dump write failures return exit code 1 and separate attempted from successful writes; texture storage/upload accounting uses internal/transfer formats with explicit unknown-call counters and renderbuffer sample accounting; CPU/GPU samples are capped at 512 with retained ranges/drop counts exposed. Focused tests passed at build `Oct 21 2026 03:46:56` (SHA-256 `57B68AC8...`); the pre-review [§12.3](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-3)-[§12.5](RENDERER_OPTIMIZATION_PHASE_0.md#section-12-5) evidence remains historical. No rendering speedup claimed. |
| 2026-10-21 | Completed the **cumulative Phase 1 regression review** ([§12.15](RENDERER_OPTIMIZATION_PHASE_1.md#section-12-15)) and withdrew the earlier exit-gate claim. Corrected the P1.2 mapping test so its recreation branch is reachable, added deferred duplicate-input coverage, and asserted callback order/map contents immediately after each operation. Re-ran S0, current-fixture S5, S6 two-viewport/resize/close-reopen, S9 A→B→A and repeated viewport cycles on the final Release OFF build; repeated safety-relevant cases with diagnostic ON; P1.0 served as the control. Final/diagnostic outputs were byte-identical. At that point Phase 1 was recorded as **implemented; final regression gate pending** because S9 scene switching and S6 per-viewport shading/culling/AA were still NOT RUN; the follow-up row below closes the supported cases. |
| 2026-10-21 | Completed the **Phase 1 supported multi-viewport follow-up** ([§12.15](RENDERER_OPTIMIZATION_PHASE_1.md#section-12-15)). Exercised independent viewport-2 `Wireframe`, AA `Off` and force-double-sided settings against viewport-1 `Default Shaded`/`FXAA`/culling-off, verified the culling effect with a one-sided-plane fixture and the P1.0 control, and confirmed viewport-2 picking (`Picked: Mask`, `Picked: Transmission`) with matching Inspector selection and P1.0 behavior. Source-confirmed that in-file scene switching and runtime viewport destruction are not viewer features, so those S9/S6 cases are documented as not applicable rather than added as product features. Phase 1 remains **implemented; final regression gate pending final acceptance**; no commit or push was made. |

---

