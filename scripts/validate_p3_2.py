# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

import argparse
import json
import os
import harness_process as subprocess
import sys
from pathlib import Path

# Prevent broken experimental MINGW-W64 numpy in Python 3.13 site-packages
# from crashing PIL's optional numpy import hooks.
sys.modules['numpy'] = None

# Enforce explicit failure if PIL/Pillow is missing
try:
    from PIL import Image
except ImportError as e:
    print(f"FATAL: PIL/Pillow is required for acceptance validation: {e}", file=sys.stderr)
    sys.exit(1)

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
P3_2_DIR = REPO / "build" / "p3.2"
POS_DIR = P3_2_DIR / "selfchecks_positive"
NEG_DIR = P3_2_DIR / "selfchecks_negative"
BASELINE_DIR = REPO / "build" / "p3.1" / "baseline"
CANDIDATE_DIR = P3_2_DIR / "candidate"

POS_DIR.mkdir(parents=True, exist_ok=True)
NEG_DIR.mkdir(parents=True, exist_ok=True)
CANDIDATE_DIR.mkdir(parents=True, exist_ok=True)

parser = argparse.ArgumentParser(description="P3.2 GL Sampler Objects and Cache Validation Suite")
parser.add_argument("--only", choices=["selfcheck", "fallback", "production-draw", "boundary", "lifecycle", "steady-state", "multi-viewport", "image-gate"],
                    help="Run only the specified focused scenario")
args = parser.parse_args()

print("=" * 72)
print("P3.2 GL Sampler Objects & Cache - Automated Validation Suite")
if args.only:
    print(f"Focused Mode: Running ONLY '{args.only}'")
print("=" * 72)

def compare_image_pixels(img_a, img_b):
    assert img_a.size == img_b.size, f"Size mismatch: {img_a.size} vs {img_b.size}"
    w, h = img_a.size
    bytes_a = img_a.convert("RGBA").tobytes()
    bytes_b = img_b.convert("RGBA").tobytes()

    diff_pixels = 0
    max_channel_diff = 0
    sum_diff = 0

    for i in range(0, len(bytes_a), 4):
        dr = abs(bytes_a[i] - bytes_b[i])
        dg = abs(bytes_a[i+1] - bytes_b[i+1])
        db = abs(bytes_a[i+2] - bytes_b[i+2])
        da = abs(bytes_a[i+3] - bytes_b[i+3])
        pixel_max = max(dr, dg, db, da)
        if pixel_max > 0:
            diff_pixels += 1
            max_channel_diff = max(max_channel_diff, pixel_max)
            sum_diff += dr + dg + db + da

    total_pixels = w * h
    diff_pct = (diff_pixels / total_pixels) * 100.0
    avg_diff = sum_diff / (diff_pixels * 4) if diff_pixels > 0 else 0.0
    return total_pixels, diff_pixels, diff_pct, max_channel_diff, avg_diff

# ======================================================================
# Step 1: Sampler Objects Diagnostic Self-Check (Positive, Forced-Fail, Unwritable)
# ======================================================================
if args.only in [None, "selfcheck", "fallback", "production-draw", "boundary", "lifecycle"]:
    print("\n[Step 1] Running Sampler Self-Check Suite...")

    # Determine which case to test
    case_filter = "all"
    if args.only in ["fallback", "production-draw", "boundary", "lifecycle"]:
        case_filter = args.only

    pattern_dump = CANDIDATE_DIR / "frame_pattern_test.png"
    if case_filter in ["all", "production-draw"] and pattern_dump.exists():
        pattern_dump.unlink()

    pos_out = POS_DIR / f"sampler_selfcheck_{case_filter}.txt"
    cmd_pos = [
        str(EXE),
        "--sampler-selfcheck", str(pos_out),
        "--sampler-selfcheck-case", case_filter,
        "--sampler-dump-pattern", str(pattern_dump)
    ]
    res_pos = subprocess.run(cmd_pos, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_pos.returncode == 0, f"Positive sampler self-check failed with code {res_pos.returncode}: {res_pos.stderr}"
    assert pos_out.exists() and pos_out.stat().st_size > 0, "Positive sampler self-check output missing or empty"
    pos_content = pos_out.read_text(encoding="utf-8")
    assert "Result: PASS" in pos_content, f"Report did not contain Result: PASS:\n{pos_content}"
    assert "FAIL:" not in pos_content, f"Report contained failures:\n{pos_content}"
    print(f"  PASS: Positive sampler self-check ({case_filter}) returned exit code 0; report written to {pos_out.name}")

    if case_filter in ["all", "production-draw"]:
        assert pattern_dump.exists() and pattern_dump.stat().st_size > 0, "Patterned test frame dump missing or empty"
        with Image.open(pattern_dump) as img:
            assert img.size == (64, 64), f"Expected 64x64 patterned dump, got {img.size}"
            # Check quadrant colors
            pixels = img.convert("RGBA").load()
            # Quadrant 0 (Red) at (16, 48) in PIL coordinates (top-down)
            p_red = pixels[16, 48]
            assert p_red[0] > 200 and p_red[1] < 50 and p_red[2] < 50, f"Expected Red quadrant, got {p_red}"
            # Center blended pixel at (32, 32)
            p_center = pixels[32, 32]
            assert 30 < p_center[0] < 200 and 30 < p_center[1] < 200, f"Expected blended center pixel, got {p_center}"
        print(f"  PASS: Verified patterned test frame {pattern_dump.name} (64x64 bilinear interpolation)")

    if args.only is None or args.only == "selfcheck":
        # Negative check (forced failure)
        neg_out = NEG_DIR / "sampler_forced_fail.txt"
        cmd_neg = [str(EXE), "--sampler-selfcheck", str(neg_out), "--selfcheck-force-failure"]
        res_neg = subprocess.run(cmd_neg, cwd=REPO, capture_output=True, text=True, timeout=30)
        assert res_neg.returncode == 1, f"Forced failure check expected exit code 1, got {res_neg.returncode}"
        assert neg_out.exists() and neg_out.stat().st_size > 0, "Forced failure output missing or empty"
        neg_content = neg_out.read_text(encoding="utf-8")
        assert "FAIL: forced sampler self-check failure requested" in neg_content, f"Unexpected content in forced failure report:\n{neg_content}"
        print(f"  PASS: Forced-failure self-check returned exit code 1; report written to {neg_out.name}")

        # Unwritable path check (passing a directory path as the output file)
        cmd_unwritable = [str(EXE), "--sampler-selfcheck", str(NEG_DIR)]
        res_unwritable = subprocess.run(cmd_unwritable, cwd=REPO, capture_output=True, text=True, timeout=30)
        assert res_unwritable.returncode == 2, f"Unwritable path expected exit code 2, got {res_unwritable.returncode}"
        print(f"  PASS: Unwritable output path returned exit code 2 as expected.")

# ======================================================================
# Step 2: Steady-State Profiling & Driver Call Verification
# ======================================================================
if args.only is None or args.only == "steady-state":
    print("\n[Step 2] Measuring Steady-State OpenGL Driver Calls & Sampler Objects...")
    bench_out = POS_DIR / "steady_state_bench.json"
    if bench_out.exists():
        bench_out.unlink()

    cmd_bench = [
        str(EXE),
        "--isolated",
        "--profile",
        "--benchmark",
        "--benchmark-output", str(bench_out),
        "--benchmark-warmup", "8",
        "--benchmark-frames", "30",
        "--exit-after-benchmark"
    ]
    res_bench = subprocess.run(cmd_bench, cwd=REPO, capture_output=True, text=True, timeout=60)
    assert res_bench.returncode == 0, f"Benchmark execution failed with code {res_bench.returncode}: {res_bench.stderr}"
    assert bench_out.exists() and bench_out.stat().st_size > 0, "Benchmark output JSON missing or empty"

    bench_data = json.loads(bench_out.read_text(encoding="utf-8"))
    measured_frames = bench_data.get("measuredFrames", [])
    assert len(measured_frames) >= 30, f"Expected at least 30 measured frames, got {len(measured_frames)}"

    for idx, sample in enumerate(measured_frames):
        c = sample["counters"]
        tex_param = c.get("texParameterCalls", 0)
        sampler_create = c.get("samplerCreations", 0)
        sampler_delete = c.get("samplerDeletions", 0)
        sampler_param = c.get("samplerParameterCalls", 0)
        sampler_bind = c.get("samplerBinds", 0)

        assert tex_param == 0, f"Frame {idx}: texParameterCalls was {tex_param}, expected 0"
        assert sampler_create == 0, f"Frame {idx}: samplerCreations was {sampler_create}, expected 0"
        assert sampler_delete == 0, f"Frame {idx}: samplerDeletions was {sampler_delete}, expected 0"
        assert sampler_param == 0, f"Frame {idx}: samplerParameterCalls was {sampler_param}, expected 0"
        assert sampler_bind > 0, f"Frame {idx}: samplerBinds was {sampler_bind}, expected > 0"

    first_frame_c = measured_frames[0]["counters"]
    print(f"  PASS: Steady-state verified across {len(measured_frames)} measured frames:")
    print(f"        texParameterCalls:        0")
    print(f"        samplerCreations:         0")
    print(f"        samplerDeletions:         0")
    print(f"        samplerParameterCalls:    0")
    print(f"        samplerBinds:             {first_frame_c.get('samplerBinds')} (active GL sampler objects)")
    print(f"        glCalls per frame:        {first_frame_c.get('glCalls')}")

# ======================================================================
# Step 3: Multi-Viewport Isolation & Lifecycle
# ======================================================================
if args.only is None or args.only == "multi-viewport":
    print("\n[Step 3] Multi-Viewport Sampler Isolation & Steady-State Verification...")
    mv_shot = CANDIDATE_DIR / "frame_multi_viewport.png"
    if mv_shot.exists():
        mv_shot.unlink()

    mv_script = CANDIDATE_DIR / "multi_viewport.script.txt"
    mv_report = CANDIDATE_DIR / "multi_viewport.report.json"
    mv_script.write_text(
        f"wait 8\n"
        f"vpcreate\n"
        f"wait 8\n"
        f"aamode fxaa 1\n"
        f"aamode taa 2\n"
        f"wait 15\n"
        f"snap multi_viewport_settled\n"
        f"shot {mv_shot.as_posix()}\n"
        f"wait 4\n",
        encoding="utf-8"
    )
    cmd_mv = [
        str(EXE),
        "--isolated",
        "--window-size", "1280x720",
        "--ui-script", str(mv_script),
        "--ui-report", str(mv_report),
    ]
    res_mv = subprocess.run(cmd_mv, cwd=REPO, capture_output=True, text=True, timeout=60)
    assert res_mv.returncode == 0, f"Multi-viewport scenario failed with code {res_mv.returncode}: {res_mv.stderr}"
    assert mv_report.exists(), "Multi-viewport report missing"
    assert mv_shot.exists() and mv_shot.stat().st_size > 0, "Multi-viewport screenshot missing or empty"
    print("  PASS: Multi-viewport scenario executed successfully with zero crashes and clean exit.")

# ======================================================================
# Step 4: Visual Regression Image Comparison Gate
# ======================================================================
if args.only is None or args.only == "image-gate":
    print("\n[Step 4] Image Comparison Acceptance Gate (Bit-for-Bit Exact Equality)...")
    MODES = ["none", "msaa2", "msaa4", "msaa8", "fxaa", "taa"]
    comparison_log = []

    for mode in MODES:
        cand_img = CANDIDATE_DIR / f"frame_{mode}.png"
        if cand_img.exists():
            cand_img.unlink()

        cmd_img = [
            str(EXE),
            "--isolated",
            "--window-size", "1280x720",
            "--aa", mode,
            "--dump-frame", str(cand_img),
            "--exit-after-dump"
        ]
        res_img = subprocess.run(cmd_img, cwd=REPO, capture_output=True, text=True, timeout=60)
        assert res_img.returncode == 0, f"Capture for mode {mode} failed with code {res_img.returncode}: {res_img.stderr}"
        assert cand_img.exists() and cand_img.stat().st_size > 0, f"Candidate image {cand_img} missing"

        base_img = BASELINE_DIR / f"frame_{mode}.png"
        assert base_img.exists(), f"Baseline image {base_img} missing"

        with Image.open(base_img) as ib, Image.open(cand_img) as ic:
            tot, diffs, diff_pct, max_d, avg_d = compare_image_pixels(ib, ic)

        assert diffs == 0, f"Mode {mode} failed exact equality gate: {diffs} differing pixels, max_d={max_d}"
        log_line = f"Mode {mode:>6}: total {tot} px, diff {diffs} (0.0000%), max_diff {max_d} -> EXACT EQUALITY MATCH"
        comparison_log.append(log_line)
        print(f"  PASS: {log_line}")

    summary_file = POS_DIR / "image_comparison_summary.txt"
    summary_file.write_text("\n".join(comparison_log) + "\n", encoding="utf-8")
    print(f"  PASS: All 6 startup captures match baseline bit-for-bit (exact equality).")

print("\n" + "=" * 72)
print("P3.2 VALIDATION SUITE: ALL CHECKS PASSED SUCCESSFULLY!")
print("=" * 72)
