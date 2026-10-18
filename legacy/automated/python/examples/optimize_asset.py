# This example demonstrates how to cleanup a glTF and how to reduce the file size

from GestaltorAutomated import core_operations as core
from GestaltorAutomated import interface


# This function applies Draco Mesh Compression to all meshes, reduces image sizes to 256x256, and removes all unused properties.
def optimize(input_path, output_path):
    handle = core.create_handle()
    gltf = interface.gltf(handle)
    print("Load: "+input_path)
    gltf.load_file(input_path)

    print("Apply Draco Mesh Compression")
    try:
        gltf.apply_draco_for_each()
    except Exception as e:
        print("Unable to apply Draco Mesh Compression")
        print(e)

    print("Reduce image size")
    try:
        gltf.convert_image_for_each(mimeType = "image/jpeg", width = 256, height = 256)
    except Exception as e:
        print("Unable to reduce image size")
        print(e)

    print("Remove unused properties")
    try:
        gltf.clean_gltf()
    except Exception as e:
        print("Unable to clean unused items")
        print(e)

    # save optimized glTF
    print("Save: "+output_path)
    gltf.save_file(filepath = output_path, overwrite = True)


# optimize glTF
optimize("./assets/Duck/Duck.gltf", "output.glb")

