//
//  OrientationGizmoMapping.cpp
//  Gestaltor
//
//  Created by Moritz Willy Becher on 28.04.21.
//

#include "renderer/editor/OrientationGizmoMapping.h"

#include "renderer/editor/EditorRenderLayer.h"

using namespace ux3d::renderer;

OrientationGizmoMapping::OrientationGizmoMapping(ux3d::renderer::EditorRenderLayer& renderLayer)
    : m_renderLayer(renderLayer)
{
}

void OrientationGizmoMapping::onDestroyMapping(ux3d::renderer::OrientationGizmoMappingKey key, ux3d::renderer::OrientationGizmoMappingValue value)
{
    const auto& [piece, interactionMode] = key;
}

OrientationGizmoMappingValue OrientationGizmoMapping::onCreateMapping(ux3d::renderer::OrientationGizmoMappingKey key)
{
    const auto& [piece, interactionMode] = key;

    const std::tuple<Mesh*, Material*>& meshMat = m_renderLayer.getEditorResourceManager().getOrientationGizmoMeshMats().at(piece);

    return meshMat;
}

bool OrientationGizmoMapping::onUpdateMapping(ux3d::renderer::OrientationGizmoMappingKey key, ux3d::renderer::OrientationGizmoMappingValue value)
{

    const auto& [piece, interactionMode] = key;

    auto& [mesh, material] = m_renderLayer.getEditorResourceManager().getOrientationGizmoMeshMats().at(piece);

    math::vec4 baseColor(1.0);

    switch (*interactionMode) {
    case OrientationCubeInteractionMode::Hover:
    case OrientationCubeInteractionMode::Selected:
        baseColor = { 1.0f, 1.0f, 0.5f, 1.0f }; // this is the same color as the accent color of the transform gizmos
        break;
    case OrientationCubeInteractionMode::Default:
    default:
        baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        break;
    }

    material->setUniform("u_BaseColorFactor", baseColor);

    return true;
}
