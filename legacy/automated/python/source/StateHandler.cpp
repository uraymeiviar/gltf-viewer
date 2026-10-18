// Copyright © 2022 UX3D GmbH. All rights reserved.

#include "GestaltorOperations/StateHandler.h"

#include "reactor/Gltf.h"
#include "reactor/Json.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"
#include "reactor/Utils.h"
#include <gltf_schema/gltf_single_json.h>

#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace ux3d;
using namespace ux3d::python;

StateHandler::StateHandler()
{
    auto schemas = gltf_schema::merged();
    auto gltfSchema = reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
    m_linearizedSchema = reactor::schema::linearize(gltfSchema);
    library.loadPlugin(operations::gltfOperations(gltfSchema, m_linearizedSchema));
    m_linearizedSchemaResolved = reactor::json::resolveAllOf(m_linearizedSchema);

    m_operationsByName = reactor::operations::byName(library.schema);
    m_operationsByPathPattern = reactor::operations::byPathPattern(m_operationsByName);
}

StateHandler::Handle StateHandler::createHandle()
{
    Handle reservedHandle = m_availableHandle;
    m_jsonMap[reservedHandle] = nlohmann::json::object();
    m_uuidMap[reservedHandle] = nlohmann::json::object();
    m_uuidPathMap[reservedHandle] = nlohmann::json::object();
    m_availableHandle++;
    return reservedHandle;
}

bool StateHandler::validHandle(Handle handle)
{
    return m_jsonMap.count(handle) != 0;
}

nlohmann::json StateHandler::getJson(Handle handle)
{
    return m_jsonMap[handle];
}

void StateHandler::updateState(Handle handle, const nlohmann::json& state)
{
    auto reverse_lookup_f = [](const nlohmann::json& uuids) -> nlohmann::json {
        nlohmann::json uuidPointers;
        auto flattened = uuids.at("gltf").flatten();
        for (const auto& [path, uuid] : flattened.items()) {
            if (uuid.is_string()) {
                uuidPointers[uuid.get<std::string>()] = nlohmann::json::json_pointer(path).parent_pointer().to_string();
            }
        }
        return uuidPointers;
    };

    auto minimalDiff = reactor::json::minimal_diff(m_jsonMap[handle], state);
    m_uuidMap[handle] = reactor::gltf::propertyUUIDs(std::move(m_uuidMap[handle]), minimalDiff);
    m_uuidPathMap[handle] = reverse_lookup_f(m_uuidMap[handle]);
    m_jsonMap[handle] = state;
}

std::string StateHandler::getUUID(Handle handle, std::string path)
{

    if (m_uuidMap.count(handle) == 0) {
        return std::string("invalid handle");
    }
    if (!m_uuidMap[handle].contains(nlohmann::json::json_pointer(path) / "uuid")) {

        return std::string("invalid path");
    }
    return m_uuidMap[handle].at(nlohmann::json::json_pointer(path) / "uuid");
}

std::string StateHandler::getPath(Handle handle, std::string uuid)
{
    auto& uuidPaths = m_uuidPathMap[handle];
    if (!uuidPaths.contains(uuid)) {
        return std::string("invalid uuid: " + uuid);
    }
    return "/gltf" + uuidPaths[uuid].get<std::string>();
}

std::string StateHandler::getLocalPath(std::string path)
{
    return reactor::gltf::schema::getLocalPath(m_linearizedSchemaResolved, path);
}

std::string StateHandler::getGlobalPath(std::string path)
{
    return reactor::gltf::schema::getGlobalPath(m_linearizedSchemaResolved, path);
}

StateHandler* StateHandler::getInstance()
{
    static StateHandler instance;
    return &instance;
}

std::vector<float> StateHandler::getBinaryData(Handle handle, std::string path)
{
    auto& state = m_jsonMap[handle];
    if (!state.contains(nlohmann::json::json_pointer(path) / "componentType")) {
        throw std::runtime_error("Invalid property");
    }
    if (state.at(nlohmann::json::json_pointer(path) / "componentType").get<uint32_t>() != 5126) {
        throw std::runtime_error("Invalid component type");
    }
    std::vector<float> result = reactor::utils::getAccessorData<float>(nlohmann::json::json_pointer(path), state, storage);
    return result;
}

nlohmann::json StateHandler::getOperationsByPath(std::string path)
{
    return reactor::operations::byPath(path, m_operationsByPathPattern);
}

nlohmann::json StateHandler::getGlobalOperations()
{
    return reactor::operations::global(m_operationsByName);
}

nlohmann::json StateHandler::getParameterSchema(std::string operation)
{
    if (!m_operationsByName.contains(operation)) {
        return nlohmann::json::object();
    }
    return m_operationsByName.at(operation);
}

nlohmann::json StateHandler::getPropertiesByPath(std::string path)
{
    auto localpath = path.substr(std::string("/gltf").size());

    std::string pattern = "";
    try {
        pattern = reactor::schema::mapToPattern(m_linearizedSchema, localpath);
    } catch (std::exception& e) {
        std::cout << "path could not be matched: " << localpath << std::endl;
        return nlohmann::json::object(); // path could not be matched
    }

    nlohmann::json properties = nlohmann::json::object();
    if (m_linearizedSchema.at(pattern).contains("properties")) {
        for (auto property : m_linearizedSchema.at(pattern).at("properties").items()) {
            properties[property.key()] = {};
            properties[property.key()]["path"] = nlohmann::json::json_pointer(path) / property.key();
        }
    }

    return properties;
}

bool StateHandler::isArray(std::string path)
{
    auto localpath = path.substr(std::string("/gltf").size());

    std::string pattern = "";
    try {
        pattern = reactor::schema::mapToPattern(m_linearizedSchema, localpath);
    } catch (std::exception& e) {
        std::cout << "path could not be matched: " << localpath << std::endl;
        return false;
    }

    if (m_linearizedSchema.at(pattern).contains("type")
        && m_linearizedSchema.at(pattern).at("type").get<std::string>() == "array") {
        return true;
    }
    return false;
}

std::string StateHandler::getResidentPath(std::string path)
{
    auto localpath = path.substr(std::string("/gltf").size());

    std::string pattern = reactor::schema::mapToPattern(m_linearizedSchema, localpath);

    if (m_linearizedSchema.at(pattern).contains("ux3d_pathPrefix")) {
        auto category = nlohmann::json::json_pointer(m_linearizedSchema.at(pattern).at("ux3d_pathPrefix").get<std::string>());
        auto gltfPrefix = nlohmann::json::json_pointer("/gltf");
        auto residentPath = (gltfPrefix / category).to_string();
        return residentPath;
    }

    throw std::runtime_error("Property has no resident path: " + path);
}
