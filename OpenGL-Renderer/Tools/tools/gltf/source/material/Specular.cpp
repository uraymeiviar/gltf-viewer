// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/material/Specular.h"
#include "gltf/Texture.h"

using namespace ux3d;
using namespace ux3d::gltf;

GltfResult Specular::setSpecularColorFactor(const math::dvec3& _factors)
{
    if (_factors.r < 0 || _factors.g < 0 || _factors.b < 0) {
        // The specular color factor is allowed to be set to values greater than [1, 1, 1].
        return GltfError::InvalidInput;
    }
    m_specularColorFactor = _factors;
    return GltfResult::Success;
}

GltfResult Specular::setSpecularFactor(const double& _factor)
{
    if (_factor < 0 || _factor > 1) {
        return GltfError::InvalidInput;
    }
    m_specularFactor = _factor;
    return GltfResult::Success;
}

GltfResult Specular::createSpecularTextureInfo()
{
    if (m_specularTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }

    m_specularTextureInfo.emplace();
    return GltfResult::Success;
}

GltfResult Specular::createSpecularColorTextureInfo()
{
    if (m_specularColorTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }

    m_specularColorTextureInfo.emplace();
    return GltfResult::Success;
}

bool Specular::isValid() const
{
    return true;
}

GltfResult Specular::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
