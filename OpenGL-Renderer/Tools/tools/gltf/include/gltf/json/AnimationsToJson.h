// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Animation;
class Gltf;

class AnimationsToJson : ChildOfRootToJson {
public:
    AnimationsToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions, const Gltf& _gltf);
    void convert(const Animation& _animation, nlohmann::json& _jsonAnimation, const JsonPath& _jsonPath) const;

private:
    void convertIsPlaying(const Animation& _animation, nlohmann::json& _jsonAnimation) const;
    void convertChannels(const Animation& _animation, nlohmann::json& _jsonAnimation) const;
    void convertSamplers(const Animation& _animation, nlohmann::json& _jsonAnimation) const;
    void convertXMP(const Animation& _animation, nlohmann::json& _jsonAnimation) const;

    const Gltf& m_gltf;
};
}
