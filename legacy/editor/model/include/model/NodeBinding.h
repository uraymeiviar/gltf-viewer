// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: David Labode

#pragma once

#include "core/String.h"
#include "math/VectorTypes.h"

#include <vector>

#include <nlohmann/json.hpp>

namespace ux3d::gltf {
class Node;
}

namespace ux3d::model {
class NodeBinding {
public:
    NodeBinding(gltf::Node* node, nlohmann::json::json_pointer& pointer);

    void setTranslation(const math::dvec3& translation) const;
    void setRotation(const math::dquat& rotation) const;
    void setScale(const math::dvec3& scale) const;

    // saves translation to mementos
    void saveTranslation();
    // saves rotation to mementos
    void saveRotation();
    // saves scale to mementos
    void saveScale();

private:
    gltf::Node* getGltfObject() const;
    nlohmann::json::json_pointer nodePointer;
    uint32_t m_gltfId;
};

}
