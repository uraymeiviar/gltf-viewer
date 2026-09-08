// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "GltfObject.h"
#include "GltfResult.h"
#include <optional>

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#perspective

namespace ux3d::gltf {
class CameraPerspective : public GltfObject {
private:
    std::optional<double> m_AspectRatio; // If this is undefined, the aspect ratio of the canvas is used.
    double m_yFov = 0.8; // ~45 degrees
    std::optional<double> m_ZFar; //If this is undefined, runtime must use infinite projection matrix
    double m_ZNear = 0.001;

public:
    // "When this is undefined, the aspect ratio of the canvas is used."
    GltfResult setAspectRatio(double _AspectRatio);
    const std::optional<double>& getAspectRatio() const { return m_AspectRatio; }
    void unsetAspectRatio() { m_AspectRatio.reset(); }

    GltfResult setYFov(double _yFov);
    double getYFov() const { return m_yFov; }

    GltfResult setZNear(double _ZNear);
    double getZNear() const { return m_ZNear; }

    GltfResult setZFar(double _ZFar);
    const std::optional<double>& getZFar() const { return m_ZFar; }
    void unsetZFar() { m_ZFar.reset(); }
};
}
