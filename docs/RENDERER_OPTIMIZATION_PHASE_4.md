# Phase 4 — Invalidation and Persistent Scene Caches

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** In progress. P4.1 PASS ([§12.32](#section-12-32)) and P4.2 PASS ([§12.33](#section-12-33)), committed as `ebd4384`; P4.3 PASS ([§12.34](#section-12-34)), committed as `fab477f`; P4.4 PASS ([§12.35](#section-12-35)), P4.5 PASS ([§12.36](#section-12-36)) the wireframe-overlay fix ([§12.37](#section-12-37)) and P4.6a/b ([§12.38](#section-12-38); command-list layer deferred) and P4.7 PASS ([§12.39](#section-12-39)), uncommitted. The exit gate is not yet met: Material, Light, SceneCamera, Metadata and Extension producers lack scripted transition tests.

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-6"></a>

### 5.6 Phase 4 — Explicit Invalidation and Persistent Scene Caches (R2–R3)

**Entry:** Phase 0. P3.3 before command/VAO integration in P4.6. This phase is deliberately before
on-demand rendering: stale caches are easier to detect while all passes still execute every frame.

- [x] **P4.1 — Revision contract ([§3.8](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-8)).** Enumerate producers/consumers for topology, geometry,
  transforms, morph weights, skin bindings, materials/pointer animation, settings, selection, camera and
  viewport size. Initially map revisions to the existing conservative rebuild behavior; add counters.
  *Implemented and validated in [§12.32](#section-12-32).*
- [x] **P4.2 — Selection-only updates ([§3.8](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-8)).** Remove full synchronization only after verifying all
  selection entry points and their actual side effects. Verify inspector, focus, OBB and camera gizmo.
  *Implemented and validated in [§12.33](#section-12-33).*
- [x] **P4.3 — Playback and initial-load decoupling ([§3.15](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-15), [§3.27](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-27)).** Separate these into two measured
  changes. Preserve pointer-driven material updates, scrubbing, stop/reset, clip toggles and initial pose.
  Do not rebuild everything each tick or blindly clear dirty flags set by initialization callbacks.
  *Implemented and validated in [§12.34](#section-12-34).*
- [x] **P4.4 — Bounds/reverse-map caches ([§3.18](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-18), [§4.1](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-1), [§4.2](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-2)).** Key by appropriate model/scene/transform
  generations, not raw addresses alone. Handle subtree changes, scene switching and deforming geometry
  conservatively. Validate selection IDs after reload and with multiple viewports.
  *Implemented and validated in [§12.35](#section-12-35).*
- [x] **P4.5 — Viewport settings ownership ([§3.6](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-6)).** Separate shared immutable material/mesh data from
  per-viewport overrides. Test two viewports with different modes, winding and culling. Keep existing
  shader variants initially; dynamic debug-mode uniforms are a separate P8.2 experiment.
  *Implemented and validated in [§12.36](#section-12-36).*
- [ ] **P4.6 — Scene and command caches ([§3.5](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-5), [§4.1](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-1)).** After P4.1/P4.3/P4.5, cache one layer at a time:
  static descriptors, defines/program selection, then full command lists. Include ownership, pass,
  settings and layout revisions. Preserve transparent sorting and per-viewport camera data. Validate
  transmission material/pass lifetimes and wireframe overlay before reusing those command sequences.
  *P4.6a (program selection) and P4.6b (picking descriptors) PASS; the full command-list layer is
  DEFERRED with measurements, so the task stays open. See [§12.38](#section-12-38).*
- [x] **P4.7 — Dirty subtree/skin updates ([§3.25](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-25)).** After P4.3, independently add subtree propagation,
  transform/normal reuse, morph dirtiness and joint upload suppression. Test animated ancestors,
  attached cameras/lights, non-uniform scales and roots shared by multiple skins.
  *Implemented and validated in [§12.39](#section-12-39).*

**Exit gate:** every revision producer has a targeted transition test; unchanged frames avoid intended
rebuilds while each relevant change refreshes exactly the affected data. No stale pointers or output
in reload/scene-switch/animation/multi-viewport sequences. Full frame comparisons remain mandatory.

<a id="section-12-32"></a>

### 12.32 P4.1 — Revision Contract (2026-10-23)

**Status:** PASS; committed as `ebd4384`. R0/R2 bookkeeping only: every change kind still triggers the same full
rebuild, so rendering and rebuild frequency are unchanged by design.

#### 1. Mechanism

- `header/imgui/SceneRevisions.h` / `source/imgui/SceneRevisions.cpp`: a `SceneChange` enum (15 kinds), a
  per-kind monotonically increasing revision, a pending bit mask, and a total mark count. Revisions never
  reset, so later caches can key on the revision they were built from.
- The shared `bool isSceneDirty` is gone. All 72 former `= true` sites in `main.cpp` and eight panels now call
  `mark(SceneChange::X)`; panel signatures take `SceneRevisions&` (or `SceneRevisions*` in Resources).
- Sync point (`main.cpp`, "Synchronize glTF Scene"): `hasPending()` triggers the unchanged
  `updateGltf` → `updateScene` → `applyAnimationTime`. `consumePending()` then clears the mask; any
  consumed kind still sets the TAA `sceneDirty` responsive blend, as before. Marks made after the sync
  point (viewport picking, panels drawn later) carry into the next frame, matching the old boolean.
- Counters: profile frames gain `sceneChangeMask`, `sceneChanges` (names) and `revisionMarks`. UI-script
  snapshots gain `revisions` (per-kind revisions, `syncs`, `marks`, `lastConsumed`, `pending`).
- Diagnostics: `--revision-selfcheck <out>` (72 assertions) and harness command `animplay on|off`.
- One behavior-neutral addition: a hierarchy node click/double-click now also marks `Selection`. That
  branch already marked `ViewCamera` through focus, so the frame synced before and still syncs once.

#### 2. Producer / consumer contract

| Kind | Producers | Current consumer |
|---|---|---|
| Topology | `loadGltfModel` (argv, recent files, File > Open, drop, reload, harness `reload`), `newEmptyGltf` | full sync |
| Visibility | hierarchy node/mesh eye toggles, `--hide-node` | full sync |
| Transform | Inspector node TRS drags and Reset | full sync |
| Material | Material Inspector edits/extension add, primitive material binding, material variant (viewport toolbar, Resources, Viewport Settings) | full sync |
| Light | Inspector light color/intensity/range | full sync |
| SceneCamera | Inspector glTF camera type/FoV/near/far | full sync |
| Extension | Extension Inspector JSON edit/remove | full sync |
| Metadata | Inspector node rename | full sync (not rendered; P4 candidate to drop) |
| AnimationSet | clip toggles and "select all" (Animation toolbar and status bar) | full sync |
| AnimationTime | per-frame tick while playing, status-bar seek | full sync |
| RenderSettings | skybox, environment/IBL, tone mapping, punctual lights, skinning/morphing toggles | full sync |
| ViewportSettings | debug render mode (menus, Ctrl+Alt+1/2, panel), AA mode, double-sided, winding, wireframe overlay | full sync |
| ViewportLayout | `--create-viewport-cam` startup viewport | full sync |
| ViewCamera | focus (hierarchy, viewport, `--select-node`), attach/detach scene camera, user projection/FoV, CLI camera flags | full sync |
| Selection | viewport picking/clear, Resources node rows, hierarchy mesh/material/camera/light rows, hierarchy node rows | full sync |

All kinds feed the TAA responsive blend for the frame they are consumed in.

Derived data with no direct UI producer: geometry, morph weights and skin bindings change only through
Topology, AnimationSet, AnimationTime and RenderSettings. Viewport size and user-camera orbit/pan/zoom are
not producers; FBO/AA targets handle resize and TAA detects camera motion from the view-projection
matrix. These remain outside the contract until a consumer needs them.

Selection entry points that already change selection without any mark (and therefore without a sync)
include the Validation panel, Inspector child/mesh/material/skin/joint links, most Resources categories,
`--select-material/-image/-texture`, and viewport selection. Their OBB, gizmo and inspector already work
without a renderer rebuild, which is direct evidence for P4.2.

#### 3. Validation (`python scripts/validate_p4_1.py`; outputs in `build/p4.1/`)

Environment: Windows 11, NVIDIA GeForce RTX 5080, OpenGL 4.1.0 NVIDIA 616.92, Release, `--isolated`,
1280×800 window, vsync off for comparisons. Baseline: `build/p4.1/baseline/Glider.exe`, SHA-256
`2A57417C…95ACCF`, which matches the accepted Phase 3 final binary recorded in §12.31. Candidate SHA-256
`C1D7A2FD…D0A3CF`.

| Check | Result |
|---|---|
| `--revision-selfcheck` | PASS, 72 assertions: per-kind bit/revision isolation, collapse of repeated marks, consume semantics, carry-over, sentinel ignored. The forced-failure run exits 1. |
| Production fixture transitions | PASS. Idle after load: 0 marks, 0 syncs over 30 frames. `picknode`: Selection +1, exactly 1 sync, then 0 syncs over 30 idle frames. Hierarchy node click: Selection +1 and ViewCamera +1, 1 sync. Reload: Topology +1, 1 sync. Nothing is left pending at any snapshot. |
| Eye toggle | PASS with a recorded quirk: the eye overlaps its tree row, so a click also selects and focuses the node. Press and release land on different frames, giving 2 syncs per click. Visibility advances exactly once per click. The baseline shows the same paired sync frames (below). |
| Animation playback (Fox) | PASS. Idle after load: 0 syncs. Playing: only AnimationTime advanced (36 ticks), with exactly one sync per tick. After pause: 0 syncs over 30 frames. |
| Startup producers | PASS. `--hide-node` → Visibility 1. Camera flags → ViewCamera 1. The argv model → Topology. Afterwards 0 syncs over 20 frames. |
| Baseline vs candidate captures | PASS, byte-identical frame-90 viewport PNGs: S0 cube (none), S5 fixture MSAA4, S5 fixture with `--select-node BlendQuadNode`, S4 Fox TAA at t=0.75. The per-frame `dirtySyncs`/`updateGltfCalls`/`updateSceneCalls` sequences are identical. |
| Interactive control | PASS. The same fixture script on both binaries syncs on identical frames `[1, 11, 64, 106, 113, 114, 120, 121, 158]`, and the final capture at frame 330 is byte-identical. |
| Viewed artifacts | `candidate/s4_fox_taa.png`, `candidate/s5_fixture_selected.png`, `candidate/interactive.png` look correct. |
| `git diff --check` | PASS |

#### 4. Coverage gaps and limitations

- Not exercised by a scripted transition: Inspector edits (Transform, Light, SceneCamera, Metadata,
  Material binding), Material and Extension Inspector edits, AnimationSet toggles, status-bar seek,
  RenderSettings, ViewportSettings panel/menus, attach/detach, and variant selection. These sites are
  covered by source review only. The exit gate's "every producer has a targeted transition test" stays
  open: P4.2–P4.7 must add a test for each kind they narrow, before narrowing it.
- The harness `aamode`/`vpsetting`/`vpcreate` commands assign settings directly and bypass the UI
  producers. They are not evidence for ViewportSettings or ViewportLayout marks.
- `hier.node_eye` probes are not recorded (`IsRectVisible` fails although the eye is drawn), so the test
  clicks fixed window coordinates (264,171) at 1280×800.
- No timing claim is made. Sync frequency is unchanged by design.
- No Debug/GL-check build was run. No GL calls or ownership changed.

#### 5. Decision

PASS for P4.1 as scoped: the contract, counters and conservative mapping are in place, and baseline
equivalence holds for images and sync frames. Rollback boundary: `SceneRevisions.{h,cpp}`, the producer
renames, the main.cpp sync/snapshot changes, `animplay`, and `scripts/validate_p4_1.py`.

*Later update (P4.2):* `validate_p4_1.py` now expects 0 syncs for the `picknode` step. Its interactive
check expects the baseline sync frames minus the frames whose only consumed change is Selection.
Everything else in the suite is unchanged and still passes on the P4.2 binary.

<a id="section-12-33"></a>

### 12.33 P4.2 — Selection-Only Updates (2026-10-23)

**Status:** PASS; committed as `ebd4384`. R2. A frame whose only pending changes are `Selection` and/or `ViewCamera` (§6) no longer runs
`updateGltf` → `updateScene` → `applyAnimationTime`.

#### 1. Side-effect audit (before editing)

- Renderer: nothing in `OpenGL-Renderer` reads application selection. `EditorRenderLayer` has its own
  highlight selection, but the ImGui app never feeds it.
- Per-frame consumers: `createScene`, the picking reverse map and the per-viewport material/mesh
  overrides (double-sided, winding, debug mode, polygon mode) are rebuilt every frame from layer state.
  The OBB (`getNodeOBB`), the coordinate gizmo, and focus (`presentFocusNode`) read layer world
  transforms and accessor bounds; selection never changes either. Inspector panels read the glTF model
  directly.
- `applyAnimationTime` at an unchanged time is idempotent, so skipping it while paused loses nothing.
  While playing, `AnimationTime` still forces the sync every frame.
- TAA: the OBB/gizmo overlay is drawn into the scene FBO before AA, so a selection change still sets the
  frame's `sceneDirty` responsive blend. Only the renderer sync is removed.
- Many selection paths already changed selection without any sync: the Validation panel, Inspector links,
  most Resources categories, the Focus (F) button and `--select-*`. Their outputs were already correct.

#### 2. Change

- `kRendererSyncChanges` = every kind except `Selection` (`SceneRevisions.h`). The sync runs only when
  `pendingMask() & kRendererSyncChanges` is non-zero. Pending selection is still consumed every frame and
  still drives the TAA blend.
- A `skippedSyncs` counter has been added to UI-script snapshots. The self-check gains 3 mask assertions (75 total).
- Extension (same day, §6 below): `ViewCamera` is also excluded, so a focus via hierarchy click or
  viewport double-click no longer rebuilds either.

#### 3. Validation (`python scripts/validate_p4_2.py`; outputs in `build/p4.2/`)

Method: every scenario runs the same UI script on the accepted Phase 3 binary (`2A57417C…`, which
rebuilt on every selection) and on the candidate (`04590C6E…`). Each snapshot must report identical
selection and camera strings, and each full-window screenshot (viewport, OBB, gizmo, hierarchy,
Resources, Inspector) must be byte-identical. On the candidate, every selection-only step must add
exactly one Selection revision, at least one skipped sync, and zero syncs.

| Scenario | Producers / checks | Result |
|---|---|---|
| Fixture, AA none | real viewport click pick; click on empty sky (clear); harness `picknode`; Resources node row; hierarchy node row (Selection + ViewCamera → 1 sync); hierarchy mesh row; Focus (F) button; Inspector "Inspect Material" link; viewport double-click; 30 idle frames; reload | PASS: 14 snapshots and 8 screenshots match the baseline. Selection-only steps: 0 syncs. Focus/link steps: 0 marks, 0 syncs. Reload: 1 sync. 7 syncs skipped in total. |
| Fixture, TAA | the same script | PASS: all 8 screenshots byte-identical, so the TAA responsive blend after selection is unchanged |
| Fox, paused at t=0.75 | skinned mesh node; joint `b_Hip_01` (gizmo, no mesh, so no OBB); Focus on the joint; reload | PASS: 4 screenshots byte-identical, joint transforms shown in the Inspector match |
| Two viewports | `picknode` 2 then 0 with both viewports open | PASS: screenshots byte-identical, 0 syncs |
| Playback (candidate only) | `picknode` while the Fox plays, then while paused | PASS: 28 syncs for 28 ticks while selecting during playback. The selection is recorded. Selection after pause: 0 syncs. |
| P4.1 suite | re-run on the P4.2 binary | PASS: captures and sync frames match the baseline except the selection-only pick frame 64 |
| Self-check | `--revision-selfcheck` | PASS, 75 assertions |
| `git diff --check` | | PASS |

Viewed: `candidate/taa_vp_pick.png` (orange OBB, gizmo and Inspector on the picked quad) and
`candidate/fox_joint_node.png` (joint gizmo and Inspector, no OBB because the joint has no mesh), plus
the P4.2 exploration capture. All match expectations.

#### 4. Benefit and limits

- Deterministic work reduction: each selection-only frame drops 1 `updateGltf`, 1 `updateScene` and
  1 `applyAnimationTime`. That is 7 full syncs in the fixture script.
- No timing claim: on these fixtures the rebuild is too small for a meaningful frame-time difference.
  The audit's large-scene hitch estimate remains unmeasured.
- Coverage gaps: hierarchy camera/light rows (the fixtures have no cameras or lights), the "Material
  Variants" selection, and Validation-panel row selection (already unmarked before this change) were not
  scripted. Viewport-only selection (hierarchy viewport rows) has no mark and no sync, before or after.
- No Debug/GL-check build: no GL calls changed.

#### 5. Decision

PASS. Rollback boundary: `kRendererSyncChanges` and its use at the sync point in `main.cpp`, the
`skippedSyncs` counter, the 3 self-check assertions, `scripts/validate_p4_2.py` and the P4.1 script
expectation update.

#### 6. Extension: ViewCamera-only changes (2026-10-23)

**Status:** PASS; committed as `ebd4384`. `ViewCamera` is removed from `kRendererSyncChanges`; `SceneCamera` (glTF
camera property edits) still syncs. The tables in §3 above describe the selection-only binary
(`04590C6E…`). Results below are for the extended binary (`105349F1…`); the whole suite was rerun on it.

Side-effect audit (before editing):
- Producers: focus (hierarchy node click, viewport double-click, `focusCameraOnNode`); attach/detach
  through the viewport dropdown, the Inspector "Detach from Camera" button and the Viewport menu; user
  camera orthographic toggle and FoV in Viewport Settings; CLI camera flags (startup, where Topology
  already syncs).
- The viewport camera is chosen every frame (`main.cpp`, before the scene build). When attached,
  `setActiveGltfCameraByNode` looks the node up in `m_cameraMapping`, which is built at load and changed
  only by a sync. `setViewportResolution`/`setAspectRatio` then recompute the projection from the stored
  FoV/near/far (or ortho mag). Otherwise the frame calls `vp->rendererCamera` +
  `updateUserCameraTransform()` and `setActiveCamera`.
- What a sync would have redone: `GltfCameraMapping::onUpdateMapping` re-applies the glTF projection with
  `viewportAspectRatio`. The same frame overwrites that via `setAspectRatio` before the camera is used.
  The camera view matrix comes from the node transform hierarchy, which attach/detach do not change.
  The TAA jittered projection is restored after each draw.
- User-camera edits write `vp->rendererCamera` directly. The Num5/menu orthographic toggles and Focus (F)
  already changed the camera with no mark and no sync.
- TAA: ViewCamera still sets the frame's responsive blend. Camera motion is also detected from the
  view-projection matrix.

Validation (`python scripts/validate_p4_2.py`, all sections; Phase 3 binary vs extended candidate):

| Scenario | Result |
|---|---|
| Duck camera script, AA none: open Viewport Settings camera tab; orthographic on/off; FoV slider click; attach "Camera Node 1" via dropdown; detach via the Inspector button; re-attach; detach via the dropdown "Free Look"; hierarchy node click (focus); viewport double-click (focus); 30 idle frames; reload | PASS: 12 snapshots match the baseline in selection and camera, and 9 full-window screenshots are byte-identical. The 5 sampled states have distinct hashes, so each click had a visible effect. Every camera step adds ViewCamera (and Selection for focus) with 0 syncs. No sync between opening the tab and idle. Reload: 1 sync. |
| Duck camera script, TAA | PASS: 9 screenshots byte-identical |
| Fixture / TAA / Fox / two viewports / playback (rerun) | PASS. Hierarchy node click is now Selection + ViewCamera with 0 syncs; the fixture skips 9 syncs in total. |
| P4.1 suite (rerun) | PASS. Interactive sync frames `[1, 11, 114, 121, 158]` equal the baseline minus the no-sync frames `[64, 106, 113, 120]`. An eye click now syncs once (the Visibility release frame) instead of twice. The final capture is byte-identical. |
| Self-check | PASS, 77 assertions, including "ViewCamera does not require renderer sync" and "SceneCamera still requires renderer sync" |
| Viewed | `candidate/camtaa_ortho_on.png`: orthographic duck, no skybox in ortho, identical to the baseline |

Limits: the Viewport menu "Detach from Camera" item and the "Num 5" key (the harness has no numpad
keys) were not scripted; they write the same fields as the tested paths. Attach was tested with a single
perspective glTF camera; no orthographic glTF camera or animated camera node fixture is available.
There is no timing claim.

<a id="section-12-34"></a>

### 12.34 P4.3 — Playback and Initial-Load Decoupling (2026-10-23)

**Status:** PASS; committed as `fab477f`. R2/R3. This was done as two separate changes with separate evidence:
P4.3a (playback ticks) and P4.3b (duplicate sync after load).

#### 1. Audit (before editing)

- Playback order per frame: `AnimationPlayer::update()` advances time and already calls
  `applyAnimationTime(t)`. The sync point then ran `updateGltf` + `updateScene` + `applyAnimationTime(t)`
  again, so each tick did two applies plus a full rebuild.
- Every animation target, including all `KHR_animation_pointer` targets (material factor uniforms,
  `KHR_texture_transform`, node TRS/weights, mesh weights, camera and light parameters), writes
  renderer objects through callbacks. Nothing writes the glTF model. With an unchanged clip set, the
  per-tick rebuild only re-derives renderer state from an unchanged model, and the next channel
  evaluation overwrites the animated parts.
- Clip-set changes (toggle, select all, active clip) are `AnimationSet`. They recreate the animation
  mapping, and `updateScene` restores the rest pose of nodes that are no longer animated, so they keep the
  full rebuild. Stop, seek and the toolbar first/last buttons call `applyAnimationTime` directly.
- Load: `loadGltfModel` runs `updateGltf` → `updateScene` → `applyAnimationTime(0)`. Then
  `AnimationPlayer::setModel` selects the playable clips, updates the animation mapping and applies time 0.
  Only then does it mark `Topology`, which made the next frame rebuild everything again.
- Initialization flags: `recalculateBones` (set by skin/animation mapping callbacks) is read only by
  `EditorRenderLayer`, which the app does not use. Transform dirtiness is resolved inside
  `applyAnimationTime`. No flag is cleared by this change.

#### 2. Changes

- P4.3a: `AnimationTime` is removed from `kRendererSyncChanges`. When it is the only pending
  renderer-relevant kind, the sync point calls `applyAnimationTime(currentTime)` instead of the full
  rebuild. The redundant re-apply is kept deliberately (P4.7 may drop it). An `animationApplies` counter
  is added to UI snapshots.
- P4.3b: `SceneRevisions::noteSynced(kind)` and `unsyncedPendingMask()`. After its own sync,
  `loadGltfModel` (and `newEmptyGltf`) marks `Topology` and notes that revision as synced. The sync point
  rebuilds only for pending kinds whose revision is unsynced. The `Topology` bit stays pending for other
  consumers (TAA blend, counters), and any later mark makes the kind unsynced again. This follows §3.27:
  "clear only dirty revisions already consumed".
- Diagnostics: `--fixed-frame-time <s>` gives deterministic playback, and `--force-full-sync` restores
  the P4.1 mapping as an A/B reference.
- Fixture: `scripts/generate_pointer_animation_fixture.py` writes
  `resources/test_models/pointer_animation_fixture.gltf`. It is not packed into the exe. It animates
  `baseColorFactor` and `emissiveFactor` through `KHR_animation_pointer`, plus a node rotation.
- Self-check: 85 assertions (+2 for the AnimationTime/AnimationSet mask, +6 for noteSynced semantics).

#### 3. Validation (`python scripts/validate_p4_3.py`; outputs in `build/p4.3/`)

P4.3a uses the same binary twice, once normally and once with `--force-full-sync`, both with
`--fixed-frame-time 0.05`. The Phase 3 binary cannot be compared mid-playback because playback there
follows wall-clock time. P4.3b compares against the accepted Phase 3 binary (`2A57417C…`). Final
candidate SHA-256 `26509DD4…`.

| Scenario | Result |
|---|---|
| Pointer fixture, AA none: play; loop wrap; pause; status-bar seek; stop; replay; clip off; clip on; reload | PASS: 13 snapshots and 10 screenshots match the reference. Playback windows of 10 and 33 ticks: 0 rebuilds and exactly one pose apply per tick; the reference rebuilt every tick. Seek and stop: 0 rebuilds. Clip off and clip on each still rebuild, and clip off shows the rest pose (red, unrotated). Reload: Topology +1, 0 rebuilds. |
| Pointer fixture, TAA, two viewports | PASS: screenshots identical, and playback uses pose applies only |
| Fox, skinned, with loop wrap | PASS: windows of 28 and 48 ticks, 0 rebuilds. Screenshots identical, including a joint pick during playback. |
| AnimatedMorphCube, morph weights | PASS: two windows of 13 ticks, 0 rebuilds, screenshots identical |
| Startup loads vs Phase 3 (7 cases: cube, fixture MSAA4, Fox TAA at t=0.75, pointer at t=1.5, morph FXAA, Duck with `--create-viewport-cam 1`, fixture with `--hide-node`) | PASS: the first presented frame and frame 30 are byte-identical in every case. Rebuilds after load: 1 → 0 when nothing else is pending; 1 → 1 for the camera viewport (ViewportLayout) and `--hide-node` (Visibility), as intended. |
| Scripted reloads vs Phase 3: fixture → Fox (joint pick) → fixture → Duck attached to its glTF camera → reload while attached → detach | PASS: all snapshots and 5 screenshots identical. Each reload: Topology +1, 0 rebuilds. The attached camera is re-resolved after reload. |
| Earlier suites rerun on the final binary | `validate_p4_1.py` 68/68, `validate_p4_2.py` 305/305, `validate_p4_3.py` 119/119 |
| Viewed | `candidate/ptr_play_b.png` (pointer colour and rotation mid-clip), `candidate/morph_play_b.png`, `candidate/rl_duck_reloaded_attached.png` |
| `git diff --check` | PASS |

Distinctness: the 14 sampled playback and transport screenshots have distinct hashes, so the
identical-to-reference results are not vacuous.

Test-only fixes: the P4.1/P4.2 expectations were updated to the new contract (playback applies instead of
rebuilds; reloads do not rebuild). Their baseline comparisons now accept only frames whose consumed kinds
are all no-rebuild kinds (Selection, ViewCamera, AnimationTime, synced Topology). Viewport double-clicks
now use the harness `dclick` command: two separate clicks 3 frames apart were occasionally split by
ImGui's wall-clock double-click window under TAA. That failure reproduced once and passed three times on
rerun. It is unrelated to this change; the Duck fixture has no animation.

#### 4. Benefit and limits

- Deterministic work reduction: during playback, each tick drops 1 `updateGltf` and 1 `updateScene`
  (reference: one full rebuild per tick; candidate: none). Each load drops the duplicate first-frame
  rebuild.
- No timing claim. On these small fixtures the rebuild is too cheap to measure. The large-scene
  playback estimate in §3.15 remains unmeasured.
- Not covered: `KHR_texture_transform` pointer animation (no fixture), multiple simultaneous active
  clips on separate targets, the toolbar first/last buttons, and loading while playback is running.
  Inactive-clip filtering in `applyAnimationTime` (§3.15 item 2) and dropping the redundant per-tick
  re-apply are left for P4.7.

#### 5. Decision

PASS. Rollback boundary: the `AnimationTime` mask change and the pose-only branch in `main.cpp` (P4.3a);
`noteSynced`/`unsyncedPendingMask` and the two loader calls (P4.3b); the diagnostic flags, the fixture
and its generator, `scripts/validate_p4_3.py`, and the P4.1/P4.2 script updates.

<a id="section-12-35"></a>

### 12.35 P4.4 — Bounds and Reverse-Map Caches (2026-10-23)

**Status:** PASS (uncommitted). R2.

#### 1. Audit (before editing)

- Selected-node OBB (§3.18): `getNodeOBB` walks the selected subtree and its accessors every frame, once
  per rendered viewport, while `showSelectedBoundingBox` is on (the default). It reads **layer** world
  transforms (`GltfRenderLayer::getWorldTransform`) plus glTF accessor min/max. It ignores visibility and
  skin/morph deformation, so a cache only has to reproduce exactly that.
- Scene extents (§4.2): `utils::getExtends` walks the whole scene every frame per viewport while
  `showSceneBoundingBox` is on. It reads **glTF** node transforms and accessors.
- What changes those inputs: layer world transforms change only in `updateScene` and
  `applyAnimationTime`, the two places that call `m_transformHierarchy.apply()`. glTF node transforms
  change through Inspector edits (marked `Transform`) and loads, and both reach the layer only through
  `updateScene`. Important: the status bar's Rewind, Stop and End buttons call `applyAnimationTime`
  directly and mark **no** revision. A cache keyed on `SceneRevisions` alone would go stale there, so the
  keys come from the layer.
- Reverse mapping (§4.1): the `createScene` result was **copied** into `currentReverseMapping` per
  viewport per frame and never used afterwards. Rebuilding it only on structural change, by caching
  `createScene` itself, belongs to P4.6.

#### 2. Changes

- `GltfRenderLayer::getSceneGeneration()` advances on every `updateScene`. `getPoseGeneration()` advances
  on every `updateScene` and every `applyAnimationTime`. These are generation counters, not raw pointers.
- `main.cpp` `BoundsCache`:
  - The selected-node OBB is keyed on (node, layer, pose generation).
  - The scene extents are keyed on (scene, layer, scene generation).
  - Both are shared by all viewports in a frame, and never reused without a layer.
  - A reload or scene switch always bumps both generations, so an address reused by a new model can't hit.
- `currentReverseMapping = std::move(reverseMapping)` replaces the per-viewport map copy.
- Counters: profile frames gain `boundsComputations`/`boundsCacheHits`, and UI snapshots gain cumulative
  `boundsComputed`/`boundsHits`.

#### 3. Validation (`python scripts/validate_p4_4.py`; outputs in `build/p4.4/`)

Every scenario runs the same UI script on the pre-change HEAD binary (P4.3, `26509DD4…`, copied to
`build/p4.4/head/`) and on the candidate (`499726C7…`). Both use
`--fixed-frame-time 0.05`, a camera placed to show both boxes, and the scene bounding box enabled. Every
snapshot must match in selection and camera, and every full-window screenshot must be byte-identical.

| Scenario | Result |
|---|---|
| Static (production fixture): select node 2; 30 idle frames; **real Inspector translation edit** (double-click field, type 0.9, Enter → `Transform` +1); Reset Transform; select node 0; reload Fox and select its mesh node; reload fixture; real viewport-click pick after reload; second viewport plus click pick | PASS: 12 snapshots and 7 screenshots match HEAD. Idle windows: 0 computations and 2 cache hits per frame (one box plus scene bounds). Each edit, reset, selection change and reload recomputes. A second viewport reuses the cache. Picking after reload, and in the new viewport, selects the same node as HEAD. |
| Playback (pointer fixture, SpinQuad selected) | PASS: 5 screenshots match HEAD. While playing, the box is recomputed exactly once per tick (10 for 10) and the scene extents are reused. Paused: reused. **Stop, End and Rewind** (unmarked paths, no revision mark) each recompute and render identically to HEAD. |
| Earlier suites on the P4.4 binary | `validate_p4_1.py` 68/68, `validate_p4_2.py` 305/305, `validate_p4_3.py` 119/119, `validate_p4_4.py` 54/54 |
| Viewed | `candidate/st_moved.png` (scene bounds plus the moved node's gizmo and box), `candidate/pb_end.png` (the box follows SpinQuad's final pose) |
| Distinctness | the 12 sampled screenshots have distinct hashes |
| `git diff --check` | PASS |

#### 4. Benefit and limits

- Deterministic reduction: on unchanged frames, 0 subtree/scene walks instead of one per viewport per
  frame for each enabled box. The picking map is no longer copied per viewport per frame. During playback
  the box is still recomputed every tick (its pose changes); the scene extents are not.
- No timing claim. These fixtures are small, and the §3.18 large-assembly estimate remains unmeasured.
- Two-viewport coverage used a tabbed second viewport (only the active tab renders); side-by-side docking
  can't be scripted. The OBB still ignores skin/morph deformation, unchanged from before. `createScene`
  and the reverse map are still rebuilt every frame (P4.6). The P4.1 gap for an Inspector Transform edit
  is now covered by a real UI transition.

#### 5. Decision

PASS. Rollback boundary: the two layer generations, `BoundsCache` and its two call sites, the reverse-map
move, the bounds counters, and `scripts/validate_p4_4.py`.

<a id="section-12-36"></a>

### 12.36 P4.5 — Viewport Settings Ownership (2026-10-23)

**Status:** PASS (uncommitted). R2.

#### 1. Audit (before editing)

Per frame and per viewport, `main.cpp` wrote viewport settings into the render layer's **shared**
materials and meshes, in two loops:

- Before `createScene`, over every material and mesh in the layer mappings:
  - material `isDoubleSided` (forced, or restored from glTF) and the material `DEBUG` define;
  - mesh `frontFace` (world-matrix determinant XOR `invertWinding`, with a map lookup per mesh);
  - mesh `polygonMode` and the mesh `DEBUG` define.
- After `createScene`, over the drawables: mesh `polygonMode`/`DEBUG`, material `DEBUG`, and forced
  double-sidedness.

Tracing who reads what:

- `createScene` resets each drawn mesh's `DEBUG` define and `polygonMode` on every call. So the first
  loop's mesh `DEBUG`/polygon writes and its material `DEBUG` write are overwritten before any command
  is built, including the transmission duplicates, which the second loop also covers.
- Only material `isDoubleSided` and mesh `frontFace` persist on shared objects between `createScene`
  calls. Commands read them in `CommandMapping::makeCommand` (culling, winding).
- Picking materials and transmission duplicates are copies taken after (picking) or during
  (transmission) these writes. So every override must stay visible on the shared objects at
  command-build time.
- The vertex shader variant is chosen from `mesh->defines`, including `DEBUG`, and `Defines::hash`
  depends on iteration order. Moving the overrides into command generation would therefore need
  per-command define copies or different variant keys, which conflicts with "keep existing shader
  variants initially". That is deferred to P4.6, where command caches can key on the overrides.

#### 2. Changes

- `header/imgui/ViewportOverrides.h`, `source/imgui/ViewportOverrides.cpp`: `ViewportDrawOverrides`
  (forceDoubleSided, invertWinding) and `SharedOverrideState`.
  - Ownership: the layer owns the glTF-derived base values. `SharedOverrideState` is the single writer
    of the persistent per-viewport overrides, and it records which overrides are currently applied.
  - It rewrites only when the overrides differ from what's applied, or when the layer's glTF, scene
    or pose generation changed. `updateGltf` resets `isDoubleSided`, `updateScene` recreates meshes,
    and any pose change can flip a determinant.
- `GltfRenderLayer::getGltfGeneration()`: a new counter that advances on `updateGltf`.
- `main.cpp`: the pre-`createScene` loops are replaced by `SharedOverrideState::apply`. The dead
  `DEBUG`/polygon writes are removed. The post-`createScene` per-drawable loop is unchanged, because
  `createScene` resets those values on every call (P4.6).
- Counters: `overrideApplies`/`overrideSkips` in profile frames and UI snapshots.
- Test harness: `vpsplit <id>` docks a viewport beside viewport 1, so both render every frame. Tabbed
  viewports render only the active tab, so earlier "two-viewport" checks never rendered both in one
  frame.

#### 3. Validation (`python scripts/validate_p4_5.py`; outputs in `build/p4.5/`)

Reference: the pre-P4.5 binary (P4.4 plus `vpsplit`, `E7D7CB06…`). Candidate: `E1E51341…`. Both use
`--fixed-frame-time 0.05` and two viewports split side by side. Every snapshot must match the reference
with both viewports rendering, every full-window screenshot must be byte-identical, and states must be
visibly distinct except where they're expected to be equal.

| Scenario | Result |
|---|---|
| Split, cull/winding fixture (front-facing and back-facing single-sided planes, a double-sided plane, a mirrored negative-scale plane): equal settings; viewport 2 double-sided + inverted; both; viewport 2 inverted only; restored; reload | PASS: 6 screenshots byte-identical. Viewport 1 culls the back-facing single-sided plane while viewport 2 (double-sided) shows it, in the same frame. "Restored" equals "same", and all other states differ. Counters: equal overrides → 0 applies and 46 skips over 46 viewport renders; differing overrides → one apply per viewport render; reload → re-applies once, then skips. |
| Modes, cull fixture: viewport 2 Geometry Normal and viewport 1 Wireframe through the real toolbar dropdowns; viewport 2 BaseColor, then + double-sided | PASS: 4 screenshots byte-identical and all distinct. Different debug modes with equal persistent overrides → 0 applies and 46 skips. |
| Playback, Fox skinned, with viewport 2 double-sided + inverted, then equal | PASS: 3 screenshots byte-identical. Differing → one apply per viewport render. Equal while playing → exactly one apply per pose change (23 for 23 ticks) and 23 skips. Paused → skips only. |
| All suites on the final binary | `validate_p4_1.py` 68/68, `validate_p4_2.py` 305/305, `validate_p4_3.py` 119/119, `validate_p4_4.py` 54/54, `validate_p4_5.py` 51/51 |
| Viewed | `candidate/sp_differ.png` (per-viewport culling), `candidate/md_vp1_wire.png` (Wireframe beside Geometry Normal) |
| `git diff --check` | PASS |

The production fixture's materials are all double-sided, so forcing double-sided changes nothing there.
That is why the split and modes scenarios use the cull/winding fixture (`scripts/generate_cull_winding_fixture.py`,
generated into `build/p4.5/fixtures/`).

#### 4. Pre-existing defect found (fixed separately in §12.37)

With **Wireframe Overlay** on (Viewport Settings, a separate option from the Wireframe debug mode), the
filled geometry of the production fixture disappears and only the lines remain. HEAD and the Phase 3
binary behave identically. It was kept out of this equivalence-only task.

*Correction:* this record first attributed the bug to the redrawn skybox cube. That was wrong. The
cause is the transmission set-up (a `TransmissiveBlit` clear) that the overlay redraws, and it affects
only scenes with transmissive materials. See [§12.37](#section-12-37).

#### 5. Benefit and limits

- Deterministic reduction: when all open viewports share the same double-sided and winding settings
  and the layer is unchanged, the per-material and per-mesh loops (one map lookup and one determinant
  per mesh) no longer run. Each viewport with different settings still re-applies, as before. During
  playback the overrides re-apply once per pose change.
- Not a full immutability split: the overrides still live on the shared objects, written by one owner
  with tracked state. Command-time overrides need P4.6's command cache (see §1).
- No timing claim. The per-drawable post-`createScene` loop remains (P4.6).

#### 6. Decision

PASS. Rollback boundary: `ViewportOverrides.{h,cpp}`, the CMake entries, `getGltfGeneration`, the
`main.cpp` applier call and counters, the `vpsplit` harness command, and `scripts/validate_p4_5.py`.

<a id="section-12-37"></a>

### 12.37 Wireframe Overlay with Transmission — Bug Fix (2026-10-23)

**Status:** FIXED (uncommitted). This is a correctness fix, not a Phase 4 task. It was found during P4.5
([§12.36](#section-12-36)) and done as its own change before P4.6, at the user's request.

#### 1. Symptom and corrected cause

With **Wireframe Overlay** on, a scene containing a transmissive material lost all its shaded geometry:
only lines remained, over the skybox. HEAD and the Phase 3 binary behave the same.

The first diagnosis in §12.36 blamed the skybox cube and was **wrong**. Removing the cube from the
overlay scene made the fixture worse (lines over the plain clear colour) and changed nothing for Fox or
Duck. Those models never showed the bug, because they have no transmissive material.

Actual cause: for transmissive scenes, `createScene` adds the transmission set-up, namely duplicate
drawables in passes `Opaque2`/`Grid2`/`Transparent2` and the `TransmissiveBlit` drawable. Passes run in
`RenderPassType` order, and `TransmissiveBlitPass` has a post-command that clears the whole framebuffer
(colour and depth) before `Opaque`, `Grid`, `Transmissive` and `Transparent` redraw the final image. The
overlay calls `createScene` again and draws the whole result, so that clear wiped the finished shaded
image. Only the overlay's own line draws and the redrawn skybox survived.

#### 2. Fix

`main.cpp`, overlay path: drop the `TransmissiveBlit`, `Opaque2`, `Grid2` and `Transparent2` drawables from
the overlay scene before switching meshes to line mode. The cube and all other drawables are kept exactly
as before.

#### 3. Evidence (`python scripts/validate_wireframe_overlay.py`; outputs in `build/wireframe_fix/`)

Pre-fix binary: `build/wireframe_fix/before/Glider.exe` (P4.4 + P4.5). The comparisons use the viewport
area of full-window screenshots at 1280×800, for AA none and TAA.

| Model | Result |
|---|---|
| Production fixture (opaque, blend and transmissive quads) | Before the fix, the overlay changed 18.2% (none) / 18.3% (TAA) of viewport pixels relative to overlay off: the shaded image was wiped. After the fix: 0.1% / 0.2%, the quad outlines drawn over the intact shaded image. With the skybox hidden, the fix also changes the output, because the clear affected that case too. |
| Fox (skinned, no transmission) and Duck (textured, no transmission) | The overlay, and the overlay with the skybox hidden, are byte-identical before and after the fix |
| All models | Overlay off and the Wireframe debug mode are byte-identical before and after |
| Earlier suites on the fixed binary (`A3CF709E…`) | `validate_p4_1.py` 68/68, `validate_p4_2.py` 305/305, `validate_p4_3.py` 119/119, `validate_p4_4.py` 54/54, `validate_p4_5.py` 51/51 |

Viewed: `after_fixture_none/overlay.png` (shaded quads present) against `before_fixture_none/overlay.png`
(lines only), and the difference mask `after_fixture_overlay_diff.png` (the quad outline). Interior
lines take the shaded colour of the surface they lie on, as in the non-transmissive overlay, so they
barely differ from the shaded image.

#### 4. Limits

The overlay still re-runs `createScene` and regenerates commands every frame; P4.6 covers reuse. A
transmissive object's own lines are drawn in the Transmissive pass, which samples the scene colour. No
separate quality review of overlay line appearance was done.

<a id="section-12-38"></a>

### 12.38 P4.6 — Scene and Command Caches (2026-10-23)

**Status:** PARTIAL (uncommitted). Layers P4.6a (program selection) and P4.6b (picking descriptors) PASS. The
full command-list layer is **DEFERRED**, with the measurement in §4. The task checkbox stays open.

#### 1. Audit (before editing)

Every frame, for every viewport, `CommandMapping::makeCommand` runs for every drawable, every transmission
duplicate, and every picking copy (about 3,200 commands at 1,600 drawables). For each, it:
- assembles a ~40-entry fragment `Defines` (an `unordered_map<std::string,int>`) and copies the mesh and
  material defines into it;
- hashes it twice for `ShaderCache::selectShader` and looks up the program;
- refills the uniform and sampler pointer vectors.

Commands hold pointers to uniforms owned by meshes, materials, the camera, the environment and the lights.
Values change in place, but transmission duplicates and picking materials were recreated every frame, so
their pointers changed every frame. `PickingRenderLayer::applyLayer` copied a full `Material` for **every**
drawable, including non-pickable ones (skybox, transmission duplicates) that it then skipped. It also
rebuilt a `std::map` of picking ids and the picking render-pass copies.

Baseline measurement: 1,600-drawable fixture (`scripts/generate_many_drawables_fixture.py`, four materials:
opaque, double-sided, blend, mask), 1280×800, AA none, vsync off, 120 warmup and 300 measured frames. The
pre-P4.6 binary (`A3CF709E…`) took a median `commands.generate` of 15.1–17.1 ms, `picking.build` 6.9–8.0 ms,
`draw.submit` about 10.8 ms and `scene.create` about 1.2 ms. The CPU frame was 34.7–39.7 ms.

#### 2. Changes

- **P4.6a — program selection cache.**
  - `Defines` keeps an order-independent content hash, updated incrementally (XOR of per-entry
    FNV-1a/splitmix64 hashes) on every define and undefine. Re-defining a name with the same value is now a
    no-op. The map type is unchanged, so iteration order, `hash()` and the existing shader-variant identities
    are unchanged.
  - `CommandMapping::selectProgram` caches the `gl::Program*`. The key holds every input of the define
    assembly and shader selection: the vertex and fragment shader names, the mesh and material define
    content hashes, the material render settings (12 flags), tone mapping, IBL, punctual lights and the light
    count. A miss runs the original assembly, moved verbatim into `buildProgram`.
  - Programs live as long as the `ShaderCache`, which has no eviction.
- **P4.6b — picking descriptors.**
  - Picking materials come from a persistent pool, refreshed each call by copy-assignment from the source
    material. This needed `Material::operator=`, which the user-declared move constructor had implicitly
    deleted.
  - Copies are made only for pickable drawables. Picking ids are still assigned to every drawable in the
    same order, so picking colours are unchanged.
  - The id → mesh `std::map` is now a vector indexed by id.
  - Render-pass copies are unchanged.
- **Diagnostics:** `--disable-command-caches` turns off the program cache for a same-binary A/B check. Profile
  frames and UI snapshots gain `programCacheHits`/`programCacheMisses`.
- **Process issue found and fixed during the work:** a patch write to `main.cpp` hit a `PermissionError` at
  `truncate()`, which left 10 stale trailing bytes (`\n}\n#endif\n`). They were detected by the compiler,
  verified and trimmed. Later patches write through a temporary file and an atomic replace.

#### 3. Validation (`python scripts/validate_p4_6.py`; outputs in `build/p4.6/`)

| Check | Result |
|---|---|
| Equivalence: pre-P4.6 vs candidate with `--disable-command-caches` vs candidate. Production fixture: default; viewport pick; Geometry Normal, BaseColor and Shaded through the dropdown; wireframe overlay; split viewports with viewport 2 double-sided and Normal; Fox load; reload. Fox playback: skinning off/on through the status-bar popup. AnimatedMorphCube playback with morphing off. | PASS: 12 screenshots byte-identical across all three, and all snapshots match. Steady windows: 0 misses (276, 552, 552 and 52 hits). The uncached run records 0 hits. |
| Picking with 1,600 drawables: a 6×6 grid of real viewport clicks | PASS: 25 distinct nodes picked, identical to pre-P4.6. The final screenshot is byte-identical. |
| Timing, pre-P4.6 vs P4.6a, 5 alternating pairs | `commands.generate` 15.1–17.1 → 2.05–2.24 ms (median paired −86.6%, 5/5). CPU frame 34.9–39.1 → 21.3–23.2 ms. Baseline noise 12.1%. |
| Timing, P4.6a vs P4.6a+b | `picking.build` 6.9–8.0 → 1.49–1.57 ms. CPU frame 21.4–24.0 → 14.9–15.5 ms (median paired −32.0%, 5/5). |
| Timing, pre-P4.6 vs P4.6a+b | `commands.generate` median paired −88.1% (5/5). CPU frame 34.7–39.7 → 15.0–15.6 ms (median paired −57.4%). Baseline noise 12.5%. |
| Earlier suites on the P4.6 binary | `validate_p4_1.py` 68/68, `validate_p4_2.py` 305/305, `validate_p4_3.py` 119/119, `validate_p4_4.py` 54/54, `validate_p4_5.py` 51/51, `validate_wireframe_overlay.py` 38/38 (binary `861B992E…`) |
| Viewed | `candidate/pk_last.png` (1,600 quads, four materials, the picked node's gizmo and box) |

Baseline run-to-run noise (9–12.5%) exceeds the protocol's 5% guideline. The measured effects are 7–8× the
noise and improve in every pair, so the decision does not depend on it, but the exact percentages should be
read with that variance in mind. Hardware: RTX 5080, OpenGL 4.1 NVIDIA 616.92.

#### 4. Deferred layer: full command lists

After P4.6a+b, the remaining 1,600-drawable frame is about 15 ms CPU: `draw.submit` about 10.3 ms (GL
submission, which is executor/uniform work outside Phase 4), `commands.generate` about 1.9 ms,
`picking.build` about 1.5 ms and `scene.create` about 1.25 ms. Caching whole command lists could remove at
most part of the ~1.9 ms. It would carry the riskiest invalidation: per-viewport camera and environment
pointers, transmission duplicates still recreated by every `createScene`, and sort keys that change with
the camera. Per §6.5 it is DEFERRED for lack of a demonstrated need. Caching `createScene` (P4.4's reverse
map, about 1.25 ms) belongs with it.

#### 5. Decision

P4.6a and P4.6b PASS. The command-list layer is DEFERRED, so P4.6 stays unchecked. Rollback boundaries:
- P4.6a: the `Defines` content hash and define fast path, `CommandMapping` `selectProgram`/`buildProgram`
  and its cache, the `getCommandMapping` accessor, and the flag and counters.
- P4.6b: `Material::operator=` and the `PickingRenderLayer` pool, id vector and skip-before-copy.

<a id="section-12-39"></a>

### 12.39 P4.7 — Dirty Subtree, Morph and Skin Updates (2026-10-23)

**Status:** PASS (uncommitted). R2/R3. Done as three independently measured substeps on top of P4.3.

#### 1. Audit (before editing)

`TransformHierarchy::apply()` runs twice per playback tick: once in `AnimationPlayer::update` and once at
the sync point. Each run traverses **every** node. For each node it:
- recomputes the world matrix;
- pushes it into each attached mesh: `u_ModelMatrix`, plus `u_NormalMatrix` through a full inverse, each
  set by a name search over the mesh uniforms;
- pushes it into each attached camera and light;
- pushes morph weights into every mesh. `updateWeights` sets two defines and, for morph meshes, builds a
  uniform-name string per weight.

The node's weight target holds a vector, so it is marked dirty on every update and is never cleaned.
`Skin::computeJoints` re-uploads the whole joint texture on every apply.

Invalidation sources:
- `TransformHierarchy::clear()` runs on every `updateScene` and rebuilds the parent links.
- Mapping updates re-attach their objects through `addMesh`/`addCamera`/`addLight`. `updateScene` attaches
  meshes **after** its own `apply()`, so a re-attachment must force the next traversal to push.
- Pointer-animated mesh weights call `Mesh::updateWeights` directly. The `weightsUpdated` guard then keeps
  the node's weights from overwriting them, and `resetWeightsUpdated` must keep running on every traversal.

#### 2. Changes

- **P4.7a — dirty-subtree propagation.** Each `TransformHierarchyNode` has a world-valid flag. `traverse`
  recomputes and pushes (meshes, cameras, lights) only when the parent changed, the node's own TRS targets
  are dirty, or the node was invalidated by `addChild`/`addMesh`/`addCamera`/`addLight` or by
  `TransformHierarchy::clear()`. Roots have an identity parent. The traversal order is unchanged.
- **P4.7b — morph dirtiness.** Weights are pushed only when the node changed or its weight target is dirty,
  and the target is then cleaned. `resetWeightsUpdated` still runs on every traversal.
- **P4.7c — joint upload suppression.** `computeJoints` still computes the joint data. It skips the GL
  upload when the texture already holds exactly this data at this size. The record is keyed by texture,
  because copied skins share one texture, and is erased when the last owner is destroyed.
- **Diagnostics:** `--disable-transform-reuse` turns off all three, for a same-binary A/B check. Counters:
  `transformNodesVisited`/`Recomputed` in profile frames, and `transformVisited`, `transformRecomputed`,
  `weightPushes`, `jointUploads` and `jointUploadsSuppressed` in UI snapshots.
- **Fixtures:** `scripts/generate_hierarchy_fixture.py` writes `resources/test_models/hierarchy_animation_fixture.gltf`.
  It has an animated root with a non-uniformly scaled child and grandchild, a camera and a point light under
  the root, and a static sibling subtree. `generate_many_drawables_fixture.py` gains `leaf` (one animated
  grid node) and `root` (an animated parent of all 1,600) variants.

#### 3. Validation (`python scripts/validate_p4_7.py`; outputs in `build/p4.7/`)

Each script runs on the pre-P4.7 binary (`861B992E…`), on the candidate with `--disable-transform-reuse`,
and on the candidate, all with `--fixed-frame-time 0.05`. Snapshots (selection, camera, AnimationTime)
must match and every screenshot must be byte-identical across the three.

| Scenario | Result |
|---|---|
| Hierarchy fixture: play; seek; stop; **Inspector edit** of the static sibling (full sync, hierarchy rebuild); attach to the animated glTF camera and play; reload | PASS: 7 screenshots identical and distinct. 75 of 210 node visits recomputed over 15 ticks; the static sibling is skipped. No-reuse recomputes 210/210. |
| Grid with one animated leaf (1,600 drawables) | PASS: 13 recomputes of 41,600 visits and 13 weight pushes over 13 ticks |
| Grid with an animated root over all 1,600 | PASS: every node is recomputed on every tick (20,813 of 41,626 visits; the second, redundant apply per tick is skipped) |
| AnimatedMorphCube playback with **morphing off/on** mid-play; Fox playback with **skinning off/on** | PASS: 6 screenshots identical and distinct. Fox playing: 13 joint uploads and 13 suppressed over 13 ticks; the no-reuse run never suppresses. Paused morph cube: no weight pushes. |
| Fox and the pointer fixture playback | PASS: screenshots identical |
| Timing on the leaf grid while playing, pre-P4.7 vs candidate, 5 alternating pairs | animation update + apply 1.52–1.55 → 0.30–0.31 ms (median −80.0%, 5/5, noise 7.8%). CPU frame about 16.7 → 15.6 ms (+6.5%). |
| P4.7a only, vs pre-P4.7 | 1.48–1.56 → 1.17–1.19 ms (−23.1%, noise 5.2%) |
| P4.7b+c, vs P4.7a | 1.13–1.19 → 0.30–0.31 ms (−74.1%, noise 5.3%) |
| Earlier suites on the final binary (`68AA8ECF…`) | `validate_p4_1.py` 68/68, `validate_p4_2.py` 305/305, `validate_p4_3.py` 119/119, `validate_p4_4.py` 54/54, `validate_p4_5.py` 51/51, `validate_p4_6.py` equivalence 36/36 and picking 39/39, `validate_wireframe_overlay.py` 38/38, `--revision-selfcheck` 85/85 |
| Viewed | `candidate/hi_attached_play.png` (view from the animated RootCamera, moved static sibling with box and gizmo) |

#### 4. Limits

- Not covered: "roots shared by multiple skins" (no such fixture; a texture-keyed upload record was chosen
  so copies that share a texture stay correct), multiple simultaneous clips, and animated lights' visual
  effect (lights are pushed only when their node changed; the hierarchy fixture's light moves with the
  root).
- `computeJoints` still computes the joint matrices on every apply; only the upload is suppressed.
- The redundant second apply per tick (kept in P4.3) now costs little and is not removed.
- The animated-root case keeps full cost, by design.
- Timing claims are for this hardware (RTX 5080, NVIDIA 616.92).

#### 5. Decision

PASS. Rollback boundaries:
- P4.7a: `TransformHierarchyNode` validity, `traverse` gating, the `clear()` invalidation and the stats.
- P4.7b: the weight push gating and cleaning.
- P4.7c: the `Skin` upload record.
- The flag, counters, fixtures and `scripts/validate_p4_7.py`.

