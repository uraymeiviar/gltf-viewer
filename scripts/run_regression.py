# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Renderer regression run: the current build against a saved baseline binary.

Usage:
  python scripts/run_regression.py                  # quick tier, baseline of HEAD
  python scripts/run_regression.py --tier full      # adds each feature's A/B switch and validate_p5_1.py
  python scripts/run_regression.py --baseline build/baselines/23f61c6.exe --extra=--show-invisible-materials
  python scripts/run_regression.py --extra=--asobo --extra=off   # uncommitted work that changes ASOBO rendering

Save a baseline right after each commit with scripts/save_baseline.py. Without
--baseline, the run uses build/baselines/<HEAD short sha>.exe, so it answers "did my
uncommitted change alter anything since the last commit?".

Quick tier (equivalence only: byte-identical viewport images, identical selections):
  validate_p8_1.py     9 models x 4 AA modes, plus scripted UI cases
  validate_p8_3.py     instancing grid in 11 situations (wireframe, ortho, playback, split viewports...)
  validate_p8_5.py     --quick: picking clicks on the 1,600-drawable grid, the fixture (AA none), ortho,
                       wireframe and alpha masks
  validate_p8_followup.py --only traversal: three TAA viewports sharing one scene traversal
  validate_p4_exit_gate.py  every UI producer of a scene revision
  the four built-in self-checks
  then, one at a time: validate_asobo_model.py, validate_asobo_normal.py, validate_asobo_common.py,
  validate_asobo_support.py, validate_asobo_draworder.py, validate_asobo_packaged.py,
  validate_asobo_uv_options.py
  validate_asobo_occlusion_strength.py, validate_asobo_fresnel_ghost.py, validate_asobo_dither.py,
  validate_asobo_anisotropy.py, validate_asobo_sss.py, validate_asobo_decal_modes.py
Full tier adds:
  --with-ab (each feature against its own switch), validate_p5_1.py against the baseline, and
  validate_p8_5.py's 10,000-drawable grid and MSAA/TAA fixture copies.
Window screenshots are compared on their viewport area only, so changes to the side panels do not
fail render checks; the panels are covered by validate_p4_exit_gate.py.
Benchmarks never run here: they back timing claims (each harness's --bench).

Harnesses run in parallel (--jobs). Captures are deterministic under --fixed-frame-time, but
validate_p8_5.py's 1-px edge clicks have failed while heavy E195 renders ran beside it, so the ASOBO
harnesses run after the parallel batch. Timing runs must not be parallel and are not part of this
script.
"""

import argparse
import harness_process as subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "regression"
SELFCHECKS = ("revision-selfcheck", "blend-selfcheck", "aa-target-selfcheck", "cache-invalidation-selfcheck",
              "compute-selfcheck")
FIXTURES = (("build/p8/fixtures/many_1600.gltf", "40", "none"), ("build/p8/fixtures/many_10000.gltf", "100", "none"),
            ("build/p8/fixtures/many_leaf.gltf", "40", "leaf"), ("build/p8/fixtures/many_root.gltf", "40", "root"))

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--tier", choices=["quick", "full"], default="quick")
parser.add_argument("--baseline", help="reference binary (default: build/baselines/<HEAD short sha>.exe)")
parser.add_argument("--jobs", type=int, default=3, help="harnesses run at the same time")
parser.add_argument("--extra", action="append", default=[], help="extra Glider argument passed to both binaries")
args = parser.parse_args()

if args.baseline:
    baseline = Path(args.baseline).resolve()
else:
    sha = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=REPO, capture_output=True, text=True).stdout.strip()
    baseline = REPO / "build" / "baselines" / f"{sha}.exe"
if not baseline.exists():
    sys.exit(f"baseline {baseline} not found; save one after committing (scripts/save_baseline.py) or pass --baseline")
if not EXE.exists():
    sys.exit(f"no build at {EXE}")

# Shared fixtures are written once, before harnesses run side by side.
for path, size, anim in FIXTURES:
    (REPO / path).parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(REPO / "scripts" / "generate_many_drawables_fixture.py"), str(REPO / path), size, anim],
                   check=True, capture_output=True)

extra = [f"--extra={e}" for e in args.extra]
ab = ["--with-ab"] if args.tier == "full" else []
jobs = [
    ("p8_1", ["scripts/validate_p8_1.py", "--only", "equivalence", "--ref-exe", str(baseline), "--label", "regression_p8_1"] + extra),
    ("p8_3", ["scripts/validate_p8_3.py", "--ref-exe", str(baseline)] + ab + extra),
    ("p8_5", ["scripts/validate_p8_5.py", "--only", "equivalence", "--pre-exe", str(baseline)] + ab + extra
             + (["--quick"] if args.tier == "quick" else [])),
    ("p8_followup", ["scripts/validate_p8_followup.py", "--only", "traversal", "--ref-exe", str(baseline)]),
    ("p4_exit_gate", ["scripts/validate_p4_exit_gate.py"]),
]
# Run one at a time after the parallel batch: their E195 renders load the machine enough to upset
# validate_p8_5.py's timing-sensitive edge clicks when they overlap.
serial_jobs = [
    ("asobo_model", ["scripts/validate_asobo_model.py"]),
    ("asobo_normal", ["scripts/validate_asobo_normal.py"]),
    ("asobo_common", ["scripts/validate_asobo_common.py", "--baseline-exe", str(baseline)]),
    ("asobo_support", ["scripts/validate_asobo_support.py"]),
    ("asobo_draworder", ["scripts/validate_asobo_draworder.py"]),
    ("asobo_uv_options", ["scripts/validate_asobo_uv_options.py"]),
    ("asobo_occlusion", ["scripts/validate_asobo_occlusion_strength.py"]),
    ("asobo_fresnel_ghost", ["scripts/validate_asobo_fresnel_ghost.py", "--baseline-exe", str(baseline)]),
    ("asobo_dither", ["scripts/validate_asobo_dither.py", "--baseline-exe", str(baseline)]),
    ("asobo_anisotropy", ["scripts/validate_asobo_anisotropy.py", "--baseline-exe", str(baseline)]),
    ("asobo_sss", ["scripts/validate_asobo_sss.py", "--baseline-exe", str(baseline)]),
    ("asobo_packaged", ["scripts/validate_asobo_packaged.py"]),
    ("asobo_decal_modes", ["scripts/validate_asobo_decal_modes.py", "--baseline-exe", str(baseline)]),
    ("texture_dds", ["scripts/validate_texture_dds.py"]),
    ("texture_ktx2", ["scripts/validate_texture_ktx2.py", "--baseline-exe", str(baseline)]),
]
if args.tier == "full":
    jobs.append(("p5_1", ["scripts/validate_p5_1.py", "--pre-exe", str(baseline)]))

OUT.mkdir(parents=True, exist_ok=True)


def run_job(job):
    name, command = job
    log = OUT / f"{name}.txt"
    start = time.time()
    result = subprocess.run([sys.executable] + command, cwd=REPO, capture_output=True, text=True)
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    passed = result.returncode == 0 and "RESULT: PASS" in result.stdout
    fails = [line.strip() for line in result.stdout.splitlines() if line.strip().startswith("FAIL")]
    return name, passed, time.time() - start, fails, log


def run_selfcheck(check):
    log = OUT / f"{check}.txt"
    start = time.time()
    result = subprocess.run([str(EXE), "--isolated", f"--{check}", str(log)], cwd=REPO, capture_output=True, text=True)
    return check, result.returncode == 0, time.time() - start, [] if result.returncode == 0 else [f"exit {result.returncode}"], log


print(f"[regression: {args.tier} tier, candidate {EXE.relative_to(REPO)} vs baseline {baseline.name}, {args.jobs} jobs]")
start = time.time()
with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
    results = list(pool.map(run_job, jobs))
results += [run_job(job) for job in serial_jobs]
results += [run_selfcheck(check) for check in SELFCHECKS]

failed = False
for name, passed, seconds, fails, log in results:
    print(f"  {'PASS' if passed else 'FAIL'} {name:28s} {seconds:6.1f} s  ({log.relative_to(REPO)})")
    for line in fails[:8]:
        print(f"         {line}")
    failed |= not passed
print("=" * 60)
print(f"RESULT: {'FAIL' if failed else 'PASS'} ({time.time() - start:.0f} s)")
sys.exit(1 if failed else 0)
