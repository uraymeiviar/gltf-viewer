// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"
#include "math/VectorTypes.h"

#include <optional>

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#pbrmetallicroughness

namespace ux3d::gltf {
class Gltf;

class PbrMetallicRoughness : public GltfObject {
private:
    std::optional<math::dvec4> m_BaseColorFactor;
    std::optional<TextureInfo> m_BaseColorTextureInfo;
    std::optional<TextureInfo> m_MetallicRoughnessTextureInfo;
    std::optional<double> m_MetallicFactor;
    std::optional<double> m_RoughnessFactor;

public:
    GltfResult setMetallicFactor(double _Factor);
    GltfResult setRoughnessFactor(double _Factor);
    GltfResult setBaseColorFactor(const math::dvec4& _Color);

    void unsetMetallicFactor() { m_MetallicFactor.reset(); }
    void unsetRoughnessFactor() { m_RoughnessFactor.reset(); }
    void unsetBaseColorFactor() { m_BaseColorFactor.reset(); }

    bool hasMetallicFactor() const { return m_MetallicFactor.has_value(); }
    bool hasRoughnessFactor() const { return m_RoughnessFactor.has_value(); }
    bool hasBaseColorFactor() const { return m_BaseColorFactor.has_value(); }

    double getMetallicFactorOrDefault() const { return m_MetallicFactor.value_or(1); }
    double getRoughnessFactorOrDefault() const { return m_RoughnessFactor.value_or(1); }
    math::dvec4 getBaseColorFactorOrDefault() const { return m_BaseColorFactor.value_or(math::dvec4(1)); }

    GltfResult createBaseColorTextureInfo();
    void destroyBaseColorTextureInfo() { m_BaseColorTextureInfo.reset(); }
    GltfResult createMetallicRoughnessTextureInfo();
    void destroyMetallicRoughnessTextureInfo() { m_MetallicRoughnessTextureInfo.reset(); }

    std::optional<TextureInfo>& getBaseColorTextureInfo() { return m_BaseColorTextureInfo; }
    const std::optional<TextureInfo>& getBaseColorTextureInfo() const { return m_BaseColorTextureInfo; }

    std::optional<TextureInfo>& getMetallicRoughnessTextureInfo() { return m_MetallicRoughnessTextureInfo; }
    const std::optional<TextureInfo>& getMetallicRoughnessTextureInfo() const { return m_MetallicRoughnessTextureInfo; }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const;
};
}
