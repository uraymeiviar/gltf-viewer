# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Validation script for portable settings storage and I/O robustness.

Checks:
  1. default_run: Portable storage <exeDir>/GliderData is created and populated on
     interactive session exit; %APPDATA%/Glider is not modified.
  2. readonly_fallback: When the candidate data directory is read-only (denied write via ACL),
     Glider falls back to %APPDATA%/Glider or in-memory mode, logs the fallback, and exits 0.
  3. corrupt_unreadable: When settings/state files are corrupt or unreadable, Glider starts
     with defaults, logs one warning per file, and exits 0.
  4. isolated_mode: Running with --isolated does not read or write settings to disk.
  5. baseline_compare: Renders with --isolated and verifies byte-identical capture to baseline.
"""

import argparse
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys
import time

REPO = pathlib.Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
BASE_EXE = REPO / "build" / "baselines" / "652def5.exe"
REVIEW_DIR = REPO / "build" / "review"
DEFAULT_DATA_DIR = REPO / "build" / "bin" / "Release" / "GliderData"
APPDATA_DIR = pathlib.Path(os.environ.get("APPDATA", "")) / "Glider" if os.environ.get("APPDATA") else None

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["default_run", "readonly_fallback", "corrupt_unreadable", "isolated_mode", "baseline_compare"])
args = parser.parse_args()

failures = []


def section(name):
    return args.only in (None, name)


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def run_glider(extra_args, env=None, timeout=60):
    cmd = [str(EXE)] + extra_args
    return subprocess.run(cmd, cwd=REPO, env=env, capture_output=True, text=True, timeout=timeout)


if section("default_run"):
    print("[default_run]")
    REVIEW_DIR.mkdir(parents=True, exist_ok=True)
    test_script = REVIEW_DIR / "val_settings_interactive.txt"
    test_report = REVIEW_DIR / "val_settings_interactive.json"
    test_script.write_text("wait 8\nreload resources/sampleGltf/HelloCube.glb\nwait 8\n", encoding="utf-8")
    test_report.unlink(missing_ok=True)

    # Clean existing GliderData to verify cold-start creation & migration
    if DEFAULT_DATA_DIR.exists():
        shutil.rmtree(DEFAULT_DATA_DIR)

    appdata_mtimes = {}
    if APPDATA_DIR and APPDATA_DIR.exists():
        appdata_mtimes = {f.name: f.stat().st_mtime for f in APPDATA_DIR.glob("*") if f.is_file()}

    # Ensure clean env (no GLIDER_DATA_DIR override)
    clean_env = dict(os.environ)
    clean_env.pop("GLIDER_DATA_DIR", None)

    res = run_glider(["--ui-script", str(test_script), "--ui-report", str(test_report)], env=clean_env)
    check(res.returncode == 0, "Default interactive session exits 0")
    check(DEFAULT_DATA_DIR.exists(), f"GliderData directory created at {DEFAULT_DATA_DIR}")

    expected_files = ["settings.ini", "recent_files.txt", "window_state.txt", "render_quality.txt", "imgui.ini"]
    for fname in expected_files:
        fpath = DEFAULT_DATA_DIR / fname
        check(fpath.exists() and fpath.stat().st_size > 0, f"GliderData contains {fname} ({fpath.stat().st_size if fpath.exists() else 0} bytes)")

    if APPDATA_DIR and APPDATA_DIR.exists():
        new_appdata_mtimes = {f.name: f.stat().st_mtime for f in APPDATA_DIR.glob("*") if f.is_file()}
        changed = [f for f in new_appdata_mtimes if f not in appdata_mtimes or new_appdata_mtimes[f] != appdata_mtimes[f]]
        check(len(changed) == 0, f"%APPDATA%/Glider not modified during portable run (changed: {changed})")

    check("Using portable settings directory" in res.stderr, "Logged portable settings directory usage")


if section("readonly_fallback"):
    print("[readonly_fallback]")
    ro_dir = REVIEW_DIR / "test_val_ro_data"
    ro_dir.mkdir(parents=True, exist_ok=True)
    user = os.environ.get("USERNAME", "Everyone")

    test_script = REVIEW_DIR / "val_settings_ro_script.txt"
    test_report = REVIEW_DIR / "val_settings_ro_report.json"
    test_script.write_text("wait 8\n", encoding="utf-8")
    test_report.unlink(missing_ok=True)

    # Sub-case A: Read-only candidate directory falls back to APPDATA
    print("  Testing ACL deny (W) on candidate directory...")
    subprocess.run(["icacls", str(ro_dir), "/deny", f"{user}:(W)"], check=True, capture_output=True)
    try:
        env_ro = dict(os.environ)
        env_ro["GLIDER_DATA_DIR"] = str(ro_dir)
        res = run_glider(["--ui-script", str(test_script), "--ui-report", str(test_report)], env=env_ro)
        check(res.returncode == 0, "Glider exits 0 when candidate dir is unwritable")
        check("is not writable; falling back to AppData" in res.stderr, "Fallback to AppData logged")
    finally:
        subprocess.run(["icacls", str(ro_dir), "/remove:d", user], check=True, capture_output=True)

    # Sub-case B: Both candidate and AppData unwritable -> in-memory fallback
    print("  Testing both candidate and AppData unwritable (in-memory mode)...")
    subprocess.run(["icacls", str(ro_dir), "/deny", f"{user}:(W)"], check=True, capture_output=True)
    try:
        env_inmem = dict(os.environ)
        env_inmem["GLIDER_DATA_DIR"] = str(ro_dir)
        env_inmem["APPDATA"] = str(ro_dir)
        res = run_glider(["--ui-script", str(test_script), "--ui-report", str(test_report)], env=env_inmem)
        check(res.returncode == 0, "Glider exits 0 when both portable and AppData are unwritable")
        check("running with in-memory settings" in res.stderr, "In-memory settings fallback logged")
        check("directory is not writable; changes kept in memory" in res.stderr, "Unwritable save warning logged")
    finally:
        subprocess.run(["icacls", str(ro_dir), "/remove:d", user], check=True, capture_output=True)


if section("corrupt_unreadable"):
    print("[corrupt_unreadable]")
    corrupt_dir = REVIEW_DIR / "test_val_corrupt_data"
    corrupt_dir.mkdir(parents=True, exist_ok=True)

    (corrupt_dir / "settings.ini").write_bytes(b"\xde\xad\xbe\xef\x00\x01\x02\x03corrupt binary data")
    (corrupt_dir / "window_state.txt").write_bytes(b"\x00\xff\xfe\x01corrupt binary state")
    (corrupt_dir / "recent_files.txt").write_bytes(b"\x00\x01\x02\x03\x04corrupt recent list")
    (corrupt_dir / "render_quality.txt").write_bytes(b"\x00\x00corrupt quality data")
    (corrupt_dir / "imgui.ini").write_bytes(b"\x80\x81\x82\x83corrupt imgui layout")

    test_script = REVIEW_DIR / "val_settings_corrupt_script.txt"
    test_report = REVIEW_DIR / "val_settings_corrupt_report.json"
    test_script.write_text("wait 8\n", encoding="utf-8")
    test_report.unlink(missing_ok=True)

    env_corrupt = dict(os.environ)
    env_corrupt["GLIDER_DATA_DIR"] = str(corrupt_dir)

    res = run_glider(["--ui-script", str(test_script), "--ui-report", str(test_report)], env=env_corrupt)
    check(res.returncode == 0, "Glider exits 0 with corrupt files")
    check("Corrupt or invalid settings.ini detected; using defaults" in res.stderr, "Logged corrupt settings.ini")
    check("Corrupt or invalid window_state.txt detected; using defaults" in res.stderr, "Logged corrupt window_state.txt")
    check("Corrupt or invalid recent_files.txt detected; using defaults" in res.stderr, "Logged corrupt recent_files.txt")
    check("Corrupt or invalid render_quality.txt detected; using defaults" in res.stderr, "Logged corrupt render_quality.txt")
    check("Corrupt or invalid imgui.ini detected; using defaults" in res.stderr, "Logged corrupt imgui.ini")

    # Sub-case: Unreadable file (permission denied on single file)
    unread_file = corrupt_dir / "settings.ini"
    unread_file.write_text("[SearchPaths]\ncount=0\n", encoding="utf-8")
    user = os.environ.get("USERNAME", "Everyone")
    subprocess.run(["icacls", str(unread_file), "/deny", f"{user}:(R)"], check=True, capture_output=True)
    try:
        res = run_glider(["--ui-script", str(test_script), "--ui-report", str(test_report)], env=env_corrupt)
        check(res.returncode == 0, "Glider exits 0 with unreadable settings.ini")
        check("Failed to read settings.ini: unable to open file; using defaults" in res.stderr, "Logged unreadable settings.ini")
    finally:
        subprocess.run(["icacls", str(unread_file), "/remove:d", user], check=True, capture_output=True)


if section("isolated_mode"):
    print("[isolated_mode]")
    # Verify --isolated does not create GliderData if missing
    if DEFAULT_DATA_DIR.exists():
        shutil.rmtree(DEFAULT_DATA_DIR)

    iso_dump = REVIEW_DIR / "val_isolated_dump.png"
    iso_dump.unlink(missing_ok=True)

    clean_env = dict(os.environ)
    clean_env.pop("GLIDER_DATA_DIR", None)

    res = run_glider(["resources/sampleGltf/HelloCube.glb", "--isolated", "--dump-frame", str(iso_dump), "--exit-after-dump"], env=clean_env)
    check(res.returncode == 0, "Glider exits 0 under --isolated")
    check(not DEFAULT_DATA_DIR.exists(), "GliderData was NOT created under --isolated")
    check(iso_dump.exists() and iso_dump.stat().st_size > 0, "Dumped frame under --isolated successfully")


if section("baseline_compare"):
    print("[baseline_compare]")
    base_dump = REVIEW_DIR / "val_baseline_dump.png"
    cand_dump = REVIEW_DIR / "val_candidate_dump.png"
    base_dump.unlink(missing_ok=True)
    cand_dump.unlink(missing_ok=True)

    clean_env = dict(os.environ)
    clean_env.pop("GLIDER_DATA_DIR", None)

    res_base = subprocess.run([str(BASE_EXE), "resources/sampleGltf/HelloCube.glb", "--isolated", "--dump-frame", str(base_dump), "--exit-after-dump"], cwd=REPO, env=clean_env, capture_output=True, text=True)
    check(res_base.returncode == 0, f"Baseline exits 0 ({res_base.returncode})")

    res_cand = run_glider(["resources/sampleGltf/HelloCube.glb", "--isolated", "--dump-frame", str(cand_dump), "--exit-after-dump"], env=clean_env)
    check(res_cand.returncode == 0, f"Candidate exits 0 ({res_cand.returncode})")

    check(base_dump.exists(), "Baseline capture exists")
    check(cand_dump.exists(), "Candidate capture exists")
    if base_dump.exists() and cand_dump.exists():
        base_bytes = base_dump.read_bytes()
        cand_bytes = cand_dump.read_bytes()
        check(base_bytes == cand_bytes, f"Candidate capture byte-identical to baseline ({len(cand_bytes)} bytes, sha256={hashlib.sha256(cand_bytes).hexdigest()[:16]})")


print(f"\nSummary: {len(failures)} failures")
if failures:
    sys.exit(1)
sys.exit(0)
