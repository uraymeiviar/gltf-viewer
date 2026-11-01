# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""EXT_meshopt_compression / KHR_meshopt_compression decoding and the validator's unsupported-extension notices.

Fixtures are written at test time into build/meshopt/fixtures by the dev-only tool build/tools/Release/
meshopt_fixture_gen.exe (scripts/meshopt_fixture_gen.cpp, built with Glider from the same fetched meshoptimizer
source whose decoder Glider links; never shipped). Each compressed file has a *_plain twin holding exactly the bytes
the decoder produces in the same bufferView layout, without the extension. The tool checks its own encode/decode
round trip and exits 1 when a stream is not lossless (unfiltered streams: identical bytes; TRIANGLES: the same
triangles with the same winding, rotations allowed) or a filtered stream decodes outside its quantization step
(OCTAHEDRAL normals, QUATERNION rotations, EXPONENTIAL floats, COLOR). Comparing renders is therefore meaningful
byte for byte: the plain twin holds the same quantized / filtered values, so a correct decoder must reproduce it
exactly, and any wrong byte (offset, stride, filter, fallback handling) changes the image or the animation.

Checks:
  * the fixture tool runs and its own codec checks pass; fixture sizes (informational);
  * renders (isolated, 1280x720, AA none, frame 24 of a fixed 0.05 s clock: the sphere's QUATERNION / EXPONENTIAL
    animation is mid-way) are byte-identical: ext.glb (EXT, vertex codec v0, fallback buffer without uri) and
    ext_gltf.gltf (.gltf, compressed .bin, fallback buffer without uri) and ext_optional.gltf (used, not required,
    fallback .bin present) to ext_plain.glb; khr.glb (KHR, vertex codec v1, COLOR filter) to khr_plain.glb;
    each loads, zero GL errors;
  * GLIDER_TEST_DISABLE_MESHOPT=1 (a loader without the extension): ext_optional.gltf reads its uncompressed
    fallback and still renders identically to ext_plain.glb; ext.glb, which requires the extension, is not loaded
    and the log names the reason;
  * a required-meshopt file whose stream is damaged (ext_corrupt.glb) is not loaded: the log names the extension,
    the bufferView and the failure, and "Failed to load";
  * validator: no error for the meshopt files and no unsupported-extension notice; a file that requires an
    unsupported extension gets the warning EXTENSION_REQUIRED_UNSUPPORTED ("Required extension X is not supported;
    the file may not display correctly."), an unsupported used-only extension the info EXTENSION_USED_UNSUPPORTED,
    supported ones (KHR_texture_transform, EXT_meshopt_compression) nothing;
  * with --baseline-exe: the plain twins render byte-identical to the baseline binary, and the baseline's render of
    ext.glb differs from the plain twin (the check would catch a missing decoder).

Run: python scripts/validate_meshopt.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import json
import os
import struct
import sys
from pathlib import Path

import harness_process as subprocess
from glider_harness import EXE, REPO, baseline_exe, capture, gl_errors_zero, read_json, run, same

OUT = REPO / "build" / "meshopt"
FIXTURES = OUT / "fixtures"
TOOL = REPO / "build" / "tools" / "Release" / "meshopt_fixture_gen.exe"
FLAGS = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
DISABLE = "GLIDER_TEST_DISABLE_MESHOPT"
FIXTURE_LIMIT = 400 * 1024  # bytes of all generated fixture files (generated at test time, never committed)

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


def render(exe, model, name, env=None):
    """(png or None, log) of one isolated capture at frame 24."""
    png = OUT / f"{name}.png"
    report = OUT / f"{name}.json"
    if env is None:
        image, _, log = capture(exe, model, png, FLAGS)
        return image, log
    png.unlink(missing_ok=True)
    result = run([exe, model, *FLAGS, "--capture-viewport", png, "--capture-frame", "24", "--profile-report", report,
                  "--exit-after-profile"], env=env)
    return (png if png.exists() else None), result.stdout + result.stderr


def validate(model, name):
    report = OUT / f"{name}.validation.json"
    report.unlink(missing_ok=True)
    run([EXE, "--isolated", "--validate-file", model, "--validation-report", report])
    data = read_json(report)
    return None if data is None else data.get("issues", [])


def unsupported_fixture():
    """A one-triangle .gltf (data URI) requiring an unsupported and a supported extension, and using another
    unsupported one without requiring it."""
    positions = struct.pack("<9f", 0, 0, 0, 1, 0, 0, 0, 1, 0)
    uri = "data:application/octet-stream;base64," + base64.b64encode(positions).decode("ascii")
    document = {
        "asset": {"version": "2.0"},
        "extensionsUsed": ["EXT_example_unsupported_required", "KHR_texture_transform",
                           "EXT_example_unsupported_used", "EXT_meshopt_compression"],
        "extensionsRequired": ["EXT_example_unsupported_required", "KHR_texture_transform"],
        "buffers": [{"byteLength": len(positions), "uri": uri}],
        "bufferViews": [{"buffer": 0, "byteLength": len(positions), "target": 34962}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
                       "min": [0, 0, 0], "max": [1, 1, 0]}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
        "nodes": [{"mesh": 0}],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }
    path = FIXTURES / "unsupported_extensions.gltf"
    path.write_text(json.dumps(document, indent=1), encoding="utf-8")
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", help="reference binary (default: the newest build/baselines/*.exe)")
    args = parser.parse_args()
    baseline = baseline_exe(args.baseline_exe)
    OUT.mkdir(parents=True, exist_ok=True)

    print("[fixtures]")
    if not check(TOOL.exists(), f"fixture tool {TOOL.relative_to(REPO)} exists (built with the Glider target)"):
        return
    result = subprocess.run([str(TOOL), str(FIXTURES)], capture_output=True, text=True)
    check(result.returncode == 0, f"meshopt_fixture_gen: codec round trip and filter checks pass {result.stderr.strip()}")
    try:
        summary = json.loads(result.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        summary = {}
    total = sum(path.stat().st_size for path in FIXTURES.iterdir() if path.is_file())
    print(f"  INFO meshoptimizer {summary.get('meshoptimizer')}; EXT {summary.get('ext')}; KHR {summary.get('khr')}")
    check(total <= FIXTURE_LIMIT, f"generated fixtures total {total} bytes (<= {FIXTURE_LIMIT})")
    unsupported = unsupported_fixture()

    print("[renders: compressed = plain twin, byte for byte]")
    images = {}
    for name, model in (("ext_plain", "ext_plain.glb"), ("ext", "ext.glb"), ("ext_gltf", "ext_gltf.gltf"),
                        ("ext_optional", "ext_optional.gltf"), ("khr_plain", "khr_plain.glb"), ("khr", "khr.glb")):
        image, log = render(EXE, FIXTURES / model, name)
        images[name] = image
        check(image is not None and "Loaded successfully" in log and gl_errors_zero(log),
              f"{model}: loaded, captured, zero GL errors")
    for name, twin in (("ext", "ext_plain"), ("ext_gltf", "ext_plain"), ("ext_optional", "ext_plain"),
                       ("khr", "khr_plain")):
        check(same(images[name], images[twin]), f"{name}: byte-identical to {twin}")
    check(images["ext_plain"] is not None and not same(images["ext_plain"], images["khr_plain"]),
          "EXT and KHR fixtures differ (COLOR filter vs 8-bit colours), so the comparisons are not trivially equal")

    print(f"[{DISABLE}=1: a loader without the extension]")
    env = dict(os.environ, **{DISABLE: "1"})
    image, log = render(EXE, FIXTURES / "ext_optional.gltf", "disabled_optional", env)
    check(same(image, images["ext_plain"]) and "Loaded successfully" in log,
          "ext_optional.gltf (not required) reads its uncompressed fallback: byte-identical to ext_plain")
    image, log = render(EXE, FIXTURES / "ext.glb", "disabled_required", env)
    check("requires EXT_meshopt_compression, which is switched off" in log and "Failed to load" in log
          and "Loaded successfully" not in log, "ext.glb (required) is not loaded; the log names the reason")

    print("[decode failure of a required stream]")
    image, log = render(EXE, FIXTURES / "ext_corrupt.glb", "corrupt")
    check("EXT_meshopt_compression: bufferView 0 cannot be decoded: ATTRIBUTES stream is invalid" in log
          and "Failed to load" in log and "Loaded successfully" not in log,
          "ext_corrupt.glb is not loaded; the log names the extension, the bufferView and the failure")
    check(gl_errors_zero(log), "ext_corrupt.glb: zero GL errors")

    print("[validator]")
    for model in ("ext.glb", "ext_gltf.gltf", "ext_optional.gltf", "khr.glb"):
        issues = validate(FIXTURES / model, model.replace(".", "_"))
        errors = [i.get("code") for i in issues or [] if i.get("severity") == "Error"]
        notices = [i.get("code") for i in issues or [] if str(i.get("code", "")).startswith("EXTENSION_")]
        check(issues is not None and not errors and not notices,
              f"{model}: no validation error, no unsupported-extension notice ({errors + notices})")
    issues = validate(unsupported, "unsupported") or []
    warnings = [i for i in issues if i.get("code") == "EXTENSION_REQUIRED_UNSUPPORTED"]
    infos = [i for i in issues if i.get("code") == "EXTENSION_USED_UNSUPPORTED"]
    check(len(warnings) == 1 and warnings[0].get("severity") == "Warning"
          and warnings[0].get("message") == "Required extension EXT_example_unsupported_required is not supported; "
                                            "the file may not display correctly."
          and warnings[0].get("pointer") == "/extensionsRequired/0",
          f"required unsupported extension: one warning {[(i.get('severity'), i.get('message'), i.get('pointer')) for i in warnings]}")
    check(len(infos) == 1 and infos[0].get("severity") == "Info"
          and "EXT_example_unsupported_used" in infos[0].get("message", "")
          and infos[0].get("pointer") == "/extensionsUsed/2",
          f"used-only unsupported extension: one info {[(i.get('severity'), i.get('message'), i.get('pointer')) for i in infos]}")

    print("[baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    print(f"  INFO baseline {baseline.name}")
    for name in ("ext_plain", "khr_plain"):
        before, _ = render(baseline, FIXTURES / f"{name}.glb", f"base_{name}")
        check(same(before, images[name]), f"{name}.glb: byte-identical to the baseline {baseline.name}")
    # A baseline from before d24305f has no decoder (its ext.glb render differs from the plain twin); once the
    # baseline decodes, the compressed render must stay byte-identical to it.
    before, _ = render(baseline, FIXTURES / "ext.glb", "base_ext")
    if same(before, images["ext_plain"]):
        check(same(before, images["ext"]), f"ext.glb: byte-identical to the baseline {baseline.name}")
    else:
        print(f"  INFO the baseline {baseline.name} has no meshopt decoder (ext.glb differs from its plain twin)")


if __name__ == "__main__":
    main()
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)
