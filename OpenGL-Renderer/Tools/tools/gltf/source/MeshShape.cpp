#include "gltf/MeshShape.h"

using namespace ux3d;
using namespace ux3d::gltf;

std::string gltf::meshShapeToString(MeshShape _shape)
{
    switch (_shape) {
    case MeshShape::Cube:
        return "Cube";
    case MeshShape::Plane:
        return "Plane";
    case MeshShape::Disc:
        return "Disc";
    case MeshShape::Sphere:
        return "Sphere";
    case MeshShape::Torus:
        return "Torus";
    default:
        return "";
    }
}

MeshShape gltf::stringToMeshShape(const std::string& _text)
{
    if (_text == "Cube") {
        return MeshShape::Cube;
    } else if (_text == "Plane") {
        return MeshShape::Plane;
    } else if (_text == "Disc") {
        return MeshShape::Disc;
    } else if (_text == "Sphere") {
        return MeshShape::Sphere;
    } else if (_text == "Torus") {
        return MeshShape::Torus;
    }

    return MeshShape::Cube;
}
