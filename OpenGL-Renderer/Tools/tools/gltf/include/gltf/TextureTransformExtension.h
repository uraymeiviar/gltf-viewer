#pragma once

#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "math/VectorTypes.h"
#include <optional>

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_texture_transform

namespace ux3d::gltf {
// TODO: integrate this into a more general solution for extensions (they should be independent from the carrier, to allow true extensibility)
class TextureTransformExtension : public GltfObject {
public:
    struct Defaults {
        static constexpr double rotation = 0;
        static constexpr math::dvec2 offset = math::dvec2(0, 0);
        static constexpr math::dvec2 scale = math::dvec2(1, 1);
        static constexpr uint32_t texCoord = 0;
    };

    const std::optional<double>& getRotation() const { return m_rotation; }
    double getRotationOrDefault() const { return m_rotation.value_or(Defaults::rotation); }
    GltfResult setRotation(double _rotation);
    void unsetRotation() { m_rotation.reset(); }

    const std::optional<math::dvec2>& getOffset() const { return m_offset; }
    math::dvec2 getOffsetOrDefault() const { return m_offset.value_or(Defaults::offset); }
    GltfResult setOffset(math::dvec2 _offset);
    void unsetOffset() { m_offset.reset(); }

    const std::optional<math::dvec2>& getScale() const { return m_scale; }
    math::dvec2 getScaleOrDefault() const { return m_scale.value_or(Defaults::scale); }
    GltfResult setScale(math::dvec2 _scale);
    void unsetScale() { m_scale.reset(); }

    const std::optional<uint32_t>& getTexCoord() const { return m_texCoord; }
    uint32_t getTexCoordOrDefault() const { return m_texCoord.value_or(Defaults::texCoord); }
    GltfResult setTexCoord(uint32_t _texCoord);
    void unsetTexCoord() { m_texCoord.reset(); }

    glm::dmat3 calculateTransformMatrix() const;

private:
    std::optional<double> m_rotation;
    std::optional<math::dvec2> m_offset;
    std::optional<math::dvec2> m_scale;
    std::optional<uint32_t> m_texCoord;
};
}
