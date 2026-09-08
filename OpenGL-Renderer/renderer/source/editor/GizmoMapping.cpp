// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Jim Eckerlein, David Labode

#include "renderer/editor/GizmoMapping.h"
#include "renderer/TransformHierarchy.h"
#include "renderer/editor/EditorRenderLayer.h"

#include <algorithm>

using namespace ux3d;
using namespace ux3d::renderer;

GizmoMapping::GizmoMapping(EditorRenderLayer& renderLayer)
    : m_renderLayer(renderLayer)
{
}

bool GizmoMapping::onUpdateMapping(GltfGizmoTuple gltfTuple, RendererGizmoTuples rendererTuples)
{
    const auto& [gltfNode, state] = gltfTuple;

    rendererTuples->clear();
    math::vec4* colors[(size_t)GizmoPiece::Num] {};
    getColors(*state, colors);

    for (size_t i = 0; i < static_cast<size_t>(GizmoPiece::Num); ++i) {
        if (colors[i] == nullptr) {
            continue;
        }
        const std::tuple<Mesh*, Material*>& meshMat = m_renderLayer.getEditorResourceManager().getGizmoMeshMats()[i];
        std::get<Material*>(meshMat)->setUniform("u_BaseColorFactor", *colors[i]);
        std::get<Mesh*>(meshMat)->setUniform("u_fixedSizeScale", getGizmoBaseScale() * m_scale);
        std::get<Mesh*>(meshMat)->nukeRotation = m_nukeRotation;
        if (*colors[i] == cInvisible) {
            // skip meshes that are not visible (to avoid z clipping)
            continue;
        }
        rendererTuples->emplace_back(meshMat);
    }

    return true;
}

RendererGizmoTuples GizmoMapping::onCreateMapping(GltfGizmoTuple gltfTuple)
{
    auto& [gltfNode, state] = gltfTuple;

    RendererGizmoTuples rendererTuples = new std::vector<std::tuple<Mesh*, Material*>>;

    // Add meshes to gizmoed glTF node, so transform is shared.
    if (auto nodeIt = m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.find(gltfNode);
        nodeIt != m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.end()) {
        for (size_t i = 0; i < static_cast<size_t>(GizmoPiece::Num); ++i) {
            nodeIt->second->addMesh(std::get<Mesh*>(m_renderLayer.getEditorResourceManager().getGizmoMeshMats()[i]));
        }
    }

    onUpdateMapping(gltfTuple, rendererTuples);
    return rendererTuples;
}

void GizmoMapping::onDestroyMapping(GltfGizmoTuple gltfTuple, RendererGizmoTuples rendererTuples)
{
    auto& [gltfNode, state] = gltfTuple;

    // TODO: Delete all gizmo meshes. Currently, all mesh pointers are aliased on null.
    if (auto nodeIt = m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.find(gltfNode);
        nodeIt != m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.end()) {
        for (size_t i = 0; i < static_cast<size_t>(GizmoPiece::Num); ++i) {
            nodeIt->second->removeMesh(std::get<Mesh*>(m_renderLayer.getEditorResourceManager().getGizmoMeshMats()[i]));
        }
    }
}

void GizmoMapping::getColors(const GizmoState& state, math::vec4* outColors[(size_t)GizmoPiece::Num])
{
    switch (state.transformMode) {
    case GizmoTransformMode::DefaultTranslate:
        outColors[(size_t)GizmoPiece::TranslationAxisX] = &cX;
        outColors[(size_t)GizmoPiece::TranslationAxisY] = &cY;
        outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cZ;
        outColors[(size_t)GizmoPiece::TranslationPlaneX] = &cX;
        outColors[(size_t)GizmoPiece::TranslationPlaneY] = &cY;
        outColors[(size_t)GizmoPiece::TranslationPlaneZ] = &cZ;
        outColors[(size_t)GizmoPiece::TranslationHighlightPlaneX] = &cInvisible;
        outColors[(size_t)GizmoPiece::TranslationHighlightPlaneY] = &cInvisible;
        outColors[(size_t)GizmoPiece::TranslationHighlightPlaneZ] = &cInvisible;
        return;

    case GizmoTransformMode::DefaultScale:
        outColors[(size_t)GizmoPiece::ScaleAxisX] = &cX;
        outColors[(size_t)GizmoPiece::ScaleAxisY] = &cY;
        outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cZ;
        outColors[(size_t)GizmoPiece::ScalePlaneX] = &cX;
        outColors[(size_t)GizmoPiece::ScalePlaneY] = &cY;
        outColors[(size_t)GizmoPiece::ScalePlaneZ] = &cZ;
        outColors[(size_t)GizmoPiece::ScaleHighlightPlaneX] = &cInvisible;
        outColors[(size_t)GizmoPiece::ScaleHighlightPlaneY] = &cInvisible;
        outColors[(size_t)GizmoPiece::ScaleHighlightPlaneZ] = &cInvisible;
        outColors[(size_t)GizmoPiece::ScaleCenter] = &cUniform;
        return;

    case GizmoTransformMode::DefaultRotate:
        outColors[(size_t)GizmoPiece::RotationXCircle] = &cX;
        outColors[(size_t)GizmoPiece::RotationYCircle] = &cY;
        outColors[(size_t)GizmoPiece::RotationZCircle] = &cZ;
        outColors[(size_t)GizmoPiece::RotationDepthStencil] = &cMask;
        return;

    default:
        break;
    }

    switch (state.interactionMode) {
    case GizmoInteractionMode::Hover:

        switch (state.transformMode) {
        case GizmoTransformMode::TranslateX:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cY;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::TranslationPlaneX] = &cX;
            outColors[(size_t)GizmoPiece::TranslationPlaneY] = &cY;
            outColors[(size_t)GizmoPiece::TranslationPlaneZ] = &cZ;
            break;
        case GizmoTransformMode::TranslateY:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cX;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::TranslationPlaneX] = &cX;
            outColors[(size_t)GizmoPiece::TranslationPlaneY] = &cY;
            outColors[(size_t)GizmoPiece::TranslationPlaneZ] = &cZ;
            break;
        case GizmoTransformMode::TranslateZ:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cX;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cY;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationPlaneX] = &cX;
            outColors[(size_t)GizmoPiece::TranslationPlaneY] = &cY;
            outColors[(size_t)GizmoPiece::TranslationPlaneZ] = &cZ;
            break;
        case GizmoTransformMode::TranslatePlaneX:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cX;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cY;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::TranslationPlaneX] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationPlaneY] = &cY;
            outColors[(size_t)GizmoPiece::TranslationPlaneZ] = &cZ;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::TranslationHighlightPlaneX] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::TranslatePlaneY:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cX;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cY;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::TranslationPlaneX] = &cX;
            outColors[(size_t)GizmoPiece::TranslationPlaneY] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationPlaneZ] = &cZ;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::TranslationHighlightPlaneY] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::TranslatePlaneZ:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cX;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cY;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::TranslationPlaneX] = &cX;
            outColors[(size_t)GizmoPiece::TranslationPlaneY] = &cY;
            outColors[(size_t)GizmoPiece::TranslationPlaneZ] = &cAccent;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::TranslationHighlightPlaneZ] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::ScaleX:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cY;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScalePlaneX] = &cX;
            outColors[(size_t)GizmoPiece::ScalePlaneY] = &cY;
            outColors[(size_t)GizmoPiece::ScalePlaneZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScaleCenter] = &cUniform;
            break;
        case GizmoTransformMode::ScaleY:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cX;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScalePlaneX] = &cX;
            outColors[(size_t)GizmoPiece::ScalePlaneY] = &cY;
            outColors[(size_t)GizmoPiece::ScalePlaneZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScaleCenter] = &cUniform;
            break;
        case GizmoTransformMode::ScaleZ:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cX;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cY;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cAccent;
            outColors[(size_t)GizmoPiece::ScalePlaneX] = &cX;
            outColors[(size_t)GizmoPiece::ScalePlaneY] = &cY;
            outColors[(size_t)GizmoPiece::ScalePlaneZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScaleCenter] = &cUniform;
            break;
        case GizmoTransformMode::ScalePlaneX:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cX;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cY;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScalePlaneX] = &cAccent;
            outColors[(size_t)GizmoPiece::ScalePlaneY] = &cY;
            outColors[(size_t)GizmoPiece::ScalePlaneZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScaleCenter] = &cUniform;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::ScaleHighlightPlaneX] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::ScalePlaneY:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cX;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cY;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScalePlaneX] = &cX;
            outColors[(size_t)GizmoPiece::ScalePlaneY] = &cAccent;
            outColors[(size_t)GizmoPiece::ScalePlaneZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScaleCenter] = &cUniform;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::ScaleHighlightPlaneY] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::ScalePlaneZ:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cX;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cY;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScalePlaneX] = &cX;
            outColors[(size_t)GizmoPiece::ScalePlaneY] = &cY;
            outColors[(size_t)GizmoPiece::ScalePlaneZ] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleCenter] = &cUniform;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::ScaleHighlightPlaneZ] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::ScaleUniform:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cX;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cY;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScalePlaneX] = &cX;
            outColors[(size_t)GizmoPiece::ScalePlaneY] = &cY;
            outColors[(size_t)GizmoPiece::ScalePlaneZ] = &cZ;
            outColors[(size_t)GizmoPiece::ScaleCenter] = &cAccent;
            break;
        case GizmoTransformMode::RotateX:
            outColors[(size_t)GizmoPiece::RotationXCircle] = &cAccent;
            outColors[(size_t)GizmoPiece::RotationYCircle] = &cY;
            outColors[(size_t)GizmoPiece::RotationZCircle] = &cZ;
            outColors[(size_t)GizmoPiece::RotationDepthStencil] = &cMask;
            break;
        case GizmoTransformMode::RotateY:
            outColors[(size_t)GizmoPiece::RotationXCircle] = &cX;
            outColors[(size_t)GizmoPiece::RotationYCircle] = &cAccent;
            outColors[(size_t)GizmoPiece::RotationZCircle] = &cZ;
            outColors[(size_t)GizmoPiece::RotationDepthStencil] = &cMask;
            break;
        case GizmoTransformMode::RotateZ:
            outColors[(size_t)GizmoPiece::RotationXCircle] = &cX;
            outColors[(size_t)GizmoPiece::RotationYCircle] = &cY;
            outColors[(size_t)GizmoPiece::RotationZCircle] = &cAccent;
            outColors[(size_t)GizmoPiece::RotationDepthStencil] = &cMask;
            break;
        default:
            break;
        }
        break;

    case GizmoInteractionMode::Drag:

        switch (state.transformMode) {
        case GizmoTransformMode::TranslateX:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cAttenuated;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cAttenuated;
            break;
        case GizmoTransformMode::TranslateY:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cAttenuated;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cAttenuated;
            break;
        case GizmoTransformMode::TranslateZ:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cAttenuated;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cAttenuated;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cAccent;
            break;
        case GizmoTransformMode::TranslatePlaneX:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cAttenuated;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationPlaneX] = &cAccent;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::TranslationHighlightPlaneX] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::TranslatePlaneY:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cAttenuated;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationPlaneY] = &cAccent;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::TranslationHighlightPlaneY] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::TranslatePlaneZ:
            outColors[(size_t)GizmoPiece::TranslationAxisX] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationAxisY] = &cAccent;
            outColors[(size_t)GizmoPiece::TranslationAxisZ] = &cAttenuated;
            outColors[(size_t)GizmoPiece::TranslationPlaneZ] = &cAccent;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::TranslationHighlightPlaneZ] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::ScaleX:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cAttenuated;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cAttenuated;
            break;
        case GizmoTransformMode::ScaleY:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cAttenuated;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cAttenuated;
            break;
        case GizmoTransformMode::ScaleZ:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cAttenuated;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cAttenuated;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cAccent;
            break;
        case GizmoTransformMode::ScalePlaneX:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cAttenuated;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cAccent;
            outColors[(size_t)GizmoPiece::ScalePlaneX] = &cAccent;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::ScaleHighlightPlaneX] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::ScalePlaneY:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cAttenuated;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cAccent;
            outColors[(size_t)GizmoPiece::ScalePlaneY] = &cAccent;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::ScaleHighlightPlaneY] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::ScalePlaneZ:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cAttenuated;
            outColors[(size_t)GizmoPiece::ScalePlaneZ] = &cAccent;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::ScaleHighlightPlaneZ] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::ScaleUniform:
            outColors[(size_t)GizmoPiece::ScaleAxisX] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleAxisY] = &cAccent;
            outColors[(size_t)GizmoPiece::ScaleAxisZ] = &cAccent;
            outColors[(size_t)GizmoPiece::ScalePlaneX] = &cAccent;
            outColors[(size_t)GizmoPiece::ScalePlaneY] = &cAccent;
            outColors[(size_t)GizmoPiece::ScalePlaneZ] = &cAccent;
            // TODO use this again once planes are rendered double sided
            //outColors[(size_t)GizmoPiece::ScaleHighlightPlaneX] = &cAccentTranslucent;
            //outColors[(size_t)GizmoPiece::ScaleHighlightPlaneY] = &cAccentTranslucent;
            //outColors[(size_t)GizmoPiece::ScaleHighlightPlaneZ] = &cAccentTranslucent;
            break;
        case GizmoTransformMode::RotateX:
            outColors[(size_t)GizmoPiece::RotationXCircle] = &cAccent;
            outColors[(size_t)GizmoPiece::RotationYCircle] = &cAttenuated;
            outColors[(size_t)GizmoPiece::RotationZCircle] = &cAttenuated;
            outColors[(size_t)GizmoPiece::RotationDepthStencil] = &cMask;
            break;
        case GizmoTransformMode::RotateY:
            outColors[(size_t)GizmoPiece::RotationXCircle] = &cAttenuated;
            outColors[(size_t)GizmoPiece::RotationYCircle] = &cAccent;
            outColors[(size_t)GizmoPiece::RotationZCircle] = &cAttenuated;
            outColors[(size_t)GizmoPiece::RotationDepthStencil] = &cMask;
            break;
        case GizmoTransformMode::RotateZ:
            outColors[(size_t)GizmoPiece::RotationXCircle] = &cAttenuated;
            outColors[(size_t)GizmoPiece::RotationYCircle] = &cAttenuated;
            outColors[(size_t)GizmoPiece::RotationZCircle] = &cAccent;
            outColors[(size_t)GizmoPiece::RotationDepthStencil] = &cMask;
            break;
        default:
            break;
        }
        break;
    }
}
