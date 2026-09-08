// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"
#include "math/VectorTypes.h"

#include <optional>

//See https://github.com/ux3d/glTF/tree/KHR_materials_pbrClearcoat/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness#khr_materials_pbrspecularglossiness

namespace ux3d::gltf {
class Gltf;

class PbrSpecularGlossiness : public GltfObject {
private:
    std::optional<math::dvec4> m_DiffuseFactor;
    std::optional<math::dvec3> m_SpecularFactor;
    std::optional<double> m_GlossinessFactor;
    std::optional<TextureInfo> m_DiffuseTextureInfo;
    std::optional<TextureInfo> m_SpecularGlossinessTextureInfo;

public:
    GltfResult setDiffuseFactor(const math::dvec4 _Factor);
    GltfResult setSpecularFactor(const math::dvec3 _Factor);
    GltfResult setGlossinessFactor(double _Factor);

    void unsetDiffuseFactor() { m_DiffuseFactor.reset(); }
    void unsetSpecularFactor() { m_SpecularFactor.reset(); }
    void unsetGlossinessFactor() { m_GlossinessFactor.reset(); }

    bool hasDiffuseFactor() const { return m_DiffuseFactor.has_value(); }
    bool hasSpecularFactor() const { return m_SpecularFactor.has_value(); }
    bool hasGlossinessFactor() const { return m_GlossinessFactor.has_value(); }

    math::dvec4 getDiffuseFactorOrDefault() const { return m_DiffuseFactor.value_or(math::dvec4(1)); }
    math::dvec3 getSpecularFactorOrDefault() const { return m_SpecularFactor.value_or(math::dvec3(1)); }
    double getGlossinessFactorOrDefault() const { return m_GlossinessFactor.value_or(1); }

    GltfResult createDiffuseTextureInfo();
    void destroyDiffuseTextureInfo() { m_DiffuseTextureInfo.reset(); }
    GltfResult createSpecularGlossinessTextureInfo();
    void destroySpecularGlossinessTextureInfo() { m_SpecularGlossinessTextureInfo.reset(); }

    const std::optional<TextureInfo>& getDiffuseTextureInfo() const { return m_DiffuseTextureInfo; }
    std::optional<TextureInfo>& getDiffuseTextureInfo() { return m_DiffuseTextureInfo; }
    const std::optional<TextureInfo>& getSpecularGlossinessTextureInfo() const { return m_SpecularGlossinessTextureInfo; }
    std::optional<TextureInfo>& getSpecularGlossinessTextureInfo() { return m_SpecularGlossinessTextureInfo; }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const;
};
}
