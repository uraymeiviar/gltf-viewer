# Agent Control Channel (Live Remote Control for Agents)

Status: **Phase 0, Phase 1 and Phase 3 implemented** (Phase 0: [LIVE_SCRIPT_CONTROL.md](LIVE_SCRIPT_CONTROL.md);
Phase 1 and 3: section 7 below, including the deviations from this plan). Phase 2 is partly done (`get_camera`,
`set_camera`, `pick`, `node_screen_rect`, `set_render_mode`, `open_model`, `log_tail` exist); Phase 4 remains a proposal.

This plan lets an AI agent (Claude Code, Codex, Gemini/agy, DeepSeek or a plain Python script) operate a
**running** Glider instance — the one the user sees on screen — instead of launching a new process with
command-line switches for every experiment.

---

## 1. Why

### 1.1 The current way

Almost all agent work on Glider today runs through CLI switches:

```cmd
cmd /c "build\bin\Release\Glider.exe model.gltf --select-node Wing --debug-view roughness --capture-viewport out.png --exit-after-dump"
```

This works and must stay (the regression suite depends on it), but it has costs when an agent explores or
inspects:

- **Every question is a full process launch.** Loading a large MSFS model, filtering IBL and compiling shaders
  is paid again for each attempt, often several seconds to minutes.
- **Camera framing is trial and error.** To get a useful screenshot of a part, the agent guesses camera switches,
  launches, looks at the picture, adjusts and relaunches. Agents regularly spend many iterations on this.
- **The agent cannot ask the app anything.** It cannot ask "what node is under this pixel?", "where is this node
  on screen?" or "what is the camera doing now?". It can only infer from saved images and reports.
- **The user cannot watch or join in.** The agent's runs happen in short-lived windows; the user's own instance
  is unrelated to what the agent sees.

### 1.2 What we want

- The user launches Glider and works normally.
- An agent attaches to that instance and drives it with **intent-level commands** — select, isolate, frame a node
  from a direction, set the camera, pick at a pixel, capture — and receives results (including images) back.
- The user sees every change live and can keep interacting.
- Any agent can learn how to do this from Glider itself (`--agent-guide`, §4.5), without project-specific
  prompting.

### 1.3 Non-goals

- **Not a replacement for the regression suite.** `run_regression.py` relies on fresh processes, byte-identical
  captures and older baseline executables that will never contain this channel. It stays CLI-based.
- **Not a remote/network API.** The channel is local to the machine and the current Windows user only.
- **No editing that writes to disk.** As with the Inspector, edits stay in memory (see project rule: glTF is only
  read; extensions are only shown/hidden).

---

## 2. Design choice

Three options were considered:

| Option | Agent attaches to the user's instance | C++ cost | Verdict |
|---|---|---|---|
| A. MCP over stdio inside Glider | **No** — the MCP client launches and kills its own Glider; stray stdout output corrupts the protocol | Medium | Rejected |
| B. MCP over HTTP inside Glider | Yes | High — HTTP server, MCP protocol, `Origin` checks and spec changes all in C++ | Rejected |
| C. **Named pipe in Glider + Python MCP shim** | Yes | Low — a simple JSON command channel | **Chosen** |

```text
 Agent (Claude Code / Codex / agy / ...)
   │  MCP over stdio (agent client launches the shim)
   ▼
 scripts/glider_mcp.py  ── MCP SDK, tool schemas, PNG → image content, attach-or-launch
   │  line-delimited JSON over a named pipe
   ▼
 \\.\pipe\glider-<pid>  ── listener thread in Glider.exe (the user's instance, on screen)
   │  command queue
   ▼
 Glider main loop  ── executes commands between frames on the GL/ImGui thread
```

Why C:

- Glider stays simple: no HTTP stack, no MCP protocol code, no new third-party dependency.
- All MCP-specific code lives in Python, where it is cheap to change when the MCP spec or SDK changes.
- The same pipe serves agents that do not use MCP well, and Python harnesses directly
  (`scripts/glider_ctl.py frame_node Wing --view iso`).
- A named pipe is local by nature and can be restricted to the current user with an ACL.

---

## 3. How it works

### 3.1 Enabling the channel

The channel is **off by default**. It is enabled by:

- `--control-pipe` on the command line, or
- **File → Settings → Agent Control → Enable control pipe** (session toggle; not persisted unless the user wants it).

When enabled, Glider logs the pipe name once (`Agent control pipe: \\.\pipe\glider-12345`) and shows a small
status-bar indicator while a client is connected, so the user always knows an agent is driving the app.

When disabled, no thread is created and nothing about frame timing or rendering changes.

### 3.2 Threading model

OpenGL and ImGui may only be touched from the main thread. Therefore:

1. A **listener thread** owns the pipe. It reads one JSON request per line and pushes it onto a thread-safe
   **command queue**, together with a `std::promise` for the reply.
2. It calls `glfwPostEmptyEvent()` so an idle, throttled window wakes immediately.
3. At the start of each frame, the **main loop drains the queue** and runs each command on the GL thread.
4. **Immediate commands** (select, isolate, set camera, get state) fulfil their promise in the same frame.
5. **Multi-frame commands** (capture, open model, settle) register a pending job; the frame loop advances it each
   frame and fulfils the promise when it completes (frame rendered, model loaded, N frames settled), or with an
   error on timeout.
6. The listener thread writes the reply line back to the pipe.

Commands run strictly in order. One client at a time; a second client is refused with a clear error.

### 3.3 Wire protocol (pipe)

Line-delimited JSON (UTF-8, one object per line):

```json
→ {"id": 7, "cmd": "frame_node", "args": {"node": "Wing_L", "view": "iso", "margin": 1.2}}
← {"id": 7, "ok": true, "result": {"camera": {"eye": [..], "target": [..], "fovY": 45.0}}}
← {"id": 8, "ok": false, "error": {"code": "node_not_found", "message": "No node named 'Wing_X'", "candidates": ["Wing_L", "Wing_R"]}}
```

- Binary results (images) are returned as a file path in a session temp folder **or** base64 (`"encoding":
  "png-base64"`), selected per request.
- Errors carry a stable `code`, a human message and, where useful, hints (`candidates` for fuzzy name matches).
- Every command that changes the camera returns the resulting camera state, so a good shot can be reproduced.

### 3.4 Self-description

Glider owns the list of commands. The pipe command `describe` returns every command with its argument schema
(JSON Schema), description and whether it is immediate or multi-frame. The shim builds its MCP tools from this
reply at connect time, so **adding a command in C++ needs no shim change**, and the shim can never drift from the
executable it is talking to.

### 3.5 Python MCP shim (`scripts/glider_mcp.py`)

- Implemented without the `mcp` SDK (section 7.4): the JSON-RPC 2.0 framing over stdio and the `initialize`,
  `notifications/initialized`, `tools/list`, `tools/call`, `ping` and `resources/*` methods are written by hand
  (protocol `2025-06-18`, `2024-11-05` accepted).
- On start: finds running instances (`\\.\pipe\glider-*`). One → attach. Several → attach to the one named by
  `--pid`, or report the list. None → optionally launch `build/bin/Release/Glider.exe --control-pipe` (or a path
  given by `--exe`) and attach.
- Converts `describe` into MCP tools, forwards calls, converts PNG results into MCP image content so the agent
  sees the frame directly.
- Exposes the agent guide (§4.5) as the MCP server `instructions` and as a resource.
- Registration example:

  ```bash
  claude mcp add glider -- python H:/git-repos/gltf-viewer/scripts/glider_mcp.py
  ```

A second, dependency-free client `scripts/glider_ctl.py` speaks the pipe directly for scripts and non-MCP agents.

### 3.6 Session behaviour

- **No persistence by default.** While a client is connected, settings changes made by commands are not written to
  `settings.ini` (like `--isolated`). The user's own UI edits still save normally. `set_persist(true)` overrides.
- **`reset`** returns to a known state: reload the current model (or the default sample), default camera, clear
  selection and isolation, reset viewport settings to defaults, clear TAA/exposure history.
- **Determinism for captures.** `capture` accepts `settle_frames` (default 8, matching `--dump-frame` warm-up) and
  can request a fixed frame time and frozen auto-quality for the duration of the capture.
- **Minimised window.** Captures read the viewport FBO, not the window framebuffer, so they work while the window is
  covered. If the window is minimised (zero-size framebuffer) the command fails with `window_minimized`.
- **User and agent share the app.** The agent's selection and camera changes are visible to the user; the user's
  changes are visible to the agent through `get_state`. This is intended.

### 3.7 Security

- Pipe created with a DACL granting access to the **current user only**; remote clients rejected
  (`PIPE_REJECT_REMOTE_CLIENTS`).
- Off unless explicitly enabled; indicator visible while connected.
- No command reads or writes arbitrary files except `open_model` (read) and `capture` (write to a session temp
  folder or a path given by the client).

---

## 4. Target specification

Names are indicative; argument schemas are delivered at runtime by `describe`.
Nodes are addressed by name (exact, then case-insensitive, then unique substring) or by index (`#12`).

### 4.1 State and session

| Command | Kind | Description |
|---|---|---|
| `describe` | immediate | List commands, argument schemas and the guide version. |
| `get_state` | immediate | Model path, scene/node counts, selection, isolation, active viewport, camera, render mode, validation summary, loading state. |
| `list_nodes(filter?, limit?)` | immediate | Node names, indices, parents, mesh/material names, visibility. |
| `log_tail(lines?, level?)` | immediate | Recent logger lines. |
| `reset` | multi-frame | Return to a known state (§3.6). |
| `set_persist(enabled)` | immediate | Allow commands to save settings. |

### 4.2 Selection and visibility

| Command | Kind | Description |
|---|---|---|
| `select(node)` / `clear_selection` | immediate | Same as clicking in the hierarchy. |
| `isolate(node, with_children=false)` / `exit_isolate` | immediate | ObjectActions isolation. |
| `set_hidden(node, hidden)` | immediate | Hide/Unhide (`H`). |
| `select_parent` / `select_first_child` | immediate | `[` / `]`. |

### 4.3 Camera (the main pain point)

| Command | Kind | Description |
|---|---|---|
| `frame_node(node, view?, margin?)` | immediate | Frame a node like `F` (`CameraActor::presentFocusNode`), optionally from a preset direction (`front`, `back`, `left`, `right`, `top`, `bottom`, `iso`). Without a node, frame the whole scene (`fitCameraToScene`). |
| `set_view(view)` | immediate | Orientation preset on the current target (`orientUserCamera`). |
| `orbit(yaw_deg, pitch_deg)` | immediate | Rotate around the current target by angles (not mouse pixels). |
| `zoom(factor)` / `pan(dx, dy)` | immediate | Relative adjustments; pan in view-plane units of the current target distance. |
| `get_camera` / `set_camera(eye, target, fov_y?, projection?)` | immediate | Exact, repeatable camera; world units, glTF Y-up. |
| `look_at(node, from?, distance?)` | immediate | Point the camera at a node from a direction/point. |

### 4.4 Inspection and capture

| Command | Kind | Description |
|---|---|---|
| `capture(viewport?, width?, height?, settle_frames?, overlays?, encoding?)` | multi-frame | Render and return the viewport image (PNG). `overlays=false` hides grid, gizmo and selection box. |
| `save_viewport_image(path?, viewport?)` | immediate | Save the viewport's current image as a PNG (the same pixels as `capture` and the Viewport menu). Without `path`: a unique `Glider_<model>_<yyyyMMdd-HHmmss>.png` in the capture folder (File > Settings > Captures, default `<Pictures>\Glider`); returns the path. Never opens a dialog. |
| `copy_viewport_image(viewport?)` | immediate | Copy the viewport's current image to the Windows clipboard as `CF_DIB` plus `PNG`. Never opens a dialog. |
| `pick(x, y)` | multi-frame | Node, mesh, primitive and material at a pixel of the last capture's coordinate frame, plus world hit position (picking FBO + depth). |
| `node_screen_rect(node)` | immediate | Projected OBB rectangle in capture pixels, or `offscreen`/`hidden`/`occluded`. |
| `probe(x, y, viewport?)` | immediate | At a pixel in capture coordinates: linear HDR scene colour before TAA, bloom and tone mapping (resolved scene target), final LDR colour, depth, reconstructed world position and the picked node/mesh/primitive/material. 1x1 reads on demand; no per-frame cost, no rendering or TAA-history change. |
| `probe_region(x0, y0, x1, y1, viewport?)` | immediate | Min/max/mean linear HDR luminance and RGB over a rectangle of the same scene target. |
| `set_render_mode(mode)` | immediate | Shaded/Wireframe and Debug Render Modes (same names as `--debug-view`). |
| `set_viewport_setting(key, value, viewport?)` / `get_viewport_setting(key?)` | immediate | Phase 3: settings from Viewport Settings (AA, SSR, grid, render mode, ASOBO switches, wireframe) and the renderer-wide keys. |
| `set_environment(key, value)` / `get_environment(key?)` | immediate | Phase 3: exposure, tone mapping, HDR rotation, time of day, emissive scale, bloom, clear colour. |
| `set_material_param(material, extension, param, value)` / `get_material_param` | immediate | Phase 3: same path as `--set-asobo-param` / Inspector. |
| `list_settings(scope?)` | immediate | Every setting key with scope, type, range or values, current value, default and CLI flag. |
| `open_model(path, without_textures?)` | multi-frame | Open a model; replies when the first model frame is ready, with load timings. |
| `validate` | immediate | Validation summary and issues (same engine as the Validation panel). |

### 4.5 Agent guide (`--agent-guide`)

Any agent should be able to learn the channel from Glider alone.

- **Source:** `resources/agent/GLIDER_AGENT_GUIDE.md`, embedded in the VFS like every other resource. It is written
  in SKILL.md style: a short front-matter description, when to use the channel, how to enable and connect, a
  workflow recipe ("isolate → frame_node → capture → pick"), conventions (Y-up, units, name matching, error codes)
  and pitfalls (minimised window, settle frames, persistence).
- **Command reference generated at runtime.** The static guide is followed by a command table produced from the
  same registry that answers `describe`, so the reference can never be out of date.
- **Ways to get it:**
  - `Glider.exe --agent-guide <out.md>` writes the guide to a file and exits (reliable for a Win32 GUI executable).
  - `Glider.exe --agent-guide` with no path writes to stdout when stdout is redirected (`cmd /c "Glider.exe --agent-guide > guide.md"`),
    otherwise attaches to the parent console (`AttachConsole(ATTACH_PARENT_PROCESS)`) and prints there, then exits.
    It never opens a window.
  - Pipe command `guide` and the MCP server `instructions` / resource return the same text.
- **Optional:** `--agent-guide --format skill <dir>` writes `<dir>/glider-control/SKILL.md` (plus the command
  reference) so it can be dropped into a Claude Code or agy skills folder.

The guide also documents the existing CLI switches as the right tool for regression captures, so agents choose the
pipe for exploration and the CLI for reproducible tests.

---

## 5. Work phases

Size estimates are approximate; the existing code (`CameraActor`, `ObjectActions`, picking, `Diagnostics`
capture via `stbi_write_png`) already does most of the work behind each command.

### Phase 1 — Plumbing and the screenshot workflow (~1,000 lines)

- Named-pipe listener thread with current-user DACL, remote-client rejection, one-client policy, clean shutdown.
- Command queue, main-loop drain, `glfwPostEmptyEvent` wake, immediate and multi-frame job framework with
  timeouts.
- Command registry with JSON-schema arguments and `describe`.
- Commands: `get_state`, `list_nodes`, `select`, `clear_selection`, `isolate`, `exit_isolate`, `frame_node`,
  `set_view`, `capture`, `reset`.
- Non-persistent session behaviour and the status-bar indicator.
- `scripts/glider_mcp.py` (attach-or-launch, dynamic tools from `describe`, image content) and
  `scripts/glider_ctl.py`.
- `--agent-guide` with the embedded guide and generated command table.
- **Exit gate:** a harness script drives a live instance: isolate a node in a sample model, frame it from `iso`,
  capture, and check the node's pixels dominate the image; the channel-disabled build shows no frame-time change and
  the quick regression tier passes.

### Phase 2 — Precise camera and inspection (~500 lines)

- `get_camera`, `set_camera`, `orbit`, `zoom`, `pan`, `look_at`.
- `pick(x, y)` at arbitrary pixels (on-demand picking render when the last picking frame is stale).
- `node_screen_rect` from the OBB (`getNodeOBB`) projected through the viewport camera.
- `set_hidden`, `select_parent`, `select_first_child`, `log_tail`, `open_model`, `validate`, `set_render_mode`.
- **Exit gate:** `set_camera` → `capture` is byte-identical to the equivalent CLI capture with the same camera and
  settle frames; `pick` at the centre of `node_screen_rect(n)` returns `n` for an unoccluded node.

### Phase 3 — Shared command registry (~500 lines, mostly refactoring)

- Move runtime-safe CLI switches (`--select-node`, `--isolate-node`, `--debug-view`, `--set-asobo-param`,
  viewport/environment overrides) onto the registry so the CLI, `--ui-script` and the pipe run one code path.
- Commands: `set_viewport_setting`, `set_environment`, `set_material_param`.
- **Exit gate:** full regression tier passes unchanged (captures and selections byte-identical to baseline).

### Phase 4 — Optional: UI-level driving

- Click menus and widgets by label (ImGui item lookup + injected input), for testing the panels themselves.
- Only if needed; evaluate the Dear ImGui Test Engine licence before adopting it, otherwise a minimal menu/button
  driver.

---

## 6. Risks and open questions

- **Multi-frame replies** are the trickiest part: loading draws its own progress frames, and auto-quality may change
  settings mid-capture. The job framework must handle cancellation (model replaced, window closed) cleanly.
- **Frame-time impact** with the channel enabled but idle must be negligible (a blocked read on its own thread).
- **Concurrency with the user:** a command arriving while the user drags the camera or has a popup open. Proposal:
  commands win, and the user simply sees the result; revisit if it proves annoying.
- **Guide placement:** whether `--agent-guide --format skill` should also be installed by the release package.
- **AGENTS.md** should gain a short section pointing to this channel once Phase 1 lands.

---

## 7. Implementation (Phase 1 and Phase 3)

### 7.1 What was built

| Part | Where |
|---|---|
| Command and setting registry (names, JSON-schema arguments, help, kind, timeouts, text form, legacy aliases) | `header/imgui/CommandRegistry.h`, `source/imgui/CommandRegistry.cpp` |
| Commands, settings, readback probes, pipe pump, `--agent-guide` | `source/imgui/application/Commands.cpp` |
| Named-pipe listener (`\\.\pipe\glider-<pid>`, current-user DACL, `PIPE_REJECT_REMOTE_CLIENTS`, first-instance flag, one client, busy reply for a second client, clean shutdown, `glfwPostEmptyEvent` wake) | `header/imgui/ControlPipe.h`, `source/imgui/ControlPipe.cpp` |
| `--ui-script` / `--ui-script-follow`: every live command runs through the registry | `header/imgui/UiScenarioHarness.h` (`UiScenarioFrame::runCommand`) |
| CLI flags through the registry | `source/imgui/application/CommandLine.cpp` (`applyCliSetting`, `selectNodeAction`, `isolateNodeAction`, `frameNodeAction`, `setMaterialParamAction`) |
| File > Settings > Agent Control tab, status-bar "Agent connected" | `source/imgui/SearchPathsPanel.cpp`, `source/imgui/StatusBar.cpp` |
| Embedded guide | `resources/agent/GLIDER_AGENT_GUIDE.md` (packed into the VFS) |
| MCP server (stdio, no dependencies) and plain client | `scripts/glider_mcp.py`, `scripts/glider_ctl.py` |
| Check | `scripts/validate_agent_control.py` (registered in `run_regression.py`) |

Commands (the generated reference is in `Glider.exe --agent-guide`): `describe`, `guide`, `get_state` (`state`),
`list_nodes`, `log_tail`, `set_persist`, `quit`, `select`, `clear_selection`, `isolate`, `exit_isolate`
(`exitisolate`), `frame_node` (`frame`), `set_view` (`view`), `get_camera`, `set_camera` (`camera`), `capture`,
`node_screen_rect` (`noderect`), `pick`, `probe`, `probe_region`, `set_render_mode` (`debugview`), `open_model`
(`open`), `reset`, `set_viewport_setting`, `get_viewport_setting`, `set_environment`, `get_environment`,
`list_settings`, `set_material_param`, `get_material_param`. Names in brackets are the Phase 0 script names; they
keep their Phase 0 argument syntax and JSONL result shapes.

Settings: viewport `aa`, `msaa`, `post_aa`, `render_mode` (alias `debug_view`), `grid`, `scene_bounding_box`,
`selection_box`, `node_gizmo`, `force_double_sided`, `invert_winding`, `wireframe_overlay`, `wireframe_color`,
`wireframe_blend`, `ssr`, `ssr_quality`, `interior_sky_reflection`, `windshield_icing`, `decal_tolerance_mm`,
`asobo`, `show_invisible_materials` and one switch per rendered ASOBO extension (keyed by the extension name);
environment `exposure`, `tone_mapping`, `hdr_rotation`, `hdr_intensity`, `hdr_blur`, `time_of_day`,
`msfs_emissive_scale`, `bloom`, `bloom_intensity`, `bloom_radius`, `auto_exposure`, `exposure_compensation`,
`background`, `clear_color`, `ibl`, `punctual_lights`, the A3.1 sun keys and the A6.1 clock (`time_of_day_mode`,
`date`, `time`, `utc_offset`, `latitude`, `longitude`, `heading`, `time_preset`, `sun_refraction`,
`night_threshold`, `time_playing`, `time_scale`; `time_of_day` is the emissive day/night/auto switch), the A6.2 sky (`environment`, `sky_mie`, `sky_ozone`, `sky_ground_albedo`), the A6.7 sun disk (`sun_disk`, `sun_disk_scale`, `sun_disk_limb_darkening`), A6.4-A6.6 (`exposure_mode`, `exposure_min_ev100`, `exposure_max_ev100`, `moon`, `moonlight_scale`, `stars`, `star_density`, `sky_airglow`, `night_vision`; `tone_mapping` gained `default`); viewport A6.7 `lens_flare`, `lens_flare_intensity`; renderer `msfs_emissive`, `grid_mode` (saved preference), `sky_time_slicing`, `sky_budget_ms`.

### 7.2 Registering a setting

A setting is registered once in `Application::registerAgentSettings()` (`Commands.cpp`); the CLI flag,
`set_*`/`get_*`, `list_settings`, the scripts and the guide table all read the same entry:

```cpp
// A field of ViewportSettingsState (scope Viewport) or SceneEnvironmentState (scope Environment):
addSetting(floatSetting<VS>("sun_elevation", V, &VS::sunElevation, "Sun elevation in degrees.",
                            "--sun-elevation <deg>", std::pair{-90.0, 90.0}));
addSetting(boolSetting<ES>("shadows", E, &ES::shadowsEnabled, "Sun shadows.", "--shadows <on|off>"));
addSetting(indexEnumSetting<VS>("ao_quality", V, &VS::aoQuality, {"low", "medium", "high"}, "AO samples."));
// Anything else: a SettingSpec with get/set lambdas (see "render_mode", "wireframe_color", "grid_mode").
```

and, for a CLI flag, one line in `configure()`:

```cpp
} else if (arg == "--sun-elevation" && i + 1 < argc) {
    if (!applyCliSetting(SettingScope::Viewport, "sun_elevation", argv[++i], "--sun-elevation")) return 1;
```

The default is read from a default-constructed struct, range and enum checks come from the entry, text values
(CLI, scripts) are parsed with `std::stof` exactly as the CLI always has, and the setter only writes state:
`set_viewport_setting` marks `ViewportSettings` and resets that viewport's TAA history, `set_environment` marks
`RenderSettings`. Mark `persisted = true` for a value stored in `settings.ini`; the pipe then requires
`set_persist(true)`.

### 7.3 Deviations from this plan (and why)

- **MCP SDK not used** (§3.5): the `mcp` package is not installed and must not be. `glider_mcp.py` implements the
  stdio JSON-RPC framing and the five required methods (plus `resources/list`/`read` for the guide) by hand;
  stdout carries only protocol messages.
- **Drain point**: pipe requests are drained in `finishFrame` (after the frame is rendered, before the swap), right
  before the UI script runs, not at the start of the frame. Both run between frames on the GL thread; this keeps
  pipe, `--ui-script` and Phase 0 timing identical (a job dispatched in frame N first advances in frame N+1, so
  `capture` with 8 settle frames reads frame N+8 on every path).
- **`reset` restores the startup state**, not the built-in defaults: viewport settings and the scene environment
  as the startup flags left them (e.g. a harness's `--aa none`), the model reloaded and the camera fitted.
- **Persistence**: pipe commands never write `settings.ini`. Instead of a per-session store, recent files are not
  recorded for loads a pipe command starts, and saved preferences (`grid_mode`) are refused with
  `persist_required` until `set_persist(true)`; with it they are saved like a UI edit. Viewport and environment
  settings are not persisted by Glider, so no other guard is needed.
- **`capture`** reads the viewport texture at its current size; `width`/`height` are not implemented (the agent
  resizes the window or uses `get_state` for the size). `overlays=false` hides the grid, scene box and selection
  box for that capture only. The capture path defaults to `%TEMP%/glider-agent/<pid>/capture_<n>.png`.
- **Loads are queued**: inside a frame `loadGltfModel` queues the file for the next frame (as File > Open does), so
  `open_model` and `reset` wait for the queued load, then settle 4 frames (the Phase 0 `open` wait).
- **One registry, one parser**: script lines use the registry's positional text form (`set_viewport_setting ssr
  off`); the Phase 0 names keep their own parsers (`camera` with comma vectors, `isolate <node> children`).
- **Phase 2 commands** `get_camera`, `set_camera`, `pick`, `node_screen_rect`, `set_render_mode`, `open_model`,
  `log_tail` exist because Phase 0 already had them; `orbit`, `zoom`, `pan`, `look_at`, `set_hidden`,
  `select_parent`, `select_first_child` and `validate` do not yet.
- **`--agent-guide`** also accepts `--format skill <dir>` (writes `<dir>/glider-control/SKILL.md`).
- **Startup-only flags** (`--env`, IBL sizes, quality presets, `--grid-mode`, window and isolation flags) keep their
  own parsing, as allowed; `--grid-mode` and the registry's `grid_mode` write the same field.

### 7.4 Verification

`scripts/validate_agent_control.py [--baseline-exe <baseline>]`: the Phase 1 exit gate (isolate, frame from iso,
capture, the node dominates the image), pipe = `--ui-script` = CLI captures (byte-identical), `set_*` against the
matching CLI flags (byte-identical: SSR off, `--asobo-disable`, exposure, a material parameter, wireframe, tone
mapping/time of day/emissive scale, debug view) and `get_*`, `probe` on an unlit quad and on the `--clear-color`
background (linear value within 2e-3; the next capture is unchanged), no `settings.ini` change from pipe commands,
a refused second client, the MCP stdio session (initialize, tools/list, a capture returning image content), the
guide, zero GL errors, and with `--baseline-exe` CLI captures using every flag now routed through the registry
against the baseline binary.
