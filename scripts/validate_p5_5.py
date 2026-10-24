# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P5.5 conservative frustum culling validation.

createScene leaves out static primitives whose local POSITION box, transformed by
the exact model-view-projection, lies outside one clip half-space. Skinned,
morphed, non-float and bound-less primitives are kept.

Sections:
  equivalence  Each UI script runs three times: the pre-P5.5 binary
               (build/p5.5/pre/Glider.exe, i.e. P5.4), the candidate with
               --disable-culling, and the candidate. Selections, cameras and
               screenshots must be identical; the candidate must actually cull
               where the view leaves geometry outside, and keep deformed meshes.
  benchmark    Per-redraw cost (--disable-redraw-skip in both builds) on the
               1,600-drawable grid seen from close up (most outside the view),
               and the whole grid in view as the control.
"""

import argparse
import hashlib
import json
import statistics
import harness_process as subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
PRE_EXE = REPO / "build" / "p5.5" / "pre" / "Glider.exe"
OUT = REPO / "build" / "p5.5"
CULL = "build/p5.5/fixtures/culling.gltf"
MANY = "build/p5.5/fixtures/many_drawables.gltf"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
HIER = "resources/test_models/hierarchy_animation_fixture.gltf"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
MORPH = "OpenGL-Renderer/Tools/tests/test_resources/AnimatedMorphCube.gltf"
CULLWIND = "build/p3.4/fixtures/cull_winding_fixture.gltf"  # fixed output path of its generator
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["equivalence", "benchmark"])
parser.add_argument("--pairs", type=int, default=5)
parser.add_argument("--scenario", help="run only equivalence scenarios whose name starts with this")
args = parser.parse_args()
failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


subprocess.run([sys.executable, str(REPO / "scripts" / "generate_culling_fixture.py"), str(REPO / CULL)], check=True)
subprocess.run([sys.executable, str(REPO / "scripts" / "generate_many_drawables_fixture.py"), str(REPO / MANY), "40"],
               check=True, capture_output=True)
subprocess.run([sys.executable, str(REPO / "scripts" / "generate_cull_winding_fixture.py")], cwd=REPO,
               check=True, capture_output=True)


def run(mode, exe, name, text, extra):
    folder = OUT / mode
    folder.mkdir(parents=True, exist_ok=True)
    text = text.replace("@OUT@", folder.relative_to(REPO).as_posix())
    shots = [line.split(" ", 1)[1] for line in text.splitlines() if line.startswith("shot ")]
    for shot in shots:
        (REPO / shot).unlink(missing_ok=True)
    scenario, report = folder / f"{name}.txt", folder / f"{name}.json"
    scenario.write_text(text, encoding="utf-8")
    report.unlink(missing_ok=True)
    flags = ["--disable-culling"] if mode == "nocull" else []
    result = subprocess.run([str(exe)] + extra + COMMON + flags + ["--ui-script", str(scenario), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=300)
    if result.returncode != 0 or not report.exists():
        check(False, f"{name} [{mode}]: exit {result.returncode} {result.stderr[-300:]}")
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    if data.get("failed", True):
        check(False, f"{name} [{mode}]: script errors {data.get('errors')}")
        return None
    return {"snaps": {s["label"]: s for s in data["snaps"]}, "order": [s["label"] for s in data["snaps"]],
            "shots": {Path(p).name: REPO / p for p in shots}}


def equivalence(name, lines, extra, expect_culled=True, expect_uncullable=False, same_binary_only=()):
    """same_binary_only: shots compared only with --disable-culling, e.g. TAA after a hold,
    whose jitter phase the P5.3 amendment (skipFrame) changed relative to older binaries."""
    if args.scenario and not name.startswith(args.scenario):
        return None
    lines = [line.replace("@OUT@/", f"@OUT@/{name}_") if line.startswith("shot ") and "@OUT@/" + name not in line else line
             for line in lines]
    text = "\n".join(lines + ["wait 2"]) + "\n"
    pre = run("pre", PRE_EXE, name, text, extra)
    noc = run("nocull", EXE, name, text, extra)
    cand = run("candidate", EXE, name, text, extra)
    if not (pre and noc and cand):
        return None
    for label, snap in cand["snaps"].items():
        same = all((o["snaps"][label]["selection"], o["snaps"][label]["camera"]) == (snap["selection"], snap["camera"])
                   for o in (pre, noc))
        check(same, f"{name}/{label}: selection and camera match pre and --disable-culling")
    for shot, path in cand["shots"].items():
        h = sha(path)
        if any(shot.endswith(s) for s in same_binary_only):
            check(h == sha(noc["shots"][shot]), f"{name}/{shot}: byte-identical to --disable-culling (TAA after a hold)")
            continue
        check(h == sha(pre["shots"][shot]) and h == sha(noc["shots"][shot]),
              f"{name}/{shot}: byte-identical to pre-P5.5 and to --disable-culling")
    last = cand["snaps"][cand["order"][-1]]["revisions"]
    nlast = noc["snaps"][noc["order"][-1]]["revisions"]
    check(nlast["culled"] == 0, f"{name}: --disable-culling culls nothing ({nlast['culled']})")
    if expect_culled:
        check(last["culled"] > 0, f"{name}: primitives culled ({last['culled']} over all redraws)")
    if expect_uncullable:
        check(last["uncullable"] > 0 and last["culled"] == 0,
              f"{name}: deformed meshes kept, never culled ({last['uncullable']} kept, {last['culled']} culled)")
    return cand


def views_script(prefix):
    lines = ["wait 10", f"snap {prefix}start", f"shot @OUT@/{prefix}start.png",
             "click 296 60", "wait 8", f"snap {prefix}ortho", f"shot @OUT@/{prefix}ortho.png",
             "click 296 60", "wait 8",
             "move 650 400", "wait 2", "wheel -4", "wait 8", f"snap {prefix}out", f"shot @OUT@/{prefix}out.png",
             "wheel 8", "wait 8", f"snap {prefix}in", f"shot @OUT@/{prefix}in.png",
             "click 968 120", "wait 8", f"snap {prefix}iso", f"shot @OUT@/{prefix}iso.png"]
    for i, (x, y) in enumerate((x, y) for y in range(150, 720, 115) for x in range(330, 980, 130)):
        lines += [f"click {x} {y}", "wait 4", f"snap {prefix}pick{i}"]
    lines += ["resize 1000 650", "wait 10", f"snap {prefix}resized", f"shot @OUT@/{prefix}resized.png"]
    return lines


if args.only in (None, "equivalence"):
    if not PRE_EXE.exists():
        check(False, f"pre-P5.5 binary missing: {PRE_EXE} (NOT RUN)")
    else:
        # Wide scene from inside: perspective and orthographic, zoomed out and in past the
        # near plane, six directions, negative / non-uniform scale, scaled parents, picks.
        for yaw, pitch in ((20, 5), (110, 15), (200, -10), (290, 40), (45, 80), (160, -60)):
            name = f"wide_y{yaw}_p{pitch}"
            print(f"[culling fixture from inside, yaw {yaw}, pitch {pitch}]")
            equivalence(name, views_script(name + "_"), [CULL, "--aa", "none", "--camera-target", "0,1,0",
                                                   "--camera-distance", "3", "--camera-yaw", str(yaw),
                                                   "--camera-pitch", str(pitch)])

        print("[culling fixture, TAA (jittered projection) and wireframe overlay]")
        equivalence("wide_taa", ["wait 80", "snap a", "shot @OUT@/taa.png", "vpsetting 1 wireframe on", "wait 80",
                                 "snap b", "shot @OUT@/taa_overlay.png"],
                    [CULL, "--aa", "taa", "--camera-target", "0,1,0", "--camera-distance", "3", "--camera-yaw", "20"],
                    same_binary_only=("taa_overlay.png",))

        print("[1,600-drawable grid from close up, picks]")
        lines = ["wait 10", "shot @OUT@/many_close.png"]
        for i, (x, y) in enumerate((x, y) for y in range(160, 720, 110) for x in range(330, 960, 120)):
            lines += [f"click {x} {y}", "wait 4", f"snap pick{i}"]
        equivalence("many_close", lines + ["shot @OUT@/many_close_last.png"], [MANY, "--aa", "none", "--camera-distance", "4"])

        print("[transmission: quads partly out of view (camera target shifted)]")
        for target in ("1.6,0,0", "-1.6,0.3,0", "0,-1.4,0", "0,1.6,0"):
            equivalence(f"transmission_{target}", ["wait 10", "snap a", "shot @OUT@/tr.png", "click 650 418", "wait 6",
                                                    "snap picked", "vpsetting 1 wireframe on", "wait 8", "shot @OUT@/tr_overlay.png"],
                        [FIXTURE, "--aa", "msaa4", "--camera-yaw", "160", "--camera-pitch", "10",
                         "--camera-target", target, "--camera-distance", "2"])

        print("[mirrored / single-sided planes (cull-winding fixture), edges of the view]")
        for target in ("2.5,0,0", "-2.5,0,0"):
            equivalence(f"cullwind_{target}", ["wait 10", "snap a", "shot @OUT@/cw.png", "vpsetting 1 doublesided on", "wait 6",
                                               "snap b", "shot @OUT@/cw_ds.png"],
                        [CULLWIND, "--aa", "none", "--camera-target", target, "--camera-distance", "2"])

        print("[hierarchy fixture: animated, non-uniformly scaled parent; playback]")
        equivalence("hierarchy", ["wait 8", "animplay on", "wait 30", "snap play", "shot @OUT@/hi_play.png", "animplay off",
                                  "wait 6", "shot @OUT@/hi_paused.png"],
                    [HIER, "--aa", "none", "--camera-distance", "4", "--camera-yaw", "60"])

        print("[deformed meshes stay uncullable: skinned Fox, morph cube]")
        equivalence("fox", ["wait 8", "animplay on", "wait 20", "snap play", "shot @OUT@/fox.png"],
                    [FOX, "--aa", "none"], expect_culled=False, expect_uncullable=True)
        equivalence("morph", ["wait 8", "animplay on", "wait 20", "snap play", "shot @OUT@/morph.png"],
                    [MORPH, "--aa", "none"], expect_culled=False, expect_uncullable=True)


def bench(exe, label, index, distance):
    folder = OUT / "bench"
    folder.mkdir(parents=True, exist_ok=True)
    out = folder / f"d{distance}_{label}_{index}.json"
    out.unlink(missing_ok=True)
    subprocess.run([str(exe), MANY, "--isolated", "--window-size", "1280x800", "--aa", "none", "--vsync", "off",
                    "--camera-distance", str(distance), "--disable-redraw-skip",
                    "--profile", "--benchmark", "--benchmark-warmup", "120", "--benchmark-frames", "300",
                    "--benchmark-output", str(out), "--exit-after-benchmark"], cwd=REPO, capture_output=True, timeout=600)
    data = json.loads(out.read_text(encoding="utf-8"))
    frames = data["measuredFrames"]
    measured = {f["frame"] for f in frames}
    gpu = {}
    for sample in data["gpu"]["completed"]:
        if sample["frame"] in measured:
            gpu[sample["frame"]] = gpu.get(sample["frame"], 0.0) + sum(p["ms"] for p in sample["passes"] if p.get("available"))
    return {"cpu": statistics.median(f["cpuFrameMs"] for f in frames),
            "submit": statistics.median(sum(s["ms"] for s in f["stages"] if s["name"] == "draw.submit") for f in frames),
            "gpu": statistics.median(gpu.values()) if gpu else None,
            "drawables": frames[-1]["counters"].get("sceneDrawables")}


if args.only in (None, "benchmark"):
    # 4: most of the grid outside the view; 30: about a quarter outside; 70: all in view, so
    # culling removes nothing and only its test cost remains (the control).
    for distance, gate in ((4, "win"), (30, None), (70, "control")):
        print(f"[benchmark: 1,600 drawables at camera distance {distance}, every frame redrawn, {args.pairs} pairs]")
        pre, cand = [], []
        for i in range(args.pairs):
            order = [("pre", PRE_EXE), ("candidate", EXE)] if i % 2 == 0 else [("candidate", EXE), ("pre", PRE_EXE)]
            res = {label: bench(exe, label, i, distance) for label, exe in order}
            pre.append(res["pre"]); cand.append(res["candidate"])
            print(f"    pair {i}: cpu {res['pre']['cpu']:.3f} -> {res['candidate']['cpu']:.3f} ms, drawables "
                  f"{res['pre']['drawables']} -> {res['candidate']['drawables']}")
        for key in ("cpu", "submit", "gpu"):
            p, c = [r[key] for r in pre], [r[key] for r in cand]
            if None in p + c:
                continue
            noise = (max(p) - min(p)) / statistics.median(p)
            deltas = [(a - b) / a for a, b in zip(p, c)]
            med = statistics.median(deltas)
            print(f"    {key}: pre {min(p):.3f}-{max(p):.3f} -> cand {min(c):.3f}-{max(c):.3f} ms, "
                  f"median paired {med:+.1%} improvement, {sum(d > 0 for d in deltas)}/{len(deltas)}, noise {noise:.1%}")
            if key == "cpu" and gate == "win":
                check(med > max(0.03, noise) and sum(d > 0 for d in deltas) >= len(deltas) - 1,
                      "close-up CPU frame: improvement exceeds 3% and noise")
            if key == "cpu" and gate == "control":
                check(all(r["drawables"] == p_["drawables"] for r, p_ in zip(cand, pre)),
                      "whole grid in view: nothing culled")
                check(med > -max(0.05, noise), "whole grid in view: no CPU regression beyond 5% and noise")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
