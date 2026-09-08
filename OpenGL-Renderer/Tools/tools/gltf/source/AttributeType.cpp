#include "gltf/AttributeType.h"

using namespace ux3d;
using namespace ux3d::gltf;

core::string gltf::attributeTypeToString(AttributeType _type)
{
    switch (_type) {
    case AttributeType::Scalar:
        return "SCALAR";
    case AttributeType::Vec2:
        return "VEC2";
    case AttributeType::Vec3:
        return "VEC3";
    case AttributeType::Vec4:
        return "VEC4";
    case AttributeType::Mat2:
        return "MAT2";
    case AttributeType::Mat3:
        return "MAT3";
    case AttributeType::Mat4:
        return "MAT4";
    default:
        return "";
    }
}
