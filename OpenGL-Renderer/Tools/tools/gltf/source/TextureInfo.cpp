// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/TextureInfo.h"
#include "gltf/Texture.h"

using namespace ux3d;
using namespace ux3d::gltf;

GltfResult TextureInfo::createTextureTransform()
{
    if (m_textureTransform.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }

    m_textureTransform.emplace();
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::TextureInfo::setTexCoord(uint32_t _texCoord)
{
    m_texCoord = _texCoord;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::TextureInfo::setScale(double _scale)
{
    m_scale = _scale;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::TextureInfo::setStrength(double _strength)
{
    m_scale = _strength;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::TextureInfo::setTexture(Texture& _texture)
{
    m_texture = &_texture;
    return GltfResult::Success;
}

uint32_t ux3d::gltf::TextureInfo::getEffectiveTexCoord() const
{
    if (m_textureTransform.has_value() && m_textureTransform->getTexCoord().has_value()) {
        return m_textureTransform->getTexCoord().value();
    }

    return m_texCoord.value_or(0u);
}

double ux3d::gltf::TextureInfo::getEffectiveUVRotation() const
{
    return m_textureTransform.value_or(TextureTransformExtension()).getRotationOrDefault();
}

ux3d::math::dvec2 ux3d::gltf::TextureInfo::getEffectiveUVOffset() const
{
    return m_textureTransform.value_or(TextureTransformExtension()).getOffsetOrDefault();
}

ux3d::math::dvec2 ux3d::gltf::TextureInfo::getEffectiveUVScale() const
{
    return m_textureTransform.value_or(TextureTransformExtension()).getScaleOrDefault();
}

bool ux3d::gltf::TextureInfo::isValid() const
{
    return m_texture != nullptr;
}

ux3d::gltf::GltfResult ux3d::gltf::TextureInfo::getInvalidProperties(core::Vector<core::string>& _invalidProperties) const
{
    if (false == _invalidProperties.reserve(1)) {
        return GltfError::AllocationFailure;
    }

    if (m_texture == nullptr) {
        (void)(_invalidProperties.emplace_back("index"));
    }

    return GltfResult::Success;
}

ux3d::math::dmat3 ux3d::gltf::TextureInfo::calculateTransformMatrix() const
{
    return m_textureTransform.value_or(TextureTransformExtension()).calculateTransformMatrix();
}
