# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Generates resources/test_models/pointer_animation_fixture.gltf (P4.3).

Two quads share one material. One animation clip drives:
  * /materials/0/pbrMetallicRoughness/baseColorFactor via KHR_animation_pointer
    (red -> green -> blue over 2 s),
  * /materials/0/emissiveFactor via KHR_animation_pointer,
  * the rotation of node 1 ("SpinQuad") about Z.
The file is self-contained (base64 buffer) and is not packed into Glider.exe.
"""

import base64
import json
import math
import struct
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "resources" / "test_models" / "pointer_animation_fixture.gltf"


def pack(fmt, values):
    return b"".join(struct.pack(fmt, *v) if isinstance(v, (list, tuple)) else struct.pack(fmt, v) for v in values)


def pad4(data):
    return data + b"\x00" * (-len(data) % 4)


positions = [(-0.5, -0.5, 0.0), (0.5, -0.5, 0.0), (0.5, 0.5, 0.0), (-0.5, 0.5, 0.0)]
normals = [(0.0, 0.0, 1.0)] * 4
indices = [0, 1, 2, 0, 2, 3]
times = [0.0, 1.0, 2.0]
base_colors = [(1.0, 0.0, 0.0, 1.0), (0.0, 1.0, 0.0, 1.0), (0.0, 0.0, 1.0, 1.0)]
emissive = [(0.0, 0.0, 0.0), (0.2, 0.2, 0.0), (0.0, 0.0, 0.0)]
rotations = []
for t in times:
    half = math.radians(45.0 * t) / 2.0
    rotations.append((0.0, 0.0, math.sin(half), math.cos(half)))

chunks = [
    ("pos", pack("<3f", positions), 5126, "VEC3", 4),
    ("nrm", pack("<3f", normals), 5126, "VEC3", 4),
    ("idx", pad4(pack("<H", indices)), 5123, "SCALAR", 6),
    ("time", pack("<f", times), 5126, "SCALAR", 3),
    ("color", pack("<4f", base_colors), 5126, "VEC4", 3),
    ("emissive", pack("<3f", emissive), 5126, "VEC3", 3),
    ("rot", pack("<4f", rotations), 5126, "VEC4", 3),
]

buffer = b""
buffer_views = []
accessors = []
for name, data, component, kind, count in chunks:
    view = {"buffer": 0, "byteOffset": len(buffer), "byteLength": len(data) if name != "idx" else len(indices) * 2}
    if name in ("pos", "nrm"):
        view["target"] = 34962
    elif name == "idx":
        view["target"] = 34963
    buffer_views.append(view)
    accessor = {"bufferView": len(buffer_views) - 1, "componentType": component, "count": count, "type": kind}
    if name == "pos":
        accessor["min"] = [-0.5, -0.5, 0.0]
        accessor["max"] = [0.5, 0.5, 0.0]
    if name == "time":
        accessor["min"] = [times[0]]
        accessor["max"] = [times[-1]]
    accessors.append(accessor)
    buffer += pad4(data)

ACC = {name: i for i, (name, *_rest) in enumerate(chunks)}


def pointer_channel(sampler, pointer):
    return {"sampler": sampler, "target": {"path": "pointer", "extensions": {"KHR_animation_pointer": {"pointer": pointer}}}}


gltf = {
    "asset": {"version": "2.0", "generator": "Glider P4.3 fixture generator"},
    "extensionsUsed": ["KHR_animation_pointer"],
    "scene": 0,
    "scenes": [{"nodes": [0, 1]}],
    "nodes": [
        {"name": "PointerQuad", "mesh": 0, "translation": [-0.6, 0.0, 0.0]},
        {"name": "SpinQuad", "mesh": 0, "translation": [0.6, 0.0, 0.0]},
    ],
    "meshes": [{"name": "Quad", "primitives": [{
        "attributes": {"POSITION": ACC["pos"], "NORMAL": ACC["nrm"]},
        "indices": ACC["idx"], "material": 0}]}],
    "materials": [{
        "name": "PointerMat",
        "doubleSided": True,
        "pbrMetallicRoughness": {"baseColorFactor": [1.0, 0.0, 0.0, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.6},
        "emissiveFactor": [0.0, 0.0, 0.0],
    }],
    "animations": [{
        "name": "PointerAndSpin",
        "samplers": [
            {"input": ACC["time"], "output": ACC["color"], "interpolation": "LINEAR"},
            {"input": ACC["time"], "output": ACC["emissive"], "interpolation": "LINEAR"},
            {"input": ACC["time"], "output": ACC["rot"], "interpolation": "LINEAR"},
        ],
        "channels": [
            pointer_channel(0, "/materials/0/pbrMetallicRoughness/baseColorFactor"),
            pointer_channel(1, "/materials/0/emissiveFactor"),
            {"sampler": 2, "target": {"node": 1, "path": "rotation"}},
        ],
    }],
    "accessors": accessors,
    "bufferViews": buffer_views,
    "buffers": [{"byteLength": len(buffer),
                 "uri": "data:application/octet-stream;base64," + base64.b64encode(buffer).decode("ascii")}],
}

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(gltf, indent=2) + "\n", encoding="utf-8")
print(f"wrote {OUT}")
