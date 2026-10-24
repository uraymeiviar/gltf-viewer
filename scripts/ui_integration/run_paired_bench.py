#!/usr/bin/env python3
"""Alternating baseline/candidate paired benchmarks for the P2.3 review fixes.

Scenarios either run the default model (unaffected control) or load a fixture and drive
the real UI with a --ui-script (scroll/filter) and then hold that state while the
built-in benchmark measures frames. Runs are strictly sequential; order alternates per pair.
Exit code: 0 ok, 1 a run failed/incomplete, 2 report could not be written.
"""
import argparse
import json
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import harness_process as subprocess  # noqa: E402 - shared harness launcher

REPO = Path(__file__).resolve().parents[2]
FIX = REPO / "build" / "p2.3" / "fixtures"
RES = "ResourcesTreeScrollRegion"
HOLD = "wait 4000"

SCENARIOS = {
    "control_default": (None, None),
    "res_4096_top": (FIX / "p2_3_resources_4096.glb", f"wait 40\nclickp Meshes -1\nwait 6\nscroll {RES} 0\nwait 4\n{HOLD}\n"),
    "res_4096_mid": (FIX / "p2_3_resources_4096.glb", f"wait 40\nclickp Meshes -1\nwait 6\nscroll {RES} 0.5\nwait 4\n{HOLD}\n"),
    "res_4096_bottom": (FIX / "p2_3_resources_4096.glb", f"wait 40\nclickp Meshes -1\nwait 6\nscroll {RES} bottom\nwait 4\n{HOLD}\n"),
    "res_4096_filter_end": (FIX / "p2_3_resources_4096.glb", f"wait 40\nclickp Meshes -1\nwait 6\nclickp res.search 0\ntype Mesh_409\nwait 6\nscroll {RES} bottom\nwait 4\n{HOLD}\n"),
    "res_multiline_bottom": (FIX / "p2_3_resources_multiline_4096.glb", f"wait 40\nclickp Meshes -1\nwait 6\nscroll {RES} bottom\nwait 4\n{HOLD}\n"),
    "variants_bottom": (FIX / "p2_3_variants_4096.glb", f"wait 40\nscroll {RES} bottom\nwait 4\n{HOLD}\n"),
}


def percentile95(values):
    ordered = sorted(values)
    return ordered[max(0, int(-(-0.95 * len(ordered) // 1)) - 1)]


def run_one(exe, scenario, tag, out_dir, warmup, frames):
    fixture, script_text = SCENARIOS[scenario]
    bench = out_dir / f"{scenario}_{tag}.json"
    cmd = [str(exe), "--isolated", "--window-size", "1280x720", "--vsync", "off", "--profile", "--benchmark",
           "--benchmark-warmup", str(warmup), "--benchmark-frames", str(frames),
           "--benchmark-output", str(bench), "--exit-after-benchmark"]
    if fixture is not None:
        script = out_dir / f"{scenario}.script.txt"
        script.write_text(script_text, encoding="utf-8")
        cmd = [str(exe), str(fixture), "--ui-script", str(script), "--ui-report", str(out_dir / f"{scenario}_{tag}.ui.json")] + cmd[1:]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=600, cwd=REPO)
    if proc.returncode != 0 or not bench.exists():
        return None, f"exit {proc.returncode}"
    data = json.loads(bench.read_text(encoding="utf-8"))
    if not data.get("complete") or data.get("measuredFramesRecorded") != frames:
        return None, "incomplete benchmark"
    cpu = [f["cpuFrameMs"] for f in data["measuredFrames"]]
    stage_values = {}
    for f in data["measuredFrames"]:
        for s in f["stages"]:
            stage_values.setdefault(s["name"], []).append(s["ms"])
    stages = {n: {"median": statistics.median(v), "p95": percentile95(v)} for n, v in stage_values.items()}
    return {"cpuMedian": statistics.median(cpu), "cpuP95": percentile95(cpu), "stages": stages}, None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--scenarios", nargs="+", default=list(SCENARIOS))
    parser.add_argument("--pairs", type=int, default=5)
    parser.add_argument("--warmup", type=int, default=120)
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--out", type=Path, default=REPO / "build" / "p2.3" / "bench")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    report = {"baseline": str(args.baseline), "candidate": str(args.candidate), "warmup": args.warmup,
              "frames": args.frames, "pairs": args.pairs, "scenarios": {}}
    failed = False
    for scenario in args.scenarios:
        runs = {"baseline": [], "candidate": []}
        for pair in range(args.pairs):
            order = ("baseline", "candidate") if pair % 2 == 0 else ("candidate", "baseline")
            for which in order:
                exe = args.baseline if which == "baseline" else args.candidate
                result, error = run_one(exe, scenario, f"{which}_{pair + 1}", args.out, args.warmup, args.frames)
                if error:
                    print(f"FAIL {scenario} {which} pair {pair + 1}: {error}")
                    failed = True
                    continue
                runs[which].append(result)
        if not runs["baseline"] or not runs["candidate"]:
            continue
        entry = {"runs": runs}
        for metric in ("cpuMedian", "cpuP95"):
            b = [r[metric] for r in runs["baseline"]]
            c = [r[metric] for r in runs["candidate"]]
            entry[metric] = {"baselineMedianOfRuns": statistics.median(b), "candidateMedianOfRuns": statistics.median(c),
                             "baselineRange": [min(b), max(b)], "candidateRange": [min(c), max(c)]}
        stage_names = sorted(set(runs["baseline"][0]["stages"]) & set(runs["candidate"][0]["stages"]))
        entry["stages"] = {}
        for name in stage_names:
            b = [r["stages"][name]["median"] for r in runs["baseline"] if name in r["stages"]]
            c = [r["stages"][name]["median"] for r in runs["candidate"] if name in r["stages"]]
            deltas = [(cc - bb) for bb, cc in zip(b, c)]
            entry["stages"][name] = {"baselineMedianOfRuns": statistics.median(b), "candidateMedianOfRuns": statistics.median(c),
                                     "baselineRange": [min(b), max(b)], "candidateRange": [min(c), max(c)],
                                     "pairedDeltaMedian": statistics.median(deltas)}
        report["scenarios"][scenario] = entry
        panels = entry["stages"].get("ui.panels")
        if panels:
            print(f"{scenario}: ui.panels {panels['baselineMedianOfRuns']:.4f} -> {panels['candidateMedianOfRuns']:.4f} ms; "
                  f"cpu median {entry['cpuMedian']['baselineMedianOfRuns']:.4f} -> {entry['cpuMedian']['candidateMedianOfRuns']:.4f} ms")
    try:
        (args.out / "paired_summary.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    except OSError as error:
        print(f"report write failed: {error}")
        return 2
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
