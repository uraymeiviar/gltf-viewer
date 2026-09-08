// Copyright (c) 2021 UX3D GmbH. All rights reserved.

#pragma once

#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/TextureInfo.h"

#include <optional>

//See https://github.com/ux3d/glTF/tree/extensions/KHR_materials_iridescence/extensions/2.0/Khronos/KHR_materials_iridescence

namespace ux3d::gltf {
class Gltf;

class Iridescence : public GltfObject {
private:
    std::optional<double> m_iridescenceFactor;
    std::optional<TextureInfo> m_iridescenceTextureInfo;
    std::optional<double> m_iridescenceIor;
    std::optional<double> m_iridescenceThicknessMinimum;
    std::optional<double> m_iridescenceThicknessMaximum;
    std::optional<TextureInfo> m_iridescenceThicknessTextureInfo;

public:
    bool hasIridescenceFactor() const { return m_iridescenceFactor.has_value(); }
    bool hasIridescenceTextureInfo() const { return m_iridescenceTextureInfo.has_value(); }
    bool hasIridescenceIor() const { return m_iridescenceIor.has_value(); }
    bool hasIridescenceThicknessMinimum() const { return m_iridescenceThicknessMinimum.has_value(); }
    bool hasIridescenceThicknessMaximum() const { return m_iridescenceThicknessMaximum.has_value(); }
    bool hasIridescenceThicknessTextureInfo() const { return m_iridescenceThicknessTextureInfo.has_value(); }

    // create/destroy textures:
    GltfResult createIridescenceTextureInfo();
    GltfResult createIridescenceThicknessTextureInfo();
    void destroyIridescenceTextureInfo() { m_iridescenceTextureInfo.reset(); }
    void destroyIridescenceThicknessTextureInfo() { m_iridescenceThicknessTextureInfo.reset(); }

    // set/unset values:
    GltfResult setIridescenceFactor(double _value);
    GltfResult setIridescenceIor(double _value);
    GltfResult setIridescenceThicknessMinimum(double _value, bool _ignoreMaximum = false);
    GltfResult setIridescenceThicknessMaximum(double _value, bool _ignoreMinimum = false);

    void unsetIridescenceFactor() { m_iridescenceFactor.reset(); }
    void unsetIridescenceIor() { m_iridescenceIor.reset(); }
    void unsetIridescenceThicknessMinimum() { m_iridescenceThicknessMinimum.reset(); }
    void unsetIridescenceThicknessMaximum() { m_iridescenceThicknessMaximum.reset(); }

    // get textures:
    std::optional<TextureInfo>& getIridescenceTextureInfo() { return m_iridescenceTextureInfo; }
    std::optional<TextureInfo>& getIridescenceThicknessTextureInfo() { return m_iridescenceThicknessTextureInfo; }
    const std::optional<TextureInfo>& getIridescenceTextureInfo() const { return m_iridescenceTextureInfo; }
    const std::optional<TextureInfo>& getIridescenceThicknessTextureInfo() const { return m_iridescenceThicknessTextureInfo; }

    // get values:
    double getIridescenceFactorOrDefault() const { return m_iridescenceFactor.value_or(0.0); };
    double getIridescenceIorOrDefault() const { return m_iridescenceIor.value_or(1.3); };
    double getIridescenceThicknessMinimumOrDefault() const { return m_iridescenceThicknessMinimum.value_or(100.0); };
    double getIridescenceThicknessMaximumOrDefault() const { return m_iridescenceThicknessMaximum.value_or(400.0); };

    // general
    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};

}
