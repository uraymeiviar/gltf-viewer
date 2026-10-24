import argparse
import json
import math
import os
import shutil
import harness_process as subprocess
import sys
from pathlib import Path

# Prevent broken experimental MINGW-W64 numpy in Python 3.13 site-packages
# from crashing PIL's optional numpy import hooks.
sys.modules['numpy'] = None

# Enforce explicit failure if PIL/Pillow is missing (Gate Requirement)
try:
    from PIL import Image
except ImportError as e:
    print(f"FATAL: PIL/Pillow is required for acceptance validation: {e}", file=sys.stderr)
    sys.exit(1)

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
POS_DIR = REPO / "build" / "p3.1" / "selfchecks_positive"
NEG_DIR = REPO / "build" / "p3.1" / "selfchecks_negative"
BASELINE_DIR = REPO / "build" / "p3.1" / "baseline"
CANDIDATE_DIR = REPO / "build" / "p3.1" / "candidate"

POS_DIR.mkdir(parents=True, exist_ok=True)
NEG_DIR.mkdir(parents=True, exist_ok=True)
BASELINE_DIR.mkdir(parents=True, exist_ok=True)
CANDIDATE_DIR.mkdir(parents=True, exist_ok=True)

parser = argparse.ArgumentParser(description="P3.1 Anti-Aliasing Mode Allocation Validation Suite")
parser.add_argument("--only", choices=["resize-history", "selfcheck", "image-gate", "lifecycle", "switch"],
                    help="Run only the specified focused scenario")
args = parser.parse_args()

print("=" * 72)
print("P3.1 Anti-Aliasing Mode Allocation - Automated Validation Suite")
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

def check_resize_history_oracle(snap_first, snap_pre):
    """
    Acceptance oracle asserting that the first TAA pass using resized targets
    discards previous history and uses effective blend factor 1.0.
    """
    vp_first = snap_first["viewports"][0]
    vp_pre = snap_pre["viewports"][0]

    gen_first = vp_first.get("targetGeneration", 0)
    gen_pre = vp_pre.get("targetGeneration", 0)
    last_taa_gen = vp_first.get("lastTaaTargetGeneration", 0)
    last_taa_hist_valid = vp_first.get("lastTaaHistoryValid")
    last_taa_blend = vp_first.get("lastTaaBlendFactor", 0.0)

    if gen_first <= gen_pre:
        raise AssertionError(f"Target generation did not advance: first={gen_first}, pre={gen_pre}")
    if last_taa_gen != gen_first:
        raise AssertionError(f"Last TAA pass generation ({last_taa_gen}) does not match current target generation ({gen_first})")
    if last_taa_hist_valid is not False:
        raise AssertionError(f"First post-resize TAA pass observed history as valid (lastTaaHistoryValid={last_taa_hist_valid}), expected False")
    if abs(last_taa_blend - 1.0) > 1e-4:
        raise AssertionError(f"First post-resize TAA pass effective blend factor was {last_taa_blend}, expected 1.0")

# ======================================================================
# Step 1: AA Target Diagnostic Self-Check (Positive, Forced-Fail, Unwritable)
# ======================================================================
if args.only is None or args.only == "selfcheck":
    print("\n[Step 1] Running AA Target Self-Check Suite...")

    # Positive check
    pos_out = POS_DIR / "aa_target_selfcheck.txt"
    res_pos = subprocess.run([str(EXE), "--aa-target-selfcheck", str(pos_out)], cwd=REPO, capture_output=True, text=True)
    assert res_pos.returncode == 0, f"Positive self-check failed with code {res_pos.returncode}: {res_pos.stderr}"
    assert pos_out.exists() and pos_out.stat().st_size > 0, "Positive self-check output missing or empty"
    print(f"  PASS: aa_target_selfcheck.txt -> exit code 0, {pos_out.stat().st_size} bytes")

    # Forced-failure negative check (exit code 1)
    neg_out = NEG_DIR / "aa_target_forced_failure.txt"
    res_neg = subprocess.run([str(EXE), "--aa-target-selfcheck", str(neg_out), "--selfcheck-force-failure"], cwd=REPO, capture_output=True, text=True)
    assert res_neg.returncode == 1, f"Forced failure got code {res_neg.returncode}, expected 1: {res_neg.stderr}"
    assert neg_out.exists() and neg_out.stat().st_size > 0, "Forced failure output missing or empty"
    print(f"  PASS: aa_target_forced_failure.txt -> exit code 1, {neg_out.stat().st_size} bytes")

    # Unwritable path check (exit code 2)
    res_unw = subprocess.run([str(EXE), "--aa-target-selfcheck", str(NEG_DIR)], cwd=REPO, capture_output=True, text=True)
    assert res_unw.returncode == 2, f"Unwritable path check got code {res_unw.returncode}, expected 2"
    print(f"  PASS: aa_target_selfcheck with unwritable dir -> exit code 2")

    # Verify unchanged selfcheck files exist in POS_DIR and NEG_DIR (reused evidence)
    for base in ["conflict", "validation_cache", "cache_invalidation", "logger_layout"]:
        pos_file = POS_DIR / f"{base}_selfcheck.txt"
        neg_file = NEG_DIR / f"{base}_forced_failure.txt"
        assert pos_file.exists() and pos_file.stat().st_size > 0, f"Reused positive evidence missing: {pos_file}"
        assert neg_file.exists() and neg_file.stat().st_size > 0, f"Reused negative evidence missing: {neg_file}"
    print("  PASS: Reused evidence verified for conflict, validation_cache, cache_invalidation, and logger_layout.")

# ======================================================================
# Step 2: Image Comparison Acceptance Gate & Negative Demonstration
# ======================================================================
if args.only is None or args.only == "image-gate":
    print("\n[Step 2] Image Comparison Acceptance Gate & Negative Test...")

    # 2a. Synthetic Comparator Positive & Negative Verification
    synth_a = Image.new("RGBA", (64, 64), (100, 150, 200, 255))
    synth_b_identical = Image.new("RGBA", (64, 64), (100, 150, 200, 255))
    synth_c_different = Image.new("RGBA", (64, 64), (100, 150, 200, 255))
    synth_c_different.putpixel((32, 32), (100, 155, 200, 255)) # 1 pixel differs by dg = 5

    tot_pos, diffs_pos, _, max_pos, _ = compare_image_pixels(synth_a, synth_b_identical)
    assert diffs_pos == 0 and max_pos == 0, "Synthetic positive comparison failed"
    print(f"  PASS: Synthetic comparator positive test: exact equality confirmed (0 diffs)")

    # Negative test: assert that comparator detects difference
    tot_neg, diffs_neg, pct_neg, max_neg, _ = compare_image_pixels(synth_a, synth_c_different)
    assert diffs_neg == 1 and max_neg == 5, f"Synthetic negative comparison unexpected result: diffs={diffs_neg}, max={max_neg}"

    # Save synthetic negative evidence to NEG_DIR
    synth_neg_path_a = NEG_DIR / "comparator_synth_a.png"
    synth_neg_path_b = NEG_DIR / "comparator_synth_b.png"
    synth_neg_report = NEG_DIR / "comparator_negative_result.txt"
    synth_a.save(synth_neg_path_a)
    synth_c_different.save(synth_neg_path_b)
    synth_neg_report.write_text(
        f"Synthetic comparator negative demonstration:\n"
        f"Image A: {synth_neg_path_a.name} (64x64 RGBA)\n"
        f"Image B: {synth_neg_path_b.name} (64x64 RGBA, pixel (32,32) altered by dg=5)\n"
        f"Result: {diffs_neg} differing pixel(s) detected, max_channel_diff={max_neg}, diff_pct={pct_neg:.4f}%\n"
        f"Demonstration SUCCESS: comparator rejected mismatched images.\n",
        encoding="utf-8"
    )
    print(f"  PASS: Synthetic comparator negative test demonstrated and logged to {synth_neg_report.name}")

    # 2b. Generate Fresh Candidate Startup Captures for the 6 AA Modes
    print("\n[Step 2b] Generating Fresh Candidate Startup Captures (None, MSAA2, MSAA4, MSAA8, FXAA, TAA)...")
    MODES = ["none", "msaa2", "msaa4", "msaa8", "fxaa", "taa"]
    comparison_log = []

    for mode in MODES:
        cand_img = CANDIDATE_DIR / f"frame_{mode}.png"
        if cand_img.exists():
            cand_img.unlink() # Delete old output to ensure fresh generation

        cmd = [
            str(EXE),
            "--isolated",
            "--window-size", "1280x720",
            "--aa", mode,
            "--dump-frame", str(cand_img),
            "--exit-after-dump"
        ]
        res = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, timeout=60)
        assert res.returncode == 0, f"Fresh capture for mode {mode} failed with code {res.returncode}: {res.stderr}"
        assert cand_img.exists() and cand_img.stat().st_size > 0, f"Candidate image {cand_img} missing or empty"

        # Compare fresh candidate against reused baseline
        base_img = BASELINE_DIR / f"frame_{mode}.png"
        assert base_img.exists(), f"Baseline image {base_img} missing"

        with Image.open(base_img) as ib, Image.open(cand_img) as ic:
            tot, diffs, diff_pct, max_d, avg_d = compare_image_pixels(ib, ic)

        # Documented acceptance threshold: exact pixel equality for deterministic startup captures
        assert diffs == 0, f"Mode {mode} failed exact equality acceptance gate: {diffs} differing pixels, max_diff={max_d}"
        assert max_d == 0, f"Mode {mode} max channel diff {max_d} > 0"
        log_line = f"Mode {mode:>6}: total {tot} px, diff {diffs} (0.0000%), max_diff {max_d} -> EXACT EQUALITY MATCH"
        comparison_log.append(log_line)
        print(f"  PASS: {log_line}")

    comparison_summary_file = POS_DIR / "startup_modes_image_comparison.txt"
    comparison_summary_file.write_text("\n".join(comparison_log) + "\n", encoding="utf-8")
    print(f"  PASS: All 6 fresh startup captures passed exact equality gate against baseline.")

# ======================================================================
# Step 3: Real Resize Scenario with TAA History Invalidation Oracle
# ======================================================================
if args.only is None or args.only == "resize-history":
    print("\n[Step 3] Real Resize Scenario with TAA History Invalidation Oracle...")

    # 3a. Demonstrate that the Oracle Rejects Synthetic Records with Valid History or Wrong Blend Factor
    print("  [Step 3a] Demonstrating Oracle Rejection of Defective Synthetic Records...")
    synth_base = {"viewports": [{"targetGeneration": 1}]}
    synth_valid = {"viewports": [{
        "targetGeneration": 2,
        "lastTaaTargetGeneration": 2,
        "lastTaaHistoryValid": False,
        "lastTaaBlendFactor": 1.0
    }]}
    # Must pass for valid first-frame record
    check_resize_history_oracle(synth_valid, synth_base)

    oracle_negative_log = []

    # Defect 1: History was erroneously valid
    synth_bad_hist = {"viewports": [{
        "targetGeneration": 2,
        "lastTaaTargetGeneration": 2,
        "lastTaaHistoryValid": True,
        "lastTaaBlendFactor": 1.0
    }]}
    try:
        check_resize_history_oracle(synth_bad_hist, synth_base)
        assert False, "Oracle failed to reject record with lastTaaHistoryValid == True"
    except AssertionError as e:
        assert "observed history as valid" in str(e)
        oracle_negative_log.append(f"Rejected erroneously valid history: {e}")

    # Defect 2: Wrong blend factor (e.g. 0.10 stationary blend)
    synth_bad_blend = {"viewports": [{
        "targetGeneration": 2,
        "lastTaaTargetGeneration": 2,
        "lastTaaHistoryValid": False,
        "lastTaaBlendFactor": 0.10
    }]}
    try:
        check_resize_history_oracle(synth_bad_blend, synth_base)
        assert False, "Oracle failed to reject record with wrong blend factor"
    except AssertionError as e:
        assert "effective blend factor was 0.1" in str(e)
        oracle_negative_log.append(f"Rejected wrong blend factor: {e}")

    # Defect 3: Stale target generation
    synth_stale_gen = {"viewports": [{
        "targetGeneration": 2,
        "lastTaaTargetGeneration": 1,
        "lastTaaHistoryValid": False,
        "lastTaaBlendFactor": 1.0
    }]}
    try:
        check_resize_history_oracle(synth_stale_gen, synth_base)
        assert False, "Oracle failed to reject record with stale target generation"
    except AssertionError as e:
        assert "does not match current target generation" in str(e)
        oracle_negative_log.append(f"Rejected stale target generation: {e}")

    # Defect 4: Generation did not advance
    synth_no_adv = {"viewports": [{
        "targetGeneration": 1,
        "lastTaaTargetGeneration": 1,
        "lastTaaHistoryValid": False,
        "lastTaaBlendFactor": 1.0
    }]}
    try:
        check_resize_history_oracle(synth_no_adv, synth_base)
        assert False, "Oracle failed to reject record where generation did not advance"
    except AssertionError as e:
        assert "Target generation did not advance" in str(e)
        oracle_negative_log.append(f"Rejected unadvanced target generation: {e}")

    oracle_neg_file = NEG_DIR / "resize_history_oracle_negative.txt"
    oracle_neg_file.write_text("\n".join(oracle_negative_log) + "\n", encoding="utf-8")
    print(f"  PASS: Oracle rejection demonstrated on 4 defective synthetic records and logged to {oracle_neg_file.name}")

    # 3b. Execute Real Resize Scenario with Narrow Event Trigger (catch_realloc)
    print("  [Step 3b] Executing Real Resize Scenario with catch_realloc Trigger...")
    resize_first_shot = CANDIDATE_DIR / "frame_post_resize_first_taa.png"
    resize_settled_shot = CANDIDATE_DIR / "frame_post_resize_settled_taa.png"
    if resize_first_shot.exists(): resize_first_shot.unlink()
    if resize_settled_shot.exists(): resize_settled_shot.unlink()

    # catch_realloc 1 <path> post_resize_first disarms on the EXACT frame where targetGeneration advances
    resize_script_text = f"""wait 10
aamode taa
wait 10
snap pre_resize
catch_realloc 1 {resize_first_shot.as_posix()} post_resize_first
resize 1600 900
wait 15
shot {resize_settled_shot.as_posix()}
snap post_resize_settled
wait 4
"""

    resize_script_file = CANDIDATE_DIR / "real_resize.script.txt"
    resize_report_file = CANDIDATE_DIR / "real_resize.report.json"
    resize_script_file.write_text(resize_script_text, encoding="utf-8")

    cmd_resize = [
        str(EXE),
        "--isolated",
        "--window-size", "1280x720",
        "--ui-script", str(resize_script_file),
        "--ui-report", str(resize_report_file),
    ]
    res_resize = subprocess.run(cmd_resize, cwd=REPO, capture_output=True, text=True, timeout=60)
    assert res_resize.returncode == 0, f"Resize scenario failed with code {res_resize.returncode}: {res_resize.stderr}"
    assert resize_report_file.exists(), "Resize report missing"

    rep_resize = json.loads(resize_report_file.read_text(encoding="utf-8"))
    assert not rep_resize.get("failed"), f"Resize scenario recorded failure: {rep_resize.get('errors')}"

    snaps_by_label = {s["label"]: s for s in rep_resize.get("snaps", [])}
    assert "pre_resize" in snaps_by_label, "pre_resize snap missing"
    assert "post_resize_first" in snaps_by_label, "post_resize_first snap missing"
    assert "post_resize_settled" in snaps_by_label, "post_resize_settled snap missing"

    snap_pre = snaps_by_label["pre_resize"]
    snap_first = snaps_by_label["post_resize_first"]
    snap_settled = snaps_by_label["post_resize_settled"]

    vp_pre = snap_pre["viewports"][0]
    vp_first = snap_first["viewports"][0]
    vp_settled = snap_settled["viewports"][0]

    print(f"  Pre-resize viewport:   render={vp_pre['renderW']}x{vp_pre['renderH']}, targetGen={vp_pre['targetGeneration']}, allocs={vp_pre['aaAllocs']}, deletes={vp_pre['aaDeletes']}")
    print(f"  Post-resize (first):   render={vp_first['renderW']}x{vp_first['renderH']}, targetGen={vp_first['targetGeneration']}, lastTaaGen={vp_first['lastTaaTargetGeneration']}, lastTaaHistValid={vp_first['lastTaaHistoryValid']}, lastTaaBlend={vp_first['lastTaaBlendFactor']}")
    print(f"  Post-resize (settled): render={vp_settled['renderW']}x{vp_settled['renderH']}, targetGen={vp_settled['targetGeneration']}, lastTaaHistValid={vp_settled['lastTaaHistoryValid']}, lastTaaBlend={vp_settled['lastTaaBlendFactor']}")

    # Apply the Oracle to the Actual Post-Resize First Frame Snap
    check_resize_history_oracle(snap_first, snap_pre)
    print("  PASS: Acceptance Oracle passed: First post-resize pass used target generation advance, discarded history (lastTaaHistoryValid=False), and effective blend factor 1.0.")

    # Assert genuine dimension changes
    assert vp_first["renderW"] != vp_pre["renderW"] and vp_first["renderH"] != vp_pre["renderH"], "Viewport render dimensions did not change after resize"
    assert vp_first["aaW"] == vp_first["renderW"] and vp_first["aaH"] == vp_first["renderH"], "AA target dimensions do not match new viewport render dimensions"
    assert vp_first["aaTargets"] == 2, f"TAA aaTargets expected 2, got {vp_first['aaTargets']}"

    # Assert target destruction and recreation counts
    assert vp_first["aaAllocs"] == vp_pre["aaAllocs"] + 2, f"Expected 2 allocations on resize, got allocs delta {vp_first['aaAllocs'] - vp_pre['aaAllocs']}"
    assert vp_first["aaDeletes"] == vp_pre["aaDeletes"] + 2, f"Expected 2 deletions on resize, got deletes delta {vp_first['aaDeletes'] - vp_pre['aaDeletes']}"
    assert vp_settled["aaAllocs"] == vp_first["aaAllocs"], "Steady state after resize allocated targets"
    assert vp_settled["aaDeletes"] == vp_first["aaDeletes"], "Steady state after resize deleted targets"

    # Assert settled pass state
    assert vp_settled["lastTaaHistoryValid"] is True, "Settled TAA pass observed history as invalid"
    assert abs(vp_settled["lastTaaBlendFactor"] - 0.10) < 1e-4, f"Settled TAA pass blend factor was {vp_settled['lastTaaBlendFactor']}, expected 0.10"

    # Assert non-blank visual captures
    assert resize_first_shot.exists() and resize_first_shot.stat().st_size > 0, "First post-resize shot missing"
    assert resize_settled_shot.exists() and resize_settled_shot.stat().st_size > 0, "Settled post-resize shot missing"

    with Image.open(resize_first_shot) as im_f, Image.open(resize_settled_shot) as im_s:
        assert im_f.size == (1600, 900), f"Expected 1600x900 first shot, got {im_f.size}"
        assert im_s.size == (1600, 900), f"Expected 1600x900 settled shot, got {im_s.size}"
        ext_f = im_f.convert("L").getextrema()
        ext_s = im_s.convert("L").getextrema()
        assert ext_f[0] != ext_f[1], f"First post-resize frame is blank: {ext_f}"
        assert ext_s[0] != ext_s[1], f"Settled post-resize frame is blank: {ext_s}"

        # Report pixel differences as an OBSERVATION of multi-frame accumulation (NOT the acceptance oracle)
        tot_rz, diffs_rz, pct_rz, max_rz, avg_rz = compare_image_pixels(im_f, im_s)
        print(f"  Observation: Multi-frame subpixel Halton jitter convergence produced {diffs_rz} edge pixel deltas ({pct_rz:.4f}%, max_diff={max_rz}) between frame 1 and settled frame (observation only; oracle is diagnostic invalidation + 1.0 blend factor).")

    resize_validation_report = POS_DIR / "resize_scenario_validation.txt"
    resize_validation_report.write_text(
        f"Real Resize Scenario Validation:\n"
        f"Pre-resize:  {vp_pre['renderW']}x{vp_pre['renderH']}, targetGen={vp_pre['targetGeneration']}, allocs={vp_pre['aaAllocs']}, deletes={vp_pre['aaDeletes']}\n"
        f"Post-resize (first): {vp_first['renderW']}x{vp_first['renderH']}, targetGen={vp_first['targetGeneration']}, allocs={vp_first['aaAllocs']}, deletes={vp_first['aaDeletes']}\n"
        f"Oracle Check: targetGen={vp_first['targetGeneration']} > {vp_pre['targetGeneration']}, lastTaaTargetGen={vp_first['lastTaaTargetGeneration']}, lastTaaHistoryValid={vp_first['lastTaaHistoryValid']} (discarded), lastTaaBlendFactor={vp_first['lastTaaBlendFactor']} (1.0)\n"
        f"Target reallocation delta: +2 allocs, +2 deletes (clean destruction & recreation)\n"
        f"Steady state post-resize: 0 allocs, 0 deletes; lastTaaHistoryValid={vp_settled['lastTaaHistoryValid']}, lastTaaBlendFactor={vp_settled['lastTaaBlendFactor']}\n"
        f"Visual observation: {diffs_rz} differing edge pixels ({pct_rz:.4f}%) between first post-resize frame and settled frame\n"
        f"First post-resize capture: {resize_first_shot.name} ({resize_first_shot.stat().st_size} bytes, 1600x900)\n"
        f"Settled post-resize capture: {resize_settled_shot.name} ({resize_settled_shot.stat().st_size} bytes, 1600x900)\n",
        encoding="utf-8"
    )
    print(f"  PASS: Real resize scenario completed and logged to {resize_validation_report.name}")

# ======================================================================
# Step 4: Application Viewport Lifecycle Scenario
# ======================================================================
if args.only is None or args.only == "lifecycle":
    print("\n[Step 4] Running Application Viewport Lifecycle Scenario (FXAA + TAA multi-viewport)...")

    multi_shot = CANDIDATE_DIR / "frame_multi_viewport_lifecycle.png"
    if multi_shot.exists(): multi_shot.unlink()

    lifecycle_script_text = f"""wait 10
aamode fxaa 1
wait 8
vpcreate
wait 8
aamode taa 2
wait 15
snap initial_two_viewports
vptoggle 2
wait 8
snap vp2_closed
vptoggle 2
wait 15
snap vp2_reopened
vptoggle 2
wait 4
vptoggle 2
wait 8
vptoggle 2
wait 4
vptoggle 2
wait 8
vptoggle 2
wait 4
vptoggle 2
wait 8
snap after_repeated_cycles
shot {multi_shot.as_posix()}
wait 4
"""

    lifecycle_script_file = CANDIDATE_DIR / "multi_viewport_lifecycle.script.txt"
    lifecycle_report_file = CANDIDATE_DIR / "multi_viewport_lifecycle.report.json"
    lifecycle_script_file.write_text(lifecycle_script_text, encoding="utf-8")

    cmd_lifecycle = [
        str(EXE),
        "--isolated",
        "--window-size", "1280x720",
        "--ui-script", str(lifecycle_script_file),
        "--ui-report", str(lifecycle_report_file),
    ]
    res_lifecycle = subprocess.run(cmd_lifecycle, cwd=REPO, capture_output=True, text=True, timeout=60)
    assert res_lifecycle.returncode == 0, f"Lifecycle scenario failed with code {res_lifecycle.returncode}: {res_lifecycle.stderr}"
    assert lifecycle_report_file.exists(), "Lifecycle report missing"

    rep_lifecycle = json.loads(lifecycle_report_file.read_text(encoding="utf-8"))
    assert not rep_lifecycle.get("failed"), f"Lifecycle scenario recorded failure: {rep_lifecycle.get('errors')}"

    lifecycle_snaps = {s["label"]: s for s in rep_lifecycle.get("snaps", [])}
    assert "initial_two_viewports" in lifecycle_snaps, "initial_two_viewports snap missing"
    assert "vp2_closed" in lifecycle_snaps, "vp2_closed snap missing"
    assert "vp2_reopened" in lifecycle_snaps, "vp2_reopened snap missing"
    assert "after_repeated_cycles" in lifecycle_snaps, "after_repeated_cycles snap missing"

    # Inspect initial two viewports (VP1 = FXAA, VP2 = TAA)
    vps_init = {v["id"]: v for v in lifecycle_snaps["initial_two_viewports"]["viewports"]}
    assert 1 in vps_init and 2 in vps_init, "Expected both viewport 1 and 2 to be present"
    assert vps_init[1]["open"] is True and vps_init[1]["aaTargets"] == 1 and vps_init[1]["aaFbos"] == 1, f"VP1 unexpected initial state: {vps_init[1]}"
    assert vps_init[2]["open"] is True and vps_init[2]["aaTargets"] == 2 and vps_init[2]["aaFbos"] == 2, f"VP2 unexpected initial state: {vps_init[2]}"
    print(f"  Initial two viewports: VP1 (FXAA) aaTargets={vps_init[1]['aaTargets']}, VP2 (TAA) aaTargets={vps_init[2]['aaTargets']}")

    # Inspect VP2 closed: VP2 targets released, VP1 targets intact
    vps_closed = {v["id"]: v for v in lifecycle_snaps["vp2_closed"]["viewports"]}
    assert vps_closed[1]["open"] is True and vps_closed[1]["aaTargets"] == 1, f"VP1 compromised on VP2 close: {vps_closed[1]}"
    assert vps_closed[2]["open"] is False and vps_closed[2]["aaTargets"] == 0 and vps_closed[2]["aaBytes"] == 0, f"VP2 targets not released: {vps_closed[2]}"
    print(f"  After VP2 closed:     VP1 aaTargets={vps_closed[1]['aaTargets']} (INTACT), VP2 aaTargets={vps_closed[2]['aaTargets']} (RELEASED)")

    # Inspect VP2 reopened: VP2 targets recreated
    vps_reopened = {v["id"]: v for v in lifecycle_snaps["vp2_reopened"]["viewports"]}
    assert vps_reopened[1]["open"] is True and vps_reopened[1]["aaTargets"] == 1
    assert vps_reopened[2]["open"] is True and vps_reopened[2]["aaTargets"] == 2 and vps_reopened[2]["aaFbos"] == 2
    print(f"  After VP2 reopened:   VP1 aaTargets={vps_reopened[1]['aaTargets']}, VP2 aaTargets={vps_reopened[2]['aaTargets']} (RECREATED)")

    # Inspect after repeated cycles: no accumulation
    vps_cycles = {v["id"]: v for v in lifecycle_snaps["after_repeated_cycles"]["viewports"]}
    assert vps_cycles[1]["aaTargets"] == 1 and vps_cycles[1]["aaFbos"] == 1, f"VP1 target leak detected: {vps_cycles[1]}"
    assert vps_cycles[2]["aaTargets"] == 2 and vps_cycles[2]["aaFbos"] == 2, f"VP2 target leak detected: {vps_cycles[2]}"
    print(f"  After 3 open/close cycles: VP1 aaTargets={vps_cycles[1]['aaTargets']}, VP2 aaTargets={vps_cycles[2]['aaTargets']} (NO ACCUMULATION)")

    # Inspect visual output
    assert multi_shot.exists() and multi_shot.stat().st_size > 0, "Multi-viewport shot missing"
    with Image.open(multi_shot) as im_m:
        ext_m = im_m.convert("L").getextrema()
        assert ext_m[0] != ext_m[1], f"Multi-viewport frame is blank: {ext_m}"
        print(f"  Representative frame {multi_shot.name}: size {im_m.size}, luminance range {ext_m} (VALID NON-BLANK)")

    lifecycle_report = POS_DIR / "viewport_lifecycle_validation.txt"
    lifecycle_report.write_text(
        f"Multi-Viewport Lifecycle Validation:\n"
        f"VP1 (FXAA): initial targets={vps_init[1]['aaTargets']}, after VP2 closed={vps_closed[1]['aaTargets']}, final={vps_cycles[1]['aaTargets']}\n"
        f"VP2 (TAA):  initial targets={vps_init[2]['aaTargets']}, when closed={vps_closed[2]['aaTargets']} (0 bytes), reopened={vps_reopened[2]['aaTargets']}, final={vps_cycles[2]['aaTargets']}\n"
        f"Accumulation test: 3 cycles completed with 0 target leaks.\n"
        f"Representative frame: {multi_shot.name} ({multi_shot.stat().st_size} bytes)\n",
        encoding="utf-8"
    )
    print(f"  PASS: Application viewport lifecycle validation completed and logged to {lifecycle_report.name}")

# ======================================================================
# Step 5: Dynamic Mode Switch Scenarios (Preserved)
# ======================================================================
if args.only is None or args.only == "switch":
    print("\n[Step 5] Running Dynamic Mode Switch Scenarios (TAA <-> FXAA)...")
    SWITCH_SCENARIOS = [
        (
            "post_switch_taa",
            """wait 10
aamode fxaa
wait 10
aamode taa
wait 15
shot build/p3.1/candidate/frame_post_switch_taa.png
wait 4
""",
            CANDIDATE_DIR / "frame_post_switch_taa.png",
        ),
        (
            "post_switch_fxaa",
            """wait 10
aamode taa
wait 10
aamode fxaa
wait 15
shot build/p3.1/candidate/frame_post_switch_fxaa.png
wait 4
""",
            CANDIDATE_DIR / "frame_post_switch_fxaa.png",
        ),
    ]

    for name, script_text, out_shot in SWITCH_SCENARIOS:
        if out_shot.exists(): out_shot.unlink()
        script_file = CANDIDATE_DIR / f"{name}.script.txt"
        script_file.write_text(script_text, encoding="utf-8")
        report_file = CANDIDATE_DIR / f"{name}.report.json"
        cmd = [
            str(EXE),
            "--isolated",
            "--window-size", "1280x720",
            "--ui-script", str(script_file),
            "--ui-report", str(report_file),
        ]
        res = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, timeout=60)
        assert res.returncode == 0, f"Scenario {name} failed with code {res.returncode}: {res.stderr}"
        assert report_file.exists(), f"Scenario {name} report missing"
        rep = json.loads(report_file.read_text(encoding="utf-8"))
        assert not rep.get("failed"), f"Scenario {name} recorded failure: {rep.get('errors')}"
        assert out_shot.exists() and out_shot.stat().st_size > 0, f"Scenario {name} image missing or empty"
        with Image.open(out_shot) as im:
            ext = im.convert("L").getextrema()
            assert ext[0] != ext[1], f"Dynamic switch {name} produced blank image"
        print(f"  PASS: {name} captured {out_shot.name} ({out_shot.stat().st_size} bytes, non-blank)")

print("\n" + "=" * 72)
print("VALIDATION EXECUTION COMPLETED SUCCESSFULLY!")
print("=" * 72)
