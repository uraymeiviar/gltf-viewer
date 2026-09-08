// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include <cstdint>

namespace ux3d::gltf {
enum class PrimitiveMode : uint32_t {
    Points = 0,
    Lines = 1,
    LineLoop = 2,
    LineStrip = 3,
    Triangles = 4,
    TriangleStrip = 5,
    TriangleFan = 6,
    Default = Triangles
};

enum class ComponentType : uint32_t {
    None = 0,
    Byte = 5120,
    UnsignedByte = 5121,
    Short = 5122,
    UnsignedShort = 5123,
    UnsignedInt = 5125,
    Float = 5126,
};

class GlTypes {
public:
    static core::string componentTypeToString(ComponentType _type);
    static ComponentType componentTypeFromString(const core::string& _text);
    static double maximumValueFromComponentType(ComponentType _type);
    static double minimumValueFromComponentType(ComponentType _type);
};

enum class TextureWrappingMode : uint32_t {
    ClampToEdge = 33071,
    MirroredRepeat = 33648,
    Repeat = 10497
};

enum class TextureFilterType : uint32_t {
    Nearest = 9728,
    Linear = 9729,
    NearestMipmapNearest = 9984,
    LinearMipmapNearest = 9985,
    NearestMipmapLinear = 9986,
    LinearMipmapLinear = 9987
};
}
