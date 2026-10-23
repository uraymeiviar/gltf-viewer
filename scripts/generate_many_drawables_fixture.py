# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Generates an S2-style many-drawables fixture (P4.6 / P4.7 measurements).

A GRID x GRID grid of nodes, each referencing one of four quad meshes (one per
material), so the renderer creates one drawable per node. Materials alternate
between opaque single-sided, opaque double-sided, alpha-blended and masked so
several passes and shader variants are exercised. The buffer is embedded so the
file is self-contained.

Usage: python scripts/generate_many_drawables_fixture.py <out.gltf> [grid] [anim]

anim: "none" (default); "leaf" (one grid node rotates, every other drawable is
static); "root" (all grid nodes are children of one rotating root node, so every
world transform changes each tick).
"""

import base64
import json
import math
import struct
import sys
from pathlib import Path


def build(grid, anim="none"):
    positions = [(-0.4, -0.4, 0.0), (0.4, -0.4, 0.0), (0.4, 0.4, 0.0), (-0.4, 0.4, 0.0)]
    normals = [(0.0, 0.0, 1.0)] * 4
    indices = [0, 1, 2, 0, 2, 3]
    data = b"".join(struct.pack("<3f", *p) for p in positions)
    data += b"".join(struct.pack("<3f", *n) for n in normals)
    data += struct.pack("<6H", *indices) + bytes(2)
    times = [0.0, 1.0, 2.0]
    rotations = []
    for t in times:
        half = math.radians(90.0 * t) / 2.0
        rotations.append((0.0, 0.0, math.sin(half), math.cos(half)))
    anim_offset = len(data)
    data += struct.pack("<3f", *times) + b"".join(struct.pack("<4f", *r) for r in rotations)

    materials = [
        {"name": "Opaque", "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.3, 0.2, 1.0], "metallicFactor": 0.0}},
        {"name": "OpaqueDoubleSided", "doubleSided": True,
         "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.7, 0.3, 1.0], "metallicFactor": 0.2}},
        {"name": "Blend", "alphaMode": "BLEND",
         "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.3, 0.9, 0.5], "metallicFactor": 0.0}},
        {"name": "Mask", "alphaMode": "MASK", "alphaCutoff": 0.4,
         "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.9, 0.2, 0.8], "metallicFactor": 0.5}},
    ]
    meshes = [{"name": f"Quad_{m['name']}", "primitives": [{
        "attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": i}]} for i, m in enumerate(materials)]
    nodes = []
    for y in range(grid):
        for x in range(grid):
            nodes.append({"name": f"N_{x}_{y}", "mesh": (x + y) % len(meshes),
                          "translation": [x - grid / 2.0, y - grid / 2.0, 0.0]})

    scene_nodes = list(range(len(nodes)))
    animations = []
    if anim == "root":
        nodes.append({"name": "Root", "children": scene_nodes})
        scene_nodes = [len(nodes) - 1]
    if anim in ("leaf", "root"):
        target = len(nodes) - 1 if anim == "root" else (grid // 2) * grid + grid // 2
        animations = [{"name": "Spin", "samplers": [{"input": 3, "output": 4, "interpolation": "LINEAR"}],
                       "channels": [{"sampler": 0, "target": {"node": target, "path": "rotation"}}]}]

    doc = {
        "asset": {"version": "2.0", "generator": "Glider P4.6/P4.7 many-drawables generator"},
        "scene": 0,
        "scenes": [{"nodes": scene_nodes}],
        "nodes": nodes,
        "meshes": meshes,
        "materials": materials,
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
             "min": [-0.4, -0.4, 0.0], "max": [0.4, 0.4, 0.0]},
            {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": 6, "type": "SCALAR"},
            {"bufferView": 3, "componentType": 5126, "count": 3, "type": "SCALAR", "min": [0.0], "max": [2.0]},
            {"bufferView": 4, "componentType": 5126, "count": 3, "type": "VEC4"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": 48, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": 96, "byteLength": 12, "target": 34963},
            {"buffer": 0, "byteOffset": anim_offset, "byteLength": 12},
            {"buffer": 0, "byteOffset": anim_offset + 12, "byteLength": 48},
        ],
        "buffers": [{"byteLength": len(data),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(data).decode("ascii")}],
    }
    if animations:
        doc["animations"] = animations
    return doc


if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "build/p4.6/fixtures/many_drawables.gltf")
    grid = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    anim = sys.argv[3] if len(sys.argv) > 3 else "none"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(build(grid, anim)), encoding="utf-8")
    print(f"wrote {out} ({grid * grid} grid nodes, anim={anim})")
