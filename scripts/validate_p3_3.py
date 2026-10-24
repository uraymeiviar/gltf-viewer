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
P3_3_DIR = REPO / "build" / "p3.3"
POS_DIR = P3_3_DIR / "selfchecks_positive"
NEG_DIR = P3_3_DIR / "selfchecks_negative"
# Provenance note: BASELINE_DIR points to build/p3.1/baseline containing canonical
# 1280x720 startup captures of HelloCube.glb rendered under identical camera/IBL settings.
# These reference captures are reused to verify bit-for-bit equivalence (0 diffs) for startup rendering.
BASELINE_DIR = REPO / "build" / "p3.1" / "baseline"
CANDIDATE_DIR = P3_3_DIR / "candidate"

POS_DIR.mkdir(parents=True, exist_ok=True)
NEG_DIR.mkdir(parents=True, exist_ok=True)
CANDIDATE_DIR.mkdir(parents=True, exist_ok=True)

VALID_ONLY = [
    "selfcheck", "hdr-rotation", "steady-state", "scaling", "indexing",
    "attributes", "shader-variants", "invalidation", "lifecycle",
    "boundary", "production-draw", "benchmark", "visual-dumps", "image-gate"
]

parser = argparse.ArgumentParser(description="P3.3 Vertex Array Object (VAO) Reuse and Cache Validation Suite")
parser.add_argument("--only", choices=VALID_ONLY, help="Run only the specified focused scenario")
args = parser.parse_args()

print("=" * 72)
print("P3.3 Vertex Array Object (VAO) Reuse & Lifecycle - Validation Suite")
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
# Step 1: VAO Reuse Diagnostic Self-Check (Positive, Forced-Fail, Unwritable)
# ======================================================================
SELFCHECK_CASES = [
    "hdr-rotation", "steady-state", "scaling", "indexing", "attributes",
    "shader-variants", "invalidation", "lifecycle", "boundary", "production-draw"
]

if args.only in [None, "selfcheck"] + SELFCHECK_CASES:
    print("\n[Step 1] Running VAO Diagnostic Self-Check Suite...")

    case_filter = "all"
    if args.only in SELFCHECK_CASES:
        case_filter = args.only

    pos_out = POS_DIR / f"vao_selfcheck_{case_filter}.txt"
    cmd_pos = [
        str(EXE),
        "--vao-selfcheck", str(pos_out),
        "--vao-selfcheck-case", case_filter
    ]
    res_pos = subprocess.run(cmd_pos, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_pos.returncode == 0, f"Positive VAO self-check failed with code {res_pos.returncode}: {res_pos.stderr}"
    assert pos_out.exists() and pos_out.stat().st_size > 0, "Positive VAO self-check output missing or empty"
    pos_content = pos_out.read_text(encoding="utf-8")
    assert "Result: PASS" in pos_content, f"Report did not contain Result: PASS:\n{pos_content}"
    assert "FAIL:" not in pos_content, f"Report contained failures:\n{pos_content}"
    print(f"  PASS: Positive VAO self-check ({case_filter}) returned exit code 0; report written to {pos_out.name}")

    if args.only in [None, "selfcheck"]:
        # Negative Case 1: Forced failure
        neg_fail_out = NEG_DIR / "vao_forced_fail.txt"
        cmd_neg = [
            str(EXE),
            "--vao-selfcheck", str(neg_fail_out),
            "--selfcheck-force-failure"
        ]
        res_neg = subprocess.run(cmd_neg, cwd=REPO, capture_output=True, text=True, timeout=30)
        assert res_neg.returncode == 1, f"Expected returncode 1 for forced failure, got {res_neg.returncode}"
        assert neg_fail_out.exists() and neg_fail_out.stat().st_size > 0, "Forced failure report missing"
        neg_content = neg_fail_out.read_text(encoding="utf-8")
        assert "Result: FAIL" in neg_content, f"Expected Result: FAIL in forced failure report:\n{neg_content}"
        print(f"  PASS: Forced failure correctly produced exit code 1 and recorded FAIL report.")

        # Negative Case 2: Unwritable path
        unwritable_path = Path("Z:/nonexistent/deep/directory/cannot_write.txt")
        cmd_unwritable = [
            str(EXE),
            "--vao-selfcheck", str(unwritable_path)
        ]
        res_unwritable = subprocess.run(cmd_unwritable, cwd=REPO, capture_output=True, text=True, timeout=30)
        assert res_unwritable.returncode == 2, f"Expected returncode 2 for unwritable path, got {res_unwritable.returncode}"
        print(f"  PASS: Unwritable report destination correctly produced exit code 2.")

# ======================================================================
# Step 2: Steady-State Profiling & Driver Call Elimination
# ======================================================================
if args.only in [None, "benchmark"]:
    print("\n[Step 2] Running Steady-State Benchmark & Profiling Counter Verification...")
    bench_out = POS_DIR / "steady_state_bench.json"
    cmd_bench = [
        str(EXE),
        "--isolated",
        "--benchmark",
        "--benchmark-warmup", "8",
        "--benchmark-frames", "30",
        "--benchmark-output", str(bench_out),
        "--exit-after-benchmark"
    ]
    res_bench = subprocess.run(cmd_bench, cwd=REPO, capture_output=True, text=True, timeout=45)
    assert res_bench.returncode == 0, f"Benchmark run failed with code {res_bench.returncode}: {res_bench.stderr}"
    assert bench_out.exists() and bench_out.stat().st_size > 0, "Benchmark output missing"

    with open(bench_out, "r", encoding="utf-8") as f:
        bench_data = json.load(f)

    measured = bench_data.get("measuredFrames", [])
    assert len(measured) == 30, f"Expected 30 measured frames, got {len(measured)}"

    total_vao_creations = 0
    total_vao_deletions = 0
    for frame_entry in measured:
        counters = frame_entry.get("counters", {})
        c = counters.get("vaoCreations", 0)
        d = counters.get("vaoDeletions", 0)
        assert c == 0, f"Spurious VAO creation ({c}) on frame {frame_entry.get('frame')}"
        assert d == 0, f"Spurious VAO deletion ({d}) on frame {frame_entry.get('frame')}"
        total_vao_creations += c
        total_vao_deletions += d

    print(f"  PASS: Steady-state frames (30 frames): vaoCreations = {total_vao_creations}, vaoDeletions = {total_vao_deletions}")
    print(f"  PASS: Zero VAO creation/deletion overhead in steady state confirmed.")

# ======================================================================
# Step 3: Multi-Mesh Frame Dumps & Visual Correctness
# ======================================================================
if args.only in [None, "visual-dumps"]:
    print("\n[Step 3] Running Visual Frame Dumps with Differing Attribute Sets...")

    models = [
        ("HelloCube", REPO / "resources" / "sampleGltf" / "HelloCube.glb"),
        ("MultiPrimitive", REPO / "OpenGL-Renderer" / "Tools" / "tools" / "operations" / "tests" / "resources" / "MultiPrimitive" / "glTF" / "MultiPrimitive.gltf"),
        ("VertexColors", REPO / "OpenGL-Renderer" / "Tools" / "tools" / "operations" / "tests" / "resources" / "VC" / "glTF" / "VC.gltf"),
    ]

    for model_name, model_path in models:
        assert model_path.exists(), f"Model {model_path} missing"
        dump_path = CANDIDATE_DIR / f"frame_{model_name.lower()}.png"
        cmd_dump = [
            str(EXE),
            "--isolated",
            "--window-size", "1280x720",
            str(model_path),
            "--dump-frame", str(dump_path),
            "--exit-after-dump"
        ]
        res_dump = subprocess.run(cmd_dump, cwd=REPO, capture_output=True, text=True, timeout=30)
        assert res_dump.returncode == 0, f"Dump failed for {model_name} with code {res_dump.returncode}: {res_dump.stderr}"
        assert dump_path.exists() and dump_path.stat().st_size > 0, f"Dump {dump_path} missing or empty"

        with Image.open(dump_path) as img:
            assert img.size == (1280, 720), f"Unexpected dimensions {img.size} for {dump_path}"
            colors = len(set(img.convert("RGB").getdata()))
            # Nonblank verification: distinct color threshold
            assert colors > 100, f"Frame {dump_path} appears blank or degenerate ({colors} distinct colors)"
            print(f"  PASS: Dumped {model_name} (size: {img.size}, bytes: {dump_path.stat().st_size:,}, distinct colors: {colors:,})")

# ======================================================================
# Step 4: Bit-for-Bit Image Comparison Gate (6 AA Modes)
# ======================================================================
if args.only in [None, "image-gate"]:
    print("\n[Step 4] Running 6-Mode AA Exact Equality Image Comparison Gate...")
    modes = ["none", "msaa2", "msaa4", "msaa8", "fxaa", "taa"]
    comparison_log = []

    for mode in modes:
        cand_img = CANDIDATE_DIR / f"startup_{mode}.png"
        cmd_img = [
            str(EXE),
            "--isolated",
            "--window-size", "1280x720",
            "--aa", mode,
            "--dump-frame", str(cand_img),
            "--exit-after-dump"
        ]
        res_img = subprocess.run(cmd_img, cwd=REPO, capture_output=True, text=True, timeout=30)
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
print("P3.3 VALIDATION SUITE: ALL CHECKS PASSED SUCCESSFULLY!")
print("=" * 72)
