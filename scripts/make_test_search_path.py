import os
import json
import struct
import zlib

def make_png(filepath, color=(33, 171, 107, 255), width=64, height=64):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    # Write a simple RGBA PNG
    raw_data = bytearray()
    for y in range(height):
        raw_data.append(0) # filter type none
        for x in range(width):
            raw_data.extend(color)
    
    compressed = zlib.compress(raw_data)
    
    png_bytes = bytearray(b'\x89PNG\r\n\x1a\n')
    
    def write_chunk(chunk_type, data):
        png_bytes.extend(struct.pack('>I', len(data)))
        png_bytes.extend(chunk_type)
        png_bytes.extend(data)
        crc = zlib.crc32(chunk_type + data) & 0xffffffff
        png_bytes.extend(struct.pack('>I', crc))
        
    # IHDR
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    write_chunk(b'IHDR', ihdr_data)
    # IDAT
    write_chunk(b'IDAT', compressed)
    # IEND
    write_chunk(b'IEND', b'')
    
    with open(filepath, 'wb') as f:
        f.write(png_bytes)

def create_test_gltf(model_dir, texture_name):
    os.makedirs(model_dir, exist_ok=True)
    gltf = {
        "asset": {"version": "2.0", "generator": "GestaltorSearchPathTest"},
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "TestPlane"}],
        "meshes": [{
            "primitives": [{
                "attributes": {
                    "POSITION": 0,
                    "NORMAL": 1,
                    "TEXCOORD_0": 2
                },
                "indices": 3,
                "material": 0,
                "mode": 4
            }]
        }],
        "materials": [{
            "name": "SearchPathTestMat",
            "pbrMetallicRoughness": {
                "baseColorTexture": {"index": 0},
                "metallicFactor": 0.0,
                "roughnessFactor": 0.5
            }
        }],
        "textures": [{"sampler": 0, "source": 0, "name": "Cockpit_Albedo_Texture"}],
        "images": [{"uri": texture_name, "name": "Cockpit_Albedo_Image"}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}],
        "accessors": [
            {
                "bufferView": 0,
                "byteOffset": 0,
                "componentType": 5126,
                "count": 4,
                "type": "VEC3",
                "max": [1.0, 1.0, 0.0],
                "min": [-1.0, -1.0, 0.0]
            },
            {
                "bufferView": 1,
                "byteOffset": 0,
                "componentType": 5126,
                "count": 4,
                "type": "VEC3",
                "max": [0.0, 0.0, 1.0],
                "min": [0.0, 0.0, 1.0]
            },
            {
                "bufferView": 2,
                "byteOffset": 0,
                "componentType": 5126,
                "count": 4,
                "type": "VEC2",
                "max": [1.0, 1.0],
                "min": [0.0, 0.0]
            },
            {
                "bufferView": 3,
                "byteOffset": 0,
                "componentType": 5123,
                "count": 6,
                "type": "SCALAR",
                "max": [3],
                "min": [0]
            }
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 48},
            {"buffer": 0, "byteOffset": 48, "byteLength": 48},
            {"buffer": 0, "byteOffset": 96, "byteLength": 32},
            {"buffer": 0, "byteOffset": 128, "byteLength": 12}
        ],
        "buffers": [{
            "byteLength": 140,
            "uri": "plane_geometry.bin"
        }]
    }
    
    # Write bin data
    # 4 positions
    positions = [
        -1.0, -1.0, 0.0,
         1.0, -1.0, 0.0,
         1.0,  1.0, 0.0,
        -1.0,  1.0, 0.0
    ]
    # 4 normals
    normals = [
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0,
        0.0, 0.0, 1.0
    ]
    # 4 uvs
    uvs = [
        0.0, 1.0,
        1.0, 1.0,
        1.0, 0.0,
        0.0, 0.0
    ]
    # 6 indices (2 triangles)
    indices = [0, 1, 2, 0, 2, 3]
    
    bin_bytes = bytearray()
    for p in positions:
        bin_bytes.extend(struct.pack('<f', p))
    for n in normals:
        bin_bytes.extend(struct.pack('<f', n))
    for u in uvs:
        bin_bytes.extend(struct.pack('<f', u))
    for idx in indices:
        bin_bytes.extend(struct.pack('<H', idx))
        
    with open(os.path.join(model_dir, "plane_geometry.bin"), "wb") as f:
        f.write(bin_bytes)
        
    with open(os.path.join(model_dir, "test_plane.gltf"), "w") as f:
        json.dump(gltf, f, indent=2)

if __name__ == "__main__":
    base_dir = os.path.abspath("test_msfs_structure")
    model_dir = os.path.join(base_dir, "model")
    texture_dir = os.path.join(base_dir, "texture")
    
    tex_filename = "Cockpit_STD_NORM.png"
    # Create the gltf in model_dir referencing Cockpit_STD_NORM.png
    create_test_gltf(model_dir, tex_filename)
    
    # Put the actual texture in texture_dir (sibling to model_dir, so relative path from gltf is ../texture)
    make_png(os.path.join(texture_dir, tex_filename), color=(33, 171, 107, 255))
    
    print(f"Created test structure in {base_dir}:")
    print(f"glTF: {os.path.join(model_dir, 'test_plane.gltf')}")
    print(f"Texture: {os.path.join(texture_dir, tex_filename)}")
