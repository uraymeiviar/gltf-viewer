# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Writes a glTF with a few very large PNG textures, to measure peak memory while images
decode in parallel (P6.2).

<count> RGBA PNGs of <size>x<size> (gradients with hashed low-bit noise), one per quad.
Needs numpy for speed.

Usage: generate_big_texture_fixture.py <out_dir> [count] [size]
"""

import json
import struct
import sys
import zlib
from pathlib import Path

import numpy as np


def png(size, seed):
    y = np.arange(size, dtype=np.uint32)[:, None]
    x = np.arange(size, dtype=np.uint32)[None, :]
    noise = ((x * np.uint32(2654435761) + y * np.uint32(2246822519) + np.uint32((seed * 3266489917) & 0xFFFFFFFF)) >> np.uint32(29)).astype(np.uint8)
    img = np.empty((size, size, 4), dtype=np.uint8)
    img[..., 0] = ((y * 255 // (size - 1)).astype(np.uint8) + noise)
    img[..., 1] = ((x * 255 // (size - 1)).astype(np.uint8) + noise)
    img[..., 2] = ((x ^ y) + seed * 37).astype(np.uint8)
    img[..., 3] = 255
    raw = np.concatenate([np.zeros((size, 1), dtype=np.uint8), img.reshape(size, size * 4)], axis=1).tobytes()

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 1)) + chunk(b"IEND", b""))


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "build/p6/fixtures/big_textures")
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 8192
    out.mkdir(parents=True, exist_ok=True)
    positions = struct.pack("<12f", -0.5, -0.5, 0, 0.5, -0.5, 0, 0.5, 0.5, 0, -0.5, 0.5, 0)
    uvs = struct.pack("<8f", 0, 1, 1, 1, 1, 0, 0, 0)
    indices = struct.pack("<6H", 0, 1, 2, 0, 2, 3)
    blob = positions + uvs + indices
    (out / "quad.bin").write_bytes(blob)
    for i in range(count):
        (out / f"big_{i}.png").write_bytes(png(size, i))
    gltf = {
        "asset": {"version": "2.0", "generator": "generate_big_texture_fixture.py"},
        "scene": 0, "scenes": [{"nodes": list(range(count))}],
        "nodes": [{"mesh": i, "translation": [i * 1.05, 0, 0]} for i in range(count)],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1}, "indices": 2, "material": i}]} for i in range(count)],
        "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": i}, "metallicFactor": 0.0}} for i in range(count)],
        "textures": [{"source": i, "sampler": 0} for i in range(count)],
        "images": [{"uri": f"big_{i}.png"} for i in range(count)],
        "samplers": [{"magFilter": 9729, "minFilter": 9987}],
        "buffers": [{"uri": "quad.bin", "byteLength": len(blob)}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 48}, {"buffer": 0, "byteOffset": 48, "byteLength": 32},
                        {"buffer": 0, "byteOffset": 80, "byteLength": 12}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3", "min": [-0.5, -0.5, 0], "max": [0.5, 0.5, 0]},
                      {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC2"},
                      {"bufferView": 2, "componentType": 5123, "count": 6, "type": "SCALAR"}],
    }
    (out / "big.gltf").write_text(json.dumps(gltf), encoding="utf-8")
    print(f"{out}: {count} PNGs {size}^2")


if __name__ == "__main__":
    main()
