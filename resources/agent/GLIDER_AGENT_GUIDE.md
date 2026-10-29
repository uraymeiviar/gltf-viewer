---
name: glider-control
description: Drive a running Glider glTF viewer (the instance the user sees) to select, isolate and frame nodes, change viewport and environment settings, capture the viewport and read back pixel values. Use it to inspect a glTF/GLB model visually without relaunching Glider for every question.
---

# Glider agent control

Glider is a glTF 2.0 / MSFS model viewer. A running instance can be driven over a local named pipe
(`\\.\pipe\glider-<pid>`) with line-delimited JSON. The command reference at the end of this guide is
generated from the same registry that executes the commands, so it always matches the executable.

## When to use what

- **Exploration** (find a part, look at it from several sides, try a setting, read a pixel): use the pipe,
  through the MCP server `scripts/glider_mcp.py` or the plain client `scripts/glider_ctl.py`. The model stays
  loaded between questions.
- **Reproducible checks and regression captures**: use the command-line flags (`--frame-node`, `--debug-view`,
  `--capture-viewport`, `--exit-after-profile`, ...) in a fresh process with `--isolated`. Every setting the pipe
  can change has the same effect as its CLI flag (they share one code path); `list_settings` names the flag.

## Enabling and connecting

- Start Glider with `--control-pipe`, or tick **File > Settings > Agent Control > Enable control pipe**
  (session only). Glider logs `Agent control pipe: \\.\pipe\glider-<pid>`; the status bar shows
  "Agent connected" while a client is attached.
- One client at a time. A second client receives `{"ok":false,"error":{"code":"busy",...}}` and is
  disconnected.
- MCP: `python scripts/glider_mcp.py` attaches to the only running instance, `--pid N` picks one,
  `--launch [--exe path] [model]` starts Glider with the pipe. Tools are built from `describe`.
- Plain client: `python scripts/glider_ctl.py frame_node node=Wing view=iso` (values are JSON when they parse,
  otherwise strings); `python scripts/glider_ctl.py --list` lists instances.

## Wire protocol

One JSON object per line in each direction (UTF-8):

```json
{"id": 7, "cmd": "frame_node", "args": {"node": "Wing_L", "view": "iso", "margin": 1.2}}
{"id": 7, "ok": true, "result": {"node": {"index": 12, "name": "Wing_L"}, "camera": {...}}}
{"id": 8, "ok": false, "error": {"code": "node_not_found", "message": "node not found: Wing_X (candidates: Wing_L, Wing_R)", "candidates": ["Wing_L", "Wing_R"]}}
```

Commands run on the render thread between frames, strictly in order. Multi-frame commands (`capture`,
`open_model`, `reset`) reply when they finish; requests sent meanwhile wait behind them.

## Recipe: look at a part

1. `get_state` and `list_nodes(filter="wing")` to find the node.
2. `isolate(node)` to hide everything else (optional; `exit_isolate` restores the scene).
3. `frame_node(node, view="iso", margin=1.2)`; repeat with `front`, `left`, `top`... or `set_camera` for an exact pose.
4. `capture(overlays=false)` and look at the image (`encoding="png-base64"` returns it inline; the MCP server
   always returns it as an image).
5. `pick(x, y)` / `probe(x, y)` at a pixel of that image to ask what is there and what colour it has.
6. `set_render_mode("roughness")`, `set_viewport_setting("ssr", false)`, `set_environment("exposure", 1)`
   ... then capture again to compare.
7. `reset` returns to the startup state.

## Conventions

- Pixel coordinates are capture-image coordinates: origin top-left, x right, y down, the viewport's render
  size (`get_state` lists it).
- World units are glTF metres, Y-up. `set_camera` takes eye and target; `fov_y` in degrees.
- Node names match exactly, then case-insensitively, then as a unique substring; `#12` is node index 12.
  Failures carry `candidates`.
- `probe` reports the linear HDR scene colour **before** TAA, bloom and tone mapping (`hdr`, from the resolved
  scene target), the final 8-bit colour (`ldr`), depth, world position and the picked node/mesh/material.
  It only reads; it never changes the image or TAA history.
- Edits stay in memory. The glTF file is never written; extensions are never added or removed
  (`set_material_param` edits parameters of extensions the material already has).

## MSFS emissive units (A1.6b)

- ASOBO emissive defaults to cd/m² / 12800 with the procedural sky; HDR keeps its previous scale 1/1000.
  Factor × texture × KHR emissive strength × ASOBO day/night multipliers are preserved. For example, the DA62's
  authored 5000 is 0.390625 sky units (5000 cd/m²), and 5 HDR units. ASOBO/MSFS emissive off is unchanged.
- `set_environment msfs_emissive_scale <x>` / `--msfs-emissive-scale <x>` explicitly override **both** sources,
  including an explicit 0.001. `get_environment` returns the effective value; `get_state.msfsEmissive` includes
  `effectiveScale`, `overridden`, nullable `overrideScale`. Scene Inspector displays the effective value in the
  existing field; editing overrides both sources, right-click → *Use source default* restores automatic selection.
  This is scene/session state, never persisted or written into the glTF.
- No reliable glTF-only signal distinguishes MSFS 2020 multiplier exports from 2024 nits exports. All ASOBO assets
  use nits with the sky, so legacy files may need an override. Core non-ASOBO glTF (including KHR emissive_strength)
  keeps its existing behavior with either environment source.

## Sun (A3.1)

- `set_environment sun true`, then `sun_dir [az, el]` (degrees; azimuth around +Y from +Z towards +X, elevation
  above the horizon), `sun_intensity` (illuminance in the HDR's units: a white Lambert surface facing the sun
  reflects colour x intensity / pi), `sun_color`, `sun_mode "hdr"` (the HDR's own sun).
- Switching the sun on or off re-filters the environment once (the HDR's sun is removed while it is on), so
  the next capture may take a moment longer. `get_state` returns `sun` (effective direction, angles,
  illuminance, and `hdr`: the detected HDR sun).
- Time of day (A6.1): `set_environment time_of_day_mode true` makes the sun follow `date` (`2026-04-15`), `time`
  (`13:09:12` local), `utc_offset` (hours, UTC = time - offset), `latitude`, `longitude` and `heading` (true degrees
  the model's nose points to; MSFS aircraft: nose on glTF +Z, left side +X, so a bearing B is at A3.1 azimuth
  heading - B). `time_preset` (`dawn`, `morning`, `noon`, `afternoon`, `dusk`, `night`) follows date and place;
  `time` turns it back into plain time; `time_playing` with `time_scale` (simulated seconds per second) runs the
  clock. `time_of_day` (`day`, `night`, `auto`) stays the ASOBO emissive switch; `auto` follows the sun elevation
  (`night_threshold`). `get_state` `timeOfDay` has the computed sun (true azimuth, elevations, fade), sunrise and
  sunset, and the moon (position, phase). Colour and intensity are the Manual sun's.
- Shadows (A3.2): on by default while the sun is on, renderer-wide keys through `set_viewport_setting`:
  `shadows`, `shadow_resolution` (1024/2048/4096), `shadow_cascades` (2-4), `shadow_distance` (scene units:
  metres for MSFS files, centimetres for some samples), `shadow_filter` (hard/pcf/soft), `shadow_split_lambda`,
  `shadow_normal_bias`, `shadow_slope_bias`, `shadow_constant_bias`. They are saved preferences: call
  `set_persist(true)` first. `get_state` `sun.shadows` lists each viewport's cascades (depth range, radius,
  texel size) and the caster counts.
- Debug views `shadowcascades` (red/green/blue/yellow per cascade, grey beyond) and `shadowvisibility` (1 lit,
  0 shadowed; `probe` reads the value directly).

## Procedural sky (A6.2 / A6.3)

- `set_environment environment sky` replaces the HDR panorama by a physically based atmosphere (Hillaire 2020 LUTs,
  Earth parameters): the background is the sky, the IBL (diffuse, specular, sheen, so reflections, SSR fallback and
  sky portals too) is filtered from it, and the sun is always on: its direction is the time-of-day sun
  (`time_of_day_mode true`) or `sun_dir`, its colour and illuminance come from the atmosphere (it reddens and dims
  towards the horizon; `sun_color`, `sun_intensity`, `sun_mode` and the HDR rotation/intensity/blur do not apply).
  `environment hdr` (the default) brings the panorama back.
- Units: 1 environment unit = 12,800 lux; the top-of-atmosphere sun is 10, about 9 at the ground at noon.
- Keys: `sky_mie` (aerosol density x, 1 = clear), `sky_ozone`, `sky_ground_albedo` (grey albedo below the horizon,
  -1 = the active viewport's A3.6 plane albedo, else 0.3); renderer-wide (through `set_viewport_setting`):
  `sky_time_slicing` (default true), `sky_budget_ms` (default 2.5).
- When the sky changes (time, sun, parameters) the background follows at once; the lighting cubes are re-filtered
  at 128 px over a few frames and swapped in when complete, then refined to the quality preset's sizes once the sky
  is still (a few seconds at High). `get_state` `sky.refine.state` is `updating`, `waiting`, `refining` or
  `converged`: before a capture or probe that must show the final lighting, repeat `capture(settle_frames=16)` until
  it is `converged` (or set `sky_time_slicing false`, which filters every change before the next frame).
- `get_state` `sky` also reports the sun transmittance and illuminance, the effective ground albedo and its source,
  and the extra GPU time per frame (`stats.gpuMsMax`, stated limit 4 ms).
- `set_camera` mirrors the requested eye offset's x and y (eye = target + (-dx, -dy, dz)); check `get_camera` when an
  exact viewing direction matters (for example probing the sky at a given direction).

## Sun disk and glare (A6.7)

- While the sun is on (HDR with `sun true`, the time-of-day clock, or the procedural sky) the background shows the
  sun's disk at the sun direction: angular radius `sun_angular_radius` x `sun_disk_scale`, mean radiance
  E / (pi sin^2 R) with E the sun illuminance (in the sky: the transmittance colour), limb-darkened
  (`sun_disk_limb_darkening`, mean unchanged). It is never in the IBL (the sun light is the directional light), so
  diffuse lighting does not change with it. Keys (environment): `sun_disk` (default true), `sun_disk_scale` (1),
  `sun_disk_limb_darkening` (true). `get_state` `sun.disk` reports `visible`, the effective `angularRadius`,
  `meanRadiance`, `centreRadiance` and `rangeScale` (the real sun exceeds the half-float scene target: the drawn
  disk is scaled by it so its brightest channel is 32000).
- `probe` reads the disk's linear radiance (with `rangeScale`). Mirror-like surfaces and windshields show the sun
  through the A3.1 specular highlight (GGX widened by the angular radius: peak E / (pi tan^2 R), the disk's size).
- Glare is the scene bloom (`bloom`, `bloom_intensity`): `bloom false` leaves the disk with a sharp edge. The lens
  effect is per viewport: `set_viewport_setting lens_flare ghosts|streak|both|off` (default off; `on` = both) and
  `lens_flare_intensity` (1); it is built from the bloom mips and only light far above white flares.

## Exposure, moon, stars, night (A6.4 / A6.5 / A6.6)

- `exposure_mode` (environment): `default` (Manual with the HDR, `auto` with the procedural sky), `manual` (the
  `exposure` EV), `auto` (metered eye adaptation; with the sky in EV100 with a key curve), `curve` (the sky's exposure
  from the time of day: sun + sky + moon + night glow illuminance). `auto_exposure true/false` = `auto` / `manual`.
- `exposure_metering` (environment, sky `auto` only): `highlight` (default) = the darker of the average meter and a
  highlight meter that keeps the brightest lit surfaces (0.3% of the image; the background sky, moon and stars do not
  count) at 1.05 exposed (0.8 before A6.4c: the A3.9 interior is darker), so lit cockpit screens are not blown out; `average` = the average meter only (the A6.4
  behaviour: dark interiors brighter, screens clipped). `get_state` `night.exposure.viewports[].meter`: `averageEv`,
  `highlightEv`, `highlightLuminance`, `surfaceFraction`, `limitedBy`. Capture runs (`--fixed-frame-time`,
  `--capture-viewport`, `--dump-frame`, `--ui-script`; pipe sessions started with them) snap Auto to the meter every frame,
  so a pipe setting and the same CLI flag give the same image; interactively it adapts (3 /s to brighter, 1.5 /s to
  darker).
  Glider EV = 13.38 - EV100 + key (+ `exposure_compensation`); `exposure_min_ev100` (-12) / `exposure_max_ev100` (18).
  With the sky in `auto`/`curve` the scene target is pre-exposed: `probe` / `probe_region` still return linear
  radiance in environment units (`get_state` `night.exposure.viewports[].preExposure`, `exposureEv`).
- `tone_mapping default` = ACES Hill with the HDR, PBR Neutral with the procedural sky.
- Moon and stars need `environment sky` and `time_of_day_mode true`. `moon` (true): disk at the A6.1 moon position
  with its phase, and the moonlight as the directional light with the shadows while it is brighter than the sun (full
  moon about 0.25 lux; `moonlight_scale`). `stars` (true), `star_density` (1): procedural stars turning with the
  sidereal time, fading in below -6 deg sun. `sky_airglow` (1): the night sky glow (part of the sky and its IBL).
  `night_vision` (0.75): dim pixels desaturate towards blue-grey (rod vision) in `auto`/`curve`.
- `get_state` `night`: exposure plan (`curveEv100`, `curveIlluminanceLux`, `preExposure`), `moon` (direction,
  `illuminatedFraction`, `illuminanceLux`, `lighting`), `stars` (`fade`, `celestialPole`, `localSiderealDegrees`),
  `directionalLight.source` (`sun` / `moon`), `emissiveNightBlend`. `sun.shadows.viewports[].light` names the light.
- `time_of_day auto` blends the ASOBO_material_emissive day/night multipliers over twilight (night weight smoothstep
  from threshold + 3 to threshold - 3 deg; `timeOfDay.dayNight.nightBlend`).
- Interiors at night: the default `auto` with `highlight` metering keeps the MSFS-scaled screens readable (A1.6b:
  3000-5000 cd/m² = 0.234375-0.390625 sky units; an unlit panel and the stars can still be near black); `exposure_metering average`
  shows the dark cockpit and the stars with the screens clipped; `curve` is the exterior's and overexposes cockpits.

## Ground (A3.6)

- `set_viewport_setting ground plane` replaces the editor grid by an infinite horizontal ground plane in the scene
  (shaded views only; it is lit by the environment and the sun, receives the sun's shadows, fades into the horizon
  and is not pickable: `pick` / `probe` see through it to the background).
- Height: `ground_height_mode` `model_base` (default: the lowest point of the model, from the static glTF pose;
  ground support equipment such as stairs counts), `zero`, or `custom` with `ground_height` (scene units).
- Surface: `ground_preset` (`concrete` with slab joints, `grass`, `sand`, `grey`; sets the next four keys),
  `ground_color` (linear albedo), `ground_roughness`, `ground_detail`, `ground_detail_scale`,
  `ground_joint_spacing`; fog: `ground_haze` (visibility distance, 0 = automatic). `ground_grid true` draws the grid
  on the plane. Editing a surface key marks the preset `custom`.
- `get_state` `ground` reports the mode, the resolved `height`, `modelBase`, and the `albedo` (the value the A6.2
  sky will read). `probe` returns the plane's linear colour like any pixel: with `sun` on and the shadows on, the
  shadow of the model on the plane is visible in it.

## Ambient occlusion (A3.5)

- `set_viewport_setting ssao true` turns on screen-space ambient occlusion (GTAO) for that viewport (off by default;
  Auto quality never changes it). It darkens only the sky light (IBL diffuse, specular, sheen, clearcoat) of OPAQUE
  and MASK surfaces, combined with the authored occlusion maps as `min(authored, SSAO)`; the sun, punctual lights,
  emissive, BLEND / transmission surfaces, the background and the A3.6 ground plane are never changed by it.
- Keys: `ssao_radius` (world units, metres in MSFS files; default 0.5), `ssao_intensity` (exponent, default 1,
  0 = no occlusion), `ssao_quality` (`low` 2 x 4, `medium` 3 x 6, `high` 4 x 8 samples per side), `ssao_resolution`
  (`full`, or `half` with a depth-aware upsample).
- Debug view `ssao` (`set_viewport_setting render_mode ssao`): the raw visibility, 1 white (open) to 0 black; `probe`
  reads it directly. `get_state` `ssao.viewports[]` reports each viewport's settings, `active` (its last render
  applied it), the AO target size, `memoryBytes` and `gpuMs` (GPU time of the AO passes).
- Radius tuning: `capture` with the debug view after each `ssao_radius` change; occluders farther than the radius do
  not count, so a cockpit-wide darkening needs a larger radius than panel creases.

## Sky portals (A3.7)

- Files with `ASOBO_sky_portal` nodes (MSFS window lights, node +Z into the cabin, radius `source_radius` cm / 100):
  the viewport key `ASOBO_sky_portal` switches their light (default on), `sky_portal_strength` scales it (0..4,
  default 0.35), `show_sky_portals` draws every portal's gizmo (UI overlay, not in `capture`).
- The light is added only where the IBL was occluded: (1 - occlusion) x strength x portal light; surfaces with
  occlusion 1 never change. `get_state` `skyPortals` lists the portals (position, direction, radius) and how many lit
  the active viewport (`active`, at most 8).
- Tuning: `capture` / `probe_region` with `ASOBO_sky_portal` false and true at the same camera; the difference is the
  portal light.

## Sun Source and Auto ambient (A3.10)

- `set_environment sun_source off|hdr|manual|clock` / `--sun-source`: HDR supports all four, procedural sky supports
  Manual and Clock. Sky colour/intensity are atmospheric. `get_state sun.source` reports requested/effective, retained
  HDR choice and valid options. Sky choices are stored separately; switching back restores HDR. Invalid sky choices
  map to Manual. From HDR is locked to the panorama even with a running clock.
- Legacy selections are applied in order: `sun on` selects stored Manual/From HDR, off selects Off; `sun_mode` updates
  that mode and selects it if the sun was enabled or Clock (mode alone keeps an off sun off). `time_of_day_mode on`
  selects Clock for both environments; off restores the legacy enabled/mode choice and sky Manual. Last selection
  wins. The clock still drives emissive, moon/stars and Curve while the sun uses another source.
  Legacy `sun` / `sun_mode` target the retained HDR choice; `sun_source` targets the current environment;
  `time_of_day_mode` selects both. An explicit sky choice survives HDR-only legacy selections.
- `ambient auto|on|off`: default Auto enables sky ambient and disables HDR ambient. `ambient_mode auto|manual`:
  unspecified = Auto with sky / Manual with HDR. Legacy `ambient on` with unspecified mode selects Manual. `ambient_fill`
  / `--ambient-fill` sets k in 0..1. Auto white radiance `L = k E_out / pi`: E_out horizontal illuminance (lux), L cd/m²;
  divide both by 12,800 for environment units. Sky uses Curve's sun+sky+moon+glow; HDR uses its cached Lambertian +Y
  sample times pi times HDR intensity plus effective horizontal sun (HDR absolute scale is conventional).
- With an active A3.9 volume, Auto is weighted by hidden diffuse sky visibility; open surfaces and the open ground
  plane get no fill. Without a volume it is uniform, because visibility is unknown. Manual keeps A3.8 exactly.
  `ambient` state/profile includes setting/mode/modeSetting/fill, outdoorIlluminance (units), outdoorIlluminanceLux,
  intensity (cd/m²), intensityEnvironmentUnits, radiance (zero while disabled) and interiorWeighted. All viewer-only,
  session settings; never part of the glTF. `validate_sun_source.py`, `validate_ambient.py`, `validate_night.py`.

## Manual ambient light (A3.8, retained by A3.10)

- A viewer light, not part of the glTF: `set_environment` keys `ambient` (default auto; see A3.10), `ambient_color` (linear
  RGB, default white) and `ambient_intensity` (luminance in cd/m^2, 0..10000, default 3). A uniform radiance
  L = colour x intensity / 12,800 (1 environment unit = 12,800 cd/m^2 with both the HDR and the procedural sky) from
  every direction, added to the IBL terms: diffuse `c_diff x L`, specular `L x (F0 A + B)` from the BRDF LUT, also
  clearcoat and sheen, then darkened by the authored occlusion and SSAO exactly like the IBL. Emissive, the background
  and the IBL filter input never see it; the A3.6 ground plane gets it too.
- `get_state` / `--profile-report` `ambient`: `enabled`, `color`, `intensity` (cd/m^2), `intensityEnvironmentUnits`,
  `radiance` (what the shaders receive, zero while off).
- Tuning: the exposure decides what is visible. At night with the procedural sky's Auto exposure the highlight meter
  is held by the MSFS-scaled screens (A1.6b: DA62 0.390625 units = 5000 cd/m²). Correcting the previous 1/1000
  scale lets Auto expose the ambient about 3.68 EV brighter in display-limited interiors, but a few cd/m² may still
  be dim beside the screens. With Manual exposure or a cockpit without bright screens the ambient is the panel
  light. Under the default HDR (exterior, Manual EV 0) 3 cd/m² changes at most 1 level.

## Sky occlusion (A3.9)

- How much sky a point sees past the model's own geometry (a cockpit, a wheel well), baked once into a volume over
  the model and applied to the sky light (IBL diffuse, specular, sheen, clearcoat) of every surface. `set_environment`
  keys: `sky_occlusion` `auto` (default: on with the procedural sky, off with the HDR, so HDR renders are unchanged),
  `on`, `off`; `sky_occlusion_quality` `low` (64 directions, up to 64 cells per axis), `medium` (default, 128 x 96),
  `high` (256 x 128); `sky_occlusion_occluders` (default true: `ASOBO_material_environment_occluder` geometry, hidden
  in the image, blocks the sky; its sky portals then bring the window light). The sun (its own shadows), punctual
  lights, emissive, Manual ambient light, the background and the ground plane are unchanged (Auto ambient uses the
  hidden-sky fraction; see A3.10). Without IBL (the Low
  quality preset) nothing is baked.
- The bake is time-sliced (8 frames at medium / high, 4 at low) and runs after a model load and after a visibility,
  transform, material, isolation, quality or occluder change, never per frame (animation is ignored). The previous
  complete volume stays in use until the new one is done; right after a load there is none (open sky). Wait until
  `get_state` `skyOcclusion.baking` is false (or capture with `settle_frames` 16) before judging an image.
- `get_state` / `--profile-report` `skyOcclusion`: `setting`, `enabled`, `active`, `quality`, `directions`,
  `baking`, `progress`, `casters`, `occluderCasters`, `volume` (`dims`, `cell` in scene units, `origin`, `max`,
  `memoryBytes`), `bake` (`completed`, `frames`, `cpuMs`, `gpuMs`, `maxFrameCpuMs`, `maxFrameGpuMs`), per-viewport
  `applied`.
- Debug view `skyvisibility` (`set_viewport_setting render_mode skyvisibility`): the cosine-weighted sky visibility,
  1 white (open) to 0 black; `probe` reads it. Walls thinner than a cell leak up to about a cell from the wall;
  `volume.cell` says how large that is.

## Pitfalls

- **Minimised window**: `capture` fails with `window_minimized`; restore the window first.
- **Settle frames**: `capture` renders `settle_frames` (default 8) frames first, like `--dump-frame`, so TAA
  and auto exposure converge. Use a larger value after big camera jumps with TAA.
- **Persistence**: pipe commands never write `settings.ini` (recent files included). Saved preferences such as
  `grid_mode` are refused with `persist_required` until `set_persist(true)`.
- **Opening a model resets the scene environment** (exposure, tone mapping, ...), as File > Open does.
- **The user shares the app**: their selection and camera changes are visible to `get_state`, and yours are
  visible to them.
