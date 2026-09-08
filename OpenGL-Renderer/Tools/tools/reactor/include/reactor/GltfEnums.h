#pragma once

#include <cstddef>
#include <nlohmann/json.hpp>

namespace ux3d::reactor::gltf {

enum class ComponentType {
    Byte = 5120,
    UnsignedByte = 5121,
    Short = 5122,
    UnsignedShort = 5123,
    UnsignedInt = 5125,
    Float = 5126,
};

enum class Type {
    SCALAR,
    VEC2,
    VEC3,
    VEC4,
    MAT2,
    MAT3,
    MAT4,
};

enum class TextureWrappingMode {
    ClampToEdge = 33071,
    MirroredRepeat = 33648,
    Repeat = 10497
};

enum class TextureFilter {
    Nearest = 9728,
    Linear = 9729,
    NearestMipmapNearest = 9984,
    LinearMipmapNearest = 9985,
    NearestMipmapLinear = 9986,
    LinearMipmapLinear = 9987
};

enum class Topology {
    POINTS,
    LINES,
    LINE_LOOP,
    LINE_STRIP,
    TRIANGLES,
    TRIANGLE_STRIP,
    TRIANGLE_FAN
};

void to_json(nlohmann::json&, const Type&);
void from_json(const nlohmann::json&, Type&);

size_t componentCount(Type);

/// Packed byte length.
size_t byteLength(ComponentType);
size_t byteLength(Type, ComponentType);
size_t elementSize(const nlohmann::json& accessor);

/// Aligned byte length, according to the spec: https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#data-alignment
size_t byteLengthPadded(Type, ComponentType);
size_t elementSizePadded(const nlohmann::json& accessor);
}
