#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Camera;

class CamerasToJson : ChildOfRootToJson {
public:
    using ChildOfRootToJson::ChildOfRootToJson;
    void convert(const Camera& _camera, nlohmann::json& _jsonCamera, const JsonPath& _jsonPath) const;

private:
    void convertType(const Camera& _camera, nlohmann::json& _jsonCamera) const;
    void convertOrthographic(const Camera& _camera, nlohmann::json& _jsonCamera) const;
    void convertPerspective(const Camera& _camera, nlohmann::json& _jsonCamera) const;
};
}
