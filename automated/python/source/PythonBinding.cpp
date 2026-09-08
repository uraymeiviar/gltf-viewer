#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11_json/pybind11_json.hpp>

#include "GestaltorOperations/Wrapper.h"

using namespace ux3d;
using namespace ux3d::python;

PYBIND11_MODULE(core_operations, m)
{
    m.def("create_handle", wrapper::createHandle, "create new handle");

    m.def("call", &wrapper::callOperation, "call op");

    m.def("get_json", &wrapper::getJson, "get json");
    m.def("get_property", &wrapper::getProperty, "get json");
    m.def("validate", &wrapper::validate, "validate glTF");
    m.def("exists", &wrapper::exists, "Check if the element at the given path exists in the glTF ");
    m.def("deref", &wrapper::deref, "Dereferences a property reference ");
    m.def("get_uuid", &wrapper::getUUID, "get uuid for a specific path");
    m.def("get_path", &wrapper::getPath, "get path for a specific uuid");
    m.def("get_local_path", &wrapper::getLocalPath, "get local path from complete path");
    m.def("get_global_path", &wrapper::getGlobalPath, "get global path from complete path");

    m.def("get_operations_by_path", &wrapper::getOperationsByPath, "get list of operations available for specified glTF path");
    m.def("get_global_operations", &wrapper::getGlobalOperations, "get list of operations available globally");
    m.def("get_parameter_schema", &wrapper::getParameterSchema, "get parameters for a specific operation");
    m.def("get_properties_by_path", &wrapper::getPropertiesByPath, "get list of properties available for specified glTF object");
    m.def("is_array", &wrapper::isArray, "returns True if the given path points to an array property");
    m.def("get_resident_path", &wrapper::getResidentPath, "returns the resident path (e.g. /buffers/2) of a pointer path. ");
    m.def("get_binary_data", &wrapper::getBinaryData, "Get binary data of accessor as float array");

    pybind11::class_<wrapper::GltfHandle>(m, "create_instance")
        .def(pybind11::init<>())
        .def("get_handle", &wrapper::GltfHandle::getHandle)
        .def("call", &wrapper::GltfHandle::call)
        .def("get_object", &wrapper::GltfHandle::getJson);

    m.attr("__version__") = "dev";
}