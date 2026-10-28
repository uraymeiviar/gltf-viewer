# Phase A3 — Sun, Shadows, Macro Lights, SSS, AO, Ground, Sky Portals

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** In progress: A3.1 and A3.4 done (A3.4 ahead of A3.1).

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
- [ ] **A3.2 — Cascaded shadow maps (R4). Owner: Claude.** Shadows from the sun: cascades fitted to each
  viewport's frustum, and filtering (PCF), applied in the deferred lighting pass.
  - **`ASOBO_material_shadow_options`:** materials with `noCastShadow` are excluded from the shadow pass.
  - **Transparent and glass:** cast or not according to the A0.6 notes.
  - **Quality settings:** resolution, cascade count and filtering are user settings, as in Phase 7.
  - **Tests:**
    - a known occluder and receiver give a shadow edge at the expected position;
    - no acne or peter-panning on fixtures at grazing sun angles;
    - `noCastShadow` objects cast nothing;
    - stable under camera motion and TAA.
  - **Cost:** measured on the three-viewport setup. The shadow map is shared between viewports when the sun is
    the same.
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
- [ ] **A3.6 — Ground plane (R3). Owner: Claude.** A ground choice per viewport: *Grid* (current), *Plane*
  (an infinite ground at the model's lowest point or at y = 0, with a colour or a tiling material such as tarmac
  or grass), and *Ocean* (A6.8). The plane receives the A3.2 sun shadow, which gives the aircraft's shadow on the
  ground, and the grid can be drawn over it. It reuses the ray-traced grid path (`--grid-mode ray`) so it reaches
  the horizon without a mesh.
  - **Ground albedo:** the plane's average colour feeds the A6.2 sky's ground albedo, so the lower hemisphere of
    the environment lighting (reflections on fuselage bellies and wing undersides) matches what is drawn. With
    the fixed HDR the HDR's own lower half is kept.
  - **Tests:** the shadow of a known occluder lands at the expected position on the plane; *Grid* keeps the
    current image byte for byte.
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
