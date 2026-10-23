# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P4.4 bounds / reverse-map cache validation.

The selected-node OBB and the scene extents are cached on the render layer's
pose/scene generations; the picking reverse map is moved instead of copied.
Each scenario runs the same UI script on the pre-change binary (HEAD, P4.3,
copied to build/p4.4/head/Glider.exe) and on the candidate, with
--fixed-frame-time so playback is deterministic, and requires identical
snapshots (selection, camera) and byte-identical full-window screenshots.
Candidate-only counters (boundsComputed / boundsHits in UI snapshots) must show
reuse on unchanged frames and recomputation after every input change, including
the Stop / Rewind / End buttons, which change the pose without marking a
revision.

Sections: static, playback.
"""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
HEAD_EXE = REPO / "build" / "p4.4" / "head" / "Glider.exe"
OUT = REPO / "build" / "p4.4"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
POINTER = "resources/test_models/pointer_animation_fixture.gltf"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05", "--aa", "none",
          "--camera-distance", "6", "--camera-yaw", "30", "--camera-pitch", "20"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["static", "playback"])
args = parser.parse_args()
failures = []


def section(name):
    return args.only in (None, name)


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(mode, exe, name, script, extra):
    folder = OUT / mode
    folder.mkdir(parents=True, exist_ok=True)
    text = script.replace("@OUT@", folder.relative_to(REPO).as_posix())
    shots = [line.split(" ", 1)[1] for line in text.splitlines() if line.startswith("shot ")]
    for shot in shots:
        (REPO / shot).unlink(missing_ok=True)
    scenario, report = folder / f"{name}.txt", folder / f"{name}.json"
    scenario.write_text(text, encoding="utf-8")
    report.unlink(missing_ok=True)
    result = subprocess.run([str(exe)] + extra + COMMON + ["--ui-script", str(scenario), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=180)
    if result.returncode != 0 or not report.exists():
        check(False, f"{name} [{mode}]: exit {result.returncode} {result.stderr[-300:]}")
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    if data.get("failed", True):
        check(False, f"{name} [{mode}]: script errors {data.get('errors')}")
        return None
    return {"snaps": {s["label"]: s for s in data["snaps"]}, "shots": {Path(p).name: REPO / p for p in shots}}


def compare(name, script, extra):
    head = run("head", HEAD_EXE, name, script, extra)
    cand = run("candidate", EXE, name, script, extra)
    if head is None or cand is None:
        return None
    for label, snap in cand["snaps"].items():
        h = head["snaps"][label]
        check((h["selection"], h["camera"]) == (snap["selection"], snap["camera"]),
              f"{name}/{label}: selection and camera match HEAD ({snap['selection'].split('|')[1]})")
    for shot, path in cand["shots"].items():
        check(sha(path) == sha(head["shots"][shot]), f"{name}/{shot}: byte-identical to HEAD")
    return {k: v["revisions"] for k, v in cand["snaps"].items()}


def window(rev, before, after):
    a, b = rev[before], rev[after]
    return b["boundsComputed"] - a["boundsComputed"], b["boundsHits"] - a["boundsHits"]


def expect_reuse(rev, before, after, label):
    computed, hits = window(rev, before, after)
    check(computed == 0 and hits >= 20, f"{label} {before}->{after}: reused ({computed} computed, {hits} hits)")


def expect_recompute(rev, before, after, label, minimum=1):
    computed, hits = window(rev, before, after)
    check(computed >= minimum, f"{label} {before}->{after}: recomputed ({computed} computed, {hits} hits)")


if not HEAD_EXE.exists():
    check(False, f"HEAD binary missing: {HEAD_EXE} (NOT RUN)")
else:
    if section("static"):
        print("[static: transform edit, visibility, selection, reload, second viewport]")
        rev = compare("static", f"""wait 8
vpsetting 1 bbox on
picknode 2
wait 8
snap selected
shot @OUT@/st_selected.png
wait 30
snap idle
dclick 1052 163
wait 4
key a ctrl
type 0.9
key enter
wait 10
snap moved
shot @OUT@/st_moved.png
wait 20
snap moved_idle
click 1084 257
wait 10
snap reset
shot @OUT@/st_reset.png
picknode 0
wait 8
snap selected0
shot @OUT@/st_selected0.png
wait 20
snap selected0_idle
reload {FOX}
wait 12
picknode 1
wait 8
snap fox_selected
shot @OUT@/st_fox_selected.png
reload {FIXTURE}
wait 12
snap reloaded
click 650 418
wait 8
snap repicked
shot @OUT@/st_repicked.png
vpcreate
wait 12
click 650 418
wait 8
snap vp2_picked
shot @OUT@/st_vp2_picked.png
wait 20
snap vp2_idle
wait 2
""", [FIXTURE])
        if rev:
            expect_reuse(rev, "selected", "idle", "static")
            expect_recompute(rev, "idle", "moved", "static")
            expect_reuse(rev, "moved", "moved_idle", "static")
            expect_recompute(rev, "moved_idle", "reset", "static")
            expect_recompute(rev, "reset", "selected0", "static")
            expect_reuse(rev, "selected0", "selected0_idle", "static")
            expect_recompute(rev, "selected0_idle", "fox_selected", "static")
            expect_recompute(rev, "fox_selected", "repicked", "static")
            expect_reuse(rev, "vp2_picked", "vp2_idle", "static")
            check(rev["moved"]["Transform"] - rev["idle"]["Transform"] == 1, "Inspector translation edit marks Transform")

    if section("playback"):
        print("[playback: animated OBB, Stop / End / Rewind without revision marks]")
        rev = compare("playback", f"""wait 8
vpsetting 1 bbox on
picknode 1
wait 8
snap selected
wait 20
snap idle
animplay on
wait 10
snap play_a
shot @OUT@/pb_play_a.png
wait 7
snap play_b
shot @OUT@/pb_play_b.png
animplay off
wait 6
snap paused
wait 20
snap paused_idle
click 155 783
wait 8
snap stopped
shot @OUT@/pb_stopped.png
click 189 783
wait 8
snap end
shot @OUT@/pb_end.png
click 88 783
wait 8
snap rewind
shot @OUT@/pb_rewind.png
wait 20
snap rewind_idle
wait 2
""", [POINTER])
        if rev:
            expect_reuse(rev, "selected", "idle", "playback")
            ticks = rev["play_b"]["AnimationTime"] - rev["play_a"]["AnimationTime"]
            computed, hits = window(rev, "play_a", "play_b")
            check(ticks >= 5 and computed == ticks,
                  f"playback play_a->play_b: OBB recomputed once per tick ({computed} for {ticks} ticks), extents reused ({hits} hits)")
            expect_reuse(rev, "paused", "paused_idle", "playback")
            for before, after in (("paused_idle", "stopped"), ("stopped", "end"), ("end", "rewind")):
                expect_recompute(rev, before, after, "playback unmarked transport")
                check(rev[after]["marks"] == rev[before]["marks"], f"playback {before}->{after}: no revision mark (unmarked path)")
            expect_reuse(rev, "rewind", "rewind_idle", "playback")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
