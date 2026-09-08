// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/EnumOps.h"
#include "core/String.h"
#include <cstdint>

namespace ux3d::gltf {
enum class AttributeType : uint32_t {
    None,
    Vec2,
    Vec3,
    Vec4,
    Mat2,
    Mat3,
    Mat4,
    Scalar
};

core::string attributeTypeToString(AttributeType _type);
}
