
import re
from functools import reduce

from GestaltorAutomated import core_operations as core


def to_snake_case(name):
    name = re.sub("(.)([A-Z][a-z]+)", r"\1_\2", name)
    name = re.sub("__([A-Z])", r"_\1", name)
    name = re.sub("([a-z0-9])([A-Z])", r"\1_\2", name)
    return name.lower()


def sanitize(cpp_operation_name):
    return to_snake_case(cpp_operation_name.replace(".", "_"))


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


def get_array_length(handle, path):
    json = core.get_json(handle)
    return len(get_element(json, path.replace("/gltf/", "")))


class Property:
    def __init__(self, path, handle):
        self._path = path
        self._handle = handle

        # Add available operations

        if path == "/gltf":
            operations = core.get_global_operations()
        else:
            operations = core.get_operations_by_path(self._path)

        for op_name in operations:
            # be aware of late binding:
            #  https://stackoverflow.com/questions/3431676/creating-functions-or-lambdas-in-a-loop-or-comprehension
            op_schema = core.get_parameter_schema(op_name)

            def op_function(op_name=op_name, op_schema=op_schema, **kwargs):
                has_path = False
                for entry in op_schema:
                    if "properties" in entry and "path" in entry["properties"]:
                        has_path = True

                if has_path:
                    op_params = {"path": self._path, **kwargs}
                else:
                    op_params = {**kwargs}
                core.call(self._handle, op_name, op_params)

            setattr(self, sanitize(op_name), op_function)

            # Add setter and getter
            if sanitize(op_name) == "set_gltf_value":

                def set_function(value):
                    op_function(value=value)

                setattr(self, "set_value", set_function)

                def get_function():
                    json = core.get_json(self._handle)
                    return get_element(json, self._path.replace("/gltf/", ""))

                setattr(self, "get_value", get_function)

        # Add child properties

        properties = core.get_properties_by_path(self._path)

        for prop_name in properties:
            prop_info = properties[prop_name]
            prop = Property(prop_info["path"], self._handle)
            self.prop_name = prop
            setattr(self, sanitize(prop_name), prop)

        # Add functions for array access
        if core.is_array(self._path):

            def at_function(index):
                return Property(self._path + f"/{index}", self._handle)

            setattr(self, "at", at_function)

    def __getitem__(self, key):
        return Property(self._path + f"/{key}", self._handle)

    def __len__(self) -> int:
        return get_array_length(self._handle, self._path)

    def __iter__(self) -> dict:
        for i, ix in enumerate(range(self.__len__())):
            yield self[i]


def exists(property):
    handle = property._handle
    path = property._path
    json = core.get_json(handle)
    resident_path = core.get_resident_path(path)
    try:
        get_element(json, resident_path.replace("/gltf", ""))
    except Exception:
        return False
    else:
        return True


def get_resident_path(property):
    resident_path = core.get_resident_path(property._path)
    return resident_path


def from_pointer(property):
    resident_path = get_resident_path(property)
    return Property(path=resident_path, handle=property._handle)

def get_index(property):
    paths = property._path.split("/")
    key = paths[-1]
    if key.isnumeric():
        return int(key)
    return -1

