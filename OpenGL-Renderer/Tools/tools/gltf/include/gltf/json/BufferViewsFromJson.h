#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class BufferView;

class BufferViewsFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonBufferView);
    ConversionResult resolve(const nlohmann::json& _jsonBufferView, BufferView& _bufferView);
};
}
