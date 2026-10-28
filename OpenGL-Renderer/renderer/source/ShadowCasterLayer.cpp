// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "renderer/ShadowCasterLayer.h"
#include "renderer/Mesh.h"
#include "renderer/RenderPass.h"
#include "renderer/passes/OpaquePass.h"
#include "renderer/gl/GL.h"

#include <utility>

using namespace ux3d;
using namespace ux3d::renderer;

ShadowCasterLayer::ShadowCasterLayer(gl::GL* gl)
    : m_gl(gl)
{
    // OpaquePass sets depth test and disables blending; the external commands bind the cascade's layer
    // and restore the viewport target, like the decal prepass.
    m_pass = std::make_unique<OpaquePass>(gl, RenderPassType::Opaque, "SunShadow");
    m_pass->getPreCommands().emplace_back(
        Command::makeExternalFunctionCommand(gl, [this] { if (prePass) prePass(); }));
    m_pass->getPostCommands().emplace_back(
        Command::makeExternalFunctionCommand(gl, [this] { if (postPass) postPass(); }));
}

ShadowCasterLayer::~ShadowCasterLayer() = default;

bool ShadowCasterLayer::materialCasts(const Material& material, const Options& options, Stats* stats)
{
    // Never cast, whether or not the viewport shows them: invisible materials (e.g. collision meshes)
    // and environment occluders (MSFS exporter: BLEND + noCastShadow).
    if (material.invisible || material.environmentOccluder) {
        if (stats != nullptr) ++stats->invisible;
        return false;
    }
    if (options.asoboEnabled) {
        if (material.drawOrderClass == DrawOrderClass::Decal) {
            if (stats != nullptr) ++stats->decals;
            return false;
        }
        // Glass and windshields (ASOBO types, or the extras material code): MSFS assumption, inferred: the
        // SDK's glass is see-through and the cockpit stays sunlit through the canopy, so they do not cast.
        const bool codeGlass = options.applyMaterialCode
            && material.asoboMaterialCode != Material::AsoboMaterialCode::None;
        if (material.drawOrderClass == DrawOrderClass::Glass || codeGlass) {
            if (stats != nullptr) ++stats->glass;
            return false;
        }
        if (options.applyShadowOptions && material.asobo != nullptr) {
            if (const gltf::asobo::ExtensionData* shadowOptions = material.asobo->find("ASOBO_material_shadow_options")) {
                if (shadowOptions->boolean("noCastShadow").value_or(false)) {
                    if (stats != nullptr) ++stats->noCastShadow;
                    return false;
                }
            }
        }
    }
    return true;
}

void ShadowCasterLayer::collect(const core::LinkedList<std::tuple<Mesh*, Material*>>& drawables,
                                const std::function<bool(const Mesh*)>& isSceneMesh, const Options& options,
                                core::LinkedList<std::tuple<Mesh*, Material*>>& casters)
{
    casters.clear();
    m_stats = Stats();
    if (options.modelGeneration != m_modelGeneration) {
        m_materials.clear();
        m_modelGeneration = options.modelGeneration;
    }
    for (const auto& meshMat : drawables) {
        Mesh* mesh = std::get<Mesh*>(meshMat);
        Material* material = std::get<Material*>(meshMat);
        if (mesh == nullptr || material == nullptr || material->renderPass == nullptr || !isSceneMesh(mesh)) {
            continue;
        }
        // OPAQUE, MASK and dithered materials draw in the Opaque pass; BLEND and transmission do not cast.
        if (material->renderPass->getId() != RenderPassType::Opaque) {
            ++m_stats.notOpaquePass;
            continue;
        }
        if (!materialCasts(*material, options, &m_stats)) {
            continue;
        }

        Entry& entry = m_materials[material];
        // Refreshed only when the sources may have changed (not per frame, and not when another viewport set
        // its debug or per-viewport defines on the shared source material).
        if (!entry.material || entry.sourceRevision != options.sourceRevision
            || entry.slopeBias != options.slopeBias || entry.constantBias != options.constantBias) {
            if (entry.material) {
                *entry.material = *material;
            } else {
                entry.material = std::make_unique<Material>(*material);
            }
            ++m_materialCopies;
            Material& copy = *entry.material;
            copy.fragmentShader = "shadow_caster.frag";
            copy.renderPass = m_pass.get();
            // No face culling: single-sided and open geometry blocks the sun from both sides.
            copy.isDoubleSided = true;
            copy.defines.undefine("ASOBO_DECAL_RECEIVER");
            copy.defines.undefine("RAW_SSR_OVERLAY");
            copy.defines.define("DEBUG", 0);
            copy.drawOrderClass = DrawOrderClass::None;
            copy.drawOrderActive = false;
            copy.decalBiasActive = false;
            copy.overrideBlend = false;
            copy.customBlend = false;
            copy.blendingEnabled = false;
            copy.logicOpEnabled = false;
            copy.depthBiasEnabled = true;
            copy.depthBiasSlope = options.slopeBias;
            copy.depthBiasUnits = options.constantBias;
            entry.sourceRevision = options.sourceRevision;
            entry.slopeBias = options.slopeBias;
            entry.constantBias = options.constantBias;
        }
        casters.emplace_back(mesh, entry.material.get());
        ++m_stats.casters;
    }
}
