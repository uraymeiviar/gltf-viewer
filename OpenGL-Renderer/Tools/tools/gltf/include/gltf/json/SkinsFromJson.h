#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class Skin;

class SkinsFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonSkin) override;
    ConversionResult resolve(const nlohmann::json& _jsonSkin, Skin& _skin);
};

}
