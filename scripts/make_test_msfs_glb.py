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
    bin_chunk = bytearray(data[20 + c0_len:])

    # 1x1 PNG bytes (70 bytes, verified valid PNG)
    png_1x1 = bytes.fromhex(
        "89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c489"
        "0000000d4944415478da6364f8cf500f00038601805a347d6b0000000049454e44ae426082"
    )
    pad = (4 - (len(png_1x1) % 4)) % 4
    png_padded = png_1x1 + b'\x00' * pad

    img_offset = len(bin_chunk) - 8 # subtract 8 bytes chunk 1 header (length + type)
    # The binary chunk starts with 8 bytes header (chunk_len, 0x004E4942)
    c1_len, c1_type = struct.unpack_from('<II', bin_chunk, 0)
    img_byte_offset = c1_len
    # Append to chunk 1 payload
    bin_chunk += png_padded
    # Update chunk 1 length in header
    struct.pack_into('<I', bin_chunk, 0, c1_len + len(png_padded))

    # Update buffers[0].byteLength
    gltf["buffers"][0]["byteLength"] = c1_len + len(png_padded)

    # Add bufferView for the image
    img_bv_idx = len(gltf.get("bufferViews", []))
    gltf.setdefault("bufferViews", []).append({
        "buffer": 0,
        "byteOffset": img_byte_offset,
        "byteLength": len(png_1x1)
    })

    # Inject sample textures and images with names and file references
    gltf["images"] = [
        {"name": "cockpit_albedo.png", "bufferView": img_bv_idx, "mimeType": "image/png"},
        {"name": "fuselage_normal.png", "bufferView": img_bv_idx, "mimeType": "image/png"},
        {"name": "instruments_decal.png", "bufferView": img_bv_idx, "mimeType": "image/png"}
    ]
    gltf["samplers"] = [
        {"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}
    ]
    gltf["textures"] = [
        {"source": 0, "sampler": 0},
        {"source": 1, "sampler": 0, "name": "Cockpit_Normal_Tex"},
        {"source": 2, "sampler": 0}
    ]

    # Inject MSFS extensions into Material 0
    if "materials" in gltf and len(gltf["materials"]) > 0:
        mat = gltf["materials"][0]
        if "pbrMetallicRoughness" in mat:
            mat["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0, "texCoord": 0}
            mat["pbrMetallicRoughness"]["metallicRoughnessTexture"] = {"index": 1, "texCoord": 0}
        mat["normalTexture"] = {"index": 1, "scale": 1.0}
        mat["occlusionTexture"] = {"index": 2, "strength": 0.8}
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
