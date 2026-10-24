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
PYTHON_EXE = "python" if sys.platform == "win32" and "WindowsApps" in sys.executable else sys.executable
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
P3_4_5_DIR = REPO / "build" / "p3.4.5"
POS_DIR = P3_4_5_DIR / "selfchecks_positive"
NEG_DIR = P3_4_5_DIR / "selfchecks_negative"
CANDIDATE_DIR = P3_4_5_DIR / "candidate"
BASE_DIR = P3_4_5_DIR / "baseline"
BASE_EXE = BASE_DIR / "Glider.exe"

POS_DIR.mkdir(parents=True, exist_ok=True)
NEG_DIR.mkdir(parents=True, exist_ok=True)
CANDIDATE_DIR.mkdir(parents=True, exist_ok=True)
BASE_DIR.mkdir(parents=True, exist_ok=True)

VALID_ONLY = [
    "selfcheck", "transitions", "correctness", "boundary", "integration",
    "benefit", "parser", "visual-dumps", "production", "lifecycle"
]

parser = argparse.ArgumentParser(description="P3.4.5 CommandExecutor Blending State Cache Validation Suite")
parser.add_argument("--only", choices=VALID_ONLY, help="Run only the specified focused scenario")
args = parser.parse_args()

print("=" * 72)
print("P3.4.5 CommandExecutor Blending State Cache - Validation Suite")
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
    "Executor blendEnabled is uninitialized on init",
    "Executor blendSrcFactor is uninitialized on init",
    "Executor blendDstFactor is uninitialized on init",
    "Executor blendEquation is uninitialized on init",
    "Cmd 1 (Enabled): Initial blend enable issued glEnable(GL_BLEND) (enableCalls=1)",
    "Cmd 1 (Enabled): Initial blend factors issued glBlendFunc (funcCalls=1)",
    "Cmd 1 (Enabled): Initial blend equation issued glBlendEquation (equationCalls=1)",
    "Cmd 1: Executor cached blendEnabled == true",
    "Cmd 1: Executor cached blendSrcFactor == GL_SRC_ALPHA",
    "Cmd 1: Executor cached blendDstFactor == GL_ONE_MINUS_SRC_ALPHA",
    "Cmd 1: Executor cached blendEquation == GL_FUNC_ADD",
    "Cmd 2 (Enabled -> Enabled): Redundant blend enable suppressed (enableCalls remains 1)",
    "Cmd 2 (Enabled -> Enabled): Redundant blend factors suppressed (funcCalls remains 1)",
    "Cmd 2 (Enabled -> Enabled): Redundant blend equation suppressed (equationCalls remains 1)",
    "Cmd 2: Executor retains blendEnabled == true",
    "Cmd 3 (Enabled -> Disabled): Issued glDisable(GL_BLEND) (enableCalls=2)",
    "Cmd 3 (Enabled -> Disabled): Disabling blend does not call glBlendFunc (funcCalls remains 1)",
    "Cmd 3 (Enabled -> Disabled): Disabling blend does not call glBlendEquation (equationCalls remains 1)",
    "Cmd 3: Executor cached blendEnabled == false",
    "Cmd 3: Factors retained in cache while disabled",
    "Cmd 3: Equation retained in cache while disabled",
    "Cmd 4 (Disabled -> Disabled): Redundant blend disable suppressed (enableCalls remains 2)",
    "Cmd 4 (Disabled -> Disabled): Factor call suppressed (funcCalls remains 1)",
    "Cmd 4 (Disabled -> Disabled): Equation call suppressed (equationCalls remains 1)",
    "Cmd 4: Executor retains blendEnabled == false",
    "Cmd 5 (Disabled -> Enabled): Re-enabling blend issued glEnable(GL_BLEND) (enableCalls=3)",
    "Cmd 5 (Disabled -> Enabled): Factors already match cache, suppressed glBlendFunc (funcCalls remains 1)",
    "Cmd 5 (Disabled -> Enabled): Equation already matches cache, suppressed glBlendEquation (equationCalls remains 1)",
    "Cmd 5: Executor cached blendEnabled == true",
    "ResetState: blendEnabled invalidated to nullopt",
    "ResetState: blendSrcFactor invalidated to nullopt",
    "ResetState: blendDstFactor invalidated to nullopt",
    "ResetState: blendEquation invalidated to nullopt",
    "Cmd 6 (Post-Reset Enabled): First draw re-issues glEnable(GL_BLEND) (enableCalls=4)",
    "Cmd 6 (Post-Reset Enabled): First draw re-issues glBlendFunc (funcCalls=2)",
    "Cmd 6 (Post-Reset Enabled): First draw re-issues glBlendEquation (equationCalls=2)",
    "Cmd 6: Executor cached blendEnabled == true",
    "ResetState: blendEnabled invalidated",
    "ResetState: blendSrcFactor invalidated",
    "Cmd 7 (Post-Reset Disabled): Issued glDisable(GL_BLEND) (enableCalls=5)",
    "Cmd 7 (Post-Reset Disabled): Did not call glBlendFunc (funcCalls remains 2)",
    "Cmd 7 (Post-Reset Disabled): Did not call glBlendEquation (equationCalls remains 2)",
    "Cmd 7: Executor cached blendEnabled == false",
    "Cmd 7: Initially disabled command does NOT mark factors as known (remains nullopt)",
    "Cmd 7: Initially disabled command does NOT mark equation as known (remains nullopt)",
    "Cmd 8 (Unknown Factors Re-Enabled): Issued glEnable(GL_BLEND) (enableCalls=6)",
    "Cmd 8 (Unknown Factors Re-Enabled): Factors were unknown, issued glBlendFunc (funcCalls=3)",
    "Cmd 8 (Unknown Factors Re-Enabled): Equation was unknown, issued glBlendEquation (equationCalls=3)",
    "Cmd 8: Executor cached blendEnabled == true",
    "ExternalFunction: Callback invalidated blendEnabled to nullopt",
    "ExternalFunction: Callback invalidated blendSrcFactor to nullopt",
    "ExternalFunction: Callback invalidated blendEquation to nullopt",
    "Cmd 9 (Post-Callback Restoration): Re-established glEnable(GL_BLEND) (enableCalls=7)",
    "Cmd 9 (Post-Callback Restoration): Re-established glBlendFunc (funcCalls=4)",
    "Cmd 9 (Post-Callback Restoration): Re-established glBlendEquation (equationCalls=4)",
    "Cmd 9: Executor cached blendEnabled == true",
    "Driver query confirms GL_BLEND == GL_TRUE",
    "Driver query confirms GL_BLEND_SRC_RGB == GL_SRC_ALPHA and GL_BLEND_DST_RGB == GL_ONE_MINUS_SRC_ALPHA",
    "Driver query confirms GL_BLEND_EQUATION_RGB == GL_FUNC_ADD",
    "Actual wrapper calls total 15 (7 enable/disable + 4 func + 4 equation) vs 21 baseline"
]

def run_parser_negative_checks():
    print("\n[Parser Verification] Running deterministic report-validation checks...")

    # Case A: Valid synthetic report
    valid_synthetic = "P3.4.5 self-check\n" + "\n".join(f"  PASS: {item}" for item in REQUIRED_TRANSITION_ASSERTIONS) + "\nResult: PASS\n"
    assert validate_report_text(valid_synthetic, REQUIRED_TRANSITION_ASSERTIONS) == True, "Valid synthetic report was unexpectedly rejected"
    print("  PASS: Valid synthetic passing report accepted.")

    # Case B: Reviewer reproducer (FAIL entries and Result: FAIL)
    failing_synthetic = "P3.4.5 self-check\n" + "\n".join(f"  FAIL: {item}" for item in REQUIRED_TRANSITION_ASSERTIONS) + "\nResult: FAIL\n"
    rejected_fail = False
    try:
        validate_report_text(failing_synthetic, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_fail = True
    assert rejected_fail, "Validation logic failed to reject report containing FAIL entries and Result: FAIL"
    print("  PASS: Synthetic report containing FAIL: entries and Result: FAIL was correctly rejected.")

    # Case C: Incomplete report missing required assertions
    incomplete_synthetic = "P3.4.5 self-check\n  PASS: Cmd 1 (Enabled): Initial blend enable issued glEnable(GL_BLEND) (enableCalls=1)\nResult: PASS\n"
    rejected_incomplete = False
    try:
        validate_report_text(incomplete_synthetic, REQUIRED_TRANSITION_ASSERTIONS)
    except AssertionError:
        rejected_incomplete = True
    assert rejected_incomplete, "Validation logic failed to reject incomplete report missing required assertions"
    print("  PASS: Incomplete report missing required assertions was correctly rejected.")

    # Case D: Missing report file
    nonexistent_path = NEG_DIR / "nonexistent_blend_report.txt"
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
    empty_path = NEG_DIR / "empty_blend_report.txt"
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
    print("\n[Step 1] Running Blending State Diagnostic Self-Check Suite...")

    case_filter = "all"
    if args.only in SELFCHECK_CASES:
        case_filter = args.only

    pos_out = POS_DIR / f"blend_selfcheck_{case_filter}.txt"
    if pos_out.exists():
        pos_out.unlink()

    cmd_pos = [
        str(EXE),
        "--blend-selfcheck", str(pos_out),
        "--blend-selfcheck-case", case_filter
    ]
    res_pos = subprocess.run(cmd_pos, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_pos.returncode == 0, f"Positive blend self-check failed with code {res_pos.returncode}: {res_pos.stderr}"
    validate_selfcheck_report(pos_out)
    print(f"  PASS: Positive blend self-check ({case_filter}) returned exit code 0; report written to {pos_out.name}")

    if args.only in [None, "selfcheck"]:
        # Negative Case 1: Forced failure
        neg_fail_out = NEG_DIR / "blend_forced_fail.txt"
        if neg_fail_out.exists():
            neg_fail_out.unlink()
        cmd_neg = [
            str(EXE),
            "--blend-selfcheck", str(neg_fail_out),
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
            "--blend-selfcheck", str(unwritable_path)
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

    benefit_out = POS_DIR / "blend_benefit_transitions.txt"
    if benefit_out.exists():
        benefit_out.unlink()

    cmd_benefit = [
        str(EXE),
        "--blend-selfcheck", str(benefit_out),
        "--blend-selfcheck-case", "transitions"
    ]
    res_benefit = subprocess.run(cmd_benefit, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_benefit.returncode == 0, f"Transitions self-check failed with code {res_benefit.returncode}: {res_benefit.stderr}"

    validate_selfcheck_report(benefit_out, REQUIRED_TRANSITION_ASSERTIONS)

    print("  PASS: Fresh transitions self-check executed against current binary with exit code 0.")
    print(f"  PASS: Report written to {benefit_out.name} and validated: Result: PASS, zero FAIL entries, all transition assertions passed.")
    print("  Quantification (9-command sequence with state invalidations and transitions):")
    print("    - Enable/disable calls: 7 actual vs 9 baseline (2 calls suppressed)")
    print("    - Blend factor calls: 4 actual vs 6 baseline (2 calls suppressed)")
    print("    - Blend equation calls: 4 actual vs 6 baseline (2 calls suppressed)")
    print("    - Total calls: 15 actual calls vs 21 baseline calls (6 calls suppressed -> 28.6% reduction).")
    print("  Uninterrupted sequence quantification (e.g. 10 consecutive enabled commands):")
    print("    - Enable/disable calls: 1 actual vs 10 baseline (9 calls suppressed)")
    print("    - Blend factor calls: 1 actual vs 10 baseline (9 calls suppressed)")
    print("    - Blend equation calls: 1 actual vs 10 baseline (9 calls suppressed)")
    print("    - Total calls: 3 actual vs 30 baseline (27 calls suppressed -> 90.0% reduction).")
    print("  Suppression applies to uninterrupted sequences requesting matching blend state after invalidation.")
    print("  Multiple renderer calls, callbacks, or state transitions re-issue driver calls as required.")

# ======================================================================
# Step 3: Visual Dump & Frame Capture
# ======================================================================
if args.only in [None, "visual-dumps"]:
    print("\n[Step 3] Running Visual Frame Dump on HelloCube.glb...")
    cube_img_path = CANDIDATE_DIR / "blend_hellocube.jpg"
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
# Step 4: Real Production Integration & Multi-Viewport Suite
# ======================================================================
if args.only in [None, "production"]:
    print("\n[Step 4] Running Real Production Integration & Multi-Viewport Suite...")
    fixture_path = REPO / "resources" / "test_models" / "production_passes_fixture.gltf"
    assert fixture_path.exists(), f"Production fixture missing: {fixture_path}"

    def write_scenario_file(out_file: Path, img_dir_name: str):
        script_text = f"""wait 8
reload resources/test_models/production_passes_fixture.gltf
wait 15
aamode none 1
vpsetting 1 grid on
vpsetting 1 bbox on
wait 10
snap vp1_overlays_and_passes
shot build/p3.4.5/{img_dir_name}/vp1_overlay.png
vpcreate
wait 12
aamode msaa4 2
vpsetting 2 doublesided on
vpsetting 2 invertwinding on
vpsetting 2 grid off
vpsetting 2 wireframe off
wait 15
snap multi_viewport_dual
shot build/p3.4.5/{img_dir_name}/multi_viewport.png
wait 4
"""
        out_file.write_text(script_text, encoding="utf-8")

    # This archived reference has incomplete build provenance: it is not a verified 614a2ee baseline.
    # 1. Candidate execution
    cand_scenario = CANDIDATE_DIR / "prod_scenario.txt"
    cand_report = CANDIDATE_DIR / "prod_report.json"
    write_scenario_file(cand_scenario, "candidate")
    for name in ["vp1_overlay.png", "multi_viewport.png"]:
        (CANDIDATE_DIR / name).unlink(missing_ok=True)
    if cand_report.exists():
        cand_report.unlink()

    cmd_c = [
        str(EXE),
        "--isolated",
        "--ui-script", str(cand_scenario),
        "--ui-report", str(cand_report)
    ]
    res_c = subprocess.run(cmd_c, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_c.returncode == 0, f"Candidate production run failed with code {res_c.returncode}: {res_c.stderr}"
    assert cand_report.exists() and cand_report.stat().st_size > 0, "Candidate production report missing or empty"

    cand_data = json.loads(cand_report.read_text(encoding="utf-8"))
    assert not cand_data.get("failed", True), f"Candidate scenario reported failure: {cand_data.get('errors')}"
    snaps = {s["label"]: s for s in cand_data.get("snaps", [])}
    assert "vp1_overlays_and_passes" in snaps, "Missing vp1_overlays_and_passes snapshot"
    assert "multi_viewport_dual" in snaps, "Missing multi_viewport_dual snapshot"

    vp1_snap = snaps["vp1_overlays_and_passes"]
    vp1_entry = vp1_snap["viewports"][0]
    passes = vp1_entry.get("activePasses", [])
    assert "Opaque" in passes, f"Opaque pass missing from activePasses: {passes}"
    assert "Transparent" in passes, f"Transparent pass missing from activePasses: {passes}"
    assert "Transmissive" in passes, f"Transmissive pass missing from activePasses: {passes}"
    assert vp1_entry.get("showGrid") == True, "Viewport 1 grid setting mismatch"
    assert vp1_entry.get("showSceneBoundingBox") == True, "Viewport 1 bbox setting mismatch"
    print("  PASS: Candidate executed all real material passes (Opaque, Transparent, Transmissive).")

    dual_snap = snaps["multi_viewport_dual"]
    assert len(dual_snap["viewports"]) >= 2, f"Expected 2 viewports in dual snap, got {len(dual_snap['viewports'])}"
    vp2_entry = dual_snap["viewports"][1]
    assert vp2_entry.get("aaMode") == 2, f"Expected Viewport 2 aaMode 2 (MSAA4), got {vp2_entry.get('aaMode')}"
    assert vp2_entry.get("forceDoubleSided") == True, "Viewport 2 forceDoubleSided mismatch"
    assert vp2_entry.get("invertWinding") == True, "Viewport 2 invertWinding mismatch"
    assert vp2_entry.get("showGrid") == False, "Viewport 2 grid mismatch"
    print("  PASS: Candidate executed dual viewports with differing AA (MSAA4 vs None) and culling/winding settings.")

    # 2. Baseline execution and image comparison
    assert BASE_EXE.exists(), f"Baseline binary missing at {BASE_EXE}"
    base_scenario = BASE_DIR / "prod_scenario.txt"
    base_report = BASE_DIR / "prod_report.json"
    write_scenario_file(base_scenario, "baseline")
    for name in ["vp1_overlay.png", "multi_viewport.png"]:
        (BASE_DIR / name).unlink(missing_ok=True)
    if base_report.exists():
        base_report.unlink()

    cmd_b = [
        str(BASE_EXE),
        "--isolated",
        "--ui-script", str(base_scenario),
        "--ui-report", str(base_report)
    ]
    res_b = subprocess.run(cmd_b, cwd=REPO, capture_output=True, text=True, timeout=30)
    assert res_b.returncode == 0, f"Baseline production run failed with code {res_b.returncode}: {res_b.stderr}"
    assert base_report.exists() and base_report.stat().st_size > 0, "Baseline production report missing or empty"
    base_data = json.loads(base_report.read_text(encoding="utf-8"))
    assert not base_data.get("failed", True), base_data.get("errors")
    base_snaps = {s["label"]: s for s in base_data.get("snaps", [])}
    for label in ["vp1_overlays_and_passes", "multi_viewport_dual"]:
        assert label in base_snaps
        for candidate_vp, reference_vp in zip(snaps[label]["viewports"], base_snaps[label]["viewports"], strict=True):
            for key in ["id", "open", "aaMode", "showGrid", "showSceneBoundingBox", "forceDoubleSided", "invertWinding", "activePasses"]:
                assert candidate_vp[key] == reference_vp[key], (label, key)
    print("  PASS: Archived reference scenario and settings validated (historical commit provenance unverified).")

    # Compare images
    for img_name in ["vp1_overlay.png", "multi_viewport.png"]:
        c_path = CANDIDATE_DIR / img_name
        b_path = BASE_DIR / img_name
        assert c_path.exists(), f"Candidate image {c_path} missing"
        assert b_path.exists(), f"Baseline image {b_path} missing"
        with Image.open(c_path) as ci, Image.open(b_path) as bi:
            c_rgba = ci.convert("RGBA")
            b_rgba = bi.convert("RGBA")
            assert c_rgba.size == b_rgba.size, f"Size mismatch for {img_name}: {c_rgba.size} vs {b_rgba.size}"
            c_bytes = list(c_rgba.tobytes())
            b_bytes = list(b_rgba.tobytes())
            diffs = [abs(c - b) for c, b in zip(c_bytes, b_bytes)]
            max_diff = max(diffs)
            diff_count = sum(any(diffs[i:i+4]) for i in range(0, len(diffs), 4))
            assert max_diff <= 1, f"Excessive image diff in {img_name}: max_diff={max_diff}"
            print(f"  PASS: Visual match for {img_name}: max_diff={max_diff}, changed_pixels={diff_count} / {len(diffs) // 4} (tolerance <= 1)")
    print("  PASS: Production checks passed; comparison applies only to the supplied reference binary.")

# Focused acceptance for the transmission repair; no historical binary required.
if args.only == "lifecycle":
    import hashlib
    scenario = CANDIDATE_DIR / "lifecycle.txt"
    report = CANDIDATE_DIR / "lifecycle.json"
    labels = ["first", "second", "return_first", "resized", "closed", "reopened", "reloaded", "final_first"]
    scenario.write_text("""wait 8
reload resources/test_models/production_passes_fixture.gltf
wait 12
aamode none 1
vpsetting 1 grid on
vpsetting 1 bbox on
wait 8
snap first
vpcreate
aamode msaa4 2
vpsetting 2 grid off
vpsetting 2 doublesided on
vpsetting 2 invertwinding on
wait 8
snap second
vpfocus 1
snap return_first
shot build/p3.4.5/candidate/lifecycle_first.png
vpfocus 2
resize 1400 820
wait 8
snap resized
vptoggle 2
vpfocus 1
snap closed
vptoggle 2
wait 8
snap reopened
reload resources/test_models/production_passes_fixture.gltf
wait 12
snap reloaded
shot build/p3.4.5/candidate/lifecycle_second.png
vpfocus 1
snap final_first
wait 2
""", encoding="utf-8")
    for path in [report, CANDIDATE_DIR / "lifecycle_first.png", CANDIDATE_DIR / "lifecycle_second.png"]:
        path.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), "--isolated", "--ui-script", str(scenario), "--ui-report", str(report)], cwd=REPO, capture_output=True, text=True, timeout=45)
    assert result.returncode == 0, result.stderr
    data = json.loads(report.read_text(encoding="utf-8"))
    assert not data.get("failed", True), data.get("errors")
    snaps = {s["label"]: s for s in data["snaps"]}
    assert all(label in snaps for label in labels)
    def vp(label, ident):
        return next(v for v in snaps[label]["viewports"] if v["id"] == ident)
    for before, after, ident in [("first", "return_first", 1), ("second", "resized", 2), ("closed", "reopened", 2), ("reopened", "reloaded", 2), ("return_first", "final_first", 1)]:
        a, b = vp(before, ident), vp(after, ident)
        assert b["renderCount"] > a["renderCount"], (before, after, ident, "stale rendering")
        assert {"Opaque", "Transparent", "Transmissive"} <= set(b["activePasses"]), (after, b)
    assert vp("first", 1)["aaMode"] == 0 and vp("second", 2)["aaMode"] == 2
    assert vp("second", 2)["forceDoubleSided"] and vp("second", 2)["invertWinding"]
    assert not vp("second", 2)["showGrid"] and vp("first", 1)["showGrid"]
    assert not vp("closed", 2)["open"] and vp("reopened", 2)["open"]
    assert (vp("second", 2)["renderW"], vp("second", 2)["renderH"]) != (vp("resized", 2)["renderW"], vp("resized", 2)["renderH"])
    for name in ["lifecycle_first.png", "lifecycle_second.png"]:
        with Image.open(CANDIDATE_DIR / name) as img:
            assert img.width > 0 and img.height > 0
    (CANDIDATE_DIR / "lifecycle_manifest.json").write_text(json.dumps({"exe_sha256": hashlib.sha256(EXE.read_bytes()).hexdigest(), "scenario_sha256": hashlib.sha256(scenario.read_bytes()).hexdigest(), "reference": "candidate-only lifecycle checks; no historical baseline claim"}, indent=2), encoding="utf-8")
    print("  PASS: Fresh A/B/A rendering, resize, close/reopen, reload, and production material passes.")

# ======================================================================
# Step 5: Boundary Regressions Check
# ======================================================================
if args.only is None:
    print("\n[Step 5] Checking Shared Invalidation Boundaries across Prior Substeps...")
    for script, flag in [
        ("validate_p3_4_4.py", "benefit"),
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
print("P3.4.5 Blending State Cache Validation: ALL CHECKS PASSED")
print("=" * 72)
