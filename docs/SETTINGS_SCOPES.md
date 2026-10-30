# Settings scopes: global quality, scene effects, viewport view

The settings used to be split between Viewport Settings (per viewport), the quality presets (renderer-wide) and the
Scene Inspector (per glTF scene) without a clear rule: anti-aliasing and the SSR / SSAO quality were per viewport, the
sun-shadow switch was renderer-wide, the IBL filter (a cost setting) was edited in the Scene Inspector. The rule now:

| Scope | Owner | What belongs there | Persisted |
|---|---|---|---|
| **App settings** | `GltfExtensionSettings` (`header/imgui/GltfExtensionSettings.h`) | which glTF extensions Glider applies (ASOBO now; KHR_/EXT_/other vendors later): one set for the whole app, every viewport and model | yes, `[GltfExtensions]` |
| **Quality** (global) | `RenderQualitySettings` (`header/imgui/RenderQuality.h`), the four quality presets | everything that costs time or memory: MSAA, post AA (none / FXAA / TAA), render scale, grid technique, IBL filter sizes / samples / precision, SSR / SSAO / sky-occlusion / ocean quality, SSAO resolution, shadow-map size / cascades / distance / filter and the shadow tuning (split lambda, biases), texture cap, the presets' "allow" gates (sun shadows, SSR, bloom, skybox, IBL, lights). Auto quality adapts this scope. | as before: `[RenderQuality]`, `[QualityProfiles]` |
| **Scene** | `SceneEnvironmentState` (one per glTF scene, `header/imgui/SceneEnvironment.h`); the moved settings are `SceneEnvironmentState::effects` (`SceneEffectSettings`, `header/imgui/SettingsScopes.h`) | whether an effect is on and how it looks: sun shadows on/off, windshield SSR on/off and its interior sky reflection, SSAO on/off, radius, intensity, the aircraft's windshield icing, the sky-portal strength, the decal depth tolerance (plus everything that was already there: sun, sky, exposure, bloom, ambient, sky occlusion on/off) | no (as before) |
| **Viewport** (view) | `ViewportSettingsState` (`header/imgui/ViewportSettingsPanel.h`) | how this view looks at the scene: camera, shading mode, debug render modes, wireframe overlay and its colour / blend, grid, ground and its parameters, per-material Khronos display switches, face-culling debug switches, helper gizmos, turntable, the lens effect, and one override per effect: **Scene / Off / On** for shadows, SSR and SSAO (default Scene) | no (as before) |

There is no per-viewport quality any more, and there are no per-viewport extension switches any more. To compare AA modes side by side, run two instances.

## Target layout (agreed 2026-10-30, not implemented yet)

The next step replaces the effect switches and the Scene / Off / On overrides with one rule: **Quality decides
whether an effect runs and how well, the Scene decides how the world looks, the Viewport decides how this view
shows it.** Vendor-specific tuning lives with its extension (File > Settings > glTF Extensions), not in these layers.

**Quality** (global, presets, Auto adapts it; File > Settings > Quality, the preset combo in the status bar beside
the FPS readout): texture cap; MSAA (off/2x/4x/8x); post AA (off/FXAA/TAA); render scale (the whole 3D image);
grid technique (ray/plane); IBL cube (off/256/512/1024) and, unless off, irradiance size (32/64/128/256/full cube),
LUT (128..1024), samples (128..1024), precision (half/float); reflections (SSR: off/low/mid/high; not off implies the
ASOBO windshield reflection); SSAO (off/low/mid/high) and, unless off, its resolution (half/full); sky occlusion
(auto/on/off) and its quality; ocean quality, mode (precomputed/live FFT) and animation (on/off); shadow map
(off/1024/2048/4096; not off = the sun always casts shadows), cascades, distance, filter, plus the advanced tuning
(split lambda, biases); bloom on/off; lens effect (off/ghosts+halo/ghosts+streak/anamorphic streak); punctual lights
on/off.

**Scene** (one per glTF scene, Scene Inspector): ground (off/plane/ocean) and the plane's and ocean's appearance
(height, preset, colour, roughness, detail, joints, haze; wind, fetch, choppiness, swell, foam, colour, period);
environment source (HDR/procedural sky) and background on/off (off = solid clear colour; the environment still
lights the scene); HDR file, rotation, intensity, blur; sky: Mie density, ozone, ground-albedo override, airglow,
atmospheric refraction; sun source and attributes (direction, colour, intensity, angular radius); time-of-day clock
(date, time, place, heading, play, speed); sun disk and limb darkening; moon and stars; ambient light; exposure
(mode, compensation, limits, metering), tone mapping, night vision; bloom strength and radius; SSAO radius and
intensity; emissive day/night and MSFS emissive scale; windshield icing (temporary, until a weather system drives it).

**Viewport** (Viewport Settings; a gear button in the viewport toolbar selects the viewport so the Inspector shows
it): render mode (Shaded + post effects, Shaded, debug views; Shaded turns off post AA, bloom, lens effect, SSR and
SSAO and keeps shadows and tone mapping); wireframe overlay/mode, colour, blend; 3D compass; ground grid (drawn at
the scene ground's height, y = 0 without ground); selection visualizers (boxes, node gizmo); camera projection, FoV,
near/far, attach to glTF camera; turntable; view presets; navigation; face-culling debug switches; show sky portals;
material variants; per-material Khronos display switches.

**glTF extensions** (app setting, File > Settings > glTF Extensions, per vendor group): the switches as today, plus
the ASOBO group's tuning: interior sky reflection strength (windshields), decal depth tolerance. Sky portals are
implicit with either environment source (fixed strength; switch `ASOBO_sky_portal` off to remove them).

## Classification of every former per-viewport setting

| Setting (field) | Key / flag | Now | Reason |
|---|---|---|---|
| `renderMode` | `render_mode` / `--debug-view`, `--render-mode` | view | how this view shows the scene |
| `showGrid` | `grid` / `--hide-grid` | view | helper |
| `ground` (type, height, preset, albedo, roughness, detail, joints, haze, grid on ground) | `ground*` / `--ground*` | view | kept per viewport (A3.6) |
| `ground.ocean` (wind, fetch, choppiness, swell, foam, colour, period, mode, time, play) | `ocean_*` / `--ocean-*` | view | ground parameters; the live / looping mode stays a ground choice |
| `ground.ocean.quality` | `ocean_quality` / `--ocean-quality` | **quality** | simulation cost (cascades x resolution x frames) |
| `forceDoubleSided`, `invertWinding` | `force_double_sided`, `invert_winding` | view | debug view switches |
| `asobo` (master and per extension), `ASOBO_*`, `show_invisible_materials` / `--asobo*` | `asobo`, `ASOBO_*`, `show_invisible_materials` | **app setting** (formerly view) | not a view choice: a switch turned off stays off after a restart; one global set |
| `materialExtensions` | Inspector display switches | view | per-material Khronos display switches (unchanged) |
| `showSkyPortals` | `show_sky_portals` | view | gizmo |
| `lensFlare`, `lensFlareIntensity` | `lens_flare*` / `--lens-flare*` | view | a property of this view's camera lens, not of the scene; it stays per viewport |
| wireframe overlay / colour / blend, bounding boxes, node gizmo, axis convention, turntable, compass | `wireframe_*`, `scene_bounding_box`, `selection_box`, `node_gizmo` | view | helpers |
| `antiAliasingIdx`, `msaaIdx`, `postAAIdx` | `aa`, `msaa`, `post_aa` / `--aa`, `--msaa`, `--post-aa` | **quality** | cost; one AA for every viewport |
| `ssrQuality` | `ssr_quality` / `--ssr-quality` | **quality** | sample budget |
| `screenSpaceReflections` | `ssr` / `--ssr` | **scene** + override `viewport_ssr` | an effect of the scene's glass |
| `interiorSkyReflection` | `interior_sky_reflection` | **scene** | look of the windshield reflection |
| `windshieldIcing` | `windshield_icing` | **scene** | the aircraft's state, the same in every view |
| `skyPortalStrength` | `sky_portal_strength` | **scene** | the scene's window light |
| `decalDepthToleranceMm` | `decal_tolerance_mm` / `--decal-tolerance` | **scene** | tuning of the asset's decals, the same in every view |
| `ssao.enabled` | `ssao` / `--ssao` | **scene** + override `viewport_ssao` | effect |
| `ssao.radius`, `ssao.intensity` | `ssao_radius`, `ssao_intensity` | **scene** | look (world units of the scene) |
| `ssao.quality`, `ssao.halfResolution` | `ssao_quality`, `ssao_resolution` | **quality** | cost |
| (renderer-wide) sun shadows on/off | `shadows` / `--shadows` | **scene** + override `viewport_shadows`; the quality keeps a gate (`shadows_allowed`, "Allow sun shadows") | the switch is a look; the gate is the presets' cost choice |
| (renderer-wide) shadow size, cascades, distance, filter | `shadow_resolution`, `shadow_cascades`, `shadow_distance`, `shadow_filter` | quality (unchanged) | cost, part of the presets, adapted by Auto |
| (renderer-wide) split lambda, normal / slope / constant bias | `shadow_split_lambda`, `shadow_*_bias` | quality (unchanged) | they compensate the shadow map's finite resolution and depth precision (the normal bias is in shadow-map texels; lambda spreads the resolution over the cascades), so they belong with the map, not with the scene's look; they stay saved in `[RenderQuality]` |
| (scene) sky-occlusion bake quality | `sky_occlusion_quality` / `--sky-occlusion-quality` | **quality** | cost (cells x directions) |
| (Scene Inspector) IBL filter settings | `--ibl-*` | quality (unchanged); edited in File > Settings > Quality now | cost; the Scene Inspector only shows a summary |

## glTF extension switches (app setting)

`ux3d::imgui::GltfExtensionSettings` (`header/imgui/GltfExtensionSettings.h`, `source/imgui/GltfExtensionSettings.cpp`)
holds a set of switched-off **keys**; a key is an extension name or a vendor group token (`ASOBO`, later `KHR`, ...).
An extension is applied when its group is not off and its name is not off (`applies`). The vendor table is
`GltfExtensionSettings::groups()` (today only **MSFS (ASOBO)**, 17 rendered extensions).

* Two sets: *effective* (this run) and *saved* (`settings.ini`). A user edit (File > Settings > glTF Extensions, the
  Material inspector's per-extension ASOBO checkbox, a pipe edit under `set_persist(true)`) changes both and saves.
  Command-line flags, script edits and pipe edits without `set_persist` change only the effective set and never reach
  `settings.ini`, even when another setting is saved later.
* `settings.ini` section `[GltfExtensions]`: one key per switched-off name with value `0` (`ASOBO=0` is the ASOBO
  master switch, `ASOBO_material_detail_map=0` one extension). Only non-default values; an all-on set removes the
  section. Loaded after `[RenderQuality]`. `--isolated` reads and writes nothing. Invalid keys or values are ignored
  with one warning; unknown future names are kept.
* Renderer: `ViewportSettingsState::asobo` is a resolved mirror written every frame by
  `Application::resolveAllViewportSettings` from the global set, so every viewport uses it; a change of the set marks
  `ViewportSettings` and resets TAA on every viewport.
* Registry: the canonical keys are in scope `renderer`: `asobo` (master), `show_invisible_materials` and one key per
  extension name (`ASOBO_material_detail_map`, ...), all bool, default true (false for invisible). They are not marked
  `persisted`; under `set_persist(true)` the key being set is committed (`SettingSpec::onPersist`). The same keys in the
  `viewport` scope forward to the global value (`compat: true`; a `viewport` argument changes nothing). Script keys
  `asobo` / `invisible` set the global session value. `reset` restores the startup effective set.
* `get_state.gltfExtensions`: `vendors` (`{ASOBO: {title, enabled, key, applied, total}}`), `extensions` (every listed
  name: applied), `disabled` (this run), `saved`, `differsFromSaved`, `persistence`. `viewports[].effective.asobo`
  (`enabled`, `disabled`) shows what each viewport renders with (always identical). `--profile-report` carries
  `gltfExtensions` too.

## Viewport overrides

`viewport_shadows`, `viewport_ssr`, `viewport_ssao` (`scene` / `off` / `on`, default `scene`). The effective value of a
viewport is `resolveEffect(override, scene)`; the shadows additionally need the quality gate (`shadows_allowed`) and the
sun, SSR the presets' / Auto's SSR gate (`RenderQuality::allowSsr`). A new viewport copies the active one's overrides.

## Keys and flags (every former key and flag still works)

| Flag / key | Meaning now |
|---|---|
| `--aa`, `--msaa`, `--post-aa`; `aa`, `msaa`, `post_aa` (renderer) | the global AA (later flags win; `--msaa` / `--post-aa` still apply after every `--aa`) |
| `--ssr-quality`, `--ssao-quality`, `--ssao-resolution`, `--ocean-quality`, `--sky-occlusion-quality`; same keys (renderer) | the global quality |
| `--asobo`, `--asobo-disable`, `--show-invisible-materials`; renderer keys `asobo`, `show_invisible_materials`, `ASOBO_*` | the global extension switch (this run; `set_persist(true)` also saves the key) |
| viewport keys of the same names | compatibility: set the global value |
| `--ssr`, `--ssao`, `--shadows`, `--ssao-radius`, `--ssao-intensity`, `--interior-sky-reflection`, `--windshield-icing`, `--sky-portal-strength`, `--decal-tolerance`; same keys (environment) | the scene's effects (replayed onto every scene after the startup load, like the other environment flags) |
| `viewport_shadows`, `viewport_ssr`, `viewport_ssao` (viewport, new) | the overrides |
| `shadows_allowed` (renderer, new, saved preference) | the presets' sun-shadow gate (`[RenderQuality] shadows=`) |
| compatibility keys in the former scopes: viewport `aa`, `msaa`, `post_aa`, `ssr_quality`, `ssao_quality`, `ssao_resolution`, `ocean_quality` | forward to the global quality |
| viewport `interior_sky_reflection`, `windshield_icing`, `sky_portal_strength`, `decal_tolerance_mm`, `ssao_radius`, `ssao_intensity` | forward to the active scene |
| viewport `ssr`, `ssao` | **this viewport's override** (`true` = On, `false` = Off); the getter returns the viewport's resolved value. Chosen so that per-viewport scripts keep their per-viewport meaning; `set_environment ssr` is the scene's switch. |
| renderer `shadows` | forwards to the scene's `shadows` (no longer a saved preference) |
| environment `sky_occlusion_quality` | forwards to the global quality |
| script `aamode <mode> [viewport]` | the global AA |

A compatibility key carries `compat: true` in the registry (`SettingSpec::forwardsTo`); `set_*` through it marks the
revision of the scope it changes. `list_settings` lists both.

## get_state / --profile-report

* `quality`: `profile`, `auto`, `configured` and `effective` (msaa, postAA, ssrQuality, ssaoQuality, ssaoResolution,
  skyOcclusionQuality, oceanQuality, renderScale, gridMode, iblCubeSize, shadowsAllowed, shadowResolution,
  shadowCascades, shadowFilter), `ssrAllowed`.
* `environment.effects`: the active scene's effects.
* `viewports[]`: `overrides` (shadows / ssr / ssao) and `effective` (shadows, ssr, ssao, msaa, postAA, ssrQuality).
* `sun.shadows`: `enabled` (scene and gate), `scene`, `allowed`, and per viewport `override`, `enabled`, `active`.
* `ssao.viewports[]`: `override` beside the resolved values.

## Implementation

The renderer reads only `ViewportSettingsState`'s **resolved mirrors** (`sunShadows`, `screenSpaceReflections`,
`ssrQuality`, `interiorSkyReflection`, `windshieldIcing`, `skyPortalStrength`, `decalDepthToleranceMm`, `ssao`, the AA
indices, `ground.ocean.quality`). `Application::resolveAllViewportSettings` (`RenderQuality::resolveViewport`) writes
them at the start of every frame, after every registry `set_*` and before `get_state` / the profile report, from the
global quality, the active scene's effects and the overrides; it resets a viewport's TAA history when its AA changed.
`RenderQuality::effectiveViewport` then applies Auto quality (AA, SSR quality, SSR gate) as before. Every read site
therefore stays as it was, which keeps every single-viewport image byte-identical (checked against `0e4fcfc`).

A new effect setting: add the field to `SceneEffectSettings` (or `RenderQualitySettings` for a cost), register it once
in `Application::registerAgentSettings` in its scope, mirror it in `resolveViewport` if the renderer reads it per
viewport, and add the UI to the Scene Inspector's "Shadows, Reflections & AO" (or File > Settings > Quality).

## UI

* **Viewport Settings → Render Modes**: debug mode, **Effect overrides (Scene / Off / On)** for shadows, SSR and SSAO
  with the viewport's current state (in the rows the AA combos used, so the controls below keep their places), face
  culling, "Show sky portals", wireframe, material variants. A dim hint and a **Quality settings...** button open
  File > Settings > Quality.
* **Viewport Settings → Camera & Helpers**: unchanged; the ocean's Quality line is read-only and points to
  File > Settings > Quality. Its "glTF Extensions" block is a note plus a **glTF Extensions settings...** button that
  opens the Settings tab.
* **File > Settings > Quality** (renderer-wide, the single place for quality): the active preset (Auto / Low / Mid /
  High / Ultra) and, while Auto adapts, its status; **Configured and in effect** (MSAA, Post AA, render scale, IBL
  cube, SSR quality, sun shadow map); two sub-tabs: **Presets** (the four definitions, also SSAO quality /
  resolution, sky occlusion and ocean quality per preset; the cost gates are labelled "Allow ...") and **Current
  Settings** (Anti-Aliasing, Viewport (ground grid technique, render scale), Effects (SSR, SSAO, SSAO resolution, sky
  occlusion, ocean), Image-Based Lighting (the IBL filter), Sun Shadows ("Allow sun shadows", map size, cascades,
  distance, filter, split lambda, biases)).
* **File > Settings > glTF Extensions**: Reset to defaults (plus "Restore saved settings" while this run differs from
  the saved set), one section per vendor group with its master switch and one checkbox per rendered extension.
* **Viewport Settings** keeps only view tabs: Render Modes, Camera & Helpers, Post.
* **Scene Inspector → Shadows, Reflections & AO** (after Sun & Time of Day): sun shadows, windshield reflections (SSR)
  and interior sky reflection, SSAO with radius and intensity, Aircraft state (icing; sky-portal strength for files with
  portals; decal depth tolerance for files with decals). The IBL filter is shown as a summary; Sky Occlusion shows its
  bake quality read-only.

## Persistence

`[GltfExtensions]`: one `name=0` per switched-off name, `ASOBO=0` for the master; only non-default values;
command-line flags and pipe edits without `set_persist` are never saved; `--isolated` reads and writes nothing.

No viewport setting was ever written to `settings.ini` (viewport state is session-only), so there is nothing to
migrate from viewports. `[RenderQuality] shadows` keeps its key and now means the gate ("Allow sun shadows");
`[QualityProfiles]` gains `profileN.ssaoQuality`, `ssaoResolution` (`full` / `half`), `skyOcclusionQuality`,
`oceanQuality`, written only when they differ from the preset's default; the AA and SSR-quality profile keys are
unchanged. The global AA / effect quality is not written to `[RenderQuality]` (it was never saved; the presets carry
it). The scene effects are not saved (like the rest of the environment). `--isolated` reads and writes nothing.

## Behaviour changes

* Multi-viewport only: every viewport renders with the one global AA, SSR / SSAO / ocean quality and SSAO resolution.
  A new viewport no longer starts with MSAA Off + TAA (it uses the global AA, TAA by default), so `--aa none` + a
  second viewport renders both without AA; `aamode <mode> <vp>` changes every viewport. The per-viewport SSAO radius /
  intensity, interior sky reflection, icing, sky-portal strength and decal tolerance became scene settings: editing
  them through any viewport changes every viewport.
* The scene effects survive opening another model (the per-viewport settings did); the rest of the environment
  resets as before. The agent `reset` restores the startup scene effects and the startup AA / effect quality.
* `--shadows` / `shadows` switch the scene's shadows (no longer a saved preference; `set_persist(true)` + `shadows`
  writes nothing); the saved gate is `shadows_allowed`.
* The ASOBO switches are global: one viewport's change reaches all, and a switch set in the UI is saved. The viewport
  Quality tab is gone (File > Settings > Quality).
* Editing the IBL filter moved to File > Settings > Quality; the Scene Inspector's Sky Occlusion shows its quality read-only.
* `--decal-prepass-check` sets the global AA to none (it did so for every viewport).
