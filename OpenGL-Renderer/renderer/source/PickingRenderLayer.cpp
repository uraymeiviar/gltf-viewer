// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "renderer/PickingRenderLayer.h"
#include "math/VectorTypes.h"
#include "renderer/Mesh.h"
#include "renderer/passes/TransparentPass.h"

using namespace ux3d;
using namespace ux3d::renderer;

PickingRenderLayer::PickingRenderLayer(gl::GL* gl)
    : m_pickingResourceManager(gl)
    , m_gl(gl)
{
}

Mesh* PickingRenderLayer::getMesh(uint32_t pickingId) const
{
    auto it = m_inverseMapping.find(pickingId);
    if (it == m_inverseMapping.end()) {
        return nullptr;
    }

    return it->second;
}

RendererScene PickingRenderLayer::applyLayer(RendererScene&& scene)
{
    m_inverseMapping.clear();
    uint32_t id = 1u;

    m_pickingResourceManager.destroyAllOf<Material>();
    m_pickingResourceManager.destroyAllOf<RenderPass>();

    ux3d::math::vec3 pickingColor;

    std::map<RenderPass*, RenderPass*> renderPassCopies;

    std::vector<std::pair<Mesh*, Material*>> pickingMeshMats;
    for (auto& [mesh, mat] : scene.drawables) {
        // yes this is as evil as it looks. We recreate one material per mesh each frame.
        //  The alternative would be to use a "Mapping" from  renderer::Mesh* to renderer::Material*
        // for picking which would involve a huge linked list and much more code
        Material* material = m_pickingResourceManager.create<Material>(*mat);
        pickingColor.r = static_cast<float>((id >> 16u) & 0xFFu) / 255.0f;
        pickingColor.g = static_cast<float>((id >> 8u) & 0xFFu) / 255.0f;
        pickingColor.b = static_cast<float>((id >> 0u) & 0xFFu) / 255.0f;
        m_inverseMapping[id] = mesh;
        ++id;

        // copy the renderpasses to be able to inject the pickinig framebuffer
        RenderPass* renderPassCopy = nullptr;
        auto renderPassIt = renderPassCopies.find(material->renderPass);
        if (renderPassIt != renderPassCopies.end()) {
            renderPassCopy = renderPassIt->second;
        }
        if (renderPassIt == renderPassCopies.end()) {
            uint32_t renderPassId = material->renderPass->getId();
            if (renderPassId == RenderPassType::Opaque
                || renderPassId == RenderPassType::Transparent
                || renderPassId == RenderPassType::Gizmos
                || renderPassId == RenderPassType::PostGizmos
                || renderPassId == RenderPassType::Transmissive) {
                renderPassCopy = m_pickingResourceManager.create<RenderPass>(*material->renderPass);
                renderPassCopy->setName("Picking/" + material->renderPass->getName());
                // attach framebuffer bind pre and post commands

                renderPassCopy->getPreCommands().emplace(renderPassCopy->getPreCommands().begin(), renderer::Command::makeExternalFunctionCommand(m_gl, prePass));

                renderPassCopy->getPostCommands().emplace_back(renderer::Command::makeExternalFunctionCommand(m_gl, postPass));
                renderPassCopies[material->renderPass] = renderPassCopy;
            } else {
                // this material doesn't belong to any of the "pickable" render passes
                continue;
            }
        }

        material->fragmentShader = "picking.frag";
        material->uniforms.emplace_back("u_PickingColor", pickingColor);
        if (!material->defines.isDefined("ALPHAMODE")) {
            // the values here don't matter as long as ALPHAMODE == ALPHAMODE_OPAQUE
            material->defines.define("ALPHAMODE", 0);
            material->defines.define("ALPHAMODE_OPAQUE", 0);
            material->defines.define("ALPHAMODE_MASK", 1);
            material->defines.define("ALPHAMODE_BLEND", 2);
        }

        material->renderPass = renderPassCopy;

        pickingMeshMats.emplace_back(mesh, material);
    }

    for (auto& meshMat : pickingMeshMats) {
        scene.drawables.emplace_back(meshMat);
    }

    return std::move(scene);
}
