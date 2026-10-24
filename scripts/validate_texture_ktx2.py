# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A1.8: block-compressed KTX2 (MSFT_texture_dds) validation harness.

Packaged MSFS 2024 glTF files point MSFT_texture_dds at BCn KTX2 images. This
harness generates small KTX2 files in Python (constant-colour blocks, 2+ mip
levels) and checks that Glider uploads them compressed and with mip levels:
  - BC1, BC3 and BC7 albedo on a quad render the expected sRGB-decoded colour;
  - a BC5 SNORM normal map renders like the equivalent PNG normal map, with and
    without ASOBO_normal_map_convention=DirectX;
  - an unsupported vkFormat logs the file and format and does not crash;
  - a model without KTX2 textures is byte-identical to the baseline binary.
"""

import base64
import json
import math
import os
import struct
import subprocess
import sys
import types
import zlib
from pathlib import Path

# validate_asobo_common imports e195_assets, which copies the E195 models on
# import. This harness only needs its render/compare helpers, so keep the run
# self-contained and read-only with a stub for the unused asset module.
_e195_assets = types.ModuleType("e195_assets")
_e195_assets.COCKPIT = None
_e195_assets.EXTERIOR = None
_e195_assets.TEXTURE_FOLDERS = []
_e195_assets.textured_copy = lambda *args, **kwargs: None
sys.modules.setdefault("e195_assets", _e195_assets)

from validate_asobo_common import COMMON, check, finish, render, same

REPO = Path(__file__).resolve().parents[1]
EXE = Path(os.environ.get("GLIDER_EXE", REPO / "build" / "bin" / "Release" / "Glider.exe"))
BASELINE = Path(os.environ.get("GLIDER_BASELINE", REPO / "build" / "baselines" / "6af3972.exe"))
OUT = REPO / "build" / "texture_ktx2"
EXTENSION = "MSFT_texture_dds"
NORMAL_EXTENSION = "ASOBO_normal_map_convention"

# DFDs copied from MSFS 2024 BCn KTX2 files. The vkFormat in the header selects
# the format; Glider does not use the DFD for upload.
DFD = {
    133: "2c00000000000000020028008001010003030000080000000000000000003f010000000000000000ffffffff",  # BC1 RGBA UNORM
    137: "3c00000000000000020038008201010003030000100000000000000000003f0f0000000000000000ffffffff40003f000000000000000000ffffffff",  # BC3 UNORM
    142: "3c00000000000000020038008401010003030000100000000000000000003f400000000000000080ffffff7f40003f410000000000000080ffffff7f",  # BC5 SNORM
    145: "2c00000000000000020028008601010003030000100000000000000000007f000000000000000000ffffffff",  # BC7 UNORM
}

KTX2_IDENTIFIER = b"\xABKTX 20\xBB\r\n\x1A\n"


def _block_bytes(vk_format):
    return 8 if vk_format in (133, 142) else 16


def _rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def bc1_block(color):
    """A 4x4 block whose first colour is color and whose indices are all 0."""
    return struct.pack("<HHI", _rgb565(*color), 0, 0)


def bc3_block(color):
    """Opaque alpha block plus a BC1 colour block."""
    return bytes((255, 0)) + b"\0" * 6 + bc1_block(color)


def bc7_mode6_block(color):
    """A constant BC7 mode-6 block; all four components must share one parity."""
    r, g, b, a = color
    assert r & 1 == g & 1 == b & 1 == a & 1
    low = 0
    bit = 0

    def put(value, count):
        nonlocal low, bit
        low |= (value & ((1 << count) - 1)) << bit
        bit += count

    put(0x40, 7)  # mode 6
    for component in color:
        put(component >> 1, 7)
        put(component >> 1, 7)
    put(r & 1, 1)  # p0 is shared by endpoint 0
    high = a & 1  # p1 is shared by endpoint 1; every selector remains 0
    return low.to_bytes(8, "little") + high.to_bytes(8, "little")


def bc5_block(x, y):
    """Two BC4 blocks storing signed [-1, 1] X and Y; index 0 returns the endpoint."""
    def bc4(value):
        endpoint = int(round(max(-1.0, min(1.0, value)) * 127.0)) & 0xFF
        return bytes((endpoint, 0)) + b"\0" * 6

    return bc4(x) + bc4(y)


def make_ktx2(vk_format, width, height, level_blocks):
    """Builds a mipmapped, non-supercompressed KTX2 file around constant blocks."""
    block_bytes = _block_bytes(vk_format)
    align = math.lcm(4, block_bytes)
    levels = []
    for level, block in enumerate(level_blocks):
        level_width = max(width >> level, 1)
        level_height = max(height >> level, 1)
        blocks = ((level_width + 3) // 4) * ((level_height + 3) // 4)
        levels.append(block * blocks)

    level_count = len(levels)
    dfd = bytes.fromhex(DFD[vk_format])
    dfd_offset = 80 + 24 * level_count
    cursor = dfd_offset + len(dfd)
    payload = bytearray()
    offsets = [0] * level_count
    # KTX2 files write the smallest mip first; the level index keeps the
    # level 0..levelCount-1 order regardless of the payload order.
    for level_index in reversed(range(level_count)):
        level = levels[level_index]
        padding = (-cursor) % align
        payload += b"\0" * padding
        cursor += padding
        offsets[level_index] = cursor
        payload += level
        cursor += len(level)

    header = struct.pack(
        "<12s9I4I2Q",
        KTX2_IDENTIFIER,
        vk_format,
        1,
        width,
        height,
        0,
        0,
        1,
        level_count,
        0,
        dfd_offset,
        len(dfd),
        0,
        0,
        0,
        0,
    )
    index = b"".join(struct.pack("<QQQ", offset, len(level), len(level))
                     for offset, level in zip(offsets, levels))
    return header + index + dfd + bytes(payload)


def make_unsupported_ktx2(name, vk_format=139):
    """A KTX2 header with an unsupported BCn vkFormat; Glider must not parse its data."""
    width = height = 8
    dfd_offset = 80 + 24
    header = struct.pack(
        "<12s9I4I2Q",
        KTX2_IDENTIFIER,
        vk_format,
        1,
        width,
        height,
        0,
        0,
        1,
        1,
        0,
        dfd_offset,
        0,
        0,
        0,
        0,
        0,
    )
    index = struct.pack("<QQQ", dfd_offset, 0, 0)
    path = OUT / name
    path.write_bytes(header + index)
    return path


def make_png(width, height, r, g, b, a=255):
    """Creates a raw uncompressed 8-bit RGBA PNG image."""
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = bytes([r, g, b, a] * width)
    payload = b"".join(b"\x00" + raw for _ in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(payload, 9)) + chunk(b"IEND", b""))


def quad_document(name, material, images, texture_extensions, extensions_used):
    positions = ((-1.0, -1.0, 0.0), (1.0, -1.0, 0.0), (1.0, 1.0, 0.0), (-1.0, 1.0, 0.0))
    normals = ((0.0, 0.0, 1.0),) * 4
    uvs = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
    indices = (0, 1, 2, 0, 2, 3)
    binary = b"".join(struct.pack("<3f", *value) for value in positions)
    binary += b"".join(struct.pack("<3f", *value) for value in normals)
    binary += b"".join(struct.pack("<2f", *value) for value in uvs)
    binary += struct.pack("<6H", *indices)

    texture = {"sampler": 0}
    if texture_extensions is not None:
        texture["extensions"] = {EXTENSION: {"source": texture_extensions}}
        texture["source"] = 0
    else:
        texture["source"] = 0
    document = {
        "asset": {"version": "2.0", "generator": "Glider A1.8 KTX2 fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
            "indices": 3,
            "material": 0,
        }]}],
        "materials": [material],
        "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071}],
        "textures": [texture],
        "images": images,
        "extensionsUsed": extensions_used,
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
        "buffers": [{
            "byteLength": len(binary),
            "uri": "data:application/octet-stream;base64," + base64.b64encode(binary).decode("ascii"),
        }],
    }
    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(document, indent=1), encoding="utf-8")
    return path


def albedo_fixture(name, ktx2_name, reference_png, color):
    (OUT / reference_png).write_bytes(make_png(64, 64, *color))
    (OUT / "fallback.png").write_bytes(make_png(64, 64, 255, 0, 255))
    material = {
        "name": "Albedo",
        "pbrMetallicRoughness": {
            "baseColorTexture": {"index": 0},
            "metallicFactor": 0.0,
            "roughnessFactor": 0.5,
        },
    }
    return quad_document(
        name,
        material,
        [{"uri": "fallback.png"}, {"uri": ktx2_name}],
        1,
        [EXTENSION],
    )


def normal_fixture(name, image_uri, convention=None, dds=True):
    material = {
        "name": "Normal",
        "pbrMetallicRoughness": {
            "baseColorFactor": [0.7, 0.7, 0.7, 1.0],
            "metallicFactor": 0.0,
            "roughnessFactor": 0.45,
        },
        "normalTexture": {"index": 0, "scale": 1.0},
    }
    extensions = [NORMAL_EXTENSION] if convention is not None else []
    if dds:
        images = [{"uri": "fallback.png"}, {"uri": image_uri}]
        extensions_used = [EXTENSION] + extensions
    else:
        images = [{"uri": image_uri}]
        extensions_used = extensions
    path = quad_document(name, material, images, 1 if dds else None, extensions_used)
    if convention is not None:
        document = json.loads(path.read_text(encoding="utf-8"))
        document["asset"]["extensions"] = {
            NORMAL_EXTENSION: {"tangent_space_convention": convention}
        }
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


def check_close(a, b, label, tolerance, allow_rounding=False):
    if same(a, b):
        return check(True, label)
    info = difference(a, b)
    if info is None:
        print(f"  INFO {label}: captures could not be decoded")
        return check(False, label)
    maximum, changed = info
    if maximum <= tolerance:
        print(f"  INFO {label}: maximum per-channel difference {maximum} over {changed} pixels")
        return check(True, f"{label} (maximum per-channel difference {maximum})")
    if allow_rounding and maximum <= 1:
        print(f"  INFO {label}: {maximum}-LSB rounding difference over {changed} pixels")
        return check(True, f"{label} ({maximum}-LSB rounding)")
    print(f"  INFO {label}: maximum per-channel difference {maximum} over {changed} pixels")
    return check(False, label)


def run_logged(exe, model, name, flags=()):
    png = OUT / f"{name}.png"
    png.unlink(missing_ok=True)
    result = subprocess.run(
        [str(exe), str(model)] + COMMON + list(flags)
        + ["--capture-viewport", str(png), "--capture-frame", "24",
           "--profile-report", str(OUT / f"{name}.json"), "--exit-after-profile"],
        cwd=REPO,
        capture_output=True,
        text=True,
        timeout=900,
    )
    return (png if png.exists() else None), (result.stdout + result.stderr), result.returncode


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if not EXE.exists():
        check(False, f"Glider executable not found at {EXE}")
        finish()

    # BC1/BC3 colours are the exact RGB565 values so the PNG reference is byte-identical.
    bc1_color = (132, 130, 132)
    bc3_color = (66, 194, 231)
    bc7_color = (129, 129, 129)

    (OUT / "bc1.PNG.KTX2").write_bytes(make_ktx2(133, 8, 8, [bc1_block(bc1_color)] * 4))
    (OUT / "bc3.PNG.KTX2").write_bytes(make_ktx2(137, 8, 8, [bc3_block(bc3_color)] * 4))
    (OUT / "bc7.PNG.KTX2").write_bytes(make_ktx2(145, 8, 8, [bc7_mode6_block(bc7_color + (255,))] * 4))

    bc1_fixture = albedo_fixture("albedo_bc1", "bc1.PNG.KTX2", "bc1_ref.png", bc1_color)
    bc3_fixture = albedo_fixture("albedo_bc3", "bc3.PNG.KTX2", "bc3_ref.png", bc3_color)
    bc7_fixture = albedo_fixture("albedo_bc7", "bc7.PNG.KTX2", "bc7_ref.png", bc7_color)

    albedo_flags = ["--select-node", "Albedo"]
    bc1_capture = render(EXE, bc1_fixture, "albedo_bc1", albedo_flags, OUT)
    bc3_capture = render(EXE, bc3_fixture, "albedo_bc3", albedo_flags, OUT)
    bc7_capture = render(EXE, bc7_fixture, "albedo_bc7", albedo_flags, OUT)

    # Normal map reference PNGs: X=0.6, Y=0.35, Z=sqrt(1-X^2-Y^2). The DirectX
    # PNG stores the inverted green so the A1.2 flip recovers the same +Y.
    nx, ny = 0.6, 0.35
    nz = math.sqrt(max(0.0, 1.0 - nx * nx - ny * ny))

    def normal_png(invert_green):
        g = (1.0 - ny) / 2.0 if invert_green else (ny + 1.0) / 2.0
        return make_png(64, 64, round((nx + 1.0) * 0.5 * 255.0),
                        round(g * 255.0), round((nz + 1.0) * 0.5 * 255.0))

    (OUT / "normal_opengl.png").write_bytes(normal_png(False))
    (OUT / "normal_directx.png").write_bytes(normal_png(True))
    (OUT / "normal_bc5_opengl.PNG.KTX2").write_bytes(
        make_ktx2(142, 8, 8, [bc5_block(nx, ny)] * 4))
    (OUT / "normal_bc5_directx.PNG.KTX2").write_bytes(
        make_ktx2(142, 8, 8, [bc5_block(nx, -ny)] * 4))

    normal_bc5_opengl = normal_fixture("normal_bc5_opengl", "normal_bc5_opengl.PNG.KTX2")
    normal_png_opengl = normal_fixture("normal_png_opengl", "normal_opengl.png", dds=False)
    normal_bc5_directx = normal_fixture("normal_bc5_directx", "normal_bc5_directx.PNG.KTX2", "DirectX")
    normal_png_directx = normal_fixture("normal_png_directx", "normal_directx.png", "DirectX", dds=False)
    normal_flags = ["--select-node", "Normal"]

    bc5_opengl_capture = render(EXE, normal_bc5_opengl, "normal_bc5_opengl", normal_flags, OUT)
    png_opengl_capture = render(EXE, normal_png_opengl, "normal_png_opengl", normal_flags, OUT)
    bc5_directx_capture = render(EXE, normal_bc5_directx, "normal_bc5_directx", normal_flags, OUT)
    png_directx_capture = render(EXE, normal_png_directx, "normal_png_directx", normal_flags, OUT)
    bc5_directx_disabled = render(EXE, normal_bc5_directx, "normal_bc5_directx_disabled",
                                  normal_flags + ["--asobo-disable", NORMAL_EXTENSION], OUT)

    # Unsupported vkFormat: a header-only KTX2 (BC4_UNORM, vkFormat 139).
    unsupported_ktx2 = make_unsupported_ktx2("unsupported.PNG.KTX2")
    unsupported_fixture = albedo_fixture("albedo_unsupported", unsupported_ktx2.name, "unsupported_ref.png", (255, 0, 255))
    unsupported_capture, unsupported_log, unsupported_exit = run_logged(EXE, unsupported_fixture, "albedo_unsupported", albedo_flags)

    # Plain PNG quad: the KTX2 code must not change the pre-existing path.
    (OUT / "plain.png").write_bytes(make_png(64, 64, 64, 128, 192))
    plain_document = quad_document(
        "albedo_plain_core",
        {"name": "Plain", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                   "metallicFactor": 0.0, "roughnessFactor": 0.5}},
        [{"uri": "plain.png"}],
        None,
        [],
    )
    plain_flags = ["--select-node", "Plain"]
    plain_current = render(EXE, plain_document, "plain_current", plain_flags, OUT)
    plain_baseline = None
    if BASELINE.exists():
        plain_baseline = render(BASELINE, plain_document, "plain_baseline", plain_flags, OUT)
    else:
        print(f"  SKIP baseline {BASELINE} not found")

    captures = [bc1_capture, bc3_capture, bc7_capture,
                bc5_opengl_capture, png_opengl_capture,
                bc5_directx_capture, png_directx_capture, bc5_directx_disabled,
                unsupported_capture, plain_current]
    if not all(captures):
        check(False, "all renders produced a viewport capture")
        finish()

    def albedo_reference(name, png):
        return quad_document(
            name,
            {"name": "AlbedoRef", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                           "metallicFactor": 0.0, "roughnessFactor": 0.5}},
            [{"uri": png}],
            None,
            [],
        )

    print("[BCn KTX2 albedo]")
    bc1_png_capture = render(EXE, albedo_reference("albedo_bc1_png", "bc1_ref.png"),
                             "albedo_bc1_png", ["--select-node", "AlbedoRef"], OUT)
    bc3_png_capture = render(EXE, albedo_reference("albedo_bc3_png", "bc3_ref.png"),
                             "albedo_bc3_png", ["--select-node", "AlbedoRef"], OUT)
    bc7_png_capture = render(EXE, albedo_reference("albedo_bc7_png", "bc7_ref.png"),
                             "albedo_bc7_png", ["--select-node", "AlbedoRef"], OUT)
    if all((bc1_png_capture, bc3_png_capture, bc7_png_capture)):
        check_close(bc1_capture, bc1_png_capture, "BC1 albedo matches the reference PNG (sRGB)", 1, True)
        check_close(bc3_capture, bc3_png_capture, "BC3 albedo matches the reference PNG (sRGB)", 1, True)
        check_close(bc7_capture, bc7_png_capture, "BC7 albedo matches the reference PNG (sRGB)", 1, True)
        check(not same(bc1_capture, bc3_png_capture), "BC1 albedo differs from the BC3 reference colour")
    else:
        check(False, "albedo reference renders produced a viewport capture")

    print("[BC5 SNORM normal map]")
    check_close(bc5_opengl_capture, png_opengl_capture,
                "BC5 SNORM normal map matches the PNG normal map (OpenGL)", 8)
    check_close(bc5_directx_capture, png_directx_capture,
                "BC5 SNORM normal map matches the PNG normal map (DirectX)", 8)
    check(not same(bc5_directx_capture, bc5_directx_disabled),
          "the DirectX green flip changes the signed BC5 result")

    print("[unsupported vkFormat]")
    check(unsupported_exit == 0, "unsupported vkFormat exits without crashing")
    check(unsupported_capture is not None, "unsupported vkFormat still renders a frame")
    check("unsupported vkFormat" in unsupported_log and "139" in unsupported_log
          and "unsupported.PNG.KTX2" in unsupported_log,
          "unsupported vkFormat logs the file and format")

    print("[file without KTX2 textures]")
    if plain_baseline is not None:
        check(same(plain_current, plain_baseline),
              "plain PNG model is byte-identical to the baseline binary")
    else:
        print("  SKIP baseline comparison")

    finish()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        check(False, str(error))
        finish()
