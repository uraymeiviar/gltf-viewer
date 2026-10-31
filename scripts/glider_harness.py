# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Shared pieces of the Glider regression harnesses (docs/BUILD_AND_TEST.md section 4.5).

Every launch goes through scripts/harness_process.py, so children get GLIDER_NO_FOCUS=1 and
SW_SHOWNOACTIVATE, and baseline launches use its render cache. Import it from a script in
scripts/ with ``import glider_harness`` (or ``from glider_harness import ...``).

  REPO, EXE, BASELINE_DIR       repository root, current build, saved baselines
  head_baseline()               build/baselines/<HEAD short sha>.exe (may not exist)
  latest_baseline()             newest build/baselines/*.exe by modification time, or None
  baseline_exe(value)           a --baseline-exe value resolved against REPO, else latest_baseline()
  run(args, timeout, env)       harness_process.run(args) in REPO, text output captured
  capture(exe, model, png, ...) one isolated viewport capture + profile report; (png|None, report|None, log)
  start_pipe_session(args, env=None)  Popen with --control-pipe already in args; waits for and connects the pipe
  connect_pipe(process)         connects to a started Glider's pipe, retrying while it lives (no 10 s give-up)
  gl_errors_zero(log)           the "GL automation errors: 0" line is present and no "GL error 0x" was logged
  read_json(path)               parsed JSON, or None when the file is missing or unreadable
  digest(path), same(a, b)      SHA-256 of a file; byte equality of two image files (None never equal)
  viewport_sha(path)            SHA-256 of a window screenshot's viewport area (side panels, menu, status bar cut)
  relative_timing(cand, base)   measured times of interleaved runs (A B A B, harness_process.uncached()): the
                                best candidate value <= TIMING_FACTOR x the best baseline value + TIMING_SLACK_MS
  capture_run(argv, ...)        one viewport capture given as its CLI argv: a CLI launch, or for the candidate a
                                capture in a shared pipe session (see "Capture sessions" below)

Capture sessions
----------------
capture_run() takes the argv of an isolated `--capture-viewport` launch exactly as a harness builds it. For a
baseline (or any non-candidate) exe, and whenever the argv holds a flag outside SESSION_STARTUP_FLAGS /
SESSION_VARIANT_FLAGS / SESSION_OUTPUT_FLAGS, it is that CLI launch (cacheable, memoized as before). Otherwise
the capture runs in a pipe session of the candidate started with the same startup flags (one session per
distinct startup-flag list, one Glider at a time; it is started at the second consecutive capture with the same
startup flags, a lone capture stays a CLI launch): `open_model <model> startup_state=true` gives the state a
fresh start with that model and those flags gives (reset's startup settings plus the replayed --select-node /
--camera-* flags), the variant flags go through the registry (`--debug-view` = set_render_mode,
`--asobo-disable X` / `--asobo off` = set_viewport_setting, `--set-asobo-param` = set_material_param,
`--without-textures` = open_model's without_textures, `--windshield-icing` = set_environment), then
`capture` settles `frame - 4` frames (open_model already rendered 4). The result's stdout is the line
"GL automation errors: N" with the session's GL error count of that capture; no profile report is written
(callers that read the report, the log or anything else of the launch pass session=False).
  GLIDER_HARNESS_SESSIONS=0          every capture is its CLI launch (the old behaviour)
  GLIDER_HARNESS_SESSION_PROOF=<f>   when the harness exits, every session capture's CLI argv is also launched
                                     (fresh, into build/session_proof/) and both images are hashed; one JSON
                                     line per capture is appended to <f> and a summary INFO line is printed
An open session is closed before any other Glider process starts (harness_process.add_launch_hook) and when
the harness exits.
"""

import hashlib
import json
import os
import time
from pathlib import Path

import harness_process

REPO = Path(__file__).resolve().parents[1]
EXE = harness_process.candidate_exe()
BASELINE_DIR = REPO / "build" / "baselines"


def head_baseline():
    """build/baselines/<HEAD short sha>.exe; the file may not exist."""
    result = harness_process.run(["git", "rev-parse", "--short", "HEAD"], cwd=REPO, capture_output=True, text=True)
    return BASELINE_DIR / f"{result.stdout.strip()}.exe"


def latest_baseline():
    """The most recently written build/baselines/*.exe, or None."""
    candidates = sorted(BASELINE_DIR.glob("*.exe"), key=lambda path: path.stat().st_mtime)
    return candidates[-1] if candidates else None


def baseline_exe(value=None):
    """A --baseline-exe argument (relative to REPO when not absolute), or latest_baseline() without one."""
    if value is None:
        return latest_baseline()
    path = Path(value)
    return path if path.is_absolute() else REPO / path


def run(args, timeout=600, env=None):
    """Runs a command in REPO with text stdout/stderr captured (through harness_process)."""
    kwargs = {"cwd": REPO, "capture_output": True, "text": True, "timeout": timeout}
    if env is not None:
        kwargs["env"] = env
    return harness_process.run([str(arg) for arg in args], **kwargs)


def capture(exe, model, png, flags=(), frame=24, report=None, timeout=600):
    """Isolated viewport capture at `frame` with a profile report; `flags` must include --isolated.

    Returns (png or None, report dict or None, stdout + stderr)."""
    png = Path(png)
    report = Path(report) if report is not None else png.with_suffix(".json")
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    args = [exe] + ([model] if model else []) + list(flags)
    result = run(args + ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report,
                         "--exit-after-profile"], timeout=timeout)
    return (png if png.exists() else None), read_json(report), result.stdout + result.stderr


def start_pipe_session(args, timeout=120, env=None):
    """Starts Glider (args include --control-pipe) and returns (process, glider_ctl.GliderPipe).

    Waits up to `timeout` seconds for the pipe to appear and accept a client; the pipe can exist a
    moment before it does. Raises RuntimeError when Glider exits or never opens it, and the
    connection error when the pipe never accepts (the process is killed then)."""
    import glider_ctl
    extra = {} if env is None else {"env": env}
    process = harness_process.Popen([str(arg) for arg in args], cwd=REPO, stdin=harness_process.DEVNULL,
                                    stdout=harness_process.DEVNULL, stderr=harness_process.DEVNULL, **extra)
    deadline = time.time() + timeout
    while not os.path.exists(glider_ctl.pipe_name(process.pid)):
        if process.poll() is not None or time.time() > deadline:
            raise RuntimeError(f"Glider did not open its pipe (exit {process.poll()})")
        time.sleep(0.1)
    return process, connect_pipe(process, max(deadline - time.time(), 1.0))


def connect_pipe(process, timeout=120):
    """Connects to the control pipe of a started Glider, retrying while the process lives.

    GliderPipe alone gives up after 10 s, but a busy startup (CPU-limited runs, model load, IBL
    filtering) can keep the pipe refusing clients longer: that was the connect_failed flake. Kills
    the process and re-raises when it exits or `timeout` seconds pass."""
    import glider_ctl
    deadline = time.time() + timeout
    while True:
        try:
            return glider_ctl.GliderPipe(process.pid)
        except glider_ctl.GliderError:
            if process.poll() is not None or time.time() > deadline:
                process.kill()
                raise
            time.sleep(0.2)


def gl_errors_zero(log):
    lines = [line for line in log.splitlines() if line.startswith("GL automation errors")]
    return lines == ["GL automation errors: 0"] and "GL error 0x" not in log


# Measured GPU / CPU times on the shared, loaded machine: compared with the baseline binary measured in the same
# conditions (interleaved A B A B, uncached), not with an absolute limit (those run under each harness's --bench).
TIMING_FACTOR = 1.25
TIMING_SLACK_MS = 0.5


def relative_timing(candidate, baseline, factor=TIMING_FACTOR, slack_ms=TIMING_SLACK_MS):
    """(ok, best candidate, best baseline, limit) for the values of interleaved candidate / baseline runs.

    The best (smallest) value of each binary is compared: background load only ever adds time, so the minimum of
    two runs filters it, while a regression in the candidate raises both of its runs. None values are ignored;
    without a value on either side the check fails."""
    cand = [float(v) for v in candidate if v is not None]
    base = [float(v) for v in baseline if v is not None]
    if not cand or not base:
        return False, (min(cand) if cand else None), (min(base) if base else None), None
    limit = factor * min(base) + slack_ms
    return min(cand) <= limit, min(cand), min(base), limit


def read_json(path):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest() if path else None


def same(a, b):
    return a is not None and b is not None and digest(a) == digest(b)


# Window-screenshot rows above the viewports' 3D image: menu bar and dock tab bar end at y = 52, then the
# viewport toolbar row (y 52..69 with small buttons only; taller when it holds a combo, e.g. material variants).
VIEWPORT_TOOLBAR_TOP = 52
# Default dark theme (Theme.cpp): WindowBg behind panels and toolbar buttons, and the dock splitter colour.
UI_BACKGROUND = (36, 36, 41)
UI_SPLITTER = (22, 22, 25)


def viewport_3d_box(image):
    """(left, top, right, bottom) of the viewports' 3D image in a window screenshot.

    The default dock layout (Menus.cpp) gives the left panels 22% of the width and the Inspector 26% of the
    rest, so the viewports lie within about 22-79.7% of the width; after a runtime resize the panels keep their
    pixel widths, so the 3D columns are then found exactly: on the last compared row (above the status bar)
    they are the runs of at least 64 px that are neither panel background nor splitter. Side-by-side splits
    (vpsplit) give one run per viewport; the box spans them (the 1-2 px splitter between them is constant UI).
    The top is the first row below the toolbar in which under 2% of those 3D columns show the background: every
    toolbar row shows it between and beside its buttons and combos (over 15% of the row in practice), a 3D row
    never. A row too low only shortens the compared area; it can never make two different images equal."""
    width, height = image.size
    bottom = height - 40  # the status bar (where hover tooltips also appear) is left out
    left0, right0 = int(width * 0.22) + 6, int(width * (1 - 0.78 * 0.26)) - 6
    probe = bottom - 1
    columns, run = [], []
    for x in range(left0, right0 + 1):
        if x < right0 and image.getpixel((x, probe)) not in (UI_BACKGROUND, UI_SPLITTER):
            run.append(x)
            continue
        if len(run) >= 64:
            columns.extend(run)
        run = []
    if not columns:
        return (left0, VIEWPORT_TOOLBAR_TOP + 18, right0, bottom)
    left, right = columns[0], columns[-1] + 1
    for y in range(VIEWPORT_TOOLBAR_TOP + 4, VIEWPORT_TOOLBAR_TOP + 80):
        if sum(1 for x in columns if image.getpixel((x, y)) == UI_BACKGROUND) < len(columns) * 0.02:
            return (left, y, right, bottom)
    return (left, VIEWPORT_TOOLBAR_TOP + 18, right, bottom)


def viewport_sha(path):
    """Hash of a window screenshot's 3D viewport image only (viewport_3d_box): the side panels, the menu bar,
    the viewport tabs and toolbar and the status bar are left out, so UI changes there do not fail
    render-equivalence checks, which compare the viewports' 3D image byte for byte. The toolbar and panels are
    covered by validate_p4_exit_gate.py (same-binary window screenshots). The box is part of the hash, so two
    screenshots whose 3D areas are found at different places never compare equal."""
    from PIL import Image
    image = Image.open(path).convert("RGB")
    box = viewport_3d_box(image)
    return hashlib.sha256(repr(box).encode() + image.crop(box).tobytes()).hexdigest()


# ----------------------------------------------------------------------------- capture sessions (docstring)

SESSIONS_ENV = "GLIDER_HARNESS_SESSIONS"
PROOF_ENV = "GLIDER_HARNESS_SESSION_PROOF"
PROOF_DIR = REPO / "build" / "session_proof"
# Startup flags (name -> value count): model independent, or replayed by open_model(startup_state=true)
# (--select-node, --camera-*). A session is started with them, so every capture in it shares them.
SESSION_STARTUP_FLAGS = {
    "--isolated": 0, "--window-size": 1, "--fixed-frame-time": 1, "--vsync": 1, "--clear-color": 1,
    "--hide-grid": 0, "--bloom": 1, "--auto-exposure": 1, "--exposure": 1, "--tone-mapping": 1,
    "--camera-target": 1, "--camera-yaw": 1, "--camera-pitch": 1, "--camera-distance": 1,
    "--camera-projection": 1, "--camera-fov": 1, "--select-node": 1, "--env": 1, "--msfs-emissive": 1,
    "--aa": 1, "--msaa": 1, "--post-aa": 1,
}
# Flags applied per capture through the registry after open_model.
SESSION_VARIANT_FLAGS = {"--debug-view": 1, "--asobo-disable": 1, "--asobo": 1, "--set-asobo-param": 4,
                         "--without-textures": 0, "--windshield-icing": 1}
SESSION_OUTPUT_FLAGS = {"--capture-viewport": 1, "--capture-frame": 1, "--profile-report": 1,
                        "--exit-after-profile": 0}
OPEN_SETTLE_FRAMES = 4  # open_model replies after this many rendered frames (kSettleAfterLoadFrames)


class _CaptureResult:
    """The parts of a CompletedProcess a harness reads from a capture launch."""

    def __init__(self, args, returncode, stdout, stderr=""):
        self.args, self.returncode, self.stdout, self.stderr = args, returncode, stdout, stderr
        self.from_cache = False
        self.from_session = True


def _session_plan(argv, cwd):
    """(exe, model, startup flags, variant flags, png, frame, report) when argv can run in a session, else None."""
    if os.environ.get(SESSIONS_ENV, "1").strip() == "0" or Path(cwd).resolve() != REPO:
        return None
    exe = Path(str(argv[0]))
    try:
        if harness_process.role(exe) != "current":
            return None
    except Exception:  # noqa: BLE001
        return None
    model, startup, variant, outputs = None, [], [], {}
    i = 1
    while i < len(argv):
        token = str(argv[i])
        for table, bucket in ((SESSION_STARTUP_FLAGS, startup), (SESSION_VARIANT_FLAGS, variant),
                              (SESSION_OUTPUT_FLAGS, None)):
            if token in table:
                count = table[token]
                values = [str(v) for v in argv[i + 1:i + 1 + count]]
                if len(values) != count:
                    return None
                if bucket is None:
                    outputs[token] = values
                else:
                    bucket.append((token, values))
                i += 1 + count
                break
        else:
            if token.startswith("--") or model is not None:
                return None  # a flag the session cannot replay: the CLI launch
            model = token
            i += 1
    if model is None or Path(model).suffix.lower() not in (".gltf", ".glb") or not (REPO / model).is_file():
        return None
    if "--capture-viewport" not in outputs or ("--isolated", []) not in startup or \
            not any(name == "--fixed-frame-time" for name, _ in startup):
        return None
    frame = int(outputs.get("--capture-frame", ["8"])[0])
    report = outputs.get("--profile-report", [None])[0]
    return exe, str((REPO / model).resolve()), startup, variant, outputs["--capture-viewport"][0], frame, report


class _SessionPool:
    """At most one open capture session (exe + startup flags) per harness process."""

    def __init__(self):
        self.key, self.process, self.pipe, self.errors = None, None, None, 0
        self.last_key = None
        self.unsupported = set()  # exes whose open_model has no startup_state (older builds): CLI launches only
        self.starting = False
        self.proofs = []
        harness_process.add_launch_hook(self._before_launch)
        import atexit
        atexit.register(self._at_exit)

    def _before_launch(self, target):
        if not self.starting:
            self.close()

    def close(self):
        if self.process is None:
            return
        process, pipe = self.process, self.pipe
        self.key, self.process, self.pipe = None, None, None
        try:
            pipe.call("quit")
        except Exception:  # noqa: BLE001
            pass
        try:
            pipe.close()
        except Exception:  # noqa: BLE001
            pass
        try:
            process.wait(60)
        except Exception:  # noqa: BLE001
            process.kill()

    @staticmethod
    def _key(plan):
        return str(plan[0]), tuple((name, tuple(values)) for name, values in plan[2])

    def wants(self, plan):
        """True when this capture should use a session: one with its startup flags is open, or the previous
        capture request had the same startup flags. A session costs about one CLI launch to start (startup, first
        frames, shader variants) and a fraction of one per capture after that, so a lone capture between others
        with different startup flags stays a CLI launch."""
        key = self._key(plan)
        if key[0] in self.unsupported:
            return False
        if self.key == key and self.process is not None and self.process.poll() is None:
            return True
        wanted, self.last_key = key == self.last_key, key
        return wanted

    def _open(self, exe, model, startup):
        key = (str(exe), tuple((name, tuple(values)) for name, values in startup))
        if self.key == key and self.process is not None and self.process.poll() is None:
            return
        self.close()
        args = [str(exe), model] + [part for name, values in startup for part in [name] + values] + ["--control-pipe"]
        self.starting = True
        try:
            self.process, self.pipe = start_pipe_session(args)
        finally:
            self.starting = False
        self.key = key
        commands = {command.get("name"): command for command in self.pipe.call("describe").get("commands", [])}
        schema = commands.get("open_model", {}).get("inputSchema", {}).get("properties", {})
        if "startup_state" not in schema:
            self.unsupported.add(str(exe))
            self.close()
            raise RuntimeError(f"{Path(str(exe)).name} has no open_model startup_state")
        self.errors = int(self.pipe.call("get_state").get("glErrors", 0))

    def capture(self, plan):
        exe, model, startup, variant, png, frame, report = plan
        self._open(exe, model, startup)
        Path(png).unlink(missing_ok=True)
        if report is not None:
            Path(report).unlink(missing_ok=True)
        self.pipe.call("open_model", path=model, startup_state=True,
                       without_textures=any(name == "--without-textures" for name, _ in variant))
        for name, values in variant:
            if name == "--debug-view":
                self.pipe.call("set_render_mode", mode=values[0])
            elif name == "--asobo-disable":
                self.pipe.call("set_viewport_setting", key=values[0], value=False)
            elif name == "--asobo":
                self.pipe.call("set_viewport_setting", key="asobo", value=values[0].lower() in ("on", "true", "1"))
            elif name == "--windshield-icing":
                self.pipe.call("set_environment", key="windshield_icing", value=float(values[0]))
            elif name == "--set-asobo-param":
                self.pipe.call("set_material_param", material=values[0], extension=values[1], param=values[2],
                               value=values[3])
        self.pipe.call("capture", path=str(Path(png).resolve()), settle_frames=max(1, frame - OPEN_SETTLE_FRAMES))
        errors = int(self.pipe.call("get_state").get("glErrors", 0))
        new_errors, self.errors = errors - self.errors, errors
        return new_errors

    def _at_exit(self):
        self.close()
        path = os.environ.get(PROOF_ENV, "").strip()
        if not path or not self.proofs:
            return
        import sys
        PROOF_DIR.mkdir(parents=True, exist_ok=True)
        same_count = 0
        with open(path, "a", encoding="utf-8") as stream:
            for index, (argv, session_hash) in enumerate(self.proofs):
                name = Path(argv[argv.index("--capture-viewport") + 1]).name
                cli = list(argv)
                png = PROOF_DIR / f"{index:04d}_{name}"
                cli[cli.index("--capture-viewport") + 1] = str(png)
                if "--profile-report" in cli:
                    cli[cli.index("--profile-report") + 1] = str(png.with_suffix(".json"))
                png.unlink(missing_ok=True)
                with harness_process.fresh():
                    harness_process.run(cli, cwd=REPO, capture_output=True, text=True, timeout=900)
                cli_hash = digest(png) if png.exists() else None
                equal = cli_hash is not None and cli_hash == session_hash
                same_count += equal
                stream.write(json.dumps({"script": Path(sys.argv[0]).name, "png": name, "argv": argv[1:],
                                         "session": session_hash, "cli": cli_hash, "equal": equal}) + "\n")
                if not equal:
                    print(f"  INFO session proof: {name} differs from its CLI launch", flush=True)
        print(f"  INFO session proof: {same_count} of {len(self.proofs)} session captures byte-identical to their "
              f"CLI launch", flush=True)


_pool = None


def capture_run(argv, timeout=600, session=True, cwd=None, **kwargs):
    """One viewport capture given as its CLI argv (the exe first); see "Capture sessions" in the docstring.

    Returns the CompletedProcess of the CLI launch (text output captured unless kwargs say otherwise), or for a
    session capture an object with returncode 0, stdout "GL automation errors: N" and from_session True.
    session=False forces the CLI launch (the caller reads the report or the launch's log)."""
    global _pool
    argv = [str(arg) for arg in argv]
    cwd = REPO if cwd is None else cwd
    plan = _session_plan(argv, cwd) if session else None
    if plan is not None and _pool is None:
        _pool = _SessionPool()
    if plan is not None and _pool.wants(plan):
        try:
            errors = _pool.capture(plan)
        except Exception as error:  # noqa: BLE001 - a broken session falls back to the CLI launch
            print(f"  INFO capture session failed ({error}); CLI launch instead", flush=True)
            _pool.close()
        else:
            if Path(plan[4]).exists():
                if os.environ.get(PROOF_ENV, "").strip():
                    _pool.proofs.append((argv, digest(plan[4])))
                text = f"GL automation errors: {errors}\n"
                binary = kwargs.get("text") is False
                return _CaptureResult(argv, 0, text.encode() if binary else text, b"" if binary else "")
            print(f"  INFO capture session wrote no {Path(plan[4]).name}; CLI launch instead", flush=True)
    options = {"cwd": cwd, "capture_output": True, "text": True, "timeout": timeout}
    options.update(kwargs)
    return harness_process.run(argv, **options)
