# Settings scopes: Quality / Scene / Viewport

Quality decides whether an effect runs and how well, Scene decides how the world looks, and Viewport decides how
this view shows it. Vendor tuning lives with its glTF extension. This implements the layout agreed on 2026-10-30.

| Layer | Owner | Persistence / UI |
|---|---|---|
| Quality, global | `RenderQualitySettings`, four `QualityProfile` definitions | `[RenderQuality]`, `[QualityProfiles]`; File > Settings > Quality, status-bar preset combo beside FPS |
| Scene, per glTF scene | `SceneEnvironmentState`, `ground`, `effects` | Session only; Scene Inspector |
| Viewport | `ViewportSettingsState` | Session only; toolbar gear selects that viewport in the Inspector |
| glTF extensions, app | `GltfExtensionSettings` | `[GltfExtensions]`; File > Settings > glTF Extensions |

## Quality

Low, Mid, High and Ultra carry every cost setting. Auto targets editable Ultra and adapts the same budgets and
stages as before. Explicit AA, reflection, shadow and IBL budget edits pin their groups for the session.

* Reflections (SSR) off / low / mid / high. Non-off enables the ASOBO windshield reflection too.
* SSAO off / low / mid / high; half / full resolution shown only while enabled. Preset defaults: Low off, Mid low,
  High mid, Ultra high (launches that apply no profile, such as captures and `--isolated`, keep it off).
* Shadow map off / 1024 / 2048 / 4096. Non-off means an enabled sun casts shadows. Cascades, distance, filter,
  split lambda and biases shown only while enabled; each profile carries tuning too.
* IBL cube off / 256 / 512 / 1024. Irradiance (32 / 64 / 128 / 256 / full cube), LUT, samples and precision shown
  only while enabled. Off disables environment lighting while the scene background can remain visible.
* Sky occlusion auto / on / off and bake quality. Auto is on with procedural sky, off with HDR.
* Ocean quality, precomputed / live FFT mode, animation on / off.
* Bloom on / off; lens effect off / ghosts + halo / ghosts + streak / anamorphic streak. **Lens intensity is
  Quality**, default 1. Legacy numeric modes remain 0 off, 1 ghosts, 2 streak, 3 both. The existing bloom chain
  produces lens effects, so bloom off removes them too.
* Punctual lights, texture cap, MSAA, post AA, render scale and grid technique.

Enable bits and retained budgets implement the levels internally. Off retains the last budget; toggling an old
bool back on restores that choice. No separate Allow gates appear in the UI.

## Scene

All viewports share one scene environment, ground surface, ocean simulation and ocean clock. Ground is off /
plane / ocean, with height, plane preset, colour, roughness, detail, joints, haze; ocean wind, fetch, choppiness,
swell, foam, colour and period. Simulation mode, quality and animation belong to Quality.

The scene also owns environment source, background on / off, clear colour, HDR file / rotation / intensity / blur;
sky Mie density, ozone, ground albedo override, airglow and atmospheric refraction; sun source and attributes;
clock date / time / place / heading / playback / speed; sun disk, moon, stars, ambient; exposure, metering, tone
mapping, night vision; bloom strength / radius; SSAO radius / intensity; emissive day / night, MSFS emissive scale;
and temporary windshield icing. Background off uses clear colour while the environment still lights the scene.
Profiles never gate background. There are no scene switches for shadows, SSR or SSAO.

Scene settings are not persisted. Ground resets with the scene on model load; existing appearance effect
carry-over is preserved. Viewports hold resolved mirrors and references, not independent scene settings.

## Viewport

**Shaded + post effects** (default) is the previous image. **Shaded** omits post AA, bloom, lens effects, SSR and
SSAO, retaining geometry MSAA, shadows, exposure and tone mapping. Debug modes remain.

The ground grid is an independent overlay per viewport, at the scene ground height (including ocean sea level),
or Y = 0 without ground. Show ground grid replaces the old grid-on-ground conjunction.
Wireframe, compass, selection visualizers, camera projection / FoV / near / far / glTF attachment, turntable,
view presets, navigation, culling debug, sky-portal gizmos, material variants and per-material Khronos display
switches remain view settings. Scene / Off / On controls are removed.

## glTF extensions and ASOBO tuning

`GltfExtensionSettings::groups()` lists vendors and rendered extensions. An extension applies unless either its
vendor or its own name is disabled. The ASOBO group also contains interior sky reflection (default 0.5) and decal
depth tolerance (default 1.2 mm). Switches and tuning have separate effective and saved values. UI edits save only
the changed key; pipe edits commit only that key under `set_persist(true)`. CLI and ordinary script edits stay
session-only, including through an unrelated save. `[GltfExtensions]` contains disabled `name=0` switches and
non-default numeric tuning. Unknown future switch names are retained; invalid tuning warns and uses defaults.

Sky portals are implicit with either environment source, at default strength **0.35**. Disable `ASOBO_sky_portal`
or ASOBO to remove them. The slider is gone. `sky_portal_strength` / `--sky-portal-strength` remain hidden
compatibility overrides with their previous range and rendering semantics.

## Compatibility keys and flags

Old-scope forwarders carry registry `compat` / `forwardsTo`. Every old flag still parses. The renderer fallback
of `set_viewport_setting` remains available.

| Old key / flag | Current home / behavior |
|---|---|
| `aa`, `msaa`, `post_aa`; corresponding flags | Global Quality; explicit MSAA / post AA flags retain precedence after `--aa`. |
| `ssr`, `--ssr`; old environment / viewport scopes | Quality reflection enable bit; `reflections` combines off / low / mid / high. |
| `ssr_quality`, `--ssr-quality` | Quality retained sample budget, including while off. |
| `ssao`, `--ssao`; old scopes | Quality enable bit; `ssao_level` combines off / low / mid / high. |
| `ssao_quality`, `ssao_resolution`, corresponding flags | Quality budget / resolution. |
| `ssao_radius`, `ssao_intensity`, `windshield_icing`, corresponding flags | Scene appearance; old viewport names forward. |
| `shadows`, `--shadows`, `shadows_allowed` | Quality map enable bit; `shadow_map` combines off / 1024 / 2048 / 4096. |
| `shadow_*`, `--shadow-*` | Quality budget / tuning. `--shadow-resolution off` disables the map. |
| `--ibl-size`, `--ibl-irradiance-size`, `--ibl-lut-size`, `--ibl-samples`, `--ibl-precision` | Quality filter. `--ibl-size off` disables lighting; `ibl_cube` combines off and size. |
| `ibl`, `punctual_lights`, `bloom`, `--bloom` in environment | Forward to Quality; bloom intensity / radius stay Scene. |
| `sky_occlusion`, `sky_occlusion_quality`, corresponding flags | Quality; old environment names forward. |
| `ocean_quality`, `ocean_mode`, `ocean_play`, corresponding flags | Quality; old viewport names forward. |
| `ground`, other `ground_*` / `ocean_*`, corresponding flags | Scene; old viewport names forward. `ground grid` aliases ground off. |
| `grid`, `--hide-grid`, `ground_grid`, `--ground-grid` | One viewport grid overlay switch; later flags win. |
| `lens_flare`, `lens_flare_intensity`, corresponding flags | Quality; old viewport names forward. |
| `interior_sky_reflection`, `decal_tolerance_mm`, corresponding flags | ASOBO app tuning; old environment / viewport names forward to renderer. |
| `sky_portal_strength`, `--sky-portal-strength` | Hidden scene compatibility value, default 0.35; no slider. |
| `viewport_shadows`, `viewport_ssr`, `viewport_ssao` | Accepted enums, warned no-op; getters return `scene`. Use Quality levels or viewport Shaded. |
| `render_mode`, `--render-mode`, `--debug-view`, `set_render_mode` | `shaded_post` default (`none` / `default` aliases), `shaded` without post effects, existing debug names. |
| `asobo`, `ASOBO_*`, `show_invisible_materials`, corresponding flags | Global app switches; old viewport names forward as before. |

## Persistence and migration

The same single `SettingsFile` store owns every section. Only non-default values are written, empty sections
removed, unchanged content not rewritten. `--isolated` reads and writes nothing. `GLIDER_DATA_DIR` selects a
persistence-test store and disables legacy-directory migration.

`[QualityProfiles]` reads old `ssr`, `ibl`, `bloom`, `lights`, `shadows` gates and independent budgets, mapping them
to the new levels without discarding budgets. New explicit fields win regardless of entry order. Writers use
`ssrEnabled`, `ssaoEnabled`, `iblEnabled`, `bloomEnabled`, `lightsEnabled`, retained budgets, `skyOcclusion`,
`oceanLive`, `oceanPlay`, `lensFlare`, `lensFlareIntensity`, texture cap and shadow tuning. Old `skybox` profile
metadata is retained but no longer gates background.

`[RenderQuality] shadows=0` disables shadows and retains `shadowResolution`. Existing filter / tuning keys still
read. New fields use the profile field names. The selected profile is applied before saved current overrides.
Quality separates saved values from session overrides: `set_persist(true)` commits the requested key; UI edits
save changed fields. CLI / session overrides are not saved at shutdown or by unrelated saves. No scene / viewport
state is added to settings.ini.

Current overrides are compared against the saved selected profile, including its edited definition. A value that
equals a struct default is still written when it overrides that profile (for example MSAA Off over Ultra 4x).

## Diagnostics

`get_state.quality.configured/effective` includes reflection / SSAO levels, shadow map, IBL cube, bloom, lens
mode / intensity, lights, texture cap, sky occlusion and ocean mode / play. `environment.ground` describes the
scene surface. `viewports[].ground` retains compatibility and shows that same simulation. `postEffects` and
`effective` explain Shaded. `gltfExtensions.tuning/savedTuning` reports vendor tuning. Old budget fields,
`environment.effects`, `viewports[].overrides`, top-level ground and effect reports remain for existing scripts.
`--profile-report` carries the Quality and vendor objects too.
