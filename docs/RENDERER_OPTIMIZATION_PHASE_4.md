# Phase 4 — Invalidation and Persistent Scene Caches

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** In progress. P4.1 PASS ([§12.32](#section-12-32)); P4.2 PASS ([§12.33](#section-12-33)); P4.3–P4.7 not started.

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
- [ ] **P4.3 — Playback and initial-load decoupling ([§3.15](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-15), [§3.27](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-27)).** Separate these into two measured
  changes. Preserve pointer-driven material updates, scrubbing, stop/reset, clip toggles and initial pose.
  Do not rebuild everything each tick or blindly clear dirty flags set by initialization callbacks.
- [ ] **P4.4 — Bounds/reverse-map caches ([§3.18](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-18), [§4.1](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-1), [§4.2](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-2)).** Key by appropriate model/scene/transform
  generations, not raw addresses alone. Handle subtree changes, scene switching and deforming geometry
  conservatively. Validate selection IDs after reload and with multiple viewports.
- [ ] **P4.5 — Viewport settings ownership ([§3.6](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-6)).** Separate shared immutable material/mesh data from
  per-viewport overrides. Test two viewports with different modes, winding and culling. Keep existing
  shader variants initially; dynamic debug-mode uniforms are a separate P8.2 experiment.
- [ ] **P4.6 — Scene and command caches ([§3.5](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-5), [§4.1](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-1)).** After P4.1/P4.3/P4.5, cache one layer at a time:
  static descriptors, defines/program selection, then full command lists. Include ownership, pass,
  settings and layout revisions. Preserve transparent sorting and per-viewport camera data. Validate
  transmission material/pass lifetimes and wireframe overlay before reusing those command sequences.
- [ ] **P4.7 — Dirty subtree/skin updates ([§3.25](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-25)).** After P4.3, independently add subtree propagation,
  transform/normal reuse, morph dirtiness and joint upload suppression. Test animated ancestors,
  attached cameras/lights, non-uniform scales and roots shared by multiple skins.

**Exit gate:** every revision producer has a targeted transition test; unchanged frames avoid intended
rebuilds while each relevant change refreshes exactly the affected data. No stale pointers or output
in reload/scene-switch/animation/multi-viewport sequences. Full frame comparisons remain mandatory.

<a id="section-12-32"></a>

### 12.32 P4.1 — Revision Contract (2026-10-23)

**Status:** PASS (uncommitted). R0/R2 bookkeeping only: every change kind still triggers the same full
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

**Status:** PASS (uncommitted). R2. A frame whose only pending changes are `Selection` and/or `ViewCamera` (§6) no longer runs
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

**Status:** PASS (uncommitted). `ViewCamera` is removed from `kRendererSyncChanges`; `SceneCamera` (glTF
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

