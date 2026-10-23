# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P8.5 hybrid picking validation.

On a click frame, GltfRenderLayer::cullToClip removes the static primitives whose
POSITION bounds lie entirely outside the clicked pixel's frustum (the pixel-window
projection W * P times the view matrix) before the 1x1 picking pass. Skinned,
morphed and bound-less primitives are always drawn, so the GPU still decides the
pick among every primitive that can cover the pixel.

Sections:
  equivalence  Each UI script runs three times: the P8 baseline binary
               (build/p8/Glider_base.exe), the candidate with
               --disable-pick-culling (the P5.2 path), and the candidate.
               Selections, cameras and screenshots must be identical and the
               candidate must cull on the large fixtures.
  benchmark    Click-frame cost on the 1,600- and 10,000-drawable grids:
               picking.draw CPU stage and GPU picking pass, baseline vs candidate.
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
PRE_EXE = REPO / "build" / "p8" / "Glider_base.exe"
OUT = REPO / "build" / "p8" / "p8.5"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
HIER = "resources/test_models/hierarchy_animation_fixture.gltf"
ALPHA = "OpenGL-Renderer/Tools/tools/operations/tests/resources/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
MANY = "build/p8/fixtures/many_1600.gltf"
HUGE = "build/p8/fixtures/many_10000.gltf"
E195 = Path(r"F:/new-fss-aircraft-ejet/msfs24-projects/fss-aircraft-e195/SimObjects/Airplanes/E195/model/E195-Cockpit-LOD00.gltf")
BEHIND = ["--camera-yaw", "160", "--camera-pitch", "10"]
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05"]
# Default 1280x800 layout: clickable viewport pixels are x 288..1013, y 70..765.
LEFT, RIGHT, TOP, BOTTOM = 288, 1013, 70, 765

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["equivalence", "benchmark"])
parser.add_argument("--pairs", type=int, default=5)
parser.add_argument("--pre-exe", help="reference binary (default: build/p8/Glider_base.exe)")
parser.add_argument("--case", action="append", help="run only equivalence cases whose name starts with this (repeatable)")
args = parser.parse_args()
if args.pre_exe:
    PRE_EXE = Path(args.pre_exe).resolve()
failures = []


def section(name):
    return args.only in (None, name)


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


for path, size in ((MANY, "40"), (HUGE, "100")):
    (REPO / path).parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(REPO / "scripts" / "generate_many_drawables_fixture.py"), str(REPO / path), size],
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
    flags = ["--disable-pick-culling"] if mode == "nocull" else []
    result = subprocess.run([str(exe)] + extra + COMMON + flags + ["--ui-script", str(scenario), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=600)
    if result.returncode != 0 or not report.exists():
        check(False, f"{name} [{mode}]: exit {result.returncode} {result.stderr[-300:]}")
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    if data.get("failed", True):
        check(False, f"{name} [{mode}]: script errors {data.get('errors')}")
        return None
    return {"snaps": {s["label"]: s for s in data["snaps"]}, "shots": {Path(p).name: REPO / p for p in shots}}


def wanted(name):
    return not args.case or any(name.startswith(c) for c in args.case)


def equivalence(name, script, extra, clicks, expect_culled=False):
    if not wanted(name):
        return None
    pre = run("pre", PRE_EXE, name, script, extra)
    nocull = run("nocull", EXE, name, script, extra)
    cand = run("candidate", EXE, name, script, extra)
    if not (pre and nocull and cand):
        return None
    mismatches = [label for label, snap in cand["snaps"].items()
                  if any((o["snaps"][label]["selection"], o["snaps"][label]["camera"]) != (snap["selection"], snap["camera"])
                         for o in (pre, nocull))]
    check(not mismatches, f"{name}: {len(cand['snaps'])} snapshots, selection and camera match baseline and "
                          f"--disable-pick-culling (mismatches: {mismatches[:5]})")
    for shot, path in cand["shots"].items():
        h = sha(path)
        check(h == sha(pre["shots"][shot]) and h == sha(nocull["shots"][shot]),
              f"{name}/{shot}: byte-identical to baseline and to --disable-pick-culling")
    last = list(cand["snaps"].values())[-1]["revisions"]
    pre_last = list(pre["snaps"].values())[-1]["revisions"]
    # A click can land where the baseline also reads nothing (e.g. on the viewport
    # toolbar), so the read count is compared with the baseline, not the click count.
    check(last["pickUnserved"] == 0 and last["pickReads"] == pre_last["pickReads"] and last["pickReads"] >= clicks - 2,
          f"{name}: {clicks} clicks -> {last['pickReads']} reads (baseline {pre_last['pickReads']}), {last['pickUnserved']} unserved")
    nocull_last = list(nocull["snaps"].values())[-1]["revisions"]
    check(nocull_last.get("pickCulled", 0) == 0, f"{name}: --disable-pick-culling culls nothing")
    culled = last.get("pickCulled", 0)
    print(f"    {name}: {culled} drawables culled over {clicks} clicks")
    if expect_culled:
        check(culled > 0, f"{name}: candidate culls picking drawables ({culled})")
    return cand


def grid(xs, ys, prefix, wait=6, spaced=False):
    """spaced clicks wait 12 fixed frames (0.6 s of --fixed-frame-time), beyond ImGui's
    double-click time, so nearby clicks never become a focusing double-click. Frame
    waits, unlike wall-clock waits, keep TAA history and screenshots reproducible."""
    lines = []
    for i, (x, y) in enumerate((x, y) for y in ys for x in xs):
        lines += [f"click {x} {y}", f"wait {12 if spaced else wait}", f"snap {prefix}{i}"]
    return lines, len(xs) * len(ys)


def nodes_of(cand, prefix=""):
    return {s["selection"].split("|")[1] for k, s in cand["snaps"].items() if k.startswith(prefix)}


if section("equivalence"):
    if not PRE_EXE.exists():
        check(False, f"baseline binary missing: {PRE_EXE} (NOT RUN)")
    else:
        print("[1,600 drawables: grid, top/bottom rows and outer columns]")
        lines, n = grid(range(330, 960, 60), range(160, 720, 55), "pick")
        rows, nr = grid(range(LEFT, RIGHT + 1, 65), [TOP, BOTTOM], "row")
        cols, nc = grid([LEFT, RIGHT], range(TOP, BOTTOM + 1, 70), "col")
        cand = equivalence("many", "\n".join(["wait 8"] + lines + rows + cols + ["shot @OUT@/many.png", "wait 2"]) + "\n",
                           [MANY, "--aa", "none", "--camera-distance", "14"], n + nr + nc, expect_culled=True)
        if cand:
            check(len(nodes_of(cand)) >= 50, f"many: {len(nodes_of(cand))} distinct selections")

        print("[10,000 drawables: whole grid in view]")
        lines, n = grid(range(330, 960, 90), range(160, 720, 80), "pick")
        cand = equivalence("huge", "\n".join(["wait 8"] + lines + ["shot @OUT@/huge.png", "wait 2"]) + "\n",
                           [HUGE, "--aa", "none"], n, expect_culled=True)
        if cand:
            check(len(nodes_of(cand)) >= 20, f"huge: {len(nodes_of(cand))} distinct selections")

        for aa in ("none", "msaa4", "taa"):
            print(f"[production fixture from behind: overlap grid and 1-px edge steps, AA {aa}]")
            lines, n = grid(range(360, 960, 75), range(250, 600, 70), "pick")
            edge, ne = grid(list(range(480, 524, 2)) + list(range(734, 752, 1)), [450], "edge", spaced=True)
            vedge, nv = grid([600], list(range(296, 312, 1)) + list(range(570, 582, 1)), "vedge", spaced=True)
            script = ["wait 10"] + lines + edge + vedge + ["shot @OUT@/fx_" + aa + ".png", "wait 2"]
            cand = equivalence(f"fixture_{aa}", "\n".join(script) + "\n", [FIXTURE, "--aa", aa] + BEHIND, n + ne + nv)
            if cand:
                check({"node=OpaqueQuadNode", "node=BlendQuadNode", "node=TransmissiveQuadNode", "node="} <= nodes_of(cand),
                      f"fixture_{aa}: clicks hit every quad and the background")

        print("[production fixture, orthographic, from behind]")
        lines, n = grid(range(360, 960, 50), range(250, 600, 50), "pick")
        cand = equivalence("ortho", "\n".join(["wait 10"] + lines + ["shot @OUT@/ortho.png", "wait 2"]) + "\n",
                           [FIXTURE, "--aa", "none", "--camera-projection", "orthographic"] + BEHIND, n)

        print("[thin geometry: wireframe render mode]")
        lines, n = grid(range(470, 860, 9), range(300, 600, 37), "pick")
        equivalence("wireframe", "\n".join(["wait 10"] + lines + ["shot @OUT@/wire.png", "wait 2"]) + "\n",
                    [FIXTURE, "--aa", "none", "--render-mode", "wireframe"] + BEHIND, n)

        print("[textured alpha masks: AlphaBlendModeTest]")
        lines, n = grid(range(300, 1010, 14), range(300, 520, 44), "pick")
        cand = equivalence("alpha", "\n".join(["wait 10"] + lines + ["shot @OUT@/alpha.png", "wait 2"]) + "\n",
                           [ALPHA, "--aa", "none"], n, expect_culled=True)

        print("[skinned Fox: clicks during playback]")
        lines, n = grid(range(420, 900, 30), range(300, 560, 40), "pick", wait=3)
        cand = equivalence("fox", "\n".join(["wait 8", "animplay on", "wait 4"] + lines + ["shot @OUT@/fox.png", "animplay off", "wait 2"]) + "\n",
                           [FOX, "--aa", "none"], n)
        if cand:
            check(len(nodes_of(cand)) >= 2, f"fox: {sorted(nodes_of(cand))}")

        print("[animated hierarchy: clicks during playback and after wheel zoom]")
        lines_a, na = grid(range(420, 860, 40), range(290, 490, 40), "play", wait=3)
        lines_b, nb = grid(range(400, 900, 125), range(250, 600, 120), "zoom", wait=3)
        zoomed = []
        for line in lines_b:
            if line.startswith("click"):
                zoomed.append("wheel 1")
            zoomed.append(line)
        script = ["wait 8", "animplay on", "wait 4"] + lines_a + ["animplay off", "wait 4", "move 640 400", "wait 2"] + zoomed + ["shot @OUT@/hier.png", "wait 2"]
        cand = equivalence("hierarchy", "\n".join(script) + "\n", [HIER, "--aa", "none"], na + nb, expect_culled=True)
        if cand:
            check(bool(nodes_of(cand, "play") & {"node=AnimatedRoot", "node=ScaledChild", "node=GrandChild"}),
                  f"hierarchy: playback clicks hit animated geometry ({sorted(nodes_of(cand, 'play'))})")

        print("[two live viewports (viewport 2 double-sided), clicks in both]")
        lines, n = grid(range(320, 980, 40), range(260, 560, 75), "pick")
        script = ["wait 8", "vpcreate", "wait 10", "vpsplit 2", "wait 10", "vpsetting 2 doublesided on", "wait 6"] + lines + ["shot @OUT@/split.png", "wait 2"]
        cand = equivalence("split", "\n".join(script) + "\n", [FIXTURE, "--aa", "none"] + BEHIND, n)

        if E195.exists():
            print("[E195 cockpit (real asset)]")
            lines, n = grid(range(320, 980, 30), range(120, 740, 40), "pick", wait=4)
            cand = equivalence("e195", "\n".join(["wait 10"] + lines + ["shot @OUT@/e195.png", "wait 2"]) + "\n",
                               [str(E195), "--aa", "none"], n, expect_culled=True)
            if cand:
                check(len(nodes_of(cand)) >= 10, f"e195: {len(nodes_of(cand))} distinct selections")
        else:
            print("  NOT RUN E195 cockpit: asset not present")


def click_bench(exe, fixture, label, index):
    folder = OUT / "bench"
    folder.mkdir(parents=True, exist_ok=True)
    lines = ["wait 30"]
    for x, y in ((x, y) for y in range(200, 700, 100) for x in range(350, 950, 100)):
        lines += [f"click {x} {y}", "wait 4"]
    lines.append("wait 200")
    stem = f"{Path(fixture).stem}_{label}_{index}"
    script, report, prof = folder / f"s_{stem}.txt", folder / f"r_{stem}.json", folder / f"p_{stem}.json"
    script.write_text("\n".join(lines) + "\n", encoding="utf-8")
    for f in (report, prof):
        f.unlink(missing_ok=True)
    subprocess.run([str(exe), fixture, "--aa", "none", "--vsync", "off", "--profile", "--profile-report", str(prof),
                    "--capture-viewport", str(folder / f"c_{stem}.png"), "--capture-frame", "400"]
                   + COMMON + ["--ui-script", str(script), "--ui-report", str(report)],
                   cwd=REPO, capture_output=True, timeout=600)
    data = json.loads(prof.read_text(encoding="utf-8"))
    click_frames = [f for f in data["frames"] if f.get("counters", {}).get("pickingBuilds", 0) > 0]
    ids = {f["frame"] for f in click_frames}
    gpu = [sum(p["ms"] for p in s["passes"] if p.get("available")) for s in data["gpu"]["completed"]
           if s["frame"] in ids and s.get("label") == "picking"]
    cpu = [sum(s["ms"] for s in f["stages"] if s["name"] == "picking.draw") for f in click_frames]
    return {"clicks": len(click_frames), "cpu": statistics.median(cpu) if cpu else None,
            "frame": statistics.median(f["cpuFrameMs"] for f in click_frames) if click_frames else None,
            "gpu": statistics.median(gpu) if gpu else None}


if section("benchmark"):
    summary = {}
    for fixture in (MANY, HUGE, FIXTURE):
        print(f"[click-frame cost: {fixture}, {args.pairs} alternating pairs]")
        pre, cand = [], []
        for i in range(args.pairs):
            order = [("pre", PRE_EXE), ("candidate", EXE)] if i % 2 == 0 else [("candidate", EXE), ("pre", PRE_EXE)]
            res = {label: click_bench(exe, fixture, label, i) for label, exe in order}
            pre.append(res["pre"])
            cand.append(res["candidate"])
        for key in ("cpu", "gpu", "frame"):
            p = [r[key] for r in pre]
            c = [r[key] for r in cand]
            if any(v is None for v in p + c):
                print(f"    {key}: not available")
                continue
            noise = (max(p) - min(p)) / statistics.median(p)
            deltas = [(a - b) / a for a, b in zip(p, c)]
            print(f"    {key}: baseline {min(p):.3f}-{max(p):.3f} -> candidate {min(c):.3f}-{max(c):.3f} ms, "
                  f"median paired {statistics.median(deltas):+.1%}, improved {sum(d > 0 for d in deltas)}/{len(deltas)}, noise {noise:.1%}")
            check(statistics.median(deltas) > -max(0.05, noise), f"{Path(fixture).stem} {key}: no regression beyond 5% and noise")
        summary[fixture] = {"pre": pre, "candidate": cand}
    (OUT / "bench" / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
