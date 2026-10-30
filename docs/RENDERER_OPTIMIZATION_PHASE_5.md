# Phase 5 — Skip Passes and Drawables

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Complete ([§12.46](#section-12-46)). P5.1 PASS ([§12.41](#section-12-41)), P5.2 PASS ([§12.42](#section-12-42)), P5.3 PASS ([§12.43](#section-12-43), amended in §6 and [§12.46 §1](#section-12-46)), P5.4 PASS ([§12.44](#section-12-44)), P5.5 PASS ([§12.45](#section-12-45)).

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-7"></a>

### 5.7 Phase 5 — Skip Passes and Drawables (R3)

**Entry:** P4.1 and relevant invalidation tests. Each row is a separate change with a baseline fallback
for comparison; fewer draw calls alone do not prove correct visibility or selection.

- [x] **P5.1 — On-demand full-size picking ([§3.7](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-7), [§4.5](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-5)).** First preserve the existing picking projection
  and framebuffer size, changing only scheduling. Compare clicked IDs and click-to-selection latency
  at fixed coordinates during motion, animation, overlap, resize and multi-viewport use.
- [x] **P5.2 — Shared small picking target ([§3.7](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-7)).** Only after P5.1, introduce the 1×1 sub-frustum and
  remove per-viewport storage. Test edges, thin geometry, alpha masks, DPI and Y-coordinate conversion.
  Match the agreed picking behavior; do not assume a scissor rectangle alone changes the projection.
- [x] **P5.3 — Cached scene redraw ([§3.11](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-11)).** After P4.3/P4.5/P4.6, skip scene passes while UI still
  renders. Check every dirty trigger, attached cameras, animations and TAA convergence/reset.
- [x] **P5.4 — Event-driven idle loop ([§3.11](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-11)).** After P5.3, add event waits separately. Verify wakeup
  and response latency for mouse/keyboard, resize, file load, background completion and animation.
- [x] **P5.5 — Conservative frustum culling ([§3.9](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-9)).** After P4.4, start with static primitives. Keep
  deformed/unsupported bounds on the uncullable path until conservative bounds are proven. Test
  near-plane edges, negative/non-uniform scale, perspective/orthographic views, picking and transmission.

**Exit gate:** unchanged output/selection on deterministic sequences, zero unintended picking passes
without requests, correct redraw wakeups, and measured CPU/GPU/idle benefits. Do not require a fixed
percentage of total draws saved: extra transmission/overlay passes change that denominator.


---

## 12. Implementation Records

<a id="section-12-41"></a>

### 12.41 P5.1 — On-Demand Full-Size Picking (2026-10-23)

**Status:** PASS (uncommitted). R3 (scheduling). Base `1a22976`; pre binary `4E6CBF74…` (HEAD build, kept as
`build/p5.1/pre/Glider.exe`); candidate `72C7456C…`.

#### 1. Audit and plan (before editing)

- Every frame, every viewport clears its full-size picking FBO, appends a picking copy of every pickable drawable
  (`PickingRenderLayer::applyLayer`), and draws it in the same `glRenderer->draw` call as the scene. The
  pre/post commands of the picking passes bind the picking FBO and rebind the scene FBO.
- The buffer is read in one place: the viewport's left-click handler. That handler runs **later in the same
  viewport iteration**, after the scene render and after `ImGui::Image`. The pick therefore always reads a
  render from the same frame, made with that frame's camera, jitter and pose. Picking ids are assigned per
  drawable of the frame's scene, and the shared `pickingLayer`'s id → mesh map is consumed before the next
  viewport rebuilds it.
- **Hypothesis.** Rendering the picking pass only in frames where that handler can fire keeps clicked ids and
  latency unchanged (same-frame read, same FBO size, same projection). It removes `picking.build` and the picking
  draws from every other frame. Expected: picking builds per idle frame 1 → 0; on 1,600 drawables, a large share
  of `draw.submit` (the picking copies roughly double the draw count) and of GPU time.
- **Scheduling test.** It must be a superset of the handler's condition. It uses the same `IsMouseClicked(Left)`
  and the same image rectangle (`ImGui::Image` is drawn at `viewportPos` with `viewportSize`, so its item rect is
  identical). It calls `IsWindowHovered` with `AllowWhenBlockedByActiveItem | AllowWhenBlockedByPopup`, so an
  active item or popup that changes during the frame can only add renders, never remove them. Double-clicks
  are clicks in the same frame.
- **Test cases.** Clicked node and camera after every click, plus screenshots. Compared three ways: the pre
  binary, the same binary with `--always-pick` (the old schedule), and the candidate. Cases: 1,600-drawable
  grid; overlapping opaque/blend/transmissive quads seen from behind, with AA none, MSAA 4× and TAA (jittered
  picking), including 1-pixel steps across the quad edges and a focusing double-click; clicks during playback
  on moving nodes; a wheel zoom on the frame before each click; clicks after a window resize; two live
  side-by-side viewports.
- **Thresholds.** All of the above identical, `pickUnserved` 0, exactly one picking render per viewport click,
  0 picking renders on idle frames. Timing per §6.5: 1,600-drawable CPU frame improves by more than 3% and more
  than the noise, in at least 4/5 pairs. HelloCube CPU frame is the control: no regression beyond 5% and noise.

#### 2. Changes (`source/imgui/main.cpp`)

- `pickingRendered` is computed per viewport before the scene render with the superset test above. It gates the
  picking FBO clear and `pickingLayer.applyLayer`. The FBO is still resized to the viewport every frame (P5.2
  changes storage).
- The click handler reads the buffer only if `pickingRendered`. Otherwise it counts `pickUnserved` and logs,
  which cannot happen while the test is a superset.
- `--always-pick` restores every-frame picking for same-binary A/B runs. UI snapshot `revisions` gain
  `pickingRenders`, `pickingSkips`, `pickReads` and `pickUnserved`. The profile counter `pickingBuilds` is now
  0 on frames without a click.

#### 3. Validation (`python scripts/validate_p5_1.py`; outputs in `build/p5.1/`)

All scripts run at 1280×800 with `--fixed-frame-time 0.05`.

| Check | Result |
|---|---|
| 1,600 drawables, 6×6 click grid (camera distance 30) | PASS: 26 distinct selections, identical across pre, `--always-pick` and candidate. 36 clicks → 36 picking renders and 36 reads. 33 idle frames → 0 renders. |
| Production fixture from behind (yaw 160, pitch 10), 8×5 grid + 17 edge clicks + double-click, AA none / MSAA 4× / TAA | PASS ×3: every quad and the background hit; 59 clicks → 59 renders; the double-click focuses identically; screenshots byte-identical |
| Hierarchy fixture: 55 clicks during playback (hits `GrandChild` and `ScaledChild` while moving), 12 clicks each after a wheel zoom, 12 after a resize to 1100×700 | PASS: 79 clicks → 79 renders; 3 screenshots byte-identical |
| Two live viewports (viewport 2 double-sided), 27 clicks across both | PASS: 4 distinct results; 46 idle frames across both viewports → 0 renders; screenshot byte-identical |
| Totals | 355/355 checks. `--always-pick` never skips. `pickUnserved` is 0 everywhere. |
| Timing, 1,600 drawables, 5 alternating pairs, 120 warmup + 300 frames, vsync off | CPU frame 13.06–15.97 → 5.05–5.30 ms (median paired −62.6%, 5/5, baseline noise 20.6%). p95 19.0–25.0 → 6.5–7.4 ms. `draw.submit` 9.26–11.34 → 3.12–3.26 ms. GPU pass time 7.78–9.11 → 1.66–1.81 ms (−78.3%, 5/5, noise 15.8%). |
| Control, HelloCube, same protocol | CPU frame 0.20–0.53 → 0.18–0.55 ms. The median paired change is −11.1% (0.03–0.05 ms) inside a baseline noise of 87%: within noise, no regression. |
| Earlier gates on the candidate | `validate_p4_exit_gate.py` 93/93 (all producers, run normally and with every Phase 4 shortcut off). `--revision-selfcheck` 85/85. |
| Viewed | `candidate/split.png` (two viewports, picked quad's box and gizmo), `candidate/hi_resized.png` |

Test-harness note: clicks closer than ImGui's double-click distance (6 px) and issued a few frames apart become
wall-clock double-clicks that focus the camera, which made the first TAA edge run diverge **between two
`--always-pick` runs**. Edge clicks are now spaced 400 ms apart (`waitms`). This was not a renderer
difference. A script slip, found while writing P5.2, fed the hierarchy run into the split run's diversity
assertion. It is fixed. The split run's own report has all four results (three quads and background), and its
equivalence checks were always made on the split run.

#### 4. Limits

- The picking pass still draws every drawable at full viewport size on a click frame. Click frames cost what
  every frame cost before. P5.2 shrinks that.
- With `--always-pick`, per-viewport FBO memory is unchanged by design. P5.1 only changes scheduling.
- Baseline noise on this machine (16–21%, 87% for the sub-millisecond control) exceeds the 5% guideline. The
  effect is about 3× the noise and holds in every pair.
- Hardware: RTX 5080, OpenGL 4.1 NVIDIA 616.92.

#### 5. Decision

PASS. Rollback boundary: the `pickingRendered` gate (clear, `applyLayer`, read), the `--always-pick` flag,
the four counters and `scripts/validate_p5_1.py`.

<a id="section-12-42"></a>

### 12.42 P5.2 — Shared 1×1 Picking Target (2026-10-23)

**Status:** PASS (uncommitted). R3. Built on P5.1. Pre binary: the P5.1 build `72C7456C…`
(`build/p5.2/pre/Glider.exe`). Candidate: `DB9E187C…`.

#### 1. Audit and plan (before editing)

- After P5.1, a click frame still clears and draws a picking buffer the size of the viewport (RGBA8 + D24S8, 8 bytes
  per pixel), and every open viewport owns one: 4.09 MB for the default 734×696 viewport, 16.6 MB at 1080p.
  The picking copies are drawn inside the scene draw and share the camera uniforms with it. So the projection
  can't change for picking alone without a separate draw.
- `picking.frag` reads neither `gl_FragCoord` nor the viewport-resolution uniform. Only clip-space positions,
  UVs (mask alpha) and depth affect the id.
- **Mechanism.** On a click frame, after the scene draw, draw the picking copies alone
  (`PickingRenderLayer::pickingOnly`; ids are still assigned over the whole scene in the same order). They go
  into one shared 1×1 target through `W · P`:
  - `P` is the projection the scene was just drawn with, jittered under TAA, as the old picking copies used.
  - `W` scales clip x/y by `vpW`/`vpH` and translates them by the exact integers `vpW − 2px − 1` and
    `vpH − 2py − 1`. The clicked pixel's centre lands on the target's only sample. One target pixel spans one
    viewport pixel, so UV derivatives and mip selection for alpha masks are unchanged. Depth is untouched.
  - The camera projection is restored bit-for-bit afterwards. The pixel `(px, py)` is computed before the render
    with the click handler's own formula. The handler counts any disagreement as unserved.
  - A scissor rectangle alone was rejected: it doesn't change the projection, so vertex and fill cost and the
    full-size storage would remain.
- **Predeclared gates.** Every clicked id and camera is identical to the P5.1 binary and to the same binary with
  `--full-size-picking`. Screenshots are byte-identical. `pickUnserved` is 0. Per-viewport picking storage is 0
  bytes. Click-frame CPU shows no regression beyond 5% and noise. Edge, thin-geometry, mask, Y-conversion and
  DPI cases must match exactly, with no tolerance; any mismatch is investigated.

#### 2. Changes

- `main.cpp`: a shared `pickingTarget` (1×1, created after the GL context and released with the viewports). The
  picking draw runs after the scene draw on click frames (GPU label `picking`, stage `picking.draw`). The read is
  at (0, 0). `--full-size-picking` keeps the P5.1 path (per-viewport buffer, picking copies drawn with the scene,
  read at `(px, py)`), and `--always-pick` implies it. Without the flag the per-viewport buffer is never
  allocated, so the scenario report's `pickingWidth`/`pickingHeight` read 0.
- `PickingRenderLayer`: a public `pickingOnly` flag. When set, the returned scene holds only the picking drawables.

#### 3. Validation (`python scripts/validate_p5_2.py` (retired, see git history); outputs in `build/p5.2/`)

All runs at 1280×800 with `--fixed-frame-time 0.05`. In the default layout the viewport image is 734×696 at
(284, 70). The outer 4-pixel columns belong to the dock splitters' hover zone, so they cannot be clicked
through the UI. They also could not be clicked before this change.

| Check | Result |
|---|---|
| 1,600 drawables (camera distance 14): 6×6 grid; a 12-click row each on the **top (py = 695) and bottom (py = 0)** image rows; 11 clicks each down columns x = 288 and 1013 | PASS: 46 distinct selections. The top and bottom rows pick different node sets (a Y flip would swap them). 80 clicks → 80 renders. Idle frames → 0. |
| Production fixture from behind, AA none / MSAA 4× / TAA: 8×5 grid, **1-px and 2-px steps** across both vertical quad edges (40 clicks) and **1-px vertical steps** across two horizontal edges (28 clicks), and a focusing double-click | PASS ×3: 110 clicks each; every quad and the background hit; ids and cameras identical; screenshots byte-identical |
| Orthographic projection, same view, 12×7 grid | PASS: all three quads and the background, 84 clicks |
| **Thin geometry**: Wireframe render mode (1-pixel lines), 44×9 grid at 9-px spacing (396 clicks) | PASS: lines of all three quads picked, identical |
| **Textured alpha masks**: AlphaBlendModeTest, 51×5 grid at 14-px spacing (255 clicks), rows on both sides of the cutoff lines | PASS: `TestCutoff25`, `TestCutoffDefault`, `TestBlend`, `TestOpaque` and the `Bed` behind discarded fragments, identical |
| Skinned Fox during playback, 16×7 grid | PASS: 112 clicks, identical |
| Hierarchy fixture: playback on moving nodes, wheel zoom, resize | PASS: 79 clicks, identical, 3 screenshots byte-identical |
| Two live viewports (viewport 2 double-sided), 17×4 grid across both | PASS: 68 clicks, identical. Idle frames → 0 renders. |
| Totals | 1,451/1,451 equivalence checks |
| Memory (scenario report) | Per-viewport picking storage 734×696×8 = **4,086,912 bytes → 0**. One shared 1×1 target (8 bytes) serves all viewports. |
| Click-frame cost, 1,600 drawables, 30 clicks per run, 5 alternating pairs | GPU picking passes per click 3.92–12.99 → 0.60–1.05 ms (median paired −94.6%, 5/5). Click-frame CPU 14.3–34.0 → 24.0–33.0 ms (+8.9% median, 4/5, noise 72%): within noise, no regression. Idle frames are unchanged from P5.1. |
| Earlier gates on the candidate | `validate_p4_exit_gate.py` 93/93 (all producers, normal and all-Phase-4-shortcuts-off runs). `--revision-selfcheck` 85/85. |
| Viewed | `candidate/alpha.png` (mask bars cut at their thresholds), `candidate/split.png` |

#### 4. Limits and coverage gaps

- **DPI > 1: NOT RUN.** This display runs at a content scale of 1. The pixel formula is unchanged from the
  full-size path (ImGui coordinates minus the image origin). The 1×1 path checks it against the handler's own
  computation, but no scaled-display run was made.
- Vertex work is unchanged: each click still transforms every pickable drawable. A CPU ray/bounds pre-filter
  (§3.7 options 3 and 4) is not part of this task.
- Click-frame CPU timing is noisy, from a single readback per frame and UI changes on selection. The claim
  rests on GPU time and memory, not on CPU time.
- Hardware: RTX 5080, OpenGL 4.1 NVIDIA 616.92.

#### 5. Decision

PASS. Rollback boundary: the shared target, the click-frame picking draw and the read at (0, 0), the
`--full-size-picking` flag, `PickingRenderLayer::pickingOnly`, and `scripts/validate_p5_2.py`. Reverting it
returns to P5.1.

<a id="section-12-43"></a>

### 12.43 P5.3 — Cached Scene Redraw (2026-10-23)

**Status:** PASS (uncommitted). R3. Built on P4.3/P4.5/P4.6 and P5.2. Pre binary: the P5.2 build `DB9E187C…`
(`build/p5.3/pre/Glider.exe`). Candidate: `3457DC36…`.

#### 1. Audit and plan (before editing)

- Each frame, every visible viewport clears its scene target, then builds and draws the scene, grid and bounding
  boxes, then runs AA. ImGui then shows the result. `frameSceneDirty` (the revisions consumed this frame) feeds
  only TAA.
- Inputs to the scene image that are **not** covered by revisions:
  - the camera: orbit, pan, zoom, the orthographic toggle and compass presets change matrices without a mark;
  - the toolbar grid and bounding-box toggles, and direct `ViewportSettingsState` assignments;
  - viewport size and samples, and the AA mode;
  - the TAA jitter and history;
  - the selection OBB, which is drawn into the scene target.
- Revisions are consumed before the viewports render. Marks made later in a frame take effect the next frame, as
  before.
- **Mechanism.** Each viewport keeps a `ViewportRenderKey` holding:
  - the layer and scene pointers;
  - **every** `SceneRevisions` counter (so a viewport hidden while something changed still notices);
  - the view and unjittered projection matrices, eye and zoom;
  - size, samples and AA mode, plus the AA target generation;
  - camera attachment;
  - the **whole** `ViewportSettingsState`, through a defaulted `operator==`.

  The scene work runs only when one of these holds:
  - the key changed;
  - a click needs the picking pass;
  - the cached image is invalid (AA targets released when a viewport closes, or first use);
  - TAA is still accumulating.

  Otherwise the clear, scene, picking, grid, boxes and AA are skipped, and the last display texture is shown
  again. ImGui, the gizmo and the compass still render every frame.
- **TAA.** It blends 10% per frame over an 8-phase jitter and never reaches a fixed point. It keeps rendering
  until 64 consecutive unchanged renders (8 jitter cycles), then holds. Any `resetHistory()` forces a render and
  restarts the count.
- **Predeclared gates.**
  - Every screenshot after every change is byte-identical to the P5.2 binary and to `--disable-redraw-skip`.
  - Every change redraws.
  - Idle frames redraw nothing.
  - The Phase 4 exit gate passes against a reference with every shortcut off, `--disable-redraw-skip` included.
  - TAA: identical before the hold. The held image must be a frame the every-frame reference actually displayed.
    Its difference from the reference's later frames is reported, not gated, since the reference never stops
    changing.
  - Timing per §6.5 on the idle 1,600-drawable fixture. Control: the same grid with one animated leaf, playing,
    which redraws every frame.

#### 2. Changes

- `ViewportInstance.h`: `ViewportRenderKey` plus per-viewport cache state (`lastRenderKey`, `cachedDisplayTexture`,
  `hasCachedImage`, `taaStableRenders` and render/skip counters).
- `ViewportSettingsPanel.h`: a defaulted `operator==` on `ViewportSettingsState`.
- `main.cpp`:
  - The scene-target clear moves after the camera set-up. Camera set-up still runs every frame and has no GL
    side effects.
  - The key is computed and compared, and the clear, TAA projection, scene, grid, boxes and AA are guarded by
    `redrawScene`.
  - The cache is invalidated where the AA targets are released.
  - `--disable-redraw-skip` restores the old schedule. Snapshot `revisions` gain `sceneRenders`/`sceneSkips`.
- `validate_p4_exit_gate.py`: the reference now also passes `--always-pick --disable-redraw-skip`.

#### 3. Validation (`python scripts/validate_p5_3.py` (retired, see git history); outputs in `build/p5.3/`)

| Check | Result |
|---|---|
| Production fixture, 19 steps: toolbar grid toggle (twice), bounding-box toggle, orthographic and back, wheel zoom, compass isometric, viewport pick and deselect (OBB), **direct assignments that mark no revision** (`vpsetting` grid, bbox, wireframe overlay on/off, invert winding), AA FXAA → MSAA 4× → none, window resize and back | PASS: every screenshot byte-identical to pre and to `--disable-redraw-skip`. Every step redrew (1–2 renders). Visible steps changed the image. Two idle windows: **0 redraws** where the reference drew 33. |
| Hierarchy fixture: play, pause, seek, **attach to the animated glTF camera**, play attached, pause, reload | PASS: byte-identical. Playback redrew every tick (29 renders per step). Idle after the reload: 0 redraws. |
| Skinned Fox: play, pause | PASS: byte-identical. Paused idle: 0 redraws. |
| Two viewports: viewport 1 changed (grid off, overlay on) **while hidden behind a tab**, then shown; split; viewport 2 changed | PASS: viewport 1 shows its change on reveal (keyed on revisions and settings, not on this frame's changes). Byte-identical. After viewport 2's change, only viewport 2 redrew. Idle: 0 redraws in both. |
| Totals | 136/136 equivalence checks. `--disable-redraw-skip` never skips. |
| TAA (production fixture) | PASS. Frame 40 is byte-identical to `--disable-redraw-skip`. The held image is byte-identical to a frame the every-frame reference displayed (`ref_012`, the 13th frame of its shot window). It stays held with 0 redraws. A change (grid off) resumes accumulation for 65 renders, then holds; a compass history reset gives 66. Held vs the reference at the same frame: max channel difference 16/255, mean 0.016, 0.99% of channels. The reference's own consecutive frames: max 16, mean 0.014. |
| Timing, idle 1,600 drawables, 9 alternating pairs | CPU frame 5.16–8.79 → 0.14–0.43 ms (median paired −93.8%, 9/9). GPU 1.79–2.98 ms → 0 (no scene pass). A 5-pair run gave −91.9% (noise 6.9%). |
| Control: leaf-animated grid playing (redraws every frame), 9 pairs | CPU frame +1.2% (5/9), GPU +4.4% (5/9): no regression. A first 5-pair run read −9% (1/5) inside 21.7% noise; the 9-pair rerun settles it. |
| Earlier gates | `validate_p4_exit_gate.py` 93/93 against the stricter reference. `--revision-selfcheck` 85/85. |
| Viewed | `candidate/vp1_shown.png`, `candidate/split.png` |

#### 4. Limits

- Under TAA, a still view holds after 64 renders instead of accumulating forever. Its output after that point is
  one of the reference's own frames, not the reference's frame at the same time. This was declared before the
  work. `--disable-redraw-skip` restores continuous accumulation.
- The key uses `sceneRevisions` counters. A future renderer input that bypasses both the revisions and the
  settings struct would display stale. The tests cover the known non-revision inputs, and the exit gate covers
  every producer.
- Click frames still redraw the scene, because the picking pass is part of the scene work.
- ImGui still renders every frame. Waiting for events is P5.4.

#### 5. Decision

PASS. Rollback boundary: `ViewportRenderKey` and the cache fields (with the layer generations from §6), the
`operator==`, the `redrawScene` guards and the moved clear, `--disable-redraw-skip`, the counters, `scripts/validate_p5_3.py` and the reference flags in
`validate_p4_exit_gate.py`.

#### 6. Amendment: Stop, End and Rewind (found while preparing P5.5)

The P4.4 audit ([§12.35](RENDERER_OPTIMIZATION_PHASE_4.md#section-12-35)) records that the status bar's **Stop, End
and Rewind** buttons call `applyAnimationTime` directly and mark no revision. The render key above missed that
input. While paused, those buttons changed the pose but the viewport kept showing the old image.

- **Test first.** The hierarchy scenario gained Stop, End and Rewind steps. On the P5.3 binary as first recorded
  (`3457DC36…`), all three fail: 0 redraws, and the screenshots differ from pre and from
  `--disable-redraw-skip`.
- **Fix.** `ViewportRenderKey` also holds `GltfRenderLayer::getSceneGeneration()` and `getPoseGeneration()`,
  the layer counters P4.4's bounds caches already key on. The pose generation advances on every
  `applyAnimationTime`, marked or not.
- **Result** (binary `A7B61B9C…`, which also contains P5.4): `validate_p5_3.py` equivalence 148/148, and Stop, End and
  Rewind each redraw once, visibly and byte-identically. TAA checks unchanged. Exit gate 93/93.

The §12.43 decision stands, with the generations added to the rollback boundary.

<a id="section-12-44"></a>

### 12.44 P5.4 — Event-Driven Idle Loop (2026-10-23)

**Status:** PASS (uncommitted). R3. Built on P5.3. Pre binary: `build/p5.4/pre/Glider.exe` (P5.3 as first
recorded). Candidate `A7B61B9C…` (also contains the P5.3 amendment). Same-binary reference: `--disable-idle-wait`.

#### 1. Audit and plan (before editing)

- The loop calls `glfwPollEvents()` every frame and is paced only by vsync. After P5.3 an idle frame skips the
  scene but still builds and renders the whole ImGui UI, about 54 times a second.
- No background threads exist. Only window events, playback, turntable, pending revisions and automation need
  frames.
- **Mechanism.** At the top of each frame, wait instead of polling when all of these hold:
  - no automation is pending (UI script, benchmark, frame dump, viewport capture, profile or scenario report);
  - the previous **3** frames were quiet.

  A frame is quiet when no input event was processed (`InputEventsTrail` is empty), no viewport redrew, no
  revision is pending, playback and turntable are off, no text field is active, and no mouse button is held. The
  wait times out after 0.25 s.
- **Harness gap.** The UI harness injects input after polling, so it can't exercise a wait.
  `scripts/validate_p5_4.py` drives a real window from outside the process instead:
  - It posts Win32 mouse, key and click messages, resizes with `SetWindowPos`, and clicks play and stop.
  - It reads frame-start and wake timestamps from the new `--idle-stats` output.
  - It measures process CPU time with `psutil`.
- **Predeclared gates.** Each is checked over runs that alternate with `--disable-idle-wait`:
  - Idle frames in a 30 s window: at most 30/0.25 + 10 = 130.
  - Idle process CPU: improves by more than 3% and more than the noise.
  - Playback frame rate: at least 90% of the polling median.
  - Idle again after Stop.
  - Wake latency (event post to next frame start): median no worse than polling + 5 ms, maximum 50 ms.
  - Exit on `WM_CLOSE`.
  - Equivalence: automation always polls, so the P5.1–P5.3 suites and the exit gate must still pass.

#### 2. Changes

- `main.cpp`: the wait condition, the quiet-frame count, `--disable-idle-wait`, and `--idle-stats <path>`.
  At exit, `--idle-stats` writes frame-start and wake times (wall clock, µs resolution) plus wait, timeout,
  input-wake and sent-only-wake counts.
- `IdleWait.{h,cpp}`: a new file, so that `windows.h` stays out of `main.cpp`. On Windows it waits with
  `MsgWaitForMultipleObjectsEx(QS_ALLINPUT, MWMO_INPUTAVAILABLE)`, then calls `glfwPollEvents`. Elsewhere it uses
  `glfwWaitEventsTimeout`.

#### 3. Findings during validation (each fixed or explained before the final run)

1. **Cross-process resizes were blocked.** With `glfwWaitEventsTimeout`, a `SetWindowPos` from another process
   took 222 ms to reach a frame. GLFW's Win32 wait uses `QS_ALLEVENTS`, which leaves out `QS_SENDMESSAGE`, so
   *sent* messages (shell snapping, other processes) block their sender until the timeout. Fix: the
   `QS_ALLINPUT` wait above.
2. **Idle frames were over budget** (150–153 per 30 s against 130). Wake-reason logging showed roughly 5 wakes a
   second for messages *sent* by another thread or process (`QS_SENDMESSAGE`), each rendering a frame. Fix: a
   sent-only wake that brings no ImGui input and no framebuffer-size change dispatches the messages and keeps
   waiting until the same deadline.
3. **Latency was inflated by the measurement.** The log truncated timestamps to whole milliseconds, so a frame
   starting 0.1 ms after the post could be stamped before it and skipped. The wake timestamps showed
   −0.4 to +0.1 ms. Fix: µs timestamps. The gate was not changed.
4. **Test bug.** Restoring the window with an outer size shrank the client area, so the play click missed. The
   original outer rectangle is now restored.
5. **Noisy attempt.** One 5-run attempt had real outside input during an idle window (2 extra input wakes, 52
   active frames), a 218 ms playback stall, polling-run stalls up to 893 ms, and 142% CPU noise. Per §6.5 it was
   rerun unchanged. Its log is kept as `build/p5.4/run_log_attempt2.txt`.
6. **Pre-existing shutdown hang.** Some runs did not exit within 20 s of `WM_CLOSE`, in both modes. The window
   was gone; the process stayed alive. A focused repro (`build/p5.4/close_hang.py`: load, cross-process resize,
   play, stop, `WM_CLOSE`) hangs **4/8 on the pre-Phase-5 HEAD binary `1a22976`** and 1/8 on the candidate. It
   predates Phase 5 and is out of scope here. The script now counts and reports it instead of gating on it.

#### 4. Validation (final run, `python scripts/validate_p5_4.py --runs 5`; outputs in `build/p5.4/`)

| Check | Polling (`--disable-idle-wait`) | Candidate | Result |
|---|---|---|---|
| Frames in a 30 s idle window | 1,590–1,612 | 107–108 | PASS (≤ 130) |
| Idle process CPU per 30 s | 0.44–0.67 s | 0.08–0.14 s | PASS (median paired −75.0%; noise 44%) |
| Frames in 5 s after Stop | 260–273 | 17–18 | PASS |
| Playback frame rate | 51.7–55.0 fps | 52.7–53.7 fps | PASS (no waits during playback) |
| Wake latency, median (max): mouse move | 13.8 ms | 0.1 (0.1) ms | PASS |
| key | 13.0 ms | 0.1 (0.2) ms | PASS |
| viewport click | 11.9 ms | 0.4 (0.4) ms | PASS |
| cross-process resize / resize back | 13.6 / 11.1 ms | 2.7 (3.5) / 1.3 (1.8) ms | PASS |
| play / stop (status bar) | 13.0 / 6.3 ms | 0.4 (0.4) / 5.2 (15.9) ms | PASS |
| Cross-process `SetWindowPos` call duration | 33.8–150.3 ms | 2.2–53.4 ms | PASS |
| Shutdown hangs (reported, not gated) | 0/5 | 2/5 | pre-existing (finding 6) |

In this run, three candidate runs completed and two hit the pre-existing shutdown hang; all five polling runs
completed. Equivalence and earlier gates on the candidate:
- `validate_p5_3.py` equivalence 148/148 and TAA PASS, with UI scripts polling as before.
- `validate_p4_exit_gate.py` 93/93.
- `--revision-selfcheck` 85/85.

#### 5. Limits

- Windows only for the sent-message handling. Other platforms use `glfwWaitEventsTimeout` and are untested here.
- The 0.25 s timeout keeps 4 frames a second while idle. ImGui still redraws on each timeout.
- A focused text field (`WantTextInput`) keeps the loop polling, for caret blinking.
- Wake latency is measured to frame start, not to display. With vsync on, the frame still presents at the next
  vertical blank.

#### 6. Decision

PASS. Rollback boundary: the idle-wait condition and quiet-frame count in `main.cpp`, `IdleWait.{h,cpp}` (and
its `CMakeLists.txt` entry), `--disable-idle-wait`, `--idle-stats`, and `scripts/validate_p5_4.py`. The shutdown
hang is a separate follow-up.

<a id="section-12-45"></a>

### 12.45 P5.5 — Conservative Frustum Culling (2026-10-23)

**Status:** PASS (uncommitted). R3 (draw membership). Built on P4.4 and P5.4. Pre binary: P5.4 `A7B61B9C…`
(`build/p5.5/pre/Glider.exe`). Candidate `8853A826…`. Same-binary reference: `--disable-culling`.

#### 1. Audit and plan (before editing)

- `createScene` adds every visible primitive of the scene, once per viewport redraw, and once more for the
  wireframe overlay. Nothing is culled.
- `primitive.vert` computes `gl_Position = u_ViewProjectionMatrix · u_ModelMatrix · position`. Only
  `USE_MORPHING` (target offsets) and `USE_SKINNING` (joint matrices) move a vertex away from its accessor
  position.
- `Mesh::getTransform()` holds the exact matrix behind `u_ModelMatrix`. The active camera's `P · V` is the
  view-projection uniform, already jittered under TAA, because the TAA projection is set before `createScene`.
- There is no `GL_DEPTH_CLAMP` or clip control, so the z clip planes apply.
- **Mechanism.** In `createScene`, when `GltfRenderLayer::frustumCulling` is set, a primitive is left out when
  its local POSITION min/max box, transformed by `P · V · M`, has all 8 corners strictly outside the same clip
  half-space (−w ≤ x, y, z ≤ w), with a relative margin of 10⁻⁴·|w|.
  - Each half-space is linear in homogeneous coordinates, so the test is exact for the whole box: corners behind
    the camera, perspective, orthographic and infinite-far projections, and negative or non-uniform scale.
  - **Uncullable** (always kept): morph targets or `USE_MORPHING`, `USE_SKINNING`, a non-`primitive.vert`
    shader, a non-float POSITION (quantized min/max are not in model units), and missing min/max. The skybox
    and transmission blit are added outside the loop and are never tested.
  - Transmission duplicates are built from the culled list. The transmission target is rendered from the same
    camera, so a culled primitive can't appear in it.
  - Picking ids are assigned over the culled list. A culled primitive can't cover the clicked pixel.
- `main.cpp` turns culling on only around the viewport's scene and wireframe-overlay `createScene` calls. The
  self-check callers keep the full list.
- **Predeclared gates.** Every screenshot, selection and camera is byte-identical to the P5.4 binary and to
  `--disable-culling`. Primitives are actually culled where the view leaves geometry out, and never for skinned
  or morphed meshes. Timing per §6.5 per redraw (`--disable-redraw-skip` in both builds), with the whole grid
  in view as the control.

#### 2. Changes

- `GltfRenderLayer`: `frustumCulling`, `getStaticPositionBounds`, `boxOutsideClip`, and `culled`/`uncullable`
  scene stats.
- `main.cpp`: `--disable-culling`. Snapshot `revisions` gain cumulative `culled`/`uncullable`.
- `scripts/generate_culling_fixture.py` writes a wide scene: a 12×12 field of cubes over 60×60 units, scaled
  `[-1,1,1]`, `[1,-2,0.5]`, `[3,0.3,1]`, `[-0.5,-0.5,-0.5]` and `[0.4,2.5,0.4]`, and rotated. Four parent
  groups are mirrored, non-uniformly scaled and rotated, and an 80-unit bar crosses every frustum plane.
- **Grid state-leak fix (`GridRenderer.cpp`), found by this task.** The grid draw set depth and blend state but
  inherited face culling and winding from the last scene command. When culling changed that last command to a
  mirrored cube, the orthographic grid was back-face culled. The `wide_y20_p5` orthographic screenshot showed
  no grid; pre and `--disable-culling` showed it.

  This was pre-existing: any scene whose last-drawn primitive is mirrored hid the grid in orthographic, where no
  skybox is drawn last. The grid is a two-sided plane, so it now disables face culling. With the fix, that
  screenshot is byte-identical to pre again, and the whole suite matches pre.

#### 3. Validation (`python scripts/validate_p5_5.py` (retired, see git history); outputs in `build/p5.5/`)

| Check | Result |
|---|---|
| Culling fixture from inside the field, 6 directions (yaw/pitch 20/5, 110/15, 200/−10, 290/40, 45/80, 160/−60). Each: start, **orthographic**, zoom out, zoom in past the **near plane**, compass isometric, 30 picks, resize. | PASS: byte-identical to pre and `--disable-culling`; 7,512–7,728 primitives culled per run; all picks identical |
| Culling fixture, **TAA** (jittered projection), then wireframe overlay | PASS: 16,622 culled |
| 1,600-drawable grid from close up (distance 4), 36 picks | PASS: 115,724 culled; picks identical |
| **Transmission** fixture with the camera target moved so quads leave the view, MSAA 4×, a pick, wireframe overlay; 4 offsets | PASS: 3–4 culled per run, identical |
| Cull-winding fixture (single-sided, back-facing, double-sided, **mirrored** planes) at both view edges, with force double-sided | PASS |
| Hierarchy fixture (animated, non-uniformly scaled parent) during playback | PASS: 74 culled |
| **Skinned** Fox and **morph** cube during playback | PASS: 28 primitives kept as uncullable and none culled; identical |
| Totals | 330/330 checks |
| Timing per redraw, 1,600 drawables, close up (1,601 → 37 drawables) | CPU frame 5.12–6.16 → 0.52–0.70 ms (median paired −90.1%, 5/5, noise 19.3%). `draw.submit` −95.8%. GPU 1.67–2.26 → 0.04 ms (−97.6%). |
| Distance 30 (1,601 → 1,225) | CPU −22.0% (5/5), GPU −28.4% |
| Control: distance 70, nothing culled | CPU 5.09–6.13 → 5.34–6.18 ms (median paired −4.7%, 1/5, noise 20.1%). Within the gate (under 5% and the noise). It is likely the real cost of testing 1,600 boxes, about 0.25 ms (see Limits). GPU +0.2%. |
| Viewed | `candidate/wide_y20_p5_ortho.png` (viewed with the grid missing before the fix; the file now holds the fixed frame); `candidate/wide_y160_p-60_in.png` (looking up through the near plane) |

#### 4. Limits

- Skinned and morphed primitives are never culled. Conservative bounds for them (joint-space or target-extended
  boxes) are future work.
- When nothing is culled, the per-primitive test costs about 0.25 ms per redraw at 1,600 primitives: a
  POSITION attribute scan with a string compare, plus 8 corner transforms. Caching the local box per mesh
  (keyed on the layer's scene generation) would remove the scan. P5.3 means idle frames don't redraw at all.
- Only view-frustum culling is done: no occlusion, small-feature or distance culling, and no instancing.
- The grid fix changes output for scenes that hit the old leak. No test scene hit it without culling.

#### 5. Decision

PASS. Rollback boundaries:
- P5.5: `frustumCulling` and its helpers, the stats and counters, the `main.cpp` toggles, `--disable-culling`,
  `scripts/validate_p5_5.py` and `generate_culling_fixture.py`.
- Grid fix: the `glDisable(GL_CULL_FACE)` in `GridRenderer::draw`, independent of P5.5.

<a id="section-12-46"></a>

### 12.46 Phase 5 Exit-Gate Review (2026-10-23)

**Status:** PASS on the final binary `12C5AC95…` (all of P5.1–P5.5, the P5.3 amendments and the grid fix). The
exception is the idle process-CPU *timing* claim, which is reported with its noise below.

#### 1. A late finding: TAA jitter phase after a hold (P5.3 amendment 2)

The cumulative run of `validate_p5_2.py` on the final binary failed one check out of 1,451: `fixture_taa/edge29`,
a 1-pixel edge pick under TAA.
- The same binary with `--full-size-picking` agreed with the candidate; only the P5.1 binary differed. So the
  cause was P5.3, not P5.2.
- `AntiAliasing` advances its 8-phase jitter index only on TAA renders. While P5.3 held a converged TAA view,
  skipped frames stopped advancing it. The next click therefore rendered, and picked, with a different sub-pixel
  jitter than an every-frame schedule would, and an exact edge pixel flipped.

**Fix:** `AntiAliasing::skipFrame()`, called on every skipped TAA frame, keeps the jitter phase equal to the
every-frame schedule. Afterwards, `validate_p5_2.py` passes 1,451/1,451, and the P5.3 TAA checks are unchanged
(the held image is the reference's `ref_012`; it stays held).

A second effect: after a hold, the TAA re-accumulation now uses the every-frame phases. Its images differ from
the P5.4 binary, which lacked the fix, in exactly one P5.5 shot (`wide_taa_taa_overlay.png`, taken after a hold
and a change). That shot is byte-identical to the same binary with `--disable-culling`, and the pre-hold shot is
identical across all three. `validate_p5_5.py` now compares post-hold TAA shots with the same-binary reference
only. Rerun: `wide_taa` PASS.

#### 2. Gate criteria

| Criterion (Phase 5 exit gate) | Evidence on the final binary | Result |
|---|---|---|
| **Unchanged output and selection on deterministic sequences** | `validate_p5_1.py` 355/355: every screenshot and selection is byte-identical to the **pre-Phase-5 HEAD binary** (`1a22976`) across the 1,600-drawable grid, overlapping quads under none/MSAA/TAA, playback, zoom, resize and two viewports. `validate_p5_2.py` 1,451/1,451 (edges, rows, columns, wireframe, masks, ortho, Fox). `validate_p5_3.py` 148/148 plus TAA 8/8. `validate_p5_5.py` 330/330 (one post-hold TAA shot against the same-binary reference, §1). `validate_p4_exit_gate.py` 93/93 against every shortcut off (`--force-full-sync --disable-command-caches --disable-transform-reuse --always-pick --disable-redraw-skip`). `--revision-selfcheck` 85/85. | PASS |
| **Zero unintended picking passes without requests** | Idle windows render 0 picking passes (P5.1/P5.2 suites), and every viewport click renders exactly one (`pickingRenders == pickReads ==` clicks, `pickUnserved` 0). No per-viewport picking storage exists (0 bytes against 4,086,912 at 734×696). | PASS |
| **Correct redraw wakeups** | Every change redraws, including those that mark no revision: toolbar toggles, camera moves, direct settings assignments, AA switches, resize, Stop/End/Rewind, and a viewport changed while hidden. Idle viewports redraw 0 times. Input wakes the loop in 0.1–2.4 ms (mouse, key, click, cross-process resize, play, stop); the loop doesn't wait during playback and is idle again after Stop. | PASS |
| **Measured CPU/GPU/idle benefits** | See §3. | PASS (one timing claim noisy, §3) |

The gate does not require a fixed share of draws saved, and none is claimed.

#### 3. Measured benefits (RTX 5080, OpenGL 4.1 NVIDIA 616.92, 1280×800)

| Task | Workload | Before → after |
|---|---|---|
| P5.1 | 1,600 drawables, steady frame | CPU 13.1–16.0 → 5.0–5.3 ms (−62.6%); GPU 7.8–9.1 → 1.7–1.8 ms (−78.3%) |
| P5.2 | Click frame | GPU picking 3.9–13.0 → 0.6–1.0 ms (−94.6%); picking memory per viewport 4.09 MB → 0 (8 bytes shared) |
| P5.3 | 1,600 drawables, idle | CPU 5.2–8.8 → 0.14–0.43 ms (−93.8%, 9/9); GPU 1.8–3.0 → 0 ms. Playback control +1.2% (no regression). |
| P5.4 | Idle 30 s, vsync on | Frames 1,590–1,627 → 107–110. Process CPU: −75.0% in the dedicated run (noise 44%); in the exit-gate reruns, 5 runs gave −74.2% (4/5 pairs better) with one implausible 0.11 s polling sample (noise 86%, gate not met), and 9 runs gave −83.2% (noise 80%, gate met). |
| P5.5 | 1,600 drawables, close up, every frame redrawn | CPU 5.1–6.2 → 0.52–0.70 ms (−90.1%); GPU −97.6%. Whole grid in view: −4.7% (≈0.25 ms test cost, within the gate). |

#### 4. Open items and follow-ups (none blocks the gate)

- **P5.4 "stop" latency maximum.** In the 9-run exit-gate rerun one sample reached 60.9 ms, against the 50 ms
  cap; the median was 14.0 against 12.2 ms polling. Stop is clicked during playback, when the loop never
  waits; that run's wake log shows no wait between play and stop. The sample is a single 90 ms frame
  during playback, and polling runs show 37 ms excursions too. Wake latency from idle (mouse, key, click,
  resize, play) stayed at 0.1–2.6 ms medians in every run.
- **Pre-existing shutdown hang.** After a cross-process resize, `WM_CLOSE` leaves the process alive with its
  window destroyed. Rate: 4/8 on HEAD `1a22976`; 0–2 of 5 in the P5.4 runs. It predates Phase 5. Repro:
  `build/p5.4/close_hang.py`.
- **P5.5 test cost** of about 0.25 ms per redraw when nothing is culled. Caching local bounds per mesh would
  remove the attribute scan.
- **DPI > 1** picking (P5.2) and non-Windows idle waits (P5.4) are NOT RUN on this machine.
- MSBuild reports a `C4005` glad macro-redefinition warning while compiling `CameraActor.cpp`, which Phase 5 did
  not change. It appears whenever a header change forces that file to recompile.

Phase 5 is complete. Nothing is committed: the user confirms commits.
