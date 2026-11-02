// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"

#include <optional>

// KHR_materials_diffuse_roughness (Draft, KhronosGroup/glTF pull request 2481): the roughness of the diffuse substrate,
//   diffuseRoughness = diffuseRoughnessFactor (README default 0; the draft schema says 1, the README wins)
//                      x diffuseRoughnessTexture.r (linear)
// in [0, 1]. The renderer replaces the Lambert lobe of direct lights by the energy-preserving Oren-Nayar model (EON,
// Portsmouth et al. 2024, the model OpenPBR uses and the draft recommends); 0 is Lambert. Animatable through
// KHR_animation_pointer (FactorPointer).

namespace ux3d::gltf {
class Gltf;

class DiffuseRoughness : public GltfObject {
public:
    static constexpr const char* ExtensionName = "KHR_materials_diffuse_roughness";
    static constexpr const char* FactorPointer = "extensions/KHR_materials_diffuse_roughness/diffuseRoughnessFactor";
    static constexpr const char* TexturePointer = "extensions/KHR_materials_diffuse_roughness/diffuseRoughnessTexture";

    GltfResult setDiffuseRoughnessFactor(double value)
    {
        if (value < 0 || value > 1) {
            return GltfError::InvalidInput;
        }
        m_factor = value;
        return GltfResult::Success;
    }
    bool hasDiffuseRoughnessFactor() const { return m_factor.has_value(); }
    void unsetDiffuseRoughnessFactor() { m_factor.reset(); }
    double getDiffuseRoughnessFactorOrDefault() const { return m_factor.value_or(0.0); }

    GltfResult createDiffuseRoughnessTextureInfo()
    {
        if (m_texture) {
            return GltfError::ObjectAlreadyExists;
        }
        m_texture.emplace();
        return GltfResult::Success;
    }
    void destroyDiffuseRoughnessTextureInfo() { m_texture.reset(); }
    std::optional<TextureInfo>& getDiffuseRoughnessTextureInfo() { return m_texture; }
    const std::optional<TextureInfo>& getDiffuseRoughnessTextureInfo() const { return m_texture; }

    bool isValid() const { return true; }
    GltfResult getInvalidProperties(core::Vector<core::string>&) const { return GltfResult::Success; }

private:
    std::optional<double> m_factor;
    std::optional<TextureInfo> m_texture;
};

}
