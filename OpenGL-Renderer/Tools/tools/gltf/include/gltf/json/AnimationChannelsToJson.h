// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "nlohmann/json.hpp"

namespace ux3d::gltf {
struct ExportOptions;
class AnimationChannel;
class IndexLookup;

class AnimationChannelsToJson {
public:
    explicit AnimationChannelsToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions)
        : m_indices(_indices)
        , m_exportOptions(_exportOptions)
    {
    }

    void convert(const AnimationChannel& _channel, nlohmann::json& _jsonChannel);

private:
    void convertSampler(const AnimationChannel& _channel, nlohmann::json& _jsonChannel);
    void convertTarget(const AnimationChannel& _channel, nlohmann::json& _jsonChannel);

    const IndexLookup& m_indices;
    const ExportOptions& m_exportOptions;
};
}
