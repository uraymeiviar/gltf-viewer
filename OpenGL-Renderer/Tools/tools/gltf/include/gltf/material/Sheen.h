// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"
#include "math/VectorTypes.h"

#include <optional>

//See https://github.com/sebavan/glTF/tree/KHR_materials_sheen/extensions/2.0/Khronos/KHR_materials_sheen

namespace ux3d::gltf {
class Gltf;

class Sheen : public GltfObject {
private:
    std::optional<math::dvec3> m_sheenColorFactor;
    std::optional<TextureInfo> m_sheenColorTextureInfo;
    std::optional<double> m_sheenRoughnessFactor;
    std::optional<TextureInfo> m_sheenRoughnessTextureInfo;

public:
    GltfResult setSheenRoughnessFactor(const double& _factor);
    GltfResult setSheenColorFactor(const math::dvec3& _factors);

    GltfResult createSheenColorTextureInfo();
    void destroySheenColorTextureInfo() { m_sheenColorTextureInfo.reset(); }

    GltfResult createSheenRoughnessTextureInfo();
    void destroySheenRoughnessTextureInfo() { m_sheenRoughnessTextureInfo.reset(); }

    bool hasSheenColorFactor() const { return m_sheenColorFactor.has_value(); }
    bool hasSheenColorTextureInfo() const { return m_sheenColorTextureInfo.has_value(); }
    bool hasSheenRoughnessFactor() const { return m_sheenRoughnessFactor.has_value(); }
    bool hasSheenRoughnessTextureInfo() const { return m_sheenRoughnessTextureInfo.has_value(); }

    void unsetSheenRoughnessFactor() { m_sheenRoughnessFactor.reset(); }
    void unsetSheenColorFactor() { m_sheenColorFactor.reset(); }

    math::dvec3 getSheenColorFactorOrDefault() const { return m_sheenColorFactor.value_or(math::vec3({ 0, 0, 0 })); }
    const std::optional<TextureInfo>& getSheenColorTextureInfo() const { return m_sheenColorTextureInfo; }
    std::optional<TextureInfo>& getSheenColorTextureInfo() { return m_sheenColorTextureInfo; }
    double getSheenRoughnessFactorOrDefault() const { return m_sheenRoughnessFactor.value_or(0); }
    const std::optional<TextureInfo>& getSheenRoughnessTextureInfo() const { return m_sheenRoughnessTextureInfo; }
    std::optional<TextureInfo>& getSheenRoughnessTextureInfo() { return m_sheenRoughnessTextureInfo; }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};
}
