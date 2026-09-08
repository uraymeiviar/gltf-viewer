// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/EnumOps.h"
#include <cstdint>
#include <string>

namespace ux3d::gltf {
enum class MeshShape : uint32_t {
    Cube,
    Plane,
    Disc,
    Sphere,
    Torus
};

std::string meshShapeToString(MeshShape _shape);
MeshShape stringToMeshShape(const std::string& _text);

}
