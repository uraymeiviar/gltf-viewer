// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "renderer/editor/CameraGizmoMapping.h"

#include "imageformats/imageformats.h"
#include "renderer/Camera.h"
#include "renderer/TransformHierarchy.h"
#include "renderer/editor/EditorRenderLayer.h"

using namespace ux3d;
using namespace ux3d::renderer;

CameraGizmoMapping::CameraGizmoMapping(EditorRenderLayer& renderLayer, const filesystem::Path& editorTexturesPath)
    : m_renderLayer(renderLayer)
{
    m_cameraTexture = m_renderLayer.getEditorResourceManager().loadTexture(editorTexturesPath / "Icon_Helper_Camera_512.png");
    m_cameraSamplerState = m_renderLayer.getEditorResourceManager().create<renderer::gl::SamplerState>();
}

bool CameraGizmoMapping::onUpdateMapping(GltfCameraTuple gltfTuple, CameraGizmoTuple gizmo)
{
    auto [mesh, material] = gizmo;

    // TODO: read this value from settings
    mesh->setUniform("u_fixedSizeScale", 0.1f * scale);

    return true;
}

CameraGizmoTuple CameraGizmoMapping::onCreateMapping(GltfCameraTuple gltfTuple)
{
    auto& [glTFNode, gltfCam] = gltfTuple;

    // Create the billboard resources
    Mesh* mesh = m_renderLayer.gltfLayer.getResourceManager().createBillboardMesh();
    Material* material = m_renderLayer.gltfLayer.getResourceManager().createBillboardMaterial();
    material->uniforms.emplace_back("u_BaseColorFactor", math::vec4(1));
    material->samplers.emplace_back("u_BaseColorSampler", m_cameraTexture, m_cameraSamplerState);

    CameraGizmoTuple gizmo = { mesh, material };
    onUpdateMapping(gltfTuple, gizmo);

    // add the billboards to the transform hierarchy
    auto nodeIt = m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.find(glTFNode);
    if (nodeIt != m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->addMesh(mesh);
    }

    return gizmo;
}

void CameraGizmoMapping::onDestroyMapping(GltfCameraTuple gltfTuple, CameraGizmoTuple gizmo)
{
    auto& [glTFNode, gltfCam] = gltfTuple;
    auto [mesh, material] = gizmo;

    auto nodeIt = m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.find(glTFNode);
    if (nodeIt != m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->removeMesh(mesh);
    }

    m_renderLayer.gltfLayer.getResourceManager().destroy(material);
    m_renderLayer.gltfLayer.getResourceManager().destroy(mesh);
}
