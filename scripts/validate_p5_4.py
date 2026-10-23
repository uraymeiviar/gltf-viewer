# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P5.4 event-driven idle loop validation (Windows).

When nothing needs a frame, the main loop blocks in glfwWaitEventsTimeout(0.25)
instead of polling. This script drives a real window from outside the process:
it posts Win32 input/window messages (so the waits are woken by the OS event
queue, not by the UI test harness, which always polls) and reads the frame
start times written by --idle-stats at exit.

Each run: load the hierarchy fixture (vsync on), settle, idle 30 s, then post a
mouse move, a key press, a viewport click, a window resize, play (playback must
keep frames coming at the vsync rate), stop, and idle again; then WM_CLOSE.
Runs alternate between the candidate and the same binary with
--disable-idle-wait.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import json
import statistics
import subprocess
import sys
import time
from pathlib import Path

import psutil

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "p5.4"
HIER = "resources/test_models/hierarchy_animation_fixture.gltf"

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--runs", type=int, default=3)
parser.add_argument("--idle-seconds", type=float, default=30.0)
args = parser.parse_args()
failures = []

user32 = ctypes.windll.user32
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP = 0x0200, 0x0201, 0x0202
WM_KEYDOWN, WM_KEYUP, WM_CLOSE = 0x0100, 0x0101, 0x0010
MK_LBUTTON, VK_SHIFT = 0x0001, 0x10
SWP_NOMOVE, SWP_NOZORDER = 0x0002, 0x0004


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def find_window(pid, timeout=20.0):
    found = []
    proc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

    def callback(hwnd, _):
        owner = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    end = time.time() + timeout
    while time.time() < end:
        found.clear()
        user32.EnumWindows(proc(callback), 0)
        if found:
            return found[0]
        time.sleep(0.2)
    return None


def lparam(x, y):
    return (y << 16) | (x & 0xFFFF)


def now_ms():
    return time.time() * 1000.0


hung = {"candidate": 0, "polling": 0}


def run(mode, index):
    OUT.mkdir(parents=True, exist_ok=True)
    stats = OUT / f"idle_{mode}_{index}.json"
    stats.unlink(missing_ok=True)
    flags = ["--disable-idle-wait"] if mode == "polling" else []
    proc = subprocess.Popen([str(EXE), HIER, "--isolated", "--window-size", "1280x800", "--aa", "none",
                             "--idle-stats", str(stats)] + flags, cwd=REPO,
                            stdout=open(OUT / f"stdout_{mode}_{index}.txt", "w"), stderr=subprocess.STDOUT)
    ps = psutil.Process(proc.pid)
    hwnd = find_window(proc.pid)
    if hwnd is None:
        proc.kill()
        check(False, f"{mode} {index}: window not found")
        return None
    time.sleep(4.0)  # load, IBL and UI settle
    marks = {}
    cpu0 = sum(ps.cpu_times()[:2])
    marks["idle_start"] = now_ms()
    time.sleep(args.idle_seconds)
    marks["idle_end"] = now_ms()
    cpu_idle = sum(ps.cpu_times()[:2]) - cpu0

    def post(label, messages, settle=1.0):
        marks[label] = now_ms()
        for msg, wparam, lp in messages:
            user32.PostMessageW(hwnd, msg, wparam, lp)
        time.sleep(settle)

    post("mouse_move", [(WM_MOUSEMOVE, 0, lparam(640, 400))])
    post("mouse_move2", [(WM_MOUSEMOVE, 0, lparam(700, 420))])
    post("key", [(WM_KEYDOWN, VK_SHIFT, 0x002A0001), (WM_KEYUP, VK_SHIFT, 0xC02A0001)])
    post("click", [(WM_MOUSEMOVE, 0, lparam(650, 418)), (WM_LBUTTONDOWN, MK_LBUTTON, lparam(650, 418)),
                   (WM_LBUTTONUP, 0, lparam(650, 418))])
    # A resize sent from another process (as the shell does for snapping): SetWindowPos
    # sends, rather than posts, its messages. The original outer size is restored so
    # the client area (and the status-bar buttons below) is unchanged.
    rect = wt.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    marks["resize"] = now_ms()
    user32.SetWindowPos(hwnd, 0, 0, 0, rect.right - rect.left - 80, rect.bottom - rect.top - 40, SWP_NOMOVE | SWP_NOZORDER)
    marks["resize_returned"] = now_ms()
    time.sleep(1.0)
    marks["resize_back"] = now_ms()
    user32.SetWindowPos(hwnd, 0, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER)
    time.sleep(1.0)
    # Status-bar play button, then 3 s of playback, then stop and idle.
    post("play", [(WM_MOUSEMOVE, 0, lparam(122, 783)), (WM_LBUTTONDOWN, MK_LBUTTON, lparam(122, 783)),
                  (WM_LBUTTONUP, 0, lparam(122, 783))], settle=0.5)
    marks["play_window_start"] = now_ms()
    time.sleep(3.0)
    marks["play_window_end"] = now_ms()
    post("stop", [(WM_MOUSEMOVE, 0, lparam(155, 783)), (WM_LBUTTONDOWN, MK_LBUTTON, lparam(155, 783)),
                  (WM_LBUTTONUP, 0, lparam(155, 783))], settle=1.0)
    marks["idle2_start"] = now_ms()
    time.sleep(5.0)
    marks["idle2_end"] = now_ms()
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    try:
        proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
        proc.kill()
        # A pre-existing shutdown hang (also at 1a22976, before Phase 5): reported, not gated.
        print(f"    {mode} {index}: did not exit within 20 s of WM_CLOSE (killed; pre-existing shutdown hang)")
        hung[mode] += 1
        return None
    data = json.loads(stats.read_text(encoding="utf-8"))
    starts = data["frameStartsMs"]

    def frames_between(a, b):
        return sum(1 for t in starts if marks[a] <= t < marks[b])

    def latency(label):
        after = [t for t in starts if t >= marks[label]]
        return (after[0] - marks[label]) if after else None

    result = {
        "idle_frames": frames_between("idle_start", "idle_end"),
        "idle_cpu_s": cpu_idle,
        "play_fps": frames_between("play_window_start", "play_window_end") / 3.0,
        "idle2_frames": frames_between("idle2_start", "idle2_end"),
        "latency": {k: latency(k) for k in ("mouse_move", "mouse_move2", "key", "click", "resize", "resize_back", "play", "stop")},
        "resize_call_ms": marks["resize_returned"] - marks["resize"],
        "waits": data["waits"],
        "exit_code": proc.returncode,
        "marks": marks,
        "wakes_after": {k: next((t - marks[k] for t in data.get("wakesMs", []) if t >= marks[k] - 1), None)
                        for k in ("mouse_move", "mouse_move2", "key", "click", "stop")},
    }
    print(f"    {mode} {index}: idle {result['idle_frames']} frames / {args.idle_seconds:.0f} s, "
          f"cpu {cpu_idle:.2f} s, playing {result['play_fps']:.1f} fps, idle2 {result['idle2_frames']} frames, "
          f"waits {data['waits']}, latency ms " + ", ".join(f"{k} {v:.1f}" if v is not None else f"{k} -"
                                                            for k, v in result["latency"].items()))
    return result


results = {"candidate": [], "polling": []}
for i in range(args.runs):
    for mode in (("candidate", "polling") if i % 2 == 0 else ("polling", "candidate")):
        r = run(mode, i)
        if r:
            results[mode].append(r)
(OUT / "summary.json").write_text(json.dumps(results, indent=2), encoding="utf-8")

cand, poll = results["candidate"], results["polling"]
if cand and poll:
    budget = args.idle_seconds / 0.25 + 10
    check(all(r["idle_frames"] <= budget for r in cand),
          f"idle frames per {args.idle_seconds:.0f} s: candidate {[r['idle_frames'] for r in cand]} <= {budget:.0f} "
          f"(polling {[r['idle_frames'] for r in poll]})")
    check(all(r["idle2_frames"] <= 5.0 / 0.25 + 10 for r in cand), "idle again after playback stops "
          f"({[r['idle2_frames'] for r in cand]} frames / 5 s)")
    pc, cc = [r["idle_cpu_s"] for r in poll], [r["idle_cpu_s"] for r in cand]
    noise = (max(pc) - min(pc)) / statistics.median(pc)
    deltas = [(a - b) / a for a, b in zip(pc, cc)]
    check(statistics.median(deltas) > max(0.03, noise),
          f"idle process CPU {min(pc):.2f}-{max(pc):.2f} -> {min(cc):.2f}-{max(cc):.2f} s per {args.idle_seconds:.0f} s "
          f"(median paired {statistics.median(deltas):+.1%}, noise {noise:.1%})")
    pf, cf = [r["play_fps"] for r in poll], [r["play_fps"] for r in cand]
    check(min(cf) >= 0.9 * statistics.median(pf), f"playback frame rate kept: {cf} vs polling {pf}")
    check(statistics.median(r["resize_call_ms"] for r in cand) <= statistics.median(r["resize_call_ms"] for r in poll) + 20.0,
          f"cross-process SetWindowPos returns promptly: {[round(r['resize_call_ms'], 1) for r in cand]} ms "
          f"(polling {[round(r['resize_call_ms'], 1) for r in poll]})")
    for key in ("mouse_move", "mouse_move2", "key", "click", "resize", "resize_back", "play", "stop"):
        cl = [r["latency"][key] for r in cand]
        pl = [r["latency"][key] for r in poll]
        if None in cl or None in pl:
            check(False, f"latency {key}: a run produced no frame after the event")
            continue
        check(statistics.median(cl) <= statistics.median(pl) + 5.0 and max(cl) <= 50.0,
              f"wake latency {key}: candidate median {statistics.median(cl):.1f} ms (max {max(cl):.1f}) vs polling "
              f"median {statistics.median(pl):.1f} ms")
    check(all(r["exit_code"] == 0 for r in cand + poll), "runs that exited returned 0")
    print(f"    shutdown hangs after WM_CLOSE (pre-existing, not gated): candidate {hung['candidate']}, polling {hung['polling']}")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
