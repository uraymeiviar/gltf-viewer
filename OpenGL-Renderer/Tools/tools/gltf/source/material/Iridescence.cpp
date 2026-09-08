// Copyright (c) 2021 UX3D GmbH. All rights reserved.

#include "gltf/material/Iridescence.h"
#include "gltf/Texture.h"

using namespace ux3d;
using namespace ux3d::gltf;

// values:

GltfResult Iridescence::setIridescenceFactor(double _value)
{
    if (_value < 0.0 || _value > 1.0) {
        return GltfError::InvalidInput;
    }
    m_iridescenceFactor = _value;
    return GltfResult::Success;
}

GltfResult Iridescence::setIridescenceIor(double _value)
{
    if (_value < 1.0) {
        return GltfError::InvalidInput;
    }
    m_iridescenceIor = _value;
    return GltfResult::Success;
}

GltfResult Iridescence::setIridescenceThicknessMinimum(double _value, bool _ignoreMaximum)
{
    if (_value < 0.0) {
        return GltfError::InvalidInput;
    }
    if (_value > getIridescenceThicknessMaximumOrDefault() && !_ignoreMaximum) {
        return GltfError::InvalidInput;
    }
    m_iridescenceThicknessMinimum = _value;
    return GltfResult::Success;
}

GltfResult Iridescence::setIridescenceThicknessMaximum(double _value, bool _ignoreMinimum)
{
    if (_value < 0.0) {
        return GltfError::InvalidInput;
    }
    if (_value < getIridescenceThicknessMinimumOrDefault() && !_ignoreMinimum) {
        return GltfError::InvalidInput;
    }
    m_iridescenceThicknessMaximum = _value;
    return GltfResult::Success;
}

// textures:

GltfResult Iridescence::createIridescenceTextureInfo()
{
    if (m_iridescenceTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }

    m_iridescenceTextureInfo.emplace();
    return GltfResult::Success;
}

GltfResult Iridescence::createIridescenceThicknessTextureInfo()
{
    if (m_iridescenceThicknessTextureInfo) {
        return GltfError::ObjectAlreadyExists;
    }

    m_iridescenceThicknessTextureInfo.emplace();
    return GltfResult::Success;
}

// general:

bool Iridescence::isValid() const
{
    if (getIridescenceThicknessMinimumOrDefault() > getIridescenceThicknessMaximumOrDefault()) {
        return false;
    }
    return true;
}

GltfResult Iridescence::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
