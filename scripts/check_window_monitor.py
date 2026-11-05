# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Check GLIDER_MONITOR: a window is never visible outside the chosen monitor, and the render does not change.

  python scripts/check_window_monitor.py [--exe path]

Launches the build with GLIDER_MONITOR=left, right, primary and an index, polling the process's window as fast as
Python can, and compares the frame with one taken without the variable. An unknown value must warn and still run.
Needs two or more monitors for the placement checks to mean anything; on one monitor it reports that and still checks
the rest. Windows only.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[0]))
import harness_process  # noqa: E402 - shared harness launcher

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build" / "check_window_monitor"

user32 = ctypes.WinDLL("user32")
shcore = ctypes.WinDLL("shcore")
WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user32.SetProcessDpiAwarenessContext.argtypes = [ctypes.c_void_p]


def monitors():
    found = []
    PROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HMONITOR, wt.HDC, ctypes.POINTER(wt.RECT), wt.LPARAM)

    def callback(_handle, _dc, rect, _data):
        r = rect.contents
        found.append((r.left, r.top, r.right, r.bottom))
        return True

    user32.EnumDisplayMonitors(0, None, PROC(callback), 0)
    return found


def visible_rects(pid):
    rects = []

    def visit(hwnd, _):
        owner = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        name = ctypes.create_unicode_buffer(16)
        user32.GetClassNameW(hwnd, name, 16)
        if owner.value == pid and name.value == "GLFW30" and user32.IsWindowVisible(hwnd):
            rect = wt.RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(rect))
            rects.append((rect.left, rect.top, rect.right, rect.bottom))
        return True

    user32.EnumWindows(WNDENUMPROC(visit), 0)
    return rects


def launch(exe, label, value):
    """Returns (exit code, visible window rects seen, frame digest, stderr)."""
    dump = OUT / f"{label}.png"
    dump.unlink(missing_ok=True)
    err_path = OUT / f"{label}.err"
    env = dict(os.environ)
    env.pop("GLIDER_MONITOR", None)
    if value is not None:
        env["GLIDER_MONITOR"] = value
    with open(err_path, "wb") as err:
        process = harness_process.Popen(
            [str(exe), "--test", "--dump-frame", str(dump), "--exit-after-dump"],
            cwd=REPO, env=env, stdout=harness_process.DEVNULL, stderr=err)
        seen = []
        while process.poll() is None:
            for rect in visible_rects(process.pid):
                if rect not in seen:
                    seen.append(rect)
        process.wait()
    digest = hashlib.sha256(dump.read_bytes()).hexdigest() if dump.exists() else None
    return process.returncode, seen, digest, err_path.read_text(encoding="utf-8", errors="replace")


def inside(rect, screen):
    middle_x, middle_y = (rect[0] + rect[2]) // 2, (rect[1] + rect[3]) // 2
    return screen[0] <= middle_x < screen[2] and screen[1] <= middle_y < screen[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", type=Path, default=harness_process.candidate_exe())
    parser.add_argument("--off-primary", action="store_true",
                        help="never open a window on the primary monitor (someone is working there): checks only the "
                             "outer monitors, against each other")
    args = parser.parse_args()
    if sys.platform != "win32":
        print("check_window_monitor: Windows only")
        return 0
    user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))  # per monitor v2: physical pixels
    OUT.mkdir(parents=True, exist_ok=True)
    screens = monitors()
    print(f"[monitors: {screens}]")
    failures = []

    def check(name, ok, detail=""):
        print(f"  {'PASS' if ok else 'FAIL'} {name}{': ' + detail if detail else ''}")
        if not ok:
            failures.append(name)

    primary = next(s for s in screens if s[0] == 0 and s[1] == 0)
    left, right = min(screens), max(screens)
    reference = None
    if not args.off_primary:
        code, _, reference, _ = launch(args.exe, "unset", None)
        check("no variable: runs and writes a frame", code == 0 and reference is not None)
    cases = [("left", left), ("right", right)]
    if not args.off_primary:
        cases += [("primary", primary), ("0", None)]
    for value, screen in cases:
        code, seen, digest, _ = launch(args.exe, f"monitor_{value}", value)
        reference = reference or digest
        check(f"{value}: runs and writes a frame", code == 0 and digest is not None)
        check(f"{value}: same frame as the reference", digest == reference)
        check(f"{value}: the window was seen", bool(seen))
        if screen is not None:
            check(f"{value}: never visible outside the monitor", all(inside(rect, screen) for rect in seen),
                  f"{len(seen)} positions {seen[:3]}")

    if args.off_primary:
        print("  SKIP no variable, primary, index and unknown value: they open on the primary monitor")
    else:
        code, seen, digest, err = launch(args.exe, "monitor_unknown", "nowhere")
        check("unknown value: warns", "GLIDER_MONITOR" in err)
        check("unknown value: still runs with the same frame", code == 0 and digest == reference)
    if len(screens) < 2:
        print("  NOTE one monitor: the placement checks could not tell monitors apart")
    print("=" * 60)
    print("RESULT: " + ("FAIL " + ", ".join(failures) if failures else "PASS"))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
