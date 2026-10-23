# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P5.2 shared 1x1 picking target validation.

On a click frame the picking copies are drawn alone into one shared 1x1 target
through a pixel-window projection W * P (P = the scene's projection, jittered
under TAA). Per-viewport picking storage is gone.

Sections:
  equivalence  Each UI script runs three times: the pre-P5.2 binary
               (build/p5.2/pre/Glider.exe, i.e. P5.1), the candidate with
               --full-size-picking (the P5.1 path), and the candidate.
               Selections, cameras and screenshots must be identical. Cases
               cover the P5.1 set plus Y conversion (top/bottom rows, outer
               columns), thin geometry (wireframe mode), textured alpha masks,
               orthographic projection and 1-pixel edge steps.
  memory       Per-viewport picking storage from the scenario report.
  benchmark    Click-frame cost: picking stage CPU time and GPU picking pass
               time on the 1,600-drawable fixture, per click, pre vs candidate.
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
PRE_EXE = REPO / "build" / "p5.2" / "pre" / "Glider.exe"
OUT = REPO / "build" / "p5.2"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
HIER = "resources/test_models/hierarchy_animation_fixture.gltf"
ALPHA = "OpenGL-Renderer/Tools/tools/operations/tests/resources/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
MANY = "build/p5.2/fixtures/many_drawables.gltf"
BEHIND = ["--camera-yaw", "160", "--camera-pitch", "10"]
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05"]
# Default 1280x800 layout: the viewport image is 734x696 at (284, 70). The outer
# 4 px columns belong to the dock splitters' hover zone, so x 288..1013 and
# y 70..765 (bottom row: py = 0; top row: py = 695) are clickable.
LEFT, RIGHT, TOP, BOTTOM = 288, 1013, 70, 765

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["equivalence", "memory", "benchmark"])
parser.add_argument("--pairs", type=int, default=5)
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


subprocess.run([sys.executable, str(REPO / "scripts" / "generate_many_drawables_fixture.py"), str(REPO / MANY), "40"],
               check=True, capture_output=True)


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
    flags = ["--full-size-picking"] if mode == "full" else []
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


def equivalence(name, script, extra, clicks, idle=()):
    pre = run("pre", PRE_EXE, name, script, extra)
    full = run("full", EXE, name, script, extra)
    cand = run("candidate", EXE, name, script, extra)
    if not (pre and full and cand):
        return None
    for label, snap in cand["snaps"].items():
        same = all((o["snaps"][label]["selection"], o["snaps"][label]["camera"]) == (snap["selection"], snap["camera"])
                   for o in (pre, full))
        check(same, f"{name}/{label}: selection and camera match pre and --full-size-picking ({snap['selection'].split('|')[1]})")
    for shot, path in cand["shots"].items():
        h = sha(path)
        check(h == sha(pre["shots"][shot]) and h == sha(full["shots"][shot]),
              f"{name}/{shot}: byte-identical to pre-P5.2 and to --full-size-picking")
    rev = {k: v["revisions"] for k, v in cand["snaps"].items()}
    last = rev[list(rev)[-1]]
    check(last["pickUnserved"] == 0, f"{name}: no click went unserved ({last['pickUnserved']})")
    check(last["pickReads"] == clicks and last["pickingRenders"] == clicks,
          f"{name}: {clicks} viewport clicks -> {last['pickReads']} reads, {last['pickingRenders']} picking renders")
    for before, after in idle:
        renders = rev[after]["pickingRenders"] - rev[before]["pickingRenders"]
        check(renders == 0, f"{name} {before}->{after}: idle frames render no picking pass ({renders})")
    return cand


def grid(xs, ys, prefix, wait=6, gap_ms=0):
    """gap_ms > 0 spaces clicks closer than ImGui's double-click distance beyond its
    wall-clock double-click time, so none becomes a focusing double-click."""
    lines = []
    for i, (x, y) in enumerate((x, y) for y in ys for x in xs):
        lines += [f"click {x} {y}", f"wait {wait}", f"snap {prefix}{i}"] + ([f"waitms {gap_ms}"] if gap_ms else [])
    return lines, len(xs) * len(ys)


def nodes_of(cand, prefix=""):
    return {s["selection"].split("|")[1] for k, s in cand["snaps"].items() if k.startswith(prefix)}


if section("equivalence"):
    if not PRE_EXE.exists():
        check(False, f"pre-P5.2 binary missing: {PRE_EXE} (NOT RUN)")
    else:
        print("[1,600 drawables: 6x6 grid; top/bottom rows and outer columns (Y conversion)]")
        lines, n = grid(range(330, 960, 120), range(160, 720, 110), "pick")
        rows, nr = grid(range(LEFT, RIGHT + 1, 65), [TOP, BOTTOM], "row")
        cols, nc = grid([LEFT, RIGHT], range(TOP, BOTTOM + 1, 70), "col")
        cand = equivalence("many", "\n".join(["wait 8", "snap idle0", "wait 30", "snap idle1"] + lines + rows + cols
                                             + ["shot @OUT@/many_last.png", "wait 2"]) + "\n",
                           [MANY, "--aa", "none", "--camera-distance", "14"], n + nr + nc, idle=[("idle0", "idle1")])
        if cand:
            top = {s["selection"] for k, s in cand["snaps"].items() if k.startswith("row") and int(k[3:]) < nr // 2}
            bottom = {s["selection"] for k, s in cand["snaps"].items() if k.startswith("row") and int(k[3:]) >= nr // 2}
            check(len(nodes_of(cand)) >= 20, f"many: {len(nodes_of(cand))} distinct selections")
            check(len(top - bottom) > 0 and len(bottom - top) > 0,
                  f"many: top and bottom rows pick different nodes ({len(top)} / {len(bottom)})")

        for aa in ("none", "msaa4", "taa"):
            print(f"[production fixture from behind: overlap grid and 1-px edge steps, AA {aa}]")
            lines, n = grid(range(360, 960, 75), range(250, 600, 70), "pick")
            edge, ne = grid(list(range(480, 524, 2)) + list(range(734, 752, 1)), [450], "edge", gap_ms=400)
            vedge, nv = grid([600], list(range(296, 312, 1)) + list(range(570, 582, 1)), "vedge", gap_ms=400)
            script = ["wait 10"] + lines + edge + vedge + ["dclick 650 418", "wait 10", "snap focused",
                                                           "shot @OUT@/fx_" + aa + ".png", "wait 2"]
            cand = equivalence(f"fixture_{aa}", "\n".join(script) + "\n", [FIXTURE, "--aa", aa] + BEHIND, n + ne + nv + 2)
            if cand:
                check({"node=OpaqueQuadNode", "node=BlendQuadNode", "node=TransmissiveQuadNode", "node="} <= nodes_of(cand),
                      f"fixture_{aa}: clicks hit every quad and the background")

        print("[production fixture, orthographic, from behind]")
        lines, n = grid(range(360, 960, 50), range(250, 600, 50), "pick")
        cand = equivalence("ortho", "\n".join(["wait 10"] + lines + ["shot @OUT@/ortho.png", "wait 2"]) + "\n",
                           [FIXTURE, "--aa", "none", "--camera-projection", "orthographic"] + BEHIND, n)
        if cand:
            check(len(nodes_of(cand)) >= 3, f"ortho: {sorted(nodes_of(cand))}")

        print("[thin geometry: wireframe render mode, dense grid]")
        lines, n = grid(range(470, 860, 9), range(300, 600, 37), "pick")
        cand = equivalence("wireframe", "\n".join(["wait 10"] + lines + ["shot @OUT@/wire.png", "wait 2"]) + "\n",
                           [FIXTURE, "--aa", "none", "--render-mode", "wireframe"] + BEHIND, n)
        if cand:
            check(len(nodes_of(cand)) >= 2, f"wireframe: lines picked {sorted(nodes_of(cand))}")

        print("[textured alpha masks: AlphaBlendModeTest]")
        lines, n = grid(range(300, 1010, 14), range(300, 520, 44), "pick")
        cand = equivalence("alpha", "\n".join(["wait 10"] + lines + ["shot @OUT@/alpha.png", "wait 2"]) + "\n",
                           [ALPHA, "--aa", "none"], n)
        if cand:
            hit = nodes_of(cand)
            print(f"    alpha nodes: {sorted(hit)}")
            check(len(hit & {"node=TestCutoff25", "node=TestCutoff75", "node=TestCutoffDefault"}) >= 2,
                  "alpha: mask-cutoff bars picked")

        print("[skinned Fox: clicks during playback]")
        lines, n = grid(range(420, 900, 30), range(300, 560, 40), "pick", wait=3)
        cand = equivalence("fox", "\n".join(["wait 8", "animplay on", "wait 4"] + lines + ["shot @OUT@/fox.png", "animplay off", "wait 2"]) + "\n",
                           [FOX, "--aa", "none"], n)
        if cand:
            check(len(nodes_of(cand)) >= 2, f"fox: {sorted(nodes_of(cand))}")

        print("[hierarchy fixture: playback, wheel zoom, resize]")
        lines_a, na = grid(range(420, 860, 40), range(290, 490, 40), "play", wait=3)
        lines_b, nb = grid(range(400, 900, 125), range(250, 600, 120), "zoom", wait=3)
        lines_c, nc = grid(range(350, 850, 125), range(220, 520, 100), "resized", wait=3)
        zoomed = []
        for line in lines_b:
            if line.startswith("click"):
                zoomed.append("wheel 1")
            zoomed.append(line)
        script = (["wait 8", "animplay on", "wait 4"] + lines_a + ["shot @OUT@/hi_play.png", "animplay off", "wait 4"]
                  + ["move 640 400", "wait 2"] + zoomed + ["shot @OUT@/hi_zoom.png", "resize 1100 700", "wait 10"]
                  + lines_c + ["shot @OUT@/hi_resized.png", "wait 2"])
        cand = equivalence("hierarchy", "\n".join(script) + "\n", [HIER, "--aa", "none"], na + nb + nc)
        if cand:
            check(bool(nodes_of(cand, "play") & {"node=AnimatedRoot", "node=ScaledChild", "node=GrandChild"}),
                  f"hierarchy: playback clicks hit animated geometry ({sorted(nodes_of(cand, 'play'))})")

        print("[two live viewports (viewport 2 double-sided), clicks in both]")
        lines, n = grid(range(320, 980, 40), range(260, 560, 75), "pick")
        script = (["wait 8", "vpcreate", "wait 10", "vpsplit 2", "wait 10", "vpsetting 2 doublesided on", "wait 6",
                   "snap split_idle0", "wait 20", "snap split_idle1"] + lines + ["shot @OUT@/split.png", "wait 2"])
        cand = equivalence("split", "\n".join(script) + "\n", [FIXTURE, "--aa", "none"] + BEHIND, n,
                           idle=[("split_idle0", "split_idle1")])
        if cand:
            check(len(nodes_of(cand)) >= 3, f"split: {sorted(nodes_of(cand))}")


if section("memory"):
    print("[per-viewport picking storage from the scenario report, two viewports]")
    for mode, flags in (("candidate", []), ("full", ["--full-size-picking"])):
        out = OUT / "memory" / f"{mode}.json"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.unlink(missing_ok=True)
        subprocess.run([str(EXE), FIXTURE, "--aa", "none"] + COMMON + flags +
                       ["--capture-viewport", str(OUT / "memory" / f"{mode}.png"), "--capture-frame", "10",
                        "--scenario-report", str(out), "--profile-report", str(OUT / "memory" / f"{mode}_profile.json"),
                        "--exit-after-profile"], cwd=REPO, capture_output=True, timeout=120)
        vp = json.loads(out.read_text(encoding="utf-8"))["viewport"]
        bytes_ = vp["pickingWidth"] * vp["pickingHeight"] * 8  # RGBA8 + D24S8
        print(f"    {mode}: viewport {vp['width']}x{vp['height']}, picking {vp['pickingWidth']}x{vp['pickingHeight']} = {bytes_} bytes")
        if mode == "candidate":
            check(bytes_ == 0, "candidate: no per-viewport picking storage (one shared 1x1 target, 8 bytes)")
        else:
            check(vp["pickingWidth"] == vp["width"] and vp["pickingHeight"] == vp["height"],
                  "--full-size-picking: per-viewport buffer at viewport size")


def click_bench(exe, label, index):
    """Click frames on the 1,600-drawable fixture: picking CPU stage and GPU picking passes per click."""
    folder = OUT / "bench"
    folder.mkdir(parents=True, exist_ok=True)
    lines = ["wait 30"]
    for i, (x, y) in enumerate((x, y) for y in range(200, 700, 100) for x in range(350, 950, 100)):
        lines += [f"click {x} {y}", "wait 4"]
    lines.append("wait 200")  # the profile report is written after the capture at frame 400
    script, report, prof = folder / f"s_{label}_{index}.txt", folder / f"r_{label}_{index}.json", folder / f"p_{label}_{index}.json"
    script.write_text("\n".join(lines) + "\n", encoding="utf-8")
    for f in (report, prof):
        f.unlink(missing_ok=True)
    subprocess.run([str(exe), MANY, "--aa", "none", "--camera-distance", "30", "--vsync", "off", "--profile",
                    "--profile-report", str(prof),
                    "--capture-viewport", str(folder / f"c_{label}_{index}.png"), "--capture-frame", "400"] + COMMON + ["--ui-script", str(script), "--ui-report", str(report)],
                   cwd=REPO, capture_output=True, timeout=300)
    data = json.loads(prof.read_text(encoding="utf-8"))
    frames = data.get("frames") or data.get("measuredFrames") or []
    click_frames = [f for f in frames if f.get("counters", {}).get("pickingBuilds", 0) > 0]
    cpu = [sum(s["ms"] for s in f["stages"] if s["name"] in ("picking.build", "picking.draw")) +
           (sum(s["ms"] for s in f["stages"] if s["name"] == "draw.submit") if label == "pre" else 0.0)
           for f in click_frames]
    frame_cpu = [f["cpuFrameMs"] for f in click_frames]
    ids = {f["frame"] for f in click_frames}
    gpu = []
    for sample in data["gpu"]["completed"]:
        if sample["frame"] in ids:
            gpu.append(sum(p["ms"] for p in sample["passes"] if p.get("available") and p["name"].startswith("Picking/")))
    return {"clicks": len(click_frames), "frame_cpu": statistics.median(frame_cpu) if frame_cpu else None,
            "gpu_picking": statistics.median(gpu) if gpu else None}


if section("benchmark"):
    print(f"[click-frame cost, 1,600 drawables, {args.pairs} alternating pairs]")
    pre, cand = [], []
    for i in range(args.pairs):
        order = [("pre", PRE_EXE), ("candidate", EXE)] if i % 2 == 0 else [("candidate", EXE), ("pre", PRE_EXE)]
        res = {label: click_bench(exe, label, i) for label, exe in order}
        pre.append(res["pre"]); cand.append(res["candidate"])
        print(f"    pair {i}: click frames {res['pre']['clicks']}/{res['candidate']['clicks']}, "
              f"click-frame CPU {res['pre']['frame_cpu']:.3f} -> {res['candidate']['frame_cpu']:.3f} ms, "
              f"GPU picking {res['pre']['gpu_picking']} -> {res['candidate']['gpu_picking']} ms")
    for key in ("frame_cpu", "gpu_picking"):
        p = [r[key] for r in pre]
        c = [r[key] for r in cand]
        if any(v is None for v in p + c):
            print(f"    {key}: not available")
            continue
        noise = (max(p) - min(p)) / statistics.median(p)
        deltas = [(a - b) / a for a, b in zip(p, c)]
        print(f"    {key}: pre {min(p):.3f}-{max(p):.3f} -> cand {min(c):.3f}-{max(c):.3f} ms, "
              f"median paired {statistics.median(deltas):+.1%} improvement, {sum(d > 0 for d in deltas)}/{len(deltas)}, noise {noise:.1%}")
        if key == "frame_cpu":
            check(statistics.median(deltas) > -max(0.05, noise), "click-frame CPU: no regression beyond 5% and noise")
    (OUT / "bench" / "summary.json").write_text(json.dumps({"pre": pre, "candidate": cand}, indent=2), encoding="utf-8")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
