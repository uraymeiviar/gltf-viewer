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
  start_pipe_session(args)      Popen with --control-pipe already in args; waits for and connects the pipe
  connect_pipe(process)         connects to a started Glider's pipe, retrying while it lives (no 10 s give-up)
  gl_errors_zero(log)           the "GL automation errors: 0" line is present and no "GL error 0x" was logged
  read_json(path)               parsed JSON, or None when the file is missing or unreadable
  digest(path), same(a, b)      SHA-256 of a file; byte equality of two image files (None never equal)
  viewport_sha(path)            SHA-256 of a window screenshot's viewport area (side panels, menu, status bar cut)
"""

import hashlib
import json
import os
import time
from pathlib import Path

import harness_process

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
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


def start_pipe_session(args, timeout=120):
    """Starts Glider (args include --control-pipe) and returns (process, glider_ctl.GliderPipe).

    Waits up to `timeout` seconds for the pipe to appear and accept a client; the pipe can exist a
    moment before it does. Raises RuntimeError when Glider exits or never opens it, and the
    connection error when the pipe never accepts (the process is killed then)."""
    import glider_ctl
    process = harness_process.Popen([str(arg) for arg in args], cwd=REPO, stdin=harness_process.DEVNULL,
                                    stdout=harness_process.DEVNULL, stderr=harness_process.DEVNULL)
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
