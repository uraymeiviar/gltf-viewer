// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "AnimationChannelTarget.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"

#include "core/String.h"
#include "core/Vector.h"

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#channel

namespace ux3d::gltf {
class Animation;
class AnimationSampler;

class AnimationChannel : public GltfObject {
private:
    AnimationSampler* m_pSampler = nullptr;
    AnimationChannelTarget m_Target;

public:
    const Animation& m_RootAnimation;

    AnimationChannel(const AnimationChannel&) = delete;

    AnimationChannel(AnimationChannel&&) = default;

    explicit AnimationChannel(const Animation& _RootAnimation)
        : m_Target(_RootAnimation)
        , m_RootAnimation(_RootAnimation)
    {
    }

    AnimationChannel(const Animation& _rootAnimation, const AnimationChannel& _source)
        : GltfObject(_source)
        , m_Target(_rootAnimation, _source.m_Target)
        , m_RootAnimation(_rootAnimation)
    {
    }

    virtual ~AnimationChannel() = default;

    AnimationSampler* getSampler() { return m_pSampler; }
    const AnimationSampler* getSampler() const { return m_pSampler; }

    AnimationChannelTarget& getTarget() { return m_Target; }
    const AnimationChannelTarget& getTarget() const { return m_Target; }

    GltfResult setSampler(AnimationSampler& _Sampler);

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const;
};
}
