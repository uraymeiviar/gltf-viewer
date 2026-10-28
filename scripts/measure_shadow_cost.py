# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.2 cost: GPU time of the sun shadow pass and of the receiver lookups, from --profile-report.

For each scene (HelloCube, the E195 exterior view, the DA62 SDK cockpit) it renders 2048 x 3 cascades (PCF) with
the sun on, shadows on and off, every frame redrawn (--disable-redraw-skip), and reads the GL_TIME_ELAPSED pass
timings of the profile report: the "shadow" frames (one per cascade) give the caster pass, the difference of the
"main" frames (shadows on minus off) gives the receiver cost. Medians over the retained frames. Not a benchmark
harness: one Glider process at a time, no parallel runs. Writes build/shadows/cost.json.

Run: python scripts/measure_shadow_cost.py
"""

import json
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO, view  # noqa: E402
from review_shadows import DA62_COCKPIT, DA62_TEXTURES  # noqa: E402

OUT = REPO / "build" / "shadows"
COMMON = ["--isolated", "--window-size", "1600x900", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none",
          "--disable-redraw-skip", "--shadow-resolution", "2048", "--shadow-cascades", "3", "--shadow-filter", "pcf"]
SUN = ["--sun", "on", "--sun-dir", "45,40", "--sun-intensity", "8"]


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


def frame_times(report, label):
    """Per frame: summed pass milliseconds of the GPU frames with this label."""
    totals = {}
    for frame in report["gpu"]["completed"]:
        if frame["label"] != label:
            continue
        ms = sum(p["ms"] for p in frame["passes"] if p.get("available"))
        totals[frame["frame"]] = totals.get(frame["frame"], 0.0) + ms
    return list(totals.values())


def run(name, args):
    report = OUT / f"cost_{name}.json"
    report.unlink(missing_ok=True)
    # The report waits for the viewport capture, so frame 120 sets how many frames are timed.
    subprocess.run([str(EXE)] + args + COMMON + ["--capture-viewport", str(OUT / f"cost_{name}.png"),
                                                 "--capture-frame", "120", "--profile-report", str(report),
                                                 "--exit-after-profile"],
                   cwd=REPO, capture_output=True, text=True, timeout=900)
    return json.loads(report.read_text(encoding="utf-8")) if report.exists() else None


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    results = {}
    for name, args in scenes():
        on = run(f"{name}_on", args + SUN)
        off = run(f"{name}_off", args + SUN + ["--shadows", "off"])
        if on is None or off is None:
            print(f"FAIL {name}: no report")
            continue
        shadow = frame_times(on, "shadow")
        main_on, main_off = frame_times(on, "main"), frame_times(off, "main")
        viewport = on["sun"]["shadows"]["viewports"][0]
        entry = {"shadowPassMs": statistics.median(shadow) if shadow else None,
                 "mainWithShadowsMs": statistics.median(main_on) if main_on else None,
                 "mainWithoutShadowsMs": statistics.median(main_off) if main_off else None,
                 "frames": [len(shadow), len(main_on), len(main_off)],
                 "casters": viewport.get("casters"), "memoryBytes": viewport.get("memoryBytes"),
                 "cascades": viewport.get("cascades")}
        if entry["mainWithShadowsMs"] is not None and entry["mainWithoutShadowsMs"] is not None:
            entry["receiverMs"] = entry["mainWithShadowsMs"] - entry["mainWithoutShadowsMs"]
        results[name] = entry
        print(f"{name}: shadow pass {entry['shadowPassMs']}, main {entry['mainWithShadowsMs']} vs "
              f"{entry['mainWithoutShadowsMs']} ms, frames {entry['frames']}, casters {entry['casters']}", flush=True)
    (OUT / "cost.json").write_text(json.dumps(results, indent=1), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
