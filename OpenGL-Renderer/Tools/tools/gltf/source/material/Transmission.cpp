// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/material/Transmission.h"
#include "gltf/Texture.h"

using namespace ux3d;
using namespace ux3d::gltf;

GltfResult Transmission::setTransmissionFactor(const double& _factor)
{
    if (_factor < 0 || _factor > 1) {
        return GltfError::InvalidInput;
    }
    m_transmissionFactor = _factor;
    return GltfResult::Success;
}

GltfResult Transmission::createTransmissionTextureInfo()
{
    if (m_transmissionTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }

    m_transmissionTextureInfo.emplace();
    return GltfResult::Success;
}

bool Transmission::isValid() const
{
    return true;
}

GltfResult Transmission::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
