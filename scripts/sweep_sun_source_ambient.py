# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.10 fill sweep using validate_night's unchanged regions and thresholds, one Glider at a time."""
import argparse
import ctypes
import json
import os
from pathlib import Path

os.environ.update(OMP_NUM_THREADS="12", OPENBLAS_NUM_THREADS="12", MKL_NUM_THREADS="12")
if os.name == "nt":
    # Keep the harness below half the logical CPUs (12 on the task host); use a pointer-sized pseudo-handle.
    core_limit = min(ctypes.sizeof(ctypes.c_size_t) * 8, max(1, (os.cpu_count() or 1) // 2))
    assert ctypes.windll.kernel32.SetProcessAffinityMask(ctypes.c_void_p(-1), ctypes.c_size_t((1 << core_limit) - 1)), ctypes.WinError()

import numpy as np
import validate_night as night
from validate_asobo_common import E195_SEARCH_PATHS, REAL_ASSETS, REPO, view


def measure(result):
    if result is None:
        raise RuntimeError("No interior capture")
    rgb, luma, rects, outside, report = result
    screens = {}
    for name, (x0, y0, x1, y1) in sorted(rects.items()):
        region = luma[y0:y1, x0:x1]
        screens[name] = {"p99": float(np.percentile(region, 99)),
                         "contrast": float(np.percentile(region, 95) - np.percentile(region, 5))}
    return {"screens": screens, "panelMedian": float(np.median(luma[outside])),
            "panelLitFraction": float((luma[outside] >= night.PANEL_LIT_LUMA).mean()),
            "windowClipFraction": float((rgb.max(axis=2)[outside] >= 250.0 / 255.0).mean()),
            "ev": night.exposure_of(report)[0], "ambient": report["ambient"]}


def passes(row):
    for name, metrics in row.items():
        if not metrics["screens"]:
            return False
        for screen in metrics["screens"].values():
            if screen["p99"] >= night.SCREEN_CLIP_LIMIT or screen["contrast"] < night.SCREEN_CONTRAST:
                return False
        if name == "noon" and (metrics["panelMedian"] < night.NOON_PANEL_MEDIAN or
                               metrics["windowClipFraction"] > night.WINDOW_CLIP_FRACTION):
            return False
        if name == "night" and metrics["panelLitFraction"] < night.PANEL_LIT_FRACTION:
            return False
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--values", default="0,0.01,0.025,0.05,0.1,0.2,0.5,1")
    args = parser.parse_args()
    night.OUT = REPO / "build/sun_source_ambient/sweep"
    night.OUT.mkdir(parents=True, exist_ok=True)
    results_path = night.OUT / "metrics.json"
    results = json.loads(results_path.read_text()) if results_path.exists() else {}
    for value in args.values.split(","):
        k = float(value)
        flags = ["--ambient", "auto", "--ambient-mode", "auto", "--ambient-fill", str(k)]
        row = {}
        for name, camera, tod in (("noon", "cockpit_windshield_inside", ["--time-preset", "noon", "--time-of-day", "auto"]),
                                  ("night", "cockpit_main_displays", night.NIGHT_EMISSIVE)):
            row[name] = measure(night.interior(f"k_{value}_{name}", REAL_ASSETS["Cockpit"],
                list(view(camera)["flags"]) + tod + flags, night.E195_SCREENS, E195_SEARCH_PATHS))
        if night.DA62_COCKPIT.exists():
            row["da62_night"] = measure(night.interior(f"k_{value}_da62_night", night.DA62_COCKPIT,
                night.NIGHT_EMISSIVE + night.DA62_CAMERA + flags, night.DA62_SCREENS))
        ok = passes(row)
        results[value] = {"passes": ok, "metrics": row}
        results_path.write_text(json.dumps(results, indent=2), encoding="utf-8")
        contrasts = "/".join(f"{v['contrast']:.6f}" for v in row["noon"]["screens"].values())
        night_p99 = max(v["p99"] for name, m in row.items() if "night" in name for v in m["screens"].values())
        print(f"k={k:.6f}: noon contrasts {contrasts}, median {row['noon']['panelMedian']:.6f}; "
              f"night p99 {night_p99:.6f}; {'PASS' if ok else 'FAIL'}", flush=True)
    bad = [name for name, log in night.gl_logs if not night.gl_errors_zero(log)]
    print(f"GL errors: {bad}", flush=True)
    return int(bool(bad))


if __name__ == "__main__":
    raise SystemExit(main())
