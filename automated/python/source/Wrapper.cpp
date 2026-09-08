// Copyright © 2022 UX3D GmbH. All rights reserved.

#include "GestaltorOperations/Wrapper.h"

#include "GestaltorOperations/StateHandler.h"
#include "reactor/Gltf.h"
#include "reactor/Json.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"
#include "reactor/Validation.h"
#include <gltf_schema/gltf_single_json.h>

#include <iostream>

using namespace ux3d;
using namespace ux3d::python;

StateHandler::Handle wrapper::createHandle()
{
    return StateHandler::getInstance()->createHandle();
}

nlohmann::json wrapper::getJson(int handle)
{
    return StateHandler::getInstance()->getJson(handle).at("gltf");
}

nlohmann::json wrapper::validate(int handle)
{
    auto gltf = StateHandler::getInstance()->getJson(handle).at("gltf");
    return reactor::validation::validate(gltf);
}

bool wrapper::exists(int handle, std::string path)
{
    if (StateHandler::getInstance()->getJson(handle).at("gltf").contains(nlohmann::json::json_pointer(path))) {
        return true;
    } else {
        return false;
    }
}

std::string wrapper::deref(int handle, std::string path)
{
    auto index = StateHandler::getInstance()->getJson(handle).at(nlohmann::json::json_pointer("/gltf" + path)).get<uint64_t>();
    auto resident_path = StateHandler::getInstance()->getResidentPath("/gltf" + path);
    return resident_path + "/" + std::to_string(index);
}

nlohmann::json wrapper::getProperty(int handle, std::string path)
{
    return StateHandler::getInstance()->getJson(handle).at(nlohmann::json::json_pointer("/gltf" + path));
}

std::string wrapper::getUUID(int handle, std::string path)
{
    return StateHandler::getInstance()->getUUID(handle, path);
}

std::string wrapper::getPath(int handle, std::string uuid)
{
    return StateHandler::getInstance()->getPath(handle, uuid);
}

std::string wrapper::getLocalPath(std::string path)
{
    return StateHandler::getInstance()->getLocalPath(path);
}

std::string wrapper::getGlobalPath(std::string path)
{
    return StateHandler::getInstance()->getGlobalPath(path);
}

int wrapper::callOperation(int handle, std::string op, const nlohmann::json& param)
{
    auto* stateHandler = StateHandler::getInstance();
    if (!stateHandler->validHandle(handle)) {
        std::cout << "Invalid handle" << std::endl;
        return 0;
    }

    auto state = stateHandler->getJson(handle);
    auto& library = stateHandler->library;
    auto& storage = stateHandler->storage;

    nlohmann::json taskDescription;
    taskDescription[op] = param;

    state = library.call(taskDescription, std::move(state), storage, [](auto&&...) {});

    stateHandler->updateState(handle, state);

    return 0;
}

std::vector<float> wrapper::getBinaryData(int handle, std::string path)
{
    return StateHandler::getInstance()->getBinaryData(handle, path);
}

wrapper::GltfHandle::GltfHandle()
{
    m_handle = wrapper::createHandle();
}

StateHandler::Handle wrapper::GltfHandle::getHandle()
{
    return m_handle;
}

int wrapper::GltfHandle::call(std::string op, const nlohmann::json& param)
{
    return wrapper::callOperation(m_handle, op, param);
}

nlohmann::json wrapper::GltfHandle::getJson()
{
    return wrapper::getJson(m_handle);
}

nlohmann::json wrapper::getOperationsByPath(std::string path)
{
    return StateHandler::getInstance()->getOperationsByPath(path);
}

nlohmann::json wrapper::getGlobalOperations()
{
    return StateHandler::getInstance()->getGlobalOperations();
}

nlohmann::json wrapper::getParameterSchema(std::string operation)
{
    return StateHandler::getInstance()->getParameterSchema(operation);
}

nlohmann::json wrapper::getPropertiesByPath(std::string path)
{
    return StateHandler::getInstance()->getPropertiesByPath(path);
}

bool wrapper::isArray(std::string path)
{
    return StateHandler::getInstance()->isArray(path);
}

std::string wrapper::getResidentPath(std::string path)
{
    return StateHandler::getInstance()->getResidentPath(path);
}