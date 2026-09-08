// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/animation/AnimationSampler.h"

#include "gltf/Accessor.h"
#include "gltf/Animation.h"

ux3d::gltf::GltfResult ux3d::gltf::AnimationSampler::setInterpolation(AnimationInterpolation _Interpolation)
{
    m_Interpolation = _Interpolation;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::AnimationSampler::setInputAccessor(Accessor& _Accessor)
{
    m_pInputAccessor = &_Accessor;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::AnimationSampler::setOutputAccessor(Accessor& _Accessor)
{
    m_pOutputAccessor = &_Accessor;
    return GltfResult::Success;
}

bool ux3d::gltf::AnimationSampler::isValid() const
{
    if (m_pInputAccessor == nullptr) {
        return false;
    }
    if (m_pOutputAccessor == nullptr) {
        return false;
    }
    return true;
}

ux3d::gltf::GltfResult ux3d::gltf::AnimationSampler::getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const
{
    if (m_pInputAccessor == nullptr) {
        if (_InvalidProperties.push_back("inputAccessor") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }

    if (m_pOutputAccessor == nullptr) {
        if (_InvalidProperties.push_back("outputAccessor") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    return GltfResult::Success;
}

const ux3d::core::LinkedList<ux3d::gltf::AnimationChannel>& ux3d::gltf::AnimationSampler::getChannels() const
{
    return m_RootAnimation.getChannels();
}
