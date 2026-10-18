from GestaltorAutomated import core_operations as core
from GestaltorAutomated import interface


handle = core.create_handle()

gltf = interface.gltf(handle)

gltf.load_file("./assets/SimpleSkin/SimpleSkin.gltf")

# core.call(handle, "remove_gltf_pointer", {"path": "/gltf/skins/0/joints/0"})
print(gltf.scenes[0].nodes[0].exists())
gltf.skins[0].joints[0].set(gltf.nodes[3])

gltf.save_file("./assets/SimpleSkin.glb", overwrite=True)
