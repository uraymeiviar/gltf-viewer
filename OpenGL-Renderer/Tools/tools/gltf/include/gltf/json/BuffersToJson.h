// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Buffer;

class BuffersToJson : ChildOfRootToJson {
public:
    using ChildOfRootToJson::ChildOfRootToJson;
    void convert(const Buffer& _buffer, nlohmann::json& _jsonBuffer, const JsonPath& _jsonPath) const;

private:
    void convertUri(const Buffer& _buffer, nlohmann::json& _jsonBuffer) const;
    void convertByteLength(const Buffer& _buffer, nlohmann::json& _jsonBuffer) const;

    void convertStorageFormat(const Buffer& _buffer, nlohmann::json& _jsonBuffer) const;
};
}
