# This example demonstrates asset merging.

from GestaltorAutomated import core_operations as core
from GestaltorAutomated import interface

sofa_path = "./assets/GlamVelvetSofa/GlamVelvetSofa.gltf"
duck_path = "./assets/Duck/Duck.gltf"

handle = core.create_handle()

gltf = interface.gltf(handle)
 
gltf.load_file(sofa_path)

# Import duck 
gltf.merge_gltf(duck_path)
imported_scene = gltf.scenes[1]
duck_node = imported_scene.nodes[0].deref()

# Add duck to default scene
gltf.scenes[0].add_node(duck_node.path)
 
# Create copies of the Duck and place them on the sofa
duck_node.transform_node(translation=[-0.8, 0.44, -0.1], scale= [0.1, 0.1, 0.1])
duck_node.clone_property(cloneModeShallow=True,  target= "parent")
duck_node.transform_node(translation= [0.2, 0, 0])
duck_node.clone_property(cloneModeShallow=True,  target= "parent")
duck_node.transform_node(translation= [0.2, 0, 0])

 
gltf.save_file(filepath="output.glb", overwrite=True)
