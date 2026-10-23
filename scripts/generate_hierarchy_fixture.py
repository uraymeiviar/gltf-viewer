# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Generates resources/test_models/hierarchy_animation_fixture.gltf (P4.7).

Scene:
  AnimatedRoot (rotates about Y, 0..2 s)
    ScaledChild   (non-uniform scale 1.6 x 0.5 x 1.0, mesh, translated)
      GrandChild  (mesh, translated; inherits the non-uniform scale)
    RootCamera    (perspective camera looking at the origin; moves with the root)
    RootLight     (KHR_lights_punctual point light; moves with the root)
  StaticSibling   (mesh, never animated; carries an unknown vendor extension)
    StaticChild   (mesh)

KHR_materials_variants: variant "Alt" maps CubeA (ScaledChild, StaticChild) to
the Teal material, for material-variant transition tests.

Covers animated ancestors, attached camera and light, non-uniform scales and a
static subtree next to an animated one. Self-contained (base64 buffer); not
packed into Glider.exe.
"""

import base64
import json
import math
import struct
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "resources" / "test_models" / "hierarchy_animation_fixture.gltf"


def cube():
    p = []
    n = []
    idx = []
    faces = [((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
             ((0, 1, 0), (0, 0, 1), (1, 0, 0)), ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
             ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (0, 1, 0), (1, 0, 0))]
    for normal, u, v in faces:
        base = len(p)
        for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p.append(tuple(0.5 * (normal[i] + a * u[i] + b * v[i]) for i in range(3)))
            n.append(normal)
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    return p, n, idx


positions, normals, indices = cube()
times = [0.0, 1.0, 2.0]
rotations = []
for t in times:
    half = math.radians(60.0 * t) / 2.0
    rotations.append((0.0, math.sin(half), 0.0, math.cos(half)))

chunks = []
chunks.append(b"".join(struct.pack("<3f", *v) for v in positions))
chunks.append(b"".join(struct.pack("<3f", *v) for v in normals))
chunks.append(struct.pack(f"<{len(indices)}H", *indices))
chunks.append(struct.pack("<3f", *times))
chunks.append(b"".join(struct.pack("<4f", *r) for r in rotations))

buffer = b""
views = []
for i, data in enumerate(chunks):
    view = {"buffer": 0, "byteOffset": len(buffer), "byteLength": len(data)}
    if i < 2:
        view["target"] = 34962
    elif i == 2:
        view["target"] = 34963
    views.append(view)
    buffer += data + bytes(-len(data) % 4)

gltf = {
    "asset": {"version": "2.0", "generator": "Glider P4.7 hierarchy fixture generator"},
    "extensionsUsed": ["KHR_lights_punctual", "KHR_materials_variants", "EXT_glider_test"],
    "extensions": {"KHR_lights_punctual": {"lights": [
        {"name": "RootPointLight", "type": "point", "color": [1.0, 0.8, 0.6], "intensity": 40.0}]},
        "KHR_materials_variants": {"variants": [{"name": "Alt"}]}},
    "scene": 0,
    "scenes": [{"nodes": [0, 5]}],
    "nodes": [
        {"name": "AnimatedRoot", "children": [1, 3, 4]},
        {"name": "ScaledChild", "mesh": 0, "translation": [1.5, 0.0, 0.0], "scale": [1.6, 0.5, 1.0], "children": [2]},
        {"name": "GrandChild", "mesh": 1, "translation": [0.0, 1.5, 0.0]},
        {"name": "RootCamera", "camera": 0, "translation": [0.0, 1.5, 7.0]},
        {"name": "RootLight", "translation": [0.0, 2.0, 2.0], "extensions": {"KHR_lights_punctual": {"light": 0}}},
        {"name": "StaticSibling", "mesh": 2, "translation": [-2.0, 0.0, 0.0], "children": [6],
         "extensions": {"EXT_glider_test": {"enabled": True, "weight": 0.5}}},
        {"name": "StaticChild", "mesh": 0, "translation": [0.0, 1.2, 0.0], "scale": [0.5, 0.5, 0.5]},
    ],
    "cameras": [{"type": "perspective", "perspective": {"yfov": 0.8, "znear": 0.1, "zfar": 100.0}}],
    "meshes": [
        {"name": "CubeA", "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 0,
                                          "extensions": {"KHR_materials_variants": {"mappings": [{"material": 1, "variants": [0]}]}}}]},
        {"name": "CubeB", "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 1}]},
        {"name": "CubeC", "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 2}]},
    ],
    "materials": [
        {"name": "Orange", "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.5, 0.1, 1.0], "metallicFactor": 0.0}},
        {"name": "Teal", "pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.7, 0.7, 1.0], "metallicFactor": 0.0}},
        {"name": "Grey", "pbrMetallicRoughness": {"baseColorFactor": [0.6, 0.6, 0.6, 1.0], "metallicFactor": 0.3}},
    ],
    "animations": [{"name": "RootSpin",
                    "samplers": [{"input": 3, "output": 4, "interpolation": "LINEAR"}],
                    "channels": [{"sampler": 0, "target": {"node": 0, "path": "rotation"}}]}],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3",
         "min": [-0.5, -0.5, -0.5], "max": [0.5, 0.5, 0.5]},
        {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
        {"bufferView": 2, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        {"bufferView": 3, "componentType": 5126, "count": 3, "type": "SCALAR", "min": [0.0], "max": [2.0]},
        {"bufferView": 4, "componentType": 5126, "count": 3, "type": "VEC4"},
    ],
    "bufferViews": views,
    "buffers": [{"byteLength": len(buffer),
                 "uri": "data:application/octet-stream;base64," + base64.b64encode(buffer).decode("ascii")}],
}

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(gltf, indent=2) + "\n", encoding="utf-8")
print(f"wrote {OUT}")
