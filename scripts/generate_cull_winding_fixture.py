import json
import struct
import base64
from pathlib import Path

def create_cull_winding_fixture(output_path):
    # Vertices: 4 vertices for a quad [-0.7, 0.7] x [-0.7, 0.7] at Z=0
    # Pos: float32 x 3
    # Normal: float32 x 3
    positions = [
        -0.7, -0.7, 0.0,
         0.7, -0.7, 0.0,
         0.7,  0.7, 0.0,
        -0.7,  0.7, 0.0
    ]
    normals_ccw = [
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0
    ]
    # Indices CCW: 0, 1, 2, 2, 3, 0
    indices_ccw = [0, 1, 2, 2, 3, 0]
    # Indices CW: 0, 2, 1, 0, 3, 2
    indices_cw = [0, 2, 1, 0, 3, 2]

    pos_bytes = struct.pack(f"<{len(positions)}f", *positions)
    norm_bytes = struct.pack(f"<{len(normals_ccw)}f", *normals_ccw)
    idx_ccw_bytes = struct.pack(f"<{len(indices_ccw)}H", *indices_ccw)
    idx_cw_bytes = struct.pack(f"<{len(indices_cw)}H", *indices_cw)

    # Pad to 4 bytes if necessary
    def pad4(b):
        pad = (4 - (len(b) % 4)) % 4
        return b + b"\x00" * pad

    idx_ccw_bytes = pad4(idx_ccw_bytes)
    idx_cw_bytes = pad4(idx_cw_bytes)

    # Buffer layout:
    # 0: pos_bytes (48 bytes)
    # 1: norm_bytes (48 bytes)
    # 2: idx_ccw_bytes (12 bytes, padded to 12)
    # 3: idx_cw_bytes (12 bytes, padded to 12)
    offset_pos = 0
    len_pos = len(pos_bytes)
    offset_norm = offset_pos + len_pos
    len_norm = len(norm_bytes)
    offset_idx_ccw = offset_norm + len_norm
    len_idx_ccw = len(idx_ccw_bytes)
    offset_idx_cw = offset_idx_ccw + len_idx_ccw
    len_idx_cw = len(idx_cw_bytes)

    total_buf = pos_bytes + norm_bytes + idx_ccw_bytes + idx_cw_bytes
    b64_buf = base64.b64encode(total_buf).decode("ascii")
    data_uri = f"data:application/octet-stream;base64,{b64_buf}"

    gltf = {
        "asset": { "version": "2.0", "generator": "Glider P3.4 Test Generator" },
        "scene": 0,
        "scenes": [ { "nodes": [ 0, 1, 2, 3 ] } ],
        "nodes": [
            { "name": "Node_FrontFacing_SingleSided", "mesh": 0, "translation": [-3.0, 0.0, 0.0] },
            { "name": "Node_BackFacing_SingleSided",  "mesh": 1, "translation": [-1.0, 0.0, 0.0] },
            { "name": "Node_BackFacing_DoubleSided",  "mesh": 2, "translation": [ 1.0, 0.0, 0.0] },
            { "name": "Node_Mirrored_SingleSided",    "mesh": 3, "translation": [ 3.0, 0.0, 0.0], "scale": [-1.0, 1.0, 1.0] }
        ],
        "meshes": [
            {
                "name": "Mesh_FrontFacing_SingleSided",
                "primitives": [{
                    "attributes": { "POSITION": 0, "NORMAL": 1 },
                    "indices": 2,
                    "material": 0
                }]
            },
            {
                "name": "Mesh_BackFacing_SingleSided",
                "primitives": [{
                    "attributes": { "POSITION": 0, "NORMAL": 1 },
                    "indices": 3,
                    "material": 1
                }]
            },
            {
                "name": "Mesh_BackFacing_DoubleSided",
                "primitives": [{
                    "attributes": { "POSITION": 0, "NORMAL": 1 },
                    "indices": 3,
                    "material": 2
                }]
            },
            {
                "name": "Mesh_Mirrored_SingleSided",
                "primitives": [{
                    "attributes": { "POSITION": 0, "NORMAL": 1 },
                    "indices": 2,
                    "material": 3
                }]
            }
        ],
        "materials": [
            {
                "name": "Mat_Green_SingleSided",
                "doubleSided": False,
                "pbrMetallicRoughness": {
                    "baseColorFactor": [0.0, 1.0, 0.0, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 1.0
                }
            },
            {
                "name": "Mat_Red_SingleSided",
                "doubleSided": False,
                "pbrMetallicRoughness": {
                    "baseColorFactor": [1.0, 0.0, 0.0, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 1.0
                }
            },
            {
                "name": "Mat_Yellow_DoubleSided",
                "doubleSided": True,
                "pbrMetallicRoughness": {
                    "baseColorFactor": [1.0, 1.0, 0.0, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 1.0
                }
            },
            {
                "name": "Mat_Cyan_SingleSided",
                "doubleSided": False,
                "pbrMetallicRoughness": {
                    "baseColorFactor": [0.0, 1.0, 1.0, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 1.0
                }
            }
        ],
        "buffers": [
            {
                "byteLength": len(total_buf),
                "uri": data_uri
            }
        ],
        "bufferViews": [
            { "buffer": 0, "byteOffset": offset_pos, "byteLength": len_pos, "target": 34962 },
            { "buffer": 0, "byteOffset": offset_norm, "byteLength": len_norm, "target": 34962 },
            { "buffer": 0, "byteOffset": offset_idx_ccw, "byteLength": len(idx_ccw_bytes), "target": 34963 },
            { "buffer": 0, "byteOffset": offset_idx_cw, "byteLength": len(idx_cw_bytes), "target": 34963 }
        ],
        "accessors": [
            {
                "bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC3",
                "min": [-0.7, -0.7, 0.0], "max": [0.7, 0.7, 0.0]
            },
            {
                "bufferView": 1, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC3",
                "min": [0.0, 0.0, 1.0], "max": [0.0, 0.0, 1.0]
            },
            {
                "bufferView": 2, "byteOffset": 0, "componentType": 5123, "count": 6, "type": "SCALAR",
                "min": [0], "max": [3]
            },
            {
                "bufferView": 3, "byteOffset": 0, "componentType": 5123, "count": 6, "type": "SCALAR",
                "min": [0], "max": [3]
            }
        ]
    }

    out_file = Path(output_path)
    out_file.parent.mkdir(parents=True, exist_ok=True)
    out_file.write_text(json.dumps(gltf, indent=2), encoding="utf-8")
    print(f"Generated glTF fixture at {out_file.resolve()}")

if __name__ == "__main__":
    create_cull_winding_fixture("build/p3.4/fixtures/cull_winding_fixture.gltf")
