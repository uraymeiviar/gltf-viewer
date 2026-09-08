// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Skin;

class SkinsToJson : ChildOfRootToJson {
public:
    using ChildOfRootToJson::ChildOfRootToJson;
    void convert(const Skin& _skin, nlohmann::json& _jsonSkin, const JsonPath& _jsonPath) const;

private:
    void convertInverseBindMatrices(const Skin& _skin, nlohmann::json& _jsonSkin) const;
    void convertSkeleton(const Skin& _skin, nlohmann::json& _jsonSkin) const;
    void convertJoints(const Skin& _skin, nlohmann::json& _jsonSkin) const;
};
}
