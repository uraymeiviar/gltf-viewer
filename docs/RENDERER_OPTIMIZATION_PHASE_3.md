# Phase 3 — GL Object Lifetime and Binding Caches

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Complete. Accepted in commit `3c86ff7`.

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-5"></a>

### 5.5 Phase 3 — GL Object Lifetime and Binding Caches (R2)

**Entry:** Phase 0; P1.1 before P3.2. Keep scene rebuild frequency unchanged for comparisons.

- [x] **P3.1 — AA allocation by mode ([§3.20](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-20)).** Allocate only needed targets; validate resize, rapid
  mode switches, viewport closure and TAA history reset. Measure live bytes and creation/deletion counts.
  *Implemented and validated in [§12.25](#section-12-25).*
- [x] **P3.2 — Sampler objects ([§3.2](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-2)).** Replace repeated per-bind texture sampler updates with cached GL sampler objects. Validate multi-unit, successive draws, wraps, filters, anisotropy, boundary unbinding, and multi-viewport isolation. *Implemented and validated in [§12.26](#section-12-26).*

- [x] **P3.3 — VAO reuse ([§3.4](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-4)).** Begin with a conservative mesh/program/layout-generation key. Test
  index buffers, missing/optional attributes, shader variants and mesh destruction/reload. Simplify to
  a mesh-only key only after proving layout compatibility across all participating shaders/passes;
  fixed locations in `pbr.vert` alone do not establish that invariant. *Implemented and validated in [§12.27](#section-12-27).*
- [x] **P3.4 — Executor shadow state ([§3.3](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-3)).** Introduce one state category per substep. Invalidate at
  external-function commands, renderer boundaries and context changes. Test grid/bounds, ImGui, AA,
  transparency/transmission, wireframe and multiple viewports interleaved with scene rendering.
  *Implemented and validated across P3.4.1 (Polygon mode, [§12.28](#section-12-28)), P3.4.2 (Culling, [§12.29](#section-12-29)), P3.4.3 (Front-face winding, [§12.29](#section-12-29)), P3.4.4 (Depth test/func, [§12.30](#section-12-30)), and P3.4.5 (Blending state, [§12.31](#section-12-31)). Combined integration and Phase 3 exit gate audited in [§12.31](#section-12-31).*

**Exit gate:** diagnostic build and frame comparisons pass the render-state matrix; repeated
open/close/reload cycles show no accumulating owned GL objects. Intended driver-call/creation counts
fall. Memory calculations are reported separately from driver-reported VRAM.
*Phase 3 accepted and committed as `3c86ff7`; see [§12.31](#section-12-31) for evidence and limitations.*

<a id="section-12-25"></a>

### 12.25 P3.1 — Anti-Aliasing Target Allocation by Active Mode (2026-10-22)

#### 1. Context and Problem Statement

Prior to P3.1 ([§3.20](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-20)), rendering the first post-processed frame in FXAA or TAA unconditionally allocated
all three full-resolution RGBA8 textures and three framebuffers:
1. One FXAA output texture + FBO (`m_fxaaTexture`, `m_fxaaFbo`).
2. Two TAA ping-pong history textures + FBOs (`m_taaTexture[0..1]`, `m_taaFbo[0..1]`).

Under that baseline implementation:
- Active **FXAA** allocated and retained two unused TAA history targets (2× unneeded storage).
- Active **TAA** allocated and retained one unused FXAA target (1× unneeded storage).
- Switching from FXAA or TAA back to **None** or **MSAA** never freed the post-processing textures;
  all three full-resolution targets remained alive indefinitely in GPU memory.
- In multi-viewport configurations, each viewport accumulated all three targets, multiplying dead storage.

At 4K UHD (3840×2160), each RGBA8 texture requires approximately 31.64 MiB (33.18 MB) of estimated pixel storage.
Retaining unused targets wasted up to 63.28 MiB per viewport in FXAA and 94.92 MiB per viewport in None/MSAA.

#### 2. Implementation & Architecture

Changes were implemented strictly within `header/imgui/AntiAliasing.h`, `source/imgui/AntiAliasing.cpp`, and callers in `source/imgui/main.cpp`.

1. **Target Ownership and Release Policy:**
   - Active mode **None** and **MSAA (2x/4x/8x)**: Owns 0 post-process textures and 0 post-process FBOs.
     `AntiAliasing::releaseTargets()` releases all FXAA and TAA targets immediately when these modes become active.
   - Active mode **FXAA**: Owns exactly 1 output texture (`m_fxaaTexture`) and 1 FBO (`m_fxaaFbo`).
     Any allocated TAA targets are released immediately via `releaseTaaTargets()`.
   - Active mode **TAA**: Owns exactly 2 ping-pong history/output textures (`m_taaTexture[0..1]`) and 2 FBOs (`m_taaFbo[0..1]`).
     Any allocated FXAA target is released immediately via `releaseFxaaTargets()`.
   - **Viewport Closure Lifecycle**: When a viewport is closed (`!vp->isOpen`), `main.cpp` calls `vp->antiAliasing.releaseTargets()`,
     releasing all targets and dropping live pixel storage to 0 bytes while closed. Reopening reallocates only the active mode's targets.

2. **ImGui Draw Data Lifetime Invariant:**
   - In Dear ImGui, `ImGui::Image((ImTextureID)textureId, ...)` queues an `ImDrawCmd` referencing `textureId`.
     The OpenGL draw submission occurs at frame end inside `ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData())`.
   - Target allocation, reallocation (on resize), and inactive target releases occur exclusively at the start of
     `AntiAliasing::render()`, before the returned texture is submitted to `ImGui::Image()`.
   - The previous frame's ImGui draw list was already rendered and presented at the end of the previous frame.
   - This ordering supports safe texture lifetime during normal per-viewport rendering. Actual mixed-mode viewport closure/reopening through the application remains a targeted validation gap; the direct-object self-check does not exercise ImGui draw-data consumption.

3. **TAA History Invalidation & Reset:**
   - Any mode switch entering TAA, dimension changes (resizing), camera orientation jumps, or viewport re-openings
     explicitly set `m_historyValid = false` and `m_frameIndex = 0`.
   - On frames where `!m_historyValid`, `blendFactor` is forced to `1.0f` and the current input texture is bound
     as history (`m_historyValid ? m_taaTexture[prev] : inputColorTexture`), completely discarding stale history
     and preventing ghosting or smearing artifacts.

4. **Zero & Minimized Dimensions Safety:**
   - If `width <= 0 || height <= 0`, `render()` safely returns `inputColorTexture` without allocating 0×0 targets or executing post-process passes.
   - Restoration to valid positive dimensions reallocates cleanly without errors.

5. **Diagnostic Inspection API:**
   - Added public inspection queries on `AntiAliasing`:
     - `getOwnedTargetCount()`: returns current count of owned GL textures (0, 1, or 2).
     - `getOwnedFramebufferCount()`: returns current count of owned GL FBOs (0, 1, or 2).
     - `getEstimatedLiveBytes()`: returns total live pixel bytes ($W \times H \times 4 \times \text{owned}$).
     - `getTargetAllocationCount()` / `getTargetDeletionCount()` / `resetTargetCounters()`.
     - `runTargetAllocationSelfCheck(bool forceFailure)`: comprehensive automated self-check suite.

#### 3. Target Counts and Estimated Live Storage Comparison

*Note: Live storage calculations reflect uncompressed RGBA8 pixel storage ($W \times H \times 4\text{ bytes per texture}$) and are reported separately from driver-managed VRAM.*

| Resolution | Bpp | Mode | Baseline Targets | Baseline Live Storage | P3.1 Targets | P3.1 Live Storage | Storage Delta |
| :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **1280×720** (720p) | 4 | None / MSAA | 3 (retained) | 10.55 MiB (11.06 MB) | **0** | **0.00 MiB** | **-10.55 MiB (-100%)** |
| | 4 | FXAA | 3 | 10.55 MiB (11.06 MB) | **1** | **3.52 MiB (3.69 MB)** | **-7.03 MiB (-66.7%)** |
| | 4 | TAA | 3 | 10.55 MiB (11.06 MB) | **2** | **7.03 MiB (7.37 MB)** | **-3.52 MiB (-33.3%)** |
| **1920×1080** (1080p) | 4 | None / MSAA | 3 (retained) | 23.73 MiB (24.88 MB) | **0** | **0.00 MiB** | **-23.73 MiB (-100%)** |
| | 4 | FXAA | 3 | 23.73 MiB (24.88 MB) | **1** | **7.91 MiB (8.29 MB)** | **-15.82 MiB (-66.7%)** |
| | 4 | TAA | 3 | 23.73 MiB (24.88 MB) | **2** | **15.82 MiB (16.59 MB)** | **-7.91 MiB (-33.3%)** |
| **2560×1440** (1440p) | 4 | None / MSAA | 3 (retained) | 42.19 MiB (44.24 MB) | **0** | **0.00 MiB** | **-42.19 MiB (-100%)** |
| | 4 | FXAA | 3 | 42.19 MiB (44.24 MB) | **1** | **14.06 MiB (14.75 MB)** | **-28.13 MiB (-66.7%)** |
| | 4 | TAA | 3 | 42.19 MiB (44.24 MB) | **2** | **28.13 MiB (29.49 MB)** | **-14.06 MiB (-33.3%)** |
| **3840×2160** (4K UHD) | 4 | None / MSAA | 3 (retained) | 94.92 MiB (99.53 MB) | **0** | **0.00 MiB** | **-94.92 MiB (-100%)** |
| | 4 | FXAA | 3 | 94.92 MiB (99.53 MB) | **1** | **31.64 MiB (33.18 MB)** | **-63.28 MiB (-66.7%)** |
| | 4 | TAA | 3 | 94.92 MiB (99.53 MB) | **2** | **63.28 MiB (66.36 MB)** | **-31.64 MiB (-33.3%)** |

#### 4. Validation Results & Verification Evidence

1. **Deterministic Self-Check Suite (`--aa-target-selfcheck`)**:
   - 101/101 programmatic assertions passing (`build/p3.1/selfchecks_positive/aa_target_selfcheck.txt`).
   - Verifies:
     - Initial state (0 targets, 0 FBOs, 0 live bytes).
     - None & MSAA startup: 0 targets owned; 10 steady-state frames produce 0 allocations and 0 deletions.
     - FXAA startup: exactly 1 target, 1 FBO, 0 TAA targets; 10 steady-state frames produce 0 allocations and 0 deletions.
     - FXAA -> TAA transition: 1 FXAA target deleted, 2 TAA targets allocated.
     - TAA steady-state: 10 frames with ping-pong index alternation and 0 allocations/deletions.
     - TAA -> None transition: 2 TAA targets deleted, live bytes drops to 0.
     - None -> TAA transition: 2 TAA targets allocated, 0 FXAA targets.
     - Resizing in TAA (800×600 -> 1280×720): 2 old targets deleted, 2 new targets allocated, history invalidated; steady state post-resize produces 0 allocations/deletions.
     - Resizing in FXAA (1280×720 -> 1920×1080): 1 old target deleted, 1 new target allocated; steady state post-resize produces 0 allocations/deletions.
     - 50 rapid mode-switching cycles: total allocations (150) equal total deletions (150) with zero resource leaks; `glGetError() == GL_NO_ERROR`.
     - Zero/negative dimensions: safe return, no 0×0 texture allocation, clean restoration.
     - Multi-viewport independent allocation: 3 viewports in FXAA, TAA, and MSAA operate independently; closing viewport 2 drops its targets without affecting viewport 1.

2. **Negative Self-Check Coverage**:
   - Forced failure (`--aa-target-selfcheck ... --selfcheck-force-failure`): exit code 1 (`build/p3.1/selfchecks_negative/aa_target_forced_failure.txt`).
   - Unwritable path (directory target): exit code 2.
   - All 8 existing self-checks and negative checks pass identically.

3. **Visual Evidence (coverage limitations identified in review)**:
   - Compared stored candidate and baseline images (`HelloCube.glb`, 1280×720, frame 8 dump). These files compare exactly, but the runner does not regenerate the six candidate startup captures or enforce a mismatch threshold:
     - `frame_none.png`: 921,600 px, 0 differing (0.0000%), max channel diff 0.
     - `frame_msaa2.png`: 921,600 px, 0 differing (0.0000%), max channel diff 0.
     - `frame_msaa4.png`: 921,600 px, 0 differing (0.0000%), max channel diff 0.
     - `frame_msaa8.png`: 921,600 px, 0 differing (0.0000%), max channel diff 0.
     - `frame_fxaa.png`: 921,600 px, 0 differing (0.0000%), max channel diff 0.
     - `frame_taa.png`: 921,600 px, 0 differing (0.0000%), max channel diff 0.
   - Dynamic UI script scenarios:
     - `frame_post_switch_taa.png`: valid non-blank (luminance [0, 239]), no ghosting or smearing.
     - `frame_post_resize_taa.png`: valid non-blank (luminance [0, 239]), but not evidence of resize reconstruction: the script requests 1280×720 after launching at that same size.
     - `frame_post_switch_fxaa.png`: valid non-blank (luminance [0, 239]), clean FXAA output.
   - All dumped frames inspected and verified per AGENTS.md.

4. **Targeted Performance Benchmark (60 frames, 20 warmup, 1280×720)**:
   - Control (`msaa4`): median CPU frame time 0.5898 ms.
   - Candidate (`fxaa`): median CPU frame time 0.6156 ms.
   - Candidate (`taa`): median CPU frame time 0.6150 ms.
   - These are candidate-only timings for different AA modes, not a before/after performance comparison. They do not establish absence of runtime overhead. No further timing run is required for acceptance; allocation counts and estimated live storage are the primary metrics.

#### 5. Summary of Changed Files

- `header/imgui/AntiAliasing.h`:
  - Added target release methods: `releaseTargets()`, `releaseFxaaTargets()`, `releaseTaaTargets()`.
  - Added target query and diagnostic methods: `getOwnedTargetCount()`, `getOwnedFramebufferCount()`, `getEstimatedLiveBytes()`, `getTargetAllocationCount()`, `getTargetDeletionCount()`, `resetTargetCounters()`, `getFxaaTexture()`, `getFxaaFbo()`, `getTaaTexture()`, `getTaaFbo()`, `isHistoryValid()`, `getWidth()`, `getHeight()`.
  - Added diagnostic self-check: `runTargetAllocationSelfCheck(bool forceFailure)`.
  - Replaced monolithic `ensureBuffers()` with `ensureFxaaBuffers()` and `ensureTaaBuffers()`.
  - Added allocation and deletion tracking counters.
- `source/imgui/AntiAliasing.cpp`:
  - Implemented active-mode target allocation in `ensureFxaaBuffers()` (1 target) and `ensureTaaBuffers()` (2 targets).
  - Implemented immediate release of inactive targets in `render()` and `cleanup()`.
  - Updated profiling counters (`countTextureCreation`, `countFramebufferCreation`, `countTextureAllocation`) to report exact per-mode counts rather than hardcoded 3×.
  - Implemented comprehensive `runTargetAllocationSelfCheck()` covering all 12 validation requirements.
- `header/imgui/UiScenarioHarness.h`:
  - Added `setAaMode` callback to `UiScenarioFrame`.
  - Added `aamode <mode>` script command for dynamic scenario execution.
- `source/imgui/main.cpp`:
  - Added `--aa-target-selfcheck <output>` CLI flag handling.
  - Wired `uiFrame.setAaMode` for interactive UI script scenarios.
  - Added target release on viewport closure in the viewport render loop (`!vp->isOpen`).
- `scripts/validate_p3_1.py` (retired, see git history):
  - New automated validation suite running all positive self-checks, forced failures, unwritable path checks, dynamic UI scenarios, and bit-exact pixel image comparisons.

#### 6. Review Gate Status

- Baseline commit: `b4b88ad`.
- Final candidate executable SHA-256: `C47D7775780E626DF02BF815A496B066D680A429013DA9EEDD148320BF0375A0`.
- Implementation submitted; acceptance remains pending the focused validation repairs below.
- Target counts and live storage verified: FXAA reduces post-process targets from 3 to 1 (-66.7%), TAA from 3 to 2 (-33.3%), None/MSAA from 3 to 0 (-100%).
- Texture lifetime is supported by code ordering; actual mixed-mode application viewport close/reopen validation remains pending.
- Direct AA object resize allocation checks and mode-switch captures pass. Actual application resize and first post-resize TAA visual checks remain pending.
- Steady-state frames produce 0 allocations and 0 deletions.
- 50 mode-switch cycles confirm 0 leaks.
- 101/101 self-check assertions pass; exit codes 0/1/2 verified.
- Stored startup captures for all 6 AA modes compare exactly. Fresh capture generation and an enforced image-comparison failure gate remain pending.
- Existing self-check suite passes completely.
- `git diff --check` passes with zero errors.
- No commit or push has been performed. P3.2–P3.4 and Phase 4 have not been started.
- Submitted for review.


#### 7. Independent Review and Reduced Follow-up Scope (2026-10-22)

Reviewer rebuilt Release, matched the candidate hash above, reran
`scripts/validate_p3_1.py`, inspected the TAA frame, and ran `git diff --check`.
Existing self-check success/failure/output-error paths pass. No confirmed
rendering defect was identified; the outstanding items concern validation.

Required focused repairs:
- Resize the application to a genuinely different size, assert changed viewport
  and AA target dimensions, and capture first post-resize and settled TAA output.
- Regenerate candidate startup captures, enforce a documented image comparison
  threshold, and demonstrate failure on a deliberately mismatched image. Missing
  image tooling must produce an explicit incomplete/failure result, not success.
- Exercise two real application viewports in different AA modes; close/reopen one
  through production lifecycle handling a few times. Assert release/recreation,
  independent target ownership, valid output, and no accumulating owned targets.

Validation budget policy:
- Test-only/script/documentation changes require only the repaired scenarios and
  comparator negative check. Reuse already passing evidence for unchanged code.
- If C++ harness instrumentation changes, build once and run the affected scenarios;
  do not automatically rerun unrelated hierarchy/resources/validation suites.
- If production AA allocation, lifetime, or history behavior changes, rerun the AA
  target self-check and affected mode/resize/lifecycle visual scenarios. Broaden
  only for a concrete dependency, observed failure, or unresolved regression risk.
- Run the six fresh startup captures once for the final candidate, not after every
  edit. Keep existing baseline captures if provenance/settings remain applicable.
- Do not run the P2.4 benchmark matrix, long timing tests, or another full suite
  solely because the executable hash changed. Record which evidence was rerun
  and which was reused, with the corresponding executable hashes.
- The follow-up gate remains submitted for review. No commit or push is authorized.


#### 8. Follow-up Validation Repairs & Final Evidence Record (2026-10-22)

All three validation findings identified in §12.25.7 were addressed using the reduced testing budget policy.

1. **Repaired Gap 1: Real Resize Scenario with TAA History Invalidation Oracle & Accurate First-Frame Capture**:
   - **Diagnostic Mechanism**: Added state observation directly to `AntiAliasing` and `UiScenarioFrame::ViewportInfo`:
     - `m_targetGeneration`: Monotonically increments when targets are allocated or reallocated on resize.
     - `m_lastTaaTargetGeneration`: Records target generation at the time of the TAA pass.
     - `m_lastTaaHistoryValid`: Observes the history validity state *prior* to rendering (before the pass sets `m_historyValid = true`).
     - `m_lastTaaBlendFactor`: Observes the effective history blend factor used during the pass ($1.0$ for discarded history, $0.10$ for stationary accumulation).
   - **Accurate First-Frame Capture Trigger (`catch_realloc`)**:
     - Eliminated arbitrary wait counts (`wait 8`). Replaced with a narrowly scoped harness command `catch_realloc <vp_id> <shot_path> <snap_label>`.
     - In `UiScenario::endFrame()`, on the exact frame where `v.targetGeneration > m_catchReallocBaseGen` (the first frame rendered using the resized targets), the harness captures the window framebuffer and records the snapshot before buffer swap.
   - **Acceptance Oracle Verification**:
     - Pre-resize (734×616): `targetGen = 1`, `allocs = 2`, `deletes = 0`.
     - Post-resize first frame (1054×796): `targetGen = 2`, `lastTaaTargetGen = 2`, `lastTaaHistValid = False`, `lastTaaBlendFactor = 1.0`.
     - Asserts `lastTaaTargetGen == targetGen`, proving the observation belongs to the current target generation.
     - Asserts `lastTaaHistValid is False`, proving previous history was discarded.
     - Asserts `lastTaaBlendFactor == 1.0`, proving no blend with invalid or stale history occurred.
     - Post-resize settled frame (1054×796): `lastTaaHistValid = True`, `lastTaaBlendFactor = 0.10` (stationary convergence).
   - **Intentional-Negative Oracle Demonstration**:
     - Tested oracle against 4 synthetic defective records: erroneously valid history (`lastTaaHistoryValid = True`), wrong blend factor (`lastTaaBlendFactor = 0.10`), stale generation (`lastTaaTargetGeneration = 1`), and unadvanced generation (`targetGeneration = 1`).
     - Oracle correctly rejected all 4 defective records, logging proof to `build/p3.1/selfchecks_negative/resize_history_oracle_negative.txt`.
   - **Dimension & Reallocation Assertions**:
     - Viewport render dimensions changed from 734×616 to 1054×796.
     - Target dimensions updated to 1054×796 (`aaTargets = 2`).
     - Reallocation delta: exactly $+2$ allocations and $+2$ deletions (clean destruction & recreation).
     - Steady state post-resize: $0$ allocations and $0$ deletions.
   - **Visual Verification & Observation vs Oracle**:
     - Captured artifacts: `frame_post_resize_first_taa.png` (1600×900, 1,204,550 bytes) and `frame_post_resize_settled_taa.png` (1600×900, 1,222,761 bytes) opened and inspected per AGENTS.md.
     - Pixel differences between first frame (single-sample geometry) and settled frame (16-sample Halton jitter accumulation) are reported as an **observation only** of multi-frame convergence (206,496 edge diff pixels, 14.3400%, max channel diff 94), explicitly not the acceptance oracle.
     - Diagnostic report logged to `build/p3.1/selfchecks_positive/resize_scenario_validation.txt`.

2. **Repaired Gap 2: Image Comparison Acceptance Gate & Intentional-Negative Demonstration**:
   - **Dependency Gate**: Enforced hard import of PIL/Pillow at startup; aborts with exit code 1 if unavailable.
   - **Synthetic Comparator Positive & Negative Checks**:
     - Positive test: Compared two identical 64×64 RGBA images; exact equality confirmed (0 differing pixels, max diff 0).
     - Negative test: Created two 64×64 RGBA images with pixel (32, 32) altered by $\Delta g = 5$. The comparator detected the mismatch (1 differing pixel, max channel diff 5, 0.0244%) and rejected equality.
     - Saved negative evidence to `build/p3.1/selfchecks_negative/comparator_synth_a.png`, `build/p3.1/selfchecks_negative/comparator_synth_b.png`, and `build/p3.1/selfchecks_negative/comparator_negative_result.txt`.
   - **Fresh Candidate Startup Captures**:
     - Deleted all stale candidate files before execution.
     - Generated fresh candidate startup captures for all 6 AA modes (`none`, `msaa2`, `msaa4`, `msaa8`, `fxaa`, `taa`) using `--isolated --window-size 1280x720 --aa <mode> --dump-frame <path> --exit-after-dump`.
   - **Exact Equality Acceptance Gate**:
     - Compared fresh candidate images against baseline (`build/p3.1/baseline/frame_<mode>.png`):
       - `none`: 921,600 px, 0 diff (0.0000%), max diff 0 (EXACT MATCH).
       - `msaa2`: 921,600 px, 0 diff (0.0000%), max diff 0 (EXACT MATCH).
       - `msaa4`: 921,600 px, 0 diff (0.0000%), max diff 0 (EXACT MATCH).
       - `msaa8`: 921,600 px, 0 diff (0.0000%), max diff 0 (EXACT MATCH).
       - `fxaa`: 921,600 px, 0 diff (0.0000%), max diff 0 (EXACT MATCH).
       - `taa`: 921,600 px, 0 diff (0.0000%), max diff 0 (EXACT MATCH).
     - Logged to `build/p3.1/selfchecks_positive/startup_modes_image_comparison.txt`.

3. **Repaired Gap 3: Application Viewport Lifecycle Behavior**:
   - **Mechanism**: Added `vpcreate`, `vptoggle`, and `viewports` state reporting to `UiScenarioHarness`, exercising production viewport creation (`Viewport -> New Viewport`) and production window close handling (`!vp->isOpen` triggering `vp->antiAliasing.releaseTargets()`).
   - **Multi-Viewport Lifecycle Scenario**:
     - Opened two real application viewports: VP1 (FXAA) and VP2 (TAA).
     - Verified independent ownership: VP1 owns 1 target and 1 FBO (3.52 MiB); VP2 owns 2 targets and 2 FBOs (7.03 MiB).
     - Closed VP2 via `vptoggle 2` (`vp->isOpen = false`).
     - Verified release: VP2 post-process targets and FBOs dropped to 0 (0 live bytes); VP1 remained completely valid and intact (1 target, 1 FBO).
     - Reopened VP2 via `vptoggle 2` (`vp->isOpen = true`).
     - Verified recreation: VP2 reallocated 2 targets and 2 FBOs; VP1 remained intact.
     - Repeated 3 close/reopen cycles: verified zero accumulation of owned targets or FBOs (VP1 remained 1 target, VP2 remained 2 targets).
     - Inspected representative frame `build/p3.1/candidate/frame_multi_viewport_lifecycle.png` (1280×720, non-blank luminance [0, 240], both viewport tabs visible and active).
     - Logged to `build/p3.1/selfchecks_positive/viewport_lifecycle_validation.txt`.

4. **Evidence Provenance & Execution Summary**:
   - Final candidate executable SHA-256: `3B8F56FD285AA331040FB0E19373B7122DBC6771533C886E5072C182AD7A5297`.
   - Baseline commit: `b4b88ad`.
   - Focused execution command: `python scripts/validate_p3_1.py --only resize-history`.
   - Newly Run Evidence for this repair:
     - Synthetic oracle rejection demonstration: 4 defective records rejected (`resize_history_oracle_negative.txt`).
     - Real application resize/history scenario with `catch_realloc`: verified target generation advance, discarded history, blend factor 1.0, and settled state (`resize_scenario_validation.txt`).
     - AA target self-checks: positive 107 assertions, forced-failure, and unwritable dir (`aa_target_selfcheck.txt`).
     - Viewed artifact: `frame_post_resize_first_taa.png` (1600×900, 1,204,550 bytes).
   - Reused Passing Evidence (independently verified on candidate `CCEACF7CB26FB1816F304961474EEC368AEFEEEBAF8AF51955BAAF5691C22693`):
     - Fresh startup captures for all 6 AA modes (`frame_<mode>.png`).
     - Baseline captures in `build/p3.1/baseline/`.
     - Image comparator positive & negative checks (`comparator_negative_result.txt`).
     - Application viewport lifecycle scenario (`multi_viewport_lifecycle.report.json`, `frame_multi_viewport_lifecycle.png`).
     - Mode switch scenarios (`frame_post_switch_taa.png`, `frame_post_switch_fxaa.png`).
     - Unchanged subsystem self-checks: `conflict_selfcheck.txt`, `validation_cache_selfcheck.txt`, `cache_invalidation_selfcheck.txt`, `logger_layout_selfcheck.txt`.

5. **Status**:
   - All validation findings resolved under strict acceptance gates.
   - Submitted for review. No commit or push has been performed. P3.2–P3.4 and Phase 4 have not been started.

#### 9. Independent Review Acceptance

P3.1 is accepted for commit. The reviewer rebuilt Release and matched final
candidate SHA-256 `3B8F56FD285AA331040FB0E19373B7122DBC6771533C886E5072C182AD7A5297`.
Only `python scripts/validate_p3_1.py --only resize-history` was rerun in this
review; earlier passing startup, lifecycle, switching, and self-check evidence
was reused as documented above.

The focused run passed all four synthetic rejection cases and the real resize:
target generation 1 -> 2; first resized TAA pass observed invalid history and
blend factor 1.0; settled pass observed valid history and blend factor 0.1.
The capture hook executes on the first observed target-generation advance,
before harness wait handling. Dimensions changed from 734x616 to 1054x796,
with exactly two allocations and two deletions and no settled-frame churn.
The reviewer opened the regenerated first-frame image and confirmed visible
scene output. `git diff --check` passed. No benchmark matrix or unrelated
suite was repeated, and no commit or push was performed during review.

---

<a id="section-12-26"></a>

### 12.26 P3.2 — Replace Per-Bind Sampler Parameter Updates with GL Sampler Objects (2026-10-22)

#### 1. Context & Baseline

- **Accepted Baseline**: Commit `c5ecd98` ("Allocate AA targets by active mode with lifecycle validation").
- **Task Scope**: P3.2 only. Replace repeated per-bind texture sampler parameter updates with GL sampler objects. Do not implement P3.3 (VAO reuse), P3.4 (executor shadow state), or Phase 4. Preserve standalone single executable, OpenGL 4.1 compatibility, and P1.1 capability cache.
- **Problem Audit ([§3.2](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-2))**:
  - In `OpenGL-Renderer/renderer/source/gl/Sampler.cpp:15-46`, every sampler commit on every draw call executed:
    - 5 `glTexParameteri` calls (`GL_TEXTURE_MIN_FILTER`, `GL_TEXTURE_MAG_FILTER`, `GL_TEXTURE_WRAP_S`, `GL_TEXTURE_WRAP_T`, `GL_TEXTURE_WRAP_R`).
    - 1 `glTexParameterf` call (`GL_TEXTURE_MAX_ANISOTROPY_EXT`).
  - For complex multi-drawable scenes with multiple textures, this caused thousands of driver calls per frame mutating texture object state (e.g. ~90,000 calls per frame in an illustrative 1,000-drawable scene with ~15 samplers).
  - Mutating the underlying texture object's internal sampler parameters caused cross-unit and cross-draw interference whenever the same texture was sampled with different sampler settings (e.g. repeat vs clamp-to-edge, or linear vs nearest).

#### 2. Architectural Implementation

1. **OpenGL Sampler Abstraction (`GL.h`, `GladGL.h`, `GladGL.cpp`)**:
   - Extended `gl::GL` and `GladGL` with standard Core OpenGL 3.3 / 4.1 sampler functions:
     `genSamplers`, `deleteSamplers`, `bindSampler`, `samplerParameteri`, `samplerParameterf`, `getSamplerParameteriv`, `getSamplerParameterfv`.
   - Instrumented each method with dedicated profiling counters in `Profiling.h`:
     `samplerCreations`, `samplerDeletions`, `samplerBinds`, `samplerParameterCalls`, and `texParameterCalls`.

2. **Complete Sampling State Key (`SamplerState`)**:
   - `gl::SamplerState` encapsulates the complete 5-tuple defining effective sampling parameters: `minFilter`, `magFilter`, `wrapS`, `wrapT`, and `wrapR`.
   - Added `operator==` and `operator!=` to `SamplerState`.
   - Implemented hashing via `ux3d::core::Hash<SamplerState>` and standard `std::hash<ux3d::renderer::gl::SamplerState>`.
   - Default/missing glTF sampler behavior maps cleanly to default `SamplerState()` (`LinearMipmapLinear`, `Linear`, `Repeat`, `Repeat`, `Repeat`).

3. **Sampler Object Cache & Ownership (`SamplerCache`)**:
   - `gl::SamplerCache` is owned by `CommandExecutor` (which is owned by `Renderer`).
   - Uses `std::unordered_map<SamplerState, GLuint, core::Hash<SamplerState>>`.
   - `getOrCreateSampler(const SamplerState& state)`:
     - On first query for a given state: generates a GL sampler object (`glGenSamplers`), sets its 5 filter/wrap parameters (`glSamplerParameteri`), configures `GL_TEXTURE_MAX_ANISOTROPY_EXT` using the cached immutable context maximum (if supported), and stores the handle in the cache.
     - On subsequent queries: returns the cached `GLuint` handle immediately (0 GL allocations, 0 GL parameter calls).
   - In steady-state rendering, zero GL sampler objects are created or deleted.

4. **Streamlined Commit Path & Zero-Handle Fallback Fix (`Sampler::commit`, `Program::commitSampler`)**:
   - In `Sampler::commit`: replaces all 5 `glTexParameteri` and 1 `glTexParameterf` calls with a single `gl->bindSampler(textureUnit, samplerHandle)`.
   - **Zero-Handle Fallback Fix**: When `samplerHandle == 0` (defensive fallback or unsupported sampler creation), `Sampler::commit` explicitly calls `gl->bindSampler(static_cast<GLuint>(textureUnit), 0)` before configuring texture-owned parameters (`glTexParameteri` / `glTexParameterf`). This prevents stale bound GL sampler objects from overriding the requested fallback parameters on that unit.
   - Handles textures used with different samplers across units in the same draw and across successive draws without mutating texture objects.

5. **Strict Boundary Isolation & Automatic Cleanup**:
   - Because GL sampler bindings persist per texture unit and override texture-owned parameters, `CommandExecutor::resetState()` and `unbindSamplers()` unbind all active sampler units (`m_gl->bindSampler(unit, 0)`).
   - Automatic unbinding is performed at the end of `Renderer::draw()`, before any `ExternalFunction` command, and at frame boundaries.
   - This guarantees that subsequent passes (Anti-Aliasing FXAA/TAA, ImGui font texture, grid renderer, bounding box renderer, picking reads) are never perturbed by scene sampler objects.

6. **Context-Safe Object Lifetime & glIsSampler Driver Deletion**:
   - `CommandExecutor::getSamplerCache().clear()` is explicitly called during `Renderer::uninit()`.
   - In `main.cpp`, `glRenderer->uninit()` and `glRenderer.reset()` are called explicitly before `ImGui_ImplOpenGL3_Shutdown()` and `glfwDestroyWindow()`, ensuring all GL sampler objects are deleted while the GL context is fully valid.
   - Driver deletion is confirmed using `glIsSampler`, verifying handles return `GL_FALSE` immediately after cache clearing.

#### 3. Verification & Validation Evidence

#### 3. Verification & Validation Evidence

Automated validation is implemented in `scripts/validate_p3_2.py` (retired, see git history):
- **Tested Candidate Executable**: `build/bin/Release/Glider.exe`
- **Candidate SHA-256**: `0B3A1EC9A627A2EAC5231F4DA7266F76D601F61F94FE7DB5550227AE263C0701`
- **Validation Scope**: Under the narrow validation budget, the amended focused checks (`--only boundary`, `--only lifecycle`, `--only production-draw`) were newly run and verified against this binary. Baseline startup image comparison (all 6 modes 0 diffs bit-for-bit exact match), multi-viewport isolation, and steady-state profiling counters were reused from the verified baseline runs.

1. **Diagnostic Self-Check (`--sampler-selfcheck`)**:
   - **Section 1: Zero-handle fallback stale sampler regression check**:
     - Binds a non-zero sampler with deliberately contrasting settings (`LinearMipmapLinear`, `Repeat`).
     - Invokes production `Sampler::commit()` zero-handle fallback path on unit 0 requesting `Nearest` and `ClampToEdge`.
     - Queries `GL_SAMPLER_BINDING` (confirms `0` - explicitly unbound) and `GL_TEXTURE_MIN_FILTER` / `GL_TEXTURE_WRAP_S` (confirms fallback parameters applied).
     - Confirms no GL errors.
   - **Section 2: SamplerState key comparison, hashing, and cache lookup**:
     - Equality, inequality, hash generation, deduplication, anisotropy against cached context maximum, and container clearing.
   - **Section 3: Production draw sampling integration (`--only production-draw`, 17 PASS assertions)**:
     - Executes actual production draw commands through `CommandExecutor` -> `Program::commitSampler` -> `Sampler::commit` rendering a 2x2 multi-quadrant patterned texture (level 0 RGBA, level 1 Yellow) onto an offscreen FBO.
     - Dual-unit single draw: same underlying texture sampled on Unit 0 with Nearest and Unit 1 with Linear simultaneously. Pixel readback confirms unblended Nearest R=255 alongside bilinear blended G channel.
     - Successive draws: alternates between Nearest (Draw A: pure unblended Red quadrant at (16, 16) and unblended center pixel at (32, 32)) and Linear (Draw B: blended center pixels ~128 across all 4 quadrants).
     - Mipmap filtering: verifies `LinearMipmapLinear` samples Level 1 (Yellow) while linear without mips samples Level 0 (Blue present).
     - **Full Wrap Mode Coverage**: Quad UV offset shifted by $+1.0$ in $U$ ($U \in [1.0, 2.0], V \in [0.0, 1.0]$):
       - Pixel $(16, 16)$ ($U \approx 1.258, V \approx 0.258$):
         - `Repeat` wraps to $U = 0.258$: pure Red `[255, 0, 0]`
         - `ClampToEdge` clamps to edge: pure Green `[0, 255, 0]`
         - `MirroredRepeat` reflects to $U = 0.742$: pure Green `[0, 255, 0]`
       - Pixel $(48, 16)$ ($U \approx 1.758, V \approx 0.258$):
         - `Repeat` wraps to $U = 0.758$: pure Green `[0, 255, 0]`
         - `ClampToEdge` clamps to edge: pure Green `[0, 255, 0]`
         - `MirroredRepeat` reflects to $U = 0.242$: pure Red `[255, 0, 0]`
       - All three modes yield distinct, mathematically unambiguous colors matching expectations.
     - Pattern frame dump: restores standard UV coordinates and dumps 64x64 patterned test frame `frame_pattern_test.png`, inspected and verified with PIL.
   - **Section 4: Independent automatic boundary cleanup verification (`--only boundary`, 7 PASS assertions)**:
     - Independent Test 1: executes `drawCmd` followed by `extCmd` (`makeExternalFunctionCommand`). Asserts `GL_SAMPLER_BINDING == 0` inside the callback.
     - Independent Test 2: executes `drawCmd` without any `ExternalFunction` in the command list. Asserts `Renderer::draw` succeeds and `GL_SAMPLER_BINDING == 0` on units 0 and 1 immediately after `Renderer::draw` exits.
     - Both cleanup paths independently detect failures in their respective unbind logic.
     - Subsequent consumer relying on texture-owned parameters (`GL_NEAREST`) and unbound samplers verified uncorrupted.
   - **Section 5: Reload and destruction ownership verification (`--only lifecycle`, 37 PASS assertions)**:
     - **Section 5A (Unit Check)**: 4 known sampler states allocated in unit cache, repeat queries confirm identical handles returned, cache size stabilized at 4 (zero growth), direct `SamplerCache::clear()` empties container and driver deletion confirmed via `glIsSampler == GL_FALSE`.
     - **Section 5B (Production Reload & Teardown Integration)**:
       - Uses a minimal self-contained glTF fixture (`build/p3.2/fixture_samplers.gltf`) with 3 known sampler states (Nearest/ClampToEdge, LinearMipmapLinear/Repeat, Linear/MirroredRepeat).
       - Executes 3 production `loadGltfModel` + scene update + `Renderer::draw` cycles.
       - Cache size stabilizes at 5 across all 3 cycles (zero growth, identical GL handles retained and reused).
       - Real teardown: calls `glRenderer->uninit()`. Verifies cache size is 0 and driver handles return `glIsSampler == GL_FALSE` without directly invoking `SamplerCache::clear()`.
   - Return codes:
     - Exit code `0` on standard run.
     - Exit code `1` on `--selfcheck-force-failure` (`build/p3.2/selfchecks_negative/sampler_forced_fail.txt`).
     - Exit code `2` on unwritable/directory target.
   - Focused execution options: supports `--only fallback`, `--only production-draw`, `--only boundary`, `--only lifecycle`, `--only steady-state`, `--only multi-viewport`, `--only image-gate`.

2. **Steady-State OpenGL Driver Call Elimination**:
   - Measured across 30 frames with `--profile --benchmark --benchmark-warmup 8 --benchmark-frames 30` in `build/p3.2/selfchecks_positive/steady_state_bench.json`:
     - `texParameterCalls`: **0** in the tested scene (the measured steady-state scene executes zero texture sampler-parameter calls; figures of tens of thousands per frame are illustrative estimates for larger multi-drawable assets unless directly measured)
     - `samplerCreations`: **0**
     - `samplerDeletions`: **0**
     - `samplerParameterCalls`: **0**
     - `samplerBinds`: 16 per frame (active `glBindSampler` calls)
     - `glCalls`: 143 total per frame

3. **Multi-Viewport Scenario Execution**:
   - Executed dynamic multi-viewport script with differing AA modes (`FXAA` on Viewport 1, `TAA` on Viewport 2) across 35 frames.
   - Clean execution with 0 crashes, valid non-blank screenshot `frame_multi_viewport.png`, and report `multi_viewport.report.json`.

4. **Visual Correctness & Image Acceptance Gate**:
   - Fresh candidate startup captures generated for all 6 AA modes (None, MSAA2, MSAA4, MSAA8, FXAA, TAA).
   - Evaluated against baseline captures:
     - Mode `none`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
     - Mode `msaa2`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
     - Mode `msaa4`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
     - Mode `msaa8`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
     - Mode `fxaa`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
     - Mode `taa`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
   - Dumped frame `frame_p3_2.png` visually verified per AGENTS.md rule 5.

5. **Regression Protection**:
   - Full regression suite `run_selfchecks_final.py` passed with expected return codes (0, 1, 2) across all prior self-checks.
   - Full P3.1 validation suite `scripts/validate_p3_1.py` passed 100% (target allocation, real resize with TAA history invalidation oracle, viewport lifecycle, and dynamic mode switching).
   - `git diff --check` passed with 0 whitespace errors.

#### 4. Scope & Guardrails Compliance

- P3.2 only implemented.
- P3.3 (VAO reuse), P3.4 (executor shadow state), and Phase 4 were not started.
- Scene rebuild frequency was not altered.
- Standalone single executable architecture and OpenGL 4.1 Core Profile compatibility preserved.
- Status: **SUBMITTED FOR REVIEW**; no commit or push performed.

#### Independent P3.2 Review Acceptance

P3.2 is accepted for commit following focused review of the final test repairs.
Reviewer rebuilt Release and matched candidate SHA-256
`0B3A1EC9A627A2EAC5231F4DA7266F76D601F61F94FE7DB5550227AE263C0701`.
Independently ran `scripts/validate_p3_2.py --only boundary` (7/7),
`--only lifecycle` (37/37), and `--only production-draw` (17/17).
External-function cleanup and renderer-exit cleanup now have independent draw
sequences. The lifecycle callback uses production loadGltfModel, scene creation,
command generation, and Renderer::draw; three cycles retain the same five
sampler handles, and Renderer::uninit deletes them as confirmed by glIsSampler.
Out-of-range UV readbacks distinguish repeat, clamp, and mirrored repeat.
Reviewer opened the regenerated patterned frame and ran git diff --check.
Earlier passing fallback, error-exit, driver-call, startup-image, and AA evidence
was reused; no broad regression suite or benchmark matrix was repeated.
No blocking findings remain. No commit or push was performed during review.

---

<a id="section-12-27"></a>

### 12.27 P3.3 — Reuse Vertex Array Objects / Vertex Layouts Across Frames (2026-10-22)

#### 1. Context & Accepted Baseline

- **Accepted Baseline**: Commit `6636b28` ("Cache GL sampler objects with isolated bindings and lifecycle validation").
- **Task Scope**: P3.3 only. Reuse vertex array objects (VAOs) / vertex layouts across frames. Eliminate per-frame VAO creation and deletion in steady-state rendering. Preserve core profile EBO encapsulation, attribute isolation, shader variant separation, and context teardown without leaks. Do not implement P3.4 (executor shadow state) or Phase 4. Do not alter scene rebuild frequency, draw ordering, shaders, or rendering quality. Do not commit or push.

#### 2. Architecture, Root Causes & Review Repairs

1. **Quadratic Cache Removal & Per-Mesh Hierarchical Organization**:
   - *Review Finding*: Initial P3.3 implementation used a flat hash map key `(meshId, gen, programId)`. On every miss and mesh invalidation, the cache iterated linearly over all entries, creating $O(N^2)$ scaling when populating or destroying $N$ distinct meshes.
   - *Resolution*: Reorganized `VertexLayoutCache` into a two-level per-mesh hierarchy:
     `std::unordered_map<uint64_t /*meshId*/, MeshLayoutGroup> m_meshes;`
     where `MeshLayoutGroup` maps `programId` to `VertexLayoutEntry { uint64_t generation, std::shared_ptr<VertexLayout> layout }`.
   - *Operation Complexity*:
     - **Brand-New Mesh Insertion**: $O(1)$ amortized map insertion. Inspects 0 existing cache entries of prior meshes.
     - **Warm Cache Lookup**: $O(1)$ amortized hash map lookup.
     - **Generation Invalidation (In-Place Mutation)**: $O(P)$ where $P$ is the number of active programs on the affected mesh (typically 1 or 2). Visits strictly 0 entries belonging to unrelated meshes.
     - **Mesh Destruction (`invalidateMesh`)**: Expected $O(1)$ group lookup plus $O(P)$ destruction of that mesh's program entries. Visits 0 entries belonging to other meshes.
     - Population and destruction for $N$ distinct meshes with a bounded number of programs per mesh is expected $O(N)$; hash-map operations are not worst-case constant-time guarantees.
   - Added `CacheWorkStats` instrumentation (`entriesVisited`, `unrelatedEntriesScanned`) verifying that populating 50 meshes, inserting a 51st mesh, bumping a mesh's generation, and invalidating a mesh all scan exactly 0 unrelated mesh entries.

2. **Core Profile Element Array Buffer (EBO) Encapsulation**:
   - In OpenGL 4.1 Core Profile, `GL_ELEMENT_ARRAY_BUFFER` is encapsulated within the Vertex Array Object's internal state.
   - In `gl::Program::createVertexLayout`, `indexBuffer ? indexBuffer->handle : 0` is explicitly bound while the VAO is active.
   - Switching between indexed and non-indexed VAOs automatically binds or restores `GL_ELEMENT_ARRAY_BUFFER` to 0 with zero cross-draw contamination.

3. **Shader Variant Separation with Genuinely Active Attributes**:
   - *Review Finding*: Initial basic and picking test fixtures had active positions at location 0 while unused normals were optimized away by the GLSL compiler, leaving attribute mapping untested.
   - *Resolution*: Created dedicated shader programs with verified active attribute locations:
     - `progNorm`: `a_position` at location 0, `a_normal` at location 1 (output to fragment shader). Querying `glGetAttribLocation` confirms loc 0 and 1 are active, while loc 2 returns -1.
     - `progCol`: `a_position` at location 0, `a_color_0` at location 2 (output to fragment shader). Querying `glGetAttribLocation` confirms loc 0 and 2 are active, while loc 1 returns -1.
   - The same mesh evaluated under `progNorm` vs `progCol` produces distinct layouts and distinct VAO handles:
     - `layoutNorm`: locations 0 and 1 enabled, location 2 explicitly disabled.
     - `layoutCol`: locations 0 and 2 enabled, location 1 explicitly disabled.
   - Draws executed through production `CommandExecutor` verify normal-derived green pixels `[0, 255, 0, 255]` and color-derived magenta pixels `[255, 0, 255, 255]` on an offscreen FBO.

4. **Supported Invalidation, Unrelated Mesh Retention & Boundary Isolation**:
   - Verified that mutating geometry buffers and calling `mesh.bumpGeneration()` creates a fresh layout and renders changed output (blue `[0, 0, 255, 255]`), while an unrelated mesh in the cache retains its cached layout unchanged.
   - Verified the `CommandTag::ExternalFunction` boundary in `CommandExecutor`: a sequence of `Draw` -> `ExternalFunction` (binding foreign VAO) -> `Draw` confirms `m_activeVertexArray` is invalidated so subsequent draws rebind their VAO cleanly with 0 GL errors.

5. **Lifecycle Behavior Inspected Per Reload Cycle**:
   - *Review Finding*: Initial lifecycle test only checked cache contents after all three reload cycles, allowing potential intermediate accumulation to pass.
   - *Resolution*: Strengthened Section 6C to inspect live cache counts after *each* cycle:
     - Cycle 1: `reloadAndDraw("HelloCube.glb")` -> record baseline count ($K = 1$). All layouts active on driver.
     - Cycle 2: `reloadAndDraw("HelloCube.glb")` -> assert live count is bounded ($K = 1 == \text{baseline}$). All layouts active on driver.
     - Cycle 3: `reloadAndDraw("HelloCube.glb")` -> assert live count is bounded ($K = 1 == \text{baseline}$). All layouts active on driver.
     - Teardown: `clearVertexLayoutCache()` drops all command references and clears cache -> size is 0 and all handles return `glIsVertexArray == GL_FALSE`.

6. **Restoration of Degree-to-Radian HDR Rotation**:
   - *Review Finding*: In `CommandGenerator.cpp`, `setEnvironmentMapRotation()` stopped converting degrees to radians.
   - *Resolution*: Restored `m_environmentMapRotation = math::rotate(math::radians(_rotation), math::vec3(0, 1, 0));` and removed unrelated bounds check in `emitCommands`.
   - Added focused verification in Section 0: `setEnvironmentMapRotation(90.0f)` produces $\cos(90^\circ) \approx 0.0$ on diagonals and $\sin(90^\circ) \approx 1.0$ on off-diagonals; `setEnvironmentMapRotation(180.0f)` produces $\cos(180^\circ) \approx -1.0$; and `setEnvironmentMapRotation(0.0f)` restores the identity matrix.

#### 3. Verification & Validation Evidence

1. **Diagnostic Self-Check Suite (`--vao-selfcheck`)**:
   - 111 total assertions evaluated across 8 sections, **111 PASS, 0 FAIL** (exit code 0):
     - Section 0: Nonzero HDR rotation degree-to-radian verification (13 assertions).
     - Section 1: Steady-state reuse across 10 successive queries (11 assertions).
     - Section 1B: Deterministic cache scaling and work counters (8 assertions).
     - Section 2: Indexed vs non-indexed core profile EBO invariant (7 assertions).
     - Section 3: Differing attribute sets and contamination isolation (11 assertions).
     - Section 4: Shader variants layout separation and active attribute locations (15 assertions).
     - Section 5: Invalidation on buffer updates and generation bump (9 assertions).
     - Section 6 & 6C: Clean destruction, per-cycle bounded reload counts, and teardown (16 assertions).
     - Section 7: Production draw execution, pixel verification, and boundary isolation (21 assertions).
   - Negative testing:
     - `--selfcheck-force-failure`: returned exit code 1 with valid FAIL report.
     - Unwritable report destination: returned exit code 2.

2. **Focused Command Executions**:
   - `python scripts/validate_p3_3.py (retired, see git history) --only hdr-rotation`: PASS (exit code 0)
   - `python scripts/validate_p3_3.py --only scaling`: PASS (exit code 0, 0 unrelated entries scanned)
   - `python scripts/validate_p3_3.py --only shader-variants`: PASS (exit code 0)
   - `python scripts/validate_p3_3.py --only invalidation`: PASS (exit code 0)
   - `python scripts/validate_p3_3.py --only lifecycle`: PASS (exit code 0, bounded per-cycle counts)
   - `python scripts/validate_p3_3.py --only boundary`: PASS (exit code 0)
   - `python scripts/validate_p3_3.py --only benchmark`: PASS (30 frames: 0 creations, 0 deletions)

3. **Steady-State Benchmark & Profiling Counters**:
   - 30 measured frames under `scripts/validate_p3_3.py --only benchmark`:
     - `vaoCreations`: **0 across all 30 frames**
     - `vaoDeletions`: **0 across all 30 frames**
   - 100% elimination of steady-state VAO creation and deletion churn.

4. **Visual Frame Inspection & Image Comparison Gate**:
   - Visual model dumps:
     - `HelloCube.glb`: 1280x720, 47,219 distinct colors (nonblank check PASS)
     - `MultiPrimitive.gltf`: 1280x720, 43,637 distinct colors (visually inspected, nonblank check PASS)
     - `VC.gltf`: 1280x720, 60,297 distinct colors (nonblank check PASS)
   - Image Acceptance Gate (Provenance: `build/p3.1/baseline` canonical 1280x720 HelloCube startup captures):
     - Mode `none`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
     - Mode `msaa2`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
     - Mode `msaa4`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
     - Mode `msaa8`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
     - Mode `fxaa`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**
     - Mode `taa`: total 921,600 px, diff **0 (0.0000%)**, max_diff 0 -> **EXACT EQUALITY MATCH**

5. **Linter & Code Standards**:
   - `git diff --check` passed with 0 whitespace errors.
   - Candidate binary SHA-256: `4CEA544A2D64D2551F9749E0A0457F89D6788B77500D3BFAD41F6BF9974D1049`.

#### 4. Scope & Guardrails Compliance

- P3.3 only implemented and repaired.
- P3.4 (executor shadow state) and Phase 4 were not started.
- Scene rebuild frequency, draw ordering, shaders, and rendering quality remain unchanged.
- Standalone single executable architecture and OpenGL 4.1 Core Profile compatibility preserved.
- Status: **SUBMITTED FOR REVIEW**; no commit or push performed.

#### 5. Independent review acceptance (2026-10-22)

The repaired P3.3 implementation is accepted for commit. Reviewer rebuilt Release
and verified candidate SHA-256
`4CEA544A2D64D2551F9749E0A0457F89D6788B77500D3BFAD41F6BF9974D1049`.
Focused runner cases passed: `hdr-rotation` (13 assertions), `scaling` (19),
`shader-variants` (27), `production-draw` (23), and `lifecycle` (30).
These totals include shared fixture assertions; the earlier per-section counts
are descriptive and must not be summed as independently measured totals.
The production-draw case includes the changed-buffer pixel readback and external
VAO boundary sequence; `--only invalidation` alone only checks layout/buffer state.
The 30-frame `benchmark` case again recorded zero VAO creations and deletions.
Reviewer generated and opened `build/p3.3/candidate/reviewer_startup.png` and
ran `git diff --check`.

Cache source inspection confirms per-mesh lookup and pruning. The
`unrelatedEntriesScanned` field is not incremented by the implementation; its
zero assertion alone is not independent proof of complexity. The inspected
algorithm and actual `entriesVisited` checks support the scaling conclusion.
Reload validation establishes bounded cache counts (one layout each cycle),
valid current GL objects, and final deletion. It does not separately trace every
retired GL object between cycles; ownership/destructor paths were inspected.
Future layout mutations must continue to bump the mesh generation.

Earlier passing negative-report checks and image evidence were reused. The six
AA image comparisons above use historical P3.1 baseline captures, not fresh
captures of commit 6636b28; no broad image matrix or earlier-phase suite was
repeated for this review. No frame-time speedup is claimed from object counts.
No remaining blocking findings. Only this journal was edited during review;
no commit or push was performed.

<a id="section-12-28"></a>

### 12.28 P3.4.1 — CommandExecutor Polygon Mode Cache (2026-10-22)

#### 1. Context & Accepted Baseline

- **Accepted Baseline**: Commit `b506b3c` ("Reuse vertex layouts with per-mesh invalidation and lifecycle validation").
- **Task Scope**: P3.4.1 only. Cache polygon mode in `CommandExecutor`. Suppress redundant `glPolygonMode` driver calls across consecutive draws requesting identical polygon mode while strictly preserving rendering behavior. Invalidate cache at `CommandExecutor::resetState()`, `Renderer::draw()` boundaries (entry, error exit, successful exit), `CommandExecutor::init()`, and `CommandTag::ExternalFunction`.
- **Exclusions**: Do not cache culling, front-face winding, depth, blend, programs, textures, samplers, framebuffers, or other state in this task. Do not start Phase 4. Preserve scene rebuild frequency, draw ordering, shaders, rendering quality, standalone single-executable architecture, and OpenGL 4.1 Core Profile compatibility. Preserve accepted P3.2 sampler behavior and P3.3 VAO behavior. Do not commit or push. Stop for review when complete.

#### 2. Ownership, Mutation Boundaries & Architecture

1. **State Ownership & Cache Invalidation in CommandExecutor**:
   - `CommandExecutor` now tracks active polygon mode via `std::optional<gl::PolygonMode> m_activePolygonMode`:
     ```cpp
     void ux3d::renderer::CommandExecutor::setupPolygonMode(const Command& command)
     {
         if (!m_activePolygonMode.has_value() || m_activePolygonMode.value() != command.polygonMode) {
             m_activePolygonMode = command.polygonMode;
             m_gl->polygonMode(GL_FRONT_AND_BACK, GLenum(command.polygonMode));
             GLIDER_HOT_GL_CHECK(m_gl, "Failed to set polygon mode");
         }
     }
     ```
   - **Boundary Invalidation Points**:
     - `CommandExecutor::init()`: Resets `m_activePolygonMode` to `std::nullopt`. Invoked once during renderer initialization (`Renderer::init()`).
     - `CommandExecutor::resetState()`: Resets `m_activePolygonMode` to `std::nullopt`. Invoked at the entry of every `Renderer::draw()`, at every error return, and at successful completion. (`Renderer::draw()` calls `resetState()` on every draw; it does not call `init()` per draw).
     - `CommandTag::ExternalFunction`: Resets `m_activePolygonMode` to `std::nullopt` before delegating control to the external callback. This prevents external GL state modifications from desynchronizing the executor.

2. **Analysis of External Mutation Boundaries**:
   - In `main.cpp`, polygon mode is directly modified in several rendering passes outside `Renderer::draw()`:
     - `glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)` in `main.cpp:3745` after main scene draw.
     - `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` in `main.cpp:3754` for viewport wireframe overlay.
     - `glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)` in `main.cpp:3768` after wireframe overlay.
     - Grid renderer, bounding box renderer, and Dear ImGui rendering passes.
   - Because `CommandExecutor` resets `m_activePolygonMode` at `Renderer::draw()` entry/exit and `CommandTag::ExternalFunction`, external mutations outside the executor never lead to stale cache hits. The first command in any subsequent `Renderer::draw()` or post-callback draw will always issue `glPolygonMode` to re-synchronize driver state.

3. **Diagnostic Profiling Instrumentation**:
   - Extended `FrameCounters` in `header/imgui/Profiling.h` and `source/imgui/Profiling.cpp` with `uint64_t polygonModeCalls`.
   - Instrumented `GladGL::polygonMode()` (`source/imgui/GladGL.cpp`) to call `countPolygonModeCall()`.
   - Exposed `getActivePolygonMode()` inspector on `CommandExecutor` for direct white-box assertions.

#### 3. Verification & Validation Evidence

1. **Diagnostic Self-Check Suite (`--polygon-mode-selfcheck`)**:
   - 34 total assertions evaluated across 4 core sections, **34 PASS, 0 FAIL** (exit code 0):
     - **Section 1: Call Suppression and Mode Transitions (16 assertions)**:
       - Evaluated a 5-draw command sequence: `Fill` -> `Fill` -> `Line` -> `Line` -> `Fill`.
       - Draw 1 (Fill): issued driver call (`polygonModeCalls = 1`), cached `PolygonMode::Fill`.
       - Draw 2 (Fill): redundant call suppressed (`polygonModeCalls = 1`).
       - Draw 3 (Line): transition issued driver call (`polygonModeCalls = 2`), cached `PolygonMode::Line`.
       - Draw 4 (Line): redundant call suppressed (`polygonModeCalls = 2`).
       - Draw 5 (Fill): transition back issued driver call (`polygonModeCalls = 3`), cached `PolygonMode::Fill`.
       - `resetState()`: invalidated cache (`activePolygonMode == nullopt`).
       - Draw 6 (Fill): re-issued driver call despite matching pre-reset mode (`polygonModeCalls = 4`).
     - **Section 2: External Callback Boundary Isolation (10 assertions)**:
       - Drew quad in `Fill` mode (`activePolygonMode == Fill`).
       - Submitted `CommandTag::ExternalFunction` modifying driver state to `GL_LINE`. Asserted cache invalidated.
       - Next nominal `Fill` draw re-issued `glPolygonMode`, queried driver state confirmed restored to `GL_FILL`.
       - Pixel readback at center `(32, 32)` confirmed solid green `[0, 255, 0, 255]`.
     - **Section 3: Renderer Boundary Isolation (11 assertions)**:
       - Executed `Renderer::draw()` on `HelloCube.glb`. Queried driver state confirmed `GL_FILL`.
       - Modified driver state to `GL_LINE` outside the renderer.
       - Executed second `Renderer::draw()`. Verified driver state restored to `GL_FILL` without assumptions.
       - Verified zero-sampler unbinding on unit 0 and 0 GL errors.
     - **Section 4: Visual Correctness with Explicit Readback Tolerances (9 assertions)**:
       - Rendered indexed quad to offscreen 64x64 RGBA8 FBO under controlled shaders.
       - **Solid Fill Pass**: Interior off-diagonal point `(43, 21)` read back solid cyan `[0, 255, 255, 255]`; background outside `(2, 2)` read back black `[0, 0, 0, 255]`.
       - **Wireframe Line Pass**: Interior off-diagonal point `(43, 21)` read back unpainted black `[0, 0, 0, 255]` (visible distinction tolerance $>200$ channel diff from Fill); boundary edge pixel `(54, 32)` contained rendered line color.
       - **Restoration Pass**: Re-rendered `Fill`, verifying interior `(43, 21)` restored to solid cyan `[0, 255, 255, 255]`.
   - **Negative Testing**:
     - `--selfcheck-force-failure`: returned exit code 1 with valid FAIL report.
     - Unwritable report destination (`Z:/nonexistent/...`): returned exit code 2.

2. **Automated Validation Runner (`scripts/validate_p3_4_1.py` (retired, see git history))**:
   - `python scripts/validate_p3_4_1.py`: **ALL CHECKS PASSED** (exit code 0).
   - Focused runner cases verified individually:
     - `--only transitions`: PASS (exit code 0)
     - `--only boundary`: PASS (exit code 0)
     - `--only renderer-boundary`: PASS (exit code 0)
     - `--only visual`: PASS (exit code 0)
     - `--only benefit`: PASS (exit code 0)
     - `--only visual-dumps`: PASS (exit code 0)

3. **Call-Count Reduction Quantification**:
   - In the verified five-draw sequence (`Fill`, `Fill`, `Line`, `Line`, `Fill`), driver calls were reduced from 5 to 3 (40.0% reduction).
   - Suppression applies strictly to an uninterrupted sequence of draws requesting the same polygon mode after invalidation. Within such a sequence, redundant `glPolygonMode` calls are suppressed (1 driver call for that sequence instead of per-draw calls). Multiple renderer calls, callbacks, or mode transitions within a frame will re-issue driver calls as required by design.

4. **Visual Differentiation Gate**:
   - Captured full-window 1600x900 frames under default shaded mode vs wireframe mode:
     - `build/p3.4/candidate/frame_solid.jpg`
     - `build/p3.4/candidate/frame_wireframe.jpg`
   - Comparison metric: 88,436 differing pixels (6.14% of total pixels), max channel diff 205, average diff 43.62.
   - Visually confirmed clean wireframe edges vs solid filled faces on candidate binary.

5. **Linter & Code Standards**:
   - `git diff --check` passed with 0 whitespace errors.
   - Candidate binary SHA-256: `BC0DFB97C3F116C0B2AE2E5F56B7CA500604F6DC6CF4F810A88ABED1DC03BFCE`.

#### 4. Review Findings & Validation Script Repair (2026-10-22)

1. **Reviewer Findings in Validation Script (`--only benefit`)**:
   - *Stale Report Reuse*: The runner previously reused pre-existing `polygon_selfcheck_all.txt` without confirming it was freshly generated by the current candidate binary.
   - *False-Positive Assertion Matching*: The check performed naive substring search (`"Draw 2 (Fill)..." in report_text`) without checking whether the line was prefixed by `PASS:` or `FAIL:` and without requiring `Result: PASS`. The reviewer reproduced a false pass by injecting a failing report.

2. **Repairs Applied**:
   - Pre-existing report file unlinked before process execution.
   - `--only benefit` explicitly executes `--polygon-mode-selfcheck-case transitions` against the reviewed candidate executable and asserts exit code 0.
   - Implemented unified `validate_report_text` / `validate_selfcheck_report`: requires non-empty file, requires `Result: PASS`, strictly forbids `FAIL:`, and requires every specified assertion to be prefixed with `PASS:`.
   - Added focused deterministic negative tests (`run_parser_negative_checks` / `--only parser`):
     - Accepted: Valid synthetic report containing all expected passing assertions and `Result: PASS`.
     - Rejected: Synthetic report containing expected descriptions prefixed by `FAIL:` and ending in `Result: FAIL` (asserted `AssertionError`).
     - Rejected: Incomplete synthetic report missing required assertions (asserted `AssertionError`).
     - Rejected: Nonexistent report path (asserted `AssertionError`).
     - Rejected: Empty report file (asserted `AssertionError`).

3. **Newly Run Evidence vs Reused Evidence**:
   - **Reused from Independent Review**: Release build compilation, complete P3.4.1 self-check suite (`all`, `transitions`, `boundary`, `renderer-boundary`, `visual`), production transition counts, reset behavior, external callback / renderer boundary isolation, visual readback tolerances, negative exit-code handling (exit 1 on forced failure, exit 2 on unwritable path), P3.2 sampler boundary checks, P3.3 VAO boundary checks, fresh solid/wireframe captures and visual inspection of wireframe.
   - **Newly Run Evidence for this Repair**:
     - `python scripts/validate_p3_4_1.py --only parser`: PASS (exit code 0; all 5 deterministic parser test cases verified).
     - `python scripts/validate_p3_4_1.py --only benefit`: PASS (exit code 0; parser negative checks + fresh executable transitions execution + report verification passed).
     - `git diff --check`: PASS (exit code 0; zero whitespace or formatting issues).

#### 5. Scope & Guardrails Compliance

- P3.4.1 implemented and validated in strict isolation. No production C++ code was modified for this script repair.
- Overall P3.4 remains open; subsequent substeps (culling, winding, depth, blend) deferred.
- Phase 4 was not started.
- Scene rebuild frequency, draw ordering, shaders, and rendering quality remain unchanged.
- Standalone single-executable architecture and OpenGL 4.1 Core Profile compatibility preserved.
- Status: **SUBMITTED FOR REVIEW**; no commit or push performed.

#### 6. Independent review acceptance (2026-10-22)

P3.4.1 is accepted for commit. Reviewer inspected the repaired report validation
and ran `python scripts/validate_p3_4_1.py --only benefit`. This invocation passed
all five deterministic parser cases and generated a fresh transitions report
from the reviewed executable. The required passing assertions confirm five
draws issue three polygon-mode calls, and the first draw after reset issues a
new call. Stale reports are removed before execution; missing, empty, incomplete,
and explicitly failing reports cannot satisfy this benefit check.

Executable SHA-256 remains
`BC0DFB97C3F116C0B2AE2E5F56B7CA500604F6DC6CF4F810A88ABED1DC03BFCE`.
`git diff --check` passed. Earlier independent build, rendering, sampler/VAO
boundary, and image evidence was reused; no rebuild or visual rerun was needed
for this script/documentation repair. Journal boundary and call-reduction wording
now matches the implementation. Overall P3.4 remains open. No blocking findings
remain. No commit or push was performed during review.

<a id="section-12-29"></a>

### 12.29 P3.4.2 & P3.4.3 — CommandExecutor Culling and Winding Cache (2026-10-22)

#### 1. Context & Accepted Baseline

- **Accepted Baseline**: Commit `4c54193` ("Cache executor polygon mode with boundary and call-count validation").
- **Task Scope**: Batch implementation of P3.4.2 (executor culling-state caching) and P3.4.3 (executor front-face winding caching). Suppress redundant `glEnable(GL_CULL_FACE)`, `glDisable(GL_CULL_FACE)`, `glCullFace()`, and `glFrontFace()` driver calls across consecutive draws while strictly preserving rendering behavior. Invalidate cache at `CommandExecutor::resetState()`, `Renderer::draw()` boundaries, `CommandExecutor::init()`, and `CommandTag::ExternalFunction`.
- **Exclusions**: Do not cache depth test, blending, programs, textures, samplers, framebuffers, or other state categories in this batch. Do not start Phase 4. Preserve scene rebuild frequency, draw ordering, shaders, rendering quality, standalone single-executable architecture, and OpenGL 4.1 Core Profile compatibility. Preserve accepted P3.2 sampler, P3.3 VAO, and P3.4.1 polygon-mode caching behavior. Do not commit or push. Stop for review when complete.

#### 2. Ownership, Mutation Boundaries & Architecture

1. **State Ownership & Invalidation in CommandExecutor**:
   - `CommandExecutor` now tracks culling and winding state via three distinct shadow variables:
     ```cpp
     std::optional<bool> m_cullingEnabled = std::nullopt;
     std::optional<gl::Culling> m_cullFace = std::nullopt;
     std::optional<gl::Winding> m_activeWinding = std::nullopt;
     ```
   - **Culling Caching Logic (`setupCulling`)**:
     - Separates `GL_CULL_FACE` enablement from cull-face selection. Disabling culling (`glDisable(GL_CULL_FACE)`) does not reset the driver's selected cull face.
     - When culling is requested (`command.culling != gl::Culling::None`), `m_cullingEnabled` is checked and set, and `m_cullFace` is checked and set via `glCullFace(GLenum(command.culling))`.
     - When culling is disabled (`command.culling == gl::Culling::None`), `m_cullingEnabled` is set to `false`, but `m_cullFace` is not mutated. If `m_cullFace` was `nullopt` after invalidation, it remains `nullopt`, ensuring a subsequent enabled draw cannot make an unsupported assumption about driver cull-face state.
   - **Winding Caching Logic (`setupFrontFace`)**:
     - Tracks front-face winding convention independently via `m_activeWinding`.
     - Sets `glFrontFace(GLenum(command.winding))` only when `m_activeWinding` is unset or differs.
     - Correctly preserves winding established during draws with culling disabled when culling is later re-enabled, and preserves mirrored geometry behavior (negative determinant world transforms).
   - **Boundary Invalidation Points**:
     - `CommandExecutor::init()`: Resets `m_cullingEnabled`, `m_cullFace`, and `m_activeWinding` to `std::nullopt`. Invoked once during `Renderer::init()`.
     - `CommandExecutor::resetState()`: Resets all three shadow variables to `std::nullopt`. Invoked at `Renderer::draw()` entry, at all error returns, and at successful completion.
     - `CommandTag::ExternalFunction`: Resets all three shadow variables to `std::nullopt` prior to delegating control to external callbacks.

2. **External GL Mutations Isolated**:
   - Dear ImGui, AntiAliasing (SMAA/FXAA/TAA), BoundingBoxRenderer, and GridRenderer passes mutate GL culling/winding outside the executor.
   - Because `CommandExecutor` invalidates shadow state at `Renderer::draw()` entry/exit and `CommandTag::ExternalFunction`, external mutations cannot corrupt executor state.

3. **Diagnostic Profiling Instrumentation**:
   - Extended `FrameCounters` in `header/imgui/Profiling.h` and `source/imgui/Profiling.cpp` with:
     - `uint64_t cullFaceEnableCalls` (tracked in `GladGL::enable` and `GladGL::disable` for `GL_CULL_FACE`).
     - `uint64_t cullFaceSelectionCalls` (tracked in `GladGL::cullFace`).
     - `uint64_t frontFaceCalls` (tracked in `GladGL::frontFace`).
   - Exposed inspector methods on `CommandExecutor`: `getCullingEnabled()`, `getCullFace()`, and `getActiveWinding()`.

#### 3. Verification & Validation Evidence

1. **Diagnostic Self-Check Suite (`--cull-winding-selfcheck`)**:
   - 92 total assertions evaluated across 4 core sections, **92 PASS, 0 FAIL** (exit code 0):
     - **Section 1: Call Counts and Mode Transitions (35 assertions)**:
       - Exercised 10-draw sequence:
         - Draw 1 (`Back`, `CCW`): issued enable (1), cullFace (1), frontFace (1). All 3 cached.
         - Draw 2 (`Back`, `CCW`): redundant draw, all 3 calls suppressed (counts remain 1, 1, 1).
         - Draw 3 (`None`, `CCW`): disable issued (enableCalls=2), cullFace untouched (1), winding suppressed (1).
         - Draw 4 (`None`, `CW`): redundant disable suppressed (2), winding transition issued (frontFaceCalls=2).
         - Draw 5 (`Back`, `CW`): re-enabling culling issued enable (3); cullFace matched driver Back (suppressed, 1); winding matched driver CW (suppressed, 2). Only 1 call issued!
         - Draw 6 (`Front`, `CW`): enable suppressed (3), cullFace transition issued (2), winding suppressed (2).
         - Draw 7 (`Front`, `CW`): redundant draw, all 3 calls suppressed (counts remain 3, 2, 2).
         - Draw 8 (`None`, `CW`): disable issued (4), cullFace untouched (2), winding suppressed (2).
         - Draw 9 (`Front`, `CCW`): enable issued (5), cullFace matched driver Front (suppressed, 2), winding transition issued (3).
         - Draw 10 (`Back`, `CCW`): enable suppressed (5), cullFace transition issued (3), winding suppressed (3).
         - Invalidation: `resetState()` invalidated all 3 shadow variables (`nullopt`).
         - Draw 11 (`Back`, `CCW`): first draw after reset re-issued all 3 calls (enableCalls=6, selectionCalls=4, frontFaceCalls=4).
     - **Section 2: Rendering Correctness with Pixel Readbacks (13 assertions)**:
       - Evaluated on offscreen 64x64 RGBA8 FBO with two distinct planar triangles: Left CCW (Green, interior sample `(20, 20)`) and Right CW (Blue, interior sample `(44, 20)`).
       - *Pass A (Culling Disabled)*: Both Left (Green) and Right (Blue) visible.
       - *Pass B (Cull Back, CCW)*: Left visible (Green), Right back-facing culled (Black `[0, 0, 0, 255]`).
       - *Pass C (Cull Back, CW)*: Left back-facing culled (Black), Right front-facing visible (Blue `[0, 0, 255, 255]`).
       - *Pass D (Cull Front, CCW)*: Left front-facing culled (Black), Right back-facing visible (Blue).
       - *Pass E (Culling Restored None)*: Both Left Green and Right Blue restored.
       - *Pass F (Suppressed Draw)*: Correct visual output retained while redundant driver calls suppressed.
     - **Section 3: Boundary Isolation (17 assertions)**:
       - `CommandTag::ExternalFunction`: Callback executed with direct driver mutations (`glDisable(GL_CULL_FACE)`, `glCullFace(GL_FRONT)`, `glFrontFace(GL_CW)`). Confirmed executor shadow variables invalidated (`nullopt`). Subsequent draw re-established driver state (`GL_TRUE`, `GL_BACK`, `GL_CCW`) and verified correct pixel readback.
       - `Renderer::draw`: Inter-frame driver state corruption between renderer calls did not leak; second `Renderer::draw` succeeded with 0 GL errors and left sampler 0 unbound.
     - **Section 4: Application Integration Case (7 assertions)**:
       - Created compact glTF fixture `build/p3.4/fixtures/cull_winding_fixture.gltf` covering single-sided front-facing (Green), single-sided back-facing (Red), double-sided back-facing (Yellow), and mirrored negative-scale geometry (`scale: [-1, 1, 1]`, Cyan).
       - Rendered fixture with 0 GL errors; verified driver state following draw was `GL_CULL_FACE=GL_TRUE`, `GL_CULL_FACE_MODE=GL_BACK`, and `GL_FRONT_FACE=GL_CW` (from mirrored geometry).
       - Additional `HelloCube.glb` smoke render completed with 0 GL errors; this is not a baseline image comparison.
   - **Negative Testing**:
     - `--selfcheck-force-failure`: returned exit code 1 with valid `Result: FAIL` report.
     - Unwritable report destination (`Z:/nonexistent/...`): returned exit code 2.

2. **Automated Validation Runner (`scripts/validate_p3_4.py` (retired, see git history))**:
   - `python scripts/validate_p3_4.py`: **ALL CHECKS PASSED** (exit code 0).
   - Focused runner cases verified individually:
     - `--only transitions`: PASS (exit code 0)
     - `--only correctness`: PASS (exit code 0)
     - `--only boundary`: PASS (exit code 0)
     - `--only integration`: PASS (exit code 0)
     - `--only benefit`: PASS (exit code 0; unlinked pre-existing report, executed fresh binary run, strictly validated report)
     - `--only parser`: PASS (exit code 0; 5/5 synthetic parser tests passed)
     - `--only visual-dumps`: PASS (exit code 0)

3. **Call-Count Reduction Quantification**:
   - In the verified 10-draw sequence:
     - Enable/disable calls: 5 actual calls vs 10 baseline (5 calls suppressed).
     - Cull-face selection calls: 3 actual calls vs 7 baseline (4 calls suppressed); the old implementation skipped selection on the three culling-disabled draws.
     - Front-face winding calls: 3 actual calls vs 10 baseline (7 calls suppressed).
     - Total: 11 actual calls vs 27 baseline calls (**59.3% reduction / 16 calls suppressed**). Baseline counts are inferred from the accepted unconditional implementation, not a separate timed run.
   - Suppression applies strictly to uninterrupted sequences requesting matching culling/winding after invalidation. Intervening transitions, callbacks, or renderer boundaries re-issue driver calls as required.

4. **Visual Differentiation Gate (`build/p3.4/candidate/`)**:
   - Full-window 1600x900 captures on `cull_winding_fixture.gltf`:
     - Standard view: Quad 1 (Green), Quad 3 (Yellow), Quad 4 (Cyan) visible; Quad 2 (Red back-facing) culled.
     - Standard vs ForceDoubleSided: 16,183 differing pixels (1.12%), max diff 102 (Quad 2 revealed).
     - Standard vs InvertWinding: 61,573 differing pixels (4.28%), max diff 130 (Quads 1 & 4 culled, Quad 2 revealed).
   - Confirmed expected visual distinction on candidate binary.

5. **Shared Boundary Regressions Check**:
   - `python scripts/validate_p3_4_1.py --only benefit`: PASS (exit code 0).
   - `python scripts/validate_p3_3.py --only boundary`: PASS (exit code 0).
   - `python scripts/validate_p3_2.py --only boundary`: PASS (exit code 0).

6. **Linter & Code Standards**:
   - `git diff --check` passed with 0 whitespace errors.
   - Candidate binary SHA-256: `88D58080EEA98C40C31E006BCC9C383BFAEB281EE84562FC615AFB3CAAA5E3B5`.

#### 4. Newly Run Evidence vs Reused Evidence

- **Newly Run Evidence for P3.4.2 + P3.4.3**:
  - Full automated runner: `python scripts/validate_p3_4.py` (exit code 0).
  - All focused cases: `transitions`, `correctness`, `boundary`, `integration`, `benefit`, `parser`, `visual-dumps`.
  - Negative exit-code tests: exit code 1 on forced failure, exit code 2 on unwritable path.
  - Shared boundary regression checks: P3.4.1 benefit, P3.3 VAO boundary, P3.2 sampler boundary.
  - Candidate binary hash `88D58080EEA98C40C31E006BCC9C383BFAEB281EE84562FC615AFB3CAAA5E3B5` and `git diff --check`.
- **Reused Evidence**:
  - Independent acceptance of baseline `4c54193` (P3.4.1).
  - Full AA matrix captures and long steady-state benchmark runs from earlier phases.

#### 5. Scope & Guardrails Compliance

- Batch implementation strictly limited to P3.4.2 (culling state) and P3.4.3 (front-face winding).
- Overall P3.4 remains open; depth testing and blending caching deferred.
- Phase 4 was not started.
- Scene rebuild frequency, draw ordering, shaders, and rendering quality remain unchanged.
- Standalone single-executable architecture and OpenGL 4.1 Core Profile compatibility preserved.
- Status: **SUBMITTED FOR REVIEW**; no commit or push performed.

#### 6. Independent review acceptance and small validation repairs (2026-10-22)

P3.4.2 and P3.4.3 are accepted for commit. Reviewer built Release and ran the
complete small batch runner on submitted candidate 88D58080EEA98C40C31E006BCC9C383BFAEB281EE84562FC615AFB3CAAA5E3B5.
Transition, pixel correctness, integration, negative-report, and visual checks
passed. Reviewer opened standard and inverted-winding fixture captures and
confirmed the expected visible/culled quads. Image-difference thresholds alone
are not proof of baseline equivalence; no baseline image comparison is claimed.

Reviewer corrected the inferred baseline counts in the script and journal:
10 enable/disable + 7 cull selections + 10 winding calls = 27 calls, versus
11 measured candidate calls (59.3% reduction). Disabled draws never selected a
cull face in the accepted baseline.

Reviewer strengthened the renderer-boundary test to reuse the existing command
generator in a second Renderer::draw and assert restored GL_CULL_FACE,
GL_CULL_FACE_MODE, and GL_FRONT_FACE after deliberate external mutations.
The explicit single-sided mirrored fixture supplies the expected Back/CW state;
HelloCube must not be assumed to end with culling enabled. This was a test-only
repair; production renderer code was unchanged.

Final candidate SHA-256:
`7C81FB694E27EA534B158DC15C7AFD5699EAE8D777088C891619E8B86B182394`.
The repaired `--only boundary` passed all 25 assertions. The corrected
`--only benefit` passed its fresh transitions run and five parser cases.
P3.4.1 polygon, P3.3 VAO, and P3.2 sampler `--only boundary` checks passed during
this review; subsequent edits only adjusted this new boundary fixture, so those
results and visual evidence were reused. No broad regression matrix was run.
No blocking findings remain. Overall P3.4 stays open. No commit or push performed.

<a id="section-12-30"></a>

### 12.30 P3.4.4 — CommandExecutor Depth Test Enablement and Depth Function Cache (2026-10-22)

#### 1. Context & Accepted Baseline

- **Accepted Baseline**: Commit `46ae7bd` ("Cache culling and winding state with focused rendering validation").
- **Task Scope**: Complete batch implementation of P3.4.4 (depth-test enablement and depth-function caching in `CommandExecutor`). Suppress redundant `glEnable(GL_DEPTH_TEST)`, `glDisable(GL_DEPTH_TEST)`, and `glDepthFunc(GL_LEQUAL)` driver calls across consecutive commands while strictly preserving rendering semantics. Invalidate cache at `CommandExecutor::resetState()`, `Renderer::draw()` boundaries, `CommandExecutor::init()`, and `CommandTag::ExternalFunction`.
- **Exclusions**: Do not cache blending, depth write masks, programs, textures, samplers, framebuffers, or other state categories in this batch. Do not start Phase 4. Preserve scene rebuild frequency, draw ordering, shaders, rendering quality, standalone single-executable architecture, and OpenGL 4.1 Core Profile compatibility. Preserve accepted P3.2 sampler, P3.3 VAO, P3.4.1 polygon-mode, and P3.4.2/P3.4.3 culling/winding caching behavior. Do not commit or push. Stop for review when complete.

#### 2. Ownership, Mutation Boundaries & Architecture

1. **State Ownership & Invalidation in CommandExecutor**:
   - `CommandExecutor` tracks depth state via two distinct shadow variables:
     ```cpp
     std::optional<bool> m_depthTestEnabled = std::nullopt;
     std::optional<GLenum> m_depthFunc = std::nullopt;
     ```
   - **Depth Caching Logic (`setupDepthTest`)**:
     - When depth testing is requested (`command.depthTestEnabled == true`):
       - If `m_depthTestEnabled` is unset or `false`, updates `m_depthTestEnabled = true` and issues `glEnable(GL_DEPTH_TEST)`.
       - If `m_depthFunc` is unset or not `GL_LEQUAL`, updates `m_depthFunc = GL_LEQUAL` and issues `glDepthFunc(GL_LEQUAL)`.
     - When depth testing is disabled (`command.depthTestEnabled == false`):
       - If `m_depthTestEnabled` is unset or `true`, updates `m_depthTestEnabled = false` and issues `glDisable(GL_DEPTH_TEST)`.
       - `m_depthFunc` is untouched. Disabling depth test in GL does not change the driver's depth function. Crucially, if `m_depthFunc` was `nullopt` after invalidation, it remains `nullopt` (an initially disabled command does not falsely mark the depth function as known). When subsequently re-enabled, `m_depthFunc` is recognized as unknown and `glDepthFunc(GL_LEQUAL)` is issued.
   - **Boundary Invalidation Points**:
     - `CommandExecutor::init()`: Resets `m_depthTestEnabled` and `m_depthFunc` to `std::nullopt`. Invoked during `Renderer::init()`.
     - `CommandExecutor::resetState()`: Resets both shadow variables to `std::nullopt`. Invoked at `Renderer::draw()` entry, on all error returns, and upon draw completion.
     - `CommandTag::ExternalFunction`: Resets both shadow variables to `std::nullopt` prior to executing callbacks.

2. **External GL Mutations Isolated**:
   - Passes in `OpenGL-Renderer/` (`OpaquePass`, `TransparentPass`, `TransmissivePass`, `OutlinePass`, etc.) emit `Command`s. No direct GL depth mutations occur inside passes.
   - Dear ImGui, AntiAliasing (FXAA/TAA), BoundingBoxRenderer, and GridRenderer passes mutate GL depth state outside the executor.
   - Because `CommandExecutor` invalidates shadow state at `Renderer::draw()` entry/exit and `CommandTag::ExternalFunction`, external mutations cannot corrupt executor state.
   - Context ownership: single OpenGL context on the GLFW main thread; the application shares its `Renderer` and `CommandExecutor` across viewport rendering. Invalidation on entry/exit prevents cached assumptions from leaking between viewport draws.

3. **Diagnostic Profiling Instrumentation**:
   - Extended `FrameCounters` in `header/imgui/Profiling.h` and `source/imgui/Profiling.cpp` with:
     - `uint64_t depthTestEnableCalls` (tracked in `GladGL::enable` and `GladGL::disable` for `GL_DEPTH_TEST`).
     - `uint64_t depthFuncCalls` (tracked in `GladGL::depthFunc`).
   - Exposed inspector methods on `CommandExecutor`: `getDepthTestEnabled()` and `getDepthFunc()`.

#### 3. Verification & Validation Evidence

1. **Diagnostic Self-Check Suite (`--depth-selfcheck`)**:
   - 74 total assertions evaluated across 4 core sections, **74 PASS, 0 FAIL** (exit code 0):
     - **Section 1: Call Counts and Mode Transitions (30 assertions)**:
       - Exercised 9-command transition sequence:
         - Cmd 1 (`Enabled`): issued `glEnable(GL_DEPTH_TEST)` and `glDepthFunc(GL_LEQUAL)` (counts 1, 1). Cached as `true` and `GL_LEQUAL`.
         - Cmd 2 (`Enabled` -> `Enabled`): redundant draw, both calls suppressed (counts remain 1, 1).
         - Cmd 3 (`Enabled` -> `Disabled`): issued `glDisable(GL_DEPTH_TEST)` (count 2), depthFunc untouched (remains 1).
         - Cmd 4 (`Disabled` -> `Disabled`): redundant disable suppressed (count remains 2, depthFunc 1).
         - Cmd 5 (`Disabled` -> `Enabled`): re-enable issued `glEnable` (count 3), depthFunc already known as `GL_LEQUAL` suppressed (remains 1).
         - Invalidation: `resetState()` invalidated both shadow variables to `nullopt`.
         - Cmd 6 (`Post-Reset Enabled`): first draw re-issued `glEnable` (count 4) and `glDepthFunc` (count 2).
         - Invalidation: `resetState()` invalidated both shadow variables to `nullopt`.
         - Cmd 7 (`Post-Reset Disabled`): issued `glDisable` (count 5), depthFunc remains `nullopt` (count 2).
         - Cmd 8 (`Unknown DepthFunc Re-Enabled`): issued `glEnable` (count 6) and `glDepthFunc` (count 3).
         - Callback: `ExternalFunction` callback corrupted GL state; shadow variables reset to `nullopt`.
         - Cmd 9 (`Post-Callback Restoration`): re-established `glEnable` (count 7) and `glDepthFunc` (count 4). Driver verified via GL queries (`GL_TRUE`, `GL_LEQUAL`).
     - **Section 2: Depth Correctness with Real FBO (5 assertions)**:
       - Evaluated on offscreen 64x64 RGBA8 FBO with DEPTH24 renderbuffer attachment.
       - *Pass A (Depth Enabled)*: Nearer Green ($Z=0.0$) remains visible; Far Red ($Z=0.5$) rejected. Readback center pixel is `[0, 255, 0, 255]`.
       - *Pass B (Depth Disabled)*: Far Red overwrites Near Green. Readback center pixel is `[255, 0, 0, 255]`.
       - *Pass C (Depth Re-Enabled)*: Occlusion restored. Readback center pixel is `[0, 255, 0, 255]`.
       - *Pass D (Equal Depth GL_LEQUAL)*: Near Green ($Z=0.0$) followed by Equal Blue ($Z=0.0$). Under `GL_LEQUAL`, $0.0 \le 0.0$ is TRUE; Blue passes and overwrites Green (`[0, 0, 255, 255]`), distinguishing `GL_LEQUAL` from `GL_LESS`.
       - *Pass E (Suppressed Redundant Commands)*: Output preserved identically while redundant calls suppressed.
     - **Section 3: Boundary Isolation (9 assertions)**:
       - `ExternalFunction`: callback corrupted driver state (`glDisable`, `glDepthFunc(GL_GREATER)`); confirmed executor shadow state invalidated; subsequent draw re-establishes driver state and pixel readback.
       - `Renderer::draw`: inter-frame GL state corruption between renderer calls does not leak; second `Renderer::draw` succeeded with 0 GL errors, restored `GL_DEPTH_TEST` and `GL_DEPTH_FUNC`, and left sampler 0 unbound.
     - **Section 4: Application Integration Case (5 assertions)**:
       - Rendered `HelloCube.glb` cleanly with 0 GL errors; driver state verified post-draw (`GL_DEPTH_TEST == GL_TRUE`, `GL_DEPTH_FUNC == GL_LEQUAL`), sampler unit 0 unbound.
   - **Negative Testing**:
     - `--selfcheck-force-failure`: returned exit code 1 with valid `Result: FAIL` report.
     - Unwritable report destination (`Z:/nonexistent/...`): returned exit code 2.

2. **Automated Validation Runner (`scripts/validate_p3_4_4.py` (retired, see git history))**:
   - `python scripts/validate_p3_4_4.py`: **ALL CHECKS PASSED** (exit code 0).
   - Focused runner cases verified individually:
     - `--only transitions`: PASS (exit code 0)
     - `--only correctness`: PASS (exit code 0)
     - `--only boundary`: PASS (exit code 0)
     - `--only integration`: PASS (exit code 0)
     - `--only benefit`: PASS (exit code 0; unlinked pre-existing report, executed fresh binary run, strictly validated report)
     - `--only parser`: PASS (exit code 0; 5/5 synthetic parser tests passed)
     - `--only visual-dumps`: PASS (exit code 0; captured `depth_hellocube.jpg`, 1600x900)

3. **Call-Count Reduction Quantification**:
   - In the verified 9-command transition sequence:
     - Enable/disable calls: 7 actual calls vs 9 baseline (2 calls suppressed).
     - Depth function calls: 4 actual calls vs 6 baseline (2 calls suppressed).
     - Total: 11 actual calls vs 15 baseline calls (**26.7% reduction / 4 calls suppressed**).
   - In an uninterrupted sequence (e.g. 10 consecutive enabled commands):
     - Enable/disable calls: 1 actual vs 10 baseline (9 calls suppressed).
     - Depth function calls: 1 actual vs 10 baseline (9 calls suppressed).
     - Total: 2 actual vs 20 baseline (**90.0% reduction / 18 calls suppressed**).
   - Suppression applies strictly to uninterrupted sequences requesting matching depth state after invalidation. Intervening transitions, callbacks, or renderer boundaries re-issue driver calls as required.

4. **Shared Boundary Regressions Check**:
   - `python scripts/validate_p3_4.py --only benefit`: PASS (exit code 0).
   - `python scripts/validate_p3_4_1.py --only benefit`: PASS (exit code 0).
   - `python scripts/validate_p3_3.py --only boundary`: PASS (exit code 0).
   - `python scripts/validate_p3_2.py --only boundary`: PASS (exit code 0).

5. **Linter & Code Standards**:
   - `git diff --check` passed with 0 whitespace errors.
   - Candidate binary SHA-256: `6A3789E8D08B492620FDA5968E38DE73F30451558DFBDEA3920A41209F81D0D8`.

#### 4. Newly Run Evidence vs Reused Evidence

- **Newly Run Evidence for P3.4.4**:
  - Full automated runner: `python scripts/validate_p3_4_4.py` (exit code 0).
  - All focused cases: `transitions`, `correctness`, `boundary`, `integration`, `benefit`, `parser`, `visual-dumps`.
  - Negative exit-code tests: exit code 1 on forced failure, exit code 2 on unwritable path.
  - Shared boundary regression checks: P3.4 benefit, P3.4.1 benefit, P3.3 VAO boundary, P3.2 sampler boundary.
  - Candidate binary hash `6A3789E8D08B492620FDA5968E38DE73F30451558DFBDEA3920A41209F81D0D8` and `git diff --check`.
- **Reused Evidence**:
  - Independent acceptance of baseline `46ae7bd` (P3.4.2 + P3.4.3).
  - Full AA matrix captures and long steady-state benchmark runs from earlier phases.

#### 5. Scope & Guardrails Compliance

- Implementation strictly limited to P3.4.4 (depth-test enablement and depth function).
- Overall P3.4 remains open; blending caching deferred.
- Phase 4 was not started.
- Scene rebuild frequency, draw ordering, shaders, and rendering quality remain unchanged.
- Standalone single-executable architecture and OpenGL 4.1 Core Profile compatibility preserved.
- Status: **SUBMITTED FOR REVIEW**; no commit or push performed.

#### 6. Independent review acceptance (2026-10-22)

P3.4.4 is accepted for commit. Reviewer restored the pre-existing unconditional
checkGLError diagnostic in setupDepthTest, removed duplicate manual program
handle deletion from the RAII-owned test fixtures, made the runner use
sys.executable for child scripts, and corrected the journal's shared renderer
ownership description. The depth cache algorithm itself was unchanged.

Final Release executable SHA-256:
`D08310EB4AD0E937072015D4940D8076040BB34A196F6AFDE2E193E2B775964F`.
Reviewer ran scripts/validate_p3_4_4.py successfully: depth self-check and negative
exit paths, parser cases, fresh benefit transitions, application frame capture,
and the runner's narrow existing-substep checks. Opened the regenerated
HelloCube image. The image is visual smoke evidence, not a baseline pixel
comparison. Controlled FBO readbacks establish occlusion, disabled-depth output,
equal-depth LEQUAL behavior, and restoration; renderer state queries verify
inter-frame restoration. The measured sequence uses 11 calls versus 15 inferred
baseline calls; the ten-consecutive-command example is an analytical estimate,
not a measured application speedup. git diff --check passed.

No blocking findings remain. No broad AA or performance matrix was repeated.
Blending and the combined P3.4 integration/exit review remain future work.
No commit or push was performed during this review.


---

<a id="section-12-31"></a>

### 12.31 Phase 3 Batch: P3.4.5 Blending Cache, Combined P3.4 Integration, and Phase 3 Exit-Gate Audit

#### 1. Executive Summary & Review Scope

This entry documents the completion of the entire remaining Phase 3 batch:
1. **PART A — P3.4.5 Blending State Cache**: Redundant `glEnable(GL_BLEND)` / `glDisable(GL_BLEND)`, `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`, and `glBlendEquation(GL_FUNC_ADD)` driver calls are suppressed in `CommandExecutor` using shadow state.
2. **PART B — Combined P3.4 Integration Validation**: Comprehensive multi-category integration testing verifying seamless interaction among polygon mode, culling, winding, depth testing, and blending across opaque/transparent overlap, wireframe overlay, multi-viewport simulation, and external AA/ImGui boundaries.
3. **PART C — Phase 3 Exit-Gate Audit & Requirement Mapping**: Systematic reconciliation of every requirement in [§5.5](#section-5-5) against concrete evidence gathered across P3.1 through P3.4, preparing the Phase 3 exit gate for final review.

**Guardrails & Scope Enforcement**:
- Accepted baseline: Commit `614a2ee` ("Cache depth-test state with focused occlusion and boundary validation").
- Scope strictly bounded: Caching applied only to blending enablement, factors, and equation currently established by `CommandExecutor::setupBlendFunc()`.
- Phase 4 was not started.
- Scene rebuild frequency, draw ordering, shaders, transparent sorting, and visual rendering quality remain unchanged.
- Standalone single-executable architecture and OpenGL 4.1 Core Profile compatibility preserved.

---

#### 2. PART A — P3.4.5 Blending-State Caching Implementation

##### 2.1 Target Operations and Invalidation Rules
`CommandExecutor::setupBlendFunc()` manages blending state in the renderer pipeline:
- `glEnable(GL_BLEND)` / `glDisable(GL_BLEND)` based on `command.blend`.
- `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` when blending is enabled.
- `glBlendEquation(GL_FUNC_ADD)` when blending is enabled.

In `CommandExecutor.h`, four shadow state variables are added:
```cpp
std::optional<bool> m_blendEnabled = std::nullopt;
std::optional<GLenum> m_blendSrcFactor = std::nullopt;
std::optional<GLenum> m_blendDstFactor = std::nullopt;
std::optional<GLenum> m_blendEquation = std::nullopt;
```

**Invalidation & Mutation Boundaries**:
1. `init()` & `resetState()`: Reset all 4 variables to `std::nullopt`.
2. `CommandTag::ExternalFunction`: Resets all 4 variables to `std::nullopt`, ensuring external OpenGL callbacks (e.g. ImGui rendering, custom passes) cannot corrupt shadow state.
3. `Renderer::draw()`: Calls `resetState()` at pass entry. At pass exit, restores `GL_BLEND == GL_FALSE` (the opaque pass default), checks for 0 GL errors, and ensures sampler 0 is unbound.

##### 2.2 Blend Semantics Preservation
- When a command requests `blend == false`:
  - If `m_blendEnabled != false`, issues `glDisable(GL_BLEND)` and updates `m_blendEnabled = false`.
  - Blend factors and equation are **not** modified and remain cached in shadow state (factors/equation are irrelevant when blend is disabled).
- When a command requests `blend == true`:
  - If `m_blendEnabled != true`, issues `glEnable(GL_BLEND)` and updates `m_blendEnabled = true`.
  - If factors are unset or differ from `(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`, issues `glBlendFunc` and updates `m_blendSrcFactor` and `m_blendDstFactor`.
  - If equation is unset or differs from `GL_FUNC_ADD`, issues `glBlendEquation` and updates `m_blendEquation`.
- Exactly preserves standard alpha blending mathematics:
  $$\text{Color}_{\text{result}} = \text{Color}_{\text{src}} \cdot \alpha_{\text{src}} + \text{Color}_{\text{dst}} \cdot (1 - \alpha_{\text{src}})$$
  $$\text{Alpha}_{\text{result}} = \alpha_{\text{src}} \cdot \alpha_{\text{src}} + \alpha_{\text{dst}} \cdot (1 - \alpha_{\text{src}})$$

##### 2.3 Instrumented Call Counters
In `header/imgui/Profiling.h` and `source/imgui/Profiling.cpp`, `FrameCounters` tracks:
- `uint64_t blendEnableCalls = 0;` (incremented in `GladGL::enable(GL_BLEND)` and `GladGL::disable(GL_BLEND)`).
- `uint64_t blendFuncCalls = 0;` (incremented in `GladGL::blendFunc`).
- `uint64_t blendEquationCalls = 0;` (incremented in `GladGL::blendEquation`).
Serialized via `countersToJson()` for profiling and telemetry.

##### 2.4 Measured vs. Analytical Call-Count Quantification
- **Measured (Section 1 9-command transition sequence with state invalidations and callbacks)**:
  - Baseline calls: 9 enable/disable + 6 `blendFunc` + 6 `blendEquation` = 21 calls.
  - Actual with cache: 7 enable/disable + 4 `blendFunc` + 4 `blendEquation` = 15 calls.
  - Suppressed: 2 enable/disable + 2 `blendFunc` + 2 `blendEquation` = **6 calls suppressed (28.6% reduction)**.
- **Analytical Estimate (Uninterrupted run of 10 enabled commands without state transitions)**:
  - Baseline: 10 enable + 10 `blendFunc` + 10 `blendEquation` = 30 calls.
  - Actual with cache: 1 enable + 1 `blendFunc` + 1 `blendEquation` = 3 calls.
  - Suppressed: **27 calls suppressed (90.0% reduction)**.
  *(Note: The 90.0% reduction represents an analytical upper bound for an idealized sequence. Real scene performance depends on transparent pass clustering and intervening passes).*

---

#### 3. PART B — Combined P3.4 Integration Validation

The dedicated diagnostic suite (`header/imgui/BlendTest.h`, `source/imgui/BlendTest.cpp`) evaluates 95 distinct assertions across 5 sections:

##### 3.1 Scenario 1: Opaque vs. Transparent Overlap with Depth
- Base: Solid Blue FBO $[0, 0, 255, 255]$.
- Near Opaque Quad (Green, $Z = 0.0$, Depth=True, Blend=False): Renders over blue $\rightarrow [0, 255, 0, 255]$.
- Front Transparent Quad (Red 50% alpha, $Z = -0.2$, Depth=True, Blend=True): Blends over green $\rightarrow [128, 128, 0, 191]$ ($R = 0.5 \cdot 255 = 127.5 \approx 128, G = 0.5 \cdot 255 = 127.5 \approx 128, B = 0, A = 0.5 \cdot 0.5 + 1.0 \cdot 0.5 = 0.75 \implies 191$).
- Back Transparent Quad (Blue 50% alpha, $Z = 0.5$, Depth=True, Blend=True): Positioned behind near opaque quad. Depth test (`GL_LEQUAL`) rejects all fragments; pixel readback remains strictly $[128, 128, 0, 191]$.
- Separate Color-over-Blue Blend Verification: In Pass A of the diagnostic fixture, 50% alpha red $[1.0, 0, 0, 0.5]$ blended over solid blue $[0, 0, 255, 255]$ produces violet $[128, 0, 128, 191]$ ($R = 0.5 \cdot 255 = 128, G = 0, B = 0.5 \cdot 255 = 128, A = 0.5 \cdot 0.5 + 1.0 \cdot 0.5 = 0.75 \implies 191$).
- Driver queries confirm `GL_BLEND == GL_TRUE`, `GL_DEPTH_TEST == GL_TRUE`, `GL_BLEND_SRC_RGB == GL_SRC_ALPHA`, `GL_BLEND_DST_RGB == GL_ONE_MINUS_SRC_ALPHA`, and `GL_BLEND_EQUATION_RGB == GL_FUNC_ADD`.

##### 3.2 Scenario 2: Interleaved Wireframe Overlay
- Executes transition sequence: `PolygonMode::Fill` $\rightarrow$ `PolygonMode::Line` $\rightarrow$ `PolygonMode::Fill`.
- Verifies that polygon mode transitions correctly while depth and blending states remain stable.

##### 3.3 Scenario 3: Multi-Viewport Simulation
- Viewport A: Back-face culling, CCW winding, Fill polygon mode, Blending disabled.
- Viewport B: No culling, CW winding, Line polygon mode, Blending enabled.
- Successive alternation between Viewport A and Viewport B verifies that executor state resets cleanly at viewport boundaries, driver state matches each viewport's requirements, and zero cross-viewport state pollution occurs.

##### 3.4 Scenario 4: External AA / ImGui Boundary Invalidation
- Simulates external AA post-process or ImGui draw callback that corrupts driver polygon mode, culling, winding, depth, and blend state.
- This synthetic scenario mutates GL state directly and calls `resetState()` before the next draw. It checks reset recovery, not execution of real AA/ImGui callbacks or the `ExternalFunction` dispatch path; separate boundary checks cover that dispatch.

---

#### 4. Phase 3 evidence audit (corrected during final review)

Prior accepted evidence is reused; this repair does not rerun the Phase 3 matrix.

| Task | Accepted implementation/evidence | Benefit and limits |
|---|---|---|
| P3.1 AA targets | [§12.25](#section-12-25); commit `c5ecd98` | Mode-specific target allocation and lifecycle checks. Attachment storage estimates are not measurements of physical driver VRAM or reclamation timing. |
| P3.2 samplers | [§12.26](#section-12-26); commit `6636b28` | Sampling state reuse; fallback unbinding and cleanup checked. CPU object size and driver memory were not measured here. |
| P3.3 vertex layouts | [§12.27](#section-12-27); commit `b506b3c` | Layout reuse with per-mesh invalidation and lifecycle checks. No new claim of program-independent sharing or Phase 4 scope. |
| P3.4.1 polygon mode | [§12.28](#section-12-28); commit `4c54193` | 3 measured calls versus 5 inferred uncached calls in the focused sequence. |
| P3.4.2–3 culling/winding | [§12.29](#section-12-29); commit `46ae7bd` | 11 measured calls versus 27 inferred uncached calls. |
| P3.4.4 depth | [§12.30](#section-12-30); commit `614a2ee` | 11 measured calls versus 15 inferred uncached calls. |
| P3.4.5 blend | [§12.31](#section-12-31); current uncommitted batch | Previously passing transition evidence: 15 measured calls versus 21 inferred uncached calls (28.6%). Ten identical enabled commands would require 3 versus 30 calls analytically; this is not a measured frame-time gain. |

The previously reported 95-assertion blend suite, parser/error checks, frame dump, and prior shared-boundary checks remain historical evidence for unchanged blend behavior. Their reported executable SHA-256 was `7D31AD7878B4CAA9F4EDB7F4FFA1256B09AD791419453FCDD7773944BAF85EEB`; this is not the final repaired binary's hash.

The archived `build/p3.4.5/baseline/Glider.exe` has incomplete build provenance. The scratch build procedure disables blend caching in the candidate source; it does not establish an isolated `614a2ee` build. Earlier exact matches for `vp1_overlay.png` and `multi_viewport.png` therefore describe the supplied binaries only, not an accepted-commit regression comparison. They do not independently validate the transmission repair. The production runner now checks reference scenario failures/settings and deletes old captures before running.

#### 5. Final focused repair

- Transmission materials retain a stable framebuffer-texture wrapper across viewport changes. The wrapper now explicitly borrows its GL handle, so destroying the resource manager cannot delete a viewport-owned texture. Other texture constructors retain owning semantics, and moves preserve ownership.
- The integration self-check verifies wrapper identity, preservation of the old handle on replacement, preservation of the current handle after manager destruction and wrapper move, and deletion by the actual owner.
- `--only lifecycle` exercises real application A/B/A viewport rendering, None/MSAA4, differing culling/winding settings, resize, close/reopen, and model reload with the opaque/transparent/transmissive fixture. Increasing per-viewport render counts reject stale snapshots. It writes two fresh screenshots and an executable/scenario hash manifest.
- Viewport close/reopen is a visibility lifecycle check; direct ownership assertions separately cover resource-manager destruction. This is not a driver memory-leak measurement.
- Unsupported CPU byte sizes, framebuffer VRAM figures, immediate driver reclamation claims, and additional roadmap promises have been removed from this audit.

Final repair validation (2026-10-23):

- Release build: PASS. Existing GL macro redefinition warnings remain.
- `validate_p3_4_5.py (retired, see git history) --only integration`: PASS, 24 assertions including six transmission ownership checks; zero failures and no GL errors in these checks.
- `validate_p3_4_5.py --only lifecycle`: PASS. Both focused runs together took approximately six seconds. Fresh rendering confirmed for A/B/A switching, resize, close/reopen, and reload.
- Viewed `build/p3.4.5/candidate/lifecycle_first.png` and `lifecycle_second.png`: geometry, environment, and bounding overlay visible in both. The differently configured viewport images are smoke checks, not a pixel-equivalence claim.
- Final executable SHA-256: `2A57417CDDAC71DE11DD5679C614256F739F4A6DE689A5A1996ACA3CC795ACCF`. Scenario hash and binary hash recorded in `build/p3.4.5/candidate/lifecycle_manifest.json`.
- `git diff --check`: PASS.

The final review issues are resolved at the stated scope. P3.4.5 and the Phase 3 exit review were accepted in commit `3c86ff7`; no Phase 4 changes were made. Prior accepted checks are reused, with the historical comparison limitations documented above. No full matrix or repeated timing campaign was run: blend-cache code is unchanged by this repair.

#### 6. Validation stopping rule for subsequent batches

Define narrow correctness checks before implementation. Reuse passing evidence unless changed code can invalidate it. Call counts establish work reduction; timing claims require timing evidence, but timing campaigns are not mandatory for each repair. Journal corrections do not trigger renderer reruns. Expand coverage only for a named regression risk or a failed check; stop when the agreed checks pass.
