// Copyright © 2022 UX3D GmbH. All rights reserved.

#pragma once

#include "operations/Operations.h"
#include <filesystem>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <tuple>

#include "datastorage/DataStorage.h"
#include "operations/Library.h"

namespace ux3d::python {

class StateHandler {
public:
    using Handle = int64_t;

    static StateHandler* getInstance();
    StateHandler(StateHandler const&) = delete;
    void operator=(StateHandler const&) = delete;

    Handle createHandle();
    bool validHandle(Handle handle);
    nlohmann::json getJson(Handle handle);
    void updateState(Handle handle, const nlohmann::json& state);
    std::string getUUID(Handle handle, std::string path);
    std::string getPath(Handle handle, std::string uuid);
    std::string getLocalPath(std::string path);
    std::string getGlobalPath(std::string path);

    ux3d::datastorage::DataStorage storage;
    ux3d::operations::Library library;

    std::vector<float> getBinaryData(Handle handle, std::string path);

    nlohmann::json getOperationsByPath(std::string path);
    nlohmann::json getGlobalOperations();
    nlohmann::json getParameterSchema(std::string operation);
    nlohmann::json getPropertiesByPath(std::string path);
    bool isArray(std::string path);
    std::string getResidentPath(std::string path);

private:
    StateHandler();
    std::unordered_map<Handle, nlohmann::json> m_jsonMap;
    std::unordered_map<Handle, nlohmann::json> m_uuidMap;
    std::unordered_map<Handle, nlohmann::json> m_uuidPathMap;
    int m_availableHandle = 0;

    // Precalculated:
    nlohmann::json m_operationsByName = {};
    nlohmann::json m_operationsByPathPattern = {};
    nlohmann::json m_linearizedSchema = {};
    nlohmann::json m_linearizedSchemaResolved = {};
};

}
