#!/usr/bin/env python3
"""Builds the A1.11 mirrored-UV tangent fixture under build/review.

Three quads share one bump normal map:

  Control_NoTangent       UV u increases with +X, no TANGENT
  Mirrored_NoTangent      UV u decreases with +X, no TANGENT
  Mirrored_WithTangent    the same mirrored UVs with T=-X, w=-1 (MikkTSpace)

The mirrored/no-TANGENT quad exercises the shader's screen-derivative frame. Before the
A1.11 fallback it produced b = cross(n, t) = -Y while the control produced +Y, so the
bump shading was inverted. After the fallback both no-TANGENT quads get MikkTSpace data
and all three bitangent views agree. The light is a point light in front/above so the
raised bumps are visible from the same direction on every quad.

Usage:
  python scripts/make_tangent_fixture.py
  build/bin/Release/Glider.exe build/review/tangent_fixture.gltf --isolated \
      --window-size 1200x800 --camera-target 0.1,0.5,0 --camera-distance 3.2 \
      --camera-yaw 0 --camera-pitch 0 --exposure -4 \
      --dump-frame build/review/tangent_fixture.png --exit-after-dump
"""

import json
import math
import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "build" / "review"
WIDTH = HEIGHT = 256
BUMPS = 4


def png_chunk(tag: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + tag
        + payload
        + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
    )


def write_normal_map(path: Path) -> None:
    # A tileable 4x4 grid of hemispherical bumps. The PNG rows go top to bottom, which is
    # how the viewer uploads them; the quads use v = y so the pattern is upright.
    heights = [[0.0] * WIDTH for _ in range(HEIGHT)]
    cells = [(x + 0.5) / BUMPS for x in range(BUMPS)]
    for y in range(HEIGHT):
        v = (y + 0.5) / HEIGHT * BUMPS
        for x in range(WIDTH):
            u = (x + 0.5) / WIDTH * BUMPS
            h = 0.0
            for cy in cells:
                for cx in cells:
                    dx = u - cx
                    dy = v - cy
                    h += math.exp(-(dx * dx + dy * dy) / 0.22)
            heights[y][x] = h * 0.12

    rows = []
    for y in range(HEIGHT):
        row = bytearray()
        for x in range(WIDTH):
            # Periodic central differences, scaled to texel units.
            dx = (heights[y][(x + 1) % WIDTH] - heights[y][(x - 1) % WIDTH]) * 0.5 * WIDTH / BUMPS
            dy = (heights[(y + 1) % HEIGHT][x] - heights[(y - 1) % HEIGHT][x]) * 0.5 * HEIGHT / BUMPS
            nx, ny, nz = -dx, -dy, 1.0
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            nx, ny, nz = nx / length, ny / length, nz / length
            row.extend(
                (
                    max(0, min(255, round((nx * 0.5 + 0.5) * 255))),
                    max(0, min(255, round((ny * 0.5 + 0.5) * 255))),
                    max(0, min(255, round((nz * 0.5 + 0.5) * 255))),
                    255,
                )
            )
        rows.append(b"\x00" + bytes(row))

    raw = b"".join(rows)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(raw, 6))
        + png_chunk(b"IEND", b"")
    )
    path.write_bytes(png)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    write_normal_map(OUT / "tangent_fixture_normal.png")

    blob = bytearray()
    accessors = []
    buffer_views = []

    def add_accessor(data: bytes, component_type: int, attribute_type: str, components: int, target: int) -> int:
        while len(blob) % 4:
            blob.append(0)
        offset = len(blob)
        blob.extend(data)
        buffer_views.append(
            {"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target}
        )
        component_size = 2 if component_type == 5123 else 4
        accessors.append(
            {
                "bufferView": len(buffer_views) - 1,
                "componentType": component_type,
                "count": len(data) // (components * component_size),
                "type": attribute_type,
            }
        )
        return len(accessors) - 1

    meshes = []
    nodes = []
    for index, (x0, mirrored, with_tangent) in enumerate(
        [(-1.8, False, False), (-0.6, True, False), (0.6, True, True)]
    ):
        xs = [x0, x0 + 1.0, x0 + 1.0, x0]
        ys = [0.0, 0.0, 1.0, 1.0]
        us = [1.0, 0.0, 0.0, 1.0] if mirrored else [0.0, 1.0, 1.0, 0.0]
        # v increases with +Y, so a non-mirrored MikkTSpace frame is (T=+X, B=+Y, w=+1)
        # and the mirrored U island is (T=-X, B=+Y, w=-1).
        vs = [0.0, 0.0, 1.0, 1.0]
        indices = [0, 1, 2, 0, 2, 3]

        attributes = {
            "POSITION": add_accessor(
                b"".join(struct.pack("<3f", xs[i], ys[i], 0.0) for i in range(4)), 5126, "VEC3", 3, 34962
            ),
            "NORMAL": add_accessor(
                b"".join(struct.pack("<3f", 0.0, 0.0, 1.0) for _ in range(4)), 5126, "VEC3", 3, 34962
            ),
            "TEXCOORD_0": add_accessor(
                b"".join(struct.pack("<2f", us[i], vs[i]) for i in range(4)), 5126, "VEC2", 2, 34962
            ),
        }
        if with_tangent:
            attributes["TANGENT"] = add_accessor(
                b"".join(struct.pack("<4f", -1.0, 0.0, 0.0, -1.0) for _ in range(4)),
                5126,
                "VEC4",
                4,
                34962,
            )
        primitive = {
            "attributes": attributes,
            "indices": add_accessor(
                b"".join(struct.pack("<H", i) for i in indices), 5123, "SCALAR", 1, 34963
            ),
            "material": 0,
        }
        meshes.append({"name": f"Quad{index}", "primitives": [primitive]})
        nodes.append(
            {
                "name": ["Control_NoTangent", "Mirrored_NoTangent", "Mirrored_WithTangent"][index],
                "mesh": index,
            }
        )

    gltf = {
        "asset": {"version": "2.0", "generator": "Glider A1.11 tangent fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0, 1, 2, 3]}],
        "nodes": nodes
        + [
            {
                "name": "KeyLight",
                "translation": [0.1, 1.5, 4.0],
                "extensions": {"KHR_lights_punctual": {"light": 0}},
            }
        ],
        "meshes": meshes,
        "materials": [
            {
                "name": "Bump",
                "pbrMetallicRoughness": {
                    "baseColorFactor": [0.2, 0.2, 0.2, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 0.5,
                },
                "normalTexture": {"index": 0, "scale": 1.0},
            }
        ],
        "textures": [{"source": 0}],
        "images": [{"uri": "tangent_fixture_normal.png"}],
        "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071}],
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"uri": "tangent_fixture.bin", "byteLength": len(blob)}],
        "extensionsUsed": ["KHR_lights_punctual"],
        "extensions": {
            "KHR_lights_punctual": {
                "lights": [{"type": "point", "intensity": 300.0, "color": [1.0, 0.97, 0.92]}]
            }
        },
    }

    (OUT / "tangent_fixture.bin").write_bytes(blob)
    (OUT / "tangent_fixture.gltf").write_text(json.dumps(gltf, indent=1))
    print(f"wrote {OUT / 'tangent_fixture.gltf'} ({len(blob)} vertex bytes)")


if __name__ == "__main__":
    main()
