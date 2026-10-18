from GestaltorAutomated import core_operations as core
from GestaltorAutomated import interface


handle = core.create_handle()

gltf = interface.gltf(handle)

gltf.load_file("./assets/GlamVelvetSofa/GlamVelvetSofa.gltf")

# iterate all materials
for material in gltf.materials:
    # check if specific property exists in material
    if not material.normalTexture.exists():
        # color all materials without normal texture in red
        material.pbrMetallicRoughness.baseColorFactor.set([1,0,0,1])

gltf.save_file("output.glb")