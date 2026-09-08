# This example demonstrates merging several glTFs to create one combined asset


import os
import json
from GestaltorAutomated import core_operations as core
from GestaltorAutomated import interface

working_directory = os.getcwd()

def assemble(config_path):
    with open(config_path, "r") as config:
        config = json.load(config)
        gltf_handle = core.create_handle() 
        gltf = interface.gltf(gltf_handle)

        gltf.create_empty_gltf()
        gltf.create_scene()

        # iterate all parts
        for part in config["parts"]:
            try:
                node_count = len(gltf.nodes)
            except KeyError:
                node_count = 0

            # Load file and merge it into the our cumulative glTF.
            path = os.path.normpath(os.path.join(working_directory, part["path"]))
            if ".fbx" in path:
                gltf.import_asset(filepath=path)
            else:
                gltf.merge_gltf(filepath=path)

            # node_path refers to the node which should be edited here.
            # it is provided by the config file.
            node_index: int = node_count + part["node"]

            translation = None
            rotation = None
            scale = None

            # only add transforms if they are found in the config file
            if "translation" in part:
                translation = part["translation"]
            if "rotation" in part:
                rotation = part["rotation"]
            if "scale" in part:
                scale = part["scale"]

            gltf.nodes[node_index].transform_node(rotation, scale, translation)

            gltf.scenes[0].add_node(pathSrc= "/gltf/nodes/" + str(node_index))


    # save final glTF
    output_path = os.path.normpath(os.path.join(working_directory, config["output_path"]))
    gltf.save_file(filepath=output_path, overwrite=True)


assemble("./assets/assemble.json")
assemble("./assets/figure.json")

