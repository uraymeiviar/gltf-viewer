# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A2.3 decal modes: ASOBO_material_geometry_decal "blendMasked" and "frosted", and the legacy
ASOBO_material_blend_gbuffer (docs/ASOBO_EXTENSIONS_PHASE_2.md, A2.3; semantics §7).

Synthetic, self-contained fixtures: a black opaque receiver quad and a white decal quad 1 cm in
front of it, with vertex colour (1, 1, 1, vA). Only baseColorBlendFactor is 1 unless a case says
otherwise, so the base-colour debug view shows the decal coverage c directly: sRGB(c).

blendMasked coverage = base-colour alpha (texture x factor x vertex alpha) x ramp, with
    ramp = linearstep(vA; low = clamp(R - h, 0, 1), high = clamp(R + h, 0, 1)), h = max(1 - s, 1e-5)
for the blend-mask red channel R and blendSharpnessFactor s. The mask has four column bands:
R = 0 | R = 128/255 | a 0..1 gradient | R = 1. With vA = 0.5:

  endpoints   s = 1: R = 0 covered (c = 0.5), R = 128/255 and R = 1 uncovered (c = 0); s = 0: the
              ramp is vA everywhere (c = 0.25) and the mask has no effect.
  mid value   s = 0.5 at R = 128/255: ramp 0.499 (c = 0.2495); s = 0.8 with vA = 0.6 at R = 128/255:
              ramp 0.745 (c = 0.447), distinct from both endpoints.
  sharpness   s = 0.5 and s = 1 agree byte for byte in the saturated R = 0 and R = 1 bands and
              differ only in the transition bands (R = 128/255 and the gradient).
  no mask     blendMasked without blendMaskTexture renders like mode "default".
  frosted     coverage follows the icing setting (frost is weather; MSFS drives it from icing):
              at --windshield-icing 1 mode "frosted" renders byte-identical to mode "default" (all six
              views and the shaded view), with all factors 1 and with the DA62 Frost factors; at the
              default icing 0 it leaves the receiver as a default decal with every factor 0 does.
  blend_gbuffer  ASOBO_material_blend_gbuffer renders byte-identical to the equivalent default
              geometry decal.
  switches    --asobo-disable ASOBO_material_geometry_decal and --asobo off render the mode
              fixtures byte-identical to the baseline binary (the A1.4 forward fallback is unchanged).

Run: python scripts/validate_asobo_decal_modes.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import hashlib
import json
import re
import struct
import sys
import zlib
from pathlib import Path

import harness_process as subprocess
from validate_asobo_normal import _read_png

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "a2_3"
COMMON = ["--isolated", "--window-size", "1280x800", "--aa", "none",
          "--fixed-frame-time", "0.05", "--vsync", "off",
          "--select-node", "Receiver", "--camera-target", "0,0,0",
          "--camera-projection", "ortho", "--camera-yaw", "0", "--camera-pitch", "0",
          "--camera-distance", "2"]
VIEWS = ("basecolor", "metallic", "roughness", "shadingnormal", "emissive", "occlusion")
ALL_FACTORS = ("baseColorBlendFactor", "metallicBlendFactor", "roughnessBlendFactor",
               "normalBlendFactor", "emissiveBlendFactor", "occlusionBlendFactor")
BASE_ONLY = {name: (1.0 if name == "baseColorBlendFactor" else 0.0) for name in ALL_FACTORS}
# The DA62 Frost material (Part_Exterior_Airframe, Airframe_LOD00.gltf).
DA62_FROST = {"baseColorBlendFactor": 0.99, "metallicBlendFactor": 0.99, "roughnessBlendFactor": 0.99,
              "normalBlendFactor": 0.81, "emissiveBlendFactor": 0.0, "occlusionBlendFactor": 0.0}
MASK_WIDTH = 64
# Band centres in decal U (the mask varies along U only).
BANDS = {"r0": 0.125, "r128": 0.375, "gradient": 0.625, "r1": 0.875}

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest() if path and Path(path).is_file() else None


def same(a, b):
    return sha(a) is not None and sha(a) == sha(b)


def srgb(value):
    """The shader's linearTosRGB approximation (pow(v, 1/2.2)), as 8-bit."""
    return round(255.0 * (max(0.0, min(1.0, value)) ** (1.0 / 2.2)))


def png_bytes(width, height, rgba_rows):
    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(
            ">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + bytes(row) for row in rgba_rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def mask_png():
    """Four column bands: 0 | 128 | gradient 0..255 | 255 (red = green = blue)."""
    quarter = MASK_WIDTH // 4
    row = []
    for x in range(MASK_WIDTH):
        if x < quarter:
            value = 0
        elif x < 2 * quarter:
            value = 128
        elif x < 3 * quarter:
            value = round(255 * (x - 2 * quarter) / (quarter - 1))
        else:
            value = 255
        row.extend((value, value, value, 255))
    return png_bytes(MASK_WIDTH, 4, [row] * 4)


def write_fixture(name, *, extension="ASOBO_material_geometry_decal", decal=None, vertex_alpha=0.5,
                  with_mask=False, decal_node=True):
    """Receiver (black, opaque) + one white BLEND decal quad 1 cm in front with COLOR_0 alpha vA."""
    document = {
        "asset": {"version": "2.0", "generator": "A2.3 decal modes fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Receiver", "mesh": 0}],
        "meshes": [],
        "materials": [],
        "accessors": [],
        "bufferViews": [],
        "extensionsUsed": [extension, "ASOBO_material_draw_order"],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
    }
    binary = bytearray()

    def add_quad(z, half, material, node_name, color=None):
        positions = ((-half, -half, z), (half, -half, z), (half, half, z), (-half, half, z))
        normals = ((0.0, 0.0, 1.0),) * 4
        uvs = ((0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0))
        indices = (0, 1, 2, 0, 2, 3)
        blocks = [b"".join(struct.pack("<3f", *v) for v in positions),
                  b"".join(struct.pack("<3f", *v) for v in normals),
                  b"".join(struct.pack("<2f", *v) for v in uvs)]
        if color is not None:
            blocks.append(b"".join(struct.pack("<4f", *color) for _ in positions))
        blocks.append(b"".join(struct.pack("<H", v) for v in indices))
        accessor = len(document["accessors"])
        attributes = {}
        kinds = [("POSITION", "VEC3", 5126), ("NORMAL", "VEC3", 5126), ("TEXCOORD_0", "VEC2", 5126)]
        if color is not None:
            kinds.append(("COLOR_0", "VEC4", 5126))
        kinds.append(("indices", "SCALAR", 5123))
        for block, (semantic, kind, component) in zip(blocks, kinds):
            view = len(document["bufferViews"])
            document["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(block),
                                            "target": 34963 if semantic == "indices" else 34962})
            binary.extend(block)
            while len(binary) % 4:
                binary.append(0)
            entry = {"bufferView": view, "componentType": component,
                     "count": len(indices) if semantic == "indices" else len(positions), "type": kind}
            if semantic == "POSITION":
                entry["min"] = [min(v[a] for v in positions) for a in range(3)]
                entry["max"] = [max(v[a] for v in positions) for a in range(3)]
            document["accessors"].append(entry)
            if semantic != "indices":
                attributes[semantic] = len(document["accessors"]) - 1
        mesh = len(document["meshes"])
        document["meshes"].append({"name": node_name, "primitives": [{
            "attributes": attributes, "indices": len(document["accessors"]) - 1, "material": material}]})
        if node_name != "Receiver":
            document["nodes"].append({"name": node_name, "mesh": mesh})
            document["scenes"][0]["nodes"].append(len(document["nodes"]) - 1)

    document["materials"].append({
        "name": "Receiver", "alphaMode": "OPAQUE",
        "pbrMetallicRoughness": {"baseColorFactor": [0.0, 0.0, 0.0, 1.0], "metallicFactor": 0.0,
                                 "roughnessFactor": 1.0},
    })
    decal_extension = dict(decal if decal is not None else {})
    if with_mask:
        decal_extension["blendMaskTexture"] = {"index": 0}
    document["materials"].append({
        "name": "Decal", "alphaMode": "BLEND",
        "pbrMetallicRoughness": {"baseColorFactor": [1.0, 1.0, 1.0, 1.0], "metallicFactor": 1.0,
                                 "roughnessFactor": 0.1},
        "emissiveFactor": [0.8, 0.4, 0.1],
        "extensions": {extension: decal_extension,
                       "ASOBO_material_draw_order": {"drawOrderOffset": 10}},
    })
    add_quad(0.0, 1.0, 0, "Receiver")
    if decal_node:
        add_quad(0.01, 0.5, 1, "Decal", color=(1.0, 1.0, 1.0, vertex_alpha))
    document["buffers"] = [{"byteLength": len(binary), "uri": "data:application/octet-stream;base64,"
                            + base64.b64encode(bytes(binary)).decode("ascii")}]
    document["images"] = [{"uri": "data:image/png;base64," + base64.b64encode(mask_png()).decode("ascii")}]
    document["textures"] = [{"source": 0, "sampler": 0}]
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(document, indent=1), encoding="utf-8")
    return path


def capture(model, view, label, extra=(), exe=EXE):
    png = OUT / f"{label}_{view or 'shaded'}.png"
    png.unlink(missing_ok=True)
    debug = ["--debug-view", view] if view else []
    result = subprocess.run([str(exe), str(model), *COMMON, *extra, *debug,
                             "--capture-viewport", str(png), "--capture-frame", "24",
                             "--profile-report", str(png.with_suffix(".json")),
                             "--exit-after-profile"], cwd=REPO, capture_output=True, text=True, timeout=300)
    if result.returncode != 0 or not png.is_file():
        check(False, f"{label}_{view}: capture failed ({result.returncode}) {result.stderr[-200:]}")
        return None
    log = result.stdout + result.stderr
    png.with_suffix(".log").write_text(log, encoding="utf-8")
    if exe == EXE and ("GL automation errors: 0" not in log or re.search(r"GL error (?:0x|during)|GL error \(0x", log)):
        check(False, f"{label}_{view}: GL diagnostics failed; see capture log")
        return None
    return png


def image(path):
    decoded = _read_png(path) if path is not None else None
    return decoded  # (width, height, channels, data)


def decal_bounds(path):
    """Columns/rows of the full-coverage decal (bright on black) in the base-colour view."""
    width, height, channels, data = image(path)
    xs, ys = [], []
    for y in range(0, height):
        for x in range(0, width):
            if data[(y * width + x) * channels] > 128:
                xs.append(x)
                ys.append(y)
    return min(xs), max(xs), min(ys), max(ys)


def band_pixels(path, bounds, u):
    """Red values of a 7-pixel-high, 5-pixel-wide patch at decal U = u, vertical centre."""
    width, height, channels, data = image(path)
    x0, x1, y0, y1 = bounds
    cx = round(x0 + u * (x1 - x0))
    cy = (y0 + y1) // 2
    return [data[(y * width + x) * channels] for y in range(cy - 3, cy + 4) for x in range(cx - 2, cx + 3)]


def band_columns(bounds, lo, hi):
    x0, x1, _, _ = bounds
    return round(x0 + lo * (x1 - x0)), round(x0 + hi * (x1 - x0))


def region_equal(a, b, bounds, lo, hi):
    """True when the two images are byte-identical inside decal U [lo, hi] (all decal rows)."""
    wa, ha, ca, da = image(a)
    wb, hb, cb, db = image(b)
    if (wa, ha, ca) != (wb, hb, cb):
        return False
    c0, c1 = band_columns(bounds, lo, hi)
    _, _, y0, y1 = bounds
    for y in range(y0, y1 + 1):
        start = (y * wa + c0) * ca
        end = (y * wa + c1 + 1) * ca
        if da[start:end] != db[start:end]:
            return False
    return True


def near(values, expected, tolerance=2):
    return all(abs(v - expected) <= tolerance for v in values)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=None,
                        help="binary before A2.3, for the switch-off checks (default: build/baselines/<HEAD>.exe)")
    args = parser.parse_args()
    baseline_exe = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline_exe is None:
        head = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=REPO, capture_output=True,
                              text=True).stdout.strip()
        baseline_exe = REPO / "build" / "baselines" / f"{head}.exe"
    OUT.mkdir(parents=True, exist_ok=True)

    print("[reference: full-coverage default decal locates the decal]")
    full = write_fixture("full_default", decal={**BASE_ONLY, "mode": "default"}, vertex_alpha=1.0)
    full_png = capture(full, "basecolor", "full_default")
    if full_png is None:
        print("RESULT: FAIL (reference capture)")
        sys.exit(1)
    bounds = decal_bounds(full_png)
    print(f"  decal spans x {bounds[0]}..{bounds[1]}, y {bounds[2]}..{bounds[3]}")

    def masked(sharpness, label, vertex_alpha=0.5):
        model = write_fixture(label, decal={**BASE_ONLY, "mode": "blendMasked",
                                            "blendSharpnessFactor": sharpness},
                              vertex_alpha=vertex_alpha, with_mask=True)
        return model, capture(model, "basecolor", label)

    print("[blendMasked endpoints, vA = 0.5]")
    _, hard = masked(1.0, "masked_s1")
    _, soft = masked(0.0, "masked_s0")
    _, mid = masked(0.5, "masked_s05")
    covered, quarter = srgb(0.5), srgb(0.25)
    if hard:
        for band, expected in (("r0", covered), ("r128", 0), ("r1", 0)):
            values = band_pixels(hard, bounds, BANDS[band])
            check(near(values, expected), f"s = 1, mask {band}: {sorted(set(values))} == {expected}")
        gradient = [band_pixels(hard, bounds, u)[17] for u in (0.52, 0.56, 0.69, 0.73)]
        check(gradient[0] == covered and gradient[1] == covered and gradient[2] == 0 and gradient[3] == 0,
              f"s = 1 gradient band is a hard step at R = vA: {gradient}")
    if soft:
        for band in BANDS:
            values = band_pixels(soft, bounds, BANDS[band])
            check(near(values, quarter), f"s = 0, mask {band}: {sorted(set(values))} == vA coverage {quarter}")

    print("[blendMasked mid value and mask 0/1 regions, s = 0.5]")
    if mid:
        expected_mid = srgb(0.5 * (0.5 - (128 / 255 - 0.5)) / (min(1.0, 128 / 255 + 0.5) - (128 / 255 - 0.5)))
        values = band_pixels(mid, bounds, BANDS["r128"])
        check(near(values, expected_mid), f"s = 0.5, R = 128/255: {sorted(set(values))} == {expected_mid}")
        check(near(band_pixels(mid, bounds, BANDS["r0"]), covered), "s = 0.5, mask 0: covered")
        check(near(band_pixels(mid, bounds, BANDS["r1"]), 0), "s = 0.5, mask 1: uncovered")
    # A mid value away from both endpoints: vA = 0.6, s = 0.8 (h = 0.2) at R = 128/255:
    # ramp = (0.6 - (R - 0.2)) / 0.4 = 0.745, c = 0.6 * ramp (s = 1 would give 0.6, s = 0 0.36).
    _, partial = masked(0.8, "masked_s08_va06", vertex_alpha=0.6)
    if partial:
        r = 128 / 255
        ramp = (0.6 - (r - 0.2)) / 0.4
        expected = srgb(0.6 * ramp)
        values = band_pixels(partial, bounds, BANDS["r128"])
        check(near(values, expected) and not near(values, srgb(0.6)) and not near(values, srgb(0.36)),
              f"s = 0.8, vA = 0.6, R = 128/255: {sorted(set(values))} == {expected} (ramp {ramp:.3f})")

    print("[sharpness 0.5 vs 1: identical where the ramp saturates, different at the transition]")
    if hard and mid:
        check(region_equal(hard, mid, bounds, 0.02, 0.23), "R = 0 band byte-identical")
        check(region_equal(hard, mid, bounds, 0.77, 0.98), "R = 1 band byte-identical")
        check(not region_equal(hard, mid, bounds, 0.27, 0.48), "R = 128/255 band differs")
        check(not region_equal(hard, mid, bounds, 0.52, 0.73), "gradient band differs")
    if hard and soft:
        check(not region_equal(hard, soft, bounds, 0.02, 0.23),
              "s = 0 differs from s = 1 also in the saturated bands (the s = 0 ramp is vA, mask-independent)")

    print("[blendMasked without a blend mask renders like mode default]")
    no_mask = write_fixture("masked_no_texture", decal={**BASE_ONLY, "mode": "blendMasked",
                                                        "blendSharpnessFactor": 1.0})
    default_half = write_fixture("default_half", decal={**BASE_ONLY, "mode": "default"})
    a = capture(no_mask, "basecolor", "masked_no_texture")
    b = capture(default_half, "basecolor", "default_half")
    check(same(a, b), "blendMasked without blendMaskTexture == default (base colour)")

    print("[frosted at icing 1 renders like default; at icing 0 like no decal]")
    all_ones = {name: 1.0 for name in ALL_FACTORS}
    for label, factors in (("ones", all_ones), ("da62", DA62_FROST)):
        frosted = write_fixture(f"frosted_{label}", decal={**factors, "mode": "frosted"}, with_mask=True)
        default = write_fixture(f"default_{label}", decal={**factors, "mode": "default"}, with_mask=True)
        for view in (*VIEWS, None):
            a = capture(frosted, view, f"frosted_{label}", ("--windshield-icing", "1"))
            b = capture(default, view, f"default_{label}")
            check(same(a, b), f"frosted {label} factors, icing 1, {view or 'shaded'}: byte-identical to default")
    zero = write_fixture("default_zero", decal={**{name: 0.0 for name in ALL_FACTORS}, "mode": "default"}, with_mask=True)
    frosted = write_fixture("frosted_ones", decal={**all_ones, "mode": "frosted"}, with_mask=True)
    for view in ("basecolor", None):
        a = capture(frosted, view, "frosted_ones_icing0")
        b = capture(zero, view, "default_zero")
        check(same(a, b), f"frosted, icing 0 (default), {view or 'shaded'}: byte-identical to a zero-factor decal")

    print("[blend_gbuffer renders like the equivalent default geometry decal]")
    gbuffer = write_fixture("gbuffer", extension="ASOBO_material_blend_gbuffer", decal=DA62_FROST)
    geometry = write_fixture("gbuffer_equivalent", decal={**DA62_FROST, "mode": "default"})
    for view in (*VIEWS, None):
        a = capture(gbuffer, view, "gbuffer")
        b = capture(geometry, view, "gbuffer_equivalent")
        check(same(a, b), f"blend_gbuffer, {view or 'shaded'}: byte-identical to the default decal")

    print(f"[switches off: unchanged from the baseline {baseline_exe.name}]")
    if not baseline_exe.is_file():
        check(False, f"baseline binary {baseline_exe} exists")
    else:
        masked_model = write_fixture("switch_masked", decal={**BASE_ONLY, "mode": "blendMasked",
                                                             "blendSharpnessFactor": 1.0}, with_mask=True)
        frosted_model = write_fixture("switch_frosted", decal={**DA62_FROST, "mode": "frosted"}, with_mask=True)
        for model, name in ((masked_model, "masked"), (frosted_model, "frosted")):
            for flags, flag_name in ((("--asobo-disable", "ASOBO_material_geometry_decal"), "decal_off"),
                                     (("--asobo", "off"), "asobo_off")):
                for view in ("basecolor", None):
                    now = capture(model, view, f"switch_{name}_{flag_name}", flags)
                    before = capture(model, view, f"switch_{name}_{flag_name}_baseline", flags, exe=baseline_exe)
                    check(same(now, before), f"{name}, {' '.join(flags)}, {view or 'shaded'}: byte-identical to baseline")
        # With the switch on, the blendMasked mask now shapes the decal (baseline: the A2.2 default path).
        now = capture(masked_model, "basecolor", "switch_masked_on")
        before = capture(masked_model, "basecolor", "switch_masked_on_baseline", exe=baseline_exe)
        check(now is not None and before is not None and not same(now, before),
              "blendMasked with the switch on differs from the baseline default coverage")

    print("=" * 60)
    if failures:
        for failure in failures:
            print("  - " + failure)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
