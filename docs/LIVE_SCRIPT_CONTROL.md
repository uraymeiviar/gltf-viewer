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
`vpsetting`, `aamode`, `asoboparam`, ...) stay in the harness. A command that runs over several frames (`capture`,
`open`, `reset`) holds back the following lines until it has replied, as before.

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
