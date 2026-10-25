# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""HDR pipeline validation:
- Phase 1: linear RGBA16F scene + post-process tone mapping (opaque parity vs dba70c4.exe, linear blending deltas)
- Phase 2: physically based bloom (Jimenez 2014 13-tap Karis downsample + 3x3 tent upsample, resolution independence, TAA stability)
- Phase 3: auto-exposure (64x64 centre-weighted outlier-rejected log-average luminance metering, exponential adaptation, determinism)
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
HDR_DIR = OUT / "hdr"
EXE = REPO / "build/bin/Release/Glider.exe"
BASELINE = REPO / "build/baselines/dba70c4.exe"
E2_COCKPIT = Path(r"G:\e-jets-e2\msfs24-projects\fss-aircraft-e2-190\SimObjects\Airplanes\E2-190\model\E2-190-Cockpit_LOD00.gltf")

failures = []


def check(ok: bool, label: str):
    print(("PASS " if ok else "FAIL ") + label, flush=True)
    if not ok:
        failures.append(label)


def make_emissive_quad(name: str, strength: float, half_extent: float = 0.35, color=(1.0, 0.85, 0.5)) -> Path:
    s = float(half_extent)
    positions = [(-s, -s, 0.0), (s, -s, 0.0), (s, s, 0.0), (-s, s, 0.0)]
    normals = [(0.0, 0.0, 1.0)] * 4
    data = bytearray()
    for vals in (positions, normals):
        data.extend(b"".join(struct.pack("<3f", *v) for v in vals))
    doc = {
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "mode": 6, "material": 0}]}],
        "materials": [{
            "pbrMetallicRoughness": {"baseColorFactor": [0.0, 0.0, 0.0, 1.0], "metallicFactor": 0.0, "roughnessFactor": 1.0},
            "emissiveFactor": list(color),
            "doubleSided": True,
            "extensions": {"KHR_materials_emissive_strength": {"emissiveStrength": float(strength)}},
        }],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3", "min": [-s, -s, 0.0], "max": [s, s, 0.0]},
            {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 48},
            {"buffer": 0, "byteOffset": 48, "byteLength": 48},
        ],
        "buffers": [{"byteLength": len(data), "uri": "data:application/octet-stream;base64," + base64.b64encode(data).decode()}],
        "extensionsUsed": ["KHR_materials_emissive_strength"],
    }
    path = HDR_DIR / f"{name}.gltf"
    path.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    return path


def run_capture(label: str, extra_args=(), exe: Path = EXE, win_size: str = "1600x900", frame: int = 40):
    png = OUT / f"hdr_{label}.png"
    prof = OUT / f"hdr_{label}.json"
    png.unlink(missing_ok=True)
    cmd = [
        str(exe),
        *extra_args,
        "--isolated",
        "--window-size", win_size,
        "--fixed-frame-time", "0.05",
        "--vsync", "off",
        "--capture-viewport", str(png),
        "--capture-frame", str(frame),
        "--exit-after-profile",
        "--profile-report", str(prof),
    ]
    res = subprocess.run(cmd, cwd=REPO, capture_output=True, timeout=300)
    log_text = (res.stdout + res.stderr).decode("utf-8", errors="replace")
    (OUT / f"hdr_{label}.log").write_text(log_text, encoding="utf-8")
    if res.returncode != 0 or not png.exists():
        raise RuntimeError(f"{label} failed with exit code {res.returncode}")
    if exe == EXE:
        if "GL automation errors: 0" not in log_text or re.search(r"GL error (?:0x|during)|GL error \(0x", log_text):
            raise RuntimeError(f"{label}: GL error reported in log")
    return png, prof


def mean_luma(png_path: Path) -> float:
    im = np.asarray(Image.open(png_path).convert("RGB"), dtype=float)
    return float((0.2126 * im[:, :, 0] + 0.7152 * im[:, :, 1] + 0.0722 * im[:, :, 2]).mean())


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    HDR_DIR.mkdir(parents=True, exist_ok=True)
    summary = {"phase1": {}, "phase2": {}, "phase3": {}}

    # -------------------------------------------------------------------------
    # Phase 1: Linear HDR scene + post tone mapping (bloom off, manual exposure)
    # -------------------------------------------------------------------------
    for aa in ("none", "msaa4", "taa"):
        cur_png, cur_prof = run_capture(f"p1_hellocube_{aa}_cur", ["--aa", aa, "--bloom", "off", "--auto-exposure", "off"], EXE)
        base_png, _ = run_capture(f"p1_hellocube_{aa}_base", ["--aa", aa], BASELINE)
        a = np.asarray(Image.open(cur_png).convert("RGB"), dtype=float)
        b = np.asarray(Image.open(base_png).convert("RGB"), dtype=float)
        diff = np.abs(a - b)
        mae = float(diff.mean())
        mx = float(diff.max())
        p99 = float(np.percentile(diff, 99))
        summary["phase1"][f"hellocube_{aa}"] = {"mae_255": mae, "max_255": mx, "p99_255": p99}
        check(mae <= 0.5, f"Phase 1 HelloCube {aa}: MAE {mae:.4f}/255 <= 0.5/255 (p99={p99:.1f}, max={mx:.1f})")

    if E2_COCKPIT.exists():
        e2_cam = [str(E2_COCKPIT), "--camera-target", "-0.45,1.6,13.4", "--camera-yaw", "180", "--camera-pitch", "0", "--camera-distance", "0.3"]
        for aa in ("none", "msaa4", "taa"):
            cur_png, cur_prof = run_capture(f"p1_e2_{aa}_cur", [*e2_cam, "--aa", aa, "--bloom", "off", "--auto-exposure", "off"], EXE)
            base_png, base_prof = run_capture(f"p1_e2_{aa}_base", [*e2_cam, "--aa", aa], BASELINE)
            a = np.asarray(Image.open(cur_png).convert("RGB"), dtype=float)
            b = np.asarray(Image.open(base_png).convert("RGB"), dtype=float)
            diff = np.abs(a - b)
            opaque_diff = diff[420:, :, :]
            summary["phase1"][f"e2_cockpit_{aa}"] = {
                "overall_mae_255": float(diff.mean()),
                "overall_p99_255": float(np.percentile(diff, 99)),
                "overall_max_255": float(diff.max()),
                "opaque_panel_mae_255": float(opaque_diff.mean()),
                "opaque_panel_p99_255": float(np.percentile(opaque_diff, 99)),
                "opaque_panel_max_255": float(opaque_diff.max()),
            }
            check(opaque_diff.mean() <= 0.5,
                  f"Phase 1 E2 cockpit {aa}: opaque panel MAE {opaque_diff.mean():.4f}/255 <= 0.5/255 (overall MAE={diff.mean():.3f}/255)")

    # -------------------------------------------------------------------------
    # Phase 2: Physically based bloom (Jimenez 2014)
    # -------------------------------------------------------------------------
    f_s1 = make_emissive_quad("emissive_s1", 1.0)
    f_s10 = make_emissive_quad("emissive_s10", 10.0)
    f_s100 = make_emissive_quad("emissive_s100", 100.0)
    bloom_cam = ["--clear-color", "0,0,0", "--hide-grid", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "3.0"]

    p_off, _ = run_capture("bloom_s100_off", [str(f_s100), *bloom_cam, "--aa", "none", "--bloom", "off"], frame=24)
    p_s1, _ = run_capture("bloom_s1_on", [str(f_s1), *bloom_cam, "--aa", "none", "--bloom", "on"], frame=24)
    p_s10, _ = run_capture("bloom_s10_on", [str(f_s10), *bloom_cam, "--aa", "none", "--bloom", "on"], frame=24)
    p_s100, _ = run_capture("bloom_s100_on", [str(f_s100), *bloom_cam, "--aa", "none", "--bloom", "on"], frame=24)

    im_off = np.asarray(Image.open(p_off).convert("RGB"), dtype=float)
    quad_mask = im_off.max(axis=2) > 10.0
    outside = ~quad_mask
    ys, xs = np.where(quad_mask)
    qy0, qy1, qx0, qx1 = int(ys.min()), int(ys.max()), int(xs.min()), int(xs.max())
    cy = (qy0 + qy1) // 2

    def halo_stats(png_path: Path):
        im = np.asarray(Image.open(png_path).convert("RGB"), dtype=float)
        pixels_cnt = int(((im.max(axis=2) >= 2.0) & outside).sum())
        energy = float(im[outside].sum())
        reach_px = int((im[cy, qx1 + 1 :, 0] >= 2.0).sum())
        return {"halo_pixels": pixels_cnt, "halo_energy": energy, "right_reach_px": reach_px, "reach_over_quad_h": reach_px / max(1, qy1 - qy0)}

    st_off = halo_stats(p_off)
    st_s1 = halo_stats(p_s1)
    st_s10 = halo_stats(p_s10)
    st_s100 = halo_stats(p_s100)
    summary["phase2"]["s100_off"] = st_off
    summary["phase2"]["s1_on"] = st_s1
    summary["phase2"]["s10_on"] = st_s10
    summary["phase2"]["s100_on"] = st_s100

    check(st_off["halo_pixels"] == 0 and st_off["halo_energy"] == 0.0, "Phase 2 Bloom off: zero halo outside emissive quad")
    check(0 < st_s1["halo_pixels"] < st_s10["halo_pixels"] < st_s100["halo_pixels"] and
          0 < st_s1["right_reach_px"] < st_s10["right_reach_px"] < st_s100["right_reach_px"],
          f"Phase 2 Bloom halo grows with strength 1 -> 10 -> 100 (reach {st_s1['right_reach_px']} -> {st_s10['right_reach_px']} -> {st_s100['right_reach_px']} px)")

    # Resolution independence: 800x450 vs 1600x900
    p_off_800, _ = run_capture("bloom_s100_off_800x450", [str(f_s100), *bloom_cam, "--aa", "none", "--bloom", "off"], win_size="800x450", frame=24)
    p_s10_800, _ = run_capture("bloom_s10_800x450", [str(f_s10), *bloom_cam, "--aa", "none", "--bloom", "on"], win_size="800x450", frame=24)
    im_off_800 = np.asarray(Image.open(p_off_800).convert("RGB"), dtype=float)
    q800 = im_off_800.max(axis=2) > 10.0
    ys8, xs8 = np.where(q800)
    qy8_0, qy8_1, qx8_0, qx8_1 = int(ys8.min()), int(ys8.max()), int(xs8.min()), int(xs8.max())
    im8 = np.asarray(Image.open(p_s10_800).convert("RGB"), dtype=float)
    cy8 = (qy8_0 + qy8_1) // 2
    reach8_px = int((im8[cy8, qx8_1 + 1 :, 0] >= 2.0).sum())
    ratio_800 = reach8_px / max(1, qy8_1 - qy8_0)
    ratio_1600 = st_s10["reach_over_quad_h"]
    summary["phase2"]["resolution_independence"] = {
        "ratio_800x450": ratio_800,
        "ratio_1600x900": ratio_1600,
        "rel_diff": abs(ratio_800 - ratio_1600) / max(1e-6, ratio_1600),
    }
    check(abs(ratio_800 - ratio_1600) < 0.05,
          f"Phase 2 Bloom resolution independence 800x450 vs 1600x900: halo/quad ratio {ratio_800:.4f} vs {ratio_1600:.4f}")

    # TAA stability
    p_taa_f24, _ = run_capture("bloom_s10_taa_f24", [str(f_s10), *bloom_cam, "--aa", "taa", "--bloom", "on"], frame=24)
    p_taa_f25, _ = run_capture("bloom_s10_taa_f25", [str(f_s10), *bloom_cam, "--aa", "taa", "--bloom", "on"], frame=25)
    taa_diff = np.abs(np.asarray(Image.open(p_taa_f24).convert("RGB"), dtype=float) -
                      np.asarray(Image.open(p_taa_f25).convert("RGB"), dtype=float))
    summary["phase2"]["taa_stability_mae_255"] = float(taa_diff.mean())
    check(taa_diff.mean() < 0.05, f"Phase 2 Bloom TAA stability (frame 24 vs 25): MAE {taa_diff.mean():.5f}/255")

    # -------------------------------------------------------------------------
    # Phase 3: Auto-exposure convergence, compensation & determinism
    # -------------------------------------------------------------------------
    dark_gltf = make_emissive_quad("ae_dark", 0.03, half_extent=3.0, color=(0.8, 0.85, 0.9))
    bright_gltf = make_emissive_quad("ae_bright", 6.0, half_extent=3.0, color=(0.8, 0.85, 0.9))
    ae_cam = ["--clear-color", "0,0,0", "--hide-grid", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "2.0"]

    p_dark_man, _ = run_capture("ae_dark_manual", [str(dark_gltf), *ae_cam, "--aa", "none", "--auto-exposure", "off"])
    p_bright_man, _ = run_capture("ae_bright_manual", [str(bright_gltf), *ae_cam, "--aa", "none", "--auto-exposure", "off"])
    p_dark_auto, _ = run_capture("ae_dark_auto", [str(dark_gltf), *ae_cam, "--aa", "none", "--auto-exposure", "on"])
    p_bright_auto, _ = run_capture("ae_bright_auto", [str(bright_gltf), *ae_cam, "--aa", "none", "--auto-exposure", "on"])
    p_bright_auto_run2, _ = run_capture("ae_bright_auto_run2", [str(bright_gltf), *ae_cam, "--aa", "none", "--auto-exposure", "on"])

    p_comp_neg, _ = run_capture("ae_bright_comp_neg1", [str(bright_gltf), *ae_cam, "--aa", "none", "--auto-exposure", "on", "--exposure-compensation", "-1.0"])
    p_comp_pos, _ = run_capture("ae_bright_comp_pos1", [str(bright_gltf), *ae_cam, "--aa", "none", "--auto-exposure", "on", "--exposure-compensation", "1.0"])

    p_adapt_f2, _ = run_capture("ae_bright_adapt_f2", [str(bright_gltf), *ae_cam, "--aa", "none", "--exposure", "0", "--auto-exposure", "on"], frame=2)
    p_adapt_f8, _ = run_capture("ae_bright_adapt_f8", [str(bright_gltf), *ae_cam, "--aa", "none", "--exposure", "0", "--auto-exposure", "on"], frame=8)
    p_adapt_f40, _ = run_capture("ae_bright_adapt_f40", [str(bright_gltf), *ae_cam, "--aa", "none", "--exposure", "0", "--auto-exposure", "on"], frame=40)
    p_adapt_f40_run2, _ = run_capture("ae_bright_adapt_f40_run2", [str(bright_gltf), *ae_cam, "--aa", "none", "--exposure", "0", "--auto-exposure", "on"], frame=40)

    l_dark_man = mean_luma(p_dark_man)
    l_bright_man = mean_luma(p_bright_man)
    l_dark_auto = mean_luma(p_dark_auto)
    l_bright_auto = mean_luma(p_bright_auto)
    l_comp_neg = mean_luma(p_comp_neg)
    l_comp_pos = mean_luma(p_comp_pos)
    l_f2, l_f8, l_f40 = mean_luma(p_adapt_f2), mean_luma(p_adapt_f8), mean_luma(p_adapt_f40)

    summary["phase3"] = {
        "manual_dark_luma_255": l_dark_man,
        "manual_bright_luma_255": l_bright_man,
        "auto_dark_luma_255": l_dark_auto,
        "auto_bright_luma_255": l_bright_auto,
        "comp_neg1_luma_255": l_comp_neg,
        "comp_pos1_luma_255": l_comp_pos,
        "adapt_f2_luma_255": l_f2,
        "adapt_f8_luma_255": l_f8,
        "adapt_f40_luma_255": l_f40,
        "deterministic_snap": p_bright_auto.read_bytes() == p_bright_auto_run2.read_bytes(),
        "deterministic_temporal": p_adapt_f40.read_bytes() == p_adapt_f40_run2.read_bytes(),
    }

    check(abs(l_dark_auto - l_bright_auto) < 5.0 and abs(l_bright_man - l_dark_man) > 150.0,
          f"Phase 3 Auto-Exposure dark ({l_dark_auto:.2f}) and bright ({l_bright_auto:.2f}) converge (manual: {l_dark_man:.2f} vs {l_bright_man:.2f})")
    check(l_comp_neg < l_bright_auto - 15.0 and l_comp_pos > l_bright_auto + 15.0,
          f"Phase 3 Exposure Compensation shifts target luminance (-1 EV: {l_comp_neg:.2f}, 0 EV: {l_bright_auto:.2f}, +1 EV: {l_comp_pos:.2f})")
    check(l_f2 > l_f8 > l_f40,
          f"Phase 3 Temporal Adaptation progresses smoothly over time (f2={l_f2:.2f} -> f8={l_f8:.2f} -> f40={l_f40:.2f})")
    check(summary["phase3"]["deterministic_snap"] and summary["phase3"]["deterministic_temporal"],
          "Phase 3 Auto-Exposure is byte-identical across runs with --fixed-frame-time")

    (OUT / "hdr_validation_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
