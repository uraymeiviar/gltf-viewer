// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"
#include "math/VectorTypes.h"

#include <optional>

// KHR_materials_diffuse_transmission (https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/
// KHR_materials_diffuse_transmission, Release Candidate): the diffuse lobe of the dielectric BRDF becomes
// mix(diffuse_brdf(baseColor), diffuse_btdf(diffuseTransmissionColor), diffuseTransmission), with
//   diffuseTransmission      = diffuseTransmissionFactor (default 0) x diffuseTransmissionTexture.a (linear)
//   diffuseTransmissionColor = diffuseTransmissionColorFactor (default 1,1,1) x diffuseTransmissionColorTexture.rgb (sRGB)
// The BTDF transmits light arriving from the other hemisphere (Lambert). Both factors are animatable through
// KHR_animation_pointer (FactorPointer / ColorFactorPointer).

namespace ux3d::gltf {
class Gltf;

class DiffuseTransmission : public GltfObject {
public:
    static constexpr const char* ExtensionName = "KHR_materials_diffuse_transmission";
    static constexpr const char* FactorPointer = "extensions/KHR_materials_diffuse_transmission/diffuseTransmissionFactor";
    static constexpr const char* ColorFactorPointer = "extensions/KHR_materials_diffuse_transmission/diffuseTransmissionColorFactor";
    static constexpr const char* TexturePointer = "extensions/KHR_materials_diffuse_transmission/diffuseTransmissionTexture";
    static constexpr const char* ColorTexturePointer = "extensions/KHR_materials_diffuse_transmission/diffuseTransmissionColorTexture";

    GltfResult setDiffuseTransmissionFactor(double value)
    {
        if (value < 0 || value > 1) {
            return GltfError::InvalidInput;
        }
        m_factor = value;
        return GltfResult::Success;
    }
    GltfResult setDiffuseTransmissionColorFactor(const math::dvec3& value)
    {
        if (value.r < 0 || value.g < 0 || value.b < 0 || value.r > 1 || value.g > 1 || value.b > 1) {
            return GltfError::InvalidInput;
        }
        m_colorFactor = value;
        return GltfResult::Success;
    }
    bool hasDiffuseTransmissionFactor() const { return m_factor.has_value(); }
    bool hasDiffuseTransmissionColorFactor() const { return m_colorFactor.has_value(); }
    void unsetDiffuseTransmissionFactor() { m_factor.reset(); }
    void unsetDiffuseTransmissionColorFactor() { m_colorFactor.reset(); }
    double getDiffuseTransmissionFactorOrDefault() const { return m_factor.value_or(0.0); }
    math::dvec3 getDiffuseTransmissionColorFactorOrDefault() const { return m_colorFactor.value_or(math::dvec3(1.0, 1.0, 1.0)); }

    GltfResult createDiffuseTransmissionTextureInfo()
    {
        if (m_texture) {
            return GltfError::ObjectAlreadyExists;
        }
        m_texture.emplace();
        return GltfResult::Success;
    }
    GltfResult createDiffuseTransmissionColorTextureInfo()
    {
        if (m_colorTexture) {
            return GltfError::ObjectAlreadyExists;
        }
        m_colorTexture.emplace();
        return GltfResult::Success;
    }
    void destroyDiffuseTransmissionTextureInfo() { m_texture.reset(); }
    void destroyDiffuseTransmissionColorTextureInfo() { m_colorTexture.reset(); }
    std::optional<TextureInfo>& getDiffuseTransmissionTextureInfo() { return m_texture; }
    const std::optional<TextureInfo>& getDiffuseTransmissionTextureInfo() const { return m_texture; }
    std::optional<TextureInfo>& getDiffuseTransmissionColorTextureInfo() { return m_colorTexture; }
    const std::optional<TextureInfo>& getDiffuseTransmissionColorTextureInfo() const { return m_colorTexture; }

    bool isValid() const { return true; }
    GltfResult getInvalidProperties(core::Vector<core::string>&) const { return GltfResult::Success; }

private:
    std::optional<double> m_factor;
    std::optional<math::dvec3> m_colorFactor;
    std::optional<TextureInfo> m_texture;
    std::optional<TextureInfo> m_colorTexture;
};

}
