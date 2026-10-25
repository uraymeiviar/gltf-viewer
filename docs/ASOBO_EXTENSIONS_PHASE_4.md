# Phase A4 — Glass, Windshields, Glow, Opacity Effects

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** A4 first part implemented: glass v2 slab refraction and opacity-independent reflection; windshield v3 reflection. Remaining A4 work is planned.

**Entry:** A1.2 and A0.6 notes. A4 can start before A2/A3 because glass stays forward, but its final review
follows A3 (shadows) and A6 (sky). **Goal:** glass with refraction, reflection and blending (user priority 3), and
emissive glow.

## Tasks

- [ ] **A4.1 — `ASOBO_material_glass_v2` (R3). Owner: Claude.** MSFS glass mapped onto Glider's transmission path:
  refraction and reflection, with blending as MSFS does it. Glider's transmission path samples a mipmapped copy
  of the opaque scene (fixed in `51266d3`).
  - **Open:** what `glassWidth` controls (thickness, tint or refraction) comes from A0.6.
  - **Covers:** the cockpit's glass and the exterior's three materials (`MAT_GLASS`, `_bluish`, `_red`).
  - **Tests:** refraction through a slab of known width; tint; reflection at grazing angles.
- [ ] **A4.2 — `ASOBO_material_windshield_v3` and v1 (R4). Owner: Claude.**
  - Detail roughness and opacity masks, micro-scratch normals, detail-normal refraction.
  - Wiper mask, lines and state; the wiper state is a viewer setting, parked by default.
  - Confirm the `enabled` and `UVScale` keys the E195 data carries.
  - Rain is not simulated (A7).
  - **Reference views:** through the cockpit windshield, and the exterior cabin windshield.
- [ ] **A4.3 — Emissive glow (bloom) (R3). Owner: Claude.** A bloom post-process (threshold, downsample chain,
  intensity) so emissive panels and lights glow as in MSFS.
  - A user setting, on by default for ASOBO files only.
  - Applied before TAA and FXAA; the order is tested.
  - **Tests:** energy and threshold on a fixture of emissive patches; stable under TAA.
- [ ] **A4.4 — `ASOBO_material_fresnel_fade` and `ASOBO_material_ghost_effect` (R2). Owner: GPT Astra.**
  View-angle-dependent opacity, using the A0.6 formulas.
  - **Test:** opacity on a sphere at normal and grazing incidence matches the formula.

**Exit gate:**
- Glass and windshield are reviewed against the reference screenshots.
- Bloom on/off is verified, with files without ASOBO extensions byte-identical.
- Cost is measured.


## A4 first part — glass reflection and slab refraction (2026-10-25)

Implemented for forward BLEND materials with declared `ASOBO_material_glass_v2` or
`ASOBO_material_windshield_v3`, only while the corresponding viewport extension switch
applies. Both support-table entries are **Partial**. Legacy glass/windshield versions
are unchanged. A1.4 distance/offset sorting remains in place, with depth testing enabled
and depth writes disabled for the active glass draws.

Let `a = clamp(baseColor.a, 0, 1)`, `D` be the base/diffuse and emissive radiance,
`S` be the existing Fresnel-weighted GGX radiance (IBL plus punctual lights), and `T`
be base-colour RGB. In the basic metallic/roughness case:

- Windshield v3 emits `a*D + S`, alpha `a`, and uses `ONE, ONE_MINUS_SRC_ALPHA`.
  Thus the framebuffer receives the base layer at its coverage and reflection independently
  of that coverage. Existing optional sheen/clearcoat layers retain their layer composition.
- Glass v2 emits `B(uv_refract)*(1-a)*T + a*D + S`, alpha **1** for the completed scene colour.
  `B` is the existing **linear**, mipmapped transmission scene copy (LOD 0 for this clear
  slab approximation). The complete result is tone mapped once by the existing shader.
  Premultiplied `ONE, ONE_MINUS_SRC_ALPHA` blending replaces the destination when this
  completed output has alpha 1. A failed copy falls back to `a*D + S` and alpha `a`.
  Windshield composition retains the renderer's existing display-space blend convention (tone mapping occurs before framebuffer blending).
- For a unit surface-to-eye vector `v`, shading normal `n`, and IOR `eta` (default 1.5),
  `r = refract(-v, n, 1/eta)`, `exit = worldPosition + r*w/max(0.001, abs(dot(r,n)))`,
  and `uv_refract = project(projection*view*vec4(exit,1)).xy*0.5+0.5`.
  The divisor intersects the slab exit plane at normal thickness `w`. UVs clamp to half a
  scene-copy texel at each edge. Zero width samples at the original projected fragment.

`GltfRenderLayer::createScene` sets the active per-viewport material defines and width,
requests the existing transmission copy when needed, and excludes active ASOBO glass and
windshields from that copy. This avoids read/write feedback and deliberately does **not**
refract glass behind glass. With several glass surfaces, the nearer replacement draw can
replace farther glass; multilayer transmission is outside this part. Other transparent
objects continue using standard glTF blending, including immediately after a glass draw.
No new runtime disk assets were introduced. The follow-up below adds a floating-point
behind target for active glass v2.

**Verified:** `scripts/validate_asobo_glass.py` produces `build/review/glass_*.png` and
`glass_metrics.json`; checks alpha 0.02 reflection at head-on/grazing shading normals,
physical width 0 versus 0.3 m, base-colour tint, and byte-identical captures against
`build/baselines/ed36dd8.exe` for individual switches off, ASOBO off, plain glTF, and
KHR transmission without ASOBO. Exposure -3 keeps the small normal-incidence reflection
measurable without saturating the grazing reflection. Normals are varied with fixed
geometry/camera so both measurements use identical checkerboard pixels.
The support-matrix harness also passes. Aircraft captures and labelled side-by-side
MSFS comparisons are in `build/review/glass_e195_*` and `glass_e2_*`; exact commands are
in `glass_aircraft_commands.json`. The session report records visual observations.

**Width conversion (maintainer review):** the exporter UI is in mm and stores UI/100;
metres therefore equal stored/10. E2 stored 0.02 is a 2 mm slab; do not apply node scaling.
IOR 1.5 and this parallel-slab model remain an
approximation, not a recovered MSFS shader. Reflection uses the HDR environment and
punctual lights: there are no scene/interior reflections or SSR. The screen copy cannot
supply off-screen geometry; edge clamping can stretch border pixels. Windshield v3 has
no new refraction, masks, wipers, icing, scratch layers or rain. No MSFS environment,
airport geometry or avionics state is reproduced by these comparisons.


### Follow-up: E2 guard copy alignment and missing labels

The initial A4 copy was incomplete: the legacy transmission duplicates ran before the
decal prepass/accumulation, so the copied receivers sampled cleared accumulation targets.
The labels were absent from the behind image and the replacement glass draw erased them.
For scenes with active glass v2 only, command generation now schedules decal prepass and
accumulation before the behind duplicates and their capture. Ordinary transparent draws
are still copied; active ASOBO glass/windshields remain excluded. A1.4 sorting is unchanged.

The ASOBO behind image now renders into a viewport-sized `RGBA32F` framebuffer, with the
same render scale and MSAA count as the forward target. MSAA resolves at that size before
capture into a matching floating-point transmission texture. This avoids the old 1024-square
resample and RGBA8 linear-radiance clipping/quantization. The KHR-only/off paths retain their
original target, order and format. The extra floating-point target is reused between viewports.
Decal pass callbacks restore the appropriate behind target, and the capture callback restores
the ordinary forward target before clearing and shading it. Both copies read the populated
decal buffer. The transmission texture handle remains stable across storage changes.

Glass lookup is anchored to the actual render pixel, with a thickness-dependent offset:

```text
size = textureSize(sceneCopy, 0)
uv0 = gl_FragCoord.xy / size
surfaceClip = viewProjection * vec4(worldPosition, 1)
exitClip = viewProjection * vec4(exitPosition, 1)
uv = uv0 + 0.5 * (exitClip.xy/exitClip.w - surfaceClip.xy/surfaceClip.w)
```

Viewport origin is (0,0) in both targets, with OpenGL bottom-up Y; there is no extra Y flip.
At width zero the projected difference vanishes exactly. The reviewer's conversion remains
`w = max(0, storedGlassWidth) * 0.1`: E2 stored 0.02 means 0.002 metres (2 mm).

**Verified:** the synthetic fixture now includes an accumulated E-shaped decal and an ordinary
transparent strip behind neutral glass (white tint, alpha zero, unlit to suppress reflection).
The zero-width RGB comparison is exact with no AA and half render scale; MSAA4 has mean
absolute difference 0.144/255, maximum 5/255 at edges, and zero translation, consistent with
resolving linear radiance before tone mapping versus resolving already tone-mapped samples.
The real E2 neutral-glass probe also matches glass-off exactly inside the guard area.
Actual lit glass intentionally differs at width zero due to coverage and specular; see the
report for both raw lit and neutral metrics. `scripts/validate_asobo_glass.py` retains its
reflection, refraction, tint and ed36dd8 byte-identity checks; its large-width test uses stored
3.0, corresponding to a 0.3 m diagnostic slab after the conversion.

**Visual review:** STOP/RUN/START labels and knobs now remain readable through the E2
2 mm guards, with slight distortion. Residual lightening is not a tint lookup error: the
asset has white RGB tint, alpha 0.01 and an emissive factor of [1000,1000,1000] clamped by
the existing loader. Specular contributes most of the extra veil; the small covered
base/emissive layer also contributes. The HDR and MSFS lighting differ, so no exact strength
calibration is claimed. New comparisons are `build/review/glass_followup_e2_guards_comparison.png`
and `glass_followup_e195_hud_comparison.png`; metrics and diagnostic images share the prefix.
The Follow-up section of `a4-glass.report.md` supersedes the initial report's copy-size,
width-unit and visual-alignment observations.


### Follow-up 2: black MSAA glass and opaque viewport composition

**Verified cause:** the framebuffer and 8x RGBA32F resolve/capture were complete and
error-free on the test GPU, and their RGB radiance was nonzero. The shader wrote a fully
composed background-plus-glass RGB value with coverage alpha. ImGui blended that viewport
texture by alpha again, making the combiners look black. TAA and FXAA output opaque alpha
and hid the problem. The previous RGB-only viewport checks missed it; the early dark
window dumps were not just warm-up state.

A valid glass-v2 copy now emits the complete colour with alpha **1**; the material coverage
still weights the base and background terms internally. All active glass uses premultiplied
blending: alpha 1 replaces the destination, while a failed copy emits the existing
reflection-only `a*D + S`, alpha `a`, and blends safely over the forward scene. A per-draw
`u_AsoboGlassCopyValid` uniform starts false and is set by the capture callback only after
success. Failed/unavailable copies are never sampled. Windshield behaviour and inactive
extension paths are unchanged.

The floating-point behind target, resolve texture and transmission copy now use **RGBA16F**.
Both resolve and draw FBOs are validated; colour/depth RBO sample counts are compared;
the resolve changes only sample count, using identical formats/dimensions and NEAREST.
Capture reads the resolved attachment explicitly. Allocation, resolve, storage/mipmap and
capture errors are checked. Unsupported floating-point multisampling retries single-sample
HDR; unavailable HDR uses the legacy source and reflection-only transparent glass. Failure
logs once. Test-only `GLIDER_TEST_GLASS_COPY_FAILURE=allocation|multisample|resolve|capture`
injects failures before the corresponding GL work; each path is checked by the fixture.
Profile/capture runs now drain and report GL errors every frame.

**Verified:** E195 HUD and E2 guards were rendered with none, msaa2, msaa4, msaa8, taa and
fxaa, with zero GL errors and no unexpected fallback. Mean absolute RGB differences inside
eroded glass masks versus none, on a 0..255 scale:

| AA | E195 HUD | E2 guards |
|---|---:|---:|
| none | 0.000 | 0.000 |
| msaa2 | 0.131 | 0.222 |
| msaa4 | 0.143 | 0.246 |
| msaa8 | 0.147 | 0.242 |
| taa | 0.498 | 1.257 |
| fxaa | 0.400 | 1.383 |

Review images: `build/review/glass_f2_black_fix_comparison.png` compares actual window
presentation before/after; `glass_f2_e195_hud_aa_comparison.png` and
`glass_f2_e2_guards_aa_comparison.png` show all six modes. Exact commands, logs and mask
metrics share `glass_f2_` names. Synthetic tests assert alpha 255 in first and steady
complete-glass captures at none/2x/4x/8x, compare RGB against none, check MSAA alignment
and exercise the four safe fallbacks. All checks pass, including retained off-path byte
identity against ed36dd8. No regression suite or benchmarks were run.

**Nominal glass-copy storage at 1920x1080:** RGBA16F behind/depth/resolve plus the full
mipmapped transmission texture uses 44.82 MiB at one sample, 84.37 MiB at 2x, 131.83 MiB
at 4x and 226.76 MiB at 8x. The previous RGBA32F 8x storage was 390.23 MiB. These figures
exclude the ordinary viewport framebuffer, AA history, decal buffers and driver overhead.
RGBA16F versus the previous RGBA32F none captures differs by mean RGB 0.021/255 for the
HUD and 0.023/255 for the guards, with maximum 1/255 in the measured masks.


## A4 part 2 — windshield screen-space reflections and scene clear colour (2026-10-25)

Windshield-v3 materials now read the viewport-sized RGBA16F scene copy before
transparencies. The opaque pass already incorporates decal receivers. Their shader
marches the reflected view-space ray for 24/48/96 steps (Low/Medium/High), refines a
front-to-back depth crossing five times, rejects sky and out-of-screen samples, and
checks a distance-dependent depth thickness. Confidence fades at screen borders and
with ray travel. The hit uses the existing GGX LUT/Fresnel weight and blends with the
IBL fallback according to confidence times `ssrAttenuation` (default 1; zero disables
SSR). Punctual specular and opacity-independent glass blending remain in place.
The decal prepass depth is reused when present; other windshield views resolve the
opaque depth to a same-sized, single-sample DEPTH24_STENCIL8 texture. Glass-only and
KHR-only views retain their existing copy paths and do not allocate SSR depth.

Viewport Settings → MSFS Extensions adds **Screen-space reflections** (on), **SSR
quality** (Medium), and **Interior sky reflection** (0.5). CLI equivalents are `--ssr`,
`--ssr-quality`, and `--interior-sky-reflection`; see AGENTS.md §4.2. The sky strength
applies per fragment inside the combined world AABB of environment-occluder geometry,
collected before those meshes are hidden. Without a bounded occluder it applies to
all windshields. The reflection shader is compiled only for applied windshield-v3
materials. SSR remains limited to visible, opaque screen information; offscreen,
occluded and transparent objects use the fallback.

The clear-colour bug was two stale clear commands: Renderer and TransmissiveBlit
retained the startup grey after the application selected the scene colour. Both now
receive the active scene colour for each rendered viewport, so the forward clear and
HDR behind source track Environment → Clear Color together. `--clear-color r,g,b`
disables the skybox for reproducible captures; `--hide-grid` is a review hook.

**Verified checks:** `python scripts/validate_asobo_glass.py` includes the retained
part-1 checks and new SSR checks (`--ssr` runs the new subset). The tilted pane reflects
an orange emissive box under none, MSAA4, TAA and MSAA4+TAA; attenuation zero matches
SSR off byte for byte, with zero GL errors. Low and High both reflect the box. Inside
and outside occluder fixtures exercise fallback bounds. Plain, glass-v2 and KHR
transmission captures match `build/baselines/c68a9ae.exe` byte for byte. Two clear
colours pass all twelve combinations of MSAA Off/2/4/8 and Post AA None/FXAA/TAA;
neutral refracting glass follows the same two scene colours.

**Visual review:** `build/review/ssr_e195_on.png` and `ssr_e195_off.png` use a pilot-eye
view of the left windshield and panel; the comparison places them next to MSFS
`30.webp`. SSR adds reflected cockpit surfaces and orange panel markings. The source
asset's blank instrument displays also reflect as bright patches; Glider does not
provide the simulator's live gauge textures. Finite-step silhouette/disocclusion
artifacts remain visible in the unfiltered High capture. This establishes interior
reflection, not a calibrated match to MSFS lighting or its temporal filtering.
The external `a4-ssr.report.md` records exact commands, file/line references, image
names, GL checks, and measured E195 GPU costs. No regression runner was used.

**Verified final E195 cost:** RTX 5080, 1610×896, High/96 steps, AA none, sequential
on/off runs, 78 completed GPU samples (frames 20–97): median renderer pass sum
0.790512 → 0.947552 ms (**+0.157040 ms/frame**); transparent pass
0.018256 → 0.175504 ms. CPU draw submission 0.9568 → 0.9727 ms. Both variants retain
the HDR copy and reuse decal depth. Reports: `ssr_e195_off.json`, `ssr_e195_on.json`,
`ssr_e195_cost.json`. The final complete glass harness exited 0 with **79 PASS** checks.


### A4 part 2 follow-up — reject false SSR hits and depth streaks

This entry supersedes the earlier coarse marcher and its interpretation of the broad
white screen reflections as merely an asset limitation. The old 3 cm + 1% travel
thickness, sky-as-front sample and growing world steps admitted silhouette crossings
and hidden/back-side screen hits. The new marcher uses perspective-correct projected
steps (Low/Medium/High budgets 256/512/1024; maximum stride 2/1/1 pixels), a two-metre
maximum range, and terminates at the first obstruction. It requires a real opaque
previous sample in front, a continuous local depth plane agreeing within 1 mm, a
front-to-back plane crossing, and a camera-facing hit surface approached from the
front. Five refinements bracket an analytic plane hit. The final colour texel must
belong to that same small plane footprint. Edge, world/screen travel, incidence and
plane/depth error reduce confidence. Unknown silhouettes/back sides use IBL fallback.

`--debug-view ssr` / Render Mode → SSR shows hit U/V in R/G and confidence in B,
black for no hit. `ssrray` / SSR Ray shows the view-space ray encoded as RGB, magenta
for a rejected pane hemisphere. Existing back-face TBN and DirectX normal handling
remain unchanged. Normal diagnostics found no wrong-hemisphere rays in the three
review views; hit/occlusion validation was the substantive fix.

**Verified:** final incremental Release build and the complete glass harness pass
115 checks. The original emissive box reflects under every one of the twelve
MSAA Off/2/4/8 × Post AA None/FXAA/TAA combinations; a thin overhanging lip blocks
that reflection while the box remains camera-visible. All GL diagnostics are zero.
The existing clear-colour, glass and c68a9ae identity checks pass. The hangar-HDR E195
pilot/left/oblique captures show no debug hits in display interiors and remove the
broad display silhouettes and streaky bands. Nonzero accepted hits and subtle
interior glints remain. `build/review/ssr2_comparison.png` shows before/after/off;
`ssr2_fixture_comparison.png` shows box/lip behaviour. Diagnostics, masks and exact
commands share `ssr2_` names. The external `a4-ssr.report.md` Follow-up section records
formulas, evidence, limitations and the revised measured cost; the previous coarse
marcher's cost does not describe this version. No regression runner or commit was used.

**Status 2026-10-25:** SSR is committed **off by default** (Viewport Settings / `--ssr on`). The strict hit test
removed the false screen reflections and the large smears, but on the E195 the accepted hits flip pixel by pixel
(1 mm plane agreement at depth-buffer precision, nearest sampling), giving comb/dotted edges on reflections of the
overhead panel, EFB and glareshield; the HUD combiner (glass v2) shows similar streaks near bright edges. Next round:
soft confidence instead of a hard 1 mm cut, tolerance scaled to depth precision, smoothing of the SSR term, and a
check of the glass v2 refraction near bright discontinuities.



### Follow-up 3 — precision-aware SSR filtering (2026-10-26; partial result)

One implementation/check pass, per maintainer budget. The fixed 1 mm residual switch
is replaced with soft confidence scaled by D24/D32F depth quantization propagated
through the projection. Hit colour is bilinear only across a depth-consistent 2×2
footprint; nearest remains at discontinuities. A deterministic, normal-aware five-tap
filter smooths premultiplied hit colour/confidence along the noisier screen direction.
The previous first-obstruction, front-to-back, hit-side and pane-hemisphere guards stay.
SSR remains **off by default**.

**Verified:** Release build and the complete glass harness pass **128 checks** with zero
GL errors. A smooth planar reflection's normalized high-frequency energy falls from
2.905 on 4568db9 to 0.288 without AA and 0.020 with TAA. Default-off identity and all
12 AA combinations pass, including the thin lip that blocks the emissive box reflection.

**Visual acceptance remains incomplete:** E195 close-HUD white angular streaks and some
real-cockpit comb/fragmented regions remain. Baseline material ablations keep the
streaks when HUD emission/specular is removed, but SSR off removes them. The combiner's
`glassWidth` is zero and its behind sampler is already linear: the evidence points to
SSR over/through the combiner, not slab refraction. No glass-order/refraction changes
were made. Before/after pairs, TAA/no-AA contact sheets and an explicit unresolved crop
are `build/review/ssr3_*.png`; see `ssr3_hud_remaining.png`. Frames 64/96 differ locally,
so complete temporal convergence is not claimed. The external report's **Follow-up 3**
section records evidence, formulas, measurements and remaining work. No iteration
beyond the requested implementation/check pass, no benchmark or regression runner,
and no commit/push were performed.

### Follow-up 4 — continuous hit refinement; SSR on by default (2026-10-26)

SSR is now **on by default** (`--ssr off` / Viewport Settings to disable). The rays are traced about the geometric
normal (the tiled detail normal map bent each pixel's ray), and only the Fresnel weight uses the shaded normal.

The remaining concentric moiré on the E2 windshield came from the hit point, not depth precision. The hit was built
from the plane of the texel that first detected the crossing. Which texel that is jumps between neighbouring
windshield pixels, so the hit jumped with it. The crossing is now refined by bisection, then a linear zero crossing,
against a **continuous** surface: `ssrContinuousZ` interpolates view depth bilinearly across the 2×2 footprint, and
uses nearest depth across silhouettes and sky. The bracket is widened by one step, so the continuous crossing lies
inside it whichever texel detected it.

The per-pixel plane, hit-normal, incidence and residual terms are removed. Confidence is now the screen-edge fade,
the travel and screen-travel fades, and a fade over the last 30 % of the obstruction-thickness limit (no hard
stair-stepped cut at reflected silhouettes). Colour is sampled bilinearly.

E2 side and forward views without AA and with TAA: `build/review/ssrfix8_*.png`, zoom against the previous version
in `ssrfix8_zoom.png`. The glass harness passes 128 checks.

**Comb stripes (same day):** close to the glareshield, neighbouring columns alternated between hit and miss on thin
reflected surfaces. The `ssr` debug view, instrumented with reject reasons, showed that these rays marched past
the surface without detecting it: a point sample per pixel misses a surface the ray only crosses between samples.
Each step now tests the ray's depth interval across its pixel against the surface (McGuire & Mara 2014, "Efficient
GPU Screen-Space Ray Tracing"). Occlusion is decided after refinement: on a continuous surface the refined bracket
closes onto the crossing; across a silhouette it keeps the depth jump, and the ray passed under an occluder. This
test does not depend on the surface's slope. The `ssrray` view confirms the windshield normals are smooth (no
facets). Renders: `build/review/z_cmp_j.png`, `z_cmp_l.png` (old/new hit masks), `s_zoom_*.png`. A pixel-scale
sawtooth remains along some reflected silhouettes.

**Silhouette sawtooth:** the `ssr` reject-reason view showed long teeth where hits alternate with "passed under an
occluder" along reflected silhouettes. Neighbouring windshield rays are nearly parallel and visit nearly the same
pixels, so the decision flips slowly from ray to ray. The ray's start phase is now offset by interleaved gradient
noise (Jimenez 2014). The noise varies with `u_SsrFrame` (the viewport's TAA accumulation count, 0 without TAA). TAA
resolves it into a soft edge; without TAA it stays a fixed fine grain. Hits on continuous surfaces are refined
independently of the phase: smooth-plane flicker 0.02 in every AA combination. Renders: `build/review/t_cmp.png`
(before / no AA / TAA), `t_cmp_l.png`.

**Magnified source staircase:** at grazing angles many windshield pixels reflect one source texel, so the yes/no
hit decision enlarged the source image's pixel staircase into blocks several pixels wide along reflected
silhouettes. That's a resolution limit of SSR. Hits are now weighted by a percentage-closer coverage
(`ssrHitCoverage`, as PCF for shadow maps): the tent-weighted share of a 4×4 depth footprint lying on the hit
surface, remapped so that it reaches zero where the nearest-texel decision flips. The reflection fades out over its
last source texel instead of ending in a block. Renders: `build/review/u_cmp.png`, `u_cmp_l.png`.

**Glossy blur (roughness and magnification):** `windshieldSample` sets one blur radius in source texels: the larger
of half the reflection's magnification (windshield pixels per source texel, from the quad's hit-UV `fwidth`) and the
GGX cone (`travel · roughness² · focal / depth`), clamped to 1.5–16. The radius widens the coverage tent (on a grid of
radius/1.5 texels, so the weights stay continuous) and sets the colour level in the behind copy's mip chain
(`log2(radius/1.5)`; the chain is regenerated after each capture). A smooth windshield is unchanged except where it
is strongly magnified. Forced-roughness check: `build/review/w_rough.png` (0 / 0.3 / 0.6).


## A4 part 4 — glass roughness refraction blur & editable ASOBO parameters (2026-10-26)

**Glass material compatibility (`ASOBO_material_glass_v2`):**
- Two causes explained why changing `roughnessFactor` and `metallicFactor` had little or no visible effect on `MAT_SCREEN_GLASS` (the POWERPLANT START/STOP switch guards in `56.webp` / `57.webp`):
  1. When `SCREEN_GLASS_COMP.png` is bound as `metallicRoughnessTexture` (as on the E190/E195 `MAT_SCREEN_GLASS`), its green (roughness) and blue (metallic) channels inside the guard UV region (`u ∈ [0.0307, 0.2004], v ∈ [0.8046, 0.9708]`, mesh `C1_011.002`) have vertex-sampled mean `G = 3.26 / 255 = 0.0128` and `B = 0.0 / 255 = 0.0000` (full-image mean `G = 6.82 / 255`, `B = 0.06 / 255`). Because `material_info.glsl` multiplies `perceptualRoughness *= roughnessSample.g` and `metallic *= roughnessSample.b`, dragging `roughnessFactor` from `0.00` to `0.87` changes effective `perceptualRoughness` only from `0.0` to `0.011`, and `metallicFactor = 1.0` multiplies by `0.0` everywhere on the guard.
  2. In `pbr.frag`, `getAsoboGlassBehind` previously sampled `u_TransmissionFramebufferSampler` at mip `0` regardless of `materialInfo.perceptualRoughness`. At low glass coverage (`alpha = 0.01` on E2 `MAT_SCREEN_GLASS`), the pixel is dominated by the refracted background `(1 - coverage) * behind * baseColor.rgb`, so roughness affected only the specular reflection lobe and left the transmitted image completely sharp.
- `getAsoboGlassBehind` now selects a mip level in `u_TransmissionFramebufferSampler` using `roughBlur = max(0.0, perceptualRoughness - 0.08) / 0.92` and `lod = clamp(maxLod * applyIorToRoughness(roughBlur, ior), 0.0, maxLod)`. Clear glass (`perceptualRoughness <= 0.08`, including E2's default `0.05` and the synthetic alignment fixtures) continues to sample mip `0` with zero blur; rougher glass progressively samples higher mip levels of the RGBA16F transmission copy, frosting the refracted background while broadening the GGX specular reflection.
- Transmitted tint (`(1.0 - coverage) * behind * baseColor.rgb`) remains independent of `metallic`: MSFS glass documentation (`docs/ASOBO_EXTENSIONS_SEMANTICS.md`, `docs/ASOBO_GBUFFER_STUDY.md`) does not attenuate glass transmission by `(1 - metallic)`, whereas `metallic` continues to blend normal-incidence Fresnel `f0` from dielectric IOR reflectance (`0.04`) toward `baseColor.rgb` and scale the covered diffuse base `c_diff`.
- Non-glass files (`plain`, `khr`, `asobo_off`) remain byte-identical to `build/baselines/399c88b.exe` and `build/baselines/ed36dd8.exe`.

**Editable ASOBO extension parameters in the Inspector:**
- `ExtensionInspector` now renders interactive, schema-bounded ImGui controls for every typed scalar/vector/boolean/enum parameter in `gltf/extensions/AsoboExtensions.h` (`AsoboExtensionSpecs.cpp` + `resources/schemas/asobo/*.schema.json`): drag floats with schema `[min, max]` bounds, `ColorEdit3`/`ColorEdit4` for `*Color*`/`*Factor` arrays, checkboxes for booleans (including decal `enabled`), combo boxes for string enums, and integer drags for integer offsets/orders (`drawOrderOffset`). Texture slots and non-ASOBO vendor extensions remain read-only; extensions cannot be added or removed, and nothing is written to disk.
- Edits update both the in-memory `GltfObject` extension JSON and the typed runtime model (`GltfRenderLayer::updateMaterialStates`, which re-parses `material->asobo` and `ASOBO_normal_map_convention`, bumps `m_gltfGeneration`, and refreshes material uniforms/shader defines for the next frame). Decal pass collection (`DecalAccumulationLayer.cpp`, `GltfRenderLayer.cpp`) now also honours the schema-defined `enabled` boolean on `ASOBO_material_geometry_decal` and `ASOBO_material_blend_gbuffer`.
- Added repeatable CLI hook `--set-asobo-param <material> <extension> <param> <value>` and UI scenario command `asoboparam <material> <extension> <param> <value>` exercising the same `ExtensionInspector::setMaterialAsoboParameter` + `GltfRenderLayer::updateMaterialStates` code path.
- **Verified:** `scripts/validate_asobo_glass.py` passes all **133 checks** (including the rough-glass refraction high-frequency reduction check, `--set-asobo-param` byte-identity against authored fixtures, and `399c88b` / `ed36dd8` baseline byte-identity). E2 POWERPLANT guard compatibility renders (`build/review/glasscompat_*.png`) and Part B live edit/restore verification (`glasscompat_partb_*.png`, confirming byte-identical restoration when `glassWidth` is edited from `0.02` to `0.50` and back to `0.02`) pass with zero GL errors.
