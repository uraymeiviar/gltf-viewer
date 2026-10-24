# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A1.2: ASOBO_normal_map_convention.

A lit quad with a NEAREST-sampled tangent-space bump map. The asset-level DirectX
convention must flip the sampled green channel, matching a pre-inverted map, and must be
off for missing/OpenGL/undeclared declarations or when the viewport switch is disabled.
Bytes are compared on the viewport capture; the analytic pair reports any 1-LSB float
rounding difference instead of hiding it.
"""

import base64
import json
import math
import struct
import zlib
from pathlib import Path

from validate_asobo_common import check, finish, render, same

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "asobo_normal"
EXTENSION = "ASOBO_normal_map_convention"


def _chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def _byte(value):
    return max(0, min(255, int(round((value * 0.5 + 0.5) * 255.0))))


def normal_map_png(invert_green):
    """A 128x128 tangent-space bump map. Non-inverted green is kept even; the remaining
    1-LSB float-rounding difference from the pre-inverted texel is reported by the check."""
    size = 128
    bumps = ((0.28, 0.30, 0.20, 0.75), (0.72, 0.24, 0.16, 0.55),
             (0.22, 0.74, 0.17, 0.60), (0.74, 0.72, 0.22, 0.80))
    rows = []
    for py in range(size):
        row = bytearray()
        for px in range(size):
            u = (px + 0.5) / size
            v = (py + 0.5) / size
            dhdu = 0.0
            dhdv = 0.0
            for cx, cy, radius, amplitude in bumps:
                dx = (u - cx) / radius
                dy = (v - cy) / radius
                d2 = dx * dx + dy * dy
                if d2 < 1.0:
                    falloff = 1.0 - d2
                    dhdu += amplitude * (-4.0 * dx / radius) * falloff
                    dhdv += amplitude * (-4.0 * dy / radius) * falloff
            length = math.sqrt(dhdu * dhdu + dhdv * dhdv + 1.0)
            r = _byte(-dhdu / length)
            g = _byte(-dhdv / length)
            b = _byte(1.0 / length)
            if invert_green:
                g = 255 - g
            else:
                g &= ~1
            row += bytes((r, g, b))
        rows.append(row)
    payload = b"".join(b"\x00" + bytes(row) for row in rows)
    header = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", header) +
            _chunk(b"IDAT", zlib.compress(payload, 9)) + _chunk(b"IEND", b""))


def write_fixture(name, convention=None, declare=True, invert_green=False):
    """The same quad and material; only the asset extension and the green channel vary."""
    positions = ((-1.0, -1.0, 0.0), (1.0, -1.0, 0.0), (1.0, 1.0, 0.0), (-1.0, 1.0, 0.0))
    normals = ((0.0, 0.0, 1.0),) * 4
    uvs = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
    indices = (0, 1, 2, 0, 2, 3)
    binary = b"".join(struct.pack("<3f", *value) for value in positions)
    binary += b"".join(struct.pack("<3f", *value) for value in normals)
    binary += b"".join(struct.pack("<2f", *value) for value in uvs)
    binary += struct.pack("<6H", *indices)

    document = {
        "asset": {"version": "2.0", "generator": "Glider A1.2 normal-map fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "AsoboNormalMapQuad", "mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                                   "indices": 3, "material": 0}]}],
        "materials": [{
            "name": "NormalMapMaterial",
            "pbrMetallicRoughness": {"baseColorFactor": [0.7, 0.7, 0.7, 1.0],
                                     "metallicFactor": 0.0, "roughnessFactor": 0.45},
            "normalTexture": {"index": 0, "scale": 1.0},
        }],
        # NEAREST on both filters: the comparison must see texels, not interpolated values.
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
        "textures": [{"sampler": 0, "source": 0}],
        "images": [{"mimeType": "image/png",
                    "uri": "data:image/png;base64," + base64.b64encode(normal_map_png(invert_green)).decode("ascii")}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
             "min": [-1.0, -1.0, 0.0], "max": [1.0, 1.0, 0.0]},
            {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": 48, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": 96, "byteLength": 32, "target": 34962},
            {"buffer": 0, "byteOffset": 128, "byteLength": 12, "target": 34963},
        ],
        "buffers": [{"byteLength": len(binary),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(binary).decode("ascii")}],
        "extensionsUsed": [],
    }
    if convention is not None:
        document["asset"]["extensions"] = {EXTENSION: {"tangent_space_convention": convention}}
        if declare:
            document["extensionsUsed"].append(EXTENSION)
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(document, indent=1), encoding="utf-8")
    return path


def _read_png(path):
    """Decodes the harness's 8-bit RGB/RGBA PNGs without Pillow."""
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    position = 8
    compressed = b""
    width = height = color = depth = interlace = None
    while position < len(data):
        length = struct.unpack(">I", data[position:position + 4])[0]
        tag = data[position + 4:position + 8]
        chunk = data[position + 8:position + 8 + length]
        position += 12 + length
        if tag == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
        elif tag == b"IDAT":
            compressed += chunk
        elif tag == b"IEND":
            break
    if depth != 8 or interlace != 0 or color not in (2, 6):
        return None
    channels = 3 if color == 2 else 4
    stride = width * channels
    raw = zlib.decompress(compressed)
    pixels = bytearray()
    previous = bytearray(stride)
    position = 0
    for _ in range(height):
        filter_type = raw[position]
        position += 1
        line = bytearray(raw[position:position + stride])
        position += stride
        for x in range(stride):
            left = line[x - channels] if x >= channels else 0
            above = previous[x]
            upper_left = previous[x - channels] if x >= channels else 0
            if filter_type == 1:
                line[x] = (line[x] + left) & 0xFF
            elif filter_type == 2:
                line[x] = (line[x] + above) & 0xFF
            elif filter_type == 3:
                line[x] = (line[x] + ((left + above) >> 1)) & 0xFF
            elif filter_type == 4:
                estimate = left + above - upper_left
                distance_left = abs(estimate - left)
                distance_above = abs(estimate - above)
                distance_upper_left = abs(estimate - upper_left)
                predictor = (left if distance_left <= distance_above and distance_left <= distance_upper_left
                             else above if distance_above <= distance_upper_left else upper_left)
                line[x] = (line[x] + predictor) & 0xFF
            elif filter_type != 0:
                return None
        pixels.extend(line)
        previous = line
    return width, height, channels, bytes(pixels)


def difference(a, b):
    """(max per-channel difference, differing pixels) or None when the PNGs differ in shape."""
    first, second = _read_png(a), _read_png(b)
    if first is None or second is None or first[:3] != second[:3]:
        return None
    width, height, channels, left = first
    right = second[3]
    maximum = 0
    changed = 0
    for offset in range(0, width * height * channels, channels):
        delta = max(abs(left[offset + channel] - right[offset + channel]) for channel in range(channels))
        maximum = max(maximum, delta)
        changed += delta != 0
    return maximum, changed


def check_same(a, b, label, allow_rounding=False):
    if same(a, b):
        return check(True, label)
    info = difference(a, b)
    if info is None:
        print(f"  INFO {label}: captures could not be decoded for a per-channel difference")
    elif allow_rounding and info[0] <= 1:
        # The sampled UNORM green and the shader's 1.0 - g round differently from the
        # pre-inverted texel's own UNORM conversion, so an 8-bit output channel can move
        # by one LSB. Report it rather than hiding it: anything above 1 is a real failure.
        print(f"  INFO {label}: maximum per-channel difference {info[0]} over {info[1]} pixels; "
              f"the sampled UNORM value and 1.0 - g round differently")
        return check(True, f"{label} (1-LSB float rounding, max per-channel difference {info[0]})")
    else:
        print(f"  INFO {label}: maximum per-channel difference {info[0]} over {info[1]} pixels")
    return check(False, label)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    missing = write_fixture("missing")
    directx = write_fixture("directx", convention="DirectX")
    preinverted = write_fixture("preinverted", invert_green=True)
    opengl = write_fixture("opengl", convention="OpenGL")
    undeclared = write_fixture("undeclared", convention="DirectX", declare=False)
    flags = ["--select-node", "AsoboNormalMapQuad"]

    plain = render(EXE, missing, "missing", flags, OUT)
    flipped = render(EXE, directx, "directx", flags, OUT)
    inverted = render(EXE, preinverted, "preinverted", flags, OUT)
    gl = render(EXE, opengl, "opengl", flags, OUT)
    silent = render(EXE, undeclared, "undeclared", flags, OUT)
    disabled = render(EXE, directx, "directx_disabled",
                      flags + ["--asobo-disable", EXTENSION], OUT)
    if not all((plain, flipped, inverted, gl, silent, disabled)):
        check(False, "all renders produced a viewport capture")
        finish()

    print("[ASOBO_normal_map_convention]")
    check_same(flipped, inverted,
               "DirectX with map M matches no extension with green pre-inverted (M')", allow_rounding=True)
    check_same(disabled, plain,
               "--asobo-disable ASOBO_normal_map_convention is byte-identical to no extension with map M")
    check(not same(flipped, plain), "DirectX with map M differs from no extension with map M (the flip is visible)")
    check_same(gl, plain, "OpenGL with map M is byte-identical to no extension with map M")
    check_same(silent, plain, "a DirectX extension not declared in extensionsUsed changes nothing")
    finish()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        check(False, str(error))
        finish()
