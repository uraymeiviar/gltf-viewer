# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A2.1 decal buffer and prepass validation.

The shared decal buffer is created only for files with a decal material, is sized to the
largest render-scale viewport that needs it, and is released when no viewport needs it.
The prepass draws OPAQUE and MASK geometry (except decals) into DEPTH_COMPONENT32F depth
and RGB10_A2 G0 before the forward opaque pass. Nothing reads the buffer yet, so default
renders must stay byte-identical to the saved baseline.

Sections:
  prepass     synthetic fixture: prepass depth equals the forward depth (readback), G0
              equals the forward Geometry Normal debug view, MASK geometry is included,
              two nodes of one mesh exercise the Phase 8 instancing path, and the decal
              material is excluded.
  no-buffer   HelloCube (no decal material), the fixture with --asobo off and with the
              geometry-decal switch disabled must not create the buffer.
  debug-views "Decal prepass depth" and "Decal prepass normal" replace the viewport image.
  viewports   resize, open a second and third viewport, close them: no GL errors and the
              memory is counted once for the shared buffer.
  memory      measured buffer size at 1600x900 and 1920x1080 render resolutions.
  baseline    E195 cockpit and HelloCube dumps are byte-identical to build/baselines/8d58094.exe.
  benchmark   optional --bench: DecalPrepass GPU pass time on the E195 cockpit.
"""

import argparse
import base64
import hashlib
import json
import statistics
import struct
import sys
from pathlib import Path

import harness_process as subprocess

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
BASELINE = REPO / "build" / "baselines" / "8d58094.exe"
OUT = REPO / "build" / "a2_1"
FIXTURE = OUT / "fixture" / "decal_prepass_fixture.gltf"
HELLO = "resources/sampleGltf/HelloCube.glb"
COMMON = ["--isolated", "--window-size", "1280x800", "--aa", "none",
          "--fixed-frame-time", "0.05", "--vsync", "off"]
BYTES_PER_PIXEL = 33  # depth 4 + G0 4 + A0 4 + A1 8 + A2 8 + A3 2 + A4 2 + A5 1
FLOAT_ULP = 1.1920928955078125e-7  # one ulp at 1.0f
ONE_OVER_512 = 1.0 / 512.0

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=("prepass", "no-buffer", "debug-views", "viewports", "memory", "baseline", "benchmark"))
parser.add_argument("--bench", action="store_true", help="also measure the E195 prepass time")
args = parser.parse_args()
failures = []


def section(name):
    if name == "benchmark":
        return args.only == "benchmark" or args.bench
    return args.only in (None, name)


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(arguments, timeout=900):
    return subprocess.run([str(EXE)] + [str(a) for a in arguments], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def fixture():
    """Two instanced cubes, an opaque MASK plane and an invisible decal triangle."""
    faces = [
        ((0, 0, 1), [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
        ((0, 0, -1), [(1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1)]),
        ((1, 0, 0), [(1, -1, 1), (1, -1, -1), (1, 1, -1), (1, 1, 1)]),
        ((-1, 0, 0), [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)]),
        ((0, 1, 0), [(-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)]),
        ((0, -1, 0), [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)]),
    ]
    positions, normals, indices = [], [], []
    for normal, vertices in faces:
        base = len(positions)
        for vertex in vertices:
            positions.append(vertex)
            normals.append(normal)
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]

    # Opaque MASK plane at x=4.5, facing +Z (the alpha test keeps it, and the prepass
    # must draw it exactly like the forward pass).
    mask_base = len(positions)
    for vertex in [(3.75, -0.75, 1.5), (5.25, -0.75, 1.5), (5.25, 0.75, 1.5), (3.75, 0.75, 1.5)]:
        positions.append(vertex)
        normals.append((0, 0, 1))
    mask_indices = [mask_base, mask_base + 1, mask_base + 2, mask_base, mask_base + 2, mask_base + 3]

    # A zero-area decal triangle: it exercises the Transparent pass and the decal class
    # without drawing a single fragment into the forward image.
    decal_base = len(positions)
    for _ in range(3):
        positions.append((0.0, 0.0, -5.0))
        normals.append((0, 0, 1))
    decal_indices = [decal_base, decal_base + 1, decal_base + 2]

    positions_bytes = b"".join(struct.pack("<3f", *p) for p in positions)
    normals_bytes = b"".join(struct.pack("<3f", *p) for p in normals)
    indices_bytes = b"".join(struct.pack("<H", i) for i in indices + mask_indices + decal_indices)
    index_offset = len(positions_bytes) + len(normals_bytes)
    pad = (4 - index_offset % 4) % 4
    blob = positions_bytes + normals_bytes + b"\0" * pad + indices_bytes
    # Indices are at byte 2 of each 2-byte element, so the accessor byteOffset must sit
    # on a 2-byte boundary (index_offset is already even here).
    index_offset += pad

    document = {
        "asset": {"version": "2.0", "generator": "A2.1 validation fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0, 1, 2, 3]}],
        "nodes": [
            {"name": "OpaqueCubeA", "mesh": 0, "translation": [-1.3, 0, 0]},
            {"name": "OpaqueCubeB", "mesh": 0, "translation": [1.3, 0, 0]},
            {"name": "MaskPlane", "mesh": 1},
            {"name": "InvisibleDecal", "mesh": 2},
        ],
        "meshes": [
            {"name": "Cube", "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 0}]},
            {"name": "MaskPlane", "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 3, "material": 1}]},
            {"name": "Decal", "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 4, "material": 2}]},
        ],
        "materials": [
            {"name": "OpaqueReceiver",
             "pbrMetallicRoughness": {"baseColorFactor": [0.6, 0.3, 0.2, 1.0], "metallicFactor": 0.1, "roughnessFactor": 0.7}},
            {"name": "MaskReceiver", "alphaMode": "MASK", "alphaCutoff": 0.5,
             "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.8, 0.2, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.8}},
            {"name": "TransparentDecal", "alphaMode": "BLEND",
             "pbrMetallicRoughness": {"baseColorFactor": [1.0, 1.0, 1.0, 0.0]},
             "extensions": {"ASOBO_material_geometry_decal": {}, "ASOBO_material_draw_order": {"drawOrderOffset": 10}}},
        ],
        "buffers": [{"uri": "data:application/octet-stream;base64," + base64.b64encode(blob).decode(), "byteLength": len(blob)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(positions_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": len(positions_bytes), "byteLength": len(normals_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": index_offset, "byteLength": len(indices_bytes), "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3",
             "min": [-1.0, -1.0, -5.0], "max": [5.25, 1.0, 1.5]},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
            {"bufferView": 2, "componentType": 5123, "count": len(mask_indices), "type": "SCALAR", "byteOffset": len(indices) * 2},
            {"bufferView": 2, "componentType": 5123, "count": len(decal_indices), "type": "SCALAR", "byteOffset": (len(indices) + len(mask_indices)) * 2},
        ],
        "extensionsUsed": ["ASOBO_material_geometry_decal", "ASOBO_material_draw_order"],
    }
    FIXTURE.parent.mkdir(parents=True, exist_ok=True)
    FIXTURE.write_text(json.dumps(document, indent=1), encoding="utf-8")


def read_json(path):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def prepass_check(model, extra=None, name="check"):
    report = OUT / f"{name}.json"
    report.unlink(missing_ok=True)
    result = run([model, *COMMON, *([] if extra is None else extra),
                  "--debug-view", "geometrynormal", "--decal-prepass-check", report])
    data = read_json(report)
    if result.returncode != 0 or data is None:
        check(False, f"{name}: exit {result.returncode} {result.stderr[-300:]}")
        return None
    return data


def dump(model, debug_view, path):
    path.unlink(missing_ok=True)
    result = run([*([model] if model else []), *COMMON, "--debug-view", debug_view,
                  "--dump-frame", path, "--exit-after-dump"])
    if result.returncode != 0 or not path.is_file():
        check(False, f"dump {debug_view}: exit {result.returncode} {result.stderr[-300:]}")
        return False
    return True


if section("prepass"):
    print("[prepass: fixture depth, G0, MASK, instancing, decal exclusion]")
    fixture()
    data = prepass_check(str(FIXTURE), name="prepass")
    if data is not None:
        buffer = data.get("decalBuffer", {})
        depth = data.get("depth", {})
        normal = data.get("normal", {})
        check(buffer.get("active") is True, "buffer is active for the decal fixture")
        check(depth.get("compared") is True and depth.get("pixels", 0) > 1000,
              f"prepass depth compared on {depth.get('pixels', 0)} receiver pixels")
        check(depth.get("maxAbsDiff", 1.0) <= FLOAT_ULP,
              f"prepass depth max |diff| {depth.get('maxAbsDiff')} <= one float ulp")
        check(depth.get("overOneUlp", 1) == 0,
              f"no depth pixel beyond one float ulp ({depth.get('exactPixels')}/{depth.get('pixels')} exact)")
        check(normal.get("compared") is True, "G0 compared with the forward Geometry Normal view")
        check(normal.get("maxEncodedDiff", 1.0) <= ONE_OVER_512,
              f"G0 normal max encoded diff {normal.get('maxEncodedDiff')} <= 1/512 "
              f"(byte diff {normal.get('maxChannelDiff8')})")
        check(normal.get("overOne512", 1) == 0, "no G0 normal channel beyond 1/512")
        check(data.get("glErrors", 1) == 0, "no GL errors during the check")

    # The two cube nodes share one mesh/material, so both passes use the instancing path.
    report = OUT / "instancing.json"
    script = OUT / "instancing.txt"
    script.write_text("wait 10\nsnap instancing\n", encoding="utf-8")
    result = run([str(FIXTURE), *COMMON, "--ui-script", script, "--ui-report", report])
    snaps = (read_json(report) or {}).get("snaps", [])
    instanced = snaps[0].get("revisions", {}).get("instancedDraws", 0) if snaps else 0
    check(result.returncode == 0 and instanced >= 2,
          f"prepass participates in Phase 8 instancing (instanced draws: {instanced})")
    passes = snaps[0]["viewports"][0]["activePasses"] if snaps else []
    check("DecalPrepass" in passes, f"DecalPrepass executes before forward Opaque: {passes}")

if section("no-buffer"):
    print("[no-buffer: no decals, ASOBO off, decal switch off]")
    report = OUT / "hello.json"
    report.unlink(missing_ok=True)
    result = run([*COMMON, "--decal-prepass-check", report])
    hello = read_json(report)
    check(result.returncode == 0 and hello is not None and hello.get("decalBuffer", {}).get("active") is False,
          "HelloCube never creates the decal buffer")
    off = prepass_check(str(FIXTURE), ["--asobo", "off"], name="asobo_off")
    check(off is not None and off.get("decalBuffer", {}).get("active") is False,
          "--asobo off never creates the decal buffer")
    switch = prepass_check(str(FIXTURE), ["--asobo-disable", "ASOBO_material_geometry_decal"], name="switch_off")
    check(switch is not None and switch.get("decalBuffer", {}).get("active") is False,
          "the ASOBO_material_geometry_decal switch gates the buffer")

if section("debug-views"):
    print("[debug-views: prepass depth and normal replace the viewport image]")
    default_path = OUT / "view_default.png"
    depth_path = OUT / "view_depth.png"
    normal_path = OUT / "view_normal.png"
    ok = dump(str(FIXTURE), "shaded", default_path)
    ok = dump(str(FIXTURE), "decalprepassdepth", depth_path) and ok
    ok = dump(str(FIXTURE), "decalprepassnormal", normal_path) and ok
    if ok:
        default, depth, normal = (sha256(p) for p in (default_path, depth_path, normal_path))
        check(depth != default, "Decal prepass depth differs from the shaded view")
        check(normal != default, "Decal prepass normal differs from the shaded view")
        check(normal != depth, "depth and normal views differ")
    check(dump(None, "decalprepassnormal", OUT / "hello_decal_view.png"),
          "inactive buffer falls back instead of failing")

if section("viewports"):
    print("[viewports: resize, two more viewports, close them]")
    script = OUT / "viewports.txt"
    report = OUT / "viewports.json"
    script.write_text("\n".join([
        "wait 8", "snap start", "resize 1600 900", "wait 12", "snap resized",
        "vpcreate", "wait 10", "vpsplit 2", "wait 10",
        "vpcreate", "wait 10", "vpsplit 3", "wait 16", "snap three",
        "vptoggle 2", "wait 6", "vptoggle 3", "wait 12", "snap one",
        "vptoggle 1", "wait 16", "snap closed",
        "vptoggle 1", "wait 20", "snap reopened",
    ]) + "\n", encoding="utf-8")
    result = run([str(FIXTURE), "--isolated", "--window-size", "1024x700", "--aa", "none",
                  "--ui-script", script, "--ui-report", report])
    data = read_json(report) or {}
    snaps = {snap.get("label"): snap for snap in data.get("snaps", [])}
    check(result.returncode == 0 and not data.get("failed", True), "viewport script completes")
    for label in ("start", "resized", "three", "one", "closed", "reopened"):
        snap = snaps.get(label)
        if not check(snap is not None, f"snapshot {label} exists"):
            continue
        check(snap.get("glErrors", 1) == 0, f"{label}: no GL errors")
        buffer = snap.get("decalBuffer", {})
        if buffer.get("active"):
            check(buffer.get("bytes") == buffer.get("width") * buffer.get("height") * BYTES_PER_PIXEL,
                  f"{label}: one shared buffer, {buffer.get('width')}x{buffer.get('height')} = {buffer.get('bytes')} B")
    check(snaps.get("start", {}).get("decalBuffer", {}).get("active") is True, "start: buffer active")
    resized = snaps.get("resized", {}).get("decalBuffer", {})
    start = snaps.get("start", {}).get("decalBuffer", {})
    check(resized.get("width") != start.get("width") or resized.get("height") != start.get("height"),
          "resizing the viewport resizes the buffer")
    three = snaps.get("three", {})
    open_viewports = [vp for vp in three.get("viewports", []) if vp.get("open")]
    check(len(open_viewports) == 3, f"three viewports open ({len(open_viewports)})")
    largest_w = max((vp["renderW"] for vp in open_viewports), default=0)
    largest_h = max((vp["renderH"] for vp in open_viewports), default=0)
    three_buffer = three.get("decalBuffer", {})
    check(three_buffer.get("width") == largest_w and three_buffer.get("height") == largest_h,
          f"three viewports share one buffer sized to the largest ({largest_w}x{largest_h})")
    check(all("DecalPrepass" in vp.get("activePasses", []) for vp in open_viewports),
          "every open viewport rendered its prepass")
    closed = snaps.get("closed", {}).get("decalBuffer", {})
    check(closed.get("active") is False and closed.get("bytes") == 0 and closed.get("releases", 0) >= 1,
          "closing the last viewport releases the buffer")
    reopened = snaps.get("reopened", {}).get("decalBuffer", {})
    check(reopened.get("active") is True and reopened.get("bytes") == reopened.get("width") * reopened.get("height") * BYTES_PER_PIXEL,
          "reopening a viewport recreates the buffer")

if section("memory"):
    print("[memory: 1600x900 and 1920x1080 render-scale resolutions]")
    for label, window in (("1600x900", "2781x1004"), ("1920x1080", "3335x1184")):
        script = OUT / f"memory_{label}.txt"
        report = OUT / f"memory_{label}.json"
        script.write_text("wait 12\nsnap memory\n", encoding="utf-8")
        result = run([str(FIXTURE), "--isolated", "--window-size", window, "--aa", "none",
                      "--ui-script", script, "--ui-report", report])
        snaps = (read_json(report) or {}).get("snaps", [])
        buffer = snaps[0].get("decalBuffer", {}) if snaps else {}
        check(result.returncode == 0 and buffer.get("active") is True,
              f"{label}: buffer active")
        if buffer.get("active"):
            bytes_expected = buffer.get("width") * buffer.get("height") * BYTES_PER_PIXEL
            check(buffer.get("bytes") == bytes_expected,
                  f"{label}: {buffer.get('width')}x{buffer.get('height')} = {buffer.get('bytes')} B "
                  f"({buffer.get('bytes', 0) / (1024 * 1024):.2f} MiB)")
    print(f"  reference: 1600x900 -> {1600 * 900 * BYTES_PER_PIXEL} B, "
          f"1920x1080 -> {1920 * 1080 * BYTES_PER_PIXEL} B")

if section("baseline"):
    print("[baseline: default renders byte-identical to build/baselines/8d58094.exe]")
    if not check(BASELINE.is_file(), f"baseline exists: {BASELINE}"):
        pass
    else:
        def compare(model, name):
            current = OUT / f"{name}_current.png"
            base = OUT / f"{name}_baseline.png"
            for path in (current, base):
                path.unlink(missing_ok=True)
            current_args = [*([model] if model else []), *COMMON, "--dump-frame", current, "--exit-after-dump"]
            base_args = [*([model] if model else []), *COMMON, "--dump-frame", base, "--exit-after-dump"]
            current_result = run(current_args)
            base_result = subprocess.run([str(BASELINE)] + [str(a) for a in base_args], cwd=REPO,
                                         capture_output=True, text=True, timeout=900)
            if current_result.returncode != 0 or base_result.returncode != 0 or not current.is_file() or not base.is_file():
                check(False, f"{name}: dump failed ({current_result.returncode}/{base_result.returncode})")
                return
            check(sha256(current) == sha256(base), f"{name}: dumps byte-identical ({sha256(current)[:16]})")

        compare(None, "hello")
        sys.path.insert(0, str(REPO / "scripts"))
        try:
            from e195_assets import COCKPIT, textured_copy
            e195 = textured_copy(COCKPIT, OUT / "e195_assets")
        except Exception as error:  # noqa: BLE001 - report the missing asset, do not hide it
            e195 = None
            print(f"  E195 asset unavailable: {error}")
        if check(e195 is not None, "E195 cockpit asset prepared"):
            compare(str(e195), "e195")

if section("benchmark"):
    print("[benchmark: E195 DecalPrepass GPU pass time]")
    sys.path.insert(0, str(REPO / "scripts"))
    from e195_assets import COCKPIT, textured_copy
    e195 = textured_copy(COCKPIT, OUT / "e195_assets")
    if check(e195 is not None, "E195 cockpit asset prepared"):
        report = OUT / "e195_benchmark.json"
        report.unlink(missing_ok=True)
        result = run([str(e195), *COMMON, "--disable-redraw-skip", "--profile", "--benchmark",
                      "--benchmark-warmup", "16", "--benchmark-frames", "40",
                      "--benchmark-output", report, "--exit-after-benchmark"])
        data = read_json(report) or {}
        values = []
        for frame in data.get("gpu", {}).get("completed", []) + data.get("gpu", {}).get("pending", []):
            for pass_report in frame.get("passes", []):
                if pass_report.get("name") == "DecalPrepass" and pass_report.get("available"):
                    values.append(pass_report["ms"])
        viewport = data.get("viewport", {})
        check(result.returncode == 0 and len(values) > 0,
              f"benchmark report has {len(values)} DecalPrepass samples")
        if values:
            values.sort()
            print(f"  viewport {viewport.get('width')}x{viewport.get('height')}, "
                  f"min {values[0]:.4f} ms, median {statistics.median(values):.4f} ms, max {values[-1]:.4f} ms")

print()
if failures:
    print(f"FAILED ({len(failures)}):")
    for failure in failures:
        print("  - " + failure)
    sys.exit(1)
print("A2.1 checks passed")
