# Agent Control Channel (Live Remote Control for Agents)

Status: **Phase 0 implemented** (see [LIVE_SCRIPT_CONTROL.md](LIVE_SCRIPT_CONTROL.md) for live file-watching follow mode and CLI framing). Higher phases (named pipe, sockets, web UI) remain proposals.

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

- Uses the official `mcp` Python SDK, stdio transport.
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
| `pick(x, y)` | multi-frame | Node, mesh, primitive and material at a pixel of the last capture's coordinate frame, plus world hit position (picking FBO + depth). |
| `node_screen_rect(node)` | immediate | Projected OBB rectangle in capture pixels, or `offscreen`/`hidden`/`occluded`. |
| `set_render_mode(mode)` | immediate | Shaded/Wireframe and Debug Render Modes (same names as `--debug-view`). |
| `set_viewport_setting(key, value)` | immediate | Phase 3: settings from Viewport Settings (AA, SSR, grid, ASOBO switches, wireframe). |
| `set_environment(key, value)` | immediate | Phase 3: exposure, tone mapping, HDR rotation, time of day, emissive scale. |
| `set_material_param(material, extension, param, value)` | immediate | Phase 3: same path as `--set-asobo-param` / Inspector. |
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
