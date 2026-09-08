# This example demonstrates how to create a grid of spheres

from GestaltorAutomated import core_operations as core
from GestaltorAutomated import interface


handle = core.create_handle()
gltf = interface.gltf(handle)

# we start with an empty glTF
gltf.create_empty_gltf()
gltf.create_scene()
 
# create a 5x5x5 grid of spheres
for ix in range(5):
    for iy in range(5):
        for iz in range(5):
            # create new node in scene
            gltf.scenes[0].add_new_node()
            node = gltf.nodes[len(gltf.nodes)-1]

            # set position of node and add sphere
            node.transform_node(translation=[ix*3, iy*3, iz*3])
            node.create_mesh(shape="sphere")#

            # lookup mesh material and set base color
            material = node.mesh.deref().primitives[0].material.deref()
            material.pbrMetallicRoughness.baseColorFactor.set([ix/5, iy/5, iz/5, 1])


# save glTF
gltf.save_file(filepath = "output.glb", overwrite = True)
