# Live Agent Script Control & Automated Camera Framing

Glider provides a live bidirectional command channel and automated camera framing designed for autonomous visual inspection agents and automated regression testing.

It enables agents to:
1. Automatically frame any scene node from preset view angles with configurable margins directly from the command line or via live commands.
2. Query exact on-screen projected bounding boxes (`screenRect`) and perform GPU-accurate pixel picking.
3. Drive an interactive Glider session in real time by watching a command script file (`--ui-script-follow`) and receiving structured JSONL output responses.

---

## 1. The Agent Visual Inspection Loop

Autonomous agents interact with Glider in an iterative perception-action loop:

```
[Start Session]
      │
      ▼
[Frame Node] ────► [Capture Viewport] ────► [Read Image / Perception]
      ▲                                                │
      │                                                ▼
      └─────────── [Adjust Camera / Debug View] ◄── [Pick / NodeRect]
```

### The Recipe:
1. **Start**: Launch Glider with `--ui-script-follow <session_file>` (automatically sets `GLIDER_NO_FOCUS=1` and enforces non-persistent isolated mode).
2. **Frame**: Orient the camera toward a target node using `frame <node> [view] [margin]`.
3. **Capture**: Take a viewport-only snapshot with `capture <path.png> [settle_frames]`.
4. **Read Image**: Analyze image artifacts, verify geometry, shaders, or decals.
5. **Pick / Locate**: Query projected 2D screen coordinates with `noderect <node>` or inspect 3D objects at specific pixels with `pick <x> <y>`.
6. **Adjust**: Switch render passes (`debugview <name>`), isolate parts (`isolate <node>`), or finely position the camera (`camera <eye> <target> [fov]`).
7. **Quit**: Terminate the session cleanly with `quit`.

---

## 2. CLI Flags Reference

### Automated Framing
- `--frame-node <name|#index>`: Frames the target node at startup using the same bounding sphere and distance logic as **Object → Focus (`F`)**. Node lookup supports exact matching, case-insensitive matching, unique substring matching, or `#<index>` by node index. If a node has no geometry bounds (e.g. empty locator nodes or light roots), Glider frames its world pivot position at a default distance without error. Selects the node.
- `--frame-view <front|back|left|right|top|bottom|iso>`: Sets camera orientation to a preset direction relative to the target node. If omitted, preserves the camera's current viewing direction.
- `--frame-margin <float>`: Scales camera framing distance (default: `1.0`, matching `F`; `2.0` frames twice as far away, reducing on-screen size).

### Spatial & Picking Diagnostics
- `--report-node <name|#index>` (repeatable): Populates a `nodes` array in `--profile-report <out.json>` containing:
  - `name`: Node name.
  - `index`: glTF node index.
  - `visible`: Whether node and its parents are visible.
  - `screenRect`: Projected oriented bounding box (OBB) as `[x0, y0, x1, y1]` in capture-image coordinates (origin top-left).
  - `onScreen`, `clipped`, `behindCamera`: Visibility classification flags.
  - `center`: World-space OBB center `[x, y, z]`.
- `--report-pick <x,y>` (repeatable): Populates a `picks` array in `--profile-report <out.json>` by performing an offscreen GPU color pick at coordinates `(x, y)`:
  - Returns `node` (index, name), `mesh`, `primitive`, and `material`, or `null` if background.
- Profile report `camera` object: Whenever `--profile-report` is emitted, it includes a `camera` object detailing `eye`, `target`, `up`, `fovY`, `near`, `far`, `projection`, `viewport`, and `cliArgs` (exact CLI string to reproduce the view).

### Live Follow Mode
- `--ui-script-follow <file>`: Enables live file-watching follow mode. Glider monitors `<file>` for appended commands, executes them between frames on the main thread, and writes JSON results to `<file>.out.jsonl`.
  - When idle, wakes up at least every 250 ms (`glfwWaitEventsTimeout(0.25)`).
  - Shows status-bar badge `Agent script: <file>`.
  - Persistence is disabled (equivalent to `--isolated`); no settings are written to disk.

---

## 3. Live Script Command Protocol

Commands are appended as single lines to the session file. Responses are written to `<file>.out.jsonl` formatted as:

```json
{"line": 1, "cmd": "frame Cockpit front 1.2", "ok": true, "result": "framed node 'Cockpit' (index 12)"}
```

If a command fails or is unrecognized, `ok` is `false`, `error` details the issue, and the session continues running without crashing or exiting.

| Command | Arguments | Description | Result Format |
|---|---|---|---|
| `frame` | `<node> [view] [margin]` | Focuses on `<node>` (`name` or `#index`). Optional `view` preset (`front`, `back`, `left`, `right`, `top`, `bottom`, `iso`). Optional distance `margin` (float). | String confirming framed node name & index. |
| `select` | `<node>` | Selects `<node>` by name or `#index`. | String confirming selected node. |
| `isolate` | `<node> [children]` | Isolates `<node>`. If second argument is `children`, `childs`, `true`, or `1`, isolates node and its subtree. | String confirming isolation. |
| `exitisolate` | *(none)* | Clears isolation filter and restores full scene visibility. | String confirming exit. |
| `view` | `<preset>` | Reorients camera to preset (`front`, `back`, `left`, `right`, `top`, `bottom`, `iso`). | String confirming view orientation. |
| `camera` | `<eye x,y,z> <target x,y,z> [fov] [near] [far]` | Sets exact camera eye and target coordinates, and optional FOV / clipping planes. | Object containing new camera parameters. |
| `capture` | `<png_path> [settle_frames]` | Captures viewport to PNG after settling (default 8 frames). | Object with `path` and full `camera` details. |
| `noderect` | `<node>` | Computes 2D projected OBB screen rect in capture image pixels. | Object with `index`, `name`, `visible`, `screenRect`, `onScreen`, `clipped`, `behindCamera`, `center`. |
| `pick` | `<x> <y>` | GPU color picking at pixel coordinate `(x, y)` in capture image space. | Object with `node`, `mesh`, `primitive`, `material` (or `null` for background). |
| `debugview` | `<name>` | Changes render mode to debug view (e.g. `roughness`, `occlusion`, `shadingnormal`, `ssr`, or `shaded`/`off`). | String confirming debug view mode. |
| `state` | *(none)* | Returns viewer state summary. | Object with `modelPath`, `selection`, `isolation`, `camera`, `nodeCount`, `materialCount`. |
| `open` | `<path>` | Opens a new glTF/GLB asset; waits until the first model frame is rendered. | String confirming opened model path. |
| `quit` | *(none)* | Cleanly closes Glider with exit code 0. | String confirming exit. |

### 3.1 Commands come from the shared registry

Since Phase 3 of [AGENT_CONTROL_PLAN.md](AGENT_CONTROL_PLAN.md) these commands are not implemented in the script
harness any more: `--ui-script`, `--ui-script-follow` and the control pipe (`--control-pipe`) all dispatch through
one command registry (`header/imgui/CommandRegistry.h`, `source/imgui/application/Commands.cpp`). The names in the
table above are aliases of the registry commands (`frame` = `frame_node`, `noderect` = `node_screen_rect`,
`debugview` = `set_render_mode`, `state` = `get_state`, `open` = `open_model`, `view` = `set_view`, `camera` =
`set_camera`, `exitisolate` = `exit_isolate`); they keep the argument syntax and the JSONL result shapes shown here.

Every other registry command can be used in a script with its own name and positional arguments, e.g.

```text
set_viewport_setting ssr off
set_environment exposure 1.5
set_material_param Glass ASOBO_material_fresnel_fade fresnelOpacityOffset 0.5
probe 400 300
list_settings environment
```

Their results are the registry results (the same objects the pipe returns). `Glider.exe --agent-guide <file>`
writes the full, generated command and settings reference. The UI-testing commands (`click`, `drag`, `snap`,
`vpsetting`, `aamode`, `asoboparam`, ...) stay in the harness. `key <name> [mods]` accepts combined modifiers
(`key space ctrl+alt`) and the digit keys (`key 7 ctrl`). A `name=value` token sets any argument of a registry command by
name (`image_viewer 0 grid=on major=32`); the other tokens fill the positionals in order. This does not apply to commands
whose last positional takes the rest of the line (`open`). A command that runs over several frames (`capture`,
`open`, `reset`) holds back the following lines until it has replied, as before.

### 3.2 Image Viewer (`image_viewer`)

`image_viewer <image> [zoom] [channels] [levels] [background] [raw] [x,y] [window_pos] [window_size]` opens (or focuses) the Image Viewer
window of glTF image `<image>` exactly like clicking its Inspector preview (ARCHITECTURE.md §3.5b) and sets its
view. `-` keeps a positional value unchanged:

- `zoom`: `fit` or a percentage (`200`, `137.5`; 1..3200).
- `channels`: the shown channels as letters (`rgba`, `rgb`, `a`, `l`, `rg`, `all`).
- `levels`: `full`, `data` (min-max of the shown channels) or `low,high` in stored values (`10,200`, `0.2,0.8`).
- `background`: `checker`, `white`, `gray`, `black`.
- `raw`: `on` shows the stored values of a decoded normal map (BC5), `off` the decoded view.
- `x,y`: the readout pixel (row 0 at the top); the reply carries its stored values.
- `window_pos`: `x,y` of the window's top-left corner relative to the main window's client area. With multi-viewport
  on (`--viewports on`; off by default under `--ui-script` / `--ui-script-follow`, ARCHITECTURE.md §3.6) a position
  outside the main window detaches the viewer into its own OS window; a position inside merges it back.
- `window_size`: `w,h` of the window (at least `100,100`).

Crosshair and pixel grid (ARCHITECTURE.md §3.5b), as `name=value` tokens. They are preferences shared by every viewer: kept
for the session, and written to `settings.ini` `[ImageViewer]` only by a pipe client after `set_persist(true)`:

- `crosshair=on|off`, `grid=on|off`.
- `major=<px>`, `minor=<px>`: grid spacing in image pixels (1..16384; defaults 256 / 64).
- `major_color=…`, `minor_color=…`: `r,g,b,a` in 0..1 or `#rrggbb[aa]` (defaults `1,1,1,0.5` / `1,1,1,0.25`).
- `grid_defaults=true`: both levels back to their defaults.
- `cursor=x,y`: the image pixel of the crosshair while the mouse is not over the image (the mouse wins while it is);
  `cursor=off` removes it. The harness `move x y` hovers for real.

```text
image_viewer 0 800 grid=on major=32 minor=8 crosshair=on cursor=100,100
image_viewer_wait 0
shot build/iv_grid.png
```

The image loads in the background (ARCHITECTURE.md §3.5b): `image_viewer` and `uv_layout` reply at once, often
before the full-resolution pixels are there. `image_viewer_wait <image>` (multi-frame, timeout 120 s) replies when the
window is `ready` (pixels, texture, histogram, UV overlay) with the same state; scripts wait with it instead of fixed
`wait` frames. Options that need the pixels (`channels`, `levels`, `raw`, `x,y`) are kept and applied when the pixels
arrive (`levels data` when the data range does), so a reply sent while loading still shows the old values for them:

```text
image_viewer 0 - - - - - 1234,567
image_viewer_wait 0
```

The reply is the viewer state: `key`, `title`, `width`, `height`, `channels`, `shown`, `type` (`uint8`, `int8`,
`uint16`, `float32`), `storedBits`, `source`, `decodedView`, `raw`, `fit`, `zoomPercent`, `levels`, `fullRange`,
`dataRange` per channel, `background`, `area` (the image area's screen rectangle of the last frame), `readout`
`{x, y, values}`, `warnings`, `window` (the window's screen rectangle), `detached` (in its own OS window) and
`splitFraction` (the picker/histogram split), `crosshair` (`on`, `drawn` in the last frame, `source` `mouse` / `script` /
`none`, `pixel`, `screen`: the column and row it is drawn at) and `grid` (`on`, `minScreenSpacing`, and per `major` /
`minor`: `spacing`, `color`, `screenSpacing`, `shown`, `lines` drawn in the last frame). Load state: `loading` (the phase: `reading`, `decoding`, `uploading`,
`histogram`, `uv`, `ready`), `ready`, `pixelsReady`, `histogramReady`, `textureComplete`, `preview` (a preview texture
is shown until the texture is complete) and `timings` (`copyMs`, `decodeMs`, `convertMs`, `variantMs`, `histogramMs`,
`allocMs`, `uploadMs`, `uploadFrames`, `mipmapMs`, `sharpMs`, `readyMs`, `maxViewerMs`: the viewer's longest main-thread
frame during the load, `maxFrameMs`: the longest frame). Until `pixelsReady`, `width` / `height` come from the image
header and the readout, data range and histogram are not final. Pipe clients pass the same names as JSON (`zoom`, `levels` and `raw` accept a
string or a number / boolean; `pixel` is `[x, y]`; `close: true` closes the window, `signed_square` overrides the
packaged normal-map decode; `window_pos` / `window_size` also accept `[x, y]`). Whole-window screenshots: the
harness's `shot <png>` (the main window's framebuffer including every ImGui window in it, never a detached one),
e.g.

```text
image_viewer 0 200 rgb - checker - 12,8
wait 4
shot build/iv_rgba_200.png
```

`capture_window <path> <window> [settle_frames=3]` saves one visible ImGui window as PNG; `<window>` is a
case-insensitive substring of its title (one word: script arguments split at spaces, e.g. the image file name).
A window in the main window is cropped from the main framebuffer; a detached window is read from its own back
buffer. The reply carries `path`, `window` (the full ImGui name), `detached`, `width` and `height`:

```text
image_viewer 0 fit - - - - 30,20 1700,100 900,650
wait 12
capture_window build/iv_detached.png rgba8.png
```

`uv_layout <material> [slot] [wrap] [fill] [transform] [show]` opens the UV layout overlay of a material's texture
slot exactly like the UV layout button of the Material Inspector (ARCHITECTURE.md §3.5b): the slot image's viewer
with the triangle edges of every primitive using the material in the slot's TEXCOORD set.

- `slot`: `baseColor` (default), `metallicRoughness`, `normal`, `occlusion`, `emissive` (a `Texture` suffix is
  accepted, case-insensitive). Other slots are opened from the Inspector.
- `wrap`, `fill`, `transform`, `show`: `on`, `off` or `-` (keep). `transform off` shows the raw UVs; without a
  KHR_texture_transform on the slot it has no effect. Pipe clients may also pass `opacity` (0.05..1).

The reply is the viewer state with `imageRect` (the image's screen rectangle of the last frame: a UV point `u,v` is at
`imageRect.x0 + u * width, imageRect.y0 + v * height`) and `uvLayout`: `material`, `materialName`, `slot`,
`texCoord`, `hasTransform`, `transformApplied`, `transform` (column-major 3x3), `show`, `wrap`, `fill`, `opacity`,
`primitives`, `triangles`, `outside` (triangles partly outside 0..1), `edges`, `fillTriangles`, `skippedPrimitives`
(points / lines), `missingUvPrimitives`, `unreadablePrimitives`, `capped`, `gatherMs`, `buildMs`, `bufferUploadMs`,
`building` (an edge build runs on a worker; the previous edges stay drawn and counted until it is adopted),
`buildLongestViewerFrameMs` and `buildLongestFrameMs` (during the last build). Opening the same
image with `image_viewer` keeps the overlay; another material or TEXCOORD set replaces it.

```text
uv_layout 1 baseColor - - off
image_viewer 0 200
image_viewer_wait 0
shot build/uv_raw.png
```

---

## 4. Python Client: `scripts/glider_live.py`

A lightweight Python client [`scripts/glider_live.py`](file:///H:/git-repos/gltf-viewer/scripts/glider_live.py) provides both a command-line interface and a programmatic Python class.

### 4.1 CLI Usage

```bash
# Start Glider with a session file
python scripts/glider_live.py start resources/sampleGltf/HelloCube.glb --session build/live.cmd

# Send commands
python scripts/glider_live.py send "frame Cube front 1.5" --session build/live.cmd
python scripts/glider_live.py send "capture build/cube.png" --session build/live.cmd
python scripts/glider_live.py send "noderect Cube" --session build/live.cmd
python scripts/glider_live.py send "pick 400 300" --session build/live.cmd

# Quit the session
python scripts/glider_live.py quit --session build/live.cmd
```

### 4.2 Programmatic Usage (`GliderLive`)

```python
from pathlib import Path
from glider_live import GliderLive

session_file = Path("build/live_session.cmd")
model_path = Path("resources/test_models/hierarchy_animation_fixture.gltf")

with GliderLive(model_path, session_file=session_file) as live:
    # Frame a specific node from the front
    res = live.frame("GrandChild", view="front", margin=1.0)
    print("Framed:", res)

    # Capture viewport image
    shot = live.capture("build/grandchild_front.png", settle_frames=8)
    print("Captured to:", shot["result"]["path"])

    # Query 2D screen bounding box
    rect = live.noderect("GrandChild")
    screen_rect = rect["result"]["screenRect"]  # [x0, y0, x1, y1]
    cx = int((screen_rect[0] + screen_rect[2]) / 2.0)
    cy = int((screen_rect[1] + screen_rect[3]) / 2.0)

    # Pick at center of the node
    pick = live.pick(cx, cy)
    print("Picked object:", pick["result"]["node"]["name"])

    # Inspect material roughness pass
    live.debugview("roughness")
    live.capture("build/grandchild_roughness.png")

    # Clean exit
    live.quit()
```

---

## 5. Verification & Tests

To verify live agent control and CLI framing functionality:

```bash
python scripts/validate_live_control.py
```

This automated suite exercises:
- CLI node framing across preset orientations, margins, `#index` resolution, and locator nodes without geometry.
- Live follow mode commands, responses, and resilience against invalid commands.
- 100% bit-identical byte-level reproducibility between CLI framing and live `camera` commands.
- Exit gate regression testing (`validate_p4_exit_gate.py`).
- Real-world MSFS asset testing on the DA62 Cockpit (`WINDSHIELD_Static` and `LIGHTS_Asobo_Unsafe`).
