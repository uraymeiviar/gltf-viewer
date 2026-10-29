# Phase A3 — Sun, Shadows, Macro Lights, SSS, AO, Ground, Sky Portals

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** In progress: A3.1, A3.2, A3.4–A3.10 implemented (A3.4 ahead of A3.1); A3.3 deferred. A3.10 validation numbers are in its task report.

**Entry:** A2.2, the deferred lighting pass. **Goal:** shadows from a sun (user priority 4), and the remaining
lighting features of user priority 5. The sun's direction is set by hand here and driven by time of day in A6.

## Tasks

- [x] **A3.1 — Sun light (R3). Owner: Claude.** A directional light with direction (azimuth and elevation, set by
  hand), color and intensity, in ~~Viewport Settings~~ Scene Inspector → *Sun & Time of Day* → *Sun* (scene-level, like
  exposure and time of day: one sun per glTF scene, shared by every viewport).
  - **A3.10 update:** the historical Sun checkbox / Sun Mode / Sun Follows Time of Day controls below are now one
    **Sun Source** selector. From HDR is locked to the panorama regardless of the independent clock. See A3.10 below
    for source-specific choices and ordered legacy mappings; the original light units and BRDF are unchanged.
  - **Done 2026-10-28.** Conventions (the reference for A3.2, A3.7 and A6; code: `header/imgui/SceneEnvironment.h`,
    `IBLSampler.h` `EnvironmentSun`, `pbr.frag` after the punctual loop):
    - **Angles:** glTF world space, Y up. Azimuth in degrees around +Y, measured from +Z towards +X (0 = the sun
      in the +Z direction, 90 = +X); elevation in degrees above the XZ plane. Direction to the sun =
      `(cos el sin az, sin el, cos el cos az)`. A6.1 sets these two angles (aircraft heading included).
    - **Units:** intensity is the illuminance on a surface facing the sun, in the environment's radiance units x
      steradian (an HDR texel of value L over solid angle w contributes L w). A white Lambert surface facing the
      sun reflects `colour x intensity / pi`, exactly what the IBL gives for the same light inside the HDR, so
      sun and IBL are balanced by construction. The colour is linear RGB of luminance 1 for white (the Kelvin
      helper, Kang 2002 fit, and From HDR give luminance-1 colours). Default: off, az 135, el 45, white, 8,
      angular radius 0.27 deg.
    - **Modes:** *Manual* (the values above) and *From HDR*: the HDR's sun, turned with the HDR rotation
      (`world = transpose(u_EnvRotation) * cube direction`) and scaled by the HDR intensity.
    - **HDR sun detection** (on every panorama load, CPU, read only): seed = the brightest ~1-degree block; sky
      level B = median luminance of the 5-10 degree ring; region = texels brighter than T = 4 B within 12 degrees,
      8-connected to such a texel within 2 degrees. The sun is what removal takes away: illuminance =
      luminance of `sum((L - L') dw)`, colour = that RGB sum / illuminance, direction = the excess-weighted mean.
    - **HDR sun removal (no double sun):** while the sun is on, in **either** mode, every region texel is scaled to
      luminance T keeping its hue (continuous at the region edge) before the cube map is built, so the IBL
      filters and the background lose the HDR's sun and its energy moves to the analytic sun. Toggling the sun
      re-loads and re-filters the environment once (100-700 ms); nothing re-filters per frame. Sun off: the HDR
      and the IBL are exactly as before (byte-identical).
    - **Default HDR's sun** (`default_hdr_env4k.hdr`): az 51.44, el 7.85 deg (rotation 0), illuminance 0.00658,
      colour (1.51, 0.91, 0.37), 595 texels / 0.0014 sr, sky 2.02, peak 38.6. The sun sits behind clouds at
      sunset: the sky gives about 2.5 on a horizontal surface, so From HDR adds almost nothing on this HDR, and
      removal only flattens the brightest cloud edges near it. HDRs with a visible sun disk get a strong sun (the
      synthetic check: direction within 0.02 deg, illuminance within 0.4%).
    - **Shading:** `u_SunDirection`, `u_SunEnabled`, `u_SunIlluminance`, `u_SunAngularRadius` in the
      EnvironmentBlock (offsets 128-159); `pbr.frag` runs the punctual-light terms (diffuse incl. iridescence,
      specular incl. anisotropy, sheen, clearcoat, transmission/volume, `asoboSssDirectDiffuse`) under
      `if (u_SunEnabled != 0)`. The specular lobe is widened by the angular radius (`alpha' = alpha + tan(r) / 2`,
      Karis 2013; GGX stays normalised), so a mirror shows a disk-sized highlight. Only `pbr.frag` lights
      surfaces (decal receivers and glass/windshield materials run through it).
    - **Shadow hook (A3.2):** `float sunVisibility = 1.0;` multiplies the sun's intensity; A3.2 replaces it by the
      cascaded shadow term. SSS: the visibility belongs on the wrapped front part only; the transmitted part needs
      the shadow seen from the light's side (comment at the SSS call).
    - **Settings / CLI:** `sun`, `sun_mode`, `sun_dir` (vec2 az,el), `sun_azimuth`, `sun_elevation`, `sun_color`,
      `sun_intensity`, `sun_angular_radius` (`set_environment`, `list_settings`); `--sun on|off`,
      `--sun-mode manual|hdr`, `--sun-dir <az,el>`, `--sun-color <r,g,b>`, `--sun-intensity <x>`,
      `--sun-angular-radius <deg>`. `get_state` and `--profile-report` carry a `sun` object (effective sun and the
      detected HDR sun).
  - **Check:** `scripts/validate_sun.py` (quick regression, serial list): Lambert sphere (KHR_materials_specular 0,
    black environment) lit from az 30 / el 20: fitted direction 0.12 deg off, fitted intensity -1.8%, brightest
    plateau 1.9 px from N = L, terminator exact outside +-0.03 N.L, +90 deg azimuth turns the light by 89.3 deg,
    pipe probes = intensity x N.L / pi within 2% + 2e-3; synthetic HDR From HDR: direction 0.015 deg, illuminance
    -0.4%, rotation followed, sun disk gone from the background, IBL-lit sphere mean -1.1% against sun off;
    default HDR detection stable across runs/quality presets; KHR punctual light + sun additive; SSS and
    anisotropy respond, finite; mirror highlight grows with the radius; sun off byte-identical to the baseline
    (HelloCube, fixtures, two E195 ASOBO views); zero GL errors.
  - **Review:** `build/review/sun/contact_sheet.png` (`scripts/review_sun.py`): HelloCube, E195 exterior and DA62
    cockpit with sun off / From HDR / two manual directions. The cockpit is lit through the airframe (no
    shadows yet: A3.2).
  - **Deviations:** the section lives in the Scene Inspector, not Viewport Settings; Manual mode also removes the
    HDR's sun; no viewport sun-direction indicator yet.
  - **Intensity and the environment map:** the HDR environment already contains a sun, so the sun's intensity and
    the IBL are balanced to avoid lighting it twice. Until A6 provides a matching sky, a documented rule is used:
    for example, dimming the IBL's direct-sun contribution, or a sun-free default HDR.
  - **Tests:** a Lambert sphere lit from a known direction.
- [x] **A3.2 — Cascaded shadow maps (R4). Owner: Claude.** Shadows from the sun: cascades fitted to each
  viewport's frustum, and filtering (PCF), applied in the forward `pbr.frag` sun term.
  - **Done 2026-10-28.** Only while the sun is on; sun off or shadows off runs no shadow pass and is
    byte-identical to the baseline (sun on + shadows off = the A3.1 sun-on image).
    - **Cascades** (`renderer/CascadedShadows.h`): the camera's depth range is cut to the world AABB of the
      scene's drawables (static POSITION boxes; the loose node extents are added for skinned/morphed/quantized
      ones) and to the max distance, widened to whole 1/8 octaves; practical split scheme (lambda, default
      0.75). Each cascade is the bounding sphere of its frustum slice computed in view space (radius independent
      of camera motion, rounded up to 1/64 of its power of two), in a light basis that depends only on the sun
      direction, with the centre snapped to whole texels. Depth runs from the far side of the sphere to beyond
      the scene's last point towards the sun, so off-screen casters cast. Blend over the last 10% of each
      cascade; fade over the last 10% of the range when the max distance cuts the scene.
    - **Caster pass** (`renderer/ShadowCasterLayer.h`, `shadow_caster.frag`): one depth-only draw per cascade
      into a `GL_DEPTH_COMPONENT32F` 2D array (one layer per cascade, one FBO per layer), from a second, unculled
      `createScene` (shared traversal) with each cascade culled by `cullToClip` against its light volume. The
      drawables are reused, so skinning, morph targets and P8.3 instancing work; caster material copies are
      pooled and refreshed only when the model, a material, the render settings or the viewport settings change.
      Casters draw without face culling (open cockpit geometry blocks the sun from both sides) and with
      `glPolygonOffset(slope 2, units 1)` (`Material::depthBias*`).
    - **Casters:** OPAQUE and MASK (same cutoff) cast; BLEND and KHR transmission do not (no documented reason
      to). A1.7 dithered materials cast with the Bayer pattern in shadow-map texels, fixed (dither frame 0 in the
      shadow pass), which the filter turns into partial shadow close to the alpha. Not casting:
      `ASOBO_material_shadow_options.noCastShadow` (while the viewport applies the extension: support status now
      Rendered, with a viewport switch), invisible materials and environment occluders (also while shown),
      geometry/blend-gbuffer decals, glass and windshield materials (ASOBO types or the extras material code;
      MSFS assumption, inferred: the canopy lets the sun into the cockpit). Hidden and isolated nodes follow the
      viewport's render filter (the same traversal).
    - **Receivers** (`shadow.glsl`): `sunShadowVisibility(worldPosition, offsetNormal)` is the reusable lookup
      (A3.5/A3.6/A3.7 include the file; the generator binds `u_SunShadowMap` to every `pbr.frag` command, last,
      so no other sampler moves). Cascade by view depth, normal offset `texel x normalBias x (0.2 + sin angle)`
      along the geometric normal, hardware compare (`GL_LEQUAL`, bilinear). Filters: hard (1 compare), PCF
      (3 x 3 compares), soft (16 Poisson compares, radius 2.5 texels, rotated by per-pixel interleaved gradient
      noise that does not change over time).
    - **SSS (A3.4):** the wrapped front part takes `sunVisibility`; the transmitted part takes the visibility
      sampled at the back side (normal offset along -ng): a thin sheet lit from behind is not shadowed by itself,
      light through a closed thick body is blocked by its sun-facing side (approximation of the missing
      thickness term). Shadows off keeps the A3.1 single call (byte-identical).
    - **Memory:** per viewport, size^2 x cascades x 4 bytes: 2048 x 3 = 48 MiB (default), 1024 x 2 = 8 MiB,
      4096 x 4 = 256 MiB. Released when the sun or its shadows are off, in wireframe mode and when the viewport
      closes. No compute; the new shaders stay `#version 410`, so shadows work on every context (4.1+).
    - **Settings:** `RenderQualitySettings` (Viewport Settings → Quality → Sun Shadows; File → Settings →
      Quality Presets for on/off, size, cascades, distance and filter; `[RenderQuality]` and `[QualityProfiles]`).
      Presets: Low 1024 x 2 hard, Mid 1024 x 3 PCF, High 2048 x 3 PCF (default), Ultra 2048 x 4 soft; lambda and
      biases are kept across preset changes. Auto starts at 1024 x 2 (at most PCF) and steps up to the configured
      values at Auto level 4; it never switches shadows off. Edits pin the shadow group. CLI `--shadows`,
      `--shadow-resolution`, `--shadow-cascades`, `--shadow-distance`, `--shadow-filter`, `--shadow-split-lambda`,
      `--shadow-normal-bias`, `--shadow-slope-bias`, `--shadow-constant-bias`; registry keys likewise
      (renderer scope, saved preferences). `get_state` / `--profile-report` `sun.shadows` lists the cascades.
      The max distance is in scene units (Fox.glb, in centimetres, needs `--shadow-distance 2000`).
    - **Debug views:** `shadowcascades` (red/green/blue/yellow, grey beyond) and `shadowvisibility`.
  - **Check:** `scripts/validate_shadows.py` (quick regression, serial list; fixtures `scripts/shadow_fixtures.py`):
    edge of a 2 x 2 m quad 1.5 m above the plane 0.51 px / 1.3 cm (az 90 el 45) and 1.18 px / 4.3 cm (az 0 el 30)
    from the analytic edge (tolerance 2 px; the normal offset moves edges towards the occluder by offset / tan el);
    pipe probes = visibility x unshadowed value (inside 0, penumbra 0.195, outside unchanged); grazing 5 deg: lit
    plane and lit cube face coefficient of variation 0.0000, no lit plane within 2 cm of the contact line;
    noCastShadow / BLEND / invisible cast nothing (identical to shadows off), `--asobo-disable
    ASOBO_material_shadow_options` makes the quad cast, MASK checker 63/64 cells; a 1 mm camera move keeps every
    cascade origin on whole texels with the same radii, visibility changes at most 5 levels at the edge; TAA
    frames 64 / 96 differ at most 5 levels at the shadow edges (geometry silhouettes 22); Fox.glb (skinned) at two
    poses: with an orthographic camera looking along the sun, 0 shadowed pixels beyond 2 px of the posed
    silhouette; 25 deg off, the shadows differ between the poses; debug views, defaults, memory, flags; sun off
    and shadows off byte-identical to the baseline (HelloCube, fixture, E195 cockpit and exterior); zero GL errors.
  - **Cost** (`scripts/measure_shadow_cost.py`, 2048 x 3 PCF, 1600 x 900 window, one viewport, medians of ~117
    redrawn frames, GL_TIME_ELAPSED; the shadow figure is the three caster passes, depth clears excluded):
    HelloCube shadow pass 0.009 ms, receiver +0.057 ms; E195 exterior (94 casters) 0.078 ms, receiver +0.16 ms;
    DA62 cockpit (307 casters) 0.10 ms, receiver +0.52 ms (main pass 5.87 -> 6.39 ms).
  - **Review:** `build/review/shadows/contact_sheet.png` (`scripts/review_shadows.py`): E195 exterior, DA62
    airframe and cockpit, grazing cube, cascade and visibility views of the E195.
  - **Deviations:** forward lookup in `pbr.frag`, not a deferred pass (Glider has none); one shadow map per
    viewport (fitted to its frustum), not shared between viewports; the receiver normal offset shifts edges by
    up to ~1-2 shadow texels; scene bounds of skinned primitives use the loose node extents.
- [ ] **A3.3 — `ASOBO_macro_light` (R3). Owner: Claude. Deferred (user, 2026-10-28).** Light gizmos, and a real light contribution through the
  punctual-light path.
  - Animated by the animation clock: flashing (frequency, duration, phase) and rotation.
  - Day/night behavior follows the time-of-day setting.
  - Captures are deterministic with `--anim-time`.
- [x] **A3.4 — `ASOBO_material_SSS` (R4). Owner: Claude.** Subsurface-scattering approximation: wrap lighting and
  translucency tinted by `SSSColor`, with thickness from `opacityTexture`, per the A0.6 notes. Uses the sun and
  shadow data.
  - **Done 2026-10-28 (before A3.1/A3.2), support status Partial.** Semantics in
    [section 18](ASOBO_EXTENSIONS_SEMANTICS.md): `SSSColor` is RGBA in every exporter, file and SDK example (the
    schema's 2 items is wrong), default white; `opacityTexture` is "not currently used" per the SDK and absent from
    all 389 surveyed SSS materials.
  - **Shading** (`resources/shaders/asobo_sss.glsl`, OPAQUE/MASK only): direct lights get a wrapped diffuse
    `clamp((N.L + w) / (1 + w))`, `w = 0.5 SSSColor` per channel, plus back-lit translucency
    `0.5 SSSColor (1 - opacityTexture.r) clamp(-N.L)` with an N.V Fresnel; the IBL adds the same weight x
    `c_diff` x irradiance(-N) before AO. `front + back <= 1`. Black `SSSColor` adds exactly zero. No blur pass.
  - **Sun hook:** the direct-light term is `asoboSssDirectDiffuse(intensity, N.L, ...)`; A3.1's sun calls it like
    the punctual-light loop does. Shadows (A3.2) should attenuate only the front (wrapped) part; the transmitted
    part needs the shadow of the light's side, a decision for A3.2.
  - **Typed model:** `SSSColor` accepts 3 or 4 items (default `[1, 1, 1, 1]`); 2 items warn (`ASOBO_VALUE_RANGE`)
    and use white (`scripts/generate_asobo_specs.py` `ITEM_COUNTS`).
  - **Check:** `scripts/validate_asobo_sss.py` (quick regression, serial ASOBO list): black = core byte-identical
    (6 lighting setups); back-lit quad transmits 0.498 of the front-lit core response; light side unchanged; front-lit
    quad 0 levels; opacity white/black/grey/missing; side-lit wrap and orange tint; bounded by the front-lit
    maximum; IBL from behind; dither kept pixels exact; decal receiver; switches; baseline byte-identical.
  - **Review** (`build/review/sss/contact_sheet.png`, default IBL only, no punctual lights in these files): PMDG
    777 seat fur shells change in 4.6% of the frame (max 20 levels), gear handle knob 0.3% (max 25), MSFS 2020
    marshaller face/arms 4-7% (max 24, warmer skin on the sky-lit side), windsock fabric (orange `SSSColor`) 2.1%
    (max 65). With IBL only the effect is a mild glow on thin/convex parts; the wrap needs direct light (A3.1).
- [x] **A3.5 — Ambient occlusion for sky light (R3). Owner: Claude.** Shadows block only the sun; without this
  the sky IBL reaches deep into cockpits and cabins and interiors look flat and too bright. Screen-space ambient
  occlusion (GTAO or an equivalent SSAO) attenuates the diffuse IBL and, through a specular-occlusion term, the
  specular IBL. It complements, not replaces, the authored occlusion maps (A1.3, A1.5, A1.10) and the
  `ASOBO_material_environment_occluder` bounds, which limit sky light inside the cabin per object.
  - **Settings:** per viewport, on/off, radius, intensity and quality (half or full resolution, sample count);
    user-selectable, never lowered silently by Auto quality.
  - **Interaction:** applied in HDR before TAA; temporally stable under TAA; the debug views gain an AO view.
  - **Tests:**
    - a box interior and an open plane: occlusion inside the box, none on the open plane;
    - off gives the current image byte for byte;
    - no halo along silhouettes against the sky.
  - **Done 2026-10-28.** Off by default (every existing image byte-identical; the maintainer may make it default on
    later). Fragment shaders only (`#version 410`): works on every 4.1+ context, no compute fallback needed.
    - **Order (forward renderer):** the A2.1 decal prepass already draws every OPAQUE / MASK drawable's depth and
      world-space geometric normal before the opaque pass; SSAO reuses it (into the shared decal buffer when decals
      are active, otherwise into the viewport's own D32F + RGB10_A2 prepass target) and runs its passes from the
      prepass's post callback, so the AO exists when the opaque pass shades. Cheaper than a deferred composite of
      the IBL terms (no MRT, no per-sample MSAA composite), and the pass order (prepass, decal accumulation, opaque,
      transparent; decal and draw-order rules) is unchanged. Applied in linear HDR before TAA.
    - **GTAO** (`resources/shaders/ssao_passes.frag`, `header/imgui/AmbientOcclusion.h`): Jimenez 2016 slices with
      XeGTAO's step distribution (s^2, 1.3 px minimum), view-space positions from the jittered projection's inverse,
      distance falloff from 0.6 to 1.0 x radius. Two deviations from the paper, both for open surfaces: horizons
      are elevations above the tangent plane (`asin(dot(d, n) / |projected n|)`), so a sample that pixel rounding
      moves off the slice stays at elevation 0 on a plane (the paper's horizon cosine darkened a grazing plane to
      0.94); and the slice sum is divided by the same slices' open-hemisphere value, so 2-4 slices give exactly 1 on
      an open tilted surface (they gave 0.96-0.997). Intensity = exponent on the visibility. Quality: low 2 slices
      x 4 steps, medium 3 x 6, high 4 x 8 (per side).
    - **Denoise / upsample / temporal:** fixed 4 x 4 Bayer noise (slice rotation and step jitter), a depth-aware
      5 x 5 filter with [0.5 1 1 1 0.5] taps (every noise phase weighted equally; taps compared with the centre's
      depth plane, 1% tolerance), at half resolution a depth-aware bilinear upsample (2% tolerance, nearest-depth
      fallback). Nothing changes over time, so TAA only integrates the sub-pixel jitter (TAA's history is the
      temporal filter); no extra temporal pass.
    - **Application** (`pbr.frag` after the authored and decal AO, `ssao.glsl`): only the IBL terms of OPAQUE /
      MASK surfaces whose depth matches the prepass surface of the pixel (0.5% of the view depth; BLEND,
      transmission and anything in front keep 1). Diffuse (incl. the A3.4 SSS back radiance) x Jimenez multi-bounce
      (albedo = c_diff); specular, sheen and clearcoat x Lagarde's specular occlusion (NdotV, alpha = roughness^2 of
      the lobe). Combination with the authored occlusion: **min** per term, `min(authored, SSAO')`, authored =
      material AO with its strength x decal AO. MSFS's authored AO is already the ratio of total sky light incl.
      bounces, so a crease both contain is darkened once and only occlusion the bake lacks (other parts, assets
      without AO maps) is added; the multi-bounce makes the SSAO term the same quantity before the min (a product
      would darken baked creases twice). The sun, punctual lights, emissive and the background never see it.
    - **Settings:** `SsaoSettings` per viewport (Viewport Settings → Render Modes → *Ambient Occlusion*: on/off,
      radius, intensity, quality, resolution); not part of the quality presets, so Auto never changes it. CLI
      `--ssao`, `--ssao-radius`, `--ssao-intensity`, `--ssao-quality`, `--ssao-resolution`; registry keys `ssao`,
      `ssao_radius`, `ssao_intensity`, `ssao_quality`, `ssao_resolution`; debug view `ssao` (raw visibility);
      `get_state` / `--profile-report` `ssao.viewports[]` (active, AO size, memory, GPU time by GL_TIMESTAMP).
    - **Memory:** RG32F targets: full 16 bytes per pixel (+8 for the own prepass target without decals), half
      adds two quarter-size targets; 918 x 796: 11.7 MB with decals, 17.5 MB without (full).
  - **Check:** `scripts/validate_ssao.py` (quick regression, serial list; fixtures `scripts/ssao_fixtures.py`), all
    PASS: off = default byte-identical to b28fda3 (HelloCube, the box fixture, E195 `exterior_fuselage_nose` and
    `cockpit_main_displays`); box interior (radius 0.5 m) corner visibility 0.37 / 0.34 / 0.32 (low / medium /
    high), 0.42 at half resolution (< 0.6), floor/wall crease 0.53-0.59 (< 0.8), floor centre 1.000; 40 m open plane
    min 1.000, mean 1.0000 (>= 0.99 / 0.999) at every quality and resolution, convex cube faces 1.000; the shaded
    image only darkens, where the visibility is below 1 (darkest corner -33% sky light); halo: wall 0.6 m behind a
    cube, no wall pixel darkened beyond 1 px (full) / 2 px (half) of the silhouette (0 darkened), and a 1.5 m radius
    does darken 15526 wall pixels (the measure can see a halo); against the sky the box changes on 29766 of its own
    pixels and on no background pixel; TAA frames 64/96 inside the box max 1 level, mean 0.004 (off: 1 / 0.001);
    black environment + sun: on = off byte-identical; probes (linear HDR): (on - off) with the sun equals (on - off)
    without it within 1e-16 while SSAO removes up to 0.115; registry, get_state, debug view name, flags, invalid
    values rejected, intensity 0 = the off image; zero GL errors. Also pass: `validate_sun.py`, `validate_shadows.py`, `validate_ground.py`, `validate_agent_control.py` (against b28fda3), `validate_asobo_glass.py` (windshield SSR, 133 PASS), `validate_asobo_decal_modes.py` (shared prepass), `validate_p8_1.py --only equivalence`.
  - **Cost** (`scripts/measure_ssao_cost.py`, 1600 x 900 window = 918 x 796 viewport, medium, radius 0.5, every
    frame redrawn, ~117 frames; the GPU also runs an inference VM, so main-frame totals are noisy): AO passes
    (GTAO + denoise [+ upsample]) full / half: HelloCube 0.041 / 0.033 ms, E195 exterior 0.059 / 0.042 ms, DA62
    cockpit 0.160 / 0.062 ms. Prepass pass incl. AO: HelloCube 0 -> 0.048 ms (own prepass), E195 0.038 -> 0.097 ms,
    DA62 0.042 -> 0.202 ms (both reuse the decal prepass). Main frame: DA62 5.88 -> 6.05 (full) / 5.93 ms (half).
  - **Review:** `build/review/ssao/contact_sheet.png` (`scripts/review_ssao.py`): DA62 cockpit (two views), E195
    cockpit and exterior, SSAO off / on 0.5 m / on 1.5 m / debug view. With the min against the authored AO the
    change on MSFS assets is subtle: 0.6% of the pixels (DA62 canopy view), 13% (DA62 seats and panel, up to 47
    levels), 1.0% (E195 cockpit), 2.6% (E195 exterior: gear, door seams); the debug view shows the creases (panel
    bezels, seat folds, gear well).
  - **Deviations:** horizon elevation and slice normalisation (above); fixed spatial noise instead of temporally
    rotated noise (stability over convergence); geometric, not normal-mapped, normals (the prepass's); the A3.6
    ground plane neither occludes (not in the prepass) nor receives SSAO, so aircraft-on-ground contact AO is
    missing; the profile report always carries the `ssao` object; the GPU time is measured with GL_TIMESTAMP
    inside the prepass pass query.
- [x] **A3.6 — Ground plane (R3). Owner: Claude.** A ground choice per viewport: *Grid* (current), *Plane*
  (an infinite ground at the model's lowest point or at y = 0, with a colour or a tiling material such as tarmac
  or grass), and *Ocean* (A6.8). The plane receives the A3.2 sun shadow, which gives the aircraft's shadow on the
  ground, and the grid can be drawn over it. It reuses the ray-traced grid path (`--grid-mode ray`) so it reaches
  the horizon without a mesh.
  - **Done 2026-10-28.** Grid stays the default and is byte-identical to the baseline; the plane is drawn per
    viewport, in shaded views only (debug views and wireframe keep the sky and the grid).
    - **Choice:** Viewport Settings → Camera & Helpers → *Ground* (`header/imgui/GroundSettings.h`): *Type* Grid /
      Plane / a disabled *Ocean (A6.8, not available yet)* entry (no registry value until A6.8), *Height* Model
      base / Y = 0 / Custom, *Material* presets (Concrete / tarmac with 5 m slab joints, Grass, Sand, Neutral grey)
      or a custom *Albedo*, *Roughness*, *Detail*, *Detail size*, *Joint spacing*, *Haze distance*, *Show grid on
      ground*. CLI `--ground grid|plane` and `--ground-height|-preset|-color|-roughness|-detail|-detail-scale|-joints|
      -haze|-grid` ([CLI reference](CLI_REFERENCE.md)); registry keys `ground`, `ground_height_mode`, `ground_height`,
      `ground_preset`, `ground_color`, `ground_roughness`, `ground_detail`, `ground_detail_scale`,
      `ground_joint_spacing`, `ground_haze`, `ground_grid`; `get_state` `ground`.
    - **Rendering** (`resources/shaders/ground.vert` / `ground.frag` / `ground_common.glsl`;
      `GltfResourceManager::getGroundTuple`): a screen quad in the Opaque pass, appended to the scene by
      `Viewports.cpp` after the model, so it is drawn into the linear HDR scene target before TAA like geometry. The
      vertex shader unprojects each corner to the near and far plane with the camera block's (TAA-jittered)
      view-projection matrix; the fragment shader intersects the pixel's ray with `y = h` (the `GridRenderer` ray
      path), writes `gl_FragDepth` from the same matrix (1 beyond the far plane), and shades a dielectric (F0 0.04):
      IBL diffuse (`getIBLRadianceLambertian`, with the multiple-scattering term) and IBL specular
      (`getIBLRadianceGGX`) from the environment (the plane has no environment of its own: its lower hemisphere is
      the HDR's own, as for every surface), plus the A3.1 sun (Lambert + GGX with the widened lobe) times
      `sunShadowVisibility` of A3.2 (`shadow.glsl`; the generator binds `u_SunShadowMap` to `ground.frag` too).
      Fog: `1 - exp(-k d)`, 98% at the *Haze distance* (default 0.9 x the camera's far plane), towards the
      environment just above the horizon in the pixel's azimuth (one mip softer than the background is drawn), or the
      clear colour while the background is off; the fog is 100% at the horizon, so the plane meets the sky without a
      line. Two-sided (seen from below it is lit as if facing the camera). Opaque, so glass, SSR and transmission
      copies see it; KHR punctual lights do not light it.
    - **Detail:** value noise in world XZ (four octaves from 8 x *Detail size*, mean-preserving, so the average
      albedo stays the colour) and the slab joints; every octave and the joints fade to their mean as the pixel
      footprint (`dFdx`/`dFdy` of the world position) approaches half their wavelength. Nothing depends on the
      camera or time, so TAA only integrates sub-pixel samples. No texture files are added: the three shader
      files grow the packed archive by about 2.3 KB (9,932,609 to 9,934,905 bytes; the executable by 41 KB).
    - **Height:** *Model base* is the lowest world Y of the POSITION boxes (accessor min/max, eight corners through
      the node's static glTF world matrix) of the visible nodes, skipping `ASOBO_material_invisible` and
      environment-occluder primitives (`Application::getCachedModelBaseHeight`, `Bounds.cpp`). It is cached with the
      model and recomputed on load and on Transform / Visibility / Material edits; animation never moves it (it reads
      the glTF nodes, not the animated pose). The existing extents cache (bounding spheres of the boxes) is too loose
      for this, so the box corners are used (exact for axis-aligned parts, conservative for turned ones).
    - **Shadows:** while the plane is shown, `updateSunShadows` adds the plane and the model's footprint on it (the
      corners of the scene box carried along the sun rays, at most ten diagonals away) to the range the cascades are
      cut to; otherwise a low sun's shadow lies outside the model's own depth range.
    - **Not pickable:** the drawable is taken out of the scene before the picking passes and is not in the overlay
      depth: clicks, `pick` and `probe` see the background (or whatever is below the plane). *Show grid on ground*
      draws the grid at the plane's height; the overlay depth is then rebuilt from the opaque model only (as under TAA).
    - **Ground albedo for A6.2:** `GroundSettings::averageAlbedo()` (the colour: the detail has mean 1, the joints
      cover under 1%), kept per viewport in `ViewportInstance::groundAlbedo` / `groundPlaneActive` /
      `groundPlaneHeight` (`header/imgui/ViewportInstance.h`, with the comment where A6.2 picks it up) and in
      `get_state` `ground.albedo`. The environment lighting is unchanged here: the HDR keeps its own lower half.
  - **Check:** `scripts/validate_ground.py` (quick regression, serial list): Grid byte-identical to the
    baseline (HelloCube, E195 `exterior_fuselage_nose` and `cockpit_main_displays`, the exterior with the sun and
    shadows on), `--ground grid` = default, debug and wireframe modes with the plane selected = Grid; shadow edge
    on the plane 0.53 px / 1.3 cm (az 90 el 45) and 0.20 px / 0.7 cm (az 0 el 30) from the analytic position for
    Y = 0, and 0.55 px / 1.3 cm for Model base at the fixture's lowest point 0.2 m (a turned box exact, a buried
    invisible box ignored); the shadow of a 10 m pole 12 m and 24 m away (fails when the footprint is not added to
    the cascade range); probe of the lit plane 0.1996 / 0.1995 against the pbr.frag BRDF terms 0.1997 / 0.1995
    (tolerance 2% + 0.002), shadow 0, the occluder occludes the plane, the plane occludes a box buried below it, a
    click on the plane picks nothing; uniform environment L = 0.5: white furnace 0.4908 (-1.8%, tolerance 3%),
    albedo 0.25 in the expected band, IBL + sun = IBL + sun separately (0.4472 vs 0.4476); presets, `ground_color`,
    custom and model-base height, registry keys, invalid values rejected; E195 exterior without its stairs and
    barrier: the lowest nose-tyre pixel 1.2 cm above the plane; grid on ground at Y = 1; TAA frames 64 / 96 differ
    by at most 3 levels on the ground and 1 at the horizon, far-field high-frequency std 0.34 level, a 0.03 degree
    turn changes the far field by 0.007 level; zero GL errors. `validate_shadows.py`, `validate_sun.py` and
    `validate_agent_control.py`, `validate_p8_1.py --only equivalence`, `validate_p8_3.py` and `validate_p7.py` (ray and plane-mesh grid) pass (validate_shadows' "sun on, --shadows off = baseline" cases compare against the
    A3.1 commit 7a21913; against the A3.2 baseline cf24b01 they cannot hold, shadows being on there).
  - **Review:** `build/review/ground/contact_sheet.png` (`scripts/review_ground.py`): E195 (without its stairs and
    barrier) and DA62 SDK airframe on concrete and on grass with the sun at az 45 el 40 and az 250 el 22, a low camera
    1.5 degrees above the horizon, and the grid on the plane.
  - **Deviations:** *Ocean* is only a disabled UI entry (no registry value); the plane is drawn in shaded views only
    (wireframe and debug views show the sky and the grid); *Model base* of the E195 exterior file is the stairs and
    the barrier pole, 1 m below the nose gear (use Custom or remove them), and the DA62 airframe has no wheels (they
    are separate parts), so its base is the gear doors; the model base follows the rest pose, so an animation that
    extends the main gear lowers it below the plane; punctual lights do not light the plane; the plane does not
    receive A3.5 ambient occlusion (see A3.5); the tyre-contact check uses the E195 copy without ground
    support equipment.
- [x] **A3.7 — `ASOBO_sky_portal` (R3). Owner: Claude.** Window area lights that bring sky and sun light into
  the cabin. Entry: A3.1 (and A3.2 for the sun term).
  - **Done 2026-10-28, support status Partial** (semantics: [section 19](ASOBO_EXTENSIONS_SEMANTICS.md)). V =
    verified, I = inferred.
    - **Format (V):** schema `resources/schemas/asobo/ASOBO_sky_portal/` written from the exporter (node scope;
      `shape_type` integer, only 1 = disc; `source_radius` >= 0; angles 0-180); exporter defaults 1 / 50 / 0 / 160
      in `generate_asobo_specs.py`; typed reader `asobo::readSkyPortal` (`AsoboExtensions.h`). **Unit: centimetres**
      (V): the Blender add-on sets the preview radius to `source_radius / 100` m (`blender/msfs_lights.py`), and the
      DA62 portals match their window panes (FL r 27 cm against a 0.56 x 0.68 m pane, ML 25 cm against 0.55 x 0.87 m).
      **Direction (V):** node +Z; on all 8 DA62 portals +Z points into the cabin (cos to the cabin centre >= 0.58).
    - **Interior (I):** every OPAQUE / MASK `pbr.frag` material of a file that contains portals (the portal's own
      file is the interior attachment), only in front of each disc (inward half-space and cone), and only the part
      of the IBL the occlusion removed (below): a surface open to the sky (occlusion 1: the exterior, an AO-free
      material) gets nothing. No visibility between disc and receiver, so a surface behind a wall that faces the
      portal and has occlusion < 1 is lit through the wall.
    - **Radiance (I):** the environment seen through the window from the receiver: the GGX prefiltered cube at
      roughness 0.6, in the direction receiver -> disc centre, rotated and scaled like the IBL (a receiver on the
      axis sees the portal's outward direction). **Sun: not added (measured):** glass and windshield materials cast
      no A3.2 shadow, so the A3.1 sun already lights the cabin through the windows, shadowed by the frame; the
      shadow-visibility view of the DA62 cockpit is lit on 23-51% of the frame (two suns, two views,
      `review_sky_portal.json`); a portal sun term would light those patches twice. With the sun on, the HDR's own
      sun is removed from the IBL (A3.1), so the portal does not carry it either.
    - **Integration (I):** diffuse only, the disc form factor without horizon clipping (Lagarde and de Rousiers
      2014): `E = pi L cos(emit) cos(receive) r^2 / (d^2 + r^2)`, exact on the axis, times
      `smoothstep(cos(outer/2), cos(inner/2), cos(emit))`; no LTC (no LUT added), no specular.
    - **No double lighting (I):** `add = (1 - occlusion) x min(strength x c_diff / pi x sum E, unoccluded IBL diffuse)`,
      occlusion = the authored AO (material map with its strength, x decal AO), `min()` with the A3.5 multi-bounce
      SSAO term while SSAO runs: the same factor the diffuse IBL received. **Strength default 0.35 (I):** calibrated
      on the SDK pair: there the cockpit panels gain +1.5/+2.4/+4.2 (left panel, mean RGB levels on ~60-78) and
      +1.2/+2.0/+3.4 (right wall, on ~27-41); at 0.35 the DA62 pilot's view gains +2.3/+2.1/+1.8 on ~29 (changed
      pixels), the seats +1.9-2.1. Strength 1 (the window radiating the full sky) gave +5.5 levels, about three
      times the SDK's relative change. Our tint follows the default HDR (sunset, warm); the SDK's sky is blue.
    - **Cost:** at most 8 portals per viewport (`kMaxSkyPortals`, the nearest to the camera), one cube-map lookup
      each, in a loop gated by the `ASOBO_SKY_PORTAL` define; files without portals and viewports with the switch
      off compile without it (byte-identical). Portal data: EnvironmentBlock (`u_SkyPortalCount` at 484,
      strength 488, three vec4[8] arrays at 496 / 624 / 752; 880 bytes).
    - **Viewer:** gizmo (`Application::drawSkyPortalGizmo`, ImGui overlay, not in viewport captures): the disc
      (yellow, source radius), the exporter preview's inner circle (blue, `r sin(inner/2) / sin(min(outer,179)/2)`)
      and an arrow along +Z; shown for the selected portal node, or for all with Viewport Settings > MSFS Extensions
      > *Show sky portals*. Same section: *Sky portal strength* (both only for files with portals) and the
      per-viewport `ASOBO_sky_portal` switch (default on). Inspector: typed fields from the schema plus a *Sky Portal
      (MSFS)* summary (radius in metres, inner circle, cone). Isolation keeps the portals (they are the environment);
      hiding a portal node switches it off.
    - **Settings / CLI:** registry `ASOBO_sky_portal`, `sky_portal_strength` (0-4), `show_sky_portals`;
      `--sky-portal-strength <x>`, `--show-sky-portals`, `--asobo-disable ASOBO_sky_portal`. `get_state` `skyPortals`
      (portals with world position, direction, radii; `active` of the active viewport); `--asobo-report`
      `skyPortals[]`.
  - **Check:** `scripts/validate_asobo_sky_portal.py` (quick regression, serial list), 49 PASS: DA62 LOD00 8 portals
    parsed (radii 10/27/25/20 cm, shape 1, inner 160 or 180, outer 180, all facing in, typed spec, no diagnostics),
    LOD01 / LOD02 all portals; files without portals (HelloCube, the fixture without its portal node, E195
    `cockpit_main_displays`) and the DA62 cockpit with `--asobo-disable ASOBO_sky_portal` and with `--asobo off`
    byte-identical to e323478, its default differs, no magenta; fixture (4 x 4 m room, portal r 0.5 m at 1.25 m,
    uniform L = 1, strength 1): six patches gain the analytic amount (e.g. 0.02338 vs 0.02342, 0.00317 vs 0.00314),
    falling with distance 0.0234 / 0.0168 / 0.0069 / 0.0032, the grazing patch cut by the cone (factor 0.60),
    strength 2 doubles, the patch behind the window wall unchanged, changed pixels only on the inside patches
    (floor, walls and ground with occlusion 1 unchanged), probes finite; environment bright towards the window
    0.0553 against 0.0065 turned 180 degrees; registry, range refusal, get_state; gizmo drawn (all / selected);
    zero GL errors. Also pass: `validate_asobo_support.py` (43 specs), `validate_sun.py`, `validate_shadows.py`,
    `validate_agent_control.py` (against e323478).
  - **Review:** `build/review/skyportal/contact_sheet.png` (`scripts/review_sky_portal.py`): DA62 pilot's view and
    seats view, portals off / on at az 45 el 40 and az 250 el 22 (shadows on), the two suns' shadow visibility, no
    sun off / on, the gizmo window view (all portals, selected FL) and the SDK before/after pair. Subtle lift of the
    frame, seats and panel near the windows, comparable in size to the SDK pair's panels; the SDK's darker face
    comes from `node_occlusion_factor` on the character (not glTF, no character in Glider).
  - **Deviations:** sky sampled along the receiver's line of sight through the disc centre, not a fixed outward
    mip; no sun term (measured above); diffuse only; no portal-to-receiver visibility (light passes through
    walls where the occlusion is below 1); interior = the file with portals + occlusion weighting, not a hull
    test; the inner circle is only drawn, the disc uses the full source radius; at most 8 portals per viewport;
    BLEND materials get no portal light.
  - **What the files carry (V, Blender exporter `io/com/extensions/light/asobo_skyportal_light.py`,
    `io/com/msfs_light_utils.py`, `blender/msfs_lights.py`):** a node extension with `shape_type` (1 = disc, the
    only portal shape), `source_radius` (Blender UI unit cm, exporter default 50), `inner_cone_angle` and
    `outer_cone_angle` (degrees; SDK: the inner angle defines the portal circle, outer is always 180). No colour or
    intensity: the light comes from the environment. The node's direction points into the cabin (the exporter
    applies a 180° X rotation to Blender lights). It replaces any `KHR_lights_punctual` on the node. Sample:
    DA62 SDK `Part_Interior_Cockpit/model/Cockpit_LOD00.gltf`, 8 portals (`PORTAL_Window_*`, `source_radius` 10,
    inner 160, outer 180). No schema ships; add one under `resources/schemas/asobo/`.
  - **SDK intent (V, *Exterior Lights → Sky Portals*):** placed on the outside of every window that shows an
    interior, pointing inside, to "simulate additional sunlight" for the characters inside; used together with
    `node_occlusion_factor` in `navigation_graph.cfg` (not glTF). The cockpit itself already carries baked
    white-sky AO with bounces, so the portal mainly adds directional window light.
  - **Proposed rendering (I):** each portal is a disc area light at the window, facing inward. Its radiance is the
    sky seen through the window: the environment (A6.2 sky or the HDR) sampled at a rough prefiltered mip in the
    portal's outward direction, plus the A3.1 sun when the sun direction passes through the disc (attenuated by
    the A3.2 shadow map), with the inner/outer angles as the angular falloff. Disc lighting uses LTC
    (linearly transformed cosines, Heitz 2016) or a representative-point approximation. To avoid lighting twice,
    the portal adds only the part the baked AO removed (`(1 - ao)` weighting or a fixed fraction), decided against
    the SDK's before/after images; calibration is inferred until an MSFS capture exists.
  - **Interior only:** portals light interior materials (cockpit/cabin files), never the exterior.
  - **Viewer:** a gizmo (two concentric circles and the direction, as in the exporter preview), the parameters in
    the Inspector, and a per-viewport switch in MSFS Extensions (default on).
  - **Tests:** off renders byte-identical to before; a portal facing a white plane lights a disc-shaped region
    falling off with the cone angles; no light leaks outside the hull; DA62 cockpit before/after review.
  - **Deferred, not planned:** `ASOBO_macro_light` (deprecated MSFS 2020 name), `ASOBO_street_light`,
    `ASOBO_advanced_light` (user decision 2026-10-28; aircraft lights mostly live in `systems.cfg`, outside glTF).

- [x] **A3.8 — Viewer ambient light (R2). Owner: Claude.** Maintainer's decision (2026-10-29): "Ambient light is
  always there on the scene, so the user doesn't need to add a light node; it can be turned on/off regardless of HDR
  IBL or procedural sky; it's not part of the glTF, but part of the viewer." Motivation: the DA62 cockpit at night is
  black apart from its screens, because nothing in the glTF lights the panel (MSFS panel lighting comes from
  `systems.cfg` lights, which Glider does not read).
  - **Done 2026-10-29.** Code: `header/imgui/SceneEnvironment.h` (`ambientEnabled`, `ambientColor`,
    `ambientLuminance`, `kAmbientUnitLuminance`, `ambientRadiance()`), `environment_block.glsl`
    (`u_AmbientRadiance` at 896, `u_AmbientEnabled` at 908; the block grows to 912 bytes), `pbr.frag` (after the
    A3.7 bound, before the occlusion), `ground.frag`, `CommandGenerator::setAmbient`, `Viewports.cpp`.
  - **Setting:** scene environment (one per glTF scene, like the sun), not persisted (like the sun). Off by default:
    every render is byte-identical to before. Colour linear RGB (default white), intensity a luminance in cd/m^2
    (0..10000, default 3 when switched on, logarithmic slider), radiance L = colour x intensity / 12,800 in
    environment units. 12,800 cd/m^2 is the procedural sky's calibrated unit (`SkyAtmosphere.h`); an HDR panorama
    has no absolute scale, so the same unit is used with it (stated in the UI tooltip): a given cd/m^2 adds the same
    radiance with both sources. Pre-exposure (`u_PreExposure`) applies to it as to every scene output.
  - **Shading:** a uniform radiance L from every direction. For a constant environment the split-sum integrals reduce
    to diffuse `c_diff x L` (Lambert, irradiance pi L) and specular `specularWeight x L x (F0 A + B)` with A, B from
    the existing BRDF LUT at (N.V, roughness); clearcoat `L x (F0_cc A + B)` at the clearcoat normal and roughness,
    sheen `L x sheenColor x CharlieLUT`. Added to `f_diffuse` / `f_specular` / `f_clearcoat` / `f_sheen` before the
    occlusion, so authored AO (with A1.5 strength, A5.1 detail AO), decal AO and A3.5 SSAO (`min(authored, SSAO)`,
    multi-bounce / Lagarde specular occlusion) darken it exactly as they darken the IBL. Without IBL (`ibl` off) the
    SSAO block now also runs while the ambient is on. Not on emissive, not on the background, never in the IBL filter
    input. KHR transmission: its diffuse share is weighted by `1 - transmission` like the IBL diffuse and the
    refracted scene copy is not changed (the scene behind already contains the ambient); BLEND, glass v2 and
    windshield v3 surfaces get the same uniform reflection term in their specular (glass v2 masked by its reflection
    mask like the IBL; the windshield's SSR is unchanged, the ambient reflection is added beside it). A3.4 SSS back
    radiance and iridescence/anisotropy variants use the plain terms (deviation). A3.7 portals keep their bound (the
    unoccluded IBL diffuse, captured before the ambient is added). The A3.6 ground plane adds `albedo x L` and, with an
    environment, `L x (0.04 A + B)`.
  - **No double counting:** the ambient is an explicit viewer addition on top of the environment, default off. It
    stands for light the glTF and the environment do not carry (cabin flood and panel lights); it is not derived from
    the HDR or the sky, so nothing in the IBL is counted twice.
  - **UI / CLI / agent:** Scene Inspector → *Ambient Light* (collapsed header after *Night Sky*): checkbox, colour,
    intensity (cd/m^2) with a units tooltip, the value in environment units. `--ambient on|off`,
    `--ambient-color r,g,b`, `--ambient-intensity <cd/m^2>`; registry keys `ambient`, `ambient_color`,
    `ambient_intensity` (`set_environment`); `get_state` / `--profile-report` `ambient` (`enabled`, `color`,
    `intensity`, `intensityEnvironmentUnits`, `unitLuminance`, `radiance`).
  - **Check:** `scripts/validate_ambient.py` (quick regression, serial list). 44 PASS against b96d742: ambient
    off (also with non-default colour / intensity) byte-identical for HelloCube and E195 `exterior_fuselage_nose` /
    `cockpit_main_displays` with the HDR and with the procedural sky (`--sky-time-slicing off` on both binaries);
    white Lambert sphere (specularFactor 0) under a black HDR, L = (0.5, 0.25, 0.125): every probe exact to 0.000%,
    3 cd/m^2 gives 2.344e-4 units (0.004%); smooth metal (F0 0.9/0.6/0.3, roughness 0) against
    `L x (F0 + (1 - F0)(1 - N.V)^5)` (the LUT integral at roughness 0) worst 0.08%; uniform 0.502 occlusion texture
    halves ambient and constant-HDR IBL diffuse alike (difference 0.0002); SSAO on two touching spheres darkens both
    alike (darkest 0.63, difference 0.0006); emissive sphere byte-identical on/off, 0 of 383,088 background pixels
    changed; ground plane lit; CLI = pipe byte-identical; HDR and sky add the same radiance (0.37%) equal to
    albedo x L (0.39%); registry, refusals (pipe and CLI), get_state / profile report; zero GL errors. Also pass:
    `validate_night`, `validate_sky` (incl. its GPU-timing checks), `validate_sun`, `validate_shadows`,
    `validate_ssao`, `validate_asobo_sky_portal`, `validate_p4_exit_gate`, `validate_agent_control`.
  - **Review:** `build/review/ambient/contact_sheet.png` (`scripts/review_ambient.py`). DA62 cockpit at night
    (procedural sky, Auto, highlight meter EV -2.6 in every capture): off, 1, 3 and 10 cd/m^2 look the same (black
    apart from the display strips; 10 cd/m^2 changes at most 2 levels), 100 cd/m^2 shows a hint of the canopy frame,
    1000 cd/m^2 the panel and frame faintly. E195 cockpit at night (EV -1.9) off / 3 cd/m^2: identical to the eye (at
    most 1 level). E195 exterior, default HDR: off / 3 cd/m^2 identical to the eye (9,533 pixels change by 1 level).
    **Finding:** the night cockpit stays black at the brief's 1-10 cd/m^2 because the highlight meter is held by the
    screens, whose MSFS-scaled emissive is 5 units = 64,000 cd/m^2 (A6.4b report): the ambient is 13+ EV below them.
    Making the panel readable needs either a much larger ambient, a Manual exposure, or the emissive-scale calibration
    already flagged in A6.4b (maintainer decision; not changed here).
- [x] **A3.9 — Sky occlusion inside the model (interior sky light). Owner: Claude.** Inside a cockpit every surface
  received the full sky IBL; only the sun was shadowed (A3.2), and authored AO / A3.5 SSAO darken small creases only.
  - **Done 2026-10-29.** Code: `header/imgui/SkyOcclusion.h` (volume, bake, time slicing),
    `resources/shaders/sky_occlusion_bake.frag` (bake passes), `resources/shaders/sky_occlusion.glsl` (lookup),
    `pbr.frag`, `environment_block.glsl` (`u_SkyOcclusionOrigin/Dims/Params` at 912/928/944; 960 bytes),
    `CommandGenerator::setSkyOcclusion`, `ShadowCasterLayer` (`includeEnvironmentOccluders`), `Viewports.cpp`
    (`updateSkyOcclusion`, the per-viewport define `SKY_OCCLUSION`), `SceneEnvironment.h`.
  - **Bake:** N directions on a spherical Fibonacci lattice (low 64, medium 128, high 256); per direction an
    orthographic DEPTH16 map (512 / 768 / 1024 px) fitted to the volume, drawn by the renderer through a second
    `ShadowCasterLayer` (the sun's caster rules: OPAQUE / MASK / dither; no decals, glass, windshields or invisible
    materials; `noCastShadow` ignored, since a part without a sun shadow still hides the sky; environment occluders
    added although the colour passes hide them). Glass / BLEND does not block (windows let the sky in). A fragment pass
    per slice tests every cell centre against 8 maps at a time (hardware compare, 2 x 2 PCF): visible only when the cell
    lies at least max(0.1 cell, 0.5 texel) in front of the outermost surface, so cells on or inside closed geometry see
    nothing. It sums (v, v d) in an RGBA32F accumulator; the last frame writes the RGBA16F volume (a, m) = (visible
    fraction, mean visible direction), a = -1 for cells inside solids. Volume: the scene's static POSITION boxes, cubic
    cells, at most 64 / 96 / 128 per axis including 2 padding cells. Time-sliced: 16 / 16 / 32 directions per frame
    (4 / 8 / 8 frames). It runs after a model load and after a Visibility, Transform, Material or Extension revision, an
    isolation, quality or occluder change; never per frame; animation is ignored (the pose at bake time). The previous
    complete volume stays in use until the new one is complete; a new model drops it at once.
  - **Lookup (pbr.frag):** at the world position moved one cell along the geometric normal (viewer side), the eight
    surrounding cells interpolated trilinearly without the cells inside solids; outside the volume visibility 1.
    Diffuse: the cosine-weighted visibility. L1 alone (a + 2 m.n) is exact for a half-space but read 25% low behind the
    test box's opening (0.187 against 0.244), so it is blended with a visibility-cone reading (axis m, solid angle
    4 pi a, irradiance of a horizon-clipped spherical cap, Lagarde and de Rousiers 2014) by how cap-like the visible set
    is (|m| against sin^2(alpha) / 4). Specular / sheen / clearcoat: the visibility cone intersected with the lobe cone
    (half-angle alpha x 90 deg), relative to the lobe's intersection with the open half-space, blended towards the
    diffuse term with the roughness. Values at or above 0.95 of an open half-space read 1 (the lattice reads a
    half-space as 1 +- 0.05 / 0.034 / 0.024 with 64 / 128 / 256 directions), so open and convex surfaces are unchanged.
    Every alpha mode (BLEND and transmissive surfaces in their reflection and diffuse share); the windshield's IBL
    fallback is occluded before the SSR mix, the SSR itself is not.
  - **AO combination: product.** The E195's authored occlusion maps are local (creases and gaps; the panel face reads
    0.85-0.9 in the Occlusion debug view) and the lookup sits a cell (4.9 cm at medium) off the surface, so the two
    measure separate scales and multiply; a crease is darkened by its map only. SSAO keeps `min(authored, SSAO)` on top.
    A multi-bounce fit (as SSAO's) was tried and rejected: it assumes surroundings of the receiver's albedo and
    brightened the white sunlit surface that limits the noon exposure.
  - **Not touched:** the sun (A3.2 shadows), punctual lights, emissive, the A3.8 ambient light (a viewer aid, it stays
    uniform), the background, the IBL filter input, the A3.6 ground (the volume ends at the model's bounds, so a seam
    would show; the sun shadow already gives the contact).
  - **Sky portals (A3.7):** the add-back is now `(1 - min(authored, SSAO) x sky visibility) x min(strength x portal,
    unoccluded IBL diffuse)`. The DA62's environment occluder is a closed 84-triangle hull, so its cabin sees no sky and
    the portals become the window light (no double counting); where the sky is seen directly they add the remainder only.
    DA62 noon, mean linear luminance of the lower panel: occluders on, portals 0 / 0.35 / 1 / 2: 0.11460 / 0.11540 /
    0.11689 / 0.11919; occluders off (geometric windows), portals 0 / 0.35: 0.11756 / 0.11832. The portal term is about
    0.0023 per unit strength, the geometric window light 0.0030, so strength 1.3 would match it; with occluders off,
    0.35 adds a quarter of the window light again (partial double count). The default 0.35 is kept: it also drives the
    HDR renders (sky occlusion off), which must stay byte-identical.
  - **Setting:** scene environment (`sky_occlusion` auto / on / off, default auto = on with the procedural sky, off
    with the HDR; `sky_occlusion_quality`; `sky_occlusion_occluders`), not persisted. Quality presets: Low has no IBL, so
    nothing is baked; the others use the scene's quality (no separate preset field). Scene Inspector → *Sky Occlusion*
    (collapsed header after *Ambient Light*): mode, quality, occluder switch, status. CLI `--sky-occlusion`,
    `--sky-occlusion-quality`, `--sky-occlusion-occluders`; debug view `skyvisibility`; `get_state` /
    `--profile-report` `skyOcclusion`.
  - **Cost (measured):** E195 cockpit (588 casters): low 58x58x64 cells of 7.6 cm, 1.6 MiB, 54 ms CPU / 35 ms GPU over
    4 frames; medium 86x86x96 of 4.9 cm, 5.4 MiB, 99 / 75 ms over 8 frames (largest frame 21 / 13 ms); high 114x114x128
    of 3.6 cm, 12.7 MiB, 165 / 143 ms (largest 30 / 27 ms). E195 exterior (94 casters): 2.0 / 6.7 / 15.9 MiB, cells
    71 / 46 / 34 cm, 22/17, 39/36, 71/67 ms CPU/GPU. DA62 cockpit (308 casters, 1 occluder): 1.9 / 6.6 / 15.5 MiB,
    66/37, 100/74, 178/147 ms. No bake frame above 32 ms CPU or 27 ms GPU. Temporary memory during a bake: the
    accumulator (16 B per cell) and 8 depth maps (medium E195 cockpit about 21 MB). Lookup: E195 cockpit main frame
    6.065 -> 6.111 ms GPU (+0.05 ms).
  - **Accuracy / leaks (`validate_sky_occlusion.py`):** closed box with an open top: floor centre 0.247 (low, medium) /
    0.275 (high) against the analytic 0.244; outer walls 1.000; open plane and isolated spheres (Lambert, glossy)
    unchanged (0.000%, debug view 1.000 everywhere). Zero-thickness walls leak up to 0.18 within one cell (2.2 cm there),
    nothing from 2 cells on. **Limitation:** each direction is a hard test (a cone filter from an outermost-surface map
    overestimated the box by 30% and was rejected), so small openings show faint contours of about 1 / N of the sphere
    (visible on the test box's flat floor).
  - **Noon E195 contrast (`validate_night.py`, thresholds unchanged):** MFD L / PFD L / EICAS screen against bezel
    0.179 / 0.189 / 0.196 -> 0.288 / 0.285 / 0.291 (0.3 not reached); panel median 0.098 -> 0.035 (now below 0.04). The
    exposure is limited by the highlight meter at the sunlit white `$DRM_UI_SCREEN` surface (1.95 -> 1.61 units; EV
    -1.28 -> -1.01), which holds the screens at about 0.306 displayed, so p95 - p5 cannot exceed about 0.29 however dark
    the bezel. No threshold, exposure or emissive value was changed.
  - **Review:** `build/review/sky_occlusion/contact_sheet.png` (`scripts/review_sky_occlusion.py`).

- [x] **A3.10 — Sun Source and Auto ambient (interior fill).** Viewer settings only; never read from or written to
  glTF, no new persistence. See the A3.10 task report for verified sweep/check results and remaining failures.
  - **Sun Source** replaces Sun / Sun Mode / Sun Follows Time of Day. HDR: Off, From HDR, Manual, Time of Day.
    Procedural sky: Manual (angles only), Time of Day; colour and intensity come from the atmosphere. The selector's
    effective choice is shared by the renderer, atmosphere, disk, state reports and Inspector. The Inspector receives
    the evaluated sun from `Application::effectiveSun`, so it does not implement another light precedence.
  - **Switching sources:** HDR's requested choice is retained separately. Until a sky choice is explicit, Clock maps
    to Clock, Off / From HDR / Manual map to Manual. Explicit sky choices stay with the sky; returning to HDR restores
    its choice. An invalid `sun_source off|hdr` request under the sky is reported as requested, but resolves to Manual.
  - **Legacy mapping, in application order:** `sun on` selects the stored manual/HDR mode, off selects Off;
    `sun_mode manual|hdr` changes the stored mode and selects it if the HDR sun was enabled or Clock, leaving an off
    sun off otherwise. `time_of_day_mode on` selects Clock for both environments and enables the clock; off selects the
    previous legacy enabled/manual/HDR choice and sky Manual. The last source selection wins. In particular HDR
    `--time-of-day-mode on --sun-mode hdr` now means From HDR, locked to the panorama; the reverse order means Clock.
    Changing the independent date/time does not move From HDR. `sun_source clock` enables the clock; selecting another
    source leaves its emissive/moon/stars evaluation available. The Time of Day section states when it does not move
    the sun. CLI `--sun-source off|hdr|manual|clock`; registry `sun_source`; state `sun.source` requested/effective/HDR/options.
    Legacy `sun` / `sun_mode` update the retained HDR choice, preserving an explicit sky choice; `sun_source` acts
    on the current environment, whereas legacy `time_of_day_mode` selects both environments.
  - **Ambient defaults:** `ambient auto` (default) enables it with the sky and disables it with HDR. An unspecified
    intensity mode resolves to Auto with sky, Manual with HDR. Explicit `ambient on` without a chosen mode selects
    legacy Manual; explicitly selected modes survive toggles/source changes. Manual uses the original uniform colour
    and cd/m² path, including its occlusion, ground and material terms.
  - **Auto formula:** white `L_fill = k E_out / pi`, default k = **0.05** (the largest passing sweep step is 0.058 with a 0.0002 noon-contrast margin; 0.05 keeps 0.0055;
    0.059 fails noon contrast), E_out in lux, L in cd/m²; shader units divide L by 12,800. In
    environment units this is simply `k E_out_units / pi`. Sky E_out is the same shared horizontal sun + clear sky +
    moon + glow used by Curve (including moonlight scale). HDR uses filtered Lambertian cube at +Y times pi times HDR
    intensity, plus the effective sun's horizontal illuminance. The Lambertian face centre is read once when filtering,
    then cached (yaw leaves +Y unchanged). HDR's absolute photometric scale remains conventional.
  - **Interior weighting:** Auto's diffuse/specular/sheen/clearcoat fill is multiplied by `1 - skyDiffuse` from A3.9.
    Open surfaces get zero, hidden interiors get the full fill; authored AO and SSAO still apply. Without an active
    volume Auto is uniform, because visibility is unknown (as Manual). With a volume the separate open ground plane
    receives no Auto term. The sun, exposure thresholds, emissive scale, background and IBL input are unchanged.
  - **Controls/telemetry:** Ambient Light enable Auto/On/Off, intensity Auto/Manual, fill fraction slider; Manual
    retains colour and luminance controls. CLI `--ambient auto|on|off`, `--ambient-mode auto|manual`, `--ambient-fill k`
    (0..1), registry `ambient`, `ambient_mode`, `ambient_fill`. State/profile `ambient` adds setting, effective mode,
    modeSetting, fill, outdoorIlluminance, outdoorIlluminanceLux, interiorWeighted, resulting intensity (cd/m² and
    environment units); radiance is zero while disabled. The new controls use unique ImGui IDs.
  - **Validation:** `validate_sun_source.py` (serial regression list) covers all 16 legacy combinations with ambient
    explicitly off on both binaries, source options/restoration, HDR lock, argument order, Manual ambient parity,
    HDR defaults and CLI=pipe. `validate_ambient.py` adds closed/open-floor `probe` and `probe_region` checks against
    albedo × k × E_out / pi within the existing 1% tolerance. `sweep_sun_source_ambient.py` uses the unmodified
    `validate_night.py` regions and thresholds to select the largest passing fill fraction.

**Exit gate:**
- Shadow tests pass.
- `noCastShadow` is honored.
- Exterior and cockpit views are reviewed against the reference screenshots at the same sun direction.
- Cost and quality presets are recorded.
- Cockpit interiors with AO are reviewed against the same references; the ground plane shows the aircraft's shadow.
- The DA62 cockpit with sky portals is reviewed against the SDK's before/after example.
