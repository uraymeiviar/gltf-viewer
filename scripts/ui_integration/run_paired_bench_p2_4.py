#!/usr/bin/env python3
"""Alternating baseline/candidate paired benchmarks for P2.4 hierarchy virtualization.

Scenarios measure:
- control_default: small-scene unaffected control
- control_resources_bottom: unaffected flat-list clipping control
- hier_broad_collapsed: broad hierarchy collapsed (Scene collapsed)
- hier_broad_expanded_top: broad hierarchy fully expanded (4096 nodes), scrolled top
- hier_broad_expanded_mid: broad hierarchy fully expanded (4096 nodes), scrolled mid
- hier_broad_expanded_bottom: broad hierarchy fully expanded (4096 nodes), scrolled bottom
- hier_broad_search_many: search matching many rows (111 nodes), scrolled bottom
- hier_broad_search_sparse: search matching sparse leaf (1 node), scrolled bottom
- hier_deep_partial: deep hierarchy partially expanded (search TreeNode_002)
- hier_deep_nested: deep hierarchy deeply expanded (search TreeNode_409)
- hier_multiline_fallback: multiline label unclipped fallback control

Runs are strictly sequential; order alternates per pair.
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
FIX_P24 = REPO / "build" / "p2.4" / "fixtures"
FIX_P23 = REPO / "build" / "p2.3" / "fixtures"
HIER_SCROLL = "HierarchyTreeScrollRegion"
RES_SCROLL = "ResourcesTreeScrollRegion"
HOLD = "wait 4000"

SCENARIOS = {
    "control_default": (None, f"wait 40\n{HOLD}\n"),
    "control_resources_bottom": (
        FIX_P23 / "p2_3_resources_4096.glb",
        f"wait 40\nclickp Meshes -1\nwait 6\nscroll {RES_SCROLL} bottom\nwait 4\n{HOLD}\n"
    ),
    "hier_broad_collapsed": (
        FIX_P24 / "p2_4_hierarchy_broad_4096.glb",
        f"wait 40\nclickarrowrow hier.scene 0 {HIER_SCROLL}\nwait 6\n{HOLD}\n"
    ),
    "hier_broad_expanded_top": (
        FIX_P24 / "p2_4_hierarchy_broad_4096.glb",
        f"wait 40\nscroll {HIER_SCROLL} 0\nwait 4\n{HOLD}\n"
    ),
    "hier_broad_expanded_mid": (
        FIX_P24 / "p2_4_hierarchy_broad_4096.glb",
        f"wait 40\nscroll {HIER_SCROLL} 0.5\nwait 4\n{HOLD}\n"
    ),
    "hier_broad_expanded_bottom": (
        FIX_P24 / "p2_4_hierarchy_broad_4096.glb",
        f"wait 40\nscroll {HIER_SCROLL} bottom\nwait 4\n{HOLD}\n"
    ),
    "hier_broad_search_many": (
        FIX_P24 / "p2_4_hierarchy_broad_4096.glb",
        f"wait 40\nclickp hier.search 0\ntype Node_40\nwait 8\nscroll {HIER_SCROLL} bottom\nwait 4\n{HOLD}\n"
    ),
    "hier_broad_search_sparse": (
        FIX_P24 / "p2_4_hierarchy_broad_4096.glb",
        f"wait 40\nclickp hier.search 0\ntype Node_4095\nwait 8\nscroll {HIER_SCROLL} bottom\nwait 4\n{HOLD}\n"
    ),
    "hier_deep_partial": (
        FIX_P24 / "p2_4_hierarchy_deep_4096.glb",
        f"wait 40\nclickp hier.search 0\ntype TreeNode_002\nwait 8\nscroll {HIER_SCROLL} bottom\nwait 4\n{HOLD}\n"
    ),
    "hier_deep_nested": (
        FIX_P24 / "p2_4_hierarchy_deep_4096.glb",
        f"wait 40\nclickp hier.search 0\ntype TreeNode_409\nwait 8\nscroll {HIER_SCROLL} bottom\nwait 4\n{HOLD}\n"
    ),
    "hier_multiline_fallback": (
        FIX_P24 / "p2_4_hierarchy_multiline_4096.glb",
        f"wait 40\nscroll {HIER_SCROLL} bottom\nwait 4\n{HOLD}\n"
    ),
}


def percentile95(values):
    ordered = sorted(values)
    return ordered[max(0, int(-(-0.95 * len(ordered) // 1)) - 1)]


def run_one(exe, scenario, tag, out_dir, warmup, frames):
    fixture, script_text = SCENARIOS[scenario]
    bench = out_dir / f"{scenario}_{tag}.json"
    cmd = [
        str(exe),
        "--isolated",
        "--window-size", "1280x720",
        "--vsync", "off",
        "--profile",
        "--benchmark",
        "--benchmark-warmup", str(warmup),
        "--benchmark-frames", str(frames),
        "--benchmark-output", str(bench),
        "--exit-after-benchmark"
    ]
    if fixture is not None:
        script = out_dir / f"{scenario}.script.txt"
        script.write_text(script_text, encoding="utf-8")
        cmd = [
            str(exe),
            str(fixture),
            "--ui-script", str(script),
            "--ui-report", str(out_dir / f"{scenario}_{tag}.ui.json")
        ] + cmd[1:]
    else:
        script = out_dir / f"{scenario}.script.txt"
        script.write_text(script_text, encoding="utf-8")
        cmd = [
            str(exe),
            "--ui-script", str(script),
            "--ui-report", str(out_dir / f"{scenario}_{tag}.ui.json")
        ] + cmd[1:]

    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=600, cwd=REPO)
    if proc.returncode != 0 or not bench.exists():
        return None, f"exit {proc.returncode}"
    try:
        data = json.loads(bench.read_text(encoding="utf-8"))
    except Exception as e:
        return None, f"invalid json: {e}"
    if not data.get("complete") or data.get("measuredFramesRecorded") != frames:
        return None, "incomplete benchmark"

    cpu = [f["cpuFrameMs"] for f in data["measuredFrames"]]
    stage_values = {}
    for f in data["measuredFrames"]:
        for s in f["stages"]:
            stage_values.setdefault(s["name"], []).append(s["ms"])
    stages = {n: {"median": statistics.median(v), "p95": percentile95(v)} for n, v in stage_values.items()}

    cold = None
    if "firstFrame" in data:
        ff = data["firstFrame"]
        cold_stages = {s["name"]: s["ms"] for s in ff.get("stages", [])}
        cold = {"cpuFrameMs": ff.get("cpuFrameMs", 0.0), "stages": cold_stages}

    return {
        "cpuMedian": statistics.median(cpu),
        "cpuP95": percentile95(cpu),
        "stages": stages,
        "cold": cold
    }, None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--scenarios", nargs="+", default=list(SCENARIOS))
    parser.add_argument("--pairs", type=int, default=5)
    parser.add_argument("--warmup", type=int, default=120)
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--out", type=Path, default=REPO / "build" / "p2.4" / "bench")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    report = {
        "baseline": str(args.baseline),
        "candidate": str(args.candidate),
        "warmup": args.warmup,
        "frames": args.frames,
        "pairs": args.pairs,
        "scenarios": {}
    }
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
            entry[metric] = {
                "baselineMedianOfRuns": statistics.median(b),
                "candidateMedianOfRuns": statistics.median(c),
                "baselineRange": [min(b), max(b)],
                "candidateRange": [min(c), max(c)]
            }
        stage_names = sorted(set(runs["baseline"][0]["stages"]) & set(runs["candidate"][0]["stages"]))
        entry["stages"] = {}
        for name in stage_names:
            b = [r["stages"][name]["median"] for r in runs["baseline"] if name in r["stages"]]
            c = [r["stages"][name]["median"] for r in runs["candidate"] if name in r["stages"]]
            deltas = [(cc - bb) for bb, cc in zip(b, c)]
            entry["stages"][name] = {
                "baselineMedianOfRuns": statistics.median(b),
                "candidateMedianOfRuns": statistics.median(c),
                "baselineRange": [min(b), max(b)],
                "candidateRange": [min(c), max(c)],
                "pairedDeltaMedian": statistics.median(deltas)
            }

        if runs["baseline"][0].get("cold") and runs["candidate"][0].get("cold"):
            b_cold_cpu = [r["cold"]["cpuFrameMs"] for r in runs["baseline"] if r.get("cold")]
            c_cold_cpu = [r["cold"]["cpuFrameMs"] for r in runs["candidate"] if r.get("cold")]
            b_cold_panel = [r["cold"]["stages"].get("ui.panels", 0.0) for r in runs["baseline"] if r.get("cold")]
            c_cold_panel = [r["cold"]["stages"].get("ui.panels", 0.0) for r in runs["candidate"] if r.get("cold")]
            entry["cold"] = {
                "baselineCpuMedian": statistics.median(b_cold_cpu),
                "candidateCpuMedian": statistics.median(c_cold_cpu),
                "baselineUiPanelsMedian": statistics.median(b_cold_panel),
                "candidateUiPanelsMedian": statistics.median(c_cold_panel),
            }

        report["scenarios"][scenario] = entry
        panels = entry["stages"].get("ui.panels")
        if panels:
            print(
                f"{scenario:28s}: ui.panels {panels['baselineMedianOfRuns']:.4f} -> {panels['candidateMedianOfRuns']:.4f} ms "
                f"(delta {panels['pairedDeltaMedian']:+.4f} ms); "
                f"cpu {entry['cpuMedian']['baselineMedianOfRuns']:.4f} -> {entry['cpuMedian']['candidateMedianOfRuns']:.4f} ms"
            )

    try:
        (args.out / "paired_summary.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    except OSError as error:
        print(f"report write failed: {error}")
        return 2
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
