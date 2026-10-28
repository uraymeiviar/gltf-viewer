// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "gltf/material/Anisotropy.h"
#include "gltf/Texture.h"

#include <cmath>

using namespace ux3d;
using namespace ux3d::gltf;

GltfResult Anisotropy::setAnisotropyStrength(double _value)
{
    if (!(_value >= 0.0 && _value <= 1.0)) {
        return GltfError::InvalidInput;
    }
    m_anisotropyStrength = _value;
    return GltfResult::Success;
}

GltfResult Anisotropy::setAnisotropyRotation(double _value)
{
    if (!std::isfinite(_value)) {
        return GltfError::InvalidInput;
    }
    m_anisotropyRotation = _value;
    return GltfResult::Success;
}

GltfResult Anisotropy::createAnisotropyTextureInfo()
{
    if (m_anisotropyTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }
    m_anisotropyTextureInfo.emplace();
    return GltfResult::Success;
}

bool Anisotropy::isValid() const
{
    return true;
}

GltfResult Anisotropy::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
