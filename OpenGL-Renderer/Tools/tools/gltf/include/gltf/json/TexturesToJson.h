// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Texture;

class TexturesToJson : ChildOfRootToJson {
public:
    using ChildOfRootToJson::ChildOfRootToJson;
    void convert(const Texture& _texture, nlohmann::json& _jsonTexture, const JsonPath& _jsonPath) const;

private:
    void convertSampler(const Texture& _texture, nlohmann::json& _jsonTexture) const;
    void convertSource(const Texture& _texture, nlohmann::json& _jsonTexture) const;

    void convertExtensionBasisu(const Texture& _texture, nlohmann::json& _jsonTexture) const;
    void convertExtensionWebP(const Texture& _texture, nlohmann::json& _jsonTexture) const;
};
}
