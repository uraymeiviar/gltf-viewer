<img width="128" align="left" src="../resources/Gestaltor_AppIcon_256.ico" />

# **Gestaltor Python Interface**

A Python module to access Gestaltor operations 

<br/>
<br/>

## Python module setup

- Build the project `GestaltorOperations` that is located in this folder (`python_binding`).
- In your binary output folder (e.g. *Gestaltor/build/bin/Debug*), you now have a python module file (e.g. *GestaltorOperationsd.cp39-win_amd64.pyd*)
- When loading a python module, the module name has to match the file name. In our case, we have to rename *GestaltorOperations`d`.cp39-win_amd64.pyd* to *GestaltorOperations.cp39-win_amd64.pyd* (`d` was removed)
- Next, our python script needs know the location of the module. You can do this by adding the folder containing the module to the respective environment variable (`PYTHONPATH` for python modules on Windows). The module is accessing the libraries. For this reason, the location of the libraries has to be known, too. Another possibility is to copy the python module and relevant libraries to your python script. 

## Python script

This example loads a glTF file, prints all nodes, changes the name of the first node and saves the file as glb
```python
import GestaltorOperations

# create a glTF handle 
avocado_handle = GestaltorOperations.create_handle()

# call an operation with a glTF handle, OP name, parameters
GestaltorOperations.call(avocado_handle, "load_file", {"filepath": "C:/tmp/glTF/Avocado.gltf"})

# retrieve the glTF as python object 
avocado_json = GestaltorOperations.get_json(avocado_handle)

# inspect elements
print(avocado_json.get("nodes"))
# [{'mesh': 0, 'name': 'Avocado', 'rotation': [0.0, 1.0, 0.0, 0.0]}]

# another way to define parameters
params = {}
params["path"] = "/gltf/nodes/0/name"
params["value"] = "new name"
GestaltorOperations.call(avocado_handle, "set_gltf_value", params)

# save modified Avocado as glb file
GestaltorOperations.call(avocado_handle, "save_file", {"filepath": "C:/tmp/glTF/Avocado.glb"})
```


## Current limitations

Python version: GestaltorOpertions does not work with Python 3.10.2
