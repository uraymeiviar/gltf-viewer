// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/CameraOrthographic.h"

using ux3d::gltf::CameraOrthographic;

ux3d::gltf::GltfResult CameraOrthographic::setXMag(const double xmag)
{
    if (xmag == 0.0) {
        return GltfError::InvalidInput;
    }

    m_xmag = xmag;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult CameraOrthographic::setYMag(const double ymag)
{
    if (ymag == 0.0) {
        return GltfError::InvalidInput;
    }

    m_ymag = ymag;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult CameraOrthographic::setZNear(const double znear)
{
    if (znear < 0.0 || znear >= m_zfar) {
        return GltfError::InvalidInput;
    }

    m_znear = znear;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult CameraOrthographic::setZFar(const double zfar)
{
    if (zfar <= 0.0 || zfar <= m_znear) {
        return GltfError::InvalidInput;
    }

    m_zfar = zfar;
    return GltfResult::Success;
}
