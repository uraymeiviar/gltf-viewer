#include "gltf/TextureTransformExtension.h"

using namespace ux3d;
using namespace ux3d::gltf;

GltfResult TextureTransformExtension::setRotation(double _rotation)
{
    m_rotation = _rotation;
    return GltfResult::Success;
}

GltfResult TextureTransformExtension::setOffset(math::dvec2 _offset)
{
    m_offset = _offset;
    return GltfResult::Success;
}

GltfResult TextureTransformExtension::setScale(math::dvec2 _scale)
{
    m_scale = _scale;
    return GltfResult::Success;
}

GltfResult TextureTransformExtension::setTexCoord(uint32_t _texCoord)
{
    m_texCoord = _texCoord;
    return GltfResult::Success;
}

glm::dmat3 ux3d::gltf::TextureTransformExtension::calculateTransformMatrix() const
{
    math::dvec2 offset = getOffsetOrDefault();
    math::dvec2 scale = getScaleOrDefault();
    double rotation = getRotationOrDefault();
    double cos = math::cos(rotation);
    double sin = math::sin(rotation);

    math::dmat3 translationMatrix(1.0);
    translationMatrix[2][0] = offset.x;
    translationMatrix[2][1] = offset.y;

    math::dmat3 rotationMatrix(1.0);
    rotationMatrix[0][0] = cos;
    rotationMatrix[0][1] = -sin;
    rotationMatrix[1][0] = sin;
    rotationMatrix[1][1] = cos;

    math::dmat3 scaleMatrix(1.0);
    scaleMatrix[0][0] = scale.x;
    scaleMatrix[1][1] = scale.y;

    return translationMatrix * rotationMatrix * scaleMatrix;
}
