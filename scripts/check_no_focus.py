# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Check that harness launches never give the current build the foreground keyboard focus.

Run from a terminal after building:

  python scripts/check_no_focus.py
  python scripts/check_no_focus.py --baseline build/baselines/a9c0a29.exe

The build is launched through scripts/harness_process.py with a dump command
(--isolated --dump-frame ... --exit-after-dump). The script records GetForegroundWindow
before each launch, polls it every 20 ms while the child runs, and passes only if the
foreground is never a Glider window. The dump must still be produced.

A baseline that predates GLIDER_NO_FOCUS is only started with SW_SHOWNOACTIVATE and may
take focus; it is skipped. The pinned regression references are served from
build/regression_cache/ after their first render instead of being launched again.
"""

import argparse
import ctypes
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[0]))
import harness_process  # noqa: E402 - shared harness launcher

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
DEFAULT_BASELINE = REPO / "build" / "baselines" / "a9c0a29.exe"
OUT = REPO / "build" / "check_no_focus"
POLL_SECONDS = 0.02


def foreground_window():
    return ctypes.windll.user32.GetForegroundWindow() if sys.platform == "win32" else 0


def window_text(hwnd):
    user32 = ctypes.windll.user32
    length = user32.GetWindowTextLengthW(hwnd)
    text = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(hwnd, text, length + 1)
    return text.value


def window_class(hwnd):
    text = ctypes.create_unicode_buffer(64)
    ctypes.windll.user32.GetClassNameW(hwnd, text, len(text))
    return text.value


def is_glider_window(hwnd):
    return bool(hwnd) and (window_text(hwnd).startswith("Glider - ") or window_class(hwnd) == "GLFW30")


def check(exe, label):
    dump = OUT / f"{label}.png"
    dump.unlink(missing_ok=True)
    before = foreground_window()
    changes = []
    glider_hits = []
    process = harness_process.Popen(
        [str(exe), "--isolated", "--dump-frame", str(dump), "--exit-after-dump"],
        cwd=REPO,
        stdout=harness_process.DEVNULL,
        stderr=harness_process.DEVNULL,
    )
    polls = 0
    while process.poll() is None:
        hwnd = foreground_window()
        polls += 1
        if hwnd != before and (not changes or changes[-1] != hwnd):
            changes.append(hwnd)
            if is_glider_window(hwnd) and hwnd not in glider_hits:
                glider_hits.append(hwnd)
        time.sleep(POLL_SECONDS)
    process.wait()
    ok = process.returncode == 0 and dump.exists() and dump.stat().st_size > 0 and not glider_hits
    details = ", ".join(f"0x{hwnd:x} {window_text(hwnd)!r}" for hwnd in changes) or "none"
    print(f"  {'PASS' if ok else 'FAIL'} {label}: exit {process.returncode}, {polls} polls, "
          f"foreground changes: {details}, Glider foreground windows: {len(glider_hits)}")
    return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE,
                        help="old baseline binary (default: build/baselines/a9c0a29.exe)")
    args = parser.parse_args()
    if sys.platform != "win32":
        print("check_no_focus: Windows only; no foreground window to check")
        return 0
    for path, label in ((EXE, "current"), (args.baseline, "baseline")):
        if not path.exists():
            print(f"  FAIL {label}: {path} not found")
            print("=" * 60)
            print("RESULT: FAIL")
            return 1

    OUT.mkdir(parents=True, exist_ok=True)
    print(f"[foreground window: 0x{foreground_window():x}]")
    ok = check(EXE, "current")
    if harness_process.supports_no_focus(args.baseline):
        ok = check(args.baseline, "baseline") and ok
    else:
        print(f"  SKIP baseline {args.baseline.name}: no GLIDER_NO_FOCUS marker; it is started "
              "with SW_SHOWNOACTIVATE only and may take focus (pinned references are cached)")
    print("=" * 60)
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
