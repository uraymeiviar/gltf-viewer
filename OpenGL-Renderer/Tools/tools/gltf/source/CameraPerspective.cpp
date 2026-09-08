// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/CameraPerspective.h"

ux3d::gltf::GltfResult ux3d::gltf::CameraPerspective::setAspectRatio(const double _AspectRatio)
{
    if (_AspectRatio <= 0.f) {
        return GltfError::InvalidInput;
    }

    m_AspectRatio = _AspectRatio;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::CameraPerspective::setYFov(const double _yFov)
{
    if (_yFov <= 0) {
        return GltfError::InvalidInput;
    }

    m_yFov = _yFov;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::CameraPerspective::setZNear(const double _ZNear)
{
    if (_ZNear <= 0.f || (m_ZFar.has_value() && _ZNear >= m_ZFar.value())) {
        return GltfError::InvalidInput;
    }

    m_ZNear = _ZNear;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::CameraPerspective::setZFar(const double _ZFar)
{
    if (_ZFar <= 0.f || _ZFar <= m_ZNear) {
        return GltfError::InvalidInput;
    }

    m_ZFar = _ZFar;
    return GltfResult::Success;
}
