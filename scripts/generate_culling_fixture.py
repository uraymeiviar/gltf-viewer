# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Writes a wide scene for frustum-culling tests (P5.5).

A 12x12 field of cubes over [-30, 30] in x and z, with mixed transforms: negative
scale on one or three axes, non-uniform scale, and rotations. Four parent groups
with mirrored, non-uniformly scaled and rotated parents hold four cubes each, and
one 80-unit bar crosses the whole field. With the camera inside the field, cubes
lie behind it, beside it, and across its near plane.

Usage: generate_culling_fixture.py <out.gltf>
"""

import base64
import json
import math
import struct
import sys
from pathlib import Path


def cube():
    faces = [((1, 0, 0), (0, 1, 0)), ((-1, 0, 0), (0, 1, 0)), ((0, 1, 0), (0, 0, 1)),
             ((0, -1, 0), (0, 0, 1)), ((0, 0, 1), (0, 1, 0)), ((0, 0, -1), (0, 1, 0))]
    pos, nor, idx = [], [], []
    for n, up in faces:
        right = (up[1] * n[2] - up[2] * n[1], up[2] * n[0] - up[0] * n[2], up[0] * n[1] - up[1] * n[0])
        base = len(pos) // 3
        for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            pos += [0.5 * (n[k] + su * right[k] + sv * up[k]) for k in range(3)]
            nor += list(n)
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    return pos, nor, idx


def quat_y(deg):
    r = math.radians(deg) / 2
    return [0.0, math.sin(r), 0.0, math.cos(r)]


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "build/p5.5/fixtures/culling.gltf")
    pos, nor, idx = cube()
    blob = struct.pack(f"<{len(pos)}f", *pos) + struct.pack(f"<{len(nor)}f", *nor) + struct.pack(f"<{len(idx)}H", *idx)
    colors = [(0.9, 0.2, 0.2), (0.2, 0.8, 0.3), (0.2, 0.4, 0.9), (0.9, 0.8, 0.2), (0.8, 0.3, 0.8), (0.3, 0.8, 0.8)]
    materials = [{"name": f"M{i}", "pbrMetallicRoughness": {"baseColorFactor": [*c, 1.0], "metallicFactor": 0.0,
                                                            "roughnessFactor": 0.6}} for i, c in enumerate(colors)]
    meshes = [{"name": f"Cube{i}", "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": i}]}
              for i in range(len(colors))]
    scales = [[1, 1, 1], [-1, 1, 1], [1, -2, 0.5], [3, 0.3, 1], [-0.5, -0.5, -0.5], [0.4, 2.5, 0.4]]
    nodes, roots = [], []
    n = 12
    for i in range(n):
        for j in range(n):
            k = i * n + j
            nodes.append({"name": f"C_{i}_{j}", "mesh": k % len(meshes),
                          "translation": [-30 + 60 * i / (n - 1), 0.5, -30 + 60 * j / (n - 1)],
                          "rotation": quat_y(17 * k), "scale": scales[k % len(scales)]})
            roots.append(len(nodes) - 1)
    parents = [([2, -1, 0.5], 30, [8, 2, 8]), ([-1, 1.5, 1], -45, [-8, 2, 8]),
               ([0.5, 0.5, -3], 90, [8, 2, -8]), ([-2, -2, -2], 10, [-8, 2, -8])]
    for p, (scale, yaw, t) in enumerate(parents):
        children = []
        for c in range(4):
            nodes.append({"name": f"G{p}_{c}", "mesh": (p + c) % len(meshes),
                          "translation": [1.5 * (c % 2) - 0.75, 0.0, 1.5 * (c // 2) - 0.75],
                          "rotation": quat_y(23 * c), "scale": [0.6, 0.6, 0.6]})
            children.append(len(nodes) - 1)
        nodes.append({"name": f"Group{p}", "translation": t, "rotation": quat_y(yaw), "scale": scale, "children": children})
        roots.append(len(nodes) - 1)
    nodes.append({"name": "Bar", "mesh": 0, "translation": [0, 0.2, 0], "rotation": quat_y(35), "scale": [80, 0.1, 0.1]})
    roots.append(len(nodes) - 1)
    gltf = {
        "asset": {"version": "2.0", "generator": "generate_culling_fixture.py"},
        "scene": 0, "scenes": [{"nodes": roots}], "nodes": nodes, "meshes": meshes, "materials": materials,
        "buffers": [{"byteLength": len(blob), "uri": "data:application/octet-stream;base64," + base64.b64encode(blob).decode()}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": len(pos) * 4, "target": 34962},
                        {"buffer": 0, "byteOffset": len(pos) * 4, "byteLength": len(nor) * 4, "target": 34962},
                        {"buffer": 0, "byteOffset": (len(pos) + len(nor)) * 4, "byteLength": len(idx) * 2, "target": 34963}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": len(pos) // 3, "type": "VEC3",
                       "min": [-0.5, -0.5, -0.5], "max": [0.5, 0.5, 0.5]},
                      {"bufferView": 1, "componentType": 5126, "count": len(nor) // 3, "type": "VEC3"},
                      {"bufferView": 2, "componentType": 5123, "count": len(idx), "type": "SCALAR"}],
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(gltf), encoding="utf-8")


if __name__ == "__main__":
    main()
