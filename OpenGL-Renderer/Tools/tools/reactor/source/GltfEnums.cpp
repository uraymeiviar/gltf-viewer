#include "reactor/GltfEnums.h"

#include <cstdint>
#include <stdexcept>

size_t ux3d::reactor::gltf::byteLength(ComponentType type)
{
    switch (type) {
    case ComponentType::Byte:
        return sizeof(int8_t);
    case ComponentType::UnsignedByte:
        return sizeof(uint8_t);
    case ComponentType::Short:
        return sizeof(int16_t);
    case ComponentType::UnsignedShort:
        return sizeof(uint16_t);
    case ComponentType::UnsignedInt:
        return sizeof(uint32_t);
    case ComponentType::Float:
        return sizeof(float);
    }
    return 0;
}

void ux3d::reactor::gltf::to_json(nlohmann::json& json, const Type& type)
{
    switch (type) {
    case Type::SCALAR:
        json = "SCALAR";
        break;
    case Type::VEC2:
        json = "VEC2";
        break;
    case Type::VEC3:
        json = "VEC3";
        break;
    case Type::VEC4:
        json = "VEC4";
        break;
    case Type::MAT2:
        json = "MAT2";
        break;
    case Type::MAT3:
        json = "MAT3";
        break;
    case Type::MAT4:
        json = "MAT4";
        break;
    }
}

void ux3d::reactor::gltf::from_json(const nlohmann::json& json, Type& type)
{
    if (json == "SCALAR") {
        type = Type::SCALAR;
    } else if (json == "VEC2") {
        type = Type::VEC2;
    } else if (json == "VEC3") {
        type = Type::VEC3;
    } else if (json == "VEC4") {
        type = Type::VEC4;
    } else if (json == "MAT2") {
        type = Type::MAT2;
    } else if (json == "MAT3") {
        type = Type::MAT3;
    } else if (json == "MAT4") {
        type = Type::MAT4;
    } else {
        throw std::runtime_error("Cannot convert glTF accessor type from json");
    }
}

size_t ux3d::reactor::gltf::componentCount(Type type)
{
    switch (type) {
    case Type::SCALAR:
        return 1;
    case Type::VEC2:
        return 2;
    case Type::VEC3:
        return 3;
    case Type::VEC4:
        return 4;
    case Type::MAT2:
        return 4;
    case Type::MAT3:
        return 9;
    case Type::MAT4:
        return 16;
    }
    return 0;
}

size_t ux3d::reactor::gltf::byteLength(Type type, ComponentType componentType)
{
    return componentCount(type) * byteLength(componentType);
}

size_t ux3d::reactor::gltf::byteLengthPadded(Type type, ComponentType componentType)
{
    size_t count = componentCount(type);
    size_t stride = count * byteLength(componentType);

    auto alignUp = [](size_t n) { return (n + 3u) & (~4u + 1u); };

    // Scalars are always packed.
    if (count > 1) {
        stride = alignUp(stride);
    }

    // For matrices, each column has to be aligned to a 4-byte boundary.
    if (type == Type::MAT2) {
        size_t rowLength = 2 * byteLength(componentType);
        size_t rowStride = alignUp(rowLength);
        stride = 2 * rowStride;
    }
    if (type == Type::MAT3) {
        size_t rowLength = 3 * byteLength(componentType);
        size_t rowStride = alignUp(rowLength);
        stride = 3 * rowStride;
    }

    return stride;
}

size_t ux3d::reactor::gltf::elementSizePadded(const nlohmann::json& accessor)
{
    return byteLengthPadded(accessor["type"], accessor["componentType"]);
}

size_t ux3d::reactor::gltf::elementSize(const nlohmann::json& accessor)
{
    return byteLength(accessor["type"], accessor["componentType"]);
}
