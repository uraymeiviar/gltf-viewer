import os
import json
import base64

def create_test_texture_model():
    out_dir = "build/test_texture_model"
    tex_dir = os.path.join(out_dir, "textures")
    os.makedirs(tex_dir, exist_ok=True)

    # 1x1 red PNG
    png_1x1 = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==")
    
    with open(os.path.join(tex_dir, "cockpit_albedo.png"), "wb") as f:
        f.write(png_1x1)
    with open(os.path.join(tex_dir, "fuselage_normal.png"), "wb") as f:
        f.write(png_1x1)
    with open(os.path.join(out_dir, "instruments_decal.png"), "wb") as f:
        f.write(png_1x1)

    # Minimal glTF with simple triangle and textures
    # Vertex buffer: 3 vertices with pos [0,0,0], [1,0,0], [0,1,0] and uv [0,0], [1,0], [0,1]
    import struct
    buf = bytearray()
    # 3 positions (vec3 float) = 36 bytes
    buf += struct.pack("<3f", 0.0, 0.0, 0.0)
    buf += struct.pack("<3f", 1.0, 0.0, 0.0)
    buf += struct.pack("<3f", 0.0, 1.0, 0.0)
    # 3 texcoords (vec2 float) = 24 bytes
    buf += struct.pack("<2f", 0.0, 0.0)
    buf += struct.pack("<2f", 1.0, 0.0)
    buf += struct.pack("<2f", 0.0, 1.0)
    # 3 indices (uint16) = 6 bytes + 2 padding = 8 bytes
    buf += struct.pack("<3H", 0, 1, 2)
    buf += b'\x00\x00'

    with open(os.path.join(out_dir, "model.bin"), "wb") as f:
        f.write(buf)

    gltf = {
        "asset": {"version": "2.0", "generator": "TestTextureGen"},
        "scenes": [{"nodes": [0]}],
        "scene": 0,
        "nodes": [{"mesh": 0, "name": "AirplaneMesh"}],
        "buffers": [{"uri": "model.bin", "byteLength": len(buf)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962},
            {"buffer": 0, "byteOffset": 36, "byteLength": 24, "target": 34962},
            {"buffer": 0, "byteOffset": 60, "byteLength": 6, "target": 34963}
        ],
        "accessors": [
            {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC3", "max": [1.0, 1.0, 0.0], "min": [0.0, 0.0, 0.0]},
            {"bufferView": 1, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC2", "max": [1.0, 1.0], "min": [0.0, 0.0]},
            {"bufferView": 2, "byteOffset": 0, "componentType": 5123, "count": 3, "type": "SCALAR", "max": [2], "min": [0]}
        ],
        "images": [
            {"uri": "textures/cockpit_albedo.png"},
            {"uri": "textures/fuselage_normal.png"},
            {"name": "DecalImage", "uri": "instruments_decal.png"},
            {"name": "InternalProcedural"}
        ],
        "samplers": [
            {"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}
        ],
        "textures": [
            {"source": 0, "sampler": 0},
            {"source": 1, "sampler": 0, "name": "Cockpit_Normal_Tex"},
            {"source": 2, "sampler": 0},
            {"source": 3, "sampler": 0}
        ],
        "materials": [
            {
                "name": "Cockpit_Material",
                "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": 0}
                },
                "normalTexture": {"index": 1}
            }
        ],
        "meshes": [
            {
                "name": "AirplaneWing",
                "primitives": [
                    {
                        "attributes": {"POSITION": 0, "TEXCOORD_0": 1},
                        "indices": 2,
                        "material": 0
                    }
                ]
            }
        ]
    }

    with open(os.path.join(out_dir, "model.gltf"), "w") as f:
        json.dump(gltf, f, indent=2)

    print(f"Created test texture model at {os.path.join(out_dir, 'model.gltf')}")

if __name__ == "__main__":
    create_test_texture_model()
