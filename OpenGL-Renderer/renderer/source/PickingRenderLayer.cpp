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
    if (pickingId == 0 || pickingId >= m_inverseMapping.size()) {
        return nullptr;
    }

    return m_inverseMapping[pickingId];
}

RendererScene PickingRenderLayer::applyLayer(RendererScene&& scene)
{
    m_inverseMapping.clear();
    m_inverseMapping.reserve(scene.drawables.size() + 1);
    m_inverseMapping.push_back(nullptr); // id 0 is "nothing picked"
    uint32_t id = 1u;
    size_t poolUsed = 0;

    ux3d::math::vec3 pickingColor;

    std::map<RenderPass*, RenderPass*> renderPassCopies;

    std::vector<std::pair<Mesh*, Material*>> pickingMeshMats;
    for (auto& [mesh, mat] : scene.drawables) {
        // Every drawable consumes a picking id, pickable or not, so ids and
        // picking colours stay exactly as before.
        pickingColor.r = static_cast<float>((id >> 16u) & 0xFFu) / 255.0f;
        pickingColor.g = static_cast<float>((id >> 8u) & 0xFFu) / 255.0f;
        pickingColor.b = static_cast<float>((id >> 0u) & 0xFFu) / 255.0f;
        m_inverseMapping.push_back(mesh);
        ++id;

        // copy the renderpasses to be able to inject the pickinig framebuffer
        RenderPass* renderPassCopy = nullptr;
        auto renderPassIt = renderPassCopies.find(mat->renderPass);
        if (renderPassIt != renderPassCopies.end()) {
            renderPassCopy = renderPassIt->second;
        }
        if (renderPassIt == renderPassCopies.end()) {
            uint32_t renderPassId = mat->renderPass->getId();
            if (renderPassId == RenderPassType::Opaque
                || renderPassId == RenderPassType::Transparent
                || renderPassId == RenderPassType::Gizmos
                || renderPassId == RenderPassType::PostGizmos
                || renderPassId == RenderPassType::Transmissive) {
                // Pooled per source pass and refreshed by assignment, so the copy keeps
                // its address across calls; contents match a fresh copy (P4.6c).
                auto& pooled = m_passPool[mat->renderPass];
                if (pooled) {
                    *pooled = *mat->renderPass;
                } else {
                    pooled = std::make_unique<RenderPass>(*mat->renderPass);
                }
                renderPassCopy = pooled.get();
                renderPassCopy->setName("Picking/" + mat->renderPass->getName());
                // attach framebuffer bind pre and post commands

                renderPassCopy->getPreCommands().emplace(renderPassCopy->getPreCommands().begin(), renderer::Command::makeExternalFunctionCommand(m_gl, prePass));

                renderPassCopy->getPostCommands().emplace_back(renderer::Command::makeExternalFunctionCommand(m_gl, postPass));
                renderPassCopies[mat->renderPass] = renderPassCopy;
            } else {
                // this material doesn't belong to any of the "pickable" render passes
                continue;
            }
        }

        // One picking material per pickable drawable: a fresh copy of the source
        // material, made by copy-assignment into a pooled object.
        Material* material = nullptr;
        if (poolUsed < m_materialPool.size()) {
            material = m_materialPool[poolUsed].get();
            *material = *mat;
            ++m_materialsReused;
        } else {
            m_materialPool.push_back(std::make_unique<Material>(*mat));
            material = m_materialPool.back().get();
            ++m_materialsCreated;
        }
        ++poolUsed;

        material->fragmentShader = "picking.frag";
        material->uniforms.emplace_back("u_PickingColor", pickingColor);
        if (material->defines.isDefined("ASOBO_ALPHA_DITHER")) {
            material->setUniform<math::vec2>("u_AsoboDitherPixelOrigin",
                math::vec2(static_cast<float>(pixelOriginX), static_cast<float>(pixelOriginY)));
        }
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

    if (pickingOnly) {
        scene.drawables.clear();
    }
    for (auto& meshMat : pickingMeshMats) {
        scene.drawables.emplace_back(meshMat);
    }

    return std::move(scene);
}
