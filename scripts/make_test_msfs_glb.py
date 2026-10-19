import struct
import json
import os

def create_msfs_glb(input_glb, output_glb):
    with open(input_glb, 'rb') as f:
        data = f.read()

    magic, version, length = struct.unpack_from('<III', data, 0)
    assert magic == 0x46546C67, "Not a valid GLB"

    # Read Chunk 0 (JSON)
    c0_len, c0_type = struct.unpack_from('<II', data, 12)
    assert c0_type == 0x4E4F534A, "Chunk 0 is not JSON"
    json_bytes = data[20:20 + c0_len]
    gltf = json.loads(json_bytes.decode('utf-8'))

    # Read remaining binary chunks
    bin_chunk = data[20 + c0_len:]

    # Inject MSFS extensions into Material 0
    if "materials" in gltf and len(gltf["materials"]) > 0:
        mat = gltf["materials"][0]
        if "extensions" not in mat:
            mat["extensions"] = {}
        mat["extensions"]["ASOBO_normal_map_convention"] = {
            "tangent_space_convention": "DirectX"
        }
        mat["extensions"]["ASOBO_material_windshield_v3"] = {
            "wiper1State": 0.65,
            "wiperLines": True,
            "wiperLinesTiling": 5.0,
            "microScratchesStrength": 0.35,
            "microScratchesTiling": 12.0
        }
        mat["extensions"]["ASOBO_material_detail_map"] = {
            "UVScale": 8.0,
            "blendThreshold": 0.45
        }
        mat["extensions"]["ASOBO_material_UV_options"] = {
            "clampUVX": True,
            "clampUVY": True,
            "UVOffSetU": 0.0,
            "UVOffSetV": 0.0,
            "UVTilingU": 2.0,
            "UVTilingV": 2.0
        }
        mat["extensions"]["ASOBO_material_alphamode_dither"] = {
            "enabled": True
        }

    # Inject MSFS extensions into Node 0
    if "nodes" in gltf and len(gltf["nodes"]) > 0:
        node = gltf["nodes"][0]
        if "extensions" not in node:
            node["extensions"] = {}
        node["extensions"]["ASOBO_gizmo_object"] = {
            "gizmoType": "switch"
        }
        node["extensions"]["ASOBO_collision_object"] = {
            "collisionType": "box"
        }

    # Inject extensionsUsed
    if "extensionsUsed" not in gltf:
        gltf["extensionsUsed"] = []
    for ext in ["ASOBO_normal_map_convention", "ASOBO_material_windshield_v3",
                "ASOBO_material_detail_map", "ASOBO_material_UV_options",
                "ASOBO_material_alphamode_dither", "ASOBO_gizmo_object",
                "ASOBO_collision_object"]:
        if ext not in gltf["extensionsUsed"]:
            gltf["extensionsUsed"].append(ext)

    # Re-pack GLB
    new_json_bytes = json.dumps(gltf, separators=(',', ':')).encode('utf-8')
    # Pad JSON to 4-byte boundary with spaces
    pad = (4 - (len(new_json_bytes) % 4)) % 4
    new_json_bytes += b' ' * pad

    new_c0_header = struct.pack('<II', len(new_json_bytes), 0x4E4F534A)
    total_len = 12 + 8 + len(new_json_bytes) + len(bin_chunk)
    new_header = struct.pack('<III', magic, version, total_len)

    os.makedirs(os.path.dirname(os.path.abspath(output_glb)), exist_ok=True)
    with open(output_glb, 'wb') as f:
        f.write(new_header)
        f.write(new_c0_header)
        f.write(new_json_bytes)
        f.write(bin_chunk)

    print(f"Created MSFS test GLB: {output_glb} ({total_len} bytes)")

if __name__ == "__main__":
    create_msfs_glb("resources/sampleGltf/HelloCube.glb", "resources/sampleGltf/MsfsTestCube.glb")
