# Phase 9 — Pass Caches, Startup Environment and Frame Values

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Complete, uncommitted ([§12.68](#section-12-68)). Profile stages and counters were added first ([§12.63](#section-12-63)). C4 overlay-depth reuse PASS ([§12.64](#section-12-64)). C1 shadow-cascade reuse PASS ([§12.65](#section-12-65)). C13 single startup environment load PASS ([§12.66](#section-12-66)). C2 frame-value settings without a renderer sync PASS ([§12.67](#section-12-67)). C6 (TransmissionTarget reallocations) was measured only and not changed.

**Scope:** The candidates come from the 2026-10-30 audit of the three-viewport E195 use case (C1, C2, C4, C6, C13). Every image stays byte-identical to `69cf29b`. Each cache has a same-binary A/B switch, `--disable-pass-cache`.

**Machine note:** The GPU was shared with an inference VM during every run (nvidia-smi showed 15–100 % utilisation before and after each launch). GPU times are therefore context only. Gains are shown with CPU stages, counters and interleaved baseline/new pairs.

<a id="section-12-63"></a>

### 12.63 Step 0 — Stages and counters (2026-10-30)

New profile stages:

- `overlay.depth`: the whole unjittered overlay-depth block.
- `post.process`: `renderPostProcess`.

New frame counters:

| Counter | Meaning |
|---|---|
| `shadowSceneBuilds` | Caster `createScene` calls in `updateSunShadows` |
| `overlayDepthSceneBuilds` | Overlay-depth `createScene` calls |
| `shadowCacheHits` / `overlayDepthCacheHits` | Reuses by C1 / C4 |
| `environmentReloads` | HDR loads in `drawViewports` |

`TransmissionTarget::captureDepth` and `capture` now report their reallocations through `countTextureAllocation`.

**C6 measurement (not fixed).** Three-viewport E195, procedural sky, TAA in each viewport, steady frames: `textureAllocations` = 3 per frame and `textureAllocationBytes` = 8,549,760 per frame. Each viewport's transmission copy is reallocated on every frame.

<a id="section-12-64"></a>

### 12.64 C4 — Overlay-depth reuse across TAA settle renders

**Change (`Viewports.cpp`, `ViewportInstance.h`).** Under TAA, or with a ground plane or ocean, the grid and box overlays test against an unjittered opaque depth. Before this change, that depth was rebuilt on every render. During TAA settle renders the render key does not change, so the depth is now kept in a per-viewport depth framebuffer (`overlayDepthCache`) and copied back into `overlayFbo` on every render. A copy is still needed each time because the boxes and wire lines drawn afterwards write depth.

**Key (`ViewportPassCacheKey`).**

- The viewport's `ViewportRenderKey`:
  - layer and scene
  - every scene revision
  - layer scene and pose generations
  - unjittered view and projection, eye and zoom
  - size
  - AA mode, samples and target generation
  - camera attachment
  - viewport settings, including the ASOBO switches and the ground
  - scene environment (the ocean simulation pointer is excluded)
  - sky-occlusion key
  - ocean version and time
- `RenderQuality::effective()`
- The overlay target's sample count

**Opaque membership.** The main pass culls with the jittered projection, so meshes at the frustum edge enter and leave its reverse mapping from one settle render to the next. `OpaquePass` draws depth with `LEQUAL` and full depth writes. The depth buffer is therefore the per-sample minimum over all drawn fragments and does not depend on draw order. As a result:

- The cache may hold a subset of the current drawables. Opaque meshes the main pass keeps now but that were left out of the cached copy are drawn on top of it (one small `createScene` per frame on E195).
- If a cached mesh has left the mapping, the copy is rebuilt.

**GL state.** The grid and box renderers inherit some state that they do not set themselves: depth func, blend equation, cull mode, front face, polygon mode and offset, and the active texture. That state is captured after the full draw and restored after every copy.

**Invalidation.** The copy is rebuilt (or bypassed) when any of the following holds:

- The key differs.
- A cached mesh is no longer in the culled mapping.
- The cache size or sample count differs from the viewport's.
- A scissor test is enabled, because the rebuild draws only inside it.
- The view is in wireframe mode.
- The viewport is closed (the cache is released).
- `--disable-pass-cache` is set.

**Measurement.** E195, three viewports, TAA in each, 1920×1080, 150 warm-up and 200 measured frames, interleaved, `build/measure/impl_bench.py`. Values are medians in ms.

| Run | CPU frame | `overlay.depth` | `shadow.render` | draws | GL calls |
|---|---:|---:|---:|---:|---:|
| sky, 69cf29b (pair 1 / 2) | 66.49 / 64.66 | (not staged) | 12.00 / 11.74 | 6105 | 41833 |
| sky, C4+C1 (pair 1 / 2) | 45.10 / 47.81 | 1.11 / 1.23 | 0.03 / 0.03 | 2221 | 20566 |
| sky, C4+C1, `--disable-pass-cache` | 65.60 / 50.20 | 11.23 / 10.76 | 11.89 / 11.17 | 6105 | 41834 |
| sky, curve exposure, 69cf29b | 59.46 / 66.85 | (not staged) | 11.72 / 15.55 | 6105 | 41842 |
| sky, curve exposure, C4+C1 | 39.21 / 39.38 | 1.19 / 1.15 | 0.03 / 0.03 | 2221 | 20566 |
| sky, curve exposure, C4+C1, `--disable-pass-cache` | 59.38 / 58.33 | 11.54 / 11.26 | 12.48 / 11.49 | 6105 | 41842 |

With the cache, each frame has 3 overlay-depth hits and 1 small edge-mesh scene build. `--disable-pass-cache` restores the exact baseline work (6105 draws).

**Equivalence.** These suites PASS against `69cf29b`: `p8_1`, `p8_3`, `p8_5`, `p8_followup`, `p4_exit_gate`, `review_fixes` and `viewport_image`. Frame-30 captures under TAA, with cache hits counted, are byte-identical to `69cf29b` for three views: the default view (grid), and the E195 cockpit with the sky, both with and without curve exposure.

<a id="section-12-65"></a>

### 12.65 C1 — Shadow-cascade reuse (2026-10-30)

**Change (`ViewportEffects.cpp`).** `updateSunShadows` now receives the viewport's render key. When all of the following match, the cascades are not re-rendered:

- the render key
- the effective quality (resolution, cascades, distance, split, biases, filter)
- whether the view is shaded
- the ground height

In that case the stored `SunShadowParameters` are handed to the command generator again, `sceneFbo` and the viewport are rebound, and the function returns. This skips the caster scene build, the cascade fit and the cascade draws.

**Why the TAA jitter is not part of the fit.** The cascades are fitted to the `view`/`proj` that the render key also stores: the unjittered camera. The jitter is applied only to the main pass's projection afterwards. The casters are drawn with a fixed dither frame (`setAsoboDitherFrame(0)`). The main pass then sets every ASOBO frame value again, so skipping the shadow pass leaves the generator in the same state.

**Invalidation.** The cascades are re-rendered when:

- the key differs (which covers every scene revision, environment and sun change, quality or ground change)
- the shadow map texture changes
- the shadows are switched off (`disable()`)
- the viewport is closed
- `--disable-pass-cache` is set

**Measurement.** See the table in [§12.64](#section-12-64): `shadow.render` falls from 11.2–15.6 ms to 0.03 ms for three viewports, and `shadowSceneBuilds` falls from 3 to 0 per frame.

**Equivalence.** These suites PASS against `69cf29b`: `shadows`, `sun`, `sky`, `time_of_day`, `ground`, `ssao`, `settings_scopes` (apart from the preset cases, see [§12.68](#section-12-68)) and `p4_exit_gate`.

<a id="section-12-66"></a>

### 12.66 C13 — One startup environment load (2026-10-30)

**Change (`Application.cpp`, `CommandLine.cpp`, `Scene.cpp`).**

- `run()` no longer loads the default HDR before `configure()`.
- `--env`/`--hdr` only record the path.
- After the flags are read, `loadStartupEnvironment` does at most one load:
  - It uses the last `--env` file that loads, otherwise the default HDR.
  - It filters with the A3.1 sun removal that the first frame's `drawViewports` would ask for, so `--sun on` no longer decodes and filters twice.
  - With the procedural sky as source, and the IBL or background allowed, no HDR is decoded. The first frame attaches the sky, and `drawViewports` loads the HDR when its source is selected.

**Measurement.** HelloCube, interleaved `69cf29b`/current pairs, 2 rounds each, `build/measure/impl_c13.py`. Values are `startup.environmentLoads`, then init + filter ms, then total startup ms.

| Launch | 69cf29b | Current |
|---|---|---|
| default | 1 · 457 / 459 · 950 / 951 | 1 · 436 / 467 · 946 / 930 |
| `--env <default hdr>` | 2 · 921 / 893 · 1449 / 1363 | 1 · 456 / 454 · 950 / 1013 |
| `--environment sky` | 1 · 449 / 523 · 838 / 923 | 0 · 0 / 0 · 498 / 595 |
| `--environment sky --env <hdr>` | 2 · 908 / 935 · 1356 / 1333 | 0 · 0 / 0 · 549 / 580 |
| `--sun on` | 1 (+1 first-frame reload) · launch 1.6 s | 1 (no reload) · launch 1.1 s |

**Equivalence.** These suites PASS against `69cf29b`: `sky`, `sun_source`, `background_projection`, `night`, `ambient`, `sun` and `p4_exit_gate`. The default launch is unchanged: one load, and the same default-view capture. `ocean` and `settings_scopes` show only the baseline-mismatch cases described in [§12.68](#section-12-68).

<a id="section-12-67"></a>

### 12.67 C2 — Frame-value settings without a renderer sync (2026-10-30)

**Change.** A scene-environment value is read by the frame: in `drawViewports`, in `resolveViewport`, and in the sky, ocean and sky-occlusion keys. No `GltfRenderLayer` state that `updateGltf` or `updateScene` consumes is derived from it. Such edits now mark `SceneChange::Environment`, which still redraws, resets TAA and changes every pass-cache key, instead of `RenderSettings`. This applies to:

- the Scene Inspector (`SceneInspector.cpp`)
- the registry `setSetting` path (`CommandsHandlers.cpp`)

`SettingSpec::resync` (`CommandRegistry.h`) keeps the renderer sync for these keys:

- `environment`: the source swaps the layer's environment.
- `background`: it switches the layer's cube map and gates the environment load. P4 classifies it as `RenderSettings`.

In the Inspector, the HDR file load and the Environment Source combo keep the sync for the same reason. Renderer-scope (Quality) keys are unchanged, because they drive IBL re-filtering and shader variants.

**Measurement.** E195, one viewport, procedural sky, `sun_azimuth` edited every other frame (`build/measure/impl_c2.py`), interleaved pairs.

| | 69cf29b (r0 / r1) | Current (r0 / r1) |
|---|---|---|
| `updateGltf` / `updateScene` calls in 140 frames | 70 / 70 | 0 / 0 |
| buffer upload | 2062 MB | 0.3 MB |
| `scene.dirtySync` on edit frames (median) | 14.08 / 13.93 ms | 0.00 / 0.00 ms |
| CPU on edit frames (median) | 38.51 / 35.41 ms | 16.85 / 12.44 ms |

**Check added.** `validate_settings_scopes.py` (pipe group, `check_frame_values`) checks four things:

- The same launch edits five frame values over the pipe: `sun_dir`, `exposure`, `bloom_intensity`, `ssao_intensity` and `tone_mapping`.
- The capture equals a CLI launch with the equivalent flags.
- `get_state.sync.syncs` does not change.
- A resync key (`environment`) still advances it.

**Equivalence.** These suites PASS against `69cf29b`: `settings_scopes` (apart from the preset cases), `agent_control`, `ssao`, `ground`, `p4_exit_gate`, `sun_source`, `background_projection` and `ambient`.

<a id="section-12-68"></a>

### 12.68 Phase 9 Exit Notes (2026-10-31)

- **Baseline-mismatch cases.** Two checks are written for an older baseline, and they fail against `69cf29b` for every build from this tree, including `69cf29b` itself:
  - `settings_scopes` `preset_low` / `preset_ultra` assume `7f9ff37`'s 4 px taller toolbar row.
  - `ocean`'s Grid/Plane cases assume a baseline that hid the grid on the plane.

  With identical flags on both binaries, all six images are byte-identical to `69cf29b` (`build/measure/impl_presets.py`, `build/measure/impl_ocean.py`). The checks were left unchanged.
- **GPU-timing flake.** `sky`'s GPU-timing check ("play then stop … max extra GPU 4.41 ms") failed once on the shared GPU and passed on the rerun.
- **Left out.**
  - C6: the per-frame TransmissionTarget reallocation (8.5 MB per frame) was measured only.
  - C4 multi-entry caching: not needed, because edge meshes are drawn over a subset copy.
  - The Renderer-scope keys and the glTF Extensions vendor tuning (`decal_tolerance_mm`, `interior_sky_reflection`) were not reclassified for C2.
