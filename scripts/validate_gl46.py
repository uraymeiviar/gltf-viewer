# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""OpenGL 4.6 context step: the move from a 4.1 to a 4.6 core context must not change a pixel.

Checks (renders run one after another, never in parallel):
  * the profile report's `gl` object shows the obtained context (4.6 where the driver offers it) and the
    compute limits, and the run raises zero GL errors;
  * byte-identical viewport captures versus the baseline exe (--baseline-exe, default build/baselines/<HEAD>.exe):
    HelloCube, hierarchy_animation_fixture.gltf (AA none and TAA), the E195 cockpit reference view and the DA62
    cockpit (skipped with a note when the SDK is not installed);
  * `--compute-selfcheck` passes (or reports "skipped" below OpenGL 4.3).
"""

import argparse
import json
import harness_process as subprocess
import sys
from pathlib import Path

from validate_asobo_common import (COMMON, EXE, OUT as COMMON_OUT, PLAIN_FIXTURE, REPO, check, finish, render,
                                   render_view, same, view)

OUT = REPO / "build" / "gl46"
SDK = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
DA62 = SDK / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo/Part_Interior_Cockpit/model/Cockpit_LOD00.gltf"
DA62_FLAGS = ["--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05", "--camera-pitch", "-20",
              "--camera-fov", "55", "--camera-yaw", "220", "--hide-grid",
              "--search-path", str(SDK / "Sources/Art/Max/textures")]


def default_baseline():
    sha = subprocess.run(["git", "log", "-1", "--format=%h"], cwd=REPO, capture_output=True, text=True).stdout.strip()
    return REPO / "build" / "baselines" / f"{sha}.exe"


def compare(baseline, model, name, flags=(), search_paths=()):
    a = render(EXE, model, f"{name}_current", flags, OUT, search_paths)
    b = render(baseline, model, f"{name}_baseline", flags, OUT, search_paths)
    check(a is not None and b is not None and same(a, b), f"{name}: byte-identical to the baseline")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", default=None)
    args = parser.parse_args()
    baseline = Path(args.baseline_exe) if args.baseline_exe else default_baseline()
    OUT.mkdir(parents=True, exist_ok=True)
    print(f"[candidate {EXE.name} vs baseline {baseline.name}]")
    if not baseline.exists():
        check(False, f"baseline {baseline} exists")
        finish()

    print("[context and GL errors]")
    hello = REPO / "resources" / "sampleGltf" / "HelloCube.glb"
    report = OUT / "profile.json"
    report.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), str(hello)] + COMMON + ["--profile-report", str(report), "--exit-after-profile"],
                            cwd=REPO, capture_output=True, text=True, timeout=300)
    gl = json.loads(report.read_text(encoding="utf-8")).get("gl", {}) if report.exists() else {}
    print(f"  INFO gl object: {json.dumps(gl)}")
    check((gl.get("major", 0), gl.get("minor", 0)) >= (4, 1), "profile report carries the context version")
    if (gl.get("major"), gl.get("minor")) >= (4, 3):
        check(gl.get("compute") is True and gl.get("maxComputeWorkGroupInvocations", 0) >= 1024,
              "compute available and GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS reported (>= 1024)")
    check((gl.get("major"), gl.get("minor")) == (4, 6), "context is OpenGL 4.6 (driver permitting)")
    errors = [line for line in result.stdout.splitlines() if line.startswith("GL automation errors")]
    check(errors == ["GL automation errors: 0"] and "GL error 0x" not in result.stderr, f"zero GL errors {errors}")

    print("[byte-identical captures]")
    compare(baseline, hello, "hellocube")
    compare(baseline, PLAIN_FIXTURE, "hierarchy_aa_none")
    compare(baseline, PLAIN_FIXTURE, "hierarchy_taa", ["--msaa", "0", "--post-aa", "taa"])
    entry = view("cockpit_main_displays")
    a = render_view(EXE, entry, "e195_current", folder=OUT)
    b = render_view(baseline, entry, "e195_baseline", folder=OUT)
    check(a is not None and same(a, b), "E195 cockpit reference view: byte-identical to the baseline")
    if DA62.exists():
        compare(baseline, DA62, "da62_cockpit", DA62_FLAGS)
    else:
        print(f"  SKIP DA62 cockpit: {DA62} not found")

    print("[compute self-check]")
    out = OUT / "compute-selfcheck.txt"
    out.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), "--isolated", "--compute-selfcheck", str(out)], cwd=REPO,
                            capture_output=True, text=True, timeout=300)
    text = out.read_text(encoding="utf-8") if out.exists() else ""
    print("  " + (text.strip().splitlines()[-1] if text.strip() else "(no report)"))
    check(result.returncode == 0 and ("Result: PASS" in text or "Result: skipped" in text), "--compute-selfcheck passes")
    finish()


if __name__ == "__main__":
    main()
