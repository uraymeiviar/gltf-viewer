// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/AnimationSamplersToJson.h"
#include "core/Algorithms.h"
#include "gltf/Animation.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/GltfObjectToJson.h"

using namespace ux3d;
using namespace ux3d::gltf;

void AnimationSamplersToJson::convert(const AnimationSampler& _sampler, nlohmann::json& _jsonSampler)
{
    checkValidity(_sampler);
    convertInput(_sampler, _jsonSampler);
    convertOutput(_sampler, _jsonSampler);
    convertInterpolation(_sampler, _jsonSampler);

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(_sampler, _jsonSampler);
    gltfObjectToJson.convertUnknownExtensions(_sampler, _jsonSampler);
}

void AnimationSamplersToJson::convertInput(const AnimationSampler& _sampler, nlohmann::json& _jsonSampler)
{
    if (_sampler.getInputAccessor() == nullptr) {
        return;
    }

    size_t index = m_indices[_sampler.getInputAccessor()];
    _jsonSampler["input"] = index;
}

void AnimationSamplersToJson::convertOutput(const AnimationSampler& _sampler, nlohmann::json& _jsonSampler)
{
    if (_sampler.getOutputAccessor() == nullptr) {
        return;
    }

    size_t index = m_indices[_sampler.getOutputAccessor()];
    _jsonSampler["output"] = index;
}

void AnimationSamplersToJson::convertInterpolation(const AnimationSampler& _sampler, nlohmann::json& _jsonSampler)
{
    _jsonSampler["interpolation"] = animationInterpolationToString(_sampler.getInterpolation());
}

void AnimationSamplersToJson::checkValidity(const AnimationSampler& _sampler)
{
    if (!_sampler.isValid()) {
        size_t animationIndex = m_indices[&_sampler.m_RootAnimation];
        size_t samplerIndex = core::findIndexOf(_sampler.m_RootAnimation.getSamplers(), &_sampler);

        core::string message = "Skipping invalid properties while serializing "
                               "AnimationSampler at index %d in Animation at index %d";

        core::Vector<core::string> invalidProperties;
        if (_sampler.getInvalidProperties(invalidProperties)) {
            nlohmann::json jsonProperties = nlohmann::json::array();

            for (const core::string& property : invalidProperties) {
                jsonProperties.emplace_back(property);
            }

            message += ". Invalid properties: " + jsonProperties.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);
        }

        core::logWarning(message.c_str(), samplerIndex, animationIndex);
    }
}
