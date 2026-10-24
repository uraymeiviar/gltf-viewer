# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P4.5 viewport settings ownership validation.

Per-viewport overrides that persist on the render layer's shared materials and
meshes (double-sidedness, winding) are written by one applier that tracks what
is applied, instead of rewriting every material and mesh per viewport per
frame. Each scenario docks two viewports side by side (harness `vpsplit`, so
both render every frame) and runs the same UI script on the pre-P4.5 binary
(P4.4 + vpsplit, build/p4.5/p4_4/Glider.exe) and on the candidate:
  * selection/camera snapshots identical, full-window screenshots byte-identical,
  * candidate counters (overrideApplies / overrideSkips in UI snapshots): equal
    overrides on both viewports skip, differing overrides re-apply per viewport,
    layer changes (reload, playback poses) re-apply.

Sections: split, modes, playback.
"""

import argparse
import hashlib
import json
import harness_process as subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
REF_EXE = REPO / "build" / "p4.5" / "p4_4" / "Glider.exe"
OUT = REPO / "build" / "p4.5"
CULL = "build/p4.5/fixtures/cull_winding_fixture.gltf"  # single-sided, back-facing, double-sided, mirrored planes
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05", "--aa", "none"]
SPLIT = "wait 8\nvpcreate\nwait 10\nvpsplit 2\nwait 10\n"

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["split", "modes", "playback"])
args = parser.parse_args()
failures = []

sys.path.insert(0, str(REPO / "scripts"))
from generate_cull_winding_fixture import create_cull_winding_fixture  # noqa: E402
create_cull_winding_fixture(str(REPO / CULL))


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


def compare(name, script, extra, same_groups=()):
    """same_groups: tuples of screenshot names expected to be identical; all others must differ."""
    ref = run("reference", REF_EXE, name, script, extra)
    cand = run("candidate", EXE, name, script, extra)
    if ref is None or cand is None:
        return None
    for label, snap in cand["snaps"].items():
        r = ref["snaps"][label]
        same = (r["selection"], r["camera"]) == (snap["selection"], snap["camera"])
        rendered = all(v["renderCount"] > 0 for v in snap["viewports"])
        check(same and rendered and len(snap["viewports"]) == 2,
              f"{name}/{label}: snapshot matches reference, both viewports rendering")
    hashes = {}
    for shot, path in cand["shots"].items():
        check(sha(path) == sha(ref["shots"][shot]), f"{name}/{shot}: byte-identical to reference")
        hashes[shot] = sha(path)
    grouped = {shot for group in same_groups for shot in group}
    for group in same_groups:
        check(len({hashes[s] for s in group}) == 1, f"{name}: {group} identical as expected")
    distinct = [hashes[s] for s in hashes if s not in grouped] + [hashes[g[0]] for g in same_groups]
    check(len(set(distinct)) == len(distinct), f"{name}: all other states visibly distinct")
    return {k: v for k, v in cand["snaps"].items()}


def counts(snaps, before, after):
    a, b = snaps[before]["revisions"], snaps[after]["revisions"]
    return b["overrideApplies"] - a["overrideApplies"], b["overrideSkips"] - a["overrideSkips"]


def frames_rendered(snaps, before, after):
    return sum(vb["renderCount"] - va["renderCount"]
               for va, vb in zip(snaps[before]["viewports"], snaps[after]["viewports"]))


def expect_skip(snaps, before, after, label):
    applies, skips = counts(snaps, before, after)
    rendered = frames_rendered(snaps, before, after)
    check(applies == 0 and skips == rendered and rendered >= 20,
          f"{label} {before}->{after}: equal overrides skip ({applies} applies, {skips} skips, {rendered} viewport renders)")


def expect_apply_each(snaps, before, after, label):
    applies, skips = counts(snaps, before, after)
    rendered = frames_rendered(snaps, before, after)
    check(applies == rendered and skips == 0 and rendered >= 20,
          f"{label} {before}->{after}: applied per viewport render ({applies} applies, {skips} skips, {rendered} renders)")


if not REF_EXE.exists():
    check(False, f"reference binary missing: {REF_EXE} (NOT RUN)")
else:
    if section("split"):
        print("[split: culling / winding overrides on two live viewports]")
        snaps = compare("split", SPLIT + f"""snap same
wait 20
snap same_idle
shot @OUT@/sp_same.png
vpsetting 2 doublesided on
vpsetting 2 invertwinding on
wait 4
snap differ
wait 20
snap differ_idle
shot @OUT@/sp_differ.png
vpsetting 1 doublesided on
vpsetting 1 invertwinding on
wait 4
snap both
wait 20
snap both_idle
shot @OUT@/sp_both.png
vpsetting 1 doublesided off
vpsetting 1 invertwinding off
vpsetting 2 doublesided off
wait 4
snap winding_only
wait 20
snap winding_only_idle
shot @OUT@/sp_winding_only.png
vpsetting 2 invertwinding off
wait 4
snap restored
wait 20
snap restored_idle
shot @OUT@/sp_restored.png
reload {CULL}
wait 12
snap reloaded
wait 20
snap reloaded_idle
shot @OUT@/sp_reloaded.png
wait 2
""", [CULL], same_groups=[("sp_same.png", "sp_restored.png")])
        if snaps:
            expect_skip(snaps, "same", "same_idle", "split")
            expect_apply_each(snaps, "differ", "differ_idle", "split")
            expect_skip(snaps, "both", "both_idle", "split")
            expect_apply_each(snaps, "winding_only", "winding_only_idle", "split")
            expect_skip(snaps, "restored", "restored_idle", "split")
            applies, _ = counts(snaps, "restored_idle", "reloaded")
            check(applies >= 1, f"split restored_idle->reloaded: reload re-applies ({applies})")
            expect_skip(snaps, "reloaded", "reloaded_idle", "split")

    if section("modes"):
        print("[modes: different debug render modes and wireframe overlay per viewport]")
        snaps = compare("modes", SPLIT + f"""click 930 60
wait 6
click 990 167
wait 10
snap vp2_normal
shot @OUT@/md_vp2_normal.png
click 560 60
wait 6
click 600 92
wait 10
snap vp1_wire
shot @OUT@/md_vp1_wire.png
wait 20
snap modes_idle
click 930 60
wait 6
click 982 317
wait 10
snap vp2_basecolor
shot @OUT@/md_vp2_basecolor.png
vpsetting 2 doublesided on
wait 10
snap vp2_basecolor_ds
shot @OUT@/md_vp2_basecolor_ds.png
wait 2
""", [CULL])
        if snaps:
            expect_skip(snaps, "vp1_wire", "modes_idle", "modes (debug modes differ, persistent overrides equal)")

    if section("playback"):
        print("[playback: skinned playback with differing winding per viewport]")
        snaps = compare("playback", SPLIT + """vpsetting 2 invertwinding on
vpsetting 2 doublesided on
animplay on
wait 10
snap play_a
shot @OUT@/pb_play_a.png
wait 20
snap play_b
shot @OUT@/pb_play_b.png
vpsetting 2 invertwinding off
vpsetting 2 doublesided off
wait 4
snap play_same
wait 20
snap play_same_b
shot @OUT@/pb_play_same.png
animplay off
wait 6
snap paused
wait 20
snap paused_idle
wait 2
""", [FOX])
        if snaps:
            expect_apply_each(snaps, "play_a", "play_b", "playback (differ)")
            applies, skips = counts(snaps, "play_same", "play_same_b")
            rendered = frames_rendered(snaps, "play_same", "play_same_b")
            ticks = snaps["play_same_b"]["revisions"]["AnimationTime"] - snaps["play_same"]["revisions"]["AnimationTime"]
            check(applies == ticks and skips == rendered - ticks,
                  f"playback (equal) play_same->play_same_b: one apply per pose change ({applies} applies for {ticks} ticks, {skips} skips)")
            expect_skip(snaps, "paused", "paused_idle", "playback (paused)")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
