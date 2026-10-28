# Phase A3 — Sun, Shadows, Macro Lights, SSS, AO, Ground, Sky Portals

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** In progress: A3.1, A3.2, A3.4 and A3.6 done (A3.4 ahead of A3.1).

**Entry:** A2.2, the deferred lighting pass. **Goal:** shadows from a sun (user priority 4), and the remaining
lighting features of user priority 5. The sun's direction is set by hand here and driven by time of day in A6.

## Tasks

- [x] **A3.1 — Sun light (R3). Owner: Claude.** A directional light with direction (azimuth and elevation, set by
  hand), color and intensity, in ~~Viewport Settings~~ Scene Inspector → Environment → *Sun* (scene-level, like
  exposure and time of day: one sun per glTF scene, shared by every viewport).
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
- [ ] **A3.5 — Ambient occlusion for sky light (R3). Owner: Claude.** Shadows block only the sun; without this
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
      -haze|-grid` (AGENTS.md section 4.2); registry keys `ground`, `ground_height_mode`, `ground_height`,
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
    receive A3.5 ambient occlusion (A3.5 is not done); the tyre-contact check uses the E195 copy without ground
    support equipment.
- [ ] **A3.7 — `ASOBO_sky_portal` (R3). Owner: Claude.** Window area lights that bring sky and sun light into
  the cabin. Entry: A3.1 (and A3.2 for the sun term).
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

**Exit gate:**
- Shadow tests pass.
- `noCastShadow` is honored.
- Exterior and cockpit views are reviewed against the reference screenshots at the same sun direction.
- Cost and quality presets are recorded.
- Cockpit interiors with AO are reviewed against the same references; the ground plane shows the aircraft's shadow.
- The DA62 cockpit with sky portals is reviewed against the SDK's before/after example.
