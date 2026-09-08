// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/material/Volume.h"
#include "gltf/Texture.h"

using namespace ux3d;
using namespace ux3d::gltf;

GltfResult Volume::setAttenuationColor(const math::dvec3& _factors)
{
    if (_factors.r < 0 || _factors.r > 1 || _factors.g < 0 || _factors.g > 1 || _factors.b < 0 || _factors.b > 1) {
        return GltfError::InvalidInput;
    }
    m_attenuationColor = _factors;
    return GltfResult::Success;
}

GltfResult Volume::setAttenuationDistance(const double& _factor)
{
    if (_factor <= 0) {
        return GltfError::InvalidInput;
    }
    m_attenuationDistance = _factor;
    return GltfResult::Success;
}

GltfResult Volume::setThicknessFactor(const double& _factor)
{
    if (_factor < 0) {
        return GltfError::InvalidInput;
    }
    m_thicknessFactor = _factor;
    return GltfResult::Success;
}

GltfResult Volume::createThicknessTextureInfo()
{
    if (m_thicknessTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }

    m_thicknessTextureInfo.emplace();
    return GltfResult::Success;
}

bool Volume::isValid() const
{
    return true;
}

GltfResult Volume::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
