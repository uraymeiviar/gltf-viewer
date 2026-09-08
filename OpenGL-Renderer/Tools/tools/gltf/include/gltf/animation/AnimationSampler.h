// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "AnimationInterpolation.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"

#include "core/LinkedList.h"
#include "core/String.h"
#include "core/Vector.h"

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#animation-sampler

namespace ux3d::gltf {
class Animation;
class Accessor;
class AnimationChannel;

class AnimationSampler : public GltfObject {
private:
    AnimationInterpolation m_Interpolation = AnimationInterpolation::Linear;
    Accessor* m_pInputAccessor = nullptr;
    Accessor* m_pOutputAccessor = nullptr;

public:
    const Animation& m_RootAnimation;

    AnimationSampler(const AnimationSampler&) = delete;

    AnimationSampler(AnimationSampler&&) = default;
    virtual ~AnimationSampler() = default;

    explicit AnimationSampler(const Animation& _RootAnimation)
        : m_RootAnimation(_RootAnimation)
    {
    }

    AnimationSampler(const Animation& _rootAnimation, const AnimationSampler& _source)
        : GltfObject(_source)
        , m_Interpolation(_source.m_Interpolation)
        , m_RootAnimation(_rootAnimation)
    {
    }

    AnimationInterpolation getInterpolation() const { return m_Interpolation; }

    Accessor* getInputAccessor() { return m_pInputAccessor; }
    const Accessor* getInputAccessor() const { return m_pInputAccessor; }

    Accessor* getOutputAccessor() { return m_pOutputAccessor; }
    const Accessor* getOutputAccessor() const { return m_pOutputAccessor; }

    const core::LinkedList<ux3d::gltf::AnimationChannel>& getChannels() const;

    GltfResult setInterpolation(AnimationInterpolation _Interpolation);
    GltfResult setInputAccessor(Accessor& _Accessor);
    GltfResult setOutputAccessor(Accessor& _Accessor);

    void unsetInputAccessor() { m_pInputAccessor = nullptr; }
    void unsetOutputAccessor() { m_pOutputAccessor = nullptr; }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const;
};
}
