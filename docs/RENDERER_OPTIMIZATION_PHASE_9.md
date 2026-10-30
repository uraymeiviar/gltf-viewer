# Phase 9 — Pass Caches, Startup Environment and Frame Values

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Complete, uncommitted ([§12.68](#section-12-68)). Profile stages and counters were added first ([§12.63](#section-12-63)). C4 overlay-depth reuse PASS ([§12.64](#section-12-64)). C1 shadow-cascade reuse PASS ([§12.65](#section-12-65)). C13 single startup environment load PASS ([§12.66](#section-12-66)). C2 frame-value settings without a renderer sync PASS ([§12.67](#section-12-67)). Round 10 (uncommitted, against `1db382b`): C6 per-viewport transmission copy PASS ([§12.69](#section-12-69)) and C3 asynchronous exposure meter PASS ([§12.70](#section-12-70)); exit notes [§12.71](#section-12-71). Round 11 (uncommitted, against `41df669`): C5 scene-build allocations and per-material policy PASS ([§12.72](#section-12-72)), C7 exact-input atmosphere and clock caches PASS ([§12.73](#section-12-73)); C9 was measured and left out; exit notes [§12.74](#section-12-74).

**Scope:** The candidates come from the 2026-10-30 audit of the three-viewport E195 use case (C1, C2, C4, C6, C13); round 10 adds C3. Every image stays byte-identical to `69cf29b` (round 10: to `1db382b`). Each cache has a same-binary A/B switch, `--disable-pass-cache`; the C3 meter has `--exposure-readback sync`.

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

**C6 measurement (not fixed).** Three-viewport E195, procedural sky, TAA in each viewport, steady frames: `textureAllocations` = 3 per frame and `textureAllocationBytes` = 8,549,760 per frame. Each viewport's transmission copy is reallocated on every frame. Fixed in round 10 ([§12.69](#section-12-69)).

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
  - C6: the per-frame TransmissionTarget reallocation (8.5 MB per frame) was measured only; fixed in round 10 ([§12.69](#section-12-69)).
  - C4 multi-entry caching: not needed, because edge meshes are drawn over a subset copy.
  - The Renderer-scope keys and the glTF Extensions vendor tuning (`decal_tolerance_mm`, `interior_sky_reflection`) were not reclassified for C2.

<a id="section-12-69"></a>

### 12.69 Round 10 — C6 per-viewport transmission copy (2026-10-31)

**Change.**

- `ViewportInstance::transmissionTarget` (`ViewportInstance.h`) gives each viewport its own full-resolution behind-scene copy (ASOBO glass) and SSR depth. Both are sized to that viewport only.
- The fixed 1024×1024 KHR transmission copy stays shared in `Application::transmissionTarget`.
- `Viewports.cpp` sets `transmissionCapture` to the rendering viewport's target around the main draw.
- The TransmissiveBlit callback (`Application.cpp`) works as follows:
  - It copies the full-resolution glass copy and the SSR depth into that target.
  - It copies the KHR copy into the shared one.
  - Before copying, it points the resource manager's framebuffer texture at the target it copies into. The passes after the blit therefore sample this render's copy, exactly as before.
- Previously, viewports of different sizes re-specified one shared texture on every render. With three E195 viewports that was 3 allocations and 8.5 MB per frame.

**Lifetime and invalidation.**

- A viewport's target is allocated lazily. The colour copy is created on the first full-resolution capture, and the depth on the first depth capture. Scenes without ASOBO glass or SSR allocate nothing extra.
- Storage changes only when that viewport's size changes (`TransmissionTarget::create/captureDepth` keep their existing size checks).
- Materials hold the `gl::Texture*` and read its handle at bind time. Material builds query the current texture's level-0 size (`u_TransmissionFramebufferSize`), so the callback re-points the texture on every capture, as the shared target did.
- On viewport close, `releaseViewportTransmission` moves the target to `transmissionSpares`. `pruneTransmissionSpares` frees the spares whose colour texture is not the current framebuffer texture and whose depth is not `CommandGenerator::getSsrDepth()`.
  - A sampler can therefore never hold a deleted handle.
  - A new viewport adopts a spare first.
- Shutdown clears the capture pointer and the spares, then destroys the shared target.

**Measurement.** E195, three TAA viewports (272×976, 550×976 and 273×976), procedural sky, steady frames, `build/measure/impl2_c6.py`. The runs were interleaved pairs against `1db382b`.

| | 1db382b (pair 1 / 2) | Current (pair 1 / 2) |
|---|---|---|
| `textureAllocations` per frame | 3 / 3 (8,549,760 B) | 0 / 0 |
| CPU frame (median) | 44.06 / 44.52 ms | 23.73 / 23.33 ms |
| `draw.submit` (median) | 26.6 / 27.2 ms | 6.27 / 6.08 ms |
| TransmissiveBlit GPU (context only) | 6.78 / 6.39 ms | 0.54 / 0.45 ms |

The GPU was shared, so GPU times are context only. An earlier pair at lower contention showed the same shape: CPU 20.5 → 17.5 ms, `draw.submit` 10.2 → 5.9 ms.

**Memory cost.**

- The per-viewport RGBA16F copies with mips are 2.83 + 5.73 + 2.84 MB in this case. The shared 1024² copy is 11.2 MB.
- `1db382b` held one texture of whichever viewport rendered last.
- Net cost is about +20 MB with three glass viewports.
- Each viewport that captures SSR depth also holds its own depth texture (w·h·4 B).

**Equivalence.** All checks were run against `1db382b`:

- **Whole-window shots, 0 differing pixels** (`build/measure/impl2_shots.py`):
  - three viewports with sky and with the HDR;
  - the cockpit view;
  - resize, close and reopen of a viewport;
  - MSAA, a debug-view mix, and ASOBO extensions off.
- **`--dump-frame`, 0 differing pixels** (`impl2_dumps.py`): the default view, the E195 default view and the E195 cockpit with the sky, with manual and with Auto exposure. The images were viewed.
- **Suites, PASS:** `review_fixes`, `viewport_image`, `settings_scopes`, `agent_control`, `aa-target-selfcheck`, `p8_1`, `p8_3`, `p8_5`, `p8_followup`, `asobo_common`, `asobo_fresnel_ghost`, `sky`, `time_of_day`, `sun`, `sun_source`, `ambient`, `background_projection` and `night`.

<a id="section-12-70"></a>

### 12.70 Round 10 — C3 asynchronous exposure meter (2026-10-31)

**Change.**

- `AntiAliasing::updateAutoExposure` (`AntiAliasingBloom.cpp`) still draws the 64×64 RG32F meter (R luma, G peak) every metered render.
- Interactively, it no longer reads the meter back with a synchronous `glReadPixels`. Instead, `issueMeterReadback` writes it into a pixel-buffer ring (3 slots), followed by a `glFenceSync`.
- The highlight meter's depth is copied as well:
  - The scene depth (D24S8) is blitted into a D24S8 texture.
  - A full-screen pass (`s_meterDepthFS`) writes it into an R32F target, which is read into a second buffer.
  - A depth `glReadPixels` into a buffer still stalled on the driver, for both `GL_FLOAT` and `GL_UNSIGNED_INT_24_8`.
- `consumeMeterReadbacks` works as follows:
  - It polls the oldest slot's fence with a zero timeout.
  - When the fence is ready, it maps the buffers and evaluates the target with that render's own parameters (`MeterReadback::params`).
- `evaluateMeter` / `adaptExposure` are the previous math, moved unchanged. The exposure adapts towards the newest available target, which is typically 1 frame old and at most 2 or 3.
- The synchronous path is unchanged and is used in these cases:
  - **Capture runs:** `--fixed-frame-time`, `--dump-frame`, `--capture-viewport`, `--capture-first-frame` and `--ui-script` snap Auto to the meter every frame, so pipe and CLI images stay byte-identical.
  - Snaps, and the first metered render of a viewport.
  - `--exposure-readback sync`.
- `--exposure-readback async` forces the asynchronous path. The default `auto` picks it only outside capture runs.

**Idle loop and invalidation.**

- `isExposureAdapting()` also reports a pending meter drain. When the exposure stops moving, the viewport keeps rendering until the readback of a render issued after it settled has been consumed (at most `kMeterDrainRenders` = 8 renders). A still image is therefore metered too.
- The drain is armed once per change of the meter's input. `HdrPostProcessParams::meterInputChanged` is set when the viewport's render key changed (camera, scene, environment, size or settings).
  - Renders that only follow the exposure or settle TAA do not re-arm it, so the TAA jitter's meter noise cannot keep a still viewport rendering.
  - An exposure-compensation edit changes the key, so the meter re-reads and adaptation resumes. The new `sky` check caught that this was missing.
- Pending readbacks are dropped when:
  - the path falls back to synchronous;
  - the meter targets are released or resized;
  - the viewport is closed (`Viewports.cpp`).

**Counters and state.**

- Profile frame counters: `exposureSyncReadbacks`, `exposureAsyncReadbacks` (issued) and `exposureAsyncResults` (consumed).
- `get_state` `night.exposure.viewports[].meterReadback` is `"async"` or `"sync"`.

**Measurement.** E195, three TAA viewports, procedural sky, Auto exposure (highlight meter), every frame rendered. Runs were interactive (`--ui-script-follow`, no capture snap) and interleaved, `build/measure/impl2_c3.py`. Each cell gives `post.process` median, then CPU frame median, both in ms.

| Pair | 1db382b | Current, `--exposure-readback sync` | Current (async) |
|---|---|---|---|
| 1 | 1.65 · 21.0 | 8.29 · 23.9 | 0.81 · 15.4 |
| 2 | 5.97 · 44.3 | 8.00 · 24.5 | 0.79 · 15.3 |
| 3 | 5.14 · 39.4 | 7.80 · 23.6 | 0.79 · 15.6 |
| final build | 1.62 · 21.2 | 7.84 · 23.3 | 0.77 · 15.5 |

nvidia-smi before/after each run ranged 4–100 % (inference VM); the baseline arm's spread follows that contention.

- Counters per frame: sync 3 / async 0 with `sync`, and async 3 issued / 3 consumed by default.
- The same-binary sync arm is the fair reference. Its `post.process` includes the readback wait that `1db382b` partly spent elsewhere (C6's blit stall, before this round).
- The Curve exposure, which has no meter, costs 0.18–0.24 ms.

**Exposure equivalence.** The interactive sessions were settled and polled with `get_state` (`build/measure/impl2_converge.py`). The values are EV per viewport.

| Scene | sync | async |
|---|---|---|
| sky, 3 viewports | 0.00804 / −0.13604 / 0.00777 | 0.00803 / −0.13602 / 0.00773 |
| sky, cockpit in viewport 1 | 2.22218 / −0.13604 / 0.00764 | 2.22135 / −0.13600 / 0.00773 |
| HDR, Auto | −0.28738 / −0.45268 / −0.28744 | −0.28727 / −0.45253 / −0.28734 |

- Both arms settle at the same wall time, without reversals.
- The cockpit viewport's 0.0008 EV difference is within the TAA meter noise. Synchronous repeats alone range over 2.2155–2.2222.

**Checks.**

- **New:** `validate_sky.py` check `exposure` (`check_async_exposure`).
  - It runs a sky session with `--aa none` and Auto exposure, once with `--exposure-readback sync` and once with the default.
  - Both must settle on the same EV within `EXPOSURE_ASYNC_TOLERANCE` = 2e-3, before and after `exposure_compensation` 1.5 over the pipe. Result: −0.42526 = −0.42526.
  - `get_state` must report the readback each session used.
  - Both sessions must show zero GL errors.
- **Byte identity:** capture runs keep the synchronous path. All shots and dumps of [§12.69](#section-12-69) show 0 differing pixels on the final build.
- **Suites, PASS against `1db382b`:** the list in §12.69, rerun on the final build for `sky`, `night`, `agent_control`, `settings_scopes`, `review_fixes` and `asobo_common`.

<a id="section-12-71"></a>

### 12.71 Round 10 Exit Notes (2026-10-31)

- Nothing was reverted. Every image is byte-identical to `1db382b`, and no check was loosened.
- The interactive Auto exposure now lags its meter by 1–2 frames, typically 1. Capture runs are unchanged.
- **Left out.**
  - C6 does not share one full-resolution copy between equally sized viewports, because each viewport must keep its own copy while the others render.
  - C3 still issues the meter draw and the depth copy on every metered render. It reads back only asynchronously.

<a id="section-12-72"></a>

### 12.72 Round 11 — C5 scene-build allocations and per-material policy (2026-10-31)

**Change.**

- `GltfRenderLayer::createScene` (`GltfRenderLayerTraversal.cpp`) returns `{ std::move(scene), std::move(reverseMapping) }`. Before, the tuple was copy-constructed from the two locals: every drawable's list cell and every reverse-map node was allocated twice and the originals freed.
- `Defines` (`Defines.h`, `Defines.cpp`) uses heterogeneous lookup: `NameHash` (`is_transparent`, `std::hash<std::string_view>`) with `std::equal_to<>`. `define` of an existing name, `undefine` and `isDefined` no longer build a `std::string`; only a newly inserted name allocates. Names longer than the SSO limit (`ASOBO_MATERIAL_WINDSHIELD_LEGACY`, `ASOBO_GLASS_REFLECTION`, ...) allocated on every call before. This also makes the per-drawable `DEBUG` / `RAW_SSR_OVERLAY` / `SKY_OCCLUSION` defines in `Viewports.cpp` and the `CommandGenerator` lookups cheaper.
- The per-viewport material policy in `createScene` (transmission pass restore, the ASOBO glass / windshield / anisotropy / SSS / sky-portal defines and uniforms, draw-order flags) and the render-settings half (`fragmentShader`, `renderSettings`, `LINEAR_OUTPUT`) run once per material per call instead of once per primitive. `Material::scenePolicyMark` / `sceneSettingsMark` hold the serial of the last call that applied each half. The serial is a process-wide atomic, one per `createScene` call.
- `drawOrderActive` (a viewport option lookup) is evaluated once per call instead of per primitive.

**Rules.**

- The policy depends only on the material, the call's options (`asoboOptions`, `materialExtensionOptions`, render settings) and the traversal (`skyPortalsActive`, the interior volume). A repeat within one call wrote the same values again. Every call takes a new serial, so nothing carries over between viewports, passes or option changes.
- The scene flags the policy raises (`windshieldSsr`, `asoboGlassCopy`, `containsTransmissiveMaterial`) are only ever set to true; the material's first surviving primitive sets them.
- The per-primitive half stays per primitive: culling, the draw-order bounding centre, the mesh defines and `polygonMode`.
- `std::hash<std::string_view>` equals `std::hash<std::string>` for the same characters (the standard requires it). Buckets and the iteration order of `toLines()` / `hash()`, i.e. the shader source and the program keys, are unchanged.

**Measurement.** E195, three TAA viewports, steady frames (`build/measure/impl3_bench.py`), interleaved against `41df669`. Medians per frame (three viewports), ms. GPU 4–100 % busy (nvidia-smi before/after in `build/measure/impl3/*_summary.json`).

| | `41df669` | C5 build |
|---|---|---|
| `scene.create`, sky, move + `Defines` only (pair 1 / 2) | 2.70 / 2.82 | 1.76 / 1.92 |
| `scene.create`, sky, with the per-material policy | 2.86 | 1.08 |
| `scene.create`, HDR, with the per-material policy | 2.61 | 0.99 |
| `scene.build`, sky / HDR | 5.55 / 5.12 | 3.59 / 3.35 |

**Equivalence.**

- `--dump-frame` of the default view, the E195 default view and the E195 cockpit with the sky (manual and Auto exposure): 0 differing pixels (`build/measure/impl3_dumps.py`). The images were viewed.
- **Suites, PASS against `41df669`:** `settings_scopes`, `p4_exit_gate`, `p8_1`, `p8_3`, `p8_5`, `p8_followup`, `review_fixes`, `viewport_image`, `asobo_common`, `asobo_draworder`, `asobo_anisotropy`, `asobo_sss`, `asobo_sky_portal`, `asobo_fresnel_ghost`, `asobo_decal_modes` and the ten self-checks.

<a id="section-12-73"></a>

### 12.73 Round 11 — C7 exact-input atmosphere and clock caches (2026-10-31)

**Change.**

- `atmosphereTransmittance` (`SkyAtmosphere.h`; a 512-step double quadrature, 8 µs per call) is now a cache in front of `atmosphereTransmittanceUncached`.
  - The key is the exact bits of every input the integral reads: `mieDensity`, `ozone` and the direction's three floats.
  - `groundAlbedo` and `airglow` are not read by the integral and are not in the key, so viewports with different ground albedo share entries.
  - 8 entries per thread, round-robin replacement.
- `evaluateTimeOfDay` (`TimeOfDay.h`) is a cache in front of `evaluateTimeOfDayUncached`.
  - A call costs 0.9 µs with Custom hours and 0.11–0.14 ms with a preset, which searches the sun events.
  - The key is the bits of all 14 `TimeOfDayState` fields; 4 entries per thread.
  - A `static_assert` on `sizeof(TimeOfDayState)` fails the build when a field is added, until it joins the key.
- Every caller is unchanged and now hits the cache: `effectiveSun`, `moonEvaluation` / `evaluateMoon`, `effectiveDirectionalLight`, `exposurePlan` / `outdoorIlluminance`, the sun disk, the star extinction, the ambient light, `emissiveNightBlend`, the Scene Inspector and `get_state`. With an unchanged sun and clock, a steady frame evaluates no integral and no ephemeris (about 27 integrals per frame with three viewports before).

**Rules.**

- Both functions are pure. A hit returns the value the uncached function returned for bit-identical inputs.
- There is no tolerance, the quadrature is unchanged, and `-0.0` / `+0.0` are different keys.
- Thread-local storage needs no locking.
- A microbenchmark of 5,000 transmittances and 3,000 clock evaluations over random, repeating inputs (hits and misses mixed) found 0 bitwise differences from the uncached functions. A hit costs about 0.01 µs.

**Measurement.** E195, three TAA viewports, interleaved against the C5 build, ms per frame (pair 1 / 2). "CPU w/o GPU waits" is `cpuFrameMs` minus `draw.submit` and `post.process`, which carry the shared GPU's waits.

| | C5 build | C5 + C7 |
|---|---|---|
| sky: `scene.build` − `scene.create` | 2.45 / 2.42 | 2.41 / 2.37 |
| sky: CPU w/o GPU waits | 6.54 / 6.47 | 6.19 / 5.89 |
| clock with the `morning` preset: `scene.build` | 3.57 / 3.42 | 3.26 / 3.21 |

The gain is small: about 0.2–0.4 ms per frame with the sky, more with a preset clock, as estimated from the call count.

**Equivalence.**

- The `--dump-frame` set of [§12.72](#section-12-72): 0 differing pixels.
- **Suites, PASS against `41df669`:** `sky`, `time_of_day`, `sun`, `sun_source`, `ambient`, `sun_disk`, `night`, `settings_scopes` and `p4_exit_gate`.

<a id="section-12-74"></a>

### 12.74 Round 11 Exit Notes (2026-10-31)

**Overall.** Final build against `41df669`, interleaved, two pairs per arm (pair 1 / 2), GPU 4–20 % busy, ms per frame.

| Arm | `scene.create` | `scene.build` | CPU w/o GPU waits | `cpuFrameMs` |
|---|---|---|---|---|
| sky | 2.50 / 2.62 → 1.05 / 0.94 | 5.02 / 5.17 → 3.51 / 3.30 | 8.27 / 8.87 → 6.71 / 6.03 | 16.4 / 17.6 → 15.7 / 14.4 |
| HDR | 2.60 / 2.46 → 0.93 / 0.90 | 5.05 / 4.97 → 3.28 / 3.21 | 8.40 / 7.93 → 6.09 / 5.78 | 14.4 / 13.3 → 11.8 / 11.4 |
| sky, clock `morning` | 2.54 / 2.47 → 0.88 / 0.79 | 5.15 / 5.08 → 3.27 / 3.13 | 8.74 / 8.55 → 5.94 / 5.49 | 17.3 / 16.7 → 14.4 / 13.5 |

- **C9 left out.** `mergeInstances`' bucket map and scratch vectors were kept as members (bucket lists cleared, not freed; unused buckets evicted with the command cache). This was implemented, measured against the same build without it, and reverted:
  - E195 ×3, `commands.generate`: 1.79 / 1.57 → 1.83 / 1.64 ms (sky), 1.08 / 1.50 → 1.34 / 1.30 ms (HDR).
  - 1,600-drawable instancing grid, three viewports (`build/measure/impl3_c9grid.py`): 0.37 / 0.50 → 0.45 / 0.48 ms.
  - No gain above noise.
- **GPU-timing flake.** `sky`'s time-sliced check ("38 swaps of complete reduced-size jobs", needs 40) failed once with the GPU 100 % busy. It passed on the rerun (42 swaps), and the same suite passed on the C7 build.
- Nothing else was reverted. Every image is byte-identical to `41df669`, and no check was changed.
