# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P4.7 dirty-subtree transform propagation validation.

TransformHierarchyNode::traverse now recomputes and pushes a node's world
transform only when its parent changed, its own TRS targets are dirty, or it
was invalidated (new attachment, hierarchy rebuild).

Sections:
  equivalence  Each UI script runs on the pre-P4.7 binary
               (build/p4.7/pre/Glider.exe), on the candidate with
               --disable-transform-reuse and on the candidate, all with
               --fixed-frame-time. Snapshots and full-window screenshots must be
               identical across the three; candidate counters must show only
               changed subtrees recomputed.
  benchmark    Paired pre/candidate playback runs on the leaf-animated
               1,600-node grid.
"""

import argparse
import hashlib
import json
import statistics
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
PRE_EXE = REPO / "build" / "p4.7" / "pre" / "Glider.exe"
OUT = REPO / "build" / "p4.7"
HIER = "resources/test_models/hierarchy_animation_fixture.gltf"
POINTER = "resources/test_models/pointer_animation_fixture.gltf"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
MORPH = "OpenGL-Renderer/Tools/tests/test_resources/AnimatedMorphCube.gltf"
LEAF = "build/p4.7/fixtures/many_leaf.gltf"
ROOT = "build/p4.7/fixtures/many_root.gltf"
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["equivalence", "benchmark"])
parser.add_argument("--pairs", type=int, default=5)
parser.add_argument("--pre-exe", help="reference binary for the benchmark (default: pre-P4.7)")
args = parser.parse_args()
if args.pre_exe:
    PRE_EXE = Path(args.pre_exe).resolve()
failures = []

for anim, path in (("leaf", LEAF), ("root", ROOT)):
    subprocess.run([sys.executable, str(REPO / "scripts" / "generate_many_drawables_fixture.py"), str(REPO / path), "40", anim],
                   check=True, capture_output=True)
subprocess.run([sys.executable, str(REPO / "scripts" / "generate_hierarchy_fixture.py")], check=True, capture_output=True)


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
    flags = ["--disable-transform-reuse"] if mode == "noreuse" else []
    result = subprocess.run([str(exe)] + extra + COMMON + flags + ["--ui-script", str(scenario), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=300)
    if result.returncode != 0 or not report.exists():
        check(False, f"{name} [{mode}]: exit {result.returncode} {result.stderr[-300:]}")
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    if data.get("failed", True):
        check(False, f"{name} [{mode}]: script errors {data.get('errors')}")
        return None
    return {"snaps": {s["label"]: s for s in data["snaps"]}, "shots": {Path(p).name: REPO / p for p in shots}}


def equivalence(name, script, extra):
    pre = run("pre", PRE_EXE, name, script, extra)
    nor = run("noreuse", EXE, name, script, extra)
    cand = run("candidate", EXE, name, script, extra)
    if not (pre and nor and cand):
        return None, None
    for label, snap in cand["snaps"].items():
        same = all((o["snaps"][label]["selection"], o["snaps"][label]["camera"],
                    o["snaps"][label]["revisions"]["AnimationTime"]) ==
                   (snap["selection"], snap["camera"], snap["revisions"]["AnimationTime"]) for o in (pre, nor))
        check(same, f"{name}/{label}: snapshot matches pre-P4.7 and no-reuse")
    hashes = set()
    for shot, path in cand["shots"].items():
        h = sha(path)
        hashes.add(h)
        check(h == sha(pre["shots"][shot]) and h == sha(nor["shots"][shot]),
              f"{name}/{shot}: byte-identical to pre-P4.7 and no-reuse")
    check(len(hashes) == len(cand["shots"]), f"{name}: {len(hashes)} of {len(cand['shots'])} screenshots distinct")
    c = {k: v["revisions"] for k, v in cand["snaps"].items()}
    n = {k: v["revisions"] for k, v in nor["snaps"].items()}
    return c, n


def window(rev, before, after):
    a, b = rev[before], rev[after]
    return (b["AnimationTime"] - a["AnimationTime"], b["transformVisited"] - a["transformVisited"],
            b["transformRecomputed"] - a["transformRecomputed"])


if section("equivalence"):
    if not PRE_EXE.exists():
        check(False, f"pre-P4.7 binary missing: {PRE_EXE} (NOT RUN)")
    else:
        print("[hierarchy: animated root, non-uniform scale, camera and light under it, static sibling]")
        c, n = equivalence("hierarchy", f"""wait 8
snap loaded
animplay on
wait 10
snap play_a
shot @OUT@/hi_play_a.png
wait 12
snap play_b
shot @OUT@/hi_play_b.png
animplay off
wait 6
snap paused
click 750 783
wait 8
snap seek
shot @OUT@/hi_seek.png
click 155 783
wait 8
snap stopped
shot @OUT@/hi_stopped.png
picknode 5
wait 6
dclick 1052 163
wait 4
key a ctrl
type -3.0
key enter
wait 10
snap moved_static
shot @OUT@/hi_moved_static.png
click 815 60
wait 6
click 885 95
wait 10
animplay on
wait 10
snap attached_play
shot @OUT@/hi_attached_play.png
animplay off
wait 4
reload {HIER}
wait 12
snap reloaded
shot @OUT@/hi_reloaded.png
wait 2
""", [HIER, "--aa", "none", "--camera-distance", "10", "--camera-yaw", "20", "--camera-pitch", "15"])
        if c:
            ticks, visited, recomputed = window(c, "play_a", "play_b")
            check(ticks >= 10 and recomputed > 0 and recomputed < visited,
                  f"hierarchy play: {recomputed} of {visited} node visits recomputed over {ticks} ticks (static sibling skipped)")
            ticks_n, visited_n, recomputed_n = window(n, "play_a", "play_b")
            check(recomputed_n == visited_n, f"hierarchy no-reuse recomputes every visit ({recomputed_n}/{visited_n})")

        print("[grids: one animated leaf vs animated root, 1,600 drawables]")
        for name, model in (("leaf", LEAF), ("root", ROOT)):
            c, n = equivalence(name, f"""wait 8
snap loaded
animplay on
wait 10
snap play_a
shot @OUT@/{name}_play_a.png
wait 10
snap play_b
shot @OUT@/{name}_play_b.png
animplay off
wait 2
""", [model, "--aa", "none", "--camera-distance", "45"])
            if c:
                ticks, visited, recomputed = window(c, "play_a", "play_b")
                if name == "leaf":
                    check(ticks >= 10 and recomputed <= 2 * ticks and visited >= 1600 * ticks,
                          f"leaf: {recomputed} recomputed of {visited} visits over {ticks} ticks")
                    pushes = c["play_b"]["weightPushes"] - c["play_a"]["weightPushes"]
                    check(pushes <= 2 * ticks, f"leaf: {pushes} morph-weight pushes over {ticks} ticks (only the animated leaf)")
                else:
                    check(ticks >= 10 and recomputed >= 1601 * ticks,
                          f"root: every node recomputed each tick ({recomputed} of {visited} visits, {ticks} ticks)")

        print("[morph weights and skins: playback with morphing / skinning toggled]")
        c, n = equivalence("morphskin", f"""wait 8
animplay on
wait 10
snap morph_play
shot @OUT@/ms_morph_play.png
click 36 783
wait 6
click 58 780
wait 6
click 1150 400
wait 10
snap morph_off
shot @OUT@/ms_morph_off.png
click 36 783
wait 6
click 58 780
wait 6
click 1150 400
wait 10
snap morph_on
shot @OUT@/ms_morph_on.png
animplay off
wait 6
snap morph_paused
wait 10
snap morph_paused_idle
reload {FOX}
wait 12
animplay on
wait 10
snap fox_play
wait 10
snap fox_play_b
shot @OUT@/ms_fox_play.png
click 36 783
wait 6
click 58 752
wait 6
click 1150 400
wait 10
snap skin_off
shot @OUT@/ms_skin_off.png
click 36 783
wait 6
click 58 752
wait 6
click 1150 400
wait 10
snap skin_on
shot @OUT@/ms_skin_on.png
animplay off
wait 6
snap fox_paused
wait 10
snap fox_paused_idle
wait 2
""", [MORPH, "--aa", "none"])
        if c:
            ticks = c["fox_play_b"]["AnimationTime"] - c["fox_play"]["AnimationTime"]
            ups = c["fox_play_b"]["jointUploads"] - c["fox_play"]["jointUploads"]
            sup = c["fox_play_b"]["jointUploadsSuppressed"] - c["fox_play"]["jointUploadsSuppressed"]
            check(ticks >= 8 and ups == ticks and sup == ticks,
                  f"fox playing: {ups} joint uploads and {sup} suppressed over {ticks} ticks (second apply per tick is identical)")
            nu = n["fox_play_b"]["jointUploadsSuppressed"] - n["fox_play"]["jointUploadsSuppressed"]
            check(nu == 0, f"no-reuse run never suppresses joint uploads ({nu})")
            check(c["morph_paused_idle"]["weightPushes"] == c["morph_paused"]["weightPushes"],
                  "paused morph cube: no weight pushes")

        print("[skinned and pointer playback]")
        for name, model in (("fox", FOX), ("pointer", POINTER)):
            equivalence(name, f"""wait 8
animplay on
wait 10
shot @OUT@/{name}_a.png
wait 9
shot @OUT@/{name}_b.png
animplay off
wait 2
""", [model, "--aa", "none"])


def bench(exe, label, index):
    folder = OUT / "bench"
    folder.mkdir(parents=True, exist_ok=True)
    out = folder / f"{label}_{index}.json"
    script = folder / "play.txt"
    script.write_text("wait 8\nanimplay on\nwait 3000\n", encoding="utf-8")
    out.unlink(missing_ok=True)
    result = subprocess.run([str(exe), LEAF, "--isolated", "--window-size", "1280x800", "--aa", "none", "--vsync", "off",
                             "--profile", "--benchmark", "--benchmark-warmup", "120", "--benchmark-frames", "300",
                             "--benchmark-output", str(out), "--exit-after-benchmark",
                             "--ui-script", str(script), "--ui-report", str(folder / f"{label}_{index}_ui.json")],
                            cwd=REPO, capture_output=True, text=True, timeout=600)
    if not out.exists():
        check(False, f"benchmark {label} {index}: exit {result.returncode}")
        return None
    frames = json.loads(out.read_text(encoding="utf-8"))["measuredFrames"]
    def stage(name):
        return statistics.median([sum(s["ms"] for s in f["stages"] if s["name"] == name) for f in frames])
    playing = sum(1 for f in frames if f["counters"].get("applyAnimationCalls", 0) > 0)
    return stage("animation.update") + stage("scene.dirtySync"), statistics.median([f["cpuFrameMs"] for f in frames]), playing, len(frames)


if section("benchmark"):
    if not PRE_EXE.exists():
        check(False, f"pre-P4.7 binary missing: {PRE_EXE} (NOT RUN)")
    else:
        print(f"[benchmark: {LEAF} playing, {args.pairs} alternating pairs]")
        pre_t, cand_t, pre_c, cand_c = [], [], [], []
        for i in range(args.pairs):
            order = [("pre", PRE_EXE), ("candidate", EXE)] if i % 2 == 0 else [("candidate", EXE), ("pre", PRE_EXE)]
            res = {label: bench(exe, label, i) for label, exe in order}
            if res.get("pre") and res.get("candidate"):
                pre_t.append(res["pre"][0]); cand_t.append(res["candidate"][0])
                pre_c.append(res["pre"][1]); cand_c.append(res["candidate"][1])
                print(f"    pair {i}: animation update+apply {res['pre'][0]:.3f} -> {res['candidate'][0]:.3f} ms, "
                      f"cpu frame {res['pre'][1]:.3f} -> {res['candidate'][1]:.3f} ms, "
                      f"playing frames {res['candidate'][2]}/{res['candidate'][3]}")
        if pre_t:
            noise = (max(pre_t) - min(pre_t)) / statistics.median(pre_t)
            deltas = [(p - c) / p for p, c in zip(pre_t, cand_t)]
            med = statistics.median(deltas)
            print(f"    baseline noise {noise:.1%}, median paired improvement {med:.1%}, "
                  f"improved in {sum(1 for d in deltas if d > 0)}/{len(deltas)} pairs")
            check(med > max(0.03, noise) and sum(1 for d in deltas if d > 0) >= len(deltas) - 1,
                  "animation transform cost improvement exceeds 3% and baseline noise")
            cpu = statistics.median([(p - c) / p for p, c in zip(pre_c, cand_c)])
            check(cpu > -0.05, f"no CPU frame regression beyond 5% (median paired change {cpu:+.1%})")
            (OUT / "bench" / "summary.json").write_text(json.dumps({"pre_ms": pre_t, "candidate_ms": cand_t,
                "pre_cpu": pre_c, "candidate_cpu": cand_c, "noise": noise, "median_improvement": med}, indent=2), encoding="utf-8")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
