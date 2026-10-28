// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"

#include <optional>

// See https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_anisotropy

namespace ux3d::gltf {

class Anisotropy : public GltfObject {
private:
    std::optional<double> m_anisotropyStrength;
    std::optional<double> m_anisotropyRotation;
    std::optional<TextureInfo> m_anisotropyTextureInfo;

public:
    bool hasAnisotropyStrength() const { return m_anisotropyStrength.has_value(); }
    bool hasAnisotropyRotation() const { return m_anisotropyRotation.has_value(); }
    bool hasAnisotropyTextureInfo() const { return m_anisotropyTextureInfo.has_value(); }

    GltfResult createAnisotropyTextureInfo();
    void destroyAnisotropyTextureInfo() { m_anisotropyTextureInfo.reset(); }

    // anisotropyStrength is in [0, 1]; anisotropyRotation is radians (any value).
    GltfResult setAnisotropyStrength(double _value);
    GltfResult setAnisotropyRotation(double _value);
    void unsetAnisotropyStrength() { m_anisotropyStrength.reset(); }
    void unsetAnisotropyRotation() { m_anisotropyRotation.reset(); }

    // RG: direction in tangent space ([0, 1] -> [-1, 1]); B: strength multiplier. Linear.
    std::optional<TextureInfo>& getAnisotropyTextureInfo() { return m_anisotropyTextureInfo; }
    const std::optional<TextureInfo>& getAnisotropyTextureInfo() const { return m_anisotropyTextureInfo; }

    double getAnisotropyStrengthOrDefault() const { return m_anisotropyStrength.value_or(0.0); }
    double getAnisotropyRotationOrDefault() const { return m_anisotropyRotation.value_or(0.0); }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};

}
