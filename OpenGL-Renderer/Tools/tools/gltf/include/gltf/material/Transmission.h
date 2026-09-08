// Copyright (c) 2019 UX3D GmbH. All rights reserved.
// Authors: David Labode

#pragma once

#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"
#include <optional>

namespace ux3d::gltf {
class Transmission : public GltfObject {
private:
    std::optional<double> m_transmissionFactor;
    std::optional<TextureInfo> m_transmissionTextureInfo;

public:
    GltfResult setTransmissionFactor(const double& _factor);

    GltfResult createTransmissionTextureInfo();
    void destroyTransmissionTextureInfo() { m_transmissionTextureInfo.reset(); }

    bool hasTransmissionFactor() const { return m_transmissionFactor.has_value(); }
    bool hasTransmissionTextureInfo() const { return m_transmissionTextureInfo.has_value(); }

    void unsetTransmissionFactor() { m_transmissionFactor.reset(); }

    const std::optional<TextureInfo>& getTransmissionTextureInfo() const { return m_transmissionTextureInfo; }
    std::optional<TextureInfo>& getTransmissionTextureInfo() { return m_transmissionTextureInfo; }
    double getTransmissionFactorOrDefault() const { return m_transmissionFactor.value_or(0); }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};
}
