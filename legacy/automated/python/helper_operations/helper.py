
from GestaltorAutomated import core_operations as ga


def create_node_grid(gltf_handle, size_x, size_y, spacing, mesh_path, target):
    gltf_json = ga.get_json(gltf_handle)
    node_count = len(gltf_json.get("nodes"))
    i = node_count
    for ix in range(size_x):
        for iy in range(size_y):
            current_node = "/gltf/nodes/"+str(i)
            ga.call(gltf_handle, "add_new_node", {"path": target})
            ga.call(gltf_handle, "transform_node", {"path":current_node, "translation": [ix*spacing,iy*spacing,0]})
            ga.call(gltf_handle, "set_gltf_pointer", {"path": current_node+"/mesh", "pointer": mesh_path})
            i+=1

