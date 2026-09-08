#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class Animation;
class AnimationChannel;
class AnimationSampler;
class AnimationChannelTarget;

class AnimationsFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonAnimation) override;
    ConversionResult resolve(const nlohmann::json& _jsonAnimation, Animation& _animation);

private:
    ConversionResult convertIsPlaying(const nlohmann::json& _editorExtension, Animation& _animation);
    ConversionResult convertChannel(const nlohmann::json& _jsonChannel, AnimationChannel* _channel);
    ConversionResult convertSampler(const nlohmann::json& _jsonSampler, AnimationSampler* _sampler);

    ConversionResult resolveChannel(const nlohmann::json& _jsonChannel, AnimationChannel& _channel, Animation& _animation);
    ConversionResult resolveSampler(const nlohmann::json& _jsonSampler, AnimationSampler& _sampler);
    ConversionResult resolveChannelTarget(const nlohmann::json& _jsonTarget, AnimationChannelTarget& _target);
    ConversionResult resolveXMP(const nlohmann::json& _jsonXMP, Animation& _animation);
};
}
