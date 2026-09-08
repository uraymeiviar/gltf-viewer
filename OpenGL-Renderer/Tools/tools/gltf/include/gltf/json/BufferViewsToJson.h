// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class BufferView;

class BufferViewsToJson : ChildOfRootToJson {
public:
    using ChildOfRootToJson::ChildOfRootToJson;
    void convert(const BufferView& _bufferView, nlohmann::json& _jsonBufferView, const JsonPath& _jsonPath) const;

private:
    void convertBuffer(const BufferView& _bufferView, nlohmann::json& _jsonBufferView) const;
    void convertByteOffset(const BufferView& _bufferView, nlohmann::json& _jsonBufferView) const;
    void convertByteLength(const BufferView& _bufferView, nlohmann::json& _jsonBufferView) const;
    void convertByteStride(const BufferView& _bufferView, nlohmann::json& _jsonBufferView) const;
    void convertTarget(const BufferView& _bufferView, nlohmann::json& _jsonBufferView) const;
};
}
