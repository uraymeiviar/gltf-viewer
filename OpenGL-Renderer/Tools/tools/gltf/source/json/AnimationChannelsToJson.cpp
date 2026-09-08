// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/AnimationChannelsToJson.h"
#include "core/Algorithms.h"
#include "gltf/Animation.h"
#include "gltf/ExportOptions.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/GltfObjectToJson.h"

using namespace ux3d;
using namespace ux3d::gltf;

void AnimationChannelsToJson::convert(const AnimationChannel& _channel, nlohmann::json& _jsonChannel)
{
    if (m_exportOptions.conversionUsage == ConversionUsage::ValidGltf && _channel.getTarget().getNode() == nullptr) {
        // Invalid if no extension is used
        if (_channel.getTarget().m_animationPointer.has_value() == false) {
            return;
        }
    }

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(_channel, _jsonChannel);
    gltfObjectToJson.convertUnknownExtensions(_channel, _jsonChannel);

    convertSampler(_channel, _jsonChannel);
    convertTarget(_channel, _jsonChannel);
}

void AnimationChannelsToJson::convertSampler(const AnimationChannel& _channel, nlohmann::json& _jsonChannel)
{
    const AnimationSampler* sampler = _channel.getSampler();

    if (sampler == nullptr) {
        return;
    }

    size_t index = core::findIndexOf(_channel.m_RootAnimation.getSamplers(), sampler);
    _jsonChannel["sampler"] = index;
}

void AnimationChannelsToJson::convertTarget(const AnimationChannel& _channel, nlohmann::json& _jsonChannel)
{
    const AnimationChannelTarget& target = _channel.getTarget();
    nlohmann::json jsonTarget;

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(target, jsonTarget);
    gltfObjectToJson.convertUnknownExtensions(target, jsonTarget);

    if (target.getNode() != nullptr) {
        size_t index = m_indices[target.getNode()];
        jsonTarget["node"] = index;
    }

    jsonTarget["path"] = animationPathToString(target.getPath());
    if (target.m_animationPointer.has_value()) {
        jsonTarget["extensions"] = nlohmann::json::object();
        jsonTarget["extensions"]["KHR_animation_pointer"] = nlohmann::json::object();
        auto pointer = AnimationChannelTarget::toAnimationPointer(m_indices, target.m_animationPointer.value());
        jsonTarget["extensions"]["KHR_animation_pointer"]["pointer"] = pointer;
    }
    _jsonChannel["target"] = jsonTarget;
}
