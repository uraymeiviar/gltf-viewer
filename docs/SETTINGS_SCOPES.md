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

`GltfExtensionSettings::groups()` lists the vendors and every supported extension (from `gltf::supportedExtensions()`).
An ASOBO rendering extension applies unless either its vendor (`ASOBO`) or its own name is disabled; every other
extension is read unless its own name is disabled (see the table below). The ASOBO group also contains interior sky reflection (default 0.5) and decal
depth tolerance (default 1.2 mm). Switches and tuning have separate effective and saved values. UI edits save only
the changed key; pipe edits commit only that key under `set_persist(true)`. CLI and ordinary script edits stay
session-only, including through an unrelated save. `[GltfExtensions]` contains disabled `name=0` switches and
non-default numeric tuning. Unknown future switch names are retained; invalid tuning warns and uses defaults.

### Extension switches (File > Settings > glTF Extensions)

Every extension Glider supports has a switch keyed by its name (registry key, `settings.ini` key, `get_state`
`gltfExtensions.extensions.<name>`), grouped by vendor: MSFS (ASOBO) first (its long-standing layout and master
switch), then Khronos (KHR), multi-vendor (EXT), Microsoft (MSFT) and any other vendor (`ux3d`). The one source is
`gltf::supportedExtensions()` next to `gltf::isExtensionSupported` (`gltf/Extensions.h`): vendor, support status,
description and what off means. Each group is collapsible, has *Enable all*, and the tab has a search filter; a row
shows the switch (name), its support (Rendered / Partial / Read / Metadata / Not applicable) and its description,
and the tooltip says what off means.

Off means Glider behaves as a loader without the extension. Two kinds of switch:

- **Applied while rendering** (the ASOBO rendering switches, unchanged): the file is rendered as if it had no such
  extension; the master switch `ASOBO` turns all of them off; a change redraws at once, the file is not reloaded.
- **Applied at load** (every other switch): `GltfFromJson` reads the file as if the extension were absent: it is
  removed from `extensionsUsed` / `extensionsRequired` and from every `extensions` object of the document (not from
  `extras`), so the core fallbacks are used. A file that lists it in `extensionsRequired` is not loaded:
  `[Loading] the file requires X, which is switched off (File > Settings > glTF Extensions): the file is not loaded`,
  then "Failed to load". An extension that cannot be ignored (KHR_mesh_quantization) refuses a file that declares it
  at all. A change of such a switch reloads the open file when the file declares the extension (queued like File >
  Open: the view is a fresh open with the new switch, framed again); a file refused because of a switch is opened
  again when that switch is turned back on. A file that does not declare it is not reloaded. The log line
  `[Loading] switched off in File > Settings > glTF Extensions, read as absent: X` names what was skipped.

The validator reports a switched-off extension a file declares like an unsupported one: warning
`EXTENSION_REQUIRED_SWITCHED_OFF` for a required one, info `EXTENSION_USED_SWITCHED_OFF` for a used one.

| Extension | Vendor | Support | Off |
|---|---|---|---|
| `KHR_animation_pointer` | KHR | Rendered | Channels with target path `pointer` are skipped; node channels still play. |
| `KHR_audio` | KHR | Metadata | Not read: no audio sources or emitters (Glider plays no sound). |
| `KHR_draco_mesh_compression` | KHR | Read | Primitives use their uncompressed fallback accessors (most Draco files have none and require it); required: not loaded. |
| `KHR_lights_punctual` | KHR | Rendered | No lights are created from the file. |
| `KHR_materials_anisotropy` | KHR | Rendered | Ignored: core material. |
| `KHR_materials_clearcoat` | KHR | Rendered | Ignored: core material. |
| `KHR_materials_dispersion` | KHR | Rendered | Ignored: no dispersion (single-IOR refraction). |
| `KHR_materials_emissive_strength` | KHR | Rendered | Ignored: core material (emissive at strength 1). |
| `KHR_materials_ior` | KHR | Rendered | Ignored: core material. |
| `KHR_materials_iridescence` | KHR | Rendered | Ignored: core material. |
| `KHR_materials_pbrSpecularGlossiness` | KHR | Rendered | The materials use their core metallic-roughness values. |
| `KHR_materials_sheen` | KHR | Rendered | Ignored: core material. |
| `KHR_materials_specular` | KHR | Rendered | Ignored: core material. |
| `KHR_materials_transmission` | KHR | Rendered | Ignored: core material. |
| `KHR_materials_unlit` | KHR | Rendered | Shaded as core PBR materials. |
| `KHR_materials_variants` | KHR | Rendered | No variants: every primitive shows its default material. |
| `KHR_materials_volume` | KHR | Rendered | Ignored: core material. |
| `KHR_mesh_quantization` | KHR | Read | Cannot be ignored: a file that declares it is not loaded. |
| `KHR_meshopt_compression` | KHR | Read | Compressed bufferViews not decoded, fallback buffers read; required: not loaded. |
| `KHR_node_hoverability` | KHR | Metadata | Not read: every node is hoverable (Glider has no hover highlight). |
| `KHR_node_selectability` | KHR | Rendered | Every node is selectable by a viewport click (its pointer channels are skipped); required: not loaded. |
| `KHR_node_visibility` | KHR | Rendered | Every node is visible (its pointer channels are skipped); required: not loaded. |
| `KHR_texture_basisu` | KHR | Rendered | Textures use their core source; none: no image; required: not loaded. |
| `KHR_texture_transform` | KHR | Rendered | Texture coordinates untransformed; required: not loaded. |
| `KHR_xmp_json_ld` | KHR | Metadata | Not read: no XMP metadata. |
| `EXT_instance_features` | EXT | Metadata | Not read: no instance feature IDs listed; required: not loaded. |
| `EXT_mesh_features` | EXT | Metadata | Not read: no feature IDs listed; required: not loaded. |
| `EXT_mesh_gpu_instancing` | EXT | Rendered | Each instanced node draws its mesh once (the node's own transform). |
| `EXT_meshopt_compression` | EXT | Read | Compressed bufferViews not decoded, fallback buffers read; required: not loaded. |
| `EXT_structural_metadata` | EXT | Metadata | Not read: no metadata listed; required: not loaded. |
| `EXT_texture_webp` | EXT | Rendered | Textures use their core source; none: no image; required: not loaded. |
| `MSFT_texture_dds` | MSFT | Rendered | Textures use their core source; none: no image; required: not loaded. |
| `CESIUM_RTC` | CESIUM | Partial | Not read: the center is not shown (rendering is unchanged: Glider renders relative to it either way); required: not loaded. |
| `CESIUM_primitive_outline` | CESIUM | Metadata | Not read: outlines not counted (never drawn); required: not loaded. |
| `ux3d_gestaltor` | ux3d | Metadata | Gestaltor's storage hints ignored (animations use their default playing state). |
| `ASOBO_*` rendering switches | ASOBO | Rendered / Partial | Rendered as if the file had no such extension (no reload; also off with `ASOBO`). |
| `ASOBO_animation_retargeting`, `ASOBO_softbody_mesh`, `ASOBO_tags`, `ASOBO_unique_id` | ASOBO | Metadata | Not read (not listed in the Inspector); applied at load, not by the master switch. |
| `ASOBO_material_disable_motion_blur`, `ASOBO_material_fake_terrain` | ASOBO | Not applicable | Not read; nothing visible changes. |
| `ASOBO_asset_optimized` | ASOBO | Rendered | No switch: it describes how the file stores its vertex data and is always read (`--asobo-disable` keeps it applied, A1.9). |

Persistence is unchanged: `[GltfExtensions]` holds only switched-off names (`name=0`) and non-default vendor tuning,
written only by `GltfExtensionSettings`; unchanged content is never rewritten; `--isolated` reads and writes nothing;
`--disable-extension` / `--asobo-disable` / `--asobo` apply to the run only. Verified by
`scripts/validate_extension_switches.py`.

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
