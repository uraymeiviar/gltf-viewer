#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class Buffer;

class BuffersFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonBuffer) override;

private:
    ConversionResult convertStorageFormat(const nlohmann::json& _editorExtension, Buffer& _buffer);
};
}
