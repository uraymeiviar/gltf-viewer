# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A1.9: ASOBO_asset_optimized vertex decoding and windshield v3 vertex colour masks.

The MSFS package tool rewrites vertex attributes without the glTF normalisation
flags: TEXCOORD_n and COLOR_0 hold IEEE half-floats in SHORT/USHORT accessors,
NORMAL and TANGENT hold signed bytes (value / 127), and the 48-byte skinned
COLOR_0 holds RGBA8 bytes in a BYTE accessor. This harness checks:
  - a synthetic lit, textured quad with float attributes and a copy stored the
    packaged way (declaring ASOBO_asset_optimized) render within a small
    tolerance; the same packaged copy without the declaration is byte-identical
    to the pre-change binary;
  - ASOBO_asset_optimized is not switchable: --asobo-disable changes nothing;
  - the E190 exterior and E195 cockpit source/package pairs render with the same
    camera; per-channel mean and maximum differences are reported, and the
    packaged E190 fuselage is no longer white;
  - a windshield v3 quad with a red COLOR_0 renders like the same quad without
    COLOR_0, disabling the extension restores the old tint, and a standard
    material still multiplies its base colour by the same COLOR_0.
"""

import base64
import json
import math
import os
import struct
import sys
import types
from pathlib import Path

# validate_asobo_common imports e195_assets, which copies E195 models on import.
# This harness only needs its render/compare helpers, so stub the unused module.
_e195_assets = types.ModuleType("e195_assets")
_e195_assets.COCKPIT = None
_e195_assets.EXTERIOR = None
_e195_assets.TEXTURE_FOLDERS = []
_e195_assets.textured_copy = lambda *args, **kwargs: None
sys.modules.setdefault("e195_assets", _e195_assets)

from validate_asobo_common import COMMON, check, finish, render, same

from PIL import Image, ImageChops, ImageStat

REPO = Path(__file__).resolve().parents[1]
EXE = Path(os.environ.get("GLIDER_EXE", REPO / "build" / "bin" / "Release" / "Glider.exe"))
# The pre-A1.9 reference is fixed, not GLIDER_BASELINE: run_regression.py points that at
# the current HEAD baseline (6285c86), which already contains A1.9. a9c0a29 predates it.
PRE_A19_EXE = REPO / "build" / "baselines" / "a9c0a29.exe"
OUT = REPO / "build" / "asobo_packaged"
ASSET_OPTIMIZED = "ASOBO_asset_optimized"
WINDSHIELD = "ASOBO_material_windshield_v3"

E190_SOURCE = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e190\SimObjects\Airplanes\E190\model\E190-Exterior-LOD00.gltf")
E190_PACKAGE = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\Packages\fss-aircraft-e190\SimObjects\Airplanes\E190\model\E190-Exterior-LOD00.gltf")
E195_SOURCE = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf")
E195_PACKAGE = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\Packages\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf")

E190_CAMERA = ["--camera-target", "0.0,0.8,13.5", "--camera-yaw", "-45", "--camera-pitch", "-10",
               "--camera-distance", "4.8"]
E195_CAMERA = ["--select-node", "SCREEN_EICAS", "--camera-yaw", "180", "--camera-pitch", "-8",
               "--camera-distance", "0.75"]
# The E190 fuselage in the reference view, in viewport-capture pixels (918x796).
FUSELAGE_REGION = (500, 300, 900, 550)
TOLERANCE = 10

POSITIONS = ((-1.0, -1.0, 0.0), (1.0, -1.0, 0.0), (1.0, 1.0, 0.0), (-1.0, 1.0, 0.0))
NORMALS = ((0.2, 0.3, 0.9327379),) * 4
TANGENTS = ((0.98, 0.1, 0.0, 1.0),) * 4
UV0 = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
UV1 = ((0.13, 0.21), (0.87, 0.21), (0.87, 0.79), (0.13, 0.79))
INDICES = (0, 1, 2, 0, 2, 3)


def make_png(width, height, pixel):
    """A minimal uncompressed 8-bit RGBA PNG; pixel(x, y) returns (r, g, b, a)."""
    import zlib

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            raw.extend(pixel(x, y))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def half_bits(value):
    return struct.unpack("<H", struct.pack("<e", float(value)))[0]


def signed_byte(value):
    return max(-127, min(127, int(round(float(value) * 127.0))))


def geometry(packed, with_color=True, color_value=(1.0, 0.62, 0.31, 1.0)):
    """The quad's bufferViews/accessors, stored as floats or in the package layout."""
    chunks = []
    views = []
    accessors = []

    def add(data, component_type, type_, count, minmax=None):
        offset = sum(len(chunk) for chunk in chunks)
        padding = (-offset) % 4
        if padding:
            chunks.append(b"\0" * padding)
            offset += padding
        chunks.append(data)
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": 34962})
        accessor = {"bufferView": len(views) - 1, "componentType": component_type, "count": count, "type": type_}
        if minmax is not None:
            accessor["min"], accessor["max"] = minmax
        accessors.append(accessor)
        return len(accessors) - 1

    if packed:
        position = add(b"".join(struct.pack("<3f", *p) for p in POSITIONS), 5126, "VEC3", 4,
                       ([-1.0, -1.0, 0.0], [1.0, 1.0, 0.0]))
        normal = add(b"".join(struct.pack("<4b", *(signed_byte(c) for c in n), 127) for n in NORMALS),
                     5120, "VEC4", 4)
        tangent = add(b"".join(struct.pack("<4b", *(signed_byte(c) for c in t)) for t in TANGENTS),
                      5120, "VEC4", 4)
        uv0 = add(b"".join(struct.pack("<2H", *(half_bits(c) for c in uv)) for uv in UV0), 5122, "VEC2", 4)
        uv1 = add(b"".join(struct.pack("<2H", *(half_bits(c) for c in uv)) for uv in UV1), 5122, "VEC2", 4)
        color = (add(b"".join(struct.pack("<4H", *(half_bits(c) for c in color_value))
                             for _ in POSITIONS), 5123, "VEC4", 4) if with_color else None)
    else:
        position = add(b"".join(struct.pack("<3f", *p) for p in POSITIONS), 5126, "VEC3", 4,
                       ([-1.0, -1.0, 0.0], [1.0, 1.0, 0.0]))
        normal = add(b"".join(struct.pack("<3f", *n) for n in NORMALS), 5126, "VEC3", 4)
        tangent = add(b"".join(struct.pack("<4f", *t) for t in TANGENTS), 5126, "VEC4", 4)
        uv0 = add(b"".join(struct.pack("<2f", *uv) for uv in UV0), 5126, "VEC2", 4)
        uv1 = add(b"".join(struct.pack("<2f", *uv) for uv in UV1), 5126, "VEC2", 4)
        color = (add(b"".join(struct.pack("<4f", *color_value) for _ in POSITIONS),
                     5126, "VEC4", 4) if with_color else None)

    indices = add(struct.pack("<6H", *INDICES), 5123, "SCALAR", 6)
    attributes = {"POSITION": position, "NORMAL": normal, "TANGENT": tangent,
                  "TEXCOORD_0": uv0, "TEXCOORD_1": uv1}
    if color is not None:
        attributes["COLOR_0"] = color
    binary = b"".join(chunks)
    return binary, views, accessors, attributes, indices


def quad_document(name, packed, extensions_used, with_color=True, material_extension=None,
                  base_color_texcoord=0, color_value=(1.0, 0.62, 0.31, 1.0)):
    binary, views, accessors, attributes, indices = geometry(packed, with_color, color_value)
    material = {
        "name": "Quad",
        "pbrMetallicRoughness": {
            "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
            "baseColorTexture": {"index": 0, "texCoord": base_color_texcoord},
            "metallicFactor": 0.0,
            "roughnessFactor": 0.6,
        },
    }
    if material_extension is not None:
        material["extensions"] = {material_extension: {}}
    document = {
        "asset": {"version": "2.0", "generator": "Glider A1.9 packaged vertex fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Quad", "mesh": 0}],
        "meshes": [{"primitives": [{"attributes": attributes, "indices": indices, "material": 0}]}],
        "materials": [material],
        "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071}],
        "textures": [{"sampler": 0, "source": 0}],
        "images": [{"uri": "quad_albedo.png"}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(binary),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(binary).decode("ascii")}],
    }
    if extensions_used:
        document["extensionsUsed"] = extensions_used
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(document), encoding="utf-8")
    return path


def texture_quad(name, packed, extensions_used, with_color, material_extension=None):
    """The synthetic test quad with a smooth albedo texture and a tan normal map."""
    document = quad_document(name, packed, extensions_used, with_color, material_extension)
    data = json.loads(document.read_text(encoding="utf-8"))
    data["images"].append({"uri": "quad_normal.png"})
    data["textures"].append({"sampler": 0, "source": 1})
    data["materials"][0]["normalTexture"] = {"index": 1, "texCoord": 0}
    document.write_text(json.dumps(data), encoding="utf-8")
    return document


def write_textures():
    # A low-contrast gradient keeps half-float UV rounding far below one LSB while
    # still making a wrong UV decode obvious.
    (OUT / "quad_albedo.png").write_bytes(make_png(64, 64, lambda x, y: (90 + x, 100 + y, 120, 255)))
    nx, ny = 0.3, 0.2
    nz = math.sqrt(max(0.0, 1.0 - nx * nx - ny * ny))
    normal = (round((nx + 1.0) * 0.5 * 255.0), round((ny + 1.0) * 0.5 * 255.0),
              round((nz + 1.0) * 0.5 * 255.0), 255)
    (OUT / "quad_normal.png").write_bytes(make_png(64, 64, lambda x, y: normal))


def search_paths(model):
    """--search-path arguments for every sibling texture* folder."""
    root = Path(model).parent.parent
    return [f"../{folder.name}" for folder in sorted(root.iterdir()) if folder.is_dir() and folder.name.lower().startswith("texture")]


def png_difference(a, b):
    """(mean per channel, maximum per channel) or None when the captures differ in shape."""
    if a is None or b is None:
        return None
    first = Image.open(a).convert("RGB")
    second = Image.open(b).convert("RGB")
    if first.size != second.size:
        return None
    difference = ImageChops.difference(first, second)
    stat = ImageStat.Stat(difference)
    return [round(value, 3) for value in stat.mean], [high for _, high in stat.extrema]


def region_mean(path, box):
    pixels = list(Image.open(path).convert("RGB").crop(box).getdata())
    return tuple(round(sum(p[channel] for p in pixels) / len(pixels), 1) for channel in range(3))


def near_white_share(path, box):
    pixels = list(Image.open(path).convert("RGB").crop(box).getdata())
    return sum(1 for p in pixels if min(p) > 235) / max(1, len(pixels))


def check_close(a, b, label, tolerance):
    if same(a, b):
        return check(True, f"{label} (byte-identical)")
    difference = png_difference(a, b)
    if difference is None:
        check(False, f"{label}: captures missing or differently sized")
        return False
    mean, maximum = difference
    print(f"  INFO {label}: mean per-channel {mean}, maximum {maximum}")
    return check(max(maximum) <= tolerance, f"{label} (tolerance {tolerance}, maximum {max(maximum)})")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if not EXE.exists():
        check(False, f"Glider executable not found at {EXE}")
        finish()
    write_textures()

    source = texture_quad("packed_source", False, [], True)
    declared = texture_quad("packed_declared", True, [ASSET_OPTIMIZED], True)
    undeclared = texture_quad("packed_undeclared", True, [], True)
    data = json.loads(declared.read_text(encoding="utf-8"))
    data["asset"]["extensions"] = {ASSET_OPTIMIZED: {}}
    declared.write_text(json.dumps(data), encoding="utf-8")

    flags = ["--select-node", "Quad"]
    print("[synthetic packaged quad]")
    source_capture = render(EXE, source, "packed_source", flags, OUT)
    declared_capture = render(EXE, declared, "packed_declared", flags, OUT)
    undeclared_capture = render(EXE, undeclared, "packed_undeclared", flags, OUT)
    disabled_capture = render(EXE, declared, "packed_declared_disabled", flags + ["--asobo-disable", ASSET_OPTIMIZED], OUT)
    check_close(declared_capture, source_capture, "packaged copy renders like the float quad", TOLERANCE)
    check(not same(declared_capture, undeclared_capture),
          "the declaration changes the render (the packed copy is not read as integers)")
    if PRE_A19_EXE.exists():
        undeclared_baseline = render(PRE_A19_EXE, undeclared, "packed_undeclared_baseline", flags, OUT)
        check(same(undeclared_capture, undeclared_baseline),
              "packaged copy without the declaration is byte-identical to the pre-change binary")
    else:
        print(f"  SKIP pre-A1.9 binary {PRE_A19_EXE} not found")
    check(same(declared_capture, disabled_capture),
          "--asobo-disable ASOBO_asset_optimized does not switch the decoding off")

    print("[windshield v3 vertex colour]")
    windshield = quad_document("windshield_red", False, [WINDSHIELD], True, WINDSHIELD,
                               color_value=(1.0, 0.0, 0.0, 1.0))
    windshield_without = quad_document("windshield_no_color", False, [WINDSHIELD], False, WINDSHIELD)
    standard = quad_document("standard_red", False, [], True, color_value=(1.0, 0.0, 0.0, 1.0))
    standard_without = quad_document("standard_no_color", False, [], False)
    quad_flags = ["--select-node", "Quad"]
    windshield_capture = render(EXE, windshield, "windshield_red", quad_flags, OUT)
    windshield_without_capture = render(EXE, windshield_without, "windshield_no_color", quad_flags, OUT)
    standard_capture = render(EXE, standard, "standard_red", quad_flags, OUT)
    standard_without_capture = render(EXE, standard_without, "standard_no_color", quad_flags, OUT)
    disabled = render(EXE, windshield, "windshield_red_disabled", quad_flags + ["--asobo-disable", WINDSHIELD], OUT)
    master_off = render(EXE, windshield, "windshield_red_master_off", quad_flags + ["--asobo", "off"], OUT)
    check(same(windshield_capture, windshield_without_capture),
          "windshield v3 with a red COLOR_0 renders like the same quad without COLOR_0")
    check(not same(standard_capture, standard_without_capture) and not same(windshield_capture, standard_capture),
          "a standard material with the same COLOR_0 is still tinted")
    if PRE_A19_EXE.exists():
        disabled_baseline = render(PRE_A19_EXE, windshield, "windshield_red_disabled_baseline", quad_flags, OUT)
        check(same(disabled, disabled_baseline),
              "--asobo-disable ASOBO_material_windshield_v3 renders as the pre-change binary")
        check(same(master_off, disabled_baseline), "--asobo off also renders as the pre-change binary")
        standard_baseline = render(PRE_A19_EXE, standard, "standard_red_baseline", quad_flags, OUT)
        check(same(standard_capture, standard_baseline),
              "the standard material keeps its pre-change vertex-colour tint")
    else:
        print(f"  SKIP pre-A1.9 binary {PRE_A19_EXE} not found")

    print("[real source/package pairs]")
    if not all(path.exists() for path in (E190_SOURCE, E190_PACKAGE, E195_SOURCE, E195_PACKAGE)):
        print("  SKIP one or more E190/E195 source or package files not found")
    else:
        e190_source = render(EXE, E190_SOURCE, "e190_source", E190_CAMERA, OUT, search_paths(E190_SOURCE))
        e190_package = render(EXE, E190_PACKAGE, "e190_package", E190_CAMERA, OUT, search_paths(E190_PACKAGE))
        e195_source = render(EXE, E195_SOURCE, "e195_source", E195_CAMERA, OUT, search_paths(E195_SOURCE))
        e195_package = render(EXE, E195_PACKAGE, "e195_package", E195_CAMERA, OUT, search_paths(E195_PACKAGE))
        if not all((e190_source, e190_package, e195_source, e195_package)):
            check(False, "real source/package renders produced a viewport capture")
        else:
            for label, first, second in (("E190 exterior", e190_source, e190_package),
                                         ("E195 cockpit", e195_source, e195_package)):
                difference = png_difference(first, second)
                if difference is None:
                    check(False, f"{label}: captures missing or differently sized")
                    continue
                mean, maximum = difference
                print(f"  INFO {label}: mean per-channel {mean}, maximum {maximum}")
                check(max(maximum) > 1, f"{label}: source and package differ")
            source_mean = region_mean(e190_source, FUSELAGE_REGION)
            package_mean = region_mean(e190_package, FUSELAGE_REGION)
            white_share = near_white_share(e190_package, FUSELAGE_REGION)
            print(f"  INFO E190 fuselage region {FUSELAGE_REGION}: source {source_mean}, package {package_mean}, "
                  f"near-white {white_share:.1%}")
            channel_delta = max(abs(a - b) for a, b in zip(source_mean, package_mean))
            check(white_share < 0.05 and min(package_mean) < 200,
                  "packaged E190 fuselage is no longer white")
            check(channel_delta < 60,
                  f"packaged E190 fuselage colour is close to the source (largest channel delta {channel_delta:.1f})")

    finish()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        check(False, str(error))
        finish()
