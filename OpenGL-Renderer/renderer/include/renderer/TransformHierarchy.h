// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#pragma once

#include <list>
#include <vector>

#include "animation/IAnimationTarget.h"
#include "math/MatrixFunctions.h"
#include "math/VectorFunctions.h"
#include "renderer/TransformAnimationTarget.h"
#include "renderer/TransformHierarchyNode.h"

namespace ux3d::renderer {

struct Light;
struct Mesh;
struct Camera;
struct AudioEmitter;

// Management class for the transform hierarchy nodes. Determines the root and holds all nodes
class TransformHierarchy {
public:
    TransformHierarchyNode* createNode(const math::vec3& translation, const math::quat& rotation, const math::vec3& scale, const std::vector<float>& weights);

    void eraseNode(TransformHierarchyNode* node);
    void clear();
    bool addRootNode(TransformHierarchyNode* rootNode);

    void apply();

private:
    std::list<TransformHierarchyNode> m_nodes;
    std::vector<TransformHierarchyNode*> m_rootNodes;
};

}
