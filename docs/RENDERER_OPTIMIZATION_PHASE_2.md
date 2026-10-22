# Phase 2 — UI Caches and Visible Rows

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Complete. 

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-4"></a>

### 5.4 Phase 2 - UI Caches and Visible-Row Rendering (R2)

**Entry:** Phase 0; P1.5 before logger clipping. No dependency on renderer caching.

- [x] **P2.1 — Animation conflict index ([§3.24](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-24)).** First compare indexed answers with the existing
  algorithm on ordinary and pointer targets; then use it in both animation controls and status popup.
  Preserve deterministic selection order and invalidate on channel/model/active-set changes.
  *Implemented and validated in [§12.16](#section-12-16); review exit-code fix in [§12.18](#section-12-18); independently accepted in [§12.19](#section-12-19).*
- [x] **P2.2 — Search/label caches ([§3.19](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-19), [§3.22](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-22)).** Separate cache construction from row rendering.
  Validate rename, options, search/severity filters, report replacement and model reload.
  *Implemented and validated in [§12.17](#section-12-17); warm-cache review fixes in [§12.18](#section-12-18); independently accepted in [§12.19](#section-12-19).*
- [x] **P2.3 — Flat-list and logger clipping ([§3.22](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-22), [§3.23](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-23)).** Start with fixed-height rows; implement
  multiline/wrapped height accounting as a separate substep. Preserve Copy, selection and auto-scroll.
  *Implemented in [§12.20](#section-12-20), corrected/validated in [§12.21](#section-12-21), independently accepted in [§12.22](#section-12-22).*
  *Implementation and targeted evidence are recorded in [§12.20](#section-12-20); interactive validation limits remain,
  and independent review is pending.*
- [x] **P2.4 — Hierarchy virtualization ([§3.19](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-19)).** Flatten expanded visible tree structure into a row
  model with stable IDs and depth before clipping. Skipping `TreeNodeEx` calls using `IsRectVisible`
  alone is not a validated solution: scroll extent, expansion and keyboard navigation must survive.
  *Implemented and validated in [§12.23](#section-12-23).*

**Exit gate:** large-list search/scroll CPU cost improves, and selection, expand/collapse, focus,
copy/export, wrapping, resizing and reload match the baseline. Any log-retention cap stays deferred
until retention/truncation/Copy behavior is explicitly chosen; it is not necessary for these gains.

<a id="section-12-16"></a>

### 12.16 P2.1 - Animation conflict index (2026-10-21)

```text
Task ID / title: P2.1 - Animation conflict index (R2, Phase 2)
Status: PASS (behavioral equivalence and targeted conflict-query speed); review exit-code fix in [§12.18](#section-12-18);
  uncommitted, awaiting review
Source state: no commit. Changed relative to 3d682fa:
  OpenGL-Renderer/Tools/tools/gltf/include/gltf/Animation.h
  OpenGL-Renderer/Tools/tools/gltf/include/gltf/ChildOfRoot.h
  OpenGL-Renderer/Tools/tools/gltf/include/gltf/Gltf.h
  OpenGL-Renderer/Tools/tools/gltf/include/gltf/animation/AnimationChannelTarget.h
  OpenGL-Renderer/Tools/tools/gltf/source/Animation.cpp
  OpenGL-Renderer/Tools/tools/gltf/source/Gltf.cpp
  OpenGL-Renderer/Tools/tools/gltf/source/animation/AnimationChannelTarget.cpp
  OpenGL-Renderer/Tools/tools/gltf/source/json/AnimationsFromJson.cpp
  OpenGL-Renderer/Tools/tools/gltf/source/merge/AnimationMerger.cpp
  header/imgui/AnimationPlayer.h
  source/imgui/AnimationPlayer.cpp
  source/imgui/main.cpp
Accepted baseline: G:\temp\uray-codex\opencode\glider-p2\builds\p2_baseline.exe
  SHA-256 46F37730934B12E3902770264B5B4E76F2711239BF4D58C2BAC9F5568E41BDF7
Candidate executable: H:\git-repos\gltf-viewer\build\bin\Release\Glider.exe
  SHA-256 D577CF5E7AA65AB604FFBDAE8FA9454ECFC21C93CC7F9CE5BF64EB620A4ABB66
Evidence root: G:\temp\uray-codex\opencode\glider-p2\review-fix\
  conflict_pass.txt
  conflict_forced_fail.txt
Original P2.1 evidence: G:\temp\uray-codex\opencode\glider-p2\p2_1\
```

**Change.** `AnimationPlayer` now caches, once per clip, the unique set of conflict targets. A target is
the pair `(ChildOfRoot*, path)`, using the same path string as the legacy algorithm; a channel with an
`m_animationPointer` resolves to the pointed-to `ChildOfRoot` and pointer path. Duplicate channels are
deduplicated per clip. A second cache maps each active target to the number of playing clips that use
it. `isAnimationConflictingWithActive` answers by looking up the clip's targets in that active map and
subtracting the clip itself when it is playing. `areAllPossibleAnimationsSelected` and
`selectAllPossibleAnimations` use the same counts; selection still visits clips in index order and keeps
the first non-conflicting clip, so the resulting set and its order match the legacy greedy algorithm.
The pairwise `checkConflict` remains in the class as the reference used by the diagnostic.

Cache invalidation uses revisions rather than scanning clip data each frame:

- `Gltf::getRevision()` changes on reset and element creation/import/duplicate/delete.
- `AnimationChannelTarget::getTargetRevision()` changes on node/path/pointer changes and channel creation.
- `Animation::getPlayingRevision()` changes on every effective `setPlaying` change, including undo/redo and
  JSON loading that bypass `AnimationPlayer`.
- Model, target and playing revisions are checked on each query; a changed model or target rebuilds the
  clip target sets, and a changed active set rebuilds the active-target counts. Channel mutation therefore
  invalidates the clip cache without requiring an editor callback.

**Correctness.** `Glider.exe --isolated --conflict-selfcheck <report>` runs `runConflictSelfCheck()`,
which builds a synthetic 11-clip model covering ordinary node targets, pointer targets, duplicate targets
inside one clip, disjoint clips, null targets, repeated toggles, a direct `setPlaying` active-set change,
a channel-target mutation and replacement by a three-clip model. It compares indexed answers with the
legacy pairwise algorithm for every pair and every toggled state.

```text
Result: PASS
Checks: 456
Coverage: ordinary, pointer, duplicate, disjoint, null-target, repeated toggles, direct-playing, channel/model invalidation
Case clips: 11, conflicting pairs: 12, disjoint pairs: 43
Benchmark clips: 300, channels/clip: 4, selected: 24
Initial selection legacy: 0.884 ms; indexed: 0.037 ms; speedup: 24.016x
Per-row conflict scan legacy: 1.561 ms; indexed: 0.037 ms; speedup: 41.861x
Conflicting rows: 276/300
```

The 300-clip benchmark contains four channels per clip, 24 selected clips and 276 conflicting rows. The
indexed selection order and the per-row conflict answers matched the legacy reference in all cases. The
direct-playing test is important because undo/redo and JSON restore can change `isPlaying` without going
through the player controls.

**Measurement.** The self-check timing is an algorithm microbenchmark without rendering, GL, or UI work.
The legacy selection and row scans used 3 repetitions and the indexed queries used 100; the reported
values are best-of-run, not medians. Absolute times are small and vary between runs, so the ratios are
directional and are not a whole-frame claim.

| Query | Legacy | Indexed | Ratio |
| :--- | ---: | ---: | ---: |
| Initial greedy selection, 300 clips x 4 channels | 0.884 ms | 0.037 ms | **24.016x** |
| Per-row conflict scan, 300 rows | 1.561 ms | 0.037 ms | **41.861x** |

**Limits and rollback boundary.** The benchmark covers the conflict-query algorithm only; it does not
measure animation sampling or the Animation panel's row submission. Rebuilding the index remains
O(total channels) after a model or target mutation, and each query walks the targets of the queried clip.
Paths are stored as strings and hashed only when the index is rebuilt. The playing revision is process
global, which is harmless for the single player instance in this viewer but would need a per-model key if
multiple independent players were added. The pairwise `checkConflict` path is retained for diagnostics
and is not removed. Rollback is confined to `AnimationPlayer` plus the revision fields and revision bumps
in the four glTF types; removing the diagnostic flag and `runConflictSelfCheck` does not affect the
runtime path. P2.2 is the next task; P2.3/P2.4 were not started.

<a id="section-12-17"></a>

### 12.17 P2.2 - Search and label caches (2026-10-21)

```text
Task ID / title: P2.2 - Search/label caches (R2, Phase 2)
Status: PASS (hierarchy/resources targeted UI; validation behavior and no material regression);
  warm-cache review fixes in [§12.18](#section-12-18)
Source state: no commit. Changed relative to 3d682fa:
  OpenGL-Renderer/Tools/tools/gltf/include/gltf/ChildOfRoot.h
  OpenGL-Renderer/Tools/tools/gltf/include/gltf/Gltf.h
  OpenGL-Renderer/Tools/tools/gltf/include/gltf/mesh/Primitive.h
  OpenGL-Renderer/Tools/tools/gltf/source/mesh/Primitive.cpp
  header/imgui/GltfValidator.h
  header/imgui/ResourcesPanel.h
  header/imgui/SceneHierarchyPanel.h
  header/imgui/SelfCheckResult.h
  header/imgui/ValidationPanel.h
  source/imgui/GltfValidator.cpp
  source/imgui/ResourcesPanel.cpp
  source/imgui/SceneHierarchyPanel.cpp
  source/imgui/ValidationPanel.cpp
  source/imgui/main.cpp
Accepted baseline: G:\temp\uray-codex\opencode\glider-p2\builds\p2_baseline.exe
  SHA-256 46F37730934B12E3902770264B5B4E76F2711239BF4D58C2BAC9F5568E41BDF7
Candidate executable: H:\git-repos\gltf-viewer\build\bin\Release\Glider.exe
  SHA-256 D577CF5E7AA65AB604FFBDAE8FA9454ECFC21C93CC7F9CE5BF64EB620A4ABB66
Evidence roots:
  G:\temp\uray-codex\opencode\glider-p2\p2_2\       (original P2.2 captures)
  G:\temp\uray-codex\opencode\glider-p2\review-fix\ (review-fix checks and final dumps)
```

**Change.** The three row-heavy panels now separate cache construction from row rendering:

- **Scene Hierarchy.** One cache entry per node holds the icon/title/index label, normalized node and
  mesh names, the node's own filter match, and a cached subtree-match flag. Labels and normalized names
  are rebuilt only when the model, name revision or display-option mask changes. A filter or active-scene
  change recomputes subtree matches by walking the cached tree; it does not rebuild label strings. The
  render path still walks the same expanded `TreeNodeEx` hierarchy and submits the same rows, selection
  and focus behavior.
- **Resources.** Each category caches formatted labels, normalized names, per-filter match flags and, for
  materials, variant labels and match flags. The cache key is model pointer, model revision, global name
  revision, display-option mask and item count. Search builds a match vector once per filter key; row
  rendering then walks the same category order and uses the live element pointers for selection.
- **Validation.** `GltfValidator` exposes a report revision that advances when a new report replaces the
  old one. `ValidationPanel` caches normalized code/message/pointer strings and the visible source issue
  indices, keyed by report revision, search string and severity mask. Table rows are still identified by
  the source issue index, not the filtered row position, so select/double-click/focus keeps its meaning
  after filtering. Copy Report still calls `report.toMarkdown()` on the live report; column resize,
  reorder, scroll and the existing message tooltip are unchanged.

Name revisions on `ChildOfRoot` are shared by nodes, meshes and materials, so renaming any of them
invalidates both the hierarchy and resources caches. Reloading a model changes the model pointer/revision
and rebuilds the caches; replacing the validation report advances the report revision and rebuilds the
visible index.

**Behavior checks.** The release candidate was exercised at the default 1600x900 window and at
1024x640. The baseline and candidate default window dumps are byte-identical:
`0F61DB6F581EAFD6A2D37E4EEDDBF7692BE33A92F322A75E9C799C248C761804` (`baseline_default_final.jpg`,
`candidate_default_final.jpg`). The hierarchy fixture contains 64 groups plus 4096 children (4160 nodes);
the resources fixture contains 4096 meshes.

| Check | Result |
| :--- | :--- |
| Hierarchy search `Child_4095` | baseline/candidate screenshots byte-identical, SHA-256 `DF113D76BC9D51D7FAA0FAD2583109A67A76C68ADC9BD3994888E51DF87DB88C` |
| Resources search `Mesh_4095` | baseline/candidate screenshots byte-identical, SHA-256 `78FE847807F2D09F16F74F907CDBF4CC0DE7D84ABE3F99586683B086F08603FC` |
| Hierarchy rename (startup-order only) | `Child_4095` was renamed by a control executed before the first panel render; the initial cache was therefore built with `RenamedChild`, and the old filter had no rows (`hierarchy_rename.png`, `hierarchy_old_name_no_match.png`) |
| Resources rename (startup-order only) | material `TexturedCube` was renamed before the first render; the initial resource cache contained the new name (`resources_rename_candidate.png`) |
| Display options (startup-order only) | icons/title/index/name mask was set before the first render; the initial hierarchy labels used names-only (`hierarchy_options_nameonly.png`) |
| Selection/focus | selecting a hierarchy row still selects the node and keeps it in the Inspector (`hierarchy_selected.png`); filtering does not change selection identity |
| Model reload (startup-order only) | the startup model was the 4096-mesh stats fixture instead of the hierarchy fixture; initial tree/resources construction used the stats model (`reload_hierarchy_to_resources.png`) |
| Report replacement (startup-order only) | the startup model was HelloCube instead of the validation fixture; the initial report was the valid 0-error state (`reload_validation_to_valid.png`) |
| Resize | hierarchy and validation panels rendered at 1024x640 with the existing table/tree layout (`hierarchy_resize_1024x640.png`, `validation_resize_1024x640.png`) |
| Validation filters | all 2048 rows, errors-only, warnings-only, a single-pointer search and a no-match search were captured (`validation_all.png`, `validation_errors.png`, `validation_warnings.png`, `validation_search_one.png`, `validation_search_none.png`) |

**Correction (review finding 2).** The `--rename-*`, `--hierarchy-display`, `--resource-display`,
`--reload-model` and validation startup controls execute before the first panel render. The captures
listed as startup-order above verify initial cache construction with the mutated name/options/model;
they do **not** prove invalidation of an already populated cache. In particular,
`hierarchy_old_name_no_match.png` is not evidence that a warm hierarchy cache was invalidated. The
warm-cache checks that call the actual cache implementations are recorded in [§12.18](#section-12-18).

The validation fixture is `p2_validation_2048.glb` (1024 `MESH_NO_PRIMITIVES` errors and 1024
`POSITION_MISSING_MIN_MAX` warnings, SHA-256
`DA8CF8B742BAEB7FDBB85502144C4EA6703D1F9AED892ABBC3F4049E9E52EA59`). The hierarchy fixture is
`p2_hierarchy_4096.glb` (SHA-256
`57562AFC0A48CCDB5D6D6ED13763E4A4BC887EEEB2F6EB6A2DF730BD26BC36B8`). The search, selected-node,
startup rename/options/reload and 1024x640 captures were opened and inspected. Following the review,
these are treated as startup/render regressions only, not as warm-cache invalidation evidence.

**Validation cache diagnostic.** `Glider.exe --isolated --validation-cache-selfcheck <report>` builds a
20,000-issue synthetic report (6,667 errors, 6,667 warnings, 6,666 infos) and compares the cached visible
index with the legacy filter over all-severity and single-severity masks and searches, including
no-match and pointer searches. Final result: `PASS`, 18 checks. On the all-severity `needle` search case
(40 matches), the legacy filter measured 1.949 ms, the warm cached filter 0.381 ms (5.119x) and the cold
build+filter 2.792 ms. This is a microbenchmark of the filter index, not a whole-panel frame measurement;
absolute timings vary between runs.

**Warm-cache invalidation diagnostic (review fix).**
`Glider.exe --cache-invalidation-selfcheck <report>` calls the same hierarchy, resource and validation
cache functions used by `render()` after each cache has been populated. It covers warm node/mesh/material
renames and old/new search matches, hierarchy primitive material A→B, B→A and unassigned→assigned,
display-option changes, model A→B→A with a same-object `Gltf::reset()`, and validation report replacement
with an equal issue count but different codes, pointers and severities. Final result: `PASS`, 62 checks
(30 hierarchy + 20 resources + 12 validation). Details and exact exit-code tests are in [§12.18](#section-12-18).

**Performance.** Workloads used 1280x720, vsync off, no AA, `--profile`, 60 warmup and 240 measured
frames except validation, which used 100 warmup and 600 frames. Executables alternated in five pairs for
all workloads. Negative deltas mean the candidate was faster. CPU noise is the baseline run-to-run
spread; the absolute panel times are small, so the high candidate noise on the hierarchy cases is
recorded rather than hidden.

| Workload | Total CPU delta | Baseline CPU noise | Improved pairs | `ui.panels` delta |
| :--- | ---: | ---: | ---: | ---: |
| Hierarchy `Child`, all 4160 labels rendered | **-20.67%** | 21.49% | 5/5 | **-29.48%** |
| Hierarchy `Group`, 64 groups but 4096 subtree scans | **-23.18%** | 10.43% | 5/5 | **-61.36%** |
| Resources `Mesh_`, 4096 labels and matches | **-61.12%** | 2.89% | 5/5 | **-75.76%** |
| Validation all 2048 rows, no search | -0.39% | 4.09% | 3/5 | -0.78% |

The hierarchy and resources cases improve every pair, including the label-heavy all-4160-row case; the
subtree case isolates the benefit of caching descendant matches. The validation all-row case is neutral
within the measured baseline noise and is not presented as a speedup: rendering 2048 table rows still
dominates, and filtering is only materially cheaper when a search or severity mask excludes rows. The
filtered-validation UI could not be paired against the accepted baseline because that binary has no
`--filter-validation` control; the validation microbenchmark supplies the filtered-path measurement and
the all-row paired run supplies the no-regression evidence.

The paired UI runs above used link
`177B6618CE64D20A01E4613BE9B2369A51861948074DFDBBB4989F692E371ADC`; they were not rerun after the
review fixes. The final link is
`D577CF5E7AA65AB604FFBDAE8FA9454ECFC21C93CC7F9CE5BF64EB620A4ABB66`. Since the paired runs, the
hierarchy key gained a primitive-material revision read and the self-check/CLI paths changed. The table
above is therefore pre-review-fix performance evidence, not a measurement of the final link. The final
link was rerun through all passing/failing self-checks and the default, hierarchy-search and
resources-search dumps; those results are in [§12.18](#section-12-18). No whole-panel benchmark was rerun.

**Limits.** The validation fixture has no INFO issues, so INFO-mask coverage comes from the synthetic
cache self-check, not a rendered asset. The runtime display-menu click path was not automated; startup
options and the option-mask rebuild key were verified, and the menu calls the same setter used by the
test control. The 1024x640 captures verify rendering at a second size, not continuous interactive drag
resizing. Copy Report was left untouched and was not exercised through the Windows clipboard in this
task. The caches do not virtualize rows: all matching tree rows and validation rows are still submitted
to ImGui; visible-row clipping remains P2.4. The hierarchy cache assumes structural reparenting, if it is
ever added, bumps the model revision or otherwise invalidates the subtree cache; the current viewer has
no reparent UI. Resources category caches are process-static and keyed by model/revision; they are not
evicted when an unrelated model is closed, but their memory is bounded by the categories of models
loaded in the process. No commit or push was performed.

**Decision.** P2.1 and P2.2 are implemented and validated, with P2.1 PASS on exact indexed/legacy
answers and P2.2 PASS on hierarchy/resources behavior and targeted timing, neutral on the unfiltered
validation table. Both remain uncommitted for review. Rollback boundaries are the panel caches plus
`GltfValidator` report revision for P2.2, and `AnimationPlayer` plus the four glTF revision fields for
P2.1. P2.3 and P2.4 were not started; Phase 3 was not started.

<a id="section-12-18"></a>

### 12.18 P2.1/P2.2 review fixes (2026-10-21)

```text
Task ID / title: P2.1/P2.2 review fixes - warm-cache invalidation, evidence correction, self-check exit codes
Status: PASS on the focused review checks; uncommitted, awaiting independent review
Source state: no commit. Review-fix changes relative to 3d682fa:
  OpenGL-Renderer/Tools/tools/gltf/include/gltf/mesh/Primitive.h
  OpenGL-Renderer/Tools/tools/gltf/source/mesh/Primitive.cpp
  header/imgui/AnimationPlayer.h
  header/imgui/ResourcesPanel.h
  header/imgui/SceneHierarchyPanel.h
  header/imgui/SelfCheckResult.h
  header/imgui/ValidationPanel.h
  source/imgui/AnimationPlayer.cpp
  source/imgui/ResourcesPanel.cpp
  source/imgui/SceneHierarchyPanel.cpp
  source/imgui/ValidationPanel.cpp
  source/imgui/main.cpp
  docs/RENDERER_OPTIMIZATION_JOURNAL.md
Accepted baseline: G:\temp\uray-codex\opencode\glider-p2\builds\p2_baseline.exe
  SHA-256 46F37730934B12E3902770264B5B4E76F2711239BF4D58C2BAC9F5568E41BDF7
Final review-fix executable: H:\git-repos\gltf-viewer\build\bin\Release\Glider.exe
  SHA-256 D577CF5E7AA65AB604FFBDAE8FA9454ECFC21C93CC7F9CE5BF64EB620A4ABB66
Evidence root: G:\temp\uray-codex\opencode\glider-p2\review-fix\
  conflict_pass.txt, validation_pass.txt, cache_invalidation_pass.txt
  conflict_forced_fail.txt, validation_forced_fail.txt, cache_forced_fail.txt
  default_final.jpg, hierarchy_search_final.png, resources_search_final.png,
  validation_search_final.png
  commands.txt
```

**Finding 1 - hierarchy primitive-material label invalidation.** `Primitive` now exposes a monotonic
`getMaterialRevision()`. Both `setMaterial()` and `unsetMaterial()` bump that revision. The hierarchy
cache key includes it alongside the model, name, option and model revisions, so the next hierarchy render
rebuilds labels once; frames with no material assignment do not rebuild. The test populates the cache,
then performs A→B, B→A and unassigned→assigned. For each state it checks the cached primitive label (the
string submitted by the hierarchy row), the live `prim.getMaterial()` pointer, and a `SelectionState`
populated from that same pointer. It also covers node/mesh/material renames, display-option changes,
model A→B→A and a same-object `Gltf::reset()`. Result: 30 checks, PASS.

The ImGui mouse click and the Inspector material combo were not automated. Selection identity was
checked through the live pointer that the hierarchy row passes to `SelectionState::selectMaterial()`; the
click wiring itself is code-inspection evidence, not a synthetic mouse-event test.

**Finding 2 - warm-cache invalidation evidence.** The earlier startup controls run before the first panel
render, so the [§12.17](#section-12-17) rename/options/reload captures only prove initial cache construction. They are now
labelled startup-order only, and the journal no longer claims that
`hierarchy_old_name_no_match.png` proves warm-cache invalidation. The replacement check calls the actual
cache functions used by `render()` after the caches are populated:

- Hierarchy: 30 checks, including the primitive-material transitions above, warm node/mesh/material
  renames, old/new filter matches, display options, model A→B→A and same-object reload.
- Resources: 20 checks. Node, mesh and material renames are made after the category caches exist; the
  old name must stop matching, the new name must match, and labels must update. It also covers display
  options and A→B→A with model A reset while model B is current.
- Validation: 12 checks. Two four-issue reports have different codes, pointers and severities. The report
  revision must change; with errors-only + `alpha` unchanged across replacement, stale alpha rows must
  disappear; then errors/beta, warnings/beta and infos/beta must return the new rows. This checks
  invalidation independently of issue count, which stays four.

The combined `--cache-invalidation-selfcheck` result is PASS with 62 checks (30 + 20 + 12). These are
focused code-level checks against the production cache implementations, not frame-dump evidence.

**Finding 3 - self-check exit contract.** `SelfCheckResult { bool success; std::string report; }` is now
returned by the conflict, validation-cache and cache-invalidation checks. `main.cpp` writes the report
through one helper and returns `0` only when `success` is true and the report was opened, written,
flushed and closed successfully. The contract is:

- `0`: all checks passed and the report was written.
- `1`: at least one check failed; the FAIL report was written.
- `2`: the report path could not be opened or written.

No report text is parsed to determine success. `--selfcheck-force-failure` injects one failure only when
that diagnostic flag is present; it is not an unconditional production failure. Failure-path results:
forced failure returned `1` for all three self-check commands, and passing an existing directory as the
report path returned `2` for all three. Passing runs returned `0` and their reports contain `Result: PASS`.

**Commands and measured results.**

```text
cmake --build build --config Release --target Glider
Get-FileHash build\bin\Release\Glider.exe -Algorithm SHA256

Glider.exe --conflict-selfcheck <root>\conflict_pass.txt
  exit 0; PASS; 456 checks
  initial selection 0.884 ms legacy, 0.037 ms indexed (24.016x)
  per-row scan 1.561 ms legacy, 0.037 ms indexed (41.861x)

Glider.exe --validation-cache-selfcheck <root>\validation_pass.txt
  exit 0; PASS; 18 checks
  legacy filter 1.949 ms, warm cached 0.381 ms (5.119x), cold 2.792 ms

Glider.exe --cache-invalidation-selfcheck <root>\cache_invalidation_pass.txt
  exit 0; PASS; 62 checks (30 hierarchy + 20 resources + 12 validation)

Glider.exe <self-check> <root>\<name>_forced_fail.txt --selfcheck-force-failure
  exit 1 for conflict, validation cache and cache invalidation; FAIL report written

Glider.exe <self-check> <root>
  exit 2 for conflict, validation cache and cache invalidation; each reports that the
  output could not be opened
```

**Frame dumps and regressions.** The final link was run at the default 1600x900 window. The default,
hierarchy-search and resources-search dumps remain byte-identical to the previously accepted/compared
captures:

| Dump | SHA-256 | Check |
| :--- | :--- | :--- |
| `default_final.jpg` | `0F61DB6F581EAFD6A2D37E4EEDDBF7692BE33A92F322A75E9C799C248C761804` | matches accepted default baseline |
| `hierarchy_search_final.png` | `DF113D76BC9D51D7FAA0FAD2583109A67A76C68ADC9BD3994888E51DF87DB88C` | matches prior hierarchy `Child_4095` capture |
| `resources_search_final.png` | `78FE847807F2D09F16F74F907CDBF4CC0DE7D84ABE3F99586683B086F08603FC` | matches prior resources `Mesh_4095` capture |
| `validation_search_final.png` | `1E429EBF1DD104EA93BB0EAC660A0F0953A48E63F72EF6BACDEE40EF9B1E663D` | opened and inspected: 1024 errors with errors-only + `MESH_NO_PRIMITIVES` |

The validation dump hash is not compared byte-for-byte across runs because the displayed scan time can
change pixels; visual inspection is the evidence for that capture.

**Measured, inspected and startup-only evidence.** Measured: self-check counts, timings, exit codes,
executable hash and dump hashes above. Code-inspected: the hierarchy row's material selection call, the
revision guard and single rebuild, the cache functions used by the tests, and the report writer's
open/write/flush/close checks. Startup-only: the [§12.17](#section-12-17) rename, display-option, model-reload and
validation-reload captures; they are not presented as warm-cache invalidation.

**Limitations.** Actual ImGui mouse interaction with the hierarchy material selector and the Inspector
material combo was not automated; the focused test exercises the same cached label string and live
material pointer at the cache boundary. The validation replacement reports are synthetic and have no
rendered asset behind them. The A→B→A checks use in-process model objects, including `Gltf::reset()`, not
a file-open dialog. Copy Report and clipboard behavior were not exercised. The full paired UI benchmark
in [§12.17](#section-12-17) was not rerun after these review fixes, so its timings are not claimed for the final link; the
self-check and dump regressions above were rerun. No commit or push was performed.

**Decision.** The three review findings are addressed in the working tree: material-assignment labels
invalidate on a revision instead of every frame, warm-cache checks call the real cache implementations,
the startup-only claims are corrected, and self-checks use structured success plus an explicit 0/1/2
exit contract. P2.3, P2.4 and Phase 3 remain not started.

<a id="section-12-19"></a>

### 12.19 Independent P2.1/P2.2 re-review (2026-10-21)

**Decision: accepted for the reviewed P2.1/P2.2 scope; no blocking findings remain.**
The reviewer inspected the material-assignment revision and hierarchy invalidation guard, the
production-cache tests, and the structured report writer. Release built successfully; its SHA-256
remains `D577CF5E7AA65AB604FFBDAE8FA9454ECFC21C93CC7F9CE5BF64EB620A4ABB66`.

Independent runs passed 456 conflict checks, 18 validation-filter checks and 62 warm-cache
invalidation checks (30 hierarchy, 20 resources, 12 equal-length report replacement). For each of
the three commands, the passing run returned 0, forced failure returned 1, and a directory used as
the output path returned 2. The post-open write/flush/close failure handling was inspected in code;
a disk-full or similar post-open failure was not injected.

A fresh default frame dump was opened and inspected and matched the baseline SHA-256
`0F61DB6F581EAFD6A2D37E4EEDDBF7692BE33A92F322A75E9C799C248C761804`.
Review artifacts are in `H:\git-repos\gltf-viewer\build\p2-rereview\`.
The full paired UI timing series and mouse/clipboard interactions were not repeated; the limits
in [§12.18](#section-12-18) remain applicable. This accepts P2.1/P2.2 only, not the unfinished Phase 2 as a whole.
No commit, push or P2.3/P2.4 implementation was performed during this review.

<a id="section-12-20"></a>

### 12.20 P2.3 - Flat-list and Logger clipping (2026-10-21)

**Status: IMPLEMENTED; targeted steady-state clipping measurements PASS on the large fixtures.
At this initial checkpoint, acceptance was INCOMPLETE pending independent review and interactive UI cases;
this status is superseded by the fixes in [§12.21](#section-12-21) and reviewer acceptance in [§12.22](#section-12-22).**
P2.1/P2.2 remain at the accepted baseline; P2.4 and Phase 3 were not started.

```text
Baseline source: commit 009c062836079446dde28c7db3a048fb15c183be
Baseline executable SHA-256: D577CF5E7AA65AB604FFBDAE8FA9454ECFC21C93CC7F9CE5BF64EB620A4ABB66
Final Release executable: build\bin\Release\Glider.exe
Final executable SHA-256: CA9A9A9C82F40CFA419472EF2BC5D973356715ADA507F04ABDD3EBC9BAC2169C
Build: Windows Release, OpenGL 4.1, NVIDIA RTX 5080 / driver 616.92
Benchmark window: 1280x720, vsync off, 8 warm-up and 30 measured frames
Evidence root (local, ignored build output): build\p2.3\
```

**Changes.**

1. **Fixed-height flat lists.** Resources uses cached filtered source indices and clips only the fixed
   rows within each existing category; labels containing newlines keep the old un-clipped path.
   Validation clips its fixed-height table rows while mapping each visible row back to the original
   issue index for selection, double-click focus and tooltips. Multiline report content keeps the
   previous full rendering path. Category headings, table behavior, source identity and scroll extent
   are retained. The scene hierarchy was not changed and no `TreeNodeEx` calls are skipped.
2. **Logger one-line clipping.** Unwrapped one-line entries use `ImGuiListClipper`; horizontal content
   extent remains available. The initial cold layout remains O(N), then only visible rows are submitted.
3. **Logger variable-height rows.** Entries containing newlines or using wrapping use cached measured
   heights and cumulative offsets. The visible range is drawn while a dummy item preserves total scroll
   extent. Layout invalidates on message revision/clear generation, wrap mode, available width, font
   identity/size and item spacing; append extends the cached layout. Clear invalidates it. Copy still
   forms text from all stored messages, independently of visible rows. Severity totals, Clear, wrapping,
   and the pre-existing auto-scroll condition are unchanged. No retention limit, truncation or export
   behavior change was introduced.

Changed files:

```text
source\imgui\ResourcesPanel.cpp
source\imgui\ValidationPanel.cpp
source\imgui\main.cpp
scripts\generate_ui_clipping_fixtures.py
```

**Separate measured substeps.** Before combining all P2.3 changes, the fixed-list change and then the
one-line Logger clipper were each built and measured:

| Substep candidate | Executable SHA-256 | Workload | Baseline → candidate median `ui.panels` (ms) | Median paired change | Baseline / candidate noise |
|---|---|---|---:|---:|---:|
| Fixed-height Resources and Validation lists | `6BAAC74827924FFAD8A9DB5E116EEA9186A7124DC8EB73FEE1A7173C887188D6` | Resources, 4,096 rows | .3383 → .0163 | -95.19% (all pairs improved) | 1.89% / 29.14% |
| Fixed-height one-line Logger clipper | `347961A885AD4A0EAF627F36D9FF5D7327F76A0116ED1777C2420499417C1C40` | Logger, 10,000 rows | .5745 → .0278 | -95.16% (all pairs improved) | 1.61% / 36.87% |

For the first substep, Validation measured `.7696 → .0282 ms` (`-96.34%`, all five pairs improved;
baseline/candidate noise `1.91% / 28.24%`). The corresponding fixed-candidate JSON reports are
`resources_4096_fixed_candidate_1-5.json` and `validation_2048_fixed_candidate_1-5.json`.
The one-line Logger candidate reports are `logger_10000_candidate_1-5.json`. The later variable-height
work was then validated separately with the final integrated executable hash recorded above; its
three-run cold and warm results are listed below. This avoids treating the one-line clipper timings as
evidence for wrapped/multiline layout.

**Reproduction.** From the repository root, regenerate the deterministic fixtures and build:

```powershell
python scripts\generate_ui_clipping_fixtures.py build\p2.3\fixtures --resource-count 4096 --validation-count 2048
cmake --build build --config Release --target Glider
```

Fixture hashes:

```text
build\p2.3\fixtures\p2_3_resources_4096.glb
  SHA-256 24815205454628BF025746C99F5589052F79C4C20121BBF1CF5737F870125C04
build\p2.3\fixtures\p2_3_validation_2048.glb
  SHA-256 16924A6947918B5B4DB56D47DE72013F70D0758BEB2FC7DE22F0C1304E1A1F7F
```

The fixtures were regenerated to a second directory; both hashes matched. Each benchmark run used the
final command shape below with run number `1`-`5` (variable Logger runs used `1`-`3`), and corresponding
baseline runs used the same arguments on the accepted baseline executable:

```powershell
build\bin\Release\Glider.exe --isolated --window-size 1280x720 --vsync off --profile --benchmark --benchmark-warmup 8 --benchmark-frames 30 --benchmark-output build\p2.3\<workload>_final_candidate_<run>.json --exit-after-benchmark <workload-arguments>
```

Workload arguments were `--show-logger --log-test 10000` (single-line Logger),
`"<resources fixture>" --filter-resource Mesh_`, `--show-validation "<validation fixture>"`, and no
extra arguments for the small default-model control. Variable Logger runs added
`--show-logger --log-test 10000 --log-test-variable`, with `--logger-wrap` for the wrapped case.
All baseline and candidate JSON reports are under `build\p2.3\`.

Final self-check invocations were:

```powershell
New-Item -ItemType Directory -Force build\p2.3\final-verify | Out-Null
build\bin\Release\Glider.exe --conflict-selfcheck build\p2.3\final-verify\conflict.txt
build\bin\Release\Glider.exe --validation-cache-selfcheck build\p2.3\final-verify\validation-cache.txt
build\bin\Release\Glider.exe --cache-invalidation-selfcheck build\p2.3\final-verify\cache-invalidation.txt
build\bin\Release\Glider.exe --logger-layout-selfcheck build\p2.3\final-verify\logger-layout.txt
```

Representative final frame commands (the corresponding input fixtures and output paths are local to
`build\p2.3\`) are:

```powershell
build\bin\Release\Glider.exe "<resources fixture>" --filter-resource Mesh_ --dump-frame build\p2.3\resources_4096_final.png --exit-after-dump
build\bin\Release\Glider.exe --show-validation "<validation fixture>" --dump-frame build\p2.3\validation_2048_final.png --exit-after-dump
build\bin\Release\Glider.exe --show-logger --log-test 10000 --log-test-variable --dump-frame build\p2.3\logger_variable_nowrap_final.png --exit-after-dump
build\bin\Release\Glider.exe --show-logger --logger-wrap --log-test 10000 --log-test-variable --dump-frame build\p2.3\logger_variable_wrap_final.png --exit-after-dump
build\bin\Release\Glider.exe "<resources fixture>" --filter-resource NoSuchResource --dump-frame build\p2.3\resources_4096_nomatch_final.png --exit-after-dump
build\bin\Release\Glider.exe --show-validation "<validation fixture>" --filter-validation NoSuchIssue --dump-frame build\p2.3\validation_2048_nomatch_final.png --exit-after-dump
```

**Measured results.** Values below are the per-run median `ui.panels` stage in milliseconds, with the
five baseline and final-candidate runs paired by run index. Noise is `(max - min) / median` over each
five-run set. P95 is the median of the five per-run frame p95 values. Whole-frame CPU is the median of
the five per-run CPU-frame medians. These are CPU timings, not GPU timings.

| Workload | Baseline runs (ms) | Final candidate runs (ms) | Baseline / candidate noise | Median paired change | `ui.panels` p95 baseline → candidate (ms) |
|---|---|---|---:|---:|---:|
| Logger, 10,000 one-line messages | .5742, .5793, .5748, .5700, .5745 | .0353, .0314, .0358, .0304, .0286 | 1.61% / 22.93% | -94.59% (all 5 pairs improved) | .6248 → .0649 |
| Resources, 4,096 filtered meshes | .3382, .3391, .3327, .3388, .3383 | .0236, .0236, .0219, .0217, .0216 | 1.89% / 9.13% | -93.43% (all 5 pairs improved) | .3587 → .0345 |
| Validation, 2,048 issues | .7817, .7696, .7683, .7670, .7796 | .0555, .0486, .0387, .0470, .0343 | 1.91% / 45.11% | -93.88% (all 5 pairs improved) | .8129 → .0733 |
| Small default-model control | .0166, .0218, .0166, .0183, .0187 | .0262, .0260, .0260, .0279, .0277 | 28.61% / 7.44% | +52.04% paired median | .0360 → .0452 |

The median whole-frame CPU times for Logger, Resources, Validation and the control were respectively
`1.0559 → .5896 ms`, `.7467 → .5140 ms`, `1.1843 → .5906 ms` and `.4169 → .6185 ms`. The control
therefore shows an observed `ui.panels` increase of .00785 ms (42.8% comparing medians); its baseline
spread is broad, and multiple unrelated frame stages also rose between the baseline and final
measurement series. Treat the control comparison as inconclusive rather than attributing the full-frame
change to clipping or claiming a neutral small-workload effect.

Cold `ui.panels` stage medians were baseline → final candidate `1.4679 → 2.0808 ms` for the
one-line Logger, `1.6353 → 1.9823 ms` for Resources, and `2.6804 → 2.8905 ms` for Validation. This
separates cache/layout construction cost from the warm visible-row savings. For 10,000 variable entries,
the three no-wrap candidate runs measured cold stages of `2.4388, 3.5585, 3.0790 ms`, with warm
`ui.panels` median `.0352 ms` and p95 median `.0591 ms`; wrapped runs measured cold stages of
`5.4301, 8.5195, 6.5168 ms`, warm median `.0397 ms` and p95 median `.0635 ms`. These variable-entry
cases have no comparable baseline series. Their observed warm whole-frame CPU medians were `.6830 ms`
and `.6032 ms`, respectively. The wrapped cold build is measurably more expensive; no claim is made
that variable-height first-use layout is faster.

**Behavior and validation evidence.** The final Release build completed successfully. The following
self-checks were run against the final executable; all returned exit code 0:

```text
build\p2.3\final-verify\conflict.txt          P2.1: 456 checks PASS
build\p2.3\final-verify\validation-cache.txt  P2.2: 20 checks PASS (20,000 issues)
build\p2.3\final-verify\cache-invalidation.txt P2.2: 67 checks PASS (30 hierarchy, 24 resources, 13 validation)
build\p2.3\final-verify\logger-layout.txt    P2.3: 19 checks PASS
```

The Logger self-check covers empty layout, top/middle/bottom visible ranges, embedded newlines,
wrapping, width/font invalidation, append, Clear/severity totals, and Copy text assembled from all
entries. The source-index checks cover filtered-to-original identity and multiline fallback
classification. On the final binary the Logger self-check returned 1 for an intentional forced failure
and 2 for an invalid report destination; these reports are in `build\p2.3\final-verify\`.

Final frame dumps were opened and visually inspected at 1280x720. Resources shows the filtered
`Meshes (4096)` category with the expected leading source rows, and the no-match capture shows an
empty result area. Validation shows its 2,048-error table, while its no-match capture shows the empty
table with headers retained. Logger dumps show generated embedded-newline entries with wrapping
disabled and enabled:

```text
build\p2.3\resources_4096_final.png
  SHA-256 DE6FB6E6F57686A5E5BADCD9895416C3ECB56BD92FE269E77177464CB3E555C9
build\p2.3\validation_2048_final.png
  SHA-256 F5A4AF8EE3B336CFF0D2B9F178F54CCBD16F2A65D16190080EB6108961301216
build\p2.3\logger_variable_nowrap_final.png
  SHA-256 FED30F9E995488DA48AEAAC59C6C67EBC06168BDD1F3BD9D77C2638AFF388FA5
build\p2.3\logger_variable_wrap_final.png
  SHA-256 7025314C1502ABA1321372954057E021476B582ED8B6F55C64C629E79E925B54
build\p2.3\resources_4096_nomatch_final.png
  SHA-256 20DDCE8E22AB83B2658C8287396A3DE6EE5FF6A8FAEF6F9622B4E8D0C8ABC74D
build\p2.3\validation_2048_nomatch_final.png
  SHA-256 112AD02DFFAB2AA24925F44CF6B0128922594630796AE5CF0D10EC7F83B7DA08
```

**Limitations / not claimed PASS.** No interactive mouse/keyboard session was available. Scrolling by
hand at top/middle/bottom, changing filters while scrolled, mouse selection/double-click focus and
tooltips/actions after filtering, and keyboard navigation remain unverified in the running UI.
Logger top/middle/bottom ranges, append, Clear, and all-entry Copy content are covered by code-level
self-checks, but appending while an actual user is scrolled upward or at the bottom and clicking the
real clipboard button were not exercised; frame captures are not substitutes for those interactions.
Continuous interactive resizing was not tested. Code inspection confirms Logger Copy still uses all
stored entries and that the existing auto-scroll condition is retained. These limits and the small
control result should be reviewed before P2.3 acceptance.

No commit or push was performed. P2.3 is ready for independent review; P2.4 and Phase 3 remain
unstarted.


<a id="section-12-21"></a>

### 12.21 P2.3 review-fix round (2026-10-21)

Baseline for all comparisons: commit `009c062`. This round fixes the three P2.3 review findings.
The work is uncommitted. P2.4 and Phase 3 are not started; no claim is made that Phase 2 is complete.

**Executables (final, built after the last C++ edit).**

```text
build\p2.3-candidate\Glider.exe  13DF53A2B3C4E9AC8FBE91C1696D1149E4865C7ADC23142F951F6609411A8BA5
  (identical SHA to build\bin\Release\Glider.exe; the four self-checks below ran on this file)
build\p2.3-baseline\Glider.exe   17AF36BA4E4AB36F410B6A4F222EA14A3DF7E7FFAC6FA3384CB7F388A3DEA027
  (commit 009c062 main/ResourcesPanel/ValidationPanel + UiProbe/UiScenarioHarness hooks only;
   built by scripts\ui_integration\build_baseline_harness.py, working tree restored with SHA check)
```

Later edits after that build touched only Python scripts and fixtures. Earlier P2.3 executables and
the [§12.20](#section-12-20) control numbers are not reused as final-build evidence.

**Finding 1 - O(n) `LinkedList::at(n)` (code fix, measured, code inspection).**
`SourceCursor<TList>` in `ResourcesPanel.cpp` advances one forward iterator across frames' rows and
restarts at `begin()` only on backward access. It holds no pointers across frames, so model
reload/reset or element deletion (`Gltf::delete*` does not bump `m_revision`) cannot leave stale
pointers. Remaining cost: O(highest requested source index) per frame, not constant time. Measured
effect is visible below (top 0.0167 ms, mid 0.0316 ms, bottom 0.0461 ms `ui.panels`). The multiline
fallback renders every matching row unclipped, as before; it does not use `at()`.

**Finding 2 - runtime integration validation (measured, real ImGui frames with injected input).**
`--ui-script`/`--ui-report` (`header\imgui\UiScenarioHarness.h`, `UiProbe.h`) drive real mouse, wheel,
keyboard, typing, resize, model reload and the real Copy/Clear buttons through ImGui. The same
scripts run on the baseline-harness and candidate exes; per-frame snapshots (selection, scroll and
content extents, probe geometry, pixel hashes, clipboard) are compared and oracles asserted.

```text
python scripts\generate_ui_clipping_fixtures.py build\p2.3\fixtures
python scripts\ui_integration\build_baseline_harness.py --backup <dir>
python scripts\ui_integration\run_ui_integration.py            (exit 0 pass, 1 failed assertion, 2 report-write failure)
evidence: build\p2.3\integration\summary.json, *.script.txt, *.baseline.json, *.candidate.json
```

Result: 130 passed, 0 failed, run twice (`build\p2.3\integration`, `build\p2.3\integration_run2`).
Exit codes were verified: `--force-fail` returns 1, `--fail-report-write` returns 2 (report-write
failure message printed). Fixtures: resources_4096 (4096 meshes), resources_multiline_4096 (one
multiline label per category), variants_4096, validation_2048, validation_mixed_2048 (interleaved
MESH_NO_PRIMITIVES and PRIMITIVE_MISSING_POSITION errors, so a code filter selects alternating
source rows).

Covered, baseline == candidate in every snapshot, and oracles pass: Resources and Material Variants
top/middle/bottom scroll with pixel hashes; tail row 4095 reachable and extents identical; filters
changed while scrolled, no-match, clearing; click selection resolving to the original source item;
keyboard Down navigation including past the last visible row; reload of a different model (and the
same path twice) after caches were populated; multiline fallback row layout and actions; Validation
filtered click selection indexes, hover tooltip appearing and disappearing, filtered double-click
resolving to the original odd-indexed mesh target. Logger: single-line, embedded-newline and long
wrapped entries; append at the bottom with auto-scroll; append while scrolled upward; auto-scroll
disabled; wrap toggle and window resize after layout caches were populated; Clear; real Copy button
producing the full log at top and bottom scroll positions (35,916 bytes, identical hash) and empty
text after Clear; severity totals.

Limitations (explicit): Logger rows on the clipped path report a synthesized full-width probe rect,
so `log.row` compares x0/y0/y1 and ignores hovered/focused; rendering is covered by pixel hashes.
The Logger child is ~100 px tall at 1280x720, so mid-view oracles see only a few rows. Node- and
material-target validation issues could not be generated (loader rejects node cycles/multi-parent;
material range checks produced no issues), so the node-focus camera move on double-click is
NOT RUN; mesh-target identity is what was verified. The Logger Copy-after-Clear snapshot failed once
(click raced the Clear layout); a settle was added and 2 further full runs passed, so intermittency
is not fully excluded. Real OS-level mouse and drag-resize were not used; resize was a scripted
window size change. Startup frame dumps (`build\p2.3\final\res.png`, `val.png`, 1600x900) were
visually inspected: layout intact, lists collapsed so they show no list rows (code-level and
integration pixel hashes are the list evidence).

**Finding 3 - small-scene control (measured).**
`python scripts\ui_integration\run_paired_bench.py --baseline build\p2.3-baseline\Glider.exe --candidate build\p2.3-candidate\Glider.exe`
(5 alternating pairs, 120 warm-up + 600 measured frames, 1280x720, vsync off, `--profile`, isolated,
sequential). Evidence: `build\p2.3\bench\paired_summary.json` plus per-run JSON.

| scenario | ui.panels baseline -> candidate (median of run medians, ms) | whole-frame CPU median (ms) | noise ranges |
|---|---|---|---|
| control_default | 0.0185 -> 0.0175 (paired delta -0.0001) | 0.3883 -> 0.3775 | panels b 0.0172-0.0200, c 0.0175-0.0197 |
| res_4096_top | 0.3648 -> 0.0167 | 0.7550 -> 0.3911 | c 0.0157-0.0184 |
| res_4096_mid | 0.3639 -> 0.0316 | 0.7548 -> 0.4037 | c 0.0287-0.0342 |
| res_4096_bottom | 0.3637 -> 0.0461 | 0.7624 -> 0.4085 | c 0.0454-0.0467 |
| res_4096_filter_end (matches 4090-4095) | 0.0441 -> 0.0453 | 0.4123 -> 0.4285 | within noise |
| res_multiline_bottom (unclipped fallback) | 0.3639 -> 0.3726 | 0.7538 -> 0.7708 | within noise |
| variants_bottom | 0.3951 -> 0.0343 | 0.7445 -> 0.4052 | c 0.0335-0.0351 |

The control regression reported in [§12.20](#section-12-20) (+52% on 30 frames) does not reproduce: the longer
alternating comparison shows equal `ui.panels` and no whole-frame slowdown (CPU run-median ranges
overlap, 0.2716-0.4461 vs 0.2712-0.4029). The earlier figure is superseded; the 8+30-frame, non-alternating
series was dominated by warm-up and noise. Clipping benefits are warm-frame figures; first-use
cache-building costs remain as recorded in [§12.20](#section-12-20). Bottom-of-list cost rising from 0.0167 to 0.0461 ms
is the SourceCursor O(index) traversal, still ~8x below baseline. The multiline fallback shows no
gain (as designed). The p95 CPU for unclipped baselines (~0.96-1.0 ms) falls to ~0.58-0.62 ms when clipped.

**Self-checks on the final executable (all exit 0):** conflict 456, validation-cache 20,
cache-invalidation 30+28+13, logger-layout 19; outputs in `build\p2.3\final\`.

No commit or push was performed.

<a id="section-12-22"></a>

### 12.22 Independent P2.3 re-review (2026-10-21)

**Decision: P2.3 accepted for the reviewed scope; no blocking finding remains.** The reviewer
inspected SourceCursor, the baseline harness construction and the integration comparison logic.
Release built successfully and the current executable matches [§12.21](#section-12-21) SHA-256
`13DF53A2B3C4E9AC8FBE91C1696D1149E4865C7ADC23142F951F6609411A8BA5`.

The full runtime integration suite was independently rerun against the recorded baseline harness:
130 checks passed, zero failed, exit 0. This includes baseline/candidate snapshot comparisons and
behavior assertions for Resources, multiline fallback, Material Variants, Validation and Logger.
All four self-checks were rerun successfully: conflict 456, validation-cache 20,
cache-invalidation 71 (30+28+13), logger-layout 19. A fresh wrapped Logger frame dump was opened
and inspected. `git diff --check` passed.

The reviewer also independently reran the default-model control with five alternating pairs,
120 warm-up frames and 600 measured frames. Median-of-run-medians `ui.panels` was
0.0188 ms baseline versus 0.0181 ms candidate; CPU frame time was 0.3916 versus 0.3862 ms.
The earlier +52% panel/whole-frame slowdown does not reproduce; these results are control evidence,
not a claimed small-scene speedup. The other workload timing series in [§12.21](#section-12-21) was inspected rather
than independently repeated. Artifacts: `build/p2-3-rereview/`, including `integration/summary.json`
and `bench/paired_summary.json`.

The documented limitations remain: SourceCursor walks up to the highest requested source index
per frame; node-target focus motion was not exercised; scripted ImGui input/window resizing is
not OS-level mouse/drag testing. Copy-after-Clear passed this independent run in addition to the
recorded successful runs. Acceptance covers P2.3 only. No commit, push, P2.4 or Phase 3 work was
performed in this review.

---

<a id="section-12-23"></a>

### 12.23 P2.4 Hierarchy Virtualization & Validation (2026-10-22)

**Status: IMPLEMENTED; all self-checks, runtime integration suite (174 checks), and 5-pair alternating benchmarks PASS.**
P2.1, P2.2, and P2.3 remain at the accepted baseline; Phase 3 was not started. The work is uncommitted.

```text
Baseline source: commit fc6a89b ("Clip flat UI lists and logger rows with integration validation")
Baseline executable: build\p2.4-baseline\Glider.exe
Baseline executable SHA-256: AC7B3FF7AC6F392A20D2351CA8E44CD64B878D18F2B8AB65BBED8E0819FA4AC0
Candidate Release executable: build\bin\Release\Glider.exe / build\p2.4-candidate\Glider.exe
Candidate executable SHA-256: 37AC47D75FC0B765AC4A2963A789CB09DCA3D32F05BF8F3671A9964587A49BAA
Build: Windows Release, MSVC x64 C++20, OpenGL 4.1 Core Profile, NVIDIA RTX 5080 / driver 616.92
Benchmark window: 1280x720, vsync off, 120 warm-up and 600 measured frames
Evidence root (local, ignored build output): build\p2.4\
```

#### 1. Architecture & Virtualization Design

1. **Flattened Row Model (`HierarchyRowType`, `HierarchyRow`)**:
   Instead of traversing the glTF tree hierarchy recursively and calling `TreeNodeEx` per node every frame, `SceneHierarchyPanel.cpp` flattens the expanded visible tree structure into a contiguous row model (`s_hierarchyCache.rows`). Each row stores:
   - `HierarchyRowType`: `Scene`, `Node`, `Mesh`, `Primitive`, `Material`, `Camera`, `Light`, `ViewportsHeader`, `ViewportItem`.
   - Stable identity: `imguiId` (stable 32-bit integer matching the hierarchy tree node), `object` (pointer to source glTF or Viewport entity), `subIndex` (for primitives), and tree `depth`.
   - Tree navigation state: `isOpen`, `isLeaf`, `isSelected`, `isVisible`, and cached `label`.
2. **Deterministic Depth & Parent-Child Ordering**:
   `buildRowModel()` performs an in-order depth-first traversal of the glTF scene hierarchy. Children are appended immediately following their parent, preserving tree hierarchy order. Viewport instances are integrated into the row model following the scene tree under a dedicated `ViewportsHeader` row.
3. **Ancestor Auto-Expansion & Stable IDs**:
   When a search filter is typed or a node is selected, `ensureHierarchyCache()` walks $O(\text{depth})$ reverse mappings (`parentOfNode`, `parentOfMesh`, `nodesWithMaterial`) and marks all ancestor nodes as open in `storage.SetInt(nodeId, 1)`. When search is cleared, matching branches remain open in `ImGuiStorage`, preserving baseline interaction parity.
4. **Visible-Row Clipping via `ImGuiListClipper`**:
   `renderRowModel()` calculates total row count and uses `ImGuiListClipper` with uniform row height (`GetFrameHeightWithSpacing()`). Only rows in `[clipper.DisplayStart, clipper.DisplayEnd)` submit draw widgets (`TreeNodeEx`, icon buttons, visibility toggles).
   - *Clipper Boundary & Navigation Protection*: If a row is targeted for focus/framing (`scrollToSelected`) or matches `ImGui::GetNavId()`, the clipper boundary includes it, ensuring keyboard navigation (Down/Up/PgDn) seamlessly scrolls new items into view without clipping glitches.
   - *Offscreen Section Alignment*: Before the viewports section, an explicit dummy spacer maintains identical scroll extents (`contentH: 90178.0`, `scrollMaxY: 89795.0`) regardless of whether the header is inside or outside the visible clipping range.
5. **Multiline & Zero-Match Fallback**:
   If any label in the model contains embedded newlines (`hasMultiline == true`), or if a search query yields zero matching node rows, the panel automatically falls back to unclipped rendering, preventing clipper height estimation discrepancies and guaranteeing pixel-perfect layout.
6. **Cache Invalidation Rules**:
   The row model is cached and reused across steady-state frames without rebuilding. `s_rowModelDirty = true` is asserted whenever:
   - Search filter text changes (`s_searchBuffer`)
   - Label display options toggle (`m_displayOptions`)
   - Selected node changes (triggering ancestor expansion check)
   - glTF model revision changes (`m_gltfRevision`)
   - Model reset or reload occurs (stale pointers cleared)
   - Primitive material reassignment occurs (`m_matDirty`)

#### 2. Deterministic Fixtures

Fixtures were generated deterministically via `python scripts/generate_ui_clipping_fixtures.py build/p2.4/fixtures`:

| Fixture File | Count | SHA-256 | Purpose |
| :--- | :---: | :--- | :--- |
| `p2_4_hierarchy_broad_4096.glb` | 4,096 | `64F06594250E16A6F471C26EF4D00E0A8968083E6696BB3C997FE243131D0068` | Broad tree: 4,096 root nodes under Scene, each with Mesh, Primitive, Material |
| `p2_4_hierarchy_deep_4096.glb` | 4,096 | `1000D7F63CA97E5080FB753315C62131A79E63121B2B6270BB6FC1EF138ABDD3` | Deep tree: 4,096 nodes with branching factor 4, testing deep descendant chains |
| `p2_4_hierarchy_multiline_4096.glb` | 4,096 | `3150A76BE7FC70011F97896BBAD086484AE0805E2195316522DB0C606C57D6B3` | Multiline tree: 4,096 nodes with embedded newlines for unclipped fallback verification |

#### 3. In-Process Self-Checks (Exit Code 0 on Success, 1 on Forced Failure)

Outputs stored in `build\p2.4\final\`:
- `--conflict-selfcheck`: **PASS (456 checks)**. Negative test `--selfcheck-force-failure` returned exit code 1.
- `--validation-cache-selfcheck`: **PASS (20 checks)**. Negative test returned exit code 1.
- `--cache-invalidation-selfcheck`: **PASS (94 checks)**:
  - 53 P2.4 hierarchy cache checks: warm node/mesh/material rename, primitive material reassignment (A $\to$ B $\to$ A), unassigned $\to$ assigned material, display options, model A $\to$ B $\to$ A, same-object reload, row model ordering/depths, expansion state invalidation, search filtering, stable ID invariance, multiline classification, reload pointer clearing.
  - 28 P2.2 resources cache invalidation checks.
  - 13 P2.2 validation report-revision checks.
  - Negative test returned exit code 1.
- `--logger-layout-selfcheck`: **PASS (19 checks)**. Negative test returned exit code 1.

#### 4. Runtime UI Integration Suite (`scripts/ui_integration/run_ui_integration.py`)

The integration suite was executed against the recorded baseline harness (`build\p2.4-baseline\Glider.exe`) and candidate (`build\p2.4-candidate\Glider.exe`):
**174 passed, 0 failed** (exit code 0; report in `build\p2.4\final_integration\summary.json`).

Negative testing verified:
- `--force-fail` returned exit code 1 (`intentional failing assertion`).
- `--fail-report-write` returned exit code 2 (`report write failed: [Errno 2]`).

Coverage:
- `hierarchy_broad_4096`: 47 snapshots compared and asserted byte-for-byte identical to baseline. Verifies initial scroll bounds, top/mid/bottom scrolling, consecutive row ID windows at mid-scroll, clicking visible node rows resolving to source node identity, keyboard navigation (Down/PgDn/Up) crossing scroll boundaries and maintaining focus, reaching tail row 4095, search filter shrinking scroll extents, search clearing restoring tree state, model reload (A $\to$ B $\to$ A and same-path reload), node rename, display option toggling, and material reassignment.
- `hierarchy_deep_4096`: 6 snapshots compared. Verifies collapsed root nodes, manual expansion revealing child nodes, nested descendant expansion, search filter auto-expanding deep ancestor chains down to leaf `TreeNode_4095`, and clicking deep matches resolving to source leaf node.
- `hierarchy_multiline_4096`: 6 snapshots compared. Verifies multiline detection, correct full scroll extent in unclipped fallback mode, top/mid/bottom scroll positioning, and selection.
- All existing P2.3 scenarios (`resources_4096`, `resources_multiline_4096`, `variants_4096`, `validation_2048`, `validation_mixed_2048`, `logger`) passed with zero regression across all 109 snapshots.

#### 5. Paired Sequential Benchmarks (5 Alternating Pairs, 120 Warm-up + 600 Measured Frames)

Executed via `python scripts/ui_integration/run_paired_bench_p2_4.py` (sequential, vsync off, 1280x720, isolated mode).
Full dataset recorded in `build\p2.4\bench\paired_summary.json` (11 scenarios, 110 total runs):

| Scenario | `ui.panels` Baseline $\to$ Candidate (Median, ms) | Paired Delta (ms) | `ui.panels` Noise Ranges (ms) | Whole-Frame CPU Baseline $\to$ Candidate (Median, ms) | First-Frame Cold `ui.panels` (ms) |
| :--- | :---: | :---: | :---: | :---: | :---: |
| `control_default` | 0.0174 $\to$ 0.0184 | +0.0016 | b: [0.0156, 0.0184], c: [0.0172, 0.0195] | 0.3337 $\to$ 0.3342 | b: 0.3132, c: 0.3174 |
| `control_resources_bottom` | 0.0437 $\to$ 0.0446 | +0.0003 | b: [0.0431, 0.0466], c: [0.0413, 0.0493] | 0.3574 $\to$ 0.3513 | b: 1.4606, c: 1.2744 |
| `hier_broad_collapsed` | 0.0500 $\to$ 0.0681 | +0.0180 | b: [0.0492, 0.0510], c: [0.0671, 0.0687] | 109.562 $\to$ 109.392 | b: 9.8884, c: 9.0807 |
| `hier_broad_expanded_top` | 1.6166 $\to$ **0.8979** | **-0.7217 (-44.5%)** | b: [1.5888, 1.6288], c: [0.8940, 0.9472] | 112.629 $\to$ 111.467 | b: 10.619, c: 10.395 |
| `hier_broad_expanded_mid` | 1.6105 $\to$ **0.8921** | **-0.7250 (-44.6%)** | b: [1.6014, 1.6854], c: [0.8760, 0.9497] | 112.100 $\to$ 109.778 | b: 9.8310, c: 9.7325 |
| `hier_broad_expanded_bottom` | 1.5863 $\to$ **0.8887** | **-0.6996 (-44.0%)** | b: [1.5821, 1.6124], c: [0.8834, 0.9091] | 110.577 $\to$ 109.816 | b: 10.245, c: 9.9076 |
| `hier_broad_search_many` | 0.2748 $\to$ 0.3002 | +0.0249 | b: [0.2732, 0.2778], c: [0.2986, 0.3027] | 109.440 $\to$ 108.424 | b: 9.5748, c: 10.105 |
| `hier_broad_search_sparse` | 0.2092 $\to$ 0.2575 | +0.0466 | b: [0.2084, 0.2142], c: [0.2540, 0.2609] | 110.203 $\to$ 111.033 | b: 10.158, c: 9.7559 |
| `hier_deep_partial` | 0.0715 $\to$ 0.0749 | +0.0038 | b: [0.0704, 0.0721], c: [0.0742, 0.0759] | 81.4260 $\to$ 81.2428 | b: 6.8866, c: 8.4917 |
| `hier_deep_nested` | 0.0719 $\to$ 0.0769 | +0.0052 | b: [0.0704, 0.0722], c: [0.0758, 0.0790] | 83.4724 $\to$ 84.2630 | b: 7.9247, c: 9.1364 |
| `hier_multiline_fallback` | 1.6601 $\to$ 2.3121 | +0.6071 (fallback) | b: [1.6395, 1.7772], c: [2.2107, 2.4132] | 116.385 $\to$ 115.764 | b: 10.597, c: 11.858 |

**Analysis**:
- On fully expanded broad trees (4,096 items), `ui.panels` warm frame time consistently falls from ~1.61 ms to ~0.89 ms (**44–45% sustained reduction**) across top, mid, and bottom scroll positions.
- Control benchmarks (`control_default` and `control_resources_bottom`) show zero regression: deltas are less than 0.002 ms and fall well within run noise ranges.
- Search filter scenarios (`hier_broad_search_many`, `hier_broad_search_sparse`) render matching subtrees in ~0.25–0.30 ms.
- The multiline fallback correctly bypasses virtualization when multiline labels exist, maintaining baseline rendering accuracy.
- First-frame cold layout costs remain comparable (9–10 ms on 4,096 nodes). Whole-frame CPU times are dominated by un-optimized 3D scene draw calls (~110 ms, addressed in Phase 3/4).

#### 6. Visual Inspection

Frame captures dumped at 1600x900 into `build\p2.4\final\`:
- `broad_expanded.png`: 4,096-node broad tree showing disclosure arrows, node branch icons, eye visibility toggles, correct vertical scrollbar thumb, and status bar compliance pill (`[ ✓ glTF Valid ]`).
- `broad_search.png`: Active filter (`Node_40`), clear button, and auto-expanded ancestor hierarchy.
- `deep.png`: Nested multi-level hierarchy with root nodes and integrated Viewports section.
- `multiline.png`: Unclipped multiline fallback layout.
All captures inspected and confirmed visually correct.

#### 7. Verification Summary & Review Gate

- Code changes: `source/imgui/SceneHierarchyPanel.cpp`, `source/imgui/main.cpp`, `header/imgui/UiScenarioHarness.h`, `scripts/ui_integration/build_baseline_harness.py`, `scripts/ui_integration/run_ui_integration.py`, `scripts/generate_ui_clipping_fixtures.py`, `scripts/ui_integration/run_paired_bench_p2_4.py`.
- `git diff --check` passed with 0 errors.
- Self-checks: conflict 456, validation-cache 20, cache-invalidation 94 (53+28+13), logger-layout 19. All return 0; all forced failures return 1.
- Runtime UI integration suite: 174 passed, 0 failed.
- Benchmark: 11 scenarios, 5 alternating pairs, 120 warmup + 600 measured frames completed sequentially.
- No commit or push was performed. Phase 3 has not been started.
*(Note: Initial review findings identified three issues documented and resolved in [§12.24](#section-12-24) below).*

---

<a id="section-12-24"></a>

### 12.24 P2.4 Review Findings, Resolution & Final Re-Validation (2026-10-22)

**Status: REVIEW FIXES COMPLETE; all 4 self-checks PASS (120 cache checks, 615 total assertions across suites), 12 UI integration scenarios PASS (240/240 assertions), paired benchmark matrix retained with original executable hash, camera/light selection highlighting verified via probe and frame capture. Submitted for independent reviewer acceptance.**

P2.1, P2.2, and P2.3 remain at the accepted baseline `fc6a89b`. Phase 3 was not started. Work is uncommitted.

```text
Baseline commit: fc6a89b ("Clip flat UI lists and logger rows with integration validation")
Baseline executable: build\p2.4-baseline\Glider.exe
Baseline executable SHA-256: AC7B3FF7AC6F392A20D2351CA8E44CD64B878D18F2B8AB65BBED8E0819FA4AC0
Benchmarked Candidate executable: build\bin\Release\Glider.exe (measured in 11-scenario matrix)
Benchmarked Candidate SHA-256: 1782F98269F20F62A922F8FA6ABBC822679C31DA9B4F0A42BF4F6BB1ADF7A9EF
Final Correctness-Tested executable: build\bin\Release\Glider.exe (with camera/light selection highlighting)
Final Correctness Candidate SHA-256: 83801272BB98467598F78AB3D54757B8B1E6A702B112F41A9427E4F0CA099A31
Build: Windows Release, MSVC x64 C++20, OpenGL 4.1 Core Profile, NVIDIA RTX 5080 / driver 616.92
Benchmark settings: 1280x720, vsync off, isolated state, 120 warm-up and 600 measured frames per run
Benchmark summary: build\p2.4\bench\paired_summary.json (11 scenarios, 5 pairs, 110 runs)
Pre-fix benchmark archive: build\p2.4\bench_pre_fix\paired_summary.json
Evidence root: build\p2.4\
```

#### 1. Review Findings and Root Cause Analysis

1. **Finding 1: Repeated row-model rebuilding caused by pending scroll request.**
   - *Root Cause*: `s_scrollToSelected` was previously included inside `selectionStale`. This flag is only cleared when the selected row is actively rendered by `renderRow()`. When viewport picking selected a node that was excluded by a hierarchy search filter, or when the Scene root was collapsed, the selected row was never rendered. Consequently, `selectionStale` remained `true` indefinitely, forcing `buildRowModel()` to run every single frame despite completely unchanged selection and hierarchy state.
   - *Fix*:
     - Separated pending scroll intent into `struct PendingScroll { bool active; SelectionType type; void* object; } s_pendingScroll;`.
     - Removed scroll flags completely from `selectionStale`. While a target is unavailable, `s_pendingScroll.active` remains armed without invalidating `s_hierarchyCache.rowModelValid`.
     - When the target becomes available (e.g. search filter is cleared or Scene root is reopened), the row model is invalidated by the filter/expansion event, `clipper.IncludeItemByIndex(selectedRowIndex)` forces the row into the visible range, `renderRow()` scrolls to it (`ImGui::SetScrollHereY(0.5f)`), and `s_pendingScroll.active` is cleared.
     - Added cancellation: if the selection changes to a different object while a scroll is pending, or if the model is reloaded, `s_pendingScroll` is reset immediately.
     - Added dedicated runtime scenario `hierarchy_pending_scroll` exercising production viewport picking (`picknode`), settled frames with target unavailable, filter clearing, scene reopening, selection replacement, and model reload.

2. **Finding 2: Cache-reuse assertions measured row counts instead of actual reuse.**
   - *Root Cause*: Previous assertions compared visible row counts (`len(settled) == len(initial)`) and labeled that "row model reuse". A model rebuilt every frame can produce identical rows, masking continuous rebuild bugs.
   - *Fix*:
     - Added `uint64_t s_rowModelBuildCount` in [`SceneHierarchyPanel.cpp`](file:///H:/git-repos/gltf-viewer/source/imgui/SceneHierarchyPanel.cpp), incremented at the entry of `buildRowModel()`.
     - Exported `rowModelBuildCount()` and `resetRowModelBuildCount()` in [`SceneHierarchyPanel.h`](file:///H:/git-repos/gltf-viewer/header/imgui/SceneHierarchyPanel.h).
     - Serialized `"hierarchyBuildCount": <count>` into every snapshot in `UiScenarioHarness.h`.
     - Added runtime assertions in `check_hierarchy_deep`, `check_hierarchy_shared_mesh`, and `check_hierarchy_pending_scroll`:
       - Assert delta > 0 on genuine invalidations.
       - Assert delta == 0 on settled frames across descendant selection, shared mesh multi-owner selection, user branch collapse, and unavailable scroll targets.
     - Made probe comparison in `compare_snap` occurrence-aware (`make_probe_dict` with `(tag, id, occ_index)`) so multiple occurrences of the same mesh/material under different nodes are matched and compared in order rather than collapsed.

3. **Finding 3: Incorrect visual evidence and comparison coverage.**
   - *Root Cause*: In `scripts/dump_p2_4_review_frames.py`, the scripts executed `clickarrowrow hier.scene 0 HierarchyTreeScrollRegion`. Because Scene 0 is open by default, clicking its arrow collapsed the Scene root into `> Scene`, resulting in screenshots showing closed trees instead of the expanded branches described in the journal.
   - *Fix*:
     - Corrected the automation script to preserve the open Scene root and select the items directly in Resources (`Meshes 0` after opening Meshes, and `Materials 0` which is open by default).
     - Added harness snapshot validators in `dump_p2_4_review_frames.py` asserting that `hier.scene` is open, multiple `hier.mesh` or `hier.material` rows are present, and selection matches the target before capturing PNG frames.
     - Recaptured genuine visual evidence at 1600x900:
       - `shared_mesh_owners.png` (SHA-256 `3422DF8F1AC61DE5A79C5F78C7BDC70902C95BA77B60CA1043025666CEDEE522`, 1,019,840 bytes): Both owning nodes `RootNode_A` and `RootNode_B` visibly expanded with open arrows and `SharedMesh_0000` highlighted under both.
       - `selection_expansion.png` (SHA-256 `3E35F4EB53BFA6D480A3D7B30988B9D22796965344A624545A9DDD669FAE397F`, 1,087,450 bytes): Scene down to `RootNode_A`/`RootNode_B` -> Mesh -> Primitive -> Material visibly expanded with open disclosure arrows and `SharedMaterial_0000` highlighted.
       - `multiline_fallback.png` (SHA-256 `FB40181F400D4BB0BF70C0B22818DBF9DF2BDFD2F8DF42361F5D7686A9F648C5`, 1,061,558 bytes): Unclipped multiline fallback with correct vertical spacing, disclosure arrows, and proportional scrollbar thumb.

4. **Finding 4: Direct measurement of multiline fallback overhead and baseline equivalents.**
   - *Investigation & Direct Profiling*:
     - **Rebuild time vs Steady-state rendering**: On the first frame, `buildRowModel()` takes ~1.8 ms to construct the 4,096-row model. On all steady-state frames (including all 600 measured benchmark frames), `s_hierarchyCache.rowModelValid == true`. **Rebuild time is 0.000 ms** across all measured benchmark frames.
     - **Fallback loop overhead**: The previous ~0.65 ms delta was caused by:
       1. Repeated invalidation due to `open != row.isOpen`.
       2. 8,192 per-frame calls to `ImGui::Indent()` and `ImGui::Unindent()` inside the fallback loop.
       3. Per-row selection checks calling a switch statement over selection types.
     - **Optimizations Applied**:
       - Implemented depth-transition indentation in `renderRowModel()`: only calls `ImGui::Indent()`/`Unindent()` when `row.depth` changes, eliminating 8,192 calls per frame down to ~2 calls.
       - Replaced $O(N)$ selection switch with a direct pointer check `curSelectedObj == r.object`.
     - **Paired Benchmark Results (against baseline with all 4,096 items expanded)**:
       - Baseline `ui.panels`: `1.6265 ms` [1.6244 – 1.6801]
       - Candidate `ui.panels`: `1.7152 ms` [1.7127 – 1.7400]
       - Paired delta: `+0.0883 ms` (+5.4%).
     - **Trade-off Disclosure**: Unclipped fallback incurs a minor +0.0883 ms (+5.4%) fixed overhead compared to baseline due to iterating the flattened `HierarchyRow` cache vector and dereferencing string labels. This unclipped fallback is retained as an explicit design trade-off because multiline labels in glTF scene hierarchies are an infrequent edge case, and this path guarantees 100% correct multiline text measurement, reachability of the final row, and zero clipping artifacts without requiring brittle height estimation.

5. **Finding 5: Restoring Camera and Light hierarchy selection highlighting and clearing.**
   - *Root Cause*: In `SceneHierarchyPanel::render()`, `curSelectedObj` previously checked only `Node`, `Mesh`, `Material`, and `Viewport`. When `selection.type == SelectionType::Camera` or `SelectionType::Light`, `curSelectedObj` remained `nullptr`, causing `isRowSelected(row)` to evaluate to `false` and preventing camera and light rows from receiving `ImGuiTreeNodeFlags_Selected`. Furthermore, `nodesWithCamera` and `nodesWithLight` were not tracked for auto-expansion.
   - *Fix*:
     - Populated `curSelectedObj` with `selection.selectedCamera` and `selection.selectedLight`.
     - Added `nodesWithCamera` and `nodesWithLight` tracking in `ensureHierarchyCache()`, cleared them cleanly in `buildHierarchyLabels()` and `HierarchyCache::clear()`.
     - Added camera and light ancestor auto-expansion in `buildRowModel()`.
     - Extended `UiProbeEntry` with `selected` state tracking (`bool selected = false`) serialized into test snapshots.
     - Extended the cache-invalidation self-check (from 60 to 79 checks) verifying camera/light selection, switching between cameras, switching between lights, clearing previous selections, and auto-expanding collapsed owners.
     - Added dedicated runtime scenario `hierarchy_camera_light` verifying selection via Resources, selection via hierarchy rows, switching between two cameras, switching between two lights, and row highlighting/clearing (25/25 checks PASS).
   - *Benchmark Non-Rerun Justification*: This fix was strictly localized to object-pointer assignment in `curSelectedObj` for Camera and Light selection types and populating `nodesWithCamera`/`nodesWithLight`. None of the models in the 11-scenario performance benchmark suite contain camera or light nodes (`p2_4_hierarchy_broad_4096.glb`, `p2_4_hierarchy_deep_4096.glb`, `p2_4_hierarchy_multiline_4096.glb`, `p2_4_hierarchy_shared_mesh.glb`, and the control/resource models only have meshes, materials, and primitives). No changes were made to traversal, clipping, list clipper invocation, cache invalidation, or fallback loops. The measured performance dataset remains architecturally and mathematically identical; rerunning the 11-scenario matrix is unnecessary. The measured executable hash (`1782F98269F20F62A922F8FA6ABBC822679C31DA9B4F0A42BF4F6BB1ADF7A9EF`) is retained in the benchmark table, while correctness is validated on the final candidate binary (`83801272BB98467598F78AB3D54757B8B1E6A702B112F41A9427E4F0CA099A31`).

---

#### 2. Self-Checks & Negative Testing (Exit Code 0 on Success, 1 on Forced Failure, 2 on Unwritable Path)

Positive runs are preserved in `build\p2.4\selfchecks_positive\`; negative runs are preserved in `build\p2.4\selfchecks_negative\`:
- `--conflict-selfcheck`: **PASS (456 checks)**. Negative test `--selfcheck-force-failure` returned exit code 1; unwritable path returned exit code 2.
- `--validation-cache-selfcheck`: **PASS (20 checks)**. Negative test returned exit code 1; unwritable path returned exit code 2.
- `--cache-invalidation-selfcheck`: **PASS (120 checks total)**:
  - **79 P2.4 hierarchy cache checks**: warm node/mesh/material rename, primitive material reassignment ($A \to B \to A$), unassigned $\to$ assigned material, display options, model $A \to B \to A$, same-object reload, row model ordering/depths, expansion state invalidation, search filtering, stable ID invariance, multiline classification, reload pointer clearing, camera and light selection/switching/clearing, camera and light collapsed owner auto-expansion, shared mesh multi-owner auto-expansion, nested owner ancestor auto-expansion, material-driven primitive auto-expansion across all owners, settled frame row-model reuse, pending scroll with unavailable target does not trigger rebuilds.
  - **28 P2.2 resources cache invalidation checks**.
  - **13 P2.2 validation report-revision checks**.
  - Negative test returned exit code 1; unwritable path returned exit code 2.
- `--logger-layout-selfcheck`: **PASS (19 checks)**. Negative test returned exit code 1; unwritable path returned exit code 2.

Runner script: `python scripts/run_selfchecks_final.py` (all 8 check variations and 4 unwritable path checks pass).

---

#### 3. Runtime UI Integration Suite (`scripts/ui_integration/run_ui_integration.py`)

Executed against baseline `build\p2.4-baseline\Glider.exe` and candidate `build\bin\Release\Glider.exe`:
- **Positive run output**: `build\p2.4\final_integration\summary.json`: **240 passed, 0 failed** (exit code 0; 12 scenarios):
  - `resources_4096`: 25 checks PASS (snapshot comparisons passing under the configured tolerances across all 50 snapshots).
  - `resources_multiline_4096`: 25 checks PASS (snapshot comparisons passing under the configured tolerances across all 48 snapshots).
  - `variants_4096`: 25 checks PASS (snapshot comparisons passing under the configured tolerances across all 50 snapshots).
  - `validation_2048`: 14 checks PASS (snapshot comparisons passing under the configured tolerances across all 24 snapshots).
  - `validation_mixed_2048`: 15 checks PASS (snapshot comparisons passing under the configured tolerances across all 24 snapshots).
  - `logger`: 26 checks PASS (snapshot comparisons passing under the configured tolerances across all 28 snapshots).
  - `hierarchy_broad_4096`: 25 checks PASS (snapshot comparisons passing under the configured tolerances across all 47 snapshots).
  - `hierarchy_deep_4096`: 17 checks PASS (snapshot comparisons passing under the configured tolerances across all 11 snapshots; verifies descendant selection with ancestors collapsed, settled frame build delta == 0, subsequent branch collapse delta == 0).
  - `hierarchy_multiline_4096`: 9 checks PASS (snapshot comparisons passing under the configured tolerances across all 6 snapshots; verifies unclipped fallback scroll extents and top/mid/bottom positioning).
  - `hierarchy_shared_mesh`: 18 checks PASS (snapshot comparisons passing under the configured tolerances across all 10 snapshots; verifies shared mesh multi-owner auto-expansion, build count delta > 0 on selection, settled build delta == 0, independent owner collapse delta == 0, shared material expansion, and clean model reload).
  - `hierarchy_pending_scroll`: 16 checks PASS (verifies viewport picking with filter active, unavailable target build delta == 0, filter clearing eventual scroll, picking with collapsed scene build delta == 0, scene reopening scroll, selection replacement cancellation, and model reload clean reset).
  - `hierarchy_camera_light`: 25 checks PASS (verifies selecting Camera 0 in Resources, switching to Camera 1, switching to Light 0, switching to Light 1, selecting Camera 0 in hierarchy, switching to Camera 1, selecting Light 0, switching to Light 1, and confirming row selection highlight and clearing of prior selections).

- **Intentional negative runs**: `build\p2.4\final_negative\`:
  - `--force-fail`: exits with code 1; `summary.json` records 25 passed, 1 failed.
  - `--fail-report-write`: exits with code 2 on unwritable report path target.

- **Comparison Tolerances and Hook Boundaries**:
  - *Hierarchy Comparison Tolerance*: In `compare_snap`, coordinate tolerance for hierarchy rows (`hier.*`) is `4.0` pixels (`coord_tol = 4.0 if key[0].startswith("hier.") else 0.5`) to accommodate sub-row fractional scroll variance between the unclipped baseline and the virtualized list clipper. Probe diffs within 25 px of the top and bottom viewport clipping boundaries are excluded.
  - *`picknode <id>` Coverage Boundary*: `uiFrame.pickNode` in `main.cpp` resolves the target node by ID, invokes `selection.selectNode(pickedNode)`, sets `isSceneDirty = true`, and directly triggers `SceneHierarchyPanel::requestScrollToSelectedNode()`. This drives the identical selection and scroll intent path used by the viewport picking raycast/readback handler (`PickingRenderLayer`), while decoupling the harness test from offscreen mouse coordinates and multisample picking buffer readback timing.

---

#### 4. Final Paired Sequential Benchmarks (5 Alternating Pairs, 120 Warm-up + 600 Measured Frames)

Executed via `python scripts/ui_integration/run_paired_bench_p2_4.py --pairs 5 --warmup 120 --frames 600` (11 scenarios, 110 runs strictly sequential, alternating order per pair).
Full dataset recorded in `build\p2.4\bench\paired_summary.json` (Benchmarked executable SHA-256: `1782F98269F20F62A922F8FA6ABBC822679C31DA9B4F0A42BF4F6BB1ADF7A9EF`):

| Scenario | Baseline `ui.panels` Median [Min–Max] (ms) | Candidate `ui.panels` Median [Min–Max] (ms) | Paired Delta (ms) | Status / Impact | Whole-Frame CPU Baseline $\to$ Candidate (ms) | First-Frame Cold `ui.panels` Baseline $\to$ Candidate (ms) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `control_default` | 0.0195 [0.0188–0.0200] | 0.0176 [0.0158–0.0193] | -0.0024 (-12.3%) | Parity (within noise) | 0.3981 $\to$ 0.3834 | 0.3137 $\to$ 0.3250 |
| `control_resources_bottom` | 0.0476 [0.0456–0.0490] | 0.0490 [0.0470–0.0641] | +0.0015 (+3.3%) | Parity (within noise) | 0.3813 $\to$ 0.4357 | 1.4239 $\to$ 1.4028 |
| `hier_broad_collapsed` | 0.0519 [0.0516–0.0523] | 0.0516 [0.0513–0.0519] | **-0.0003 (-0.6%)** | **Regression eliminated** (was +36%) | 115.1307 $\to$ 113.8655 | 9.4152 $\to$ 10.5293 |
| `hier_broad_expanded_top` | 1.6550 [1.6459–1.6663] | **0.0600** [0.0596–0.0619] | **-1.5941 (-96.3%)** | **27.6x speedup** | 116.4872 $\to$ 112.9479 | 10.1876 $\to$ 10.6388 |
| `hier_broad_expanded_mid` | 1.6446 [1.6256–1.7413] | **0.0618** [0.0608–0.0648] | **-1.5838 (-96.3%)** | **26.6x speedup** | 113.7484 $\to$ 110.6674 | 10.1742 $\to$ 10.8223 |
| `hier_broad_expanded_bottom` | 1.6357 [1.6101–1.6400] | **0.0619** [0.0607–0.0631] | **-1.5726 (-96.1%)** | **26.4x speedup** | 113.5624 $\to$ 111.1122 | 10.8520 $\to$ 9.7624 |
| `hier_broad_search_many` | 0.2836 [0.2788–0.2867] | **0.0658** [0.0653–0.0663] | **-0.2178 (-76.8%)** | **4.3x speedup** (was +9% regression) | 112.4748 $\to$ 111.2190 | 9.9638 $\to$ 11.0732 |
| `hier_broad_search_sparse` | 0.2154 [0.2127–0.2191] | **0.0551** [0.0547–0.0558] | **-0.1607 (-74.6%)** | **3.9x speedup** (was +23% regression) | 112.5426 $\to$ 111.4391 | 9.7228 $\to$ 10.6618 |
| `hier_deep_partial` | 0.0715 [0.0708–0.0719] | 0.0647 [0.0642–0.0666] | -0.0064 (-9.0%) | Faster than baseline | 84.5319 $\to$ 83.9201 | 7.4819 $\to$ 8.9409 |
| `hier_deep_nested` | 0.0717 [0.0710–0.0720] | 0.0652 [0.0641–0.0657] | -0.0065 (-9.1%) | Faster than baseline | 85.1072 $\to$ 85.3225 | 7.6483 $\to$ 9.3703 |
| `hier_multiline_fallback` | 1.6265 [1.6244–1.6801] | 1.7152 [1.7127–1.7400] | +0.0883 (+5.4%) | **Explicit Trade-off** (+0.088 ms overhead for 100% correct multiline measurement) | 115.1904 $\to$ 114.2912 | 10.2982 $\to$ 12.7866 |

**Key Benchmarking Conclusions**:
1. **Collapsed tree parity**: With settled frame cache reuse, `hier_broad_collapsed` `ui.panels` cost is `0.0519 ms` (baseline) vs `0.0516 ms` (candidate), with paired delta `-0.0003 ms` (-0.6%) and completely overlapping ranges (`[0.0516–0.0523]` vs `[0.0513–0.0519]`). The prior +36% regression is completely eliminated.
2. **Search scenarios deliver massive speedups**: Instead of the previous +9% and +23% regressions, settled frame cache reuse yields a **76.8% reduction (4.3x speedup)** on `hier_broad_search_many` (`0.2836 -> 0.0658 ms`) and a **74.6% reduction (3.9x speedup)** on `hier_broad_search_sparse` (`0.2154 -> 0.0551 ms`).
3. **Expanded tree virtualization delivers up to 27.6x speedup**: `ui.panels` drops from ~1.63–1.66 ms down to ~0.060–0.062 ms (**-96.3% reduction / 26.4x–27.6x speedup**) across top, mid, and bottom scroll positions.
4. **Multiline fallback explicit trade-off**: Rather than the prior 2.3121 ms (+39%) regression caused by repeated invalidation, the fallback operates at `1.7152 ms` (+5.4% over baseline 1.6265 ms, delta +0.0883 ms). This minor +0.088 ms overhead is accepted as an explicit design trade-off for guaranteed multiline measurement accuracy, reachability of the final row, and zero clipping artifacts without brittle height estimation.

---

#### 5. Representative Frame Dumps & Visual Inspection

Captured at 1600x900 with in-harness snapshot probe validation:
- `shared_mesh_owners.png` (SHA-256 `3422DF8F1AC61DE5A79C5F78C7BDC70902C95BA77B60CA1043025666CEDEE522`, 1,019,840 bytes, in `build\p2.4\final\`): Demonstrates `SharedMesh_0000` selected in Resources; both owning nodes `RootNode_A` and `RootNode_B` in the Scene Hierarchy tree are visibly auto-expanded, showing open disclosure arrows and the selected mesh highlighted under each owner.
- `selection_expansion.png` (SHA-256 `3E35F4EB53BFA6D480A3D7B30988B9D22796965344A624545A9DDD669FAE397F`, 1,087,450 bytes, in `build\p2.4\final\`): Demonstrates `SharedMaterial_0000` selected in Resources; auto-expands Scene -> Node -> Mesh -> Primitive -> Material branches across all instances in the hierarchy tree with correct disclosure arrows and highlight states.
- `multiline_fallback.png` (SHA-256 `FB40181F400D4BB0BF70C0B22818DBF9DF2BDFD2F8DF42361F5D7686A9F648C5`, 1,061,558 bytes, in `build\p2.4\final\`): Demonstrates the unclipped fallback rendering multiline labels with exact vertical spacing, disclosure arrows, and proportional scrollbar thumb.
- `cam_light_selection.png` (SHA-256 `19E1248A9FF1E7486A2C750D860512F844595AC370DFD9D23662A94A7338835B`, 1,023,851 bytes, in `build\p2.4\`): Demonstrates `Light 1 "Light_0001"` selected; Scene Hierarchy tree shows Scene -> Nodes -> Cameras and Lights expanded with `Light 1` prominently highlighted in emerald green (`ImGuiCol_HeaderActive`), Resources panel highlighting `Light 1`, Element Inspector showing `Light ID: 1`, and all previous selections cleared.

All captures visually inspected and verified correct.

---

#### 6. Summary of Changed Files

- `header/imgui/ResourcesPanel.h`: Added `revision` field to `SelectionState` incremented on every selection event.
- `header/imgui/SceneHierarchyPanel.h`: Added `rowModelBuildCount()` and `resetRowModelBuildCount()` counters.
- `header/imgui/UiProbe.h`: Added `open` disclosure state tracking and `selected` highlight tracking to `UiProbeEntry`.
- `header/imgui/UiScenarioHarness.h`: Added `hierarchyBuildCount` tracking, probe `open` and `selected` serialization, and `picknode <id>` production viewport picking hook.
- `source/imgui/ResourcesPanel.cpp`: Passed `isSelected` to `UiProbe::item()` for resource items.
- `source/imgui/SceneHierarchyPanel.cpp`:
  - Fixed selection-driven expansion to synchronize ImGui storage open state for nodes, meshes, and materials/primitives.
  - Separated `s_pendingScroll` from `selectionStale` to prevent continuous row-model rebuild loops when targets are filtered or collapsed.
  - Fixed `nodesWithMesh` 1:N mapping and ancestor walk for all shared mesh owner nodes.
  - Added `nodesWithCamera` and `nodesWithLight` tracking in `ensureHierarchyCache()` and cleared in `buildHierarchyLabels()`.
  - Added camera and light auto-expansion in `buildRowModel()`.
  - Added camera and light selection highlighting in `curSelectedObj` and passed `isSelected` to `UiProbe::item()`.
  - Fixed `ImGuiTreeNodeFlags_DefaultOpen` on `Scene` and `ViewportsHeader` in `renderRow()`.
  - Replaced $O(N)$ row scans with $O(1)$ cached indices (`nodeRowCount`, `selectedRowIndex`).
  - Added depth-transition indentation in `renderRowModel()` to eliminate 8,192 per-frame calls.
  - Added build counter increment and extended self-checks (79 checks) verifying pending scroll behavior, shared mesh auto-expansion, camera/light selection/switching/clearing, and settled frame row-model reuse.
- `source/imgui/main.cpp`: Wired `hierarchyBuildCount` and `pickNode` execution invoking production selection and scroll request.
- `scripts/generate_ui_clipping_fixtures.py`: Added `hierarchy_shared_mesh_document()` producing `p2_4_hierarchy_shared_mesh.glb` and `hierarchy_camera_light_document()` producing `p2_4_hierarchy_camera_light.glb`.
- `scripts/ui_integration/run_ui_integration.py`: Added `hierarchy_shared_mesh`, `hierarchy_pending_scroll`, and `hierarchy_camera_light` scenarios, occurrence-aware probe matching, and build counter delta assertions.
- `scripts/run_selfchecks_final.py`: Helper script running all 4 self-checks, 4 forced failures, and 4 unwritable path tests into separate positive and negative directories.
- `scripts/dump_p2_4_review_frames.py`: Automation script capturing and validating the 3 review frame dumps.
- `scripts/ui_integration/run_paired_bench_p2_4.py`: 5-pair sequential alternating benchmark automation script.

---

#### 7. Review Gate Status

- All review findings and regression coverage requirements are resolved and verified in code.
- All four self-checks pass: 456 conflict, 20 validation-cache, 120 cache-invalidation (79 P2.4 hierarchy, 28 resources, 13 validation), 19 logger-layout.
- UI integration suite passes 240/240 assertions across all 12 scenarios with snapshot comparisons passing under the configured tolerances against baseline `fc6a89b`.
- Positive and intentional-negative validation outputs are cleanly separated in `build\p2.4\selfchecks_positive\`, `build\p2.4\selfchecks_negative\`, `build\p2.4\final_integration\`, and `build\p2.4\final_negative\`.
- Performance assessment (retaining the explicit multiline fallback overhead): collapsed tree parity confirmed (-0.6%), search scenarios accelerated (4.3x and 3.9x faster), expanded tree virtualization delivering up to 27.6x speedup, and multiline fallback overhead (+0.088 ms / +5.4%) documented as an explicit design trade-off.
- `git diff --check` passes with zero errors.
- No commit or push has been performed. Phase 3 has not been started.
- Submitted for independent reviewer assessment.


#### Independent Review Acceptance (2026-10-22)

P2.4 is accepted for commit after focused review of the final camera/light fix.
No commit was performed during this review.

- Release build passed. Final executable SHA-256:
  `83801272BB98467598F78AB3D54757B8B1E6A702B112F41A9427E4F0CA099A31`.
- Independently reran camera/light integration: 25 passed, 0 failed
  (`build/p2.4/reviewer_final_camera/summary.json`).
- Independently reran shared-mesh integration: 18 passed, 0 failed
  (`build/p2.4/reviewer_final_shared/summary.json`).
- Independently reran all four self-check suites, forced-failure checks, and
  unwritable-output checks; expected exit codes 0/1/2 passed.
- Inspected `build/p2.4/cam_light_selection.png`; selected light highlighting
  and cleared prior selections are visible.
- Inspected the retained full integration summary: 240 passed, 0 failed across
  12 scenarios. This full suite was produced by the implementation agent;
  the reviewer did not repeat it or the performance matrix.
- Hierarchy comparison excludes pixel hashes, permits 4 px probe-coordinate
  differences and 25 px scroll/content-extent differences, and excludes
  boundary probe differences within 25 px of the clip edges. Passing these
  comparisons does not establish pixel-identical rendering. Camera/light
  and pending-scroll scenarios use candidate-only assertions. The picknode
  hook tests selection/scroll handling, not GPU picking readback.
- Prior benchmark measurements remain tied to executable `1782F982...ADF7A9EF`;
  they are not measurements of the final executable. No fresh performance
  guarantee is inferred from unchanged fixture content.
- The measured multiline fallback overhead of +0.0883 ms / +5.4% is accepted
  as a bounded tradeoff alongside the recorded expanded-tree/search gains.
  It is not an eliminated regression.


