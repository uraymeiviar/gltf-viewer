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
P3_4_DIR = REPO / "build" / "p3.4"
POS_DIR = P3_4_DIR / "selfchecks_positive"
NEG_DIR = P3_4_DIR / "selfchecks_negative"
CANDIDATE_DIR = P3_4_DIR / "candidate"

POS_DIR.mkdir(parents=True, exist_ok=True)
NEG_DIR.mkdir(parents=True, exist_ok=True)
CANDIDATE_DIR.mkdir(parents=True, exist_ok=True)

VALID_ONLY = [
    "selfcheck", "transitions", "boundary", "renderer-boundary", "visual",
    "benefit", "parser", "visual-dumps"
]

parser = argparse.ArgumentParser(description="P3.4.1 CommandExecutor Polygon Mode Cache Validation Suite")
parser.add_argument("--only", choices=VALID_ONLY, help="Run only the specified focused scenario")
args = parser.parse_args()

print("=" * 72)
print("P3.4.1 CommandExecutor Polygon Mode Cache - Validation Suite")
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

def validate_report_text(content, required_passed_assertions=None):
    """
    Validates self-check report text:
    - Must contain 'Result: PASS'.
    - Must NOT contain 'FAIL:'.
    - If required_passed_assertions is specified, each item must appear as 'PASS: <item>'.
    """
    if "Result: PASS" not in content:
        raise AssertionError(f"Report missing 'Result: PASS' marker:\n{content}")
    if "FAIL:" in content:
        raise AssertionError(f"Report contains failure entries:\n{content}")
    if required_passed_assertions:
        for item in required_passed_assertions:
            expected_pass = f"PASS: {item}"
            if expected_pass not in content:
                raise AssertionError(f"Required passing assertion not found: '{expected_pass}'")
    return True

def validate_selfcheck_report(report_path, required_passed_assertions=None):
    """
    Validates a self-check report file on disk:
    - Path must exist and have non-zero size.
    - Content passes validate_report_text.
    """
    path = Path(report_path)
    if not path.exists():
        raise AssertionError(f"Report file does not exist: {path}")
    if path.stat().st_size == 0:
        raise AssertionError(f"Report file is empty: {path}")
    content = path.read_text(encoding="utf-8")
    return validate_report_text(content, required_passed_assertions)

REQUIRED_TRANSITION_ASSERTIONS = [
    "Draw 1 (Fill): First draw issued glPolygonMode call (count=1)",
    "Draw 2 (Fill): Redundant Fill draw suppressed glPolygonMode call (count remains 1)",
    "Draw 3 (Line): Transition from Fill to Line issued glPolygonMode call (count=2)",
    "Draw 4 (Line): Redundant Line draw suppressed glPolygonMode call (count remains 2)",
    "Draw 5 (Fill): Transition from Line to Fill issued glPolygonMode call (count=3)",
    "resetState() invalidated activePolygonMode cache",
    "Draw 6 (Fill): First draw after reset issued glPolygonMode call despite matching pre-reset mode (count=4)"
]

def run_parser_negative_checks():
    print("\n[Parser Verification] Running deterministic report-validation checks...")

    # Case A: Valid synthetic report must pass
    valid_synthetic = "P3.4.1 self-check\n" + "\n".join(f"  PASS: {item}" for item in REQUIRED_TRANSITION_ASSERTIONS) + "\nResult: PASS\n"
    assert validate_report_text(valid_synthetic, REQUIRED_TRANSITION_ASSERTIONS) == True, "Valid synthetic report was unexpectedly rejected"
    print("  PASS: Valid synthetic passing report accepted.")

    # Case B: Report with FAIL entries and Result: FAIL (reviewer reproducer) must be rejected
    failing_synthetic = "P3.4.1 self-check\n" + "\n".join(f"  FAIL: {item}" for item in REQUIRED_TRANSITION_ASSERTIONS) + "\nResult: FAIL\n"
    rejected_fail = False
    try:
        validate_report_text(failing_synthetic, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_fail = True
    assert rejected_fail, "Validation logic failed to reject report containing FAIL entries and Result: FAIL"
    print("  PASS: Synthetic report containing FAIL: entries and Result: FAIL was correctly rejected.")

    # Case C: Incomplete report missing required assertions must be rejected
    incomplete_synthetic = "P3.4.1 self-check\n  PASS: Draw 1 (Fill): First draw issued glPolygonMode call (count=1)\nResult: PASS\n"
    rejected_incomplete = False
    try:
        validate_report_text(incomplete_synthetic, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_incomplete = True
    assert rejected_incomplete, "Validation logic failed to reject incomplete report missing required assertions"
    print("  PASS: Incomplete report missing required assertions was correctly rejected.")

    # Case D: Missing report file on disk must be rejected
    nonexistent_path = NEG_DIR / "nonexistent_synthetic_report.txt"
    if nonexistent_path.exists():
        nonexistent_path.unlink()
    rejected_missing = False
    try:
        validate_selfcheck_report(nonexistent_path, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_missing = True
    assert rejected_missing, "Validation logic failed to reject nonexistent report file"
    print("  PASS: Missing report file was correctly rejected.")

    # Case E: Empty report file on disk must be rejected
    empty_path = NEG_DIR / "empty_synthetic_report.txt"
    empty_path.write_text("", encoding="utf-8")
    rejected_empty = False
    try:
        validate_selfcheck_report(empty_path, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_empty = True
    assert rejected_empty, "Validation logic failed to reject empty report file"
    print("  PASS: Empty report file was correctly rejected.")

# ======================================================================
# Step 1: Self-Checks (Positive, Focused Filters, Negative Tests)
# ======================================================================
SELFCHECK_CASES = ["transitions", "boundary", "renderer-boundary", "visual"]

if args.only in [None, "selfcheck"] + SELFCHECK_CASES:
    print("\n[Step 1] Running Polygon Mode Diagnostic Self-Check Suite...")

    case_filter = "all"
    if args.only in SELFCHECK_CASES:
        case_filter = args.only

    pos_out = POS_DIR / f"polygon_selfcheck_{case_filter}.txt"
    if pos_out.exists():
        pos_out.unlink()

    cmd_pos = [
        str(EXE),
        "--polygon-mode-selfcheck", str(pos_out),
        "--polygon-mode-selfcheck-case", case_filter
    ]
    res_pos = subprocess.run(cmd_pos, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_pos.returncode == 0, f"Positive polygon mode self-check failed with code {res_pos.returncode}: {res_pos.stderr}"
    validate_selfcheck_report(pos_out)
    print(f"  PASS: Positive polygon mode self-check ({case_filter}) returned exit code 0; report written to {pos_out.name}")

    if args.only in [None, "selfcheck"]:
        # Negative Case 1: Forced failure
        neg_fail_out = NEG_DIR / "polygon_forced_fail.txt"
        if neg_fail_out.exists():
            neg_fail_out.unlink()
        cmd_neg = [
            str(EXE),
            "--polygon-mode-selfcheck", str(neg_fail_out),
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
            "--polygon-mode-selfcheck", str(unwritable_path)
        ]
        res_unwritable = subprocess.run(cmd_unwritable, cwd=REPO, capture_output=True, text=True, timeout=30)
        assert res_unwritable.returncode == 2, f"Expected returncode 2 for unwritable path, got {res_unwritable.returncode}"
        print(f"  PASS: Unwritable report destination correctly produced exit code 2.")

# ======================================================================
# Step 2: Benefit & Call Suppression Quantification (and Parser Checks)
# ======================================================================
if args.only in [None, "benefit", "parser"]:
    run_parser_negative_checks()

if args.only in [None, "benefit"]:
    print("\n[Step 2] Evaluating Call Suppression & Call-Count Reduction...")

    # Execute a fresh transitions self-check against the current executable,
    # preventing stale report reuse from masking a failed execution.
    benefit_out = POS_DIR / "polygon_benefit_transitions.txt"
    if benefit_out.exists():
        benefit_out.unlink()

    cmd_benefit = [
        str(EXE),
        "--polygon-mode-selfcheck", str(benefit_out),
        "--polygon-mode-selfcheck-case", "transitions"
    ]
    res_benefit = subprocess.run(cmd_benefit, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_benefit.returncode == 0, f"Transitions self-check failed with code {res_benefit.returncode}: {res_benefit.stderr}"

    validate_selfcheck_report(benefit_out, REQUIRED_TRANSITION_ASSERTIONS)

    print("  PASS: Fresh transitions self-check executed against current binary with exit code 0.")
    print(f"  PASS: Report written to {benefit_out.name} and validated: Result: PASS, zero FAIL entries, all transition assertions passed.")
    print("  PASS: Call suppression verified across consecutive Fill->Fill and Line->Line draws.")
    print("  PASS: Invalidation boundary verified across resetState().")
    print("  Quantification: In an uninterrupted 5-draw alternating sequence (Fill, Fill, Line, Line, Fill),")
    print("  calls were reduced from 5 to 3 (40.0% reduction).")
    print("  Suppression applies to an uninterrupted sequence of draws requesting the same mode after invalidation.")
    print("  Multiple renderer calls, callbacks, or mode transitions will re-issue driver calls as required.")

# ======================================================================
# Step 3: Visual Dump & Image Differentiation Gate
# ======================================================================
if args.only in [None, "visual-dumps"]:
    print("\n[Step 3] Running Visual Frame Dumps & Visual Differentiation Verification...")
    solid_img_path = CANDIDATE_DIR / "frame_solid.jpg"
    wire_img_path = CANDIDATE_DIR / "frame_wireframe.jpg"

    cmd_solid = [
        str(EXE),
        "--isolated",
        "--render-mode", "default",
        "--dump-frame", str(solid_img_path),
        "--exit-after-dump"
    ]
    res_solid = subprocess.run(cmd_solid, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_solid.returncode == 0, f"Solid dump failed with code {res_solid.returncode}: {res_solid.stderr}"
    assert solid_img_path.exists() and solid_img_path.stat().st_size > 0, "Solid frame dump missing or empty"

    cmd_wire = [
        str(EXE),
        "--isolated",
        "--render-mode", "wireframe",
        "--dump-frame", str(wire_img_path),
        "--exit-after-dump"
    ]
    res_wire = subprocess.run(cmd_wire, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_wire.returncode == 0, f"Wireframe dump failed with code {res_wire.returncode}: {res_wire.stderr}"
    assert wire_img_path.exists() and wire_img_path.stat().st_size > 0, "Wireframe frame dump missing or empty"

    img_solid = Image.open(solid_img_path)
    img_wire = Image.open(wire_img_path)

    total_px, diff_px, diff_pct, max_chan, avg_diff = compare_image_pixels(img_solid, img_wire)

    print(f"  Frame resolution: {img_solid.size[0]}x{img_solid.size[1]} ({total_px} total pixels)")
    print(f"  Differing pixels: {diff_px} ({diff_pct:.2f}%)")
    print(f"  Max channel diff: {max_chan}")
    print(f"  Average diff:     {avg_diff:.2f}")

    # Solid vs wireframe must have significant visible distinction (>5% pixel difference, max channel diff == 255)
    assert diff_px > 50000, f"Expected >50,000 differing pixels between solid and wireframe, got {diff_px}"
    assert max_chan >= 200, f"Expected substantial channel difference, got {max_chan}"

    print("  PASS: Solid fill vs wireframe line rasterization confirmed visibly distinct on candidate binary.")

print("\n" + "=" * 72)
print("P3.4.1 Polygon Mode Cache Validation: ALL CHECKS PASSED")
print("=" * 72)
