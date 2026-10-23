# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P5.1 on-demand full-size picking validation.

The picking pass is rendered only in frames where a left click can reach the
viewport click handler; the framebuffer size and projection are unchanged.

Sections:
  equivalence  Each UI script runs three times: the pre-P5.1 binary
               (build/p5.1/pre/Glider.exe), the candidate with --always-pick
               (every-frame picking), and the candidate. Selections, cameras
               and full-window screenshots must be identical across all three.
               Candidate counters: one picking render per viewport click, no
               picking renders on other frames, no unserved clicks.
  benchmark    Paired pre/candidate runs: CPU frame and GPU time on the
               1,600-drawable fixture, CPU frame on HelloCube as the control.
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
PRE_EXE = REPO / "build" / "p5.1" / "pre" / "Glider.exe"
OUT = REPO / "build" / "p5.1"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
HIER = "resources/test_models/hierarchy_animation_fixture.gltf"
MANY = "build/p5.1/fixtures/many_drawables.gltf"
# Seen from behind, the transmissive, blend and opaque quads overlap with visible edges.
BEHIND = ["--camera-yaw", "160", "--camera-pitch", "10"]
# --fixed-frame-time makes scripted runs deterministic, so they need not wait for vsync.
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05", "--vsync", "off"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["equivalence", "benchmark"])
parser.add_argument("--pairs", type=int, default=5)
parser.add_argument("--pre-exe", help="reference binary (default: pre-P5.1)")
parser.add_argument("--off-flag", default="--always-pick", help="same-binary reference switch")
parser.add_argument("--bench", action="store_true", help="also run the benchmark section")
parser.add_argument("--with-ab", action="store_true", help="also compare with the candidate's own --off-flag switch")
args = parser.parse_args()
if args.pre_exe:
    PRE_EXE = Path(args.pre_exe).resolve()
failures = []


def section(name):
    if name == "benchmark":
        return args.only == "benchmark" or args.bench
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
    flags = [args.off_flag] if mode == "always" else []
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
    """clicks: viewport clicks in the script (each must render exactly one picking pass)."""
    pre = run("pre", PRE_EXE, name, script, extra)
    always = run("always", EXE, name, script, extra) if args.with_ab else None
    cand = run("candidate", EXE, name, script, extra)
    if not (pre and cand and (always or not args.with_ab)):
        return None
    others = (pre, always) if args.with_ab else (pre,)
    against = "the reference and --always-pick" if args.with_ab else "the reference"
    for label, snap in cand["snaps"].items():
        same = all((o["snaps"][label]["selection"], o["snaps"][label]["camera"]) == (snap["selection"], snap["camera"])
                   for o in others)
        check(same, f"{name}/{label}: selection and camera match {against} ({snap['selection']})")
    for shot, path in cand["shots"].items():
        h = sha(path)
        check(all(h == sha(o["shots"][shot]) for o in others), f"{name}/{shot}: byte-identical to {against}")
    rev = {k: v["revisions"] for k, v in cand["snaps"].items()}
    last = rev[list(rev)[-1]]
    check(last["pickUnserved"] == 0, f"{name}: no click went unserved ({last['pickUnserved']})")
    check(last["pickReads"] == clicks and last["pickingRenders"] == clicks,
          f"{name}: {clicks} viewport clicks -> {last['pickReads']} reads, {last['pickingRenders']} picking renders")
    for before, after in idle:
        renders = rev[after]["pickingRenders"] - rev[before]["pickingRenders"]
        skips = rev[after]["pickingSkips"] - rev[before]["pickingSkips"]
        check(renders == 0 and skips > 0, f"{name} {before}->{after}: idle frames render no picking pass ({renders} renders, {skips} skips)")
    if args.with_ab:
        arev = {k: v["revisions"] for k, v in always["snaps"].items()}
        alast = arev[list(arev)[-1]]
        check(alast["pickingSkips"] == 0, f"{name}: --always-pick never skips ({alast['pickingSkips']})")
    return cand


def grid(xs, ys, prefix, wait=6, gap_ms=0):
    """gap_ms > 0 spaces clicks that are closer than ImGui's double-click distance
    beyond its wall-clock double-click time, so none becomes a focusing double-click."""
    lines = []
    for i, (x, y) in enumerate((x, y) for y in ys for x in xs):
        # Spaced clicks wait 12 fixed frames (0.6 s of --fixed-frame-time) instead of wall-clock
        # time, so TAA screenshots repeat exactly between runs.
        lines += [f"click {x} {y}", f"wait {12 if gap_ms else wait}", f"snap {prefix}{i}"]
    return lines, len(xs) * len(ys)


if section("equivalence"):
    if not PRE_EXE.exists():
        check(False, f"pre-P5.1 binary missing: {PRE_EXE} (NOT RUN)")
    else:
        print("[1,600 drawables: 6x6 grid of real viewport clicks]")
        lines, n = grid(range(330, 960, 120), range(160, 720, 110), "pick")
        cand = equivalence("many", "\n".join(["wait 8", "snap idle0", "wait 30", "snap idle1"] + lines
                                             + ["shot @OUT@/many_last.png", "wait 2"]) + "\n",
                           [MANY, "--aa", "none", "--camera-distance", "30"], n, idle=[("idle0", "idle1")])
        if cand:
            nodes = {s["selection"] for s in cand["snaps"].values()}
            check(len(nodes) >= 10, f"many: clicks resolved {len(nodes)} distinct selections")

        # Overlapping opaque / blend / transmissive quads, all AA modes that change the
        # scene target (MSAA) or jitter the projection (TAA); a double-click focuses.
        for aa in ("none", "msaa4", "taa"):
            print(f"[production fixture: overlap grid, AA {aa}]")
            lines, n = grid(range(360, 960, 75), range(250, 600, 70), "pick")
            edge, ne = grid(list(range(480, 524, 4)) + list(range(734, 752, 3)), [450], "edge", gap_ms=400)
            n += ne
            script = ["wait 10"] + lines + edge + ["dclick 650 418", "wait 10", "snap focused",
                                            "shot @OUT@/fx_" + aa + ".png", "wait 2"]
            # dclick queues two clicks: two picking passes and reads.
            cand = equivalence(f"fixture_{aa}", "\n".join(script) + "\n", [FIXTURE, "--aa", aa] + BEHIND, n + 2)
            if cand:
                hit = {s["selection"].split("|")[1] for s in cand["snaps"].values()}
                check({"node=OpaqueQuadNode", "node=BlendQuadNode", "node=TransmissiveQuadNode", "node="} <= hit,
                      f"fixture_{aa}: clicks hit every quad and the background ({sorted(hit)})")

        print("[hierarchy fixture: clicks during playback, after a wheel zoom, after a resize]")
        # Dense over the animated root's cubes, so picks land on moving geometry.
        lines_a, na = grid(range(420, 860, 40), range(290, 490, 40), "play", wait=3)
        lines_b, nb = grid(range(400, 900, 125), range(250, 600, 120), "zoom", wait=3)
        lines_c, nc = grid(range(350, 850, 125), range(220, 520, 100), "resized", wait=3)
        zoomed = []
        for i, line in enumerate(lines_b):
            if line.startswith("click"):
                zoomed += ["wheel 1"]  # camera moves one frame before each click
            zoomed.append(line)
        script = (["wait 8", "animplay on", "wait 4"] + lines_a + ["shot @OUT@/hi_play.png", "animplay off", "wait 4"]
                  + ["move 640 400", "wait 2"] + zoomed + ["shot @OUT@/hi_zoom.png", "resize 1100 700", "wait 10"]
                  + lines_c + ["shot @OUT@/hi_resized.png", "wait 2"])
        cand = equivalence("hierarchy", "\n".join(script) + "\n", [HIER, "--aa", "none"], na + nb + nc)
        if cand:
            moving = {s["selection"].split("|")[1] for k, s in cand["snaps"].items() if k.startswith("play")}
            check(bool(moving & {"node=AnimatedRoot", "node=ScaledChild", "node=GrandChild"}),
                  f"hierarchy: playback clicks hit animated geometry ({sorted(moving)})")

        print("[production fixture: two live viewports, clicks in both]")
        lines, n = grid(range(320, 980, 80), range(260, 560, 100), "pick")
        script = (["wait 8", "vpcreate", "wait 10", "vpsplit 2", "wait 10", "vpsetting 2 doublesided on", "wait 6",
                   "snap split_idle0", "wait 20", "snap split_idle1"] + lines + ["shot @OUT@/split.png", "wait 2"])
        cand = equivalence("split", "\n".join(script) + "\n", [FIXTURE, "--aa", "none"] + BEHIND, n, idle=[("split_idle0", "split_idle1")])
        if cand:
            hit = {s["selection"] for s in cand["snaps"].values()}
            print(f"    split selections: {sorted(hit)}")
            nodes = {h.split("|")[1] for h in hit}
            check(len(nodes) >= 3, f"split: clicks across both viewports hit {len(nodes)} distinct nodes/background")


def bench(exe, label, index, model):
    folder = OUT / "bench"
    folder.mkdir(parents=True, exist_ok=True)
    tag = "many" if model else "cube"
    out = folder / f"{tag}_{label}_{index}.json"
    out.unlink(missing_ok=True)
    result = subprocess.run([str(exe)] + ([model] if model else []) +
                            ["--isolated", "--window-size", "1280x800", "--aa", "none", "--vsync", "off",
                             "--profile", "--benchmark", "--benchmark-warmup", "120", "--benchmark-frames", "300",
                             "--benchmark-output", str(out), "--exit-after-benchmark"],
                            cwd=REPO, capture_output=True, text=True, timeout=600)
    if result.returncode != 0 or not out.exists():
        check(False, f"benchmark {tag} {label} {index}: exit {result.returncode}")
        return None
    data = json.loads(out.read_text(encoding="utf-8"))
    frames = data["measuredFrames"]
    cpu = [f["cpuFrameMs"] for f in frames]
    p95 = sorted(cpu)[int(0.95 * (len(cpu) - 1))]
    submit = [sum(s["ms"] for s in f.get("stages", []) if s["name"] == "draw.submit") for f in frames]
    # GPU pass times (GL_TIME_ELAPSED per pass, P0.3), summed per frame over the retained measured frames.
    measured = {f["frame"] for f in frames}
    per_frame = {}
    for sample in data["gpu"]["completed"]:
        if sample["frame"] in measured:
            per_frame[sample["frame"]] = per_frame.get(sample["frame"], 0.0) + sum(
                p["ms"] for p in sample["passes"] if p.get("available"))
    gpu = list(per_frame.values())
    return {"cpu": statistics.median(cpu), "p95": p95, "submit": statistics.median(submit),
            "gpu": statistics.median(gpu) if gpu else None, "counters": frames[-1]["counters"]}


def paired(name, key, pre, cand, gate):
    p = [r[key] for r in pre]
    c = [r[key] for r in cand]
    if any(v is None for v in p + c):
        print(f"    {name}: not available")
        return
    noise = (max(p) - min(p)) / statistics.median(p)
    deltas = [(a - b) / a for a, b in zip(p, c)]
    med = statistics.median(deltas)
    improved = sum(1 for d in deltas if d > 0)
    print(f"    {name}: pre {min(p):.3f}-{max(p):.3f} -> cand {min(c):.3f}-{max(c):.3f} ms, "
          f"median paired {med:+.1%} improvement, {improved}/{len(deltas)} pairs, noise {noise:.1%}")
    if gate == "win":
        check(med > max(0.03, noise) and improved >= len(deltas) - 1, f"{name}: improvement exceeds 3% and noise (VALIDATION §6.5)")
    elif gate == "control":
        check(med > -max(0.05, noise), f"{name}: no regression beyond 5% and noise")
    return {"pre": p, "candidate": c, "noise": noise, "median_paired_improvement": med}


if section("benchmark"):
    if not PRE_EXE.exists():
        check(False, f"pre-P5.1 binary missing: {PRE_EXE} (NOT RUN)")
    else:
        summary = {}
        for model, tag in ((MANY, "many"), (None, "cube")):
            print(f"[benchmark {tag}: {args.pairs} alternating pairs, 120 warmup + 300 measured frames]")
            pre, cand = [], []
            for i in range(args.pairs):
                order = [("pre", PRE_EXE), ("candidate", EXE)] if i % 2 == 0 else [("candidate", EXE), ("pre", PRE_EXE)]
                results = {label: bench(exe, label, i, model) for label, exe in order}
                if results.get("pre") and results.get("candidate"):
                    pre.append(results["pre"]); cand.append(results["candidate"])
                    print(f"    pair {i}: cpu {results['pre']['cpu']:.3f} -> {results['candidate']['cpu']:.3f} ms, "
                          f"picking builds/frame {results['pre']['counters'].get('pickingBuilds')} -> "
                          f"{results['candidate']['counters'].get('pickingBuilds')}")
            if pre:
                summary[tag] = {
                    "cpu": paired(f"{tag} CPU frame", "cpu", pre, cand, "win" if tag == "many" else "control"),
                    "p95": paired(f"{tag} CPU p95", "p95", pre, cand, "control"),
                    "submit": paired(f"{tag} draw.submit", "submit", pre, cand, None),
                    "gpu": paired(f"{tag} GPU frame", "gpu", pre, cand, "win" if tag == "many" else None),
                }
        (OUT / "bench" / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
