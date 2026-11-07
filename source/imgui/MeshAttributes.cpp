// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/MeshAttributes.h"

#include "gltf/Accessor.h"
#include "gltf/BufferView.h"
#include "gltf/mesh/Primitive.h"

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

#include <algorithm>
#include <cstring>

namespace ux3d::imgui::mesh_attributes {

namespace {

namespace gltf = ux3d::gltf;

float readComponent(const uint8_t* p, gltf::ComponentType type, bool normalized, bool halfFloat)
{
    switch (type) {
    case gltf::ComponentType::Float: {
        float v;
        std::memcpy(&v, p, sizeof(v));
        return v;
    }
    case gltf::ComponentType::UnsignedByte:
        return normalized ? *p / 255.0f : static_cast<float>(*p);
    case gltf::ComponentType::Byte: {
        const auto v = static_cast<float>(static_cast<int8_t>(*p));
        return normalized ? std::max(v / 127.0f, -1.0f) : v;
    }
    case gltf::ComponentType::UnsignedShort: {
        uint16_t v;
        std::memcpy(&v, p, sizeof(v));
        return halfFloat ? glm::unpackHalf1x16(v) : normalized ? v / 65535.0f : static_cast<float>(v);
    }
    case gltf::ComponentType::Short: {
        int16_t v;
        std::memcpy(&v, p, sizeof(v));
        if (halfFloat) {
            return glm::unpackHalf1x16(static_cast<uint16_t>(v));
        }
        return normalized ? std::max(v / 32767.0f, -1.0f) : static_cast<float>(v);
    }
    default:
        return 0.0f;
    }
}

} // namespace

Floats decode(const gltf::Accessor& accessor, uint32_t components, bool halfFloat)
{
    const uint32_t stored = accessor.getComponentCountFrom(accessor.getType());
    if (stored < components || accessor.getCount() == 0) {
        return nullptr;
    }
    const size_t componentSize = accessor.getByteLengthFrom(accessor.getComponentType());
    const size_t elementSize = accessor.getByteLengthFrom(accessor.getComponentType(), accessor.getType(), false);
    if (componentSize == 0 || elementSize == 0) {
        return nullptr;
    }
    core::Slice<uint8_t> sparse;
    core::MemoryRange<const uint8_t> range;
    size_t stride = elementSize;
    if (accessor.getBufferView() != nullptr) {
        stride = accessor.getBufferView()->getByteStride().value_or(static_cast<uint32_t>(elementSize));
    }
    if (accessor.getSparse().has_value()) {
        if (!accessor.getSparseDataAs(sparse)) {
            return nullptr;
        }
        const auto bytes = sparse.getMemoryRange();
        range = core::makeRange(static_cast<const uint8_t*>(bytes.data()), bytes.size());
    } else if (!accessor.getData(range)) {
        return nullptr;
    }
    const size_t count = accessor.getCount();
    if (range.empty() || stride * (count - 1) + elementSize > range.size()) {
        return nullptr;
    }
    auto out = std::make_shared<std::vector<float>>(count * components);
    const bool normalized = accessor.isNormalized();
    const auto type = accessor.getComponentType();
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* element = range.data() + i * stride;
        for (uint32_t c = 0; c < components; ++c) {
            (*out)[i * components + c] = readComponent(element + c * componentSize, type, normalized, halfFloat);
        }
    }
    return out;
}

bool uvIsHalfFloat(const gltf::Accessor& accessor, bool packaged)
{
    const auto type = accessor.getComponentType();
    return packaged && !accessor.isNormalized()
        && (type == gltf::ComponentType::Short || type == gltf::ComponentType::UnsignedShort);
}

Floats AccessorCache::read(const gltf::Accessor* accessor, uint32_t components, bool halfFloat)
{
    if (accessor == nullptr) {
        return nullptr;
    }
    const auto it = m_cache.find(accessor);
    if (it != m_cache.end()) {
        return it->second;
    }
    Floats result = decode(*accessor, components, halfFloat);
    m_cache.emplace(accessor, result);
    return result;
}

bool readTriangles(const gltf::Primitive& primitive, size_t indexCount, size_t indexByteOffset, int64_t baseVertex,
                   size_t vertexCount, std::vector<std::array<uint32_t, 3>>& triangles)
{
    const gltf::Accessor* indexAccessor = primitive.getIndicesAccessor();
    const auto mode = primitive.getPrimitiveMode();
    size_t count = vertexCount;
    std::vector<uint32_t> indices;
    if (indexAccessor != nullptr) {
        core::MemoryRange<const uint8_t> range;
        if (!indexAccessor->getData(range) || range.empty()) {
            return false;
        }
        const size_t size = indexAccessor->getByteLengthFrom(indexAccessor->getComponentType());
        count = indexCount;
        if (size == 0 || indexByteOffset + count * size > range.size()) {
            return false;
        }
        indices.resize(count);
        const uint8_t* p = range.data() + indexByteOffset;
        for (size_t i = 0; i < count; ++i) {
            uint32_t value = 0;
            std::memcpy(&value, p + i * size, size); // little endian, as the file
            indices[i] = value + static_cast<uint32_t>(baseVertex);
        }
    }
    const auto index = [&indices, indexAccessor](size_t i) {
        return indexAccessor != nullptr ? indices[i] : static_cast<uint32_t>(i);
    };
    triangles.clear();
    if (mode == gltf::PrimitiveMode::Triangles) {
        triangles.reserve(count / 3);
        for (size_t i = 0; i + 2 < count; i += 3) {
            triangles.push_back({ index(i), index(i + 1), index(i + 2) });
        }
    } else if (mode == gltf::PrimitiveMode::TriangleStrip) {
        for (size_t i = 0; i + 2 < count; ++i) {
            triangles.push_back({ index(i), index(i + 1), index(i + 2) });
        }
    } else if (mode == gltf::PrimitiveMode::TriangleFan) {
        for (size_t i = 0; i + 2 < count; ++i) {
            triangles.push_back({ index(0), index(i + 1), index(i + 2) });
        }
    }
    for (const auto& triangle : triangles) {
        for (uint32_t vertex : triangle) {
            if (vertex >= vertexCount) {
                return false;
            }
        }
    }
    return true;
}

} // namespace ux3d::imgui::mesh_attributes
