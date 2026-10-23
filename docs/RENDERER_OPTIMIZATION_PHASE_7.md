# Phase 7 — IBL and Render Quality

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Complete, uncommitted ([§12.56](#section-12-56)). P7.1 PASS with a limited claim ([§12.51](#section-12-51)). P7.2 DEFERRED ([§12.52](#section-12-52)). P7.3 PASS as user options ([§12.53](#section-12-53)). P7.4 PASS as a user option ([§12.54](#section-12-54)). P7.5 render scale PASS as a user option; the depth-format change was DEFERRED ([§12.55](#section-12-55)).

Every change that alters the image is a setting under **Viewport Settings → Quality**, except the IBL filter settings (cube sizes, LUT, samples, precision), which moved to the **Scene Inspector** and show only while IBL is enabled. The defaults are unchanged: the **High (original)** preset is byte-identical to the pre-Phase-7 binary. The setting applies to every viewport and is saved in `%APPDATA%/Glider/render_quality.txt`, but not in `--isolated` runs.

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-9"></a>

### 5.9 Phase 7 — IBL and Render-Quality Experiments (R3–R4)

**Entry:** Phase 0 plus a fixed quality reference suite. These are optional experiments, not automatic
follow-ons from cache work; each requires its own quality budget agreed before implementation.

- [x] **P7.1 — IBL synchronization ([§3.12](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-12), R3).** Evaluate removing the final blocking finish while
  preserving dependency ordering. Measure first presented usable frame and total filter completion,
  not only the apparent duration of the submitting function.
- [ ] **P7.2 — Prebaked default IBL ([§3.12](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-12), R3).** Preserve filter parameters/formats initially; package
  through EmbeddedVFS. Test orientation, roughness mips and sheen. Retain custom HDR/EXR behavior.
  *DEFERRED: the High outputs would add about 400 MB to the executable ([§12.52](#section-12-52)).*
- [x] **P7.3 — IBL precision, dimensions and samples ([§3.12](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-12), R4).** Test half-float, cube/LUT resolution
  and sample count independently. Include bright HDR highlights, smooth metals, rough surfaces and sheen.
- [x] **P7.4 — Grid algorithm ([§3.13](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-13), R4).** Compare grazing angles, zoom extremes, depth intersections
  and both projection types; record visual tradeoffs as well as fill cost.
- [x] **P7.5 — MSAA/defaults/render scale ([§3.14](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-14), R4).** Separate allocation-format changes from changes
  to default samples/resolution. Validate depth/stencil requirements, thin edges, transparency, AA and
  picking coordinates. Lower quality is an explicit setting/product choice, not a transparent speedup.

**Exit gate:** quantified memory/startup/GPU benefit and accepted per-experiment image differences.
If no quality budget is agreed, retain the existing defaults and mark the experiment DEFERRED.

---

## 12. Implementation Records

Common setup for every record below. Base: `abdbad7` plus the uncommitted Phase 6 tree. The baseline binary was built from that tree and is saved as `build/p7/Glider_base.exe`.

The harness is `python scripts/validate_p7.py --baseline build/p7/Glider_base.exe`, with outputs in `build/p7/`. It generates two fixtures:
- A sphere grid: rows of metal, white dielectric and sheen spheres, each row with roughness 0 to 1.
- A synthetic HDR with a 1° sun at 2×10⁵, beyond half-float range.

Captures are of the active viewport (918×796 in a 1600×900 window), taken at frame 8. Test hardware is an NVIDIA RTX 5080 (driver 616.92) running OpenGL 4.1.

**Quality budget:** the user asked that any change that lowers image quality be a UI choice and never a hard-coded default. So each change below is a setting. Defaults stay at High, and presets combine the settings.

<a id="section-12-51"></a>

### 12.51 P7.1 — IBL Synchronization (2026-10-23)

**Status:** PASS. Images and dependency order are unchanged. The startup benefit is limited (§3).

#### 1. Change
- `IBLSampler::filter` no longer calls `glFinish`.
- The filter passes are still queued in the same order, ahead of any frame that samples their outputs.
- `destroyRuntimeResources` deletes the panorama and source cube right after submission. GL defers that deletion until the queued passes stop using them.

#### 2. Validation
- The High capture is byte-identical to the baseline (max diff 0).

#### 3. Measurement
- **Submission:** the time spent in `environmentFilter` drops from 229–307 ms to about 41 ms.
- **Where the wait went:** the GPU work does not shrink. Frame 2 now waits about 164 ms for it instead of startup.
- **Time to first usable frame:** measured as process start to the start of frame 3, median of 3 runs. It is within noise: 864–890 ms before, 834–864 ms after.
- **When it helps:** only when CPU work after the filter overlaps with it, for example parsing a large model at startup. This tiny fixture shows no such gain.

<a id="section-12-52"></a>

### 12.52 P7.2 — Prebaked Default IBL: DEFERRED (2026-10-23)

**Reason:** the task says to keep the filter parameters and formats. With the High outputs (RGBA32F, 1024² cubes with mips, 1024² LUTs), the resident result is about 400 MB, which would all be embedded in `Glider.exe`. Two smaller options were also rejected:
- **Prebaking only Balanced (about 134 MB):** it still multiplies the 17 MB HDR it would replace.
- **Prebaking only Performance (about 34 MB):** it would help only that preset.

Every other preset and every custom HDR would still filter at runtime, and the panorama must stay embedded for that.

**What gets most of the startup benefit instead:** P7.3. On this GPU the Balanced preset removes about 150 ms of the filter cost without packaging anything.

<a id="section-12-53"></a>

### 12.53 P7.3 — IBL Precision, Dimensions and Samples (2026-10-23)

**Status:** PASS. Each mechanism is a separate setting and was measured on its own. Defaults are unchanged.

#### 1. Changes

`InitizalizationParameters` gains:
- `lambertianSize`: the irradiance cube size, where 0 means the same as the specular cube.
- `halfFloat`: RGBA16F outputs.

The GGX sample count already existed in `SamplingParameters`. `GltfRenderLayer::iblParameters` and `iblSampling` replace the old `iblMaxResolution`.

`ibl_filtering.frag` clamps its output to `u_maxValue`. That value is 65504 for half-float outputs and 3×10³⁸ otherwise, so a bright sun clamps instead of overflowing to infinity. The source cube stays RGBA32F; it is temporary.

**Latent bug fixed:** both parameter structs implemented `operator==` as `memcmp(...)`, which returns true when the parameters *differ*.
- **Effect on sampling parameters:** `setSamplingParamters` ignored every real change.
- **Effect on `init`:** its "already initialized" early return fired only when the parameters differed. That return was also unsafe, because `setEnvironmentMap` has already destroyed the `EnvironmentMap` that owns the textures.
- **Fix:** the operators are now defaulted, and `init` always rebuilds.

#### 2. Results

Variants are compared with High on the default environment. `ready` is the median of 3 runs. `alloc` counts all texture allocations at startup, including the temporary panorama and source cube.

| Variant | ready ms | alloc MB | max diff | % px > 2 | Note |
|---|---:|---:|---:|---:|---|
| High (original) | 834 | 624 | — | — | byte-identical to the baseline |
| Irradiance 64 | 723 | 528 | 1 | 0 | irradiance at 1024² is 100 MB |
| Irradiance 32 | 724 | 528 | 1 | 0 | |
| LUT 512 | 836 | 588 | 1 | 0 | |
| LUT 256 | 865 | 579 | 2 | 0 | |
| Half float | 839 | 424 | 1 | 0 | resident IBL halves |
| Samples 512 | 821 | 624 | 2 | 0 | |
| Samples 256 | 838 | 624 | 6 | 0.54 | |
| Cube 512 | 675 | 264 | 18 | 0.92 | the skybox shows cube mip 0, so the background softens |
| **Balanced** | **714** | **358** | **2** | **0** | |
| Performance | 657 | 162 | — | — | renders at 75% scale, so its image size differs |

**Bright-sun HDR** (2×10⁵ sun), compared with High:

| Variant | max diff | % px > 2 | Note |
|---|---:|---:|---:|
| Half float | 1 | 0 | no black or NaN pixels in any variant |
| Irradiance 64 | 3 | 0 | |
| LUT 512 | 20 | 0.008 | sphere silhouettes only, about 60 px |
| LUT 256 | 24 | 0.009 | |
| Samples 512 | 77 | 3.6 | highlight rims on rough metals move |
| Balanced | 20 | 0.009 | |

The first Balanced draft used 512 samples and a 256 LUT. The bright-sun case showed that 512 samples is visible, so Balanced now keeps 1024 samples and uses a 512 LUT.

**Resident IBL memory** (from the UI estimate): High about 400 MB, Balanced about 134 MB, Performance about 34 MB.

#### 3. Presets

| | Cube | Irradiance | LUT | Precision | Samples | Grid | Scale |
|---|---|---|---|---|---|---|---|
| High (default) | 1024 | same as cube | 1024 | 32-bit | 1024 | screen ray | 100% |
| Balanced | 1024 | 64 | 512 | 16-bit | 1024 | plane mesh | 100% |
| Performance | 512 | 32 | 256 | 16-bit | 256 | plane mesh | 75% |

Changing an IBL setting re-filters the current environment at once; the live UI switch was checked. Any individual change makes the preset Custom. The IBL controls now live in the Scene Inspector under **IBL Filter Settings** (visible only while IBL is on) so that they sit with the environment they filter; the preset, ground grid and render scale stay under Viewport Settings → Quality.

**Command-line flags:** `--quality high|balanced|performance`, `--ibl-size`, `--ibl-irradiance-size`, `--ibl-lut-size`, `--ibl-precision 32|16`, `--ibl-samples`, `--grid-mode ray|plane`, `--render-scale`. Flags override the saved choice for that run and are never saved.

<a id="section-12-54"></a>

### 12.54 P7.4 — Grid Algorithm (2026-10-23)

**Status:** PASS as the option **Ground Grid: Plane mesh (faster)**. The default stays Screen ray.

#### 1. Change

The plane mesh is a square on y = 0 centered on the camera, as wide as the fade distance. Without `gl_FragDepth`, early depth testing stays enabled, and pixels off the plane are never shaded. Both programs share the same line, axis and fade code.

Testing found one fix: zoomed out, the camera's far plane clipped the square into a hard edge before the horizon. `GL_DEPTH_CLAMP` during the draw now gives that region depth 1, just as the ray cast's clamped `gl_FragDepth` does.

#### 2. Image comparison (plane mesh against ray)

| View | % px > 2 | Note |
|---|---:|---|
| Grazing (pitch 1°) | 0.001 | |
| Below plane | 0.001 | |
| Close (0.3 m) | 0.002 | |
| Far (400 m) | 22.9 | mostly within 8/255 |
| Far (3000 m) | 0.04 | |
| Orthographic | 0.001 | |

- **Isolated pixels:** the max-diff values of 124–189 come from 3–15 single pixels on line edges.
- **The 400 m case:** the large differences are in the minor-line moiré region, where the lines are denser than the pixels. Interpolated and ray-intersected positions round differently there. The difference is not visible side by side.

#### 3. Fill cost

This was measured with a temporary `GL_TIME_ELAPSED` query around the grid draw: 2560×1440 window, 500 frames, then removed.

| View | Ray (ms) | Plane mesh (ms) |
|---|---:|---:|
| Default | 0.015 | 0.005 |
| Grazing | 0.011 | 0.004–0.005 |
| Far | 0.022 | 0.007 |

That is about 3× less GPU time. In absolute terms it is negligible on this GPU. The benefit applies to integrated GPUs and 4K panels, which were not available to test.

<a id="section-12-55"></a>

### 12.55 P7.5 — MSAA, Defaults and Render Scale (2026-10-23)

**Status:** render scale PASS as a user option. The depth-format change was DEFERRED. The MSAA default was already 4×.

#### 1. Default sample count
`antiAliasingIdx = 2` (MSAA 4×) was already the default before this phase. Nothing changed.

#### 2. Depth-only buffer (DEFERRED)
- **Stencil use:** nothing in the renderer uses stencil. `AntiAliasing.cpp` only disables the stencil test.
- **Why no change:** D24S8 and D24 both use 32 bits per sample on current hardware, so switching saves no memory. The only format that would save memory, D16, adds depth-precision risk for no measured gain.

#### 3. Render scale (50/67/75/85/100%)
- **How it works:** the scene FBO, the camera viewport, AA and picking all use the scaled size, and `ImGui::Image` stretches the result to the panel. Mouse picking coordinates are multiplied by the same scale.
- **Picking check:** clicks scripted at three spheres and one empty spot, at 100%, 75% and 50% scale, picked the same node each time (or nothing, on the empty spot).
- **Memory:** at 75% the scene targets and their bandwidth fall to 56%.

<a id="section-12-56"></a>

### 12.56 Phase 7 Exit Gate and Live-Resize Fix (2026-10-23)

| Criterion | Evidence | Result |
|---|---|---|
| **Quantified benefit** | Balanced: startup ready time −14% (834 → 714 ms); IBL resident memory about 400 → 134 MB. Performance: about 34 MB and 75% fill. Grid: about 3× less GPU time. | PASS |
| **Accepted image differences** | Defaults unchanged and byte-identical. Every reduction is a user setting. On normal content Balanced stays within 2/255 everywhere. | PASS (as options) |
| **No regressions** | `validate_p4_exit_gate.py` PASS. `validate_p5_1.py` PASS (picking). `--revision-selfcheck` 85/85. `validate_p5_4.py` FAIL 3 on the candidate, but the baseline binary also fails 3, run back to back on the same machine. In both, idle frames sit just over the 130 limit (candidate 104–216, baseline 106–159), playback drops to about 12 fps in one run of each, and the polling reference varies from 350 to 1460 frames. The failing checks change from run to run, so this is machine timing noise. | PASS except the timing gate (environmental) |

#### Live window resize
- **The bug:** while the user dragged the window border, the UI froze and the new area stayed blank until the drag ended. Windows runs a modal loop during the drag, and `glfwPollEvents` does not return until it ends.
- **The fix:** the main loop body is now a `runFrame` lambda, also called from the framebuffer-size callback during the drag. A `frameInProgress` guard prevents re-entry, for example when a UI script resizes the window from inside a frame.
- **Verification:** a SendInput drag of the right border, with a screenshot taken while the mouse was still held.
  - Baseline: stale layout plus a blank strip.
  - Candidate: the docked UI re-laid out to the new width.
