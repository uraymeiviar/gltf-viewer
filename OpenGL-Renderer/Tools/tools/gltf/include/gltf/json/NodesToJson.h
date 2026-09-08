#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Node;
class Gltf;

class NodesToJson : ChildOfRootToJson {
public:
    NodesToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions, const Gltf& _gltf);
    void convert(const Node& _node, nlohmann::json& _jsonNode, const JsonPath& _jsonPath) const;

private:
    void convertCamera(const Node& _node, nlohmann::json& _jsonNode) const;
    void convertChildren(const Node& _node, nlohmann::json& _jsonNode) const;
    void convertSkin(const Node& _node, nlohmann::json& _jsonNode) const;
    void convertMatrix(const Node& _node, nlohmann::json& _jsonNode) const;
    void convertMesh(const Node& _node, nlohmann::json& _jsonNode) const;
    void convertRotation(const Node& _node, nlohmann::json& _jsonNode) const;
    void convertScale(const Node& _node, nlohmann::json& _jsonNode) const;
    void convertTranslation(const Node& _node, nlohmann::json& _jsonNode) const;
    void convertWeights(const Node& _node, nlohmann::json& _jsonNode) const;

    // Extensions:
    void convertLight(const Node& _node, nlohmann::json& _jsonNode) const;
    void convertXMP(const Node& _node, nlohmann::json& _jsonNode) const;
    void convertAudio(const Node& _node, nlohmann::json& _jsonNode) const;

    const Gltf& m_gltf;
};
}
