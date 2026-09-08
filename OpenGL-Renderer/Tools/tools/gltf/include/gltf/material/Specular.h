// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"
#include "math/VectorTypes.h"

#include <optional>

//See https://github.com/ux3d/glTF/tree/KHR_materials_pbrClearcoat/extensions/2.0/Khronos/KHR_materials_specular#khr_materials_specular

namespace ux3d::gltf {
class Gltf;

class Specular : public GltfObject {
private:
    std::optional<math::dvec3> m_specularColorFactor;
    std::optional<double> m_specularFactor;
    std::optional<TextureInfo> m_specularTextureInfo;
    std::optional<TextureInfo> m_specularColorTextureInfo;

public:
    GltfResult setSpecularFactor(const double& _factor);
    GltfResult setSpecularColorFactor(const math::dvec3& _factors);

    GltfResult createSpecularTextureInfo();
    void destroySpecularTextureInfo() { m_specularTextureInfo.reset(); }

    GltfResult createSpecularColorTextureInfo();
    void destroySpecularColorTextureInfo() { m_specularColorTextureInfo.reset(); }

    bool hasSpecularFactor() const { return m_specularFactor.has_value(); }
    bool hasSpecularColorFactor() const { return m_specularColorFactor.has_value(); }
    bool hasSpecularTextureInfo() const { return m_specularTextureInfo.has_value(); }
    bool hasSpecularColorTextureInfo() const { return m_specularColorTextureInfo.has_value(); }

    void unsetSpecularFactor() { m_specularFactor.reset(); }
    void unsetSpecularColorFactor() { m_specularColorFactor.reset(); }

    const std::optional<TextureInfo>& getSpecularTextureInfo() const { return m_specularTextureInfo; }
    std::optional<TextureInfo>& getSpecularTextureInfo() { return m_specularTextureInfo; }

    const std::optional<TextureInfo>& getSpecularColorTextureInfo() const { return m_specularColorTextureInfo; }
    std::optional<TextureInfo>& getSpecularColorTextureInfo() { return m_specularColorTextureInfo; }

    double getSpecularFactorOrDefault() const { return m_specularFactor.value_or(1); }
    math::dvec3 getSpecularColorFactorOrDefault() const { return m_specularColorFactor.value_or(math::vec3({ 1, 1, 1 })); }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};
}
