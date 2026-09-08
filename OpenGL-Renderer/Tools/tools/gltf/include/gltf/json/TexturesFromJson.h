#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class Texture;

class TexturesFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonTexture) override;
    ConversionResult resolve(const nlohmann::json& _jsonTexture, Texture& _texture);
};
}
