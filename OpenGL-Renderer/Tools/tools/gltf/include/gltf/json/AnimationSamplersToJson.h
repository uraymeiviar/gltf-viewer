// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "nlohmann/json.hpp"

namespace ux3d::gltf {
struct ExportOptions;
class AnimationSampler;
class IndexLookup;

class AnimationSamplersToJson {
public:
    explicit AnimationSamplersToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions)
        : m_indices(_indices)
        , m_exportOptions(_exportOptions)
    {
    }

    void convert(const AnimationSampler& _sampler, nlohmann::json& _jsonSampler);

private:
    void checkValidity(const AnimationSampler& _sampler);
    void convertInput(const AnimationSampler& _sampler, nlohmann::json& _jsonSampler);
    void convertOutput(const AnimationSampler& _sampler, nlohmann::json& _jsonSampler);
    void convertInterpolation(const AnimationSampler& _sampler, nlohmann::json& _jsonSampler);

    const IndexLookup& m_indices;
    const ExportOptions& m_exportOptions;
};
}
