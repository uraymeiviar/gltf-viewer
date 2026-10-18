import sys
from typing import TypeVar, Generic, Iterator
module_path = "build/bin/Debug"
sys.path.append(module_path)
from GestaltorAutomated import core_operations as core

def get_path_by_uuid(handle, uuid):
    return core.get_path(handle, uuid)

def get_element(json, path):
    paths = path.split("/")
    data = json
    for i in range(0, len(paths)):
        key = paths[i]
        if key=="":
            continue
        if key.isnumeric():
            key = int(key)
        data = data[key]
    return data

def get_property(property):
    handle = property.handle
    path = property.path
    return core.get_property(handle, path.replace("/gltf", ""))

def get_array_length(handle, path):
    json = core.get_json(handle)
    return len(get_element(json, path.replace("/gltf/", "")))

def get_binary_data(property):
    handle = property.handle
    path = property.path
    return core.get_binary_data(handle, path)

def exists(property):
    handle = property.handle
    path = property.path
    return core.exists(handle, path.replace("/gltf", ""))

def deref(property):
    handle = property.handle
    path = property.path
    return core.deref(handle, path.replace("/gltf", ""))


def call(self, handle, op_name, op_params):
    if "path" in op_params and hasattr(self, 'uuid'): 
        op_params["path"] = get_path_by_uuid(handle, self.uuid) + self.local_path
    core.call(handle,  op_name, op_params)

class ObjectProperty:
    def __init__(self, path, handle):
        self.global_path = core.get_global_path(path)
        self.local_path = core.get_local_path(path)
        self.uuid = core.get_uuid(handle, self.global_path)

    def get_binary_data(self):
        self.path = get_path_by_uuid(self.handle, self.uuid) + self.local_path
        return get_binary_data(self)
    
    def exists(self):
        self.path = get_path_by_uuid(self.handle, self.uuid) + self.local_path
        return exists(self)
    

T = TypeVar('T')
class Reference(ObjectProperty, Generic[T]):
    def __init__(self, path, handle, ref_type):
        self.ref_type = ref_type
        self.path = path
        self.handle = handle
        ObjectProperty.__init__(self, path, handle) 

    def deref(self)->T:
        self.path = get_path_by_uuid(self.handle, self.uuid) + self.local_path
        deref_path = deref(self)
        return self.ref_type(deref_path, self.handle)

    def exists(self):
        self.path = get_path_by_uuid(self.handle, self.uuid) + self.local_path
        return exists(self)
    
    def set(self, value):
        if type(value) is str:
            path = value
        else:
            path = value.path
        call(self, self.handle, 'set_gltf_pointer', {"path": self.path, "pointer": path})


# E.g. gltf.nodes
T = TypeVar('T')
class PropertyArray(Generic[T]):
    def __getitem__(self, key) -> T:
        return self.type(self.path + f'/{key}', self.handle)

    def __len__(self) -> int:
        return get_array_length(self.handle, self.path)

    def __iter__(self) -> Iterator[T]:
        for i, ix in enumerate(range(self.__len__())):
            yield self[i]

# RerferenceArrays are json arrays containing references to properties.
# E.g. node.children
T = TypeVar('T')
class ReferenceArray(ObjectProperty, Generic[T]):
    def __init__(self, path, handle, type):
        self.type = type
        self.path = path
        self.handle = handle
        ObjectProperty.__init__(self, path, handle) 
    
    def __getitem__(self, key) -> Reference[T]:
        self.path = get_path_by_uuid(self.handle, self.uuid) + self.local_path
        return Reference[T](str(self.path)+f'/{key}', self.handle, self.type)

    def __len__(self) -> int:
        self.path = get_path_by_uuid(self.handle, self.uuid) + self.local_path
        return get_array_length(self.handle, self.path)

    def __iter__(self) -> Iterator[Reference]:
        for i, ix in enumerate(range(self.__len__())):
            yield self[i]
    

class GenericValue(ObjectProperty):
    def __init__(self, path, handle):
        self.path = path
        self.handle = handle
        ObjectProperty.__init__(self, path, handle) 

    def get(self):
        self.path = get_path_by_uuid(self.handle, self.uuid) + self.local_path
        return get_property(self)

    def set(self, value):
        call(self, self.handle,  'set_gltf_value', {"path":self.path, "value":value})

    def exists(self):
        self.path = get_path_by_uuid(self.handle, self.uuid) + self.local_path
        return exists(self)


