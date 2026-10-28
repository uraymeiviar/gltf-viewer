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
- No shadows yet (A3.2): the sun lights interiors through the airframe.

## Pitfalls

- **Minimised window**: `capture` fails with `window_minimized`; restore the window first.
- **Settle frames**: `capture` renders `settle_frames` (default 8) frames first, like `--dump-frame`, so TAA
  and auto exposure converge. Use a larger value after big camera jumps with TAA.
- **Persistence**: pipe commands never write `settings.ini` (recent files included). Saved preferences such as
  `grid_mode` are refused with `persist_required` until `set_persist(true)`.
- **Opening a model resets the scene environment** (exposure, tone mapping, ...), as File > Open does.
- **The user shares the app**: their selection and camera changes are visible to `get_state`, and yours are
  visible to them.
