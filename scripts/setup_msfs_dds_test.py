import os
import shutil
from test_formats_and_dds import make_bc1_dds
from make_test_search_path import create_test_gltf

if __name__ == "__main__":
    base_dir = os.path.abspath("test_dds_scenario")
    model_dir = os.path.join(base_dir, "model")
    texture_dir = os.path.join(base_dir, "texture")
    
    os.makedirs(model_dir, exist_ok=True)
    os.makedirs(texture_dir, exist_ok=True)
    
    # In glTF, the image URI is "Cockpit_STD_NORM.png"
    create_test_gltf(model_dir, "Cockpit_STD_NORM.png")
    
    # On disk in ../texture, the file is named "Cockpit_STD_NORM.png.dds" (standard MSFS format!)
    # and is a BC1 DDS texture
    make_bc1_dds(os.path.join(texture_dir, "Cockpit_STD_NORM.png.dds"), width=64, height=64)
    
    print("Created MSFS glTF scenario in:", base_dir)
    print("glTF references: Cockpit_STD_NORM.png")
    print("Disk contains:  texture/Cockpit_STD_NORM.png.dds")
