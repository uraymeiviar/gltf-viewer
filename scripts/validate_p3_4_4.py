# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

import argparse
import json
import os
import subprocess
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
PYTHON_EXE = sys.executable
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
P3_4_4_DIR = REPO / "build" / "p3.4.4"
POS_DIR = P3_4_4_DIR / "selfchecks_positive"
NEG_DIR = P3_4_4_DIR / "selfchecks_negative"
CANDIDATE_DIR = P3_4_4_DIR / "candidate"

POS_DIR.mkdir(parents=True, exist_ok=True)
NEG_DIR.mkdir(parents=True, exist_ok=True)
CANDIDATE_DIR.mkdir(parents=True, exist_ok=True)

VALID_ONLY = [
    "selfcheck", "transitions", "correctness", "boundary", "integration",
    "benefit", "parser", "visual-dumps"
]

parser = argparse.ArgumentParser(description="P3.4.4 CommandExecutor Depth Test and Depth Func Cache Validation Suite")
parser.add_argument("--only", choices=VALID_ONLY, help="Run only the specified focused scenario")
args = parser.parse_args()

print("=" * 72)
print("P3.4.4 CommandExecutor Depth Test & Depth Func Cache - Validation Suite")
if args.only:
    print(f"Focused Mode: Running ONLY '{args.only}'")
print("=" * 72)

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
    "Cmd 1 (Enabled): Initial depth test enable issued glEnable(GL_DEPTH_TEST) (enableCalls=1)",
    "Cmd 1 (Enabled): Initial depth func issued glDepthFunc(GL_LEQUAL) (depthFuncCalls=1)",
    "Cmd 1: Executor cached depthTestEnabled == true",
    "Cmd 1: Executor cached depthFunc == GL_LEQUAL",
    "Cmd 2 (Enabled -> Enabled): Redundant depth test enable suppressed (enableCalls remains 1)",
    "Cmd 2 (Enabled -> Enabled): Redundant depth func suppressed (depthFuncCalls remains 1)",
    "Cmd 3 (Enabled -> Disabled): Issued glDisable(GL_DEPTH_TEST) (enableCalls=2)",
    "Cmd 3 (Enabled -> Disabled): Disabling depth test does not call glDepthFunc (depthFuncCalls remains 1)",
    "Cmd 3: Executor cached depthTestEnabled == false",
    "Cmd 3: Disabling depth test retains previously established depth func in shadow state",
    "Cmd 4 (Disabled -> Disabled): Redundant depth test disable suppressed (enableCalls remains 2)",
    "Cmd 4 (Disabled -> Disabled): Depth func call suppressed (depthFuncCalls remains 1)",
    "Cmd 5 (Disabled -> Enabled): Re-enabling depth test issued glEnable(GL_DEPTH_TEST) (enableCalls=3)",
    "Cmd 5 (Disabled -> Enabled): Depth func already known as GL_LEQUAL, glDepthFunc suppressed (depthFuncCalls remains 1)",
    "ResetState: depthTestEnabled invalidated to nullopt",
    "ResetState: depthFunc invalidated to nullopt",
    "Cmd 6 (Post-Reset Enabled): First draw re-issues glEnable(GL_DEPTH_TEST) (enableCalls=4)",
    "Cmd 6 (Post-Reset Enabled): First draw re-issues glDepthFunc(GL_LEQUAL) (depthFuncCalls=2)",
    "Cmd 7 (Post-Reset Disabled): Issued glDisable(GL_DEPTH_TEST) (enableCalls=5)",
    "Cmd 7 (Post-Reset Disabled): Did not call glDepthFunc (depthFuncCalls remains 2)",
    "Cmd 7: Initially disabled command does NOT mark depth func as known (remains nullopt)",
    "Cmd 8 (Unknown DepthFunc Re-Enabled): Issued glEnable(GL_DEPTH_TEST) (enableCalls=6)",
    "Cmd 8 (Unknown DepthFunc Re-Enabled): Depth func was unknown, issued glDepthFunc(GL_LEQUAL) (depthFuncCalls=3)",
    "ExternalFunction: Callback invalidated depthTestEnabled to nullopt",
    "ExternalFunction: Callback invalidated depthFunc to nullopt",
    "Cmd 9 (Post-Callback Restoration): Re-established glEnable(GL_DEPTH_TEST) (enableCalls=7)",
    "Cmd 9 (Post-Callback Restoration): Re-established glDepthFunc(GL_LEQUAL) (depthFuncCalls=4)",
    "Driver query confirms GL_DEPTH_TEST == GL_TRUE",
    "Driver query confirms GL_DEPTH_FUNC == GL_LEQUAL",
    "Actual wrapper calls total 11 (7 enable/disable + 4 depthFunc) vs 15 baseline"
]

def run_parser_negative_checks():
    print("\n[Parser Verification] Running deterministic report-validation checks...")

    # Case A: Valid synthetic report
    valid_synthetic = "P3.4.4 self-check\n" + "\n".join(f"  PASS: {item}" for item in REQUIRED_TRANSITION_ASSERTIONS) + "\nResult: PASS\n"
    assert validate_report_text(valid_synthetic, REQUIRED_TRANSITION_ASSERTIONS) == True, "Valid synthetic report was unexpectedly rejected"
    print("  PASS: Valid synthetic passing report accepted.")

    # Case B: Reviewer reproducer (FAIL entries and Result: FAIL)
    failing_synthetic = "P3.4.4 self-check\n" + "\n".join(f"  FAIL: {item}" for item in REQUIRED_TRANSITION_ASSERTIONS) + "\nResult: FAIL\n"
    rejected_fail = False
    try:
        validate_report_text(failing_synthetic, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_fail = True
    assert rejected_fail, "Validation logic failed to reject report containing FAIL entries and Result: FAIL"
    print("  PASS: Synthetic report containing FAIL: entries and Result: FAIL was correctly rejected.")

    # Case C: Incomplete report missing required assertions
    incomplete_synthetic = "P3.4.4 self-check\n  PASS: Cmd 1 (Enabled): Initial depth test enable issued glEnable(GL_DEPTH_TEST) (enableCalls=1)\nResult: PASS\n"
    rejected_incomplete = False
    try:
        validate_report_text(incomplete_synthetic, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_incomplete = True
    assert rejected_incomplete, "Validation logic failed to reject incomplete report missing required assertions"
    print("  PASS: Incomplete report missing required assertions was correctly rejected.")

    # Case D: Missing report file
    nonexistent_path = NEG_DIR / "nonexistent_depth_report.txt"
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
    empty_path = NEG_DIR / "empty_depth_report.txt"
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
    print("\n[Step 1] Running Depth Test Diagnostic Self-Check Suite...")

    case_filter = "all"
    if args.only in SELFCHECK_CASES:
        case_filter = args.only

    pos_out = POS_DIR / f"depth_selfcheck_{case_filter}.txt"
    if pos_out.exists():
        pos_out.unlink()

    cmd_pos = [
        str(EXE),
        "--depth-selfcheck", str(pos_out),
        "--depth-selfcheck-case", case_filter
    ]
    res_pos = subprocess.run(cmd_pos, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_pos.returncode == 0, f"Positive depth self-check failed with code {res_pos.returncode}: {res_pos.stderr}"
    validate_selfcheck_report(pos_out)
    print(f"  PASS: Positive depth self-check ({case_filter}) returned exit code 0; report written to {pos_out.name}")

    if args.only in [None, "selfcheck"]:
        # Negative Case 1: Forced failure
        neg_fail_out = NEG_DIR / "depth_forced_fail.txt"
        if neg_fail_out.exists():
            neg_fail_out.unlink()
        cmd_neg = [
            str(EXE),
            "--depth-selfcheck", str(neg_fail_out),
            "--selfcheck-force-failure"
        ]
        res_neg = subprocess.run(cmd_neg, cwd=REPO, capture_output=True, text=True, timeout=30)
        assert res_neg.returncode == 1, f"Expected returncode 1 for forced failure, got {res_neg.returncode}"
        assert neg_fail_out.exists() and neg_fail_out.stat().st_size > 0, "Forced failure report missing"
        neg_content = neg_fail_out.read_text(encoding="utf-8")
        assert "Result: FAIL" in neg_content, f"Expected Result: FAIL in forced failure report:\n{neg_content}"
        print("  PASS: Forced failure correctly produced exit code 1 and recorded FAIL report.")

        # Negative Case 2: Unwritable path
        unwritable_path = Path("Z:/nonexistent/deep/directory/cannot_write.txt")
        cmd_unwritable = [
            str(EXE),
            "--depth-selfcheck", str(unwritable_path)
        ]
        res_unwritable = subprocess.run(cmd_unwritable, cwd=REPO, capture_output=True, text=True, timeout=30)
        assert res_unwritable.returncode == 2, f"Expected returncode 2 for unwritable path, got {res_unwritable.returncode}"
        print("  PASS: Unwritable report destination correctly produced exit code 2.")

# ======================================================================
# Step 2: Benefit & Call Suppression Quantification (and Parser Checks)
# ======================================================================
if args.only in [None, "benefit", "parser"]:
    run_parser_negative_checks()

if args.only in [None, "benefit"]:
    print("\n[Step 2] Evaluating Call Suppression & Call-Count Reduction...")

    benefit_out = POS_DIR / "depth_benefit_transitions.txt"
    if benefit_out.exists():
        benefit_out.unlink()

    cmd_benefit = [
        str(EXE),
        "--depth-selfcheck", str(benefit_out),
        "--depth-selfcheck-case", "transitions"
    ]
    res_benefit = subprocess.run(cmd_benefit, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_benefit.returncode == 0, f"Transitions self-check failed with code {res_benefit.returncode}: {res_benefit.stderr}"

    validate_selfcheck_report(benefit_out, REQUIRED_TRANSITION_ASSERTIONS)

    print("  PASS: Fresh transitions self-check executed against current binary with exit code 0.")
    print(f"  PASS: Report written to {benefit_out.name} and validated: Result: PASS, zero FAIL entries, all transition assertions passed.")
    print("  Quantification (9-command sequence with state invalidations and transitions):")
    print("    - Enable/disable calls: 7 actual vs 9 baseline (2 calls suppressed)")
    print("    - Depth function calls: 4 actual vs 6 baseline (2 calls suppressed)")
    print("    - Total calls: 11 actual calls vs 15 baseline calls (4 calls suppressed -> 26.7% reduction).")
    print("  Uninterrupted sequence quantification (e.g. 10 consecutive enabled commands):")
    print("    - Enable/disable calls: 1 actual vs 10 baseline (9 calls suppressed)")
    print("    - Depth function calls: 1 actual vs 10 baseline (9 calls suppressed)")
    print("    - Total calls: 2 actual vs 20 baseline (18 calls suppressed -> 90.0% reduction).")
    print("  Suppression applies to uninterrupted sequences requesting matching depth state after invalidation.")
    print("  Multiple renderer calls, callbacks, or state transitions re-issue driver calls as required.")

# ======================================================================
# Step 3: Visual Dump & Frame Capture
# ======================================================================
if args.only in [None, "visual-dumps"]:
    print("\n[Step 3] Running Visual Frame Dump on HelloCube.glb...")
    cube_img_path = CANDIDATE_DIR / "depth_hellocube.jpg"
    if cube_img_path.exists():
        cube_img_path.unlink()

    cmd_dump = [
        str(EXE),
        "resources/sampleGltf/HelloCube.glb",
        "--isolated",
        "--dump-frame", str(cube_img_path),
        "--exit-after-dump"
    ]
    res_dump = subprocess.run(cmd_dump, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_dump.returncode == 0, f"HelloCube dump failed: {res_dump.stderr}"
    assert cube_img_path.exists() and cube_img_path.stat().st_size > 0, "HelloCube frame dump missing or empty"
    with Image.open(cube_img_path) as img:
        w, h = img.size
        assert w == 1600 and h == 900, f"Unexpected frame dimensions: {w}x{h}"
    print(f"  PASS: HelloCube.glb visual frame captured: {cube_img_path.name} (1600x900, {cube_img_path.stat().st_size} bytes)")

# ======================================================================
# Step 4: Boundary Regressions Check
# ======================================================================
if args.only is None:
    print("\n[Step 4] Checking Shared Invalidation Boundaries across Prior Substeps...")
    for script, flag in [
        ("validate_p3_4.py", "benefit"),
        ("validate_p3_4_1.py", "benefit"),
        ("validate_p3_3.py", "boundary"),
        ("validate_p3_2.py", "boundary")
    ]:
        cmd_reg = [PYTHON_EXE, str(REPO / "scripts" / script), "--only", flag]
        res_reg = subprocess.run(cmd_reg, cwd=REPO, capture_output=True, text=True, timeout=40)
        assert res_reg.returncode == 0, f"Regression check {script} failed: {res_reg.stderr}"
        print(f"  PASS: Regression check {script} --only {flag} exited with code 0.")

print("\n" + "=" * 72)
print("P3.4.4 Depth Test & Depth Func Cache Validation: ALL CHECKS PASSED")
print("=" * 72)
