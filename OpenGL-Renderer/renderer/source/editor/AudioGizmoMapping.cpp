// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "renderer/editor/AudioGizmoMapping.h"
#include "imageformats/imageformats.h"
#include "renderer/TransformHierarchy.h"
#include "renderer/editor/EditorRenderLayer.h"

using namespace ux3d;
using namespace ux3d::renderer;

AudioGizmoMapping::AudioGizmoMapping(EditorRenderLayer& renderLayer, const filesystem::Path& editorTexturesPath)
    : m_renderLayer { renderLayer }
    , m_texture { m_renderLayer.getEditorResourceManager().loadTexture(editorTexturesPath / "Icon_Helper_Audio_512.png") }
    , m_samplerState { new gl::SamplerState }
{
}

bool AudioGizmoMapping::onUpdateMapping(const ux3d::gltf::Node* gltfTuple, AudioGizmoTuple gizmo)
{
    auto [mesh, material] = gizmo;
    mesh->setUniform("u_fixedSizeScale", 0.1f * scale);
    return true;
}

AudioGizmoTuple AudioGizmoMapping::onCreateMapping(const ux3d::gltf::Node* node)
{
    // Create the billboard resources
    Mesh* mesh = m_renderLayer.gltfLayer.getResourceManager().createBillboardMesh();
    Material* material = m_renderLayer.gltfLayer.getResourceManager().createBillboardMaterial();
    material->uniforms.emplace_back("u_BaseColorFactor", math::vec4(1));
    material->samplers.emplace_back("u_BaseColorSampler", m_texture, m_samplerState.get());

    AudioGizmoTuple gizmo = { mesh, material };
    onUpdateMapping(node, gizmo);

    // add the billboards to the transform hierarchy
    auto nodeIt = m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.find(node);
    if (nodeIt != m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.end()) {
        auto t = nodeIt->second->getWorldTransform();
        nodeIt->second->addMesh(mesh);
    }

    return gizmo;
}

void AudioGizmoMapping::onDestroyMapping(const ux3d::gltf::Node* node, AudioGizmoTuple gizmo)
{
    auto [mesh, material] = gizmo;

    auto nodeIt = m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.find(node);
    if (nodeIt != m_renderLayer.gltfLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->removeMesh(mesh);
    }

    m_renderLayer.gltfLayer.getResourceManager().destroy(material);
    m_renderLayer.gltfLayer.getResourceManager().destroy(mesh);
}
