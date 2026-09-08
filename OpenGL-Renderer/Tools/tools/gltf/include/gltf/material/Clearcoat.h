// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"
#include "math/VectorTypes.h"

#include <optional>

//See https://github.com/ux3d/glTF/tree/KHR_materials_pbrClearcoat/extensions/2.0/Khronos/KHR_materials_clearcoat#khr_materials_clearcoat

namespace ux3d::gltf {
class Gltf;

class Clearcoat : public GltfObject {
private:
    std::optional<double> m_ClearcoatFactor;
    std::optional<TextureInfo> m_ClearcoatTexture;
    std::optional<double> m_ClearcoatRoughnessFactor;
    std::optional<TextureInfo> m_ClearcoatRoughnessTexture;
    std::optional<TextureInfo> m_ClearcoatNormalTexture;

public:
    GltfResult setClearcoatFactor(double _Factor);
    GltfResult setClearcoatRoughnessFactor(double _Factor);

    bool hasClearcoatFactor() const { return m_ClearcoatFactor.has_value(); }
    bool hasClearcoatRoughnessFactor() const { return m_ClearcoatRoughnessFactor.has_value(); }

    double getClearcoatFactorOrDefault() const { return m_ClearcoatFactor.value_or(0); }
    double getClearcoatRoughnessFactorOrDefault() const { return m_ClearcoatRoughnessFactor.value_or(0); }

    GltfResult createClearcoatTexture();
    GltfResult createClearcoatRoughnessTexture();
    GltfResult createClearcoatNormalTexture();

    std::optional<TextureInfo>& getClearcoatTexture() { return m_ClearcoatTexture; }
    const std::optional<TextureInfo>& getClearcoatTexture() const { return m_ClearcoatTexture; }

    std::optional<TextureInfo>& getClearcoatRoughnessTexture() { return m_ClearcoatRoughnessTexture; }
    const std::optional<TextureInfo>& getClearcoatRoughnessTexture() const { return m_ClearcoatRoughnessTexture; }

    std::optional<TextureInfo>& getClearcoatNormalTexture() { return m_ClearcoatNormalTexture; }
    const std::optional<TextureInfo>& getClearcoatNormalTexture() const { return m_ClearcoatNormalTexture; }

    void unsetClearcoatTexture() { m_ClearcoatTexture.reset(); }
    void unsetClearcoatRoughnessTexture() { m_ClearcoatRoughnessTexture.reset(); }
    void unsetClearcoatNormalTexture() { m_ClearcoatNormalTexture.reset(); }
    void unsetClearcoatFactor() { m_ClearcoatFactor.reset(); }
    void unsetClearcoatRoughnessFactor() { m_ClearcoatRoughnessFactor.reset(); }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const;
};
}
