#!/usr/bin/env python3
"""
Generates an intentionally invalid glTF/GLB file with controlled violations
to test Gestaltor's Tier 1 (schema) and Tier 2 (semantic & buffer) validation.
"""

import struct
import json
import os

def create_invalid_glb(output_path):
    # Create simple binary buffer with 3 vertices (triangle)
    # Positions: 3 x (float, float, float) = 36 bytes
    # Normals: 2 x (float, float, float) = 24 bytes (MISMATCH: only 2 normals for 3 vertices!)
    # Indices: [0, 1, 5] (OUT OF RANGE: index 5 is >= vertex count 3!)
    positions = struct.pack("<9f",
        0.0, 1.0, 0.0,
        -1.0, -1.0, 0.0,
        1.0, -1.0, 0.0
    )
    normals = struct.pack("<6f",
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0
    ) # only 2 normals!
    indices = struct.pack("<3H", 0, 1, 5) # 5 is out of bounds (vertex count is 3)

    bin_data = positions + normals + indices
    # Pad to 4 bytes
    while len(bin_data) % 4 != 0:
        bin_data += b"\x00"

    gltf_doc = {
        "asset": {
            "version": "2.0",
            "generator": "InvalidGLBTestGenerator"
        },
        "scene": 0,
        "scenes": [
            {
                "name": "InvalidTestScene",
                "nodes": [0]
            }
        ],
        "nodes": [
            {
                "name": "CorruptedTriangleNode",
                "mesh": 0
            }
        ],
        "materials": [
            {
                "name": "OutOfRangeMaterial",
                "pbrMetallicRoughness": {
                    "baseColorFactor": [1.5, -0.2, 0.5, 1.0], # Warning: out of range [0, 1]
                    "roughnessFactor": 2.5                    # Warning: out of range [0, 1]
                }
            }
        ],
        "meshes": [
            {
                "name": "CorruptedMesh",
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": 0,
                            "NORMAL": 1 # Count mismatch!
                        },
                        "indices": 2, # Out-of-range index!
                        "material": 0,
                        "mode": 4 # Triangles
                    }
                ]
            }
        ],
        "accessors": [
            {
                # Accessor 0: POSITION
                "bufferView": 0,
                "byteOffset": 0,
                "componentType": 5126, # FLOAT
                "count": 3,
                "type": "VEC3",
                "min": [-1.0, -1.0, 0.0],
                "max": [1.0, 1.0, 0.0]
            },
            {
                # Accessor 1: NORMAL (count = 2, mismatched!)
                "bufferView": 1,
                "byteOffset": 0,
                "componentType": 5126, # FLOAT
                "count": 2, # MISMATCH!
                "type": "VEC3"
            },
            {
                # Accessor 2: INDICES (contains index 5 >= 3)
                "bufferView": 2,
                "byteOffset": 0,
                "componentType": 5123, # UNSIGNED_SHORT
                "count": 3,
                "type": "SCALAR"
            }
        ],
        "bufferViews": [
            {
                "buffer": 0,
                "byteOffset": 0,
                "byteLength": len(positions)
            },
            {
                "buffer": 0,
                "byteOffset": len(positions),
                "byteLength": len(normals)
            },
            {
                "buffer": 0,
                "byteOffset": len(positions) + len(normals),
                "byteLength": len(indices)
            }
        ],
        "buffers": [
            {
                "byteLength": len(bin_data)
            }
        ]
    }

    json_str = json.dumps(gltf_doc, indent=None)
    json_bytes = json_str.encode("utf-8")
    while len(json_bytes) % 4 != 0:
        json_bytes += b" "

    # GLB container
    total_length = 12 + (8 + len(json_bytes)) + (8 + len(bin_data))
    header = struct.pack("<4sII", b"glTF", 2, total_length)
    chunk0_hdr = struct.pack("<II", len(json_bytes), 0x4E4F534A)
    chunk1_hdr = struct.pack("<II", len(bin_data), 0x004E4942)

    with open(output_path, "wb") as f:
        f.write(header)
        f.write(chunk0_hdr)
        f.write(json_bytes)
        f.write(chunk1_hdr)
        f.write(bin_data)

    print(f"Created invalid test GLB: {output_path} ({total_length} bytes)")

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "invalid_test.glb")
    create_invalid_glb(out)
