#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class Scene;

class ScenesFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonScene) override;
    ConversionResult resolve(const nlohmann::json& _jsonScene, Scene& _scene);
};
}
