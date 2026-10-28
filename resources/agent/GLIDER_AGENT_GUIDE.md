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

## Pitfalls

- **Minimised window**: `capture` fails with `window_minimized`; restore the window first.
- **Settle frames**: `capture` renders `settle_frames` (default 8) frames first, like `--dump-frame`, so TAA
  and auto exposure converge. Use a larger value after big camera jumps with TAA.
- **Persistence**: pipe commands never write `settings.ini` (recent files included). Saved preferences such as
  `grid_mode` are refused with `persist_required` until `set_persist(true)`.
- **Opening a model resets the scene environment** (exposure, tone mapping, ...), as File > Open does.
- **The user shares the app**: their selection and camera changes are visible to `get_state`, and yours are
  visible to them.
