import os
from test_formats_and_dds import make_tga
from make_test_search_path import create_test_gltf

if __name__ == "__main__":
    base_dir = os.path.abspath("test_tga_scenario")
    model_dir = os.path.join(base_dir, "model")
    texture_dir = os.path.join(base_dir, "texture")
    
    os.makedirs(model_dir, exist_ok=True)
    os.makedirs(texture_dir, exist_ok=True)
    
    # In glTF, the image URI is "test_texture.tga"
    create_test_gltf(model_dir, "test_texture.tga")
    
    # On disk in ../texture, the file is named "test_texture.tga"
    make_tga(os.path.join(texture_dir, "test_texture.tga"), width=64, height=64, r=40, g=180, b=120)
    
    print("Created TGA test scenario in:", base_dir)
