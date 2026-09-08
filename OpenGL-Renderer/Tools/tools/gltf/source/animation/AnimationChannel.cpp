// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/animation/AnimationChannel.h"
#include "gltf/animation/AnimationSampler.h"

ux3d::gltf::GltfResult ux3d::gltf::AnimationChannel::setSampler(AnimationSampler& _Sampler)
{
    m_pSampler = &_Sampler;
    return GltfResult::Success;
}

bool ux3d::gltf::AnimationChannel::isValid() const
{
    return m_pSampler == nullptr;
}

ux3d::gltf::GltfResult ux3d::gltf::AnimationChannel::getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const
{
    if (m_pSampler == nullptr) {
        if (_InvalidProperties.push_back("sampler") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    return GltfResult::Success;
}
