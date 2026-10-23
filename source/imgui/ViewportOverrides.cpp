// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ViewportOverrides.h"

#include "renderer/gltf/GltfRenderLayer.h"

namespace ux3d::imgui {

bool SharedOverrideState::needsApply(const renderer::GltfRenderLayer& layer, const ViewportDrawOverrides& overrides) const
{
    return !m_valid
        || m_layer != &layer
        || !(m_applied == overrides)
        || m_gltfGeneration != layer.getGltfGeneration()
        || m_sceneGeneration != layer.getSceneGeneration()
        || m_poseGeneration != layer.getPoseGeneration();
}

bool SharedOverrideState::apply(renderer::GltfRenderLayer& layer, const ViewportDrawOverrides& overrides)
{
    if (!needsApply(layer, overrides)) {
        return false;
    }

    for (auto& [gltfMat, renMat] : layer.m_materialMapping.map) {
        if (renMat != nullptr) {
            renMat->isDoubleSided = overrides.forceDoubleSided || (gltfMat ? gltfMat->isDoubleSidedOrDefault() : false);
        }
    }

    for (auto& [gltfTuple, renMesh] : layer.m_meshMapping.map) {
        if (renMesh != nullptr) {
            const auto* gltfNode = std::get<0>(gltfTuple);
            auto nodeIt = layer.m_transformHierarchyMapping.map.find(gltfNode);
            float det = 1.0f;
            if (nodeIt != layer.m_transformHierarchyMapping.map.end() && nodeIt->second != nullptr) {
                det = math::determinant(nodeIt->second->getWorldTransform());
            }
            const bool isClockwise = (det < 0.0f) ^ overrides.invertWinding;
            renMesh->frontFace = isClockwise ? renderer::gl::Winding::Clockwise : renderer::gl::Winding::CounterClockwise;
        }
    }

    m_valid = true;
    m_layer = &layer;
    m_applied = overrides;
    m_gltfGeneration = layer.getGltfGeneration();
    m_sceneGeneration = layer.getSceneGeneration();
    m_poseGeneration = layer.getPoseGeneration();
    return true;
}

} // namespace ux3d::imgui
