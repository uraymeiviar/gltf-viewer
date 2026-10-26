# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Validation test suite for MSFS emissive brightness on the HDR pipeline.
Tests:
1. Core glTF fixture with emissiveFactor [1, 1, 1]: byte-identical parity vs baseline 44fb53e.exe.
2. ASOBO fixture with emissiveFactor [3000, 3000, 3000]:
   - Default MSFS scale (1/1000): blooms at effective factor 3.0.
   - Scale 1/3000: effective factor 1.0 (no bloom).
   - Scale 1/300: effective factor 10.0 (intense bloom).
   - Viewport with ASOBO off (--asobo off): factor clamped to 1.0, matching baseline.
3. Day / Night multiplier:
   - ASOBO_material_emissive day vs night multiplier application with --time-of-day day|night.
4. Zero GL automation errors across all test runs.
"""
import base64
import json
import re
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image
import harness_process as subprocess

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build/review"
EMIS_DIR = OUT / "emissive"
EXE = REPO / "build/bin/Release/Glider.exe"
BASELINE = REPO / "build/baselines/44fb53e.exe"

failures = []


def check(ok: bool, label: str):
    print(("PASS " if ok else "FAIL ") + label, flush=True)
    if not ok:
        failures.append(label)


def make_quad_fixture(name: str, emissive_factor, is_asobo: bool = False,
                      day_mult: float = None, night_mult: float = None,
                      half_extent: float = 0.25) -> Path:
    s = float(half_extent)
    positions = [(-s, -s, 0.0), (s, -s, 0.0), (s, s, 0.0), (-s, s, 0.0)]
    normals = [(0.0, 0.0, 1.0)] * 4
    indices = [0, 1, 2, 0, 2, 3]
    data = bytearray()
    data.extend(b"".join(struct.pack("<3f", *v) for v in positions))
    data.extend(b"".join(struct.pack("<3f", *v) for v in normals))
    data.extend(b"".join(struct.pack("<H", i) for i in indices))

    material = {
        "pbrMetallicRoughness": {
            "baseColorFactor": [0.0, 0.0, 0.0, 1.0],
            "metallicFactor": 0.0,
            "roughnessFactor": 1.0,
        },
        "emissiveFactor": list(emissive_factor),
        "doubleSided": True,
    }

    extensions_used = []
    if is_asobo:
        extensions_used.append("ASOBO_material_emissive")
        asobo_ext = {}
        if day_mult is not None:
            asobo_ext["emissiveDayMultiplier"] = float(day_mult)
        if night_mult is not None:
            asobo_ext["emissiveNightMultiplier"] = float(night_mult)
        material["extensions"] = {"ASOBO_material_emissive": asobo_ext}

    doc = {
        "asset": {"version": "2.0", "generator": "Glider MSFS Emissive Fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "Quad"}],
        "meshes": [{
            "name": "QuadMesh",
            "primitives": [{
                "attributes": {"POSITION": 0, "NORMAL": 1},
                "indices": 2,
                "mode": 4,
                "material": 0,
            }]
        }],
        "materials": [material],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3", "min": [-s, -s, 0.0], "max": [s, s, 0.0]},
            {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": 6, "type": "SCALAR"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 48},
            {"buffer": 0, "byteOffset": 48, "byteLength": 48},
            {"buffer": 0, "byteOffset": 96, "byteLength": 12},
        ],
        "buffers": [{"byteLength": len(data), "uri": "data:application/octet-stream;base64," + base64.b64encode(data).decode()}],
    }
    if extensions_used:
        doc["extensionsUsed"] = extensions_used

    path = EMIS_DIR / f"{name}.gltf"
    path.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    return path


def run_capture(label: str, model_path: Path, extra_args=(), exe: Path = EXE,
                win_size: str = "1280x800", frame: int = 30):
    png = EMIS_DIR / f"{label}.png"
    prof = EMIS_DIR / f"{label}.json"
    png.unlink(missing_ok=True)
    prof.unlink(missing_ok=True)
    cmd = [
        str(exe),
        str(model_path),
        *extra_args,
        "--isolated",
        "--window-size", win_size,
        "--fixed-frame-time", "0.05",
        "--vsync", "off",
        "--camera-projection", "ortho",
        "--camera-yaw", "0",
        "--camera-pitch", "0",
        "--camera-distance", "2",
        "--capture-viewport", str(png),
        "--capture-frame", str(frame),
        "--exit-after-profile",
        "--profile-report", str(prof),
    ]
    res = subprocess.run(cmd, cwd=REPO, capture_output=True, timeout=120)
    log_text = (res.stdout + res.stderr).decode("utf-8", errors="replace")
    (EMIS_DIR / f"{label}.log").write_text(log_text, encoding="utf-8")
    if res.returncode != 0 or not png.exists():
        raise RuntimeError(f"{label} failed with exit code {res.returncode}: {log_text[-300:]}")
    if exe == EXE:
        if "GL automation errors: 0" not in log_text or re.search(r"GL error (?:0x|during)|GL error \(0x", log_text):
            raise RuntimeError(f"{label}: GL error reported in log")
    return png, prof


def read_image_stats(png_path: Path):
    im = np.asarray(Image.open(png_path).convert("RGB"), dtype=float)
    h, w, _ = im.shape
    # Center pixel (on the quad)
    cy, cx = h // 2, w // 2
    center_color = tuple(int(round(x)) for x in im[cy, cx])
    # Halo region: margin between quad extent and border
    # Quad extent is 0.25 on ortho distance 2, so quad occupies central ~25% of viewport
    # Sample halo at 15% distance from center
    halo_y, halo_x = int(cy + h * 0.22), cx
    halo_luma = float(0.2126 * im[halo_y, halo_x, 0] + 0.7152 * im[halo_y, halo_x, 1] + 0.0722 * im[halo_y, halo_x, 2])
    # Corner (black background)
    corner_luma = float(0.2126 * im[5, 5, 0] + 0.7152 * im[5, 5, 1] + 0.0722 * im[5, 5, 2])
    return {
        "center": center_color,
        "halo_luma": halo_luma,
        "corner_luma": corner_luma,
        "raw": im,
    }


def main():
    EMIS_DIR.mkdir(parents=True, exist_ok=True)

    print("=== MSFS Emissive Brightness Validation Suite ===", flush=True)

    # 1. Core glTF fixture with emissiveFactor [1, 1, 1]
    core_model = make_quad_fixture("core_quad_1", [1.0, 1.0, 1.0], is_asobo=False)

    print("\n--- 1. Core glTF Parity vs Baseline (44fb53e) ---", flush=True)
    png_base, _ = run_capture("core_baseline_44fb53e", core_model, exe=BASELINE, extra_args=["--aa", "none", "--bloom", "off"])
    png_curr, _ = run_capture("core_current_glider", core_model, exe=EXE, extra_args=["--aa", "none", "--bloom", "off"])

    diff = np.abs(np.asarray(Image.open(png_curr), dtype=int) - np.asarray(Image.open(png_base), dtype=int))
    max_diff = int(diff.max())
    check(max_diff == 0, f"Core glTF [1, 1, 1] is byte-identical vs 44fb53e (max diff {max_diff})")

    # 2. ASOBO quad with emissiveFactor [3000, 3000, 3000]
    asobo_model = make_quad_fixture("asobo_quad_3000", [3000.0, 3000.0, 3000.0], is_asobo=True)

    print("\n--- 2. ASOBO Emissive Scaling & Bloom ---", flush=True)
    # Default scale 1/1000 with bloom enabled (effective emissive = 3.0)
    png_asobo_bloom, _ = run_capture("asobo_scale_1000_bloom", asobo_model, exe=EXE,
                                     extra_args=["--aa", "none", "--bloom", "on", "--bloom-intensity", "0.08"])
    stats_bloom = read_image_stats(png_asobo_bloom)

    # Default scale 1/1000 with bloom disabled
    png_asobo_nobloom, _ = run_capture("asobo_scale_1000_nobloom", asobo_model, exe=EXE,
                                       extra_args=["--aa", "none", "--bloom", "off"])
    stats_nobloom = read_image_stats(png_asobo_nobloom)

    check(stats_bloom["halo_luma"] >= stats_nobloom["halo_luma"] + 5.0,
          f"ASOBO quad blooms at default 1/1000 scale: halo luma {stats_bloom['halo_luma']:.1f} vs nobloom {stats_nobloom['halo_luma']:.1f}")

    # Scale 1/3000: effective emissive is 1.0 (should have minimal bloom, matching factor 1.0)
    png_asobo_3000, _ = run_capture("asobo_scale_3000_bloom", asobo_model, exe=EXE,
                                    extra_args=["--aa", "none", "--bloom", "on", "--bloom-intensity", "0.08",
                                                "--msfs-emissive-scale", "0.0003333333"])
    stats_3000 = read_image_stats(png_asobo_3000)
    check(stats_3000["halo_luma"] < stats_bloom["halo_luma"],
          f"Scale 1/3000 reduces bloom halo: {stats_3000['halo_luma']:.1f} < {stats_bloom['halo_luma']:.1f}")

    # Scale 1/300: effective emissive is 10.0 (very strong bloom)
    png_asobo_300, _ = run_capture("asobo_scale_300_bloom", asobo_model, exe=EXE,
                                   extra_args=["--aa", "none", "--bloom", "on", "--bloom-intensity", "0.08",
                                               "--msfs-emissive-scale", "0.003333333"])
    stats_300 = read_image_stats(png_asobo_300)
    check(stats_300["halo_luma"] >= stats_bloom["halo_luma"] + 8.0,
          f"Scale 1/300 produces intense bloom: halo {stats_300['halo_luma']:.1f} > {stats_bloom['halo_luma']:.1f}")

    # Viewport with ASOBO off (--asobo off): factor clamped to 1.0, falls back to baseline behavior
    png_asobo_off, _ = run_capture("asobo_off_fallback", asobo_model, exe=EXE,
                                   extra_args=["--aa", "none", "--asobo", "off", "--bloom", "off"])
    png_core_nobloom, _ = run_capture("core_nobloom", core_model, exe=EXE,
                                      extra_args=["--aa", "none", "--bloom", "off"])
    diff_off = np.abs(np.asarray(Image.open(png_asobo_off), dtype=int) - np.asarray(Image.open(png_core_nobloom), dtype=int))
    check(int(diff_off.max()) == 0,
          f"ASOBO off falls back to clamped factor [0, 1] byte-identically (diff {int(diff_off.max())})")

    # 3. Day / Night Multipliers
    print("\n--- 3. ASOBO_material_emissive Day/Night Multipliers ---", flush=True)
    asobo_daynight = make_quad_fixture("asobo_daynight", [3000.0, 3000.0, 3000.0], is_asobo=True,
                                       day_mult=1.0, night_mult=0.1)

    png_day, _ = run_capture("day_mult", asobo_daynight, exe=EXE,
                             extra_args=["--aa", "none", "--bloom", "off", "--time-of-day", "day"])
    png_night, _ = run_capture("night_mult", asobo_daynight, exe=EXE,
                               extra_args=["--aa", "none", "--bloom", "off", "--time-of-day", "night"])

    stats_day = read_image_stats(png_day)
    stats_night = read_image_stats(png_night)
    day_center = stats_day["center"][0]
    night_center = stats_night["center"][0]
    check(night_center < day_center - 50,
          f"Night multiplier reduces emissive brightness: night center {night_center} vs day center {day_center}")

    print("\n--- Summary ---", flush=True)
    if failures:
        print(f"FAILED ({len(failures)} failures):", flush=True)
        for f in failures:
            print(f"  - {f}", flush=True)
        sys.exit(1)
    else:
        print("ALL TESTS PASSED!", flush=True)


if __name__ == "__main__":
    main()
