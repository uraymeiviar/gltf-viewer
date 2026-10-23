// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴, David Labode

#include "renderer/TransformHierarchy.h"
#include "renderer/AudioEmitter.h"
#include "renderer/Camera.h"
#include "renderer/Lights.h"
#include "renderer/Mesh.h"

#include <algorithm>

using namespace ux3d;
using namespace ux3d::renderer;

TransformHierarchyNode* TransformHierarchy::createNode(const math::vec3& translation, const math::quat& rotation, const math::vec3& scale, const std::vector<float>& weights)
{
    return &m_nodes.emplace_back(translation, rotation, scale, weights);
}

bool TransformHierarchy::addRootNode(TransformHierarchyNode* rootNode)
{
    return m_rootNodes.emplace_back(rootNode) != nullptr;
}

void TransformHierarchy::apply()
{
    math::mat4x4 transform(1.0f);
    for (auto rootNode : m_rootNodes) {
        // Roots have a fixed identity parent, so only their own state can change them.
        rootNode->traverse(transform, math::mat3(1.0f), false);
    }
}

void TransformHierarchy::clear()
{
    // The hierarchy is about to be rebuilt; cached world transforms may refer
    // to old parents.
    for (auto& node : m_nodes) {
        node.invalidate();
    }
    for (auto rootNode : m_rootNodes) {
        rootNode->clearChildrenRecursive();
    }
    m_rootNodes.clear();
}

void TransformHierarchy::eraseNode(TransformHierarchyNode* node)
{
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [node](auto& x) { return &x == node; });
    if (it != m_nodes.end()) {
        m_nodes.erase(it);
    }
}
