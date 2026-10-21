import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
POS_DIR = REPO / "build" / "p2.4" / "selfchecks_positive"
NEG_DIR = REPO / "build" / "p2.4" / "selfchecks_negative"

POS_DIR.mkdir(parents=True, exist_ok=True)
NEG_DIR.mkdir(parents=True, exist_ok=True)

CHECKS = [
    ("--conflict-selfcheck", "conflict_selfcheck.txt", False, 0),
    ("--conflict-selfcheck", "conflict_forced_failure.txt", True, 1),
    ("--validation-cache-selfcheck", "validation_cache_selfcheck.txt", False, 0),
    ("--validation-cache-selfcheck", "validation_cache_forced_failure.txt", True, 1),
    ("--cache-invalidation-selfcheck", "cache_invalidation_selfcheck.txt", False, 0),
    ("--cache-invalidation-selfcheck", "cache_invalidation_forced_failure.txt", True, 1),
    ("--logger-layout-selfcheck", "logger_layout_selfcheck.txt", False, 0),
    ("--logger-layout-selfcheck", "logger_layout_forced_failure.txt", True, 1),
]

for flag, out_name, force_fail, expected_code in CHECKS:
    target_dir = NEG_DIR if force_fail else POS_DIR
    out_file = target_dir / out_name
    cmd = [str(EXE), flag, str(out_file)]
    if force_fail:
        cmd.append("--selfcheck-force-failure")
    res = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    assert res.returncode == expected_code, f"{out_name} got {res.returncode}, expected {expected_code}"
    assert out_file.exists() and out_file.stat().st_size > 0, f"{out_name} missing or empty"
    print(f"{out_name} ({target_dir.name}): returncode {res.returncode} (expected {expected_code}), {out_file.stat().st_size} bytes")

# Also test code 2 when output directory is passed as file
dir_path = NEG_DIR
for flag in ["--conflict-selfcheck", "--validation-cache-selfcheck", "--cache-invalidation-selfcheck", "--logger-layout-selfcheck"]:
    res = subprocess.run([str(EXE), flag, str(dir_path)], cwd=REPO, capture_output=True, text=True)
    assert res.returncode == 2, f"{flag} directory target got {res.returncode}, expected 2"
    print(f"{flag} directory target ({dir_path.name}): returncode 2 (expected 2)")

print("All self-checks, forced failures, and unwritable path tests PASS!")
