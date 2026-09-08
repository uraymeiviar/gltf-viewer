#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class Node;

class NodesFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonNode) override;
    ConversionResult resolve(const nlohmann::json& _jsonNode, Node& _node);
};
}
