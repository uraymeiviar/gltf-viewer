// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Sampler;

class SamplersToJson : ChildOfRootToJson {
public:
    using ChildOfRootToJson::ChildOfRootToJson;
    void convert(const Sampler& _sampler, nlohmann::json& _jsonSampler, const JsonPath& _jsonPath) const;

private:
    void convertMagFilter(const Sampler& _sampler, nlohmann::json& _jsonSampler) const;
    void convertMinFilter(const Sampler& _sampler, nlohmann::json& _jsonSampler) const;
    void convertWrapS(const Sampler& _sampler, nlohmann::json& _jsonSampler) const;
    void convertWrapT(const Sampler& _sampler, nlohmann::json& _jsonSampler) const;
};
}
