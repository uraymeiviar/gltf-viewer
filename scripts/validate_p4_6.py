# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P4.6 scene/command cache validation.

Layer 1 (P4.6a) caches program selection in CommandMapping, keyed on the exact
inputs of the define assembly (order-independent Defines content hashes, shader
names, render settings, tone mapping, IBL/punctual flags, light count).

Sections:
  equivalence  Each UI script runs three times: the pre-P4.6 binary
               (build/p4.6/pre/Glider.exe), the candidate with
               --disable-command-caches, and the candidate. Snapshots and
               full-window screenshots must be identical across all three.
               Candidate counters must show steady frames served by the cache.
  benchmark    Paired pre/candidate runs on the 1,600-drawable fixture: median
               commands.generate stage time and CPU frame time.
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
PRE_EXE = REPO / "build" / "p4.6" / "pre" / "Glider.exe"
OUT = REPO / "build" / "p4.6"
FIXTURE = "resources/test_models/production_passes_fixture.gltf"
FOX = "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
MORPH = "OpenGL-Renderer/Tools/tests/test_resources/AnimatedMorphCube.gltf"
MANY = "build/p4.6/fixtures/many_drawables.gltf"
COMMON = ["--isolated", "--window-size", "1280x800", "--fixed-frame-time", "0.05"]

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["equivalence", "picking", "benchmark"])
parser.add_argument("--pairs", type=int, default=5)
parser.add_argument("--pre-exe", help="reference binary for the benchmark (default: pre-P4.6)")
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
    flags = ["--disable-command-caches"] if mode == "uncached" else []
    result = subprocess.run([str(exe)] + extra + COMMON + flags + ["--ui-script", str(scenario), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=240)
    if result.returncode != 0 or not report.exists():
        check(False, f"{name} [{mode}]: exit {result.returncode} {result.stderr[-300:]}")
        return None
    data = json.loads(report.read_text(encoding="utf-8"))
    if data.get("failed", True):
        check(False, f"{name} [{mode}]: script errors {data.get('errors')}")
        return None
    return {"snaps": {s["label"]: s for s in data["snaps"]}, "shots": {Path(p).name: REPO / p for p in shots}}


def equivalence(name, script, extra, steady=()):
    pre = run("pre", PRE_EXE, name, script, extra)
    unc = run("uncached", EXE, name, script, extra)
    cand = run("candidate", EXE, name, script, extra)
    if not (pre and unc and cand):
        return None
    for label, snap in cand["snaps"].items():
        same = all((o["snaps"][label]["selection"], o["snaps"][label]["camera"]) == (snap["selection"], snap["camera"])
                   for o in (pre, unc))
        check(same, f"{name}/{label}: snapshot matches pre and uncached")
    for shot, path in cand["shots"].items():
        h = sha(path)
        check(h == sha(pre["shots"][shot]) and h == sha(unc["shots"][shot]),
              f"{name}/{shot}: byte-identical to pre-P4.6 and to uncached")
    rev = {k: v["revisions"] for k, v in cand["snaps"].items()}
    for before, after in steady:
        hits = rev[after]["programCacheHits"] - rev[before]["programCacheHits"]
        misses = rev[after]["programCacheMisses"] - rev[before]["programCacheMisses"]
        # Since P4.6c a command-cache hit skips program selection entirely.
        chits = rev[after].get("commandCacheHits", 0) - rev[before].get("commandCacheHits", 0)
        cmisses = rev[after].get("commandCacheMisses", 0) - rev[before].get("commandCacheMisses", 0)
        check(misses == 0 and cmisses == 0 and hits + chits > 0,
              f"{name} {before}->{after}: steady frames all cache hits (command {chits}/{cmisses}, program {hits}/{misses} hits/misses)")
    urev = {k: v["revisions"] for k, v in unc["snaps"].items()}
    last = list(urev)[-1]
    check(urev[last]["programCacheHits"] == 0 and urev[last].get("commandCacheHits", 0) == 0,
          f"{name}: --disable-command-caches never hits ({urev[last]['programCacheHits']}, {urev[last].get('commandCacheHits', 0)})")
    return rev


if section("equivalence"):
    if not PRE_EXE.exists():
        check(False, f"pre-P4.6 binary missing: {PRE_EXE} (NOT RUN)")
    else:
        print("[equivalence: debug modes, transmission, picking, overlay, split viewports, reload]")
        equivalence("fixture", f"""wait 8
snap loaded
wait 20
snap steady
shot @OUT@/fx_default.png
click 650 418
wait 8
snap picked
click 930 60
wait 6
click 990 167
wait 10
snap normal
shot @OUT@/fx_normal.png
click 930 60
wait 6
click 982 317
wait 10
snap basecolor
shot @OUT@/fx_basecolor.png
click 930 60
wait 6
click 968 74
wait 10
snap shaded
vpsetting 1 wireframe on
wait 10
snap overlay
shot @OUT@/fx_overlay.png
vpsetting 1 wireframe off
vpcreate
wait 10
vpsplit 2
wait 10
vpsetting 2 doublesided on
click 930 60
wait 6
click 990 167
wait 10
snap split
wait 20
snap split_steady
shot @OUT@/fx_split.png
reload {FOX}
wait 12
snap fox
shot @OUT@/fx_fox.png
reload {FIXTURE}
wait 12
snap reloaded
wait 20
snap reloaded_steady
shot @OUT@/fx_reloaded.png
wait 2
""", [FIXTURE, "--aa", "none"], steady=[("loaded", "steady"), ("split", "split_steady"), ("reloaded", "reloaded_steady")])

        print("[equivalence: skinning / morphing toggles and playback]")
        equivalence("anim", f"""wait 8
animplay on
wait 10
snap playing
shot @OUT@/an_fox_play.png
click 36 783
wait 6
click 58 752
wait 6
click 1150 400
wait 10
snap skin_off
shot @OUT@/an_fox_skin_off.png
click 36 783
wait 6
click 58 752
wait 6
click 1150 400
wait 10
snap skin_on
wait 10
snap skin_on_steady
shot @OUT@/an_fox_skin_on.png
animplay off
reload {MORPH}
wait 12
animplay on
wait 10
snap morph_play
shot @OUT@/an_morph_play.png
click 36 783
wait 6
click 58 780
wait 6
click 1150 400
wait 10
snap morph_off
shot @OUT@/an_morph_off.png
wait 2
""", [FOX, "--aa", "none"], steady=[("skin_on", "skin_on_steady")])


if section("picking"):
    if not PRE_EXE.exists():
        check(False, f"pre-P4.6 binary missing: {PRE_EXE} (NOT RUN)")
    else:
        print("[picking: 1,600 drawables, real viewport clicks resolve the same node]")
        clicks = [(x, y) for y in range(160, 720, 110) for x in range(330, 960, 120)]  # avoids the gizmo corner
        lines = ["wait 8"]
        for i, (x, y) in enumerate(clicks):
            lines += [f"click {x} {y}", "wait 8", f"snap pick{i}"]
        lines += ["shot @OUT@/pk_last.png", "wait 2"]
        rev = equivalence("picking", "\n".join(lines) + "\n", [MANY, "--aa", "none", "--camera-distance", "30"])
        if rev:
            report = json.loads((OUT / "candidate" / "picking.json").read_text(encoding="utf-8"))
            nodes = {s["selection"].split("|")[1] for s in report["snaps"]} - {"node="}
            check(len(nodes) >= 10, f"clicks resolved {len(nodes)} distinct nodes, identical to pre-P4.6")


def bench(exe, label, index):
    folder = OUT / "bench"
    folder.mkdir(parents=True, exist_ok=True)
    out = folder / f"{label}_{index}.json"
    out.unlink(missing_ok=True)
    result = subprocess.run([str(exe), MANY, "--isolated", "--window-size", "1280x800", "--aa", "none", "--vsync", "off",
                             "--profile", "--benchmark", "--benchmark-warmup", "120", "--benchmark-frames", "300",
                             "--benchmark-output", str(out), "--exit-after-benchmark"],
                            cwd=REPO, capture_output=True, text=True, timeout=600)
    if result.returncode != 0 or not out.exists():
        check(False, f"benchmark {label} {index}: exit {result.returncode}")
        return None
    data = json.loads(out.read_text(encoding="utf-8"))
    stage = [sum(s["ms"] for s in f.get("stages", []) if s["name"] == "commands.generate") for f in data["measuredFrames"]]
    picking = [sum(s["ms"] for s in f.get("stages", []) if s["name"] == "picking.build") for f in data["measuredFrames"]]
    print(f"      {label}: picking.build median {statistics.median(picking):.3f} ms")
    cpu = [f["cpuFrameMs"] for f in data["measuredFrames"]]
    counters = data["measuredFrames"][-1]["counters"]
    return statistics.median(stage), statistics.median(cpu), counters


if section("benchmark"):
    if not PRE_EXE.exists():
        check(False, f"pre-P4.6 binary missing: {PRE_EXE} (NOT RUN)")
    else:
        print(f"[benchmark: {MANY}, {args.pairs} alternating pairs, 120 warmup + 300 measured frames]")
        pre_stage, cand_stage, pre_cpu, cand_cpu = [], [], [], []
        for i in range(args.pairs):
            order = [("pre", PRE_EXE), ("candidate", EXE)] if i % 2 == 0 else [("candidate", EXE), ("pre", PRE_EXE)]
            results = {}
            for label, exe in order:
                results[label] = bench(exe, label, i)
            if results.get("pre") and results.get("candidate"):
                pre_stage.append(results["pre"][0]); cand_stage.append(results["candidate"][0])
                pre_cpu.append(results["pre"][1]); cand_cpu.append(results["candidate"][1])
                print(f"    pair {i}: commands.generate {results['pre'][0]:.3f} -> {results['candidate'][0]:.3f} ms, "
                      f"cpu frame {results['pre'][1]:.3f} -> {results['candidate'][1]:.3f} ms, "
                      f"drawables {results['candidate'][2].get('sceneDrawables')}, "
                      f"program hits/misses {results['candidate'][2].get('programCacheHits')}/{results['candidate'][2].get('programCacheMisses')}")
        if pre_stage:
            noise = (max(pre_stage) - min(pre_stage)) / statistics.median(pre_stage)
            deltas = [(p - c) / p for p, c in zip(pre_stage, cand_stage)]
            improved = sum(1 for d in deltas if d > 0)
            med = statistics.median(deltas)
            summary = {"pre_stage_ms": pre_stage, "candidate_stage_ms": cand_stage, "pre_cpu_ms": pre_cpu,
                       "candidate_cpu_ms": cand_cpu, "baseline_noise": noise, "median_paired_improvement": med}
            (OUT / "bench" / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
            print(f"    baseline noise {noise:.1%}, median paired improvement {med:.1%}, improved in {improved}/{len(deltas)} pairs")
            check(med > max(0.03, noise) and improved >= len(deltas) - 1,
                  "commands.generate improvement exceeds 3% and baseline noise (VALIDATION §6.5)")
            cpu_delta = statistics.median([(p - c) / p for p, c in zip(pre_cpu, cand_cpu)])
            check(cpu_delta > -0.05, f"no CPU frame regression beyond 5% (median paired change {cpu_delta:+.1%})")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
