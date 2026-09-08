// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Light;

class LightsToJson : ChildOfRootToJson {
public:
    using ChildOfRootToJson::ChildOfRootToJson;
    void convert(const Light& _light, nlohmann::json& _jsonLight, const JsonPath& _jsonPath) const;

private:
    void convertColor(const Light& _light, nlohmann::json& _jsonLight) const;
    void convertIntensity(const Light& _light, nlohmann::json& _jsonLight) const;
    void convertType(const Light& _light, nlohmann::json& _jsonLight) const;
    void convertRange(const Light& _light, nlohmann::json& _jsonLight) const;
    void convertSpot(const Light& _light, nlohmann::json& _jsonLight) const;
};
}
