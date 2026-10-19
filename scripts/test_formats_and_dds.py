import os
import struct
import json

def make_rgba8_dds(filepath, width=64, height=64, r=33, g=171, b=107, a=255):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    header = bytearray(128)
    # Magic
    header[0:4] = b"DDS "
    # dwSize = 124
    struct.pack_into('<I', header, 4, 124)
    # dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
    struct.pack_into('<I', header, 8, 0x1007)
    struct.pack_into('<I', header, 12, height)
    struct.pack_into('<I', header, 16, width)
    struct.pack_into('<I', header, 20, width * 4) # pitch
    struct.pack_into('<I', header, 24, 0) # depth
    struct.pack_into('<I', header, 28, 1) # mipMapCount
    
    # ddspf at offset 80 (size 32)
    struct.pack_into('<I', header, 80, 32)
    struct.pack_into('<I', header, 84, 0x41) # DDPF_RGB | DDPF_ALPHAPIXELS
    struct.pack_into('<I', header, 88, 0) # fourCC
    struct.pack_into('<I', header, 92, 32) # rgbBitCount
    struct.pack_into('<I', header, 96, 0x000000FF) # R mask
    struct.pack_into('<I', header, 100, 0x0000FF00) # G mask
    struct.pack_into('<I', header, 104, 0x00FF0000) # B mask
    struct.pack_into('<I', header, 108, 0xFF000000) # A mask
    
    # dwCaps
    struct.pack_into('<I', header, 112, 0x1000) # DDSCAPS_TEXTURE
    
    pixels = bytearray()
    for _ in range(width * height):
        pixels.extend([r, g, b, a])
        
    with open(filepath, 'wb') as f:
        f.write(header)
        f.write(pixels)

def make_bc1_dds(filepath, width=64, height=64):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    header = bytearray(128)
    header[0:4] = b"DDS "
    struct.pack_into('<I', header, 4, 124)
    struct.pack_into('<I', header, 8, 0x1007)
    struct.pack_into('<I', header, 12, height)
    struct.pack_into('<I', header, 16, width)
    struct.pack_into('<I', header, 20, (width * height) // 2)
    struct.pack_into('<I', header, 24, 0)
    struct.pack_into('<I', header, 28, 1)
    
    # ddspf
    struct.pack_into('<I', header, 80, 32)
    struct.pack_into('<I', header, 84, 0x04) # DDPF_FOURCC
    header[88:92] = b"DXT1"
    struct.pack_into('<I', header, 112, 0x1000)
    
    # Green 565 is 0x07E0
    block = struct.pack('<HHI', 0x07E0, 0x07E0, 0x00000000)
    num_blocks = (width // 4) * (height // 4)
    
    with open(filepath, 'wb') as f:
        f.write(header)
        f.write(block * num_blocks)

def make_tga(filepath, width=64, height=64, r=33, g=171, b=107, a=255):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    # 18-byte TGA header (uncompressed true-color)
    header = bytearray(18)
    header[2] = 2 # uncompressed true-color
    struct.pack_into('<H', header, 12, width)
    struct.pack_into('<H', header, 14, height)
    header[16] = 32 # 32 bits per pixel
    header[17] = 8 # 8-bit alpha, origin bottom-left
    
    pixels = bytearray()
    for _ in range(width * height):
        pixels.extend([b, g, r, a]) # BGRA order in TGA
        
    with open(filepath, 'wb') as f:
        f.write(header)
        f.write(pixels)

if __name__ == "__main__":
    test_dir = os.path.abspath("test_formats")
    os.makedirs(test_dir, exist_ok=True)
    
    # 1. DDS RGBA8
    make_rgba8_dds(os.path.join(test_dir, "texture_rgba8.dds"))
    print("Created texture_rgba8.dds")
    
    # 2. DDS BC1 (DXT1)
    make_bc1_dds(os.path.join(test_dir, "texture_bc1.dds"))
    print("Created texture_bc1.dds")
    
    # 3. TGA
    make_tga(os.path.join(test_dir, "texture.tga"))
    print("Created texture.tga")
