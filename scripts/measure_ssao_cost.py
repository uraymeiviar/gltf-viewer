# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.5 cost: GPU time of the SSAO passes and of the whole viewport draw, from --profile-report.

For each scene (HelloCube, the E195 exterior view, the DA62 SDK cockpit) it renders SSAO off, on at full resolution
and on at half resolution (medium quality, radius 0.5 m), every frame redrawn (--disable-redraw-skip), 1600 x 900
window, one viewport, no AA, and reads:
  * ssaoMs: the GL_TIMESTAMP time of the AO passes (GTAO + denoise [+ upsample]), the profile report's ssao.gpuMsAverage
    (exponential average over the frames);
  * prepassMs: the GL_TIME_ELAPSED time of the "DecalPrepass" pass (prepass draws + the AO passes run from its post
    callback); without SSAO and decals the pass does not run;
  * mainMs: the sum of the "main" frame's pass times (medians over the frames), so mainMs(on) - mainMs(off) is the
    whole cost of SSAO including the depth/normal prepass and the shading lookups.
Not a benchmark harness: one Glider process at a time, no parallel runs. Writes build/ssao/cost.json.

Run: python scripts/measure_ssao_cost.py
"""

import json
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from review_shadows import DA62_COCKPIT, DA62_TEXTURES  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO, view  # noqa: E402

OUT = REPO / "build" / "ssao"
COMMON = ["--isolated", "--window-size", "1600x900", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none",
          "--disable-redraw-skip"]
CONFIGS = [("off", []), ("full", ["--ssao", "on"]), ("half", ["--ssao", "on", "--ssao-resolution", "half"])]


def scenes():
    result = [("hellocube", [])]
    exterior = REAL_ASSETS.get("Exterior")
    if exterior and Path(exterior).exists():
        result.append(("e195_exterior", [str(exterior)] + view("exterior_fuselage_nose")["flags"]))
    if DA62_COCKPIT.exists():
        result.append(("da62_cockpit", [str(DA62_COCKPIT), "--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05",
                                        "--camera-pitch", "-20", "--camera-fov", "55", "--camera-yaw", "220",
                                        "--search-path", str(DA62_TEXTURES)]))
    return result


def frame_times(report, label, only_pass=None):
    totals = {}
    for frame in report["gpu"]["completed"]:
        if frame["label"] != label:
            continue
        ms = sum(p["ms"] for p in frame["passes"] if p.get("available") and (only_pass is None or p["name"] == only_pass))
        totals[frame["frame"]] = totals.get(frame["frame"], 0.0) + ms
    return list(totals.values())


def run(name, args):
    report = OUT / f"cost_{name}.json"
    report.unlink(missing_ok=True)
    subprocess.run([str(EXE)] + args + COMMON + ["--capture-viewport", str(OUT / f"cost_{name}.png"),
                                                 "--capture-frame", "120", "--profile-report", str(report),
                                                 "--exit-after-profile"],
                   cwd=REPO, capture_output=True, text=True, timeout=900)
    return json.loads(report.read_text(encoding="utf-8")) if report.exists() else None


def median(values):
    return statistics.median(values) if values else None


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    results = {}
    for scene, args in scenes():
        entry = {}
        for config, flags in CONFIGS:
            report = run(f"{scene}_{config}", args + flags)
            if report is None:
                print(f"FAIL {scene} {config}: no report")
                continue
            viewport = (report.get("ssao") or {}).get("viewports", [{}])[0]
            entry[config] = {"mainMs": median(frame_times(report, "main")),
                             "prepassMs": median(frame_times(report, "main", "DecalPrepass")),
                             "ssaoMs": viewport.get("gpuMsAverage"),
                             "aoSize": [viewport.get("aoWidth"), viewport.get("aoHeight")],
                             "memoryBytes": viewport.get("memoryBytes"),
                             "frames": len(frame_times(report, "main"))}
        results[scene] = entry
        print(scene, json.dumps(entry), flush=True)
    (OUT / "cost.json").write_text(json.dumps(results, indent=1), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
