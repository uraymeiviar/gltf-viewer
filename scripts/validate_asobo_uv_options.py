# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A5.2: deterministic main UV options, CPU-baked references and real-asset review.

Run this suite directly; run_regression.py registers it in the serial ASOBO list.
All generated fixtures and captures stay under build/. SDK and add-ons are read-only.
"""
import base64
import json
import math
import re
import struct
import sys
from pathlib import Path

import harness_process as subprocess
from PIL import Image
from validate_asobo_detail_map import Fixture, make_png, base_material, CAMERA, same

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build/bin/Release/Glider.exe"
OUT = REPO / "build/asobo_uv_options"
EXT = "ASOBO_material_UV_options"
DETAIL = "ASOBO_material_detail_map"
failures = []


def check(ok, label):
    print(("  PASS " if ok else "  FAIL ") + label, flush=True)
    if not ok:
        failures.append(label)
    return ok


def transform(uv, options):
    # Independent CPU oracle: scale about centre, positive CCW coordinate rotation,
    # positive offsets in glTF UV units. +V moves the displayed pattern toward -V.
    x = (uv[0] - .5) * options.get("UVTilingU", 1)
    y = (uv[1] - .5) * options.get("UVTilingV", 1)
    a = math.radians(options.get("UVRotation", 0))
    return (math.cos(a)*x - math.sin(a)*y + .5 + options.get("UVOffsetU", options.get("UVOffSetU", 0)),
            math.sin(a)*x + math.cos(a)*y + .5 + options.get("UVOffsetV", options.get("UVOffSetV", 0)))


def fixture(name, options=None, slot="base", uv1=False, baked=None, khr=None,
            detail=False, mask=False, windshield=False, declare=True, sampler=None):
    f = Fixture(name)
    # Asymmetric, recognizable 8x8 pattern with distinct rows and columns.
    rows = [[(32 + x*28, 32 + y*28, 48 + ((x//2 + y//2) % 2)*144, 255)
             for x in range(8)] for y in range(8)]
    texture = f.add_png_texture(make_png(8, 8, rows))
    info = {"index": texture}
    if uv1:
        info["texCoord"] = 1
    used = []
    if khr:
        info["extensions"] = {"KHR_texture_transform": khr}
        used.append("KHR_texture_transform")
    material = base_material((.8, .8, .8, 1))
    if slot == "base":
        material["pbrMetallicRoughness"]["baseColorTexture"] = info
    elif slot == "orm":
        material["pbrMetallicRoughness"]["metallicRoughnessTexture"] = info
    elif slot == "normal":
        material["normalTexture"] = info
    elif slot == "emissive":
        material["emissiveTexture"] = info
        material["emissiveFactor"] = [1, 1, 1]
    elif slot == "ao":
        material["occlusionTexture"] = info
    extensions = {}
    if options is not None:
        extensions[EXT] = options
        if declare:
            used.append(EXT)
    if detail or mask or windshield:
        extensions[DETAIL] = {"detailColorTexture": info, "UVScale": 1.5 if detail else 1}
        if mask:
            solid = f.add_png_texture(make_png(1, 1, [[(192, 48, 96, 255)]]))
            extensions[DETAIL] = {"detailColorTexture": {"index": solid},
                                  "blendMaskTexture": info, "UVScale": 3, "blendThreshold": .05}
        used.append(DETAIL)
    if windshield:
        extensions["ASOBO_material_windshield_v3"] = {}
        used.append("ASOBO_material_windshield_v3")
    material["extensions"] = extensions
    # Reuse only the quad writer, redirecting its output directory for this suite.
    import validate_asobo_detail_map as writer
    old = writer.OUT
    writer.OUT = OUT
    try:
        path = f.write(material, extensions_used=used, declare_detail=False, color_alpha=.5 if mask else 1)
    finally:
        writer.OUT = old
    doc = json.loads(path.read_text())
    if sampler:
        doc["samplers"][0].update(sampler)
    # Explicit UV1 attribute; it shares the original UV accessor, while UV0 can be baked independently.
    attrs = doc["meshes"][0]["primitives"][0]["attributes"]
    if uv1 or (khr and khr.get("texCoord") == 1):
        attrs["TEXCOORD_1"] = attrs["TEXCOORD_0"]
    if baked is not None:
        raw = bytearray(base64.b64decode(doc["buffers"][0]["uri"].split(",")[1]))
        for i, uv in enumerate(((0,0), (1,0), (1,1), (0,1))):
            struct.pack_into("<2f", raw, 96 + i*8, *transform(uv, baked))
        doc["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(raw).decode()
    path.write_text(json.dumps(doc, indent=1), encoding="utf-8")
    return path


def capture(path, label, flags=(), view="basecolor", exe=EXE):
    png = OUT / (label + ".png")
    png.unlink(missing_ok=True)
    report = OUT / (label + ".json")
    args = [str(exe), str(path), *CAMERA, "--msfs-emissive", "off", *flags]
    if view:
        args += ["--debug-view", view]
    result = subprocess.run(args + ["--capture-viewport", str(png), "--capture-frame", "8",
                                   "--profile-report", str(report), "--exit-after-profile"],
                            cwd=REPO, capture_output=True, text=True, timeout=300)
    log = result.stdout + result.stderr
    (OUT / (label + ".log")).write_text(log, encoding="utf-8")
    if result.returncode & 0xffffffff == 0xc0000142:
        raise SystemExit("STOP: Windows 0xc0000142; no further Glider launches permitted by brief")
    if not check(result.returncode == 0 and png.exists(), label + ": capture completed"):
        return None
    check("GL automation errors: 0" in log and not re.search(r"GL error (?:0x|during)|GL error \(0x", log),
          label + ": zero GL errors")
    return png


def probes_equal(actual, reference, label):
    if actual is None or reference is None:
        return check(False, label + ": missing capture")
    a = Image.open(actual).convert("RGB")
    b = Image.open(reference).convert("RGB")
    # Sample the interior of the quad, away from texel/raster boundaries. Each
    # checked RGB triplet must match exactly; no image-difference tolerance.
    box = Image.open(OUT / "plain.png").convert("RGB").getbbox()
    left, top, right, bottom = box
    points = [(left + int((right-left)*u), top + int((bottom-top)*v))
              for u in (.071, .193, .317, .441, .563, .687, .809, .931)
              for v in (.083, .207, .329, .453, .577, .701, .823, .947)]
    bad = [(p, a.getpixel(p), b.getpixel(p)) for p in points if a.getpixel(p) != b.getpixel(p)]
    return check(not bad, f"{label}: 64 exact spatial probes ({len(bad)} mismatches)" + (str(bad[:2]) if bad else ""))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    plain = capture(fixture("plain"), "plain")
    defaults = {"clampUVX": False, "clampUVY": False, "UVTilingU": 1, "UVTilingV": 1,
                "UVOffsetU": 0, "UVOffsetV": 0, "UVRotation": 0}
    for name, options in (("empty", {}), ("defaults", defaults), ("clamp_z", {"clampUVZ": True})):
        check(same(plain, capture(fixture(name, options), name)), name + ": byte-identical to no extension")
    cases = {"tile2": {"UVTilingU": 2, "UVTilingV": 2},
             "offset_u": {"UVOffsetU": .25}, "offset_v": {"UVOffsetV": .25},
             "rotate90": {"UVRotation": 90},
             "order": {"UVTilingU": 2, "UVTilingV": 3, "UVRotation": 27, "UVOffsetU": .17, "UVOffsetV": -.23},
             "aliases": {"UVOffSetU": .25, "UVOffSetV": -.25},
             "both_spellings": {"UVOffsetU": .25, "UVOffSetU": -.25, "UVOffsetV": -.25, "UVOffSetV": .25}}
    for name, options in cases.items():
        actual = capture(fixture(name, options), name)
        expected = capture(fixture(name + "_reference", baked=options), name + "_reference")
        probes_equal(actual, expected, name + ": CPU-baked UV reference")
        check(not same(actual, plain), name + ": visible spatial change")
    log = (OUT / "both_spellings.log").read_text()
    check(log.count("Both offset spellings are present") == 1, "offset spelling conflict logged once")
    tiled = {"UVTilingU": 2, "UVTilingV": 2}
    tiled_model = fixture("tile2", tiled)
    for name, flags in (("disabled", ["--asobo-disable", EXT]), ("master_off", ["--asobo", "off"])):
        check(same(plain, capture(tiled_model, name, flags)), name + ": byte-identical to no extension")
    check(same(plain, capture(fixture("undeclared", tiled, declare=False), "undeclared")),
          "undeclared extension is ignored")
    for axes in ((True, False), (False, True), (True, True)):
        suffix = "clamp_" + str(int(axes[0])) + str(int(axes[1]))
        options = {**tiled, "clampUVX": axes[0], "clampUVY": axes[1]}
        # The reference uses the same baked coordinates and independent GL clamp samplers.
        reference = fixture(suffix + "_reference", baked=tiled,
                            sampler={"wrapS": 33071 if axes[0] else 10497, "wrapT": 33071 if axes[1] else 10497})
        a = capture(fixture(suffix, options), suffix)
        probes_equal(a, capture(reference, suffix + "_reference"), suffix + ": edge texels stretch")
        check(not same(a, OUT / "tile2.png"), suffix + ": differs from repeat")
    repeat = capture(fixture("clamp_false", {**tiled, "clampUVX": False, "clampUVY": False}), "clamp_false")
    check(same(repeat, OUT / "tile2.png"), "false and absent clamps repeat identically")
    for slot, view in (("base", "basecolor"), ("orm", "roughness"), ("normal", "shadingnormal"),
                       ("emissive", "emissive"), ("ao", "occlusion")):
        for uv1 in (False, True):
            name = slot + ("_uv1" if uv1 else "_uv0")
            options = {"UVOffsetU": .25, "UVOffsetV": .25}
            a = capture(fixture(name, options, slot=slot, uv1=uv1), name, view=view)
            b = capture(fixture(name + "_reference", slot=slot, uv1=uv1,
                                baked=None if uv1 else options), name + "_reference", view=view)
            probes_equal(a, b, name + ": correct UV selection")
            if uv1:
                check(same(a, b), name + ": UV1 byte-identical with transform active")
    options = {"UVTilingU": 2, "UVOffsetV": .25}
    khr = {"offset": [.13, -.21], "scale": [.7, 1.3], "rotation": .4}
    a = capture(fixture("khr", options, khr=khr), "khr")
    b = capture(fixture("khr_reference", baked=options, khr=khr), "khr_reference")
    probes_equal(a, b, "ASOBO first, then texture KHR transform")
    # The effective UV set is selected by KHR, even when textureInfo uses UV0.
    override = {"texCoord": 1, "offset": [.13, -.21], "scale": [.7, 1.3]}
    a = capture(fixture("khr_uv1", options, khr=override), "khr_uv1")
    b = capture(fixture("khr_uv1_reference", khr=override), "khr_uv1_reference")
    check(same(a, b), "KHR texCoord override to UV1 bypasses ASOBO transform")
    # Clamping follows KHR, and retains the texture's own repeat on the other axis.
    a = capture(fixture("khr_clamp", {**options, "clampUVX": True}, khr=khr), "khr_clamp")
    b = capture(fixture("khr_clamp_reference", baked=options, khr=khr,
                        sampler={"wrapS": 33071}), "khr_clamp_reference")
    probes_equal(a, b, "per-axis clamp follows KHR transform")
    a = capture(fixture("detail_uv1", options, detail=True, uv1=True), "detail_uv1")
    b = capture(fixture("detail_uv1_reference", detail=True, uv1=True), "detail_uv1_reference")
    check(same(a, b), "UV1 detail texture bypasses ASOBO transform")
    for mode in ("detail", "mask", "windshield"):
        kw = {mode: True}
        if mode == "windshield":
            kw["slot"] = "emissive"
        a = capture(fixture(mode, options, **kw), mode)
        b = capture(fixture(mode + "_reference", baked=options if mode != "windshield" else None, **kw), mode + "_reference")
        probes_equal(a, b, mode + ": UV composition/exclusion")
    # Existing Inspector setter path refreshes the material variant in memory.
    edited = capture(fixture("edit", {}), "edited", ["--set-asobo-param", "edit", EXT, "UVTilingU", "2",
                                                       "--set-asobo-param", "edit", EXT, "UVTilingV", "2"])
    check(same(edited, OUT / "tile2.png"), "Inspector parameter edits refresh the UV variant")
    baseline = REPO / "build/baselines/d0761e7.exe"
    for name, model in (("hello", REPO / "resources/sampleGltf/HelloCube.glb"),
                        ("non_asobo", REPO / "resources/test_models/hierarchy_animation_fixture.gltf")):
        a = capture(model, name, view=None)
        b = capture(model, name + "_baseline", exe=baseline, view=None)
        check(same(a, b), name + ": byte-identical to d0761e7")
    (OUT / "results.json").write_text(json.dumps({"failures": failures}, indent=2))
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"), flush=True)
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
