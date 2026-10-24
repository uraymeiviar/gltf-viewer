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
FIXTURES_DIR = P3_4_DIR / "fixtures"

POS_DIR.mkdir(parents=True, exist_ok=True)
NEG_DIR.mkdir(parents=True, exist_ok=True)
CANDIDATE_DIR.mkdir(parents=True, exist_ok=True)
FIXTURES_DIR.mkdir(parents=True, exist_ok=True)

VALID_ONLY = [
    "selfcheck", "transitions", "correctness", "boundary", "integration",
    "benefit", "parser", "visual-dumps"
]

parser = argparse.ArgumentParser(description="P3.4.2 + P3.4.3 CommandExecutor Culling and Winding Cache Validation Suite")
parser.add_argument("--only", choices=VALID_ONLY, help="Run only the specified focused scenario")
args = parser.parse_args()

print("=" * 72)
print("P3.4.2 + P3.4.3 CommandExecutor Culling & Winding Cache - Validation Suite")
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
    path = Path(report_path)
    if not path.exists():
        raise AssertionError(f"Report file does not exist: {path}")
    if path.stat().st_size == 0:
        raise AssertionError(f"Report file is empty: {path}")
    content = path.read_text(encoding="utf-8")
    return validate_report_text(content, required_passed_assertions)

REQUIRED_TRANSITION_ASSERTIONS = [
    "Draw 1: Initial culling enable issued glEnable(GL_CULL_FACE) (enableCalls=1)",
    "Draw 1: Initial cull-face selection issued glCullFace(GL_BACK) (selectionCalls=1)",
    "Draw 1: Initial front-face winding issued glFrontFace(GL_CCW) (frontFaceCalls=1)",
    "Draw 2: Redundant culling enable suppressed (remains 1)",
    "Draw 2: Redundant cull-face selection suppressed (remains 1)",
    "Draw 2: Redundant front-face winding suppressed (remains 1)",
    "Draw 3: Transition to Culling::None issued glDisable(GL_CULL_FACE) (enableCalls=2)",
    "Draw 3: Disabling culling did not touch selected cull face (selectionCalls remains 1)",
    "Draw 3: Winding unchanged, suppressed glFrontFace (frontFaceCalls remains 1)",
    "Draw 4: Redundant disable suppressed (enableCalls remains 2)",
    "Draw 4: Cull face untouched while disabled (selectionCalls remains 1)",
    "Draw 4: Winding transition while disabled issued glFrontFace(GL_CW) (frontFaceCalls=2)",
    "Draw 5: Re-enabling culling issued glEnable(GL_CULL_FACE) (enableCalls=3)",
    "Draw 5: Cull face matches driver state, suppressed glCullFace (selectionCalls remains 1)",
    "Draw 5: Winding matches established CW state, suppressed glFrontFace (frontFaceCalls remains 2)",
    "Draw 6: Culling already enabled, suppressed glEnable (enableCalls remains 3)",
    "Draw 6: Transition from Back to Front issued glCullFace(GL_FRONT) (selectionCalls=2)",
    "Draw 6: Winding unchanged, suppressed glFrontFace (frontFaceCalls remains 2)",
    "Draw 7: Redundant enable suppressed (enableCalls remains 3)",
    "Draw 7: Redundant Front selection suppressed (selectionCalls remains 2)",
    "Draw 7: Redundant winding suppressed (frontFaceCalls remains 2)",
    "Draw 8: Disabling culling issued glDisable(GL_CULL_FACE) (enableCalls=4)",
    "Draw 8: Cull face untouched while disabled (selectionCalls remains 2)",
    "Draw 8: Winding unchanged (frontFaceCalls remains 2)",
    "Draw 9: Re-enabling culling issued glEnable(GL_CULL_FACE) (enableCalls=5)",
    "Draw 9: Cull face still Front, suppressed glCullFace (selectionCalls remains 2)",
    "Draw 9: Winding transition to CCW issued glFrontFace(GL_CCW) (frontFaceCalls=3)",
    "Draw 10: Culling already enabled, suppressed glEnable (enableCalls remains 5)",
    "Draw 10: Transition to Back issued glCullFace(GL_BACK) (selectionCalls=3)",
    "Draw 10: Winding unchanged, suppressed glFrontFace (frontFaceCalls remains 3)",
    "resetState() invalidated cullingEnabled cache",
    "resetState() invalidated cullFace cache",
    "resetState() invalidated activeWinding cache",
    "Draw 11: First draw after reset re-issued glEnable(GL_CULL_FACE) (enableCalls=6)",
    "Draw 11: First draw after reset re-issued glCullFace(GL_BACK) (selectionCalls=4)",
    "Draw 11: First draw after reset re-issued glFrontFace(GL_CCW) (frontFaceCalls=4)"
]

def run_parser_negative_checks():
    print("\n[Parser Verification] Running deterministic report-validation checks...")

    # Case A: Valid synthetic report
    valid_synthetic = "P3.4.2+3 self-check\n" + "\n".join(f"  PASS: {item}" for item in REQUIRED_TRANSITION_ASSERTIONS) + "\nResult: PASS\n"
    assert validate_report_text(valid_synthetic, REQUIRED_TRANSITION_ASSERTIONS) == True, "Valid synthetic report was unexpectedly rejected"
    print("  PASS: Valid synthetic passing report accepted.")

    # Case B: Reviewer reproducer (FAIL entries and Result: FAIL)
    failing_synthetic = "P3.4.2+3 self-check\n" + "\n".join(f"  FAIL: {item}" for item in REQUIRED_TRANSITION_ASSERTIONS) + "\nResult: FAIL\n"
    rejected_fail = False
    try:
        validate_report_text(failing_synthetic, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_fail = True
    assert rejected_fail, "Validation logic failed to reject report containing FAIL entries and Result: FAIL"
    print("  PASS: Synthetic report containing FAIL: entries and Result: FAIL was correctly rejected.")

    # Case C: Incomplete report missing required assertions
    incomplete_synthetic = "P3.4.2+3 self-check\n  PASS: Draw 1: Initial culling enable issued glEnable(GL_CULL_FACE) (enableCalls=1)\nResult: PASS\n"
    rejected_incomplete = False
    try:
        validate_report_text(incomplete_synthetic, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_incomplete = True
    assert rejected_incomplete, "Validation logic failed to reject incomplete report missing required assertions"
    print("  PASS: Incomplete report missing required assertions was correctly rejected.")

    # Case D: Missing report file
    nonexistent_path = NEG_DIR / "nonexistent_cull_report.txt"
    if nonexistent_path.exists():
        nonexistent_path.unlink()
    rejected_missing = False
    try:
        validate_selfcheck_report(nonexistent_path, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_missing = True
    assert rejected_missing, "Validation logic failed to reject nonexistent report file"
    print("  PASS: Missing report file was correctly rejected.")

    # Case E: Empty report file
    empty_path = NEG_DIR / "empty_cull_report.txt"
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
SELFCHECK_CASES = ["transitions", "correctness", "boundary", "integration"]

if args.only in [None, "selfcheck"] + SELFCHECK_CASES:
    print("\n[Step 1] Running Cull & Winding Diagnostic Self-Check Suite...")

    if args.only in [None, "selfcheck", "boundary", "integration"]:
        subprocess.run([sys.executable, str(REPO / "scripts" / "generate_cull_winding_fixture.py")],
                       cwd=REPO, check=True)

    case_filter = "all"
    if args.only in SELFCHECK_CASES:
        case_filter = args.only

    pos_out = POS_DIR / f"cull_winding_selfcheck_{case_filter}.txt"
    if pos_out.exists():
        pos_out.unlink()

    cmd_pos = [
        str(EXE),
        "--cull-winding-selfcheck", str(pos_out),
        "--cull-winding-selfcheck-case", case_filter
    ]
    res_pos = subprocess.run(cmd_pos, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_pos.returncode == 0, f"Positive cull/winding self-check failed with code {res_pos.returncode}: {res_pos.stderr}"
    validate_selfcheck_report(pos_out)
    print(f"  PASS: Positive cull/winding self-check ({case_filter}) returned exit code 0; report written to {pos_out.name}")

    if args.only in [None, "selfcheck"]:
        # Negative Case 1: Forced failure
        neg_fail_out = NEG_DIR / "cull_winding_forced_fail.txt"
        if neg_fail_out.exists():
            neg_fail_out.unlink()
        cmd_neg = [
            str(EXE),
            "--cull-winding-selfcheck", str(neg_fail_out),
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
            "--cull-winding-selfcheck", str(unwritable_path)
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

    benefit_out = POS_DIR / "cull_winding_benefit_transitions.txt"
    if benefit_out.exists():
        benefit_out.unlink()

    cmd_benefit = [
        str(EXE),
        "--cull-winding-selfcheck", str(benefit_out),
        "--cull-winding-selfcheck-case", "transitions"
    ]
    res_benefit = subprocess.run(cmd_benefit, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_benefit.returncode == 0, f"Transitions self-check failed with code {res_benefit.returncode}: {res_benefit.stderr}"

    validate_selfcheck_report(benefit_out, REQUIRED_TRANSITION_ASSERTIONS)

    print("  PASS: Fresh transitions self-check executed against current binary with exit code 0.")
    print(f"  PASS: Report written to {benefit_out.name} and validated: Result: PASS, zero FAIL entries, all transition assertions passed.")
    print("  Quantification (10-draw sequence):")
    print("    - Enable/disable calls: 5 actual vs 10 baseline (5 calls suppressed)")
    print("    - Cull-face selection calls: 3 actual vs 7 baseline (4 calls suppressed)")
    print("    - Front-face winding calls: 3 actual vs 10 baseline (7 calls suppressed)")
    print("    - Total calls: 11 actual calls vs 27 baseline calls (16 calls suppressed -> 59.3% reduction).")
    print("  Suppression applies to uninterrupted sequences requesting matching culling/winding after invalidation.")
    print("  Multiple renderer calls, callbacks, or state transitions re-issue driver calls as required.")

# ======================================================================
# Step 3: Visual Dump & Image Differentiation Gate
# ======================================================================
if args.only in [None, "visual-dumps"]:
    print("\n[Step 3] Running Visual Frame Dumps & Viewport Mode Verification...")
    fixture_gltf = FIXTURES_DIR / "cull_winding_fixture.gltf"
    if not fixture_gltf.exists():
        subprocess.run([sys.executable, str(REPO / "scripts" / "generate_cull_winding_fixture.py")], cwd=REPO, check=True)

    std_img_path = CANDIDATE_DIR / "fixture_standard.jpg"
    ds_img_path = CANDIDATE_DIR / "fixture_double_sided.jpg"
    iw_img_path = CANDIDATE_DIR / "fixture_invert_winding.jpg"

    cmd_std = [
        str(EXE),
        str(fixture_gltf),
        "--isolated",
        "--dump-frame", str(std_img_path),
        "--exit-after-dump"
    ]
    res_std = subprocess.run(cmd_std, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_std.returncode == 0, f"Standard dump failed: {res_std.stderr}"

    cmd_ds = [
        str(EXE),
        str(fixture_gltf),
        "--isolated",
        "--force-double-sided",
        "--dump-frame", str(ds_img_path),
        "--exit-after-dump"
    ]
    res_ds = subprocess.run(cmd_ds, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_ds.returncode == 0, f"Force double-sided dump failed: {res_ds.stderr}"

    cmd_iw = [
        str(EXE),
        str(fixture_gltf),
        "--isolated",
        "--invert-winding",
        "--dump-frame", str(iw_img_path),
        "--exit-after-dump"
    ]
    res_iw = subprocess.run(cmd_iw, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_iw.returncode == 0, f"Invert winding dump failed: {res_iw.stderr}"

    img_std = Image.open(std_img_path)
    img_ds = Image.open(ds_img_path)
    img_iw = Image.open(iw_img_path)

    t_px, d_px_ds, pct_ds, max_ds, avg_ds = compare_image_pixels(img_std, img_ds)
    _, d_px_iw, pct_iw, max_iw, avg_iw = compare_image_pixels(img_std, img_iw)

    print(f"  Standard vs ForceDoubleSided: {d_px_ds} differing pixels ({pct_ds:.2f}%), max diff {max_ds}")
    print(f"  Standard vs InvertWinding:    {d_px_iw} differing pixels ({pct_iw:.2f}%), max diff {max_iw}")

    # Double sided must reveal Quad 2 (red back-facing quad)
    assert d_px_ds > 5000, f"Expected >5000 differing pixels for double-sided override, got {d_px_ds}"
    assert max_ds >= 90, f"Expected channel difference for double-sided override, got {max_ds}"

    # Invert winding must flip visibility of quads 1, 2, 4
    assert d_px_iw > 20000, f"Expected >20000 differing pixels for invert-winding override, got {d_px_iw}"
    assert max_iw >= 90, f"Expected channel difference for invert-winding override, got {max_iw}"

    print("  PASS: Single-sided, double-sided, and inverted winding behavior confirmed visibly distinct on candidate binary.")

print("\n" + "=" * 72)
print("P3.4.2 + P3.4.3 Culling & Winding Cache Validation: ALL CHECKS PASSED")
print("=" * 72)
