# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Writes a glTF whose many KHR_texture_basisu (KTX2/Basis) images are transcoded in
parallel on load (P6.2 thread-safety stress).

The three KTX2 images of the BarramundiFish sample are copied <count> times as separate
image files (tex_<i>.ktx2), each used as the base colour of its own quad in a grid, so the
loader transcodes <count> Basis images concurrently from the first moment.

Usage: generate_ktx2_stress_fixture.py <out_dir> [count]
"""

import json
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SOURCE = REPO / "OpenGL-Renderer/Tools/tools/operations/tests/resources/BarramundiFish/ktx2/BarramundiFish.mixed.glb"


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "build/p6/fixtures/ktx2_stress")
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 48
    out.mkdir(parents=True, exist_ok=True)
    data = SOURCE.read_bytes()
    json_len = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20 + json_len])
    bin_start = 20 + json_len + 8
    ktx = []
    for image in doc["images"]:
        view = doc["bufferViews"][image["bufferView"]]
        start = bin_start + view.get("byteOffset", 0)
        ktx.append(data[start:start + view["byteLength"]])

    positions = struct.pack("<12f", -0.5, -0.5, 0, 0.5, -0.5, 0, 0.5, 0.5, 0, -0.5, 0.5, 0)
    uvs = struct.pack("<8f", 0, 1, 1, 1, 1, 0, 0, 0)
    indices = struct.pack("<6H", 0, 1, 2, 0, 2, 3)
    blob = positions + uvs + indices
    (out / "quad.bin").write_bytes(blob)
    side = int(count ** 0.5 + 0.999)
    images, textures, materials, meshes, nodes = [], [], [], [], []
    for i in range(count):
        (out / f"tex_{i}.ktx2").write_bytes(ktx[i % len(ktx)])
        images.append({"uri": f"tex_{i}.ktx2", "mimeType": "image/ktx2"})
        textures.append({"sampler": 0, "extensions": {"KHR_texture_basisu": {"source": i}}})
        materials.append({"pbrMetallicRoughness": {"baseColorTexture": {"index": i}, "metallicFactor": 0.0},
                          "doubleSided": True})
        meshes.append({"primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1}, "indices": 2, "material": i}]})
        nodes.append({"name": f"Quad{i}", "mesh": i, "translation": [(i % side) * 1.05, -(i // side) * 1.05, 0]})
    gltf = {
        "asset": {"version": "2.0", "generator": "generate_ktx2_stress_fixture.py"},
        "extensionsUsed": ["KHR_texture_basisu"],
        "scene": 0, "scenes": [{"nodes": list(range(count))}], "nodes": nodes, "meshes": meshes,
        "materials": materials, "textures": textures, "images": images,
        "samplers": [{"magFilter": 9729, "minFilter": 9987}],
        "buffers": [{"uri": "quad.bin", "byteLength": len(blob)}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 48}, {"buffer": 0, "byteOffset": 48, "byteLength": 32},
                        {"buffer": 0, "byteOffset": 80, "byteLength": 12}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
                       "min": [-0.5, -0.5, 0], "max": [0.5, 0.5, 0]},
                      {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC2"},
                      {"bufferView": 2, "componentType": 5123, "count": 6, "type": "SCALAR"}],
    }
    (out / "stress.gltf").write_text(json.dumps(gltf), encoding="utf-8")
    print(f"{out}: {count} KTX2 images")


if __name__ == "__main__":
    main()
