# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A1.8: MSFT_texture_dds validation harness.

Verifies MSFT_texture_dds on a glTF texture:
  - When declared in extensionsUsed and DDS is present, Glider renders the DDS image;
  - Tested with both uncompressed 32-bit RGBA8 and block-compressed BC1 (DXT1) DDS formats;
  - When declared but the DDS file is missing, Glider falls back to the core PNG source without crashing;
  - When MSFT_texture_dds is present on the texture but undeclared in extensionsUsed, it falls back to the core PNG;
  - Plain files without the extension render standard textures.
"""

import base64
import json
import struct
import zlib
from pathlib import Path

from validate_asobo_common import check, finish, render, same

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "texture_dds"
EXTENSION = "MSFT_texture_dds"


def make_png(width, height, r, g, b, a=255):
    """Creates a raw uncompressed 8-bit RGBA PNG image."""
    def _chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = bytes([r, g, b, a] * width)
    payload = b"".join(b"\x00" + raw for _ in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", header) +
            _chunk(b"IDAT", zlib.compress(payload, 9)) + _chunk(b"IEND", b""))


def make_dds_rgba(width, height, r, g, b, a=255):
    """Creates an uncompressed 32-bit RGBA8 DirectDraw Surface (DDS) file."""
    # DDS magic (4 bytes) + DDS_HEADER (124 bytes) = 128 bytes total
    hdr = struct.pack(
        "<4s7I11I8I5I",
        b"DDS ",
        124, 0x1007, height, width, width * 4, 0, 0,
        *([0] * 11),
        32, 0x41, 0, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000,
        0x1000, 0, 0, 0, 0
    )
    pixel_data = bytes([r, g, b, a] * (width * height))
    return hdr + pixel_data


def make_dds_bc1(width, height, rgb565):
    """Creates a block-compressed BC1 (DXT1) DirectDraw Surface (DDS) file."""
    num_blocks_x = (width + 3) // 4
    num_blocks_y = (height + 3) // 4
    hdr = struct.pack(
        "<4s7I11I8I5I",
        b"DDS ",
        124, 0x1007, height, width, num_blocks_x * num_blocks_y * 8, 0, 0,
        *([0] * 11),
        32, 0x04, int.from_bytes(b"DXT1", "little"), 0, 0, 0, 0, 0,
        0x1000, 0, 0, 0, 0
    )
    # 8 bytes per 4x4 block: color0 (uint16), color1 (uint16), 4x4 2-bit indices (uint32).
    # All pixels use index 0 (color0).
    block = struct.pack("<HHI", rgb565, 0, 0)
    pixel_data = block * (num_blocks_x * num_blocks_y)
    return hdr + pixel_data


def write_fixture(name, dds_filename=None, declare=True):
    """Writes a lit quad glTF model referencing fallback.png and optional DDS."""
    positions = ((-1.0, -1.0, 0.0), (1.0, -1.0, 0.0), (1.0, 1.0, 0.0), (-1.0, 1.0, 0.0))
    normals = ((0.0, 0.0, 1.0),) * 4
    uvs = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
    indices = (0, 1, 2, 0, 2, 3)
    binary = b"".join(struct.pack("<3f", *v) for v in positions)
    binary += b"".join(struct.pack("<3f", *v) for v in normals)
    binary += b"".join(struct.pack("<2f", *v) for v in uvs)
    binary += struct.pack("<6H", *indices)

    texture_def = {"sampler": 0, "source": 0}
    images_def = [{"uri": "fallback.png"}]

    if dds_filename is not None:
        texture_def["extensions"] = {EXTENSION: {"source": 1}}
        images_def.append({"uri": dds_filename})

    document = {
        "asset": {"version": "2.0", "generator": "Glider A1.8 texture_dds fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "TextureDdsQuad", "mesh": 0}],
        "meshes": [{
            "primitives": [{
                "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                "indices": 3,
                "material": 0
            }]
        }],
        "materials": [{
            "name": "DdsMaterial",
            "pbrMetallicRoughness": {
                "baseColorTexture": {"index": 0},
                "metallicFactor": 0.0,
                "roughnessFactor": 0.5
            }
        }],
        "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071}],
        "textures": [texture_def],
        "images": images_def,
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
            "uri": "data:application/octet-stream;base64," + base64.b64encode(binary).decode("ascii")
        }],
        "extensionsUsed": [EXTENSION] if (declare and dds_filename is not None) else [],
    }

    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(document, indent=1), encoding="utf-8")
    return path


def write_reference(name, image_filename):
    """Writes a reference quad model using only image_filename without any extensions."""
    positions = ((-1.0, -1.0, 0.0), (1.0, -1.0, 0.0), (1.0, 1.0, 0.0), (-1.0, 1.0, 0.0))
    normals = ((0.0, 0.0, 1.0),) * 4
    uvs = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
    indices = (0, 1, 2, 0, 2, 3)
    binary = b"".join(struct.pack("<3f", *v) for v in positions)
    binary += b"".join(struct.pack("<3f", *v) for v in normals)
    binary += b"".join(struct.pack("<2f", *v) for v in uvs)
    binary += struct.pack("<6H", *indices)

    document = {
        "asset": {"version": "2.0", "generator": "Glider A1.8 reference fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "TextureDdsQuad", "mesh": 0}],
        "meshes": [{
            "primitives": [{
                "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                "indices": 3,
                "material": 0
            }]
        }],
        "materials": [{
            "name": "RefMaterial",
            "pbrMetallicRoughness": {
                "baseColorTexture": {"index": 0},
                "metallicFactor": 0.0,
                "roughnessFactor": 0.5
            }
        }],
        "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071}],
        "textures": [{"sampler": 0, "source": 0}],
        "images": [{"uri": image_filename}],
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
            "uri": "data:application/octet-stream;base64," + base64.b64encode(binary).decode("ascii")
        }],
    }

    path = OUT / f"{name}.gltf"
    path.write_text(json.dumps(document, indent=1), encoding="utf-8")
    return path


def main():
    OUT.mkdir(parents=True, exist_ok=True)

    # 1. Generate image assets:
    # Core PNG: solid Blue (0, 0, 255)
    (OUT / "fallback.png").write_bytes(make_png(64, 64, 0, 0, 255))
    # DDS RGBA8: solid Green (0, 255, 0)
    (OUT / "green_rgba.dds").write_bytes(make_dds_rgba(64, 64, 0, 255, 0))
    # DDS BC1: solid Red (RGB565 0xF800 -> 255, 0, 0)
    (OUT / "red_bc1.dds").write_bytes(make_dds_bc1(64, 64, 0xF800))

    # 2. Reference models (plain glTF 2.0 without extensions):
    ref_png = write_reference("ref_png", "fallback.png")
    ref_dds_rgba = write_reference("ref_dds_rgba", "green_rgba.dds")
    ref_dds_bc1 = write_reference("ref_dds_bc1", "red_bc1.dds")

    # 3. Test fixtures:
    # Declared with RGBA8 DDS present -> should render green
    fixture_declared_rgba = write_fixture("declared_rgba", "green_rgba.dds", declare=True)
    # Declared with BC1 DDS present -> should render red
    fixture_declared_bc1 = write_fixture("declared_bc1", "red_bc1.dds", declare=True)
    # Declared with DDS file missing -> should fall back to blue PNG without crashing
    fixture_declared_missing = write_fixture("declared_missing", "missing_nonexistent.dds", declare=True)
    # Undeclared with RGBA8 DDS present -> should fall back to blue PNG
    fixture_undeclared = write_fixture("undeclared", "green_rgba.dds", declare=False)
    # Plain model without extension -> should render blue PNG
    fixture_plain = write_fixture("plain", dds_filename=None, declare=False)

    flags = ["--select-node", "TextureDdsQuad"]

    # Render references
    cap_ref_png = render(EXE, ref_png, "ref_png", flags, OUT)
    cap_ref_dds_rgba = render(EXE, ref_dds_rgba, "ref_dds_rgba", flags, OUT)
    cap_ref_dds_bc1 = render(EXE, ref_dds_bc1, "ref_dds_bc1", flags, OUT)

    # Render test fixtures
    cap_declared_rgba = render(EXE, fixture_declared_rgba, "declared_rgba", flags, OUT)
    cap_declared_bc1 = render(EXE, fixture_declared_bc1, "declared_bc1", flags, OUT)
    cap_declared_missing = render(EXE, fixture_declared_missing, "declared_missing", flags, OUT)
    cap_undeclared = render(EXE, fixture_undeclared, "undeclared", flags, OUT)
    cap_plain = render(EXE, fixture_plain, "plain", flags, OUT)

    renders = [cap_ref_png, cap_ref_dds_rgba, cap_ref_dds_bc1,
               cap_declared_rgba, cap_declared_bc1, cap_declared_missing,
               cap_undeclared, cap_plain]
    if not all(renders):
        check(False, "all renders produced a viewport capture")
        finish()

    print("[MSFT_texture_dds]")
    # Check 1: Declared and DDS present renders the DDS colour (not the PNG colour)
    check(same(cap_declared_rgba, cap_ref_dds_rgba),
          "declared MSFT_texture_dds with RGBA DDS renders the DDS image")
    check(not same(cap_declared_rgba, cap_ref_png),
          "declared MSFT_texture_dds with RGBA DDS differs from core fallback PNG")

    # Check 2: Block-compressed BC1 DDS decoder verification
    check(same(cap_declared_bc1, cap_ref_dds_bc1),
          "declared MSFT_texture_dds with BC1 (DXT1) DDS renders the block-compressed DDS image")
    check(not same(cap_declared_bc1, cap_ref_png),
          "declared MSFT_texture_dds with BC1 DDS differs from core fallback PNG")

    # Check 3: Declared but DDS file missing falls back to core PNG and does not crash
    check(same(cap_declared_missing, cap_ref_png),
          "declared MSFT_texture_dds with missing DDS file falls back to core PNG")

    # Check 4: Not declared in extensionsUsed falls back to core PNG
    check(same(cap_undeclared, cap_ref_png),
          "MSFT_texture_dds used but not declared in extensionsUsed falls back to core PNG")

    # Check 5: Plain model without extension is identical to reference PNG
    check(same(cap_plain, cap_ref_png),
          "plain model without MSFT_texture_dds matches reference PNG")

    finish()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        check(False, str(error))
        finish()
