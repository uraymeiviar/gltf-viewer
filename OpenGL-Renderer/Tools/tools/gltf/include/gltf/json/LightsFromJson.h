#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class Gltf;
class Light;

class LightsFromJson : public ChildOfRootFromJson {
private:
    Light* createLight(const nlohmann::json& _jsonLight, Gltf& _Gltf) const;
    ConversionResult convertSpot(const nlohmann::json& _jsonSpot, Light& _Light) const;

public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonLight) override;
};
}
