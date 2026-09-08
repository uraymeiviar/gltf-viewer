// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"
#include "math/VectorTypes.h"

#include <optional>

//See https://github.com/DassaultSystemes-Technology/glTF/tree/KHR_materials_volume/extensions/2.0/Khronos/KHR_materials_volume

namespace ux3d::gltf {
class Gltf;

class Volume : public GltfObject {
private:
    std::optional<double> m_thicknessFactor;
    std::optional<TextureInfo> m_thicknessTextureInfo;
    std::optional<double> m_attenuationDistance;
    std::optional<math::dvec3> m_attenuationColor;

public:
    bool hasThicknessFactor() const { return m_thicknessFactor.has_value(); }
    bool hasThicknessTextureInfo() const { return m_thicknessTextureInfo.has_value(); }
    bool hasAttenuationDistance() const { return m_attenuationDistance.has_value(); }
    bool hasAttenuationColor() const { return m_attenuationColor.has_value(); }

    GltfResult createThicknessTextureInfo();
    void destroyThicknessTextureInfo() { m_thicknessTextureInfo.reset(); }

    GltfResult setThicknessFactor(const double& _factor);
    GltfResult setAttenuationDistance(const double& _factor);
    GltfResult setAttenuationColor(const math::dvec3& _factors);

    void unsetThicknessFactor() { m_thicknessFactor.reset(); }
    void unsetAttenuationDistance() { m_attenuationDistance.reset(); }
    void unsetAttenuationColor() { m_attenuationColor.reset(); }

    const std::optional<TextureInfo>& getThicknessTextureInfo() const { return m_thicknessTextureInfo; }
    std::optional<TextureInfo>& getThicknessTextureInfo() { return m_thicknessTextureInfo; }
    double getThicknessFactorOrDefault() const { return m_thicknessFactor.value_or(0); }
    std::optional<double> getAttenuationDistance() const { return m_attenuationDistance; }
    math::dvec3 getAttenuationColorOrDefault() const { return m_attenuationColor.value_or(math::vec3({ 1, 1, 1 })); }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};

}
