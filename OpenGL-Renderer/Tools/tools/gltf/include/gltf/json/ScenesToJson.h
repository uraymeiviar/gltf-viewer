#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Scene;
class Gltf;

class ScenesToJson : ChildOfRootToJson {
public:
    ScenesToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions, const Gltf& _gltf);
    void convert(const Scene& _scene, nlohmann::json& _jsonScene, const JsonPath& _jsonPath) const;

private:
    void convertNodes(const Scene& _scene, nlohmann::json& _jsonScene) const;
    void convertXMP(const Scene& _scene, nlohmann::json& _jsonScene) const;
    void convertAudio(const Scene& _scene, nlohmann::json& _jsonScene) const;

    const Gltf& m_gltf;
};
}
