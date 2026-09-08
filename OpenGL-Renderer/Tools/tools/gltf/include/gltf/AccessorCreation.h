// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "gltf/Accessor.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/Gltf.h"
#include "gltf/GltfResult.h"

#include "math/VectorTypes.h"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ux3d::gltf {

class AccessorCreation {
public:
    AccessorCreation(Gltf& _gltf)
        : m_gltf(_gltf) {};
    template <typename T>
    Accessor* create(const std::vector<T>& _data, std::optional<std::string> _name = std::nullopt);

    Accessor* create(AttributeType _attributeType,
        ComponentType _componentType,
        core::MemoryRange<const uint8_t> accessorData,
        uint32_t _count,
        std::optional<std::string> _name = std::nullopt);

private:
    Gltf& m_gltf;
};

template <typename T>
inline Accessor* AccessorCreation::create(const std::vector<T>& _data, std::optional<std::string> _name)
{
    if (_data.empty()) {
        return nullptr;
    }
    AttributeType attributeType;
    ComponentType componentType;
    if constexpr (std::is_same_v<T, math::vec2>) {
        attributeType = AttributeType::Vec2;
        componentType = ComponentType::Float;
    } else if (std::is_same_v<T, math::vec3>) {
        attributeType = AttributeType::Vec3;
        componentType = ComponentType::Float;
    } else if (std::is_same_v<T, math::vec4>) {
        attributeType = AttributeType::Vec4;
        componentType = ComponentType::Float;
    } else if (std::is_same_v<T, uint32_t>) {
        attributeType = AttributeType::Scalar;
        componentType = ComponentType::UnsignedInt;
    } else if (std::is_same_v<T, uint16_t>) {
        attributeType = AttributeType::Scalar;
        componentType = ComponentType::UnsignedShort;
    } else if (std::is_same_v<T, uint8_t>) {
        attributeType = AttributeType::Scalar;
        componentType = ComponentType::UnsignedByte;
    } else {
        core::logInfo("AccessorCreation: unsupported type ");
        return nullptr;
    }

    const uint8_t* begin = reinterpret_cast<const uint8_t*>(&_data[0]);
    const size_t length = _data.size() * sizeof(_data[0]);
    core::MemoryRange<const uint8_t> accessorData = core::makeRange(begin, length);

    Accessor* newAccessor = create(attributeType, componentType, accessorData, static_cast<uint32_t>(_data.size()), _name);

    return newAccessor;
}

}
