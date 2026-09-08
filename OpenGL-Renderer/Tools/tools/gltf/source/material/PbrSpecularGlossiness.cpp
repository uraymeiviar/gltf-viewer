// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/material/PbrSpecularGlossiness.h"

#include "gltf/Texture.h"

ux3d::gltf::GltfResult ux3d::gltf::PbrSpecularGlossiness::setDiffuseFactor(const math::dvec4 _Factor)
{
    for (int i = 0; i < _Factor.length(); ++i) {
        if (_Factor[i] < 0 || _Factor[i] > 1) {
            return GltfError::InvalidInput;
        }
    }
    m_DiffuseFactor = _Factor;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::PbrSpecularGlossiness::setSpecularFactor(const math::dvec3 _Factor)
{
    for (int i = 0; i < _Factor.length(); ++i) {
        if (_Factor[i] < 0 || _Factor[i] > 1) {
            return GltfError::InvalidInput;
        }
    }
    m_SpecularFactor = _Factor;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::PbrSpecularGlossiness::setGlossinessFactor(double _Factor)
{
    if (_Factor < 0 || _Factor > 1) {
        return GltfError::InvalidInput;
    }
    m_GlossinessFactor = _Factor;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::PbrSpecularGlossiness::createDiffuseTextureInfo()
{
    if (m_DiffuseTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }

    m_DiffuseTextureInfo.emplace();
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::PbrSpecularGlossiness::createSpecularGlossinessTextureInfo()
{
    if (m_SpecularGlossinessTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }

    m_SpecularGlossinessTextureInfo.emplace();
    return GltfResult::Success;
}

bool ux3d::gltf::PbrSpecularGlossiness::isValid() const
{
    return true;
}

ux3d::gltf::GltfResult ux3d::gltf::PbrSpecularGlossiness::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
