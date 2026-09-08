// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "renderer/editor/LightGizmoMapping.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"
#include "renderer/editor/EditorRenderLayer.h"

using namespace ux3d;
using namespace ux3d::renderer;

LightGizmoMapping::LightGizmoMapping(EditorRenderLayer& renderLayer, const filesystem::Path& editorTexturesPath)
    : m_renderLayer(renderLayer)
{
    // Load the billboard images
    m_pointLightTexture = m_renderLayer.getEditorResourceManager().loadTexture(editorTexturesPath / "Icon_Helper_Light_Point_512.png");
    m_spotLightTexture = m_renderLayer.getEditorResourceManager().loadTexture(editorTexturesPath / "Icon_Helper_Light_Spot_512.png");
    m_directionalLightTexture = m_renderLayer.getEditorResourceManager().loadTexture(editorTexturesPath / "Icon_Helper_Light_Directional_512.png");

    m_lightSamplerState = m_renderLayer.getEditorResourceManager().create<renderer::gl::SamplerState>();
}

bool LightGizmoMapping::onUpdateMapping(GltfLightTuple gltfTuple, LightGizmoTuple gizmo)
{
    auto [glTFNode, pGltfLight] = gltfTuple;
    auto [mesh, material] = gizmo;

    // TODO: read this value from settings
    mesh->setUniform("u_fixedSizeScale", 0.1f * m_scale);

    math::vec3 lightColor = pGltfLight->getColorOrDefault();
    float max = math::max(lightColor.r, math::max(lightColor.g, lightColor.b));
    lightColor *= 1.0f / max;
    material->setUniform("u_BaseColorFactor", math::vec4(lightColor, 1));

    switch (pGltfLight->getType()) {
    case gltf::Light::Type::Directional:
        material->samplers.clear();
        material->samplers.emplace_back("u_BaseColorSampler", m_directionalLightTexture, m_lightSamplerState);
        break;
    case gltf::Light::Type::Point:
        material->samplers.clear();
        material->samplers.emplace_back("u_BaseColorSampler", m_pointLightTexture, m_lightSamplerState);
        break;
    case gltf::Light::Type::Spot:
        material->samplers.clear();
        material->samplers.emplace_back("u_BaseColorSampler", m_spotLightTexture, m_lightSamplerState);
        break;
    default:
        break;
    }
    return true;
}

LightGizmoTuple LightGizmoMapping::onCreateMapping(GltfLightTuple gltfTuple)
{
    auto [glTFNode, pGltfLight] = gltfTuple;

    // Create the billboard resources
    Mesh* mesh = m_renderLayer.gltfLayer.getResourceManager().createBillboardMesh();
    Material* material = m_renderLayer.gltfLayer.getResourceManager().createBillboardMaterial();

    LightGizmoTuple tuple = { mesh, material };
    onUpdateMapping(gltfTuple, tuple);

    auto nodeIt = m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.find(glTFNode);
    if (nodeIt != m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->addMesh(mesh);
    }

    return tuple;
}

void LightGizmoMapping::onDestroyMapping(GltfLightTuple gltfTuple, LightGizmoTuple gizmo)
{
    auto [glTFNode, pGltfLight] = gltfTuple;
    auto [mesh, material] = gizmo;

    auto nodeIt = m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.find(glTFNode);
    if (nodeIt != m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->removeMesh(mesh);
    }

    m_renderLayer.gltfLayer.getResourceManager().destroy(material);
    m_renderLayer.gltfLayer.getResourceManager().destroy(mesh);
}
