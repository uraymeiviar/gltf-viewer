from GestaltorAutomated import core_operations as core
from GestaltorAutomated import interface


handle = core.create_handle()

gltf = interface.gltf(handle)

gltf.load_file("./assets/Duck/Duck.gltf")

gltf.scenes[0].name.set("My favorite scene")

gltf.save_file("output.glb")