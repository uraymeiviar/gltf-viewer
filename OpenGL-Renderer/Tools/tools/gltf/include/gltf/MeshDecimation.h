#pragma once

#include "gltf/GlTypes.h"
#include "math/VectorTypes.h"
#include <variant>
#include <vector>

namespace ux3d::gltf {
class Gltf;
class Primitive;
class Accessor;
class string;

struct AttributeCollection {
    std::vector<math::vec3> positions;
    std::vector<uint32_t> indices32u;
    std::vector<uint16_t> indices16u;
    std::vector<math::vec2> texCoords0;
    std::vector<math::vec2> texCoords1;
    std::vector<math::vec3> normals;
    std::vector<math::vec4> tangents;
    std::vector<math::vec4> color0;
};

class MeshDecimation {
public:
    MeshDecimation(gltf::Gltf& _gltf, gltf::Primitive& _primitive);
    bool execute(float _targetPercentage, uint32_t _aggressiveness);

private:
    bool setData();
    bool callFQMSLibrary(float _targetPercentage, uint32_t _aggressiveness);
    void getData(AttributeCollection& _attributeCollection);
    void fillAttributeData(AttributeCollection& _attributeCollection);

    gltf::Gltf& m_gltf;
    gltf::Primitive& m_primitive;
};

}
