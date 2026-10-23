# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Writes deterministic large load fixtures for Phase 6 (load pipeline) measurements.

Outputs in <out_dir>:
  load.glb                      one GLB: JSON + BIN chunk with the vertex data and embedded PNGs
  load.gltf, load.bin, tex_*.png  the same asset as text glTF with an external buffer and image files

The scene is a grid of <nodes> nodes, each with its own mesh (a subdivided quad with
POSITION/NORMAL/TEXCOORD_0 and indices), cycling through one material per texture, so
the JSON grows with the node count and the BIN chunk with the vertex count. Textures are
<size>x<size> RGBA PNGs: smooth gradients with low-amplitude hashed noise, so they compress
like photographs rather than flat colour or white noise.

Usage: generate_load_fixture.py <out_dir> [--nodes N] [--grid G] [--images N] [--size S]
"""

import argparse
import json
import struct
import zlib
from pathlib import Path


def png(width, height, seed):
    rows = bytearray()
    for y in range(height):
        row = bytearray(width * 4)
        base_r = (y * 255) // max(1, height - 1)
        h = (seed * 0x9E3779B1 + y * 0x85EBCA77) & 0xFFFFFFFF
        for x in range(width):
            h = (h * 1103515245 + 12345) & 0xFFFFFFFF
            n = (h >> 28) & 0x7
            i = x * 4
            row[i] = (base_r + n) & 0xFF
            row[i + 1] = ((x * 255) // max(1, width - 1) + n) & 0xFF
            row[i + 2] = ((seed * 37 + (x ^ y)) & 0xFF)
            row[i + 3] = 255
        rows += b"\x00" + row
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(rows), 6)) + chunk(b"IEND", b""))


def mesh_data(grid):
    pos, nor, uv, idx = [], [], [], []
    for j in range(grid + 1):
        for i in range(grid + 1):
            u, v = i / grid, j / grid
            pos += [u - 0.5, 0.0, v - 0.5]
            nor += [0.0, 1.0, 0.0]
            uv += [u, v]
    for j in range(grid):
        for i in range(grid):
            a = j * (grid + 1) + i
            idx += [a, a + grid + 1, a + 1, a + 1, a + grid + 1, a + grid + 2]
    return pos, nor, uv, idx


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("out_dir")
    parser.add_argument("--nodes", type=int, default=4000)
    parser.add_argument("--grid", type=int, default=24, help="quads per side of each mesh")
    parser.add_argument("--images", type=int, default=16)
    parser.add_argument("--size", type=int, default=1024)
    args = parser.parse_args()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    pos, nor, uv, idx = mesh_data(args.grid)
    vcount = len(pos) // 3
    blob = bytearray()
    views, accessors, meshes, nodes = [], [], [], []

    def add_view(data, target=None):
        while len(blob) % 4:
            blob.append(0)
        view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)}
        if target:
            view["target"] = target
        blob.extend(data)
        views.append(view)
        return len(views) - 1

    side = int(args.nodes ** 0.5) + 1
    for n in range(args.nodes):
        # Each mesh has its own vertex data (offset in y), so the BIN chunk scales with the node count.
        p = list(pos)
        for k in range(1, len(p), 3):
            p[k] = (n % 7) * 0.01
        a0 = len(accessors)
        accessors.append({"bufferView": add_view(struct.pack(f"<{len(p)}f", *p), 34962), "componentType": 5126, "count": vcount,
                          "type": "VEC3", "min": [-0.5, (n % 7) * 0.01, -0.5], "max": [0.5, (n % 7) * 0.01, 0.5]})
        accessors.append({"bufferView": add_view(struct.pack(f"<{len(nor)}f", *nor), 34962), "componentType": 5126,
                          "count": vcount, "type": "VEC3"})
        accessors.append({"bufferView": add_view(struct.pack(f"<{len(uv)}f", *uv), 34962), "componentType": 5126,
                          "count": vcount, "type": "VEC2"})
        accessors.append({"bufferView": add_view(struct.pack(f"<{len(idx)}H", *idx), 34963), "componentType": 5123,
                          "count": len(idx), "type": "SCALAR"})
        meshes.append({"name": f"Mesh{n}", "primitives": [{"attributes": {"POSITION": a0, "NORMAL": a0 + 1, "TEXCOORD_0": a0 + 2},
                                                           "indices": a0 + 3, "material": n % max(1, args.images)}]})
        nodes.append({"name": f"Node{n}", "mesh": n, "translation": [(n % side) * 1.1, 0.0, (n // side) * 1.1]})

    pngs = [png(args.size, args.size, i) for i in range(args.images)]
    materials = [{"name": f"Mat{i}", "pbrMetallicRoughness": {"baseColorTexture": {"index": i}, "metallicFactor": 0.0}}
                 for i in range(args.images)]
    textures = [{"source": i, "sampler": 0} for i in range(args.images)]
    base = {"asset": {"version": "2.0", "generator": "generate_load_fixture.py"}, "scene": 0,
            "scenes": [{"nodes": list(range(len(nodes)))}], "nodes": nodes, "meshes": meshes,
            "materials": materials, "textures": textures, "samplers": [{"magFilter": 9729, "minFilter": 9987}]}

    # Text glTF: external buffer and image files.
    gltf = dict(base)
    gltf["buffers"] = [{"uri": "load.bin", "byteLength": len(blob)}]
    gltf["bufferViews"] = views
    gltf["accessors"] = accessors
    gltf["images"] = [{"uri": f"tex_{i}.png"} for i in range(args.images)]
    (out / "load.bin").write_bytes(bytes(blob))
    for i, data in enumerate(pngs):
        (out / f"tex_{i}.png").write_bytes(data)
    (out / "load.gltf").write_text(json.dumps(gltf), encoding="utf-8")

    # GLB: images as buffer views after the vertex data.
    glb_views = list(views)
    images = []
    for data in pngs:
        images.append({"bufferView": add_view(data), "mimeType": "image/png"})
    glb_views = views
    glb = dict(base)
    glb["buffers"] = [{"byteLength": len(blob)}]
    glb["bufferViews"] = glb_views
    glb["accessors"] = accessors
    glb["images"] = images
    js = json.dumps(glb).encode("utf-8")
    js += b" " * ((4 - len(js) % 4) % 4)
    while len(blob) % 4:
        blob.append(0)
    total = 12 + 8 + len(js) + 8 + len(blob)
    data = (struct.pack("<4sII", b"glTF", 2, total) + struct.pack("<I4s", len(js), b"JSON") + js
            + struct.pack("<I4s", len(blob), b"BIN\x00") + bytes(blob))
    (out / "load.glb").write_bytes(data)
    print(f"{out}: {args.nodes} nodes, {args.images} images {args.size}^2, JSON {len(js) / 1e6:.1f} MB, "
          f"GLB {len(data) / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
