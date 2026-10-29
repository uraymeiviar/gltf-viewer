# Settings scopes: global quality, scene effects, viewport view

The settings used to be split between Viewport Settings (per viewport), the quality presets (renderer-wide) and the
Scene Inspector (per glTF scene) without a clear rule: anti-aliasing and the SSR / SSAO quality were per viewport, the
sun-shadow switch was renderer-wide, the IBL filter (a cost setting) was edited in the Scene Inspector. The rule now:

| Scope | Owner | What belongs there | Persisted |
|---|---|---|---|
| **Quality** (global) | `RenderQualitySettings` (`header/imgui/RenderQuality.h`), the four quality presets | everything that costs time or memory: MSAA, post AA (none / FXAA / TAA), render scale, grid technique, IBL filter sizes / samples / precision, SSR / SSAO / sky-occlusion / ocean quality, SSAO resolution, shadow-map size / cascades / distance / filter and the shadow tuning (split lambda, biases), texture cap, the presets' "allow" gates (sun shadows, SSR, bloom, skybox, IBL, lights). Auto quality adapts this scope. | as before: `[RenderQuality]`, `[QualityProfiles]` |
| **Scene** | `SceneEnvironmentState` (one per glTF scene, `header/imgui/SceneEnvironment.h`); the moved settings are `SceneEnvironmentState::effects` (`SceneEffectSettings`, `header/imgui/SettingsScopes.h`) | whether an effect is on and how it looks: sun shadows on/off, windshield SSR on/off and its interior sky reflection, SSAO on/off, radius, intensity, the aircraft's windshield icing, the sky-portal strength, the decal depth tolerance (plus everything that was already there: sun, sky, exposure, bloom, ambient, sky occlusion on/off) | no (as before) |
| **Viewport** (view) | `ViewportSettingsState` (`header/imgui/ViewportSettingsPanel.h`) | how this view looks at the scene: camera, shading mode, debug render modes, wireframe overlay and its colour / blend, grid, ground and its parameters, extension display switches (ASOBO, Khronos), face-culling debug switches, helper gizmos, turntable, the lens effect, and one override per effect: **Scene / Off / On** for shadows, SSR and SSAO (default Scene) | no (as before) |

There is no per-viewport quality any more. To compare AA modes side by side, run two instances.

## Classification of every former per-viewport setting

| Setting (field) | Key / flag | Now | Reason |
|---|---|---|---|
| `renderMode` | `render_mode` / `--debug-view`, `--render-mode` | view | how this view shows the scene |
| `showGrid` | `grid` / `--hide-grid` | view | helper |
| `ground` (type, height, preset, albedo, roughness, detail, joints, haze, grid on ground) | `ground*` / `--ground*` | view | kept per viewport (A3.6) |
| `ground.ocean` (wind, fetch, choppiness, swell, foam, colour, period, mode, time, play) | `ocean_*` / `--ocean-*` | view | ground parameters; the live / looping mode stays a ground choice |
| `ground.ocean.quality` | `ocean_quality` / `--ocean-quality` | **quality** | simulation cost (cascades x resolution x frames) |
| `forceDoubleSided`, `invertWinding` | `force_double_sided`, `invert_winding` | view | debug view switches |
| `asobo` (master and per extension) | `asobo`, `ASOBO_*`, `show_invisible_materials` / `--asobo*` | view | display switches (kept per viewport) |
| `materialExtensions` | Inspector display switches | view | display switches |
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
| (Scene Inspector) IBL filter settings | `--ibl-*` | quality (unchanged); edited in the Quality tab now | cost; the Scene Inspector only shows a summary |

## Viewport overrides

`viewport_shadows`, `viewport_ssr`, `viewport_ssao` (`scene` / `off` / `on`, default `scene`). The effective value of a
viewport is `resolveEffect(override, scene)`; the shadows additionally need the quality gate (`shadows_allowed`) and the
sun, SSR the presets' / Auto's SSR gate (`RenderQuality::allowSsr`). A new viewport copies the active one's overrides.

## Keys and flags (every former key and flag still works)

| Flag / key | Meaning now |
|---|---|
| `--aa`, `--msaa`, `--post-aa`; `aa`, `msaa`, `post_aa` (renderer) | the global AA (later flags win; `--msaa` / `--post-aa` still apply after every `--aa`) |
| `--ssr-quality`, `--ssao-quality`, `--ssao-resolution`, `--ocean-quality`, `--sky-occlusion-quality`; same keys (renderer) | the global quality |
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
viewport, and add the UI to the Scene Inspector's "Shadows, Reflections & AO" (or the Quality tab).

## UI

* **Viewport Settings → Render Modes**: debug mode, **Effect overrides (Scene / Off / On)** for shadows, SSR and SSAO
  with the viewport's current state (in the rows the AA combos used, so the controls below keep their places), face
  culling, MSFS extension display switches, "Show sky portals", wireframe, material variants.
* **Viewport Settings → Camera & Helpers**: unchanged; the ocean's Quality line is read-only and points to the
  Quality tab.
* **Viewport Settings → Quality** (renderer-wide, the single place for quality): preset selector, Anti-Aliasing
  (MSAA, Post AA), Viewport (ground grid technique, render scale), Effects (SSR, SSAO, SSAO resolution, sky occlusion,
  ocean), Image-Based Lighting (the IBL filter), Sun Shadows ("Allow sun shadows", map size, cascades, distance,
  filter, split lambda, biases).
* **File → Settings → Quality Presets**: also SSAO quality / resolution, sky occlusion and ocean quality per preset;
  the cost gates are labelled "Allow ...".
* **Scene Inspector → Shadows, Reflections & AO** (after Sun & Time of Day): sun shadows, windshield reflections (SSR)
  and interior sky reflection, SSAO with radius and intensity, Aircraft state (icing; sky-portal strength for files with
  portals; decal depth tolerance for files with decals). The IBL filter is shown as a summary; Sky Occlusion shows its
  bake quality read-only.

## Persistence

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
* Editing the IBL filter moved to the Quality tab; the Scene Inspector's Sky Occlusion shows its quality read-only.
* `--decal-prepass-check` sets the global AA to none (it did so for every viewport).
