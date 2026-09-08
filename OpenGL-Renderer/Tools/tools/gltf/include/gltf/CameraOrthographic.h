// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "GltfObject.h"
#include "GltfResult.h"

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#orthographic

namespace ux3d::gltf {
class CameraOrthographic : public GltfObject {
private:
    double m_xmag = 1;
    double m_ymag = 1;
    double m_zfar = 1;
    double m_znear = 0;

public:
    explicit CameraOrthographic() = default;
    CameraOrthographic(const CameraOrthographic&) = default;
    CameraOrthographic(CameraOrthographic&&) = default;

    GltfResult setXMag(double xmag);
    double getXMag() const { return m_xmag; }

    GltfResult setYMag(double ymag);
    double getYMag() const { return m_ymag; }

    GltfResult setZFar(double zfar);
    double getZFar() const { return m_zfar; }

    GltfResult setZNear(double znear);
    double getZNear() const { return m_znear; }
};
}
