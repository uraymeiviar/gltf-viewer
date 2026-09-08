// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/material/Sheen.h"
#include "gltf/Texture.h"

using namespace ux3d;
using namespace ux3d::gltf;

GltfResult Sheen::setSheenColorFactor(const math::dvec3& _factors)
{
    if (_factors.r < 0 || _factors.r > 1 || _factors.g < 0 || _factors.g > 1 || _factors.b < 0 || _factors.b > 1) {
        return GltfError::InvalidInput;
    }
    m_sheenColorFactor = _factors;
    return GltfResult::Success;
}

GltfResult Sheen::createSheenColorTextureInfo()
{
    if (m_sheenColorTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }

    m_sheenColorTextureInfo.emplace();
    return GltfResult::Success;
}

GltfResult Sheen::setSheenRoughnessFactor(const double& _factor)
{
    if (_factor < 0 || _factor > 1) {
        return GltfError::InvalidInput;
    }
    m_sheenRoughnessFactor = _factor;
    return GltfResult::Success;
}

GltfResult Sheen::createSheenRoughnessTextureInfo()
{
    if (m_sheenRoughnessTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }

    m_sheenRoughnessTextureInfo.emplace();
    return GltfResult::Success;
}

bool Sheen::isValid() const
{
    return true;
}

GltfResult Sheen::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
