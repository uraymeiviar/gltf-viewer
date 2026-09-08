// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/material/PbrMetallicRoughness.h"

#include "gltf/Texture.h"

ux3d::gltf::GltfResult ux3d::gltf::PbrMetallicRoughness::setMetallicFactor(double _Factor)
{
    if (_Factor < 0 || _Factor > 1) {
        return GltfError::InvalidInput;
    }
    m_MetallicFactor = _Factor;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::PbrMetallicRoughness::setRoughnessFactor(double _Factor)
{
    if (_Factor < 0 || _Factor > 1) {
        return GltfError::InvalidInput;
    }
    m_RoughnessFactor = _Factor;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::PbrMetallicRoughness::setBaseColorFactor(const math::dvec4& _Color)
{
    for (int i = 0; i < _Color.length(); ++i) {
        if (_Color[i] < 0 || _Color[i] > 1) {
            return GltfError::InvalidInput;
        }
    }
    m_BaseColorFactor = _Color;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::PbrMetallicRoughness::createBaseColorTextureInfo()
{
    if (!m_BaseColorTextureInfo) {
        m_BaseColorTextureInfo.emplace();
        return GltfResult::Success;
    }
    return GltfError::ObjectAlreadyExists;
}

ux3d::gltf::GltfResult ux3d::gltf::PbrMetallicRoughness::createMetallicRoughnessTextureInfo()
{
    if (!m_MetallicRoughnessTextureInfo) {
        m_MetallicRoughnessTextureInfo.emplace();
        return GltfResult::Success;
    }
    return GltfError::ObjectAlreadyExists;
}

bool ux3d::gltf::PbrMetallicRoughness::isValid() const
{
    return true;
}

ux3d::gltf::GltfResult ux3d::gltf::PbrMetallicRoughness::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
