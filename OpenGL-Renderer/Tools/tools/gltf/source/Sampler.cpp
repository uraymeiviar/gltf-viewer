// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Sampler.h"

ux3d::gltf::Sampler::Sampler(uint32_t _id)
    : ChildOfRoot(_id)
{
}

ux3d::gltf::Sampler::Sampler(uint32_t _id, const Sampler& _source)
    : ChildOfRoot(_id, _source)
    , m_MagFilter(_source.m_MagFilter)
    , m_MinFilter(_source.m_MinFilter)
    , m_WrapS(_source.m_WrapS)
    , m_WrapT(_source.m_WrapT)
{
}

ux3d::gltf::GltfResult ux3d::gltf::Sampler::setMagFilter(TextureFilterType _MagFilter)
{
    m_MagFilter = _MagFilter;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Sampler::setMinFilter(TextureFilterType _MinFilter)
{
    m_MinFilter = _MinFilter;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Sampler::setWrapS(TextureWrappingMode _WrapS)
{
    m_WrapS = _WrapS;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Sampler::setWrapT(TextureWrappingMode _WrapT)
{
    m_WrapT = _WrapT;
    return GltfResult::Success;
}

bool ux3d::gltf::Sampler::isValid() const
{
    return true;
}

ux3d::gltf::GltfResult ux3d::gltf::Sampler::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
