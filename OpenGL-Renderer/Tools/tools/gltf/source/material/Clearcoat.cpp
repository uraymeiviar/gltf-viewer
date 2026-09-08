// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/material/Clearcoat.h"

#include "gltf/Texture.h"

ux3d::gltf::GltfResult ux3d::gltf::Clearcoat::setClearcoatFactor(double _Factor)
{
    if (_Factor < 0 || _Factor > 1) {
        return GltfError::InvalidInput;
    }
    m_ClearcoatFactor = _Factor;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Clearcoat::setClearcoatRoughnessFactor(double _Factor)
{
    if (_Factor < 0 || _Factor > 1) {
        return GltfError::InvalidInput;
    }
    m_ClearcoatRoughnessFactor = _Factor;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Clearcoat::createClearcoatTexture()
{
    if (!m_ClearcoatTexture) {
        m_ClearcoatTexture.emplace();
        return GltfResult::Success;
    }
    return GltfError::ObjectAlreadyExists;
}

ux3d::gltf::GltfResult ux3d::gltf::Clearcoat::createClearcoatRoughnessTexture()
{
    if (!m_ClearcoatRoughnessTexture) {
        m_ClearcoatRoughnessTexture.emplace();
        return GltfResult::Success;
    }
    return GltfError::ObjectAlreadyExists;
}

ux3d::gltf::GltfResult ux3d::gltf::Clearcoat::createClearcoatNormalTexture()
{
    if (!m_ClearcoatNormalTexture) {
        m_ClearcoatNormalTexture.emplace();
        return GltfResult::Success;
    }
    return GltfError::ObjectAlreadyExists;
}

bool ux3d::gltf::Clearcoat::isValid() const
{
    return true;
}

ux3d::gltf::GltfResult ux3d::gltf::Clearcoat::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
