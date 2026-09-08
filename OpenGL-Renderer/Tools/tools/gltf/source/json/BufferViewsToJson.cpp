// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/BufferViewsToJson.h"
#include "gltf/BufferView.h"
#include "gltf/IndexLookup.h"

void ux3d::gltf::BufferViewsToJson::convert(const BufferView& _bufferView, nlohmann::json& _jsonBufferView, const JsonPath& _jsonPath) const
{
    convertBase(_bufferView, _jsonBufferView);
    convertBuffer(_bufferView, _jsonBufferView);
    convertByteOffset(_bufferView, _jsonBufferView);
    convertByteLength(_bufferView, _jsonBufferView);
    convertByteStride(_bufferView, _jsonBufferView);
    convertTarget(_bufferView, _jsonBufferView);
}

void ux3d::gltf::BufferViewsToJson::convertBuffer(const BufferView& _bufferView, nlohmann::json& _jsonBufferView) const
{
    if (_bufferView.getBuffer() == nullptr) {
        return;
    }

    size_t index = m_indices[_bufferView.getBuffer()];
    _jsonBufferView["buffer"] = index;
}

void ux3d::gltf::BufferViewsToJson::convertByteOffset(const BufferView& _bufferView, nlohmann::json& _jsonBufferView) const
{
    if (!_bufferView.hasByteOffset()) {
        return;
    }

    _jsonBufferView["byteOffset"] = _bufferView.getByteOffset();
}

void ux3d::gltf::BufferViewsToJson::convertByteLength(const BufferView& _bufferView, nlohmann::json& _jsonBufferView) const
{
    if (_bufferView.getByteLength() == 0u) {
        core::logWarning("BufferViewsToJson: ByteLength is 0");
        return;
    }

    _jsonBufferView["byteLength"] = _bufferView.getByteLength();
}

void ux3d::gltf::BufferViewsToJson::convertByteStride(const BufferView& _bufferView, nlohmann::json& _jsonBufferView) const
{
    const auto& byteStride = _bufferView.getByteStride();
    if (!byteStride.has_value()) {
        return;
    }

    _jsonBufferView["byteStride"] = byteStride.value();
}

void ux3d::gltf::BufferViewsToJson::convertTarget(const BufferView& _bufferView, nlohmann::json& _jsonBufferView) const
{
    const auto& target = _bufferView.getTarget();
    if (!target.has_value()) {
        return;
    }

    _jsonBufferView["target"] = target.value();
}
