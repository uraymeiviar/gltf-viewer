// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "GltfObject.h"
#include "GltfResult.h"
#include "core/String.h"
#include "core/Vector.h"
#include "gltf/TextureTransformExtension.h"
#include <cstdint>
#include <optional>

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#textureinfo

namespace ux3d::gltf {
class Texture;
class Gltf;

class TextureInfo : public GltfObject {
public:
    explicit TextureInfo() = default;
    TextureInfo(const TextureInfo&) = default;
    TextureInfo(TextureInfo&&) = default;

    const std::optional<uint32_t>& getTexCoord() const { return m_texCoord; }
    bool hasTexCoord() const { return m_texCoord.has_value(); }
    uint32_t getTexCoordOrDefault() const { return m_texCoord.value_or(0u); }
    GltfResult setTexCoord(uint32_t _texCoord);
    void unsetTexCoord() { m_texCoord.reset(); }

    double getScale() const { return m_scale.value_or(1.0f); }
    GltfResult setScale(double _scale);

    double getStrength() const { return m_scale.value_or(1.0f); }
    GltfResult setStrength(double _strength);

    bool hasScaleOrStrength() const { return m_scale.has_value(); }
    void unsetScaleOrStrength() { m_scale.reset(); }

    Texture* getTexture() { return m_texture; }
    const Texture* getTexture() const { return m_texture; }
    GltfResult setTexture(Texture& _texture);
    void unsetTexture() { m_texture = nullptr; }

    uint32_t getEffectiveTexCoord() const;
    double getEffectiveUVRotation() const;
    math::dvec2 getEffectiveUVOffset() const;
    math::dvec2 getEffectiveUVScale() const;
    math::dmat3 calculateTransformMatrix() const;

    bool hasTextureTransform() const { return m_textureTransform.has_value(); }
    std::optional<TextureTransformExtension>& getTextureTransform() { return m_textureTransform; }
    const std::optional<TextureTransformExtension>& getTextureTransform() const { return m_textureTransform; }
    GltfResult createTextureTransform();
    void destroyTextureTransform() { m_textureTransform.reset(); }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;

private:
    Texture* m_texture = nullptr;
    std::optional<double> m_scale;
    std::optional<TextureTransformExtension> m_textureTransform;
    std::optional<uint32_t> m_texCoord;
};
}
