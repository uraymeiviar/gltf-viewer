// Copyright © 2022 UX3D GmbH. All rights reserved.

#pragma once

#include "operations/Operations.h"
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <tuple>

#include "datastorage/DataStorage.h"
#include "operations/Library.h"

#include "GestaltorOperations/StateHandler.h"

namespace ux3d::python::wrapper {

StateHandler::Handle createHandle();

struct GltfHandle {
    GltfHandle();

    StateHandler::Handle getHandle();
    int call(std::string op, const nlohmann::json& param);
    nlohmann::json getJson();

    StateHandler::Handle m_handle = -1;
};

nlohmann::json getJson(int handle);
nlohmann::json validate(int handle);
bool exists(int handle, std::string path);
std::string deref(int handle, std::string path);
nlohmann::json getProperty(int handle, std::string path);
int callOperation(int handle, std::string op, const nlohmann::json& param);
std::string getUUID(int handle, std::string path);
std::string getPath(int handle, std::string uuid);
std::string getLocalPath(std::string path);
std::string getGlobalPath(std::string path);
std::vector<float> getBinaryData(int handle, std::string path);

nlohmann::json getOperationsByPath(std::string path);
nlohmann::json getGlobalOperations();
nlohmann::json getParameterSchema(std::string operation);
nlohmann::json getPropertiesByPath(std::string path);
bool isArray(std::string path);
std::string getResidentPath(std::string path);
}
