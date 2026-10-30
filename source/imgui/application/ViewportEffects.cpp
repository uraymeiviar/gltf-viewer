// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Per-viewport render effects drawViewports drives: the A2.1 decal buffer, the A3.2 cascaded sun shadow map and
// the A3.9 sky-occlusion volume, with their get_state / profile-report JSON.

#include "Application.h"
#include "imgui/OceanSimulation.h"
#include "imgui/RenderQuality.h"
#include "imgui/SearchPathsPanel.h"
#include "imgui/Theme.h"
#include "imgui/ObjectActions.h"
#include "imgui/SceneHierarchyPanel.h"
#include "renderer/editor/EditorRenderLayer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace ux3d::imgui::application {

bool Application::viewportNeedsDecalBuffer(const ux3d::imgui::ViewportInstance& viewport) const
{
    // Inspection needs the decal geometry itself, even without a receiver beneath it.
    // Keeping forward draws also makes isolated decals eligible for the picking pass.
    if (gltfLayer == nullptr || gltfLayer->getIsolationRoot() != nullptr
        || viewport.settings.renderMode == ux3d::renderer::RenderMode::Wireframe) {
        return false;
    }
    // A2.1/A2.2: the buffer exists only when the file has decals of an extension the
    // viewport applies. AsoboOptions::applies already includes the master switch; checking
    // it explicitly keeps the requirement readable and guards a future change to applies().
    if (!viewport.settings.asobo.enabled) {
        return false;
    }
    return (gltfLayer->hasGeometryDecalMaterials()
               && viewport.settings.asobo.applies("ASOBO_material_geometry_decal"))
        || (gltfLayer->hasBlendGbufferMaterials()
            && viewport.settings.asobo.applies("ASOBO_material_blend_gbuffer"));
}

void Application::updateSunShadows(ux3d::imgui::ViewportInstance& vp, FrameContext& frame, const glm::mat4& view,
                                   const glm::mat4& projection, const ux3d::imgui::SceneEnvironmentState& environment,
                                   bool shadingAllowed, int vpW, int vpH, std::optional<float> groundHeight)
{
    ux3d::renderer::CommandGenerator::SunShadowParameters parameters; // disabled
    // A6.6: the shadowed directional light: the sun, or the moon at night (one at a time).
    const EffectiveSun sun = effectiveDirectionalLight(environment);
    vp.sunShadowFromMoon = sun.moon;
    vp.sunShadowDirection = sun.direction;
    const ux3d::imgui::RenderQualitySettings quality = ux3d::imgui::RenderQuality::effective();
    const bool wanted = sun.enabled && quality.shadows && vp.settings.sunShadows && shadingAllowed && gltfLayer != nullptr && commandGenerator != nullptr
        && glRenderer != nullptr && shadowCasterLayer != nullptr && sunShadowCamera != nullptr && activeScene != nullptr;
    const auto disable = [&] {
        vp.sunShadowMap.release();
        vp.sunShadowSetup = ux3d::renderer::ShadowCascadeSetup();
        vp.sunShadowActive = false;
        if (commandGenerator != nullptr) commandGenerator->setSunShadow(parameters);
    };
    if (!wanted) {
        disable();
        return;
    }

    // The viewport's unculled scene: the casters, and (with the receivers) the extent the cascades are cut to.
    gltfLayer->frustumCulling = false;
    gltfLayer->asoboOptions = vp.settings.asobo;
    gltfLayer->materialExtensionOptions = vp.settings.materialExtensions;
    auto [casterScene, casterMapping] = gltfLayer->createScene(activeScene);

    // Cascades fitted to this viewport's (unjittered) frustum, cut to the scene's extent.
    ux3d::renderer::ShadowCascadeInput input;
    input.view = view;
    input.projection = projection;
    input.directionToSun = sun.direction;
    input.cascadeCount = std::clamp(quality.shadowCascades, 2, 4);
    input.resolution = static_cast<int>(quality.shadowResolution);
    input.maxDistance = quality.shadowDistance;
    input.splitLambda = quality.shadowSplitLambda;
    {
        // Tight bounds from the drawables' static POSITION boxes (world space, current pose). Skinned, morphed
        // and quantized primitives cannot be bounded that way: the loose node extents (bounding spheres of
        // the accessor boxes) are added for them.
        ux3d::math::vec3 lo(0.0f);
        ux3d::math::vec3 hi(0.0f);
        bool bounded = false;
        const size_t unbounded = gltfLayer->boundDrawables(casterScene.drawables, casterMapping, lo, hi, bounded);
        glm::vec3 sceneMin(lo);
        glm::vec3 sceneMax(hi);
        if (unbounded > 0 || !bounded) {
            const ux3d::gltf::Extends& extents = getCachedSceneExtents(activeScene, gltfModel, gltfLayer.get());
            const glm::vec3 extentsMin(extents.min);
            const glm::vec3 extentsMax(extents.max);
            sceneMin = bounded ? glm::min(sceneMin, extentsMin) : extentsMin;
            sceneMax = bounded ? glm::max(sceneMax, extentsMax) : extentsMax;
        }
        if (groundHeight.has_value() && std::isfinite(sceneMin.x + sceneMin.y + sceneMin.z + sceneMax.x + sceneMax.y + sceneMax.z)
            && sceneMax.x >= sceneMin.x && sceneMax.y >= sceneMin.y && sceneMax.z >= sceneMin.z) {
            // A3.6: the ground plane receives the model's shadow. Add the plane to the range the cascades are cut
            // to, together with the model's footprint on it: every corner of the scene box carried along the sun's
            // rays to the plane (at most ten box diagonals away when the sun is near the horizon).
            const float height = groundHeight.value();
            const glm::vec3 toSun = glm::normalize(sun.direction);
            const float reach = 10.0f * glm::length(sceneMax - sceneMin);
            glm::vec3 lo = sceneMin;
            glm::vec3 hi = sceneMax;
            lo.y = std::min(lo.y, height);
            hi.y = std::max(hi.y, height);
            if (toSun.y > 1.0e-4f) {
                for (int corner = 0; corner < 8; ++corner) {
                    const glm::vec3 point((corner & 1) ? sceneMax.x : sceneMin.x, (corner & 2) ? sceneMax.y : sceneMin.y,
                                          (corner & 4) ? sceneMax.z : sceneMin.z);
                    const float distance = std::min((point.y - height) / toSun.y, reach);
                    if (distance <= 0.0f) continue;
                    const glm::vec3 onGround = point - toSun * distance;
                    lo = glm::min(lo, onGround);
                    hi = glm::max(hi, onGround);
                }
            }
            sceneMin = lo;
            sceneMax = hi;
        }
        if (std::isfinite(sceneMin.x + sceneMin.y + sceneMin.z + sceneMax.x + sceneMax.y + sceneMax.z)
            && sceneMax.x >= sceneMin.x && sceneMax.y >= sceneMin.y && sceneMax.z >= sceneMin.z) {
            const glm::vec3 margin(0.01f * glm::length(sceneMax - sceneMin) + 0.01f);
            input.hasSceneBounds = true;
            input.sceneMin = sceneMin - margin;
            input.sceneMax = sceneMax + margin;
        }
    }
    const ux3d::renderer::ShadowCascadeSetup setup = ux3d::renderer::fitShadowCascades(input);
    if (!setup.valid || !vp.sunShadowMap.ensure(input.resolution, setup.count)) {
        disable();
        vp.sceneFbo.bind();
        glViewport(0, 0, vpW, vpH);
        return;
    }

    // Casters: every primitive of the unculled scene that casts (ShadowCasterLayer rules); each cascade
    // then culls them against its own light volume.
    ux3d::renderer::ShadowCasterLayer::Options options;
    options.asoboEnabled = vp.settings.asobo.enabled;
    options.applyShadowOptions = vp.settings.asobo.applies("ASOBO_material_shadow_options");
    options.applyMaterialCode = vp.settings.asobo.applies("ASOBO_material_code");
    options.slopeBias = quality.shadowSlopeBias;
    options.constantBias = quality.shadowConstantBias;
    // The caster copies follow their sources when the model, a material or the render settings change.
    uint64_t sourceRevision = gltfLayer->getGltfGeneration();
    for (const auto change : { ux3d::imgui::SceneChange::Material, ux3d::imgui::SceneChange::Extension,
                               ux3d::imgui::SceneChange::RenderSettings, ux3d::imgui::SceneChange::ViewportSettings }) {
        sourceRevision = sourceRevision * 0x9E3779B97F4A7C15ULL + sceneRevisions.revision(change);
    }
    options.sourceRevision = sourceRevision;
    options.modelGeneration = gltfLayer->getSceneGeneration();
    ux3d::core::LinkedList<std::tuple<ux3d::renderer::Mesh*, ux3d::renderer::Material*>> casters;
    shadowCasterLayer->collect(casterScene.drawables,
                               [&casterMapping](const ux3d::renderer::Mesh* mesh) {
                                   return casterMapping.find(const_cast<ux3d::renderer::Mesh*>(mesh)) != casterMapping.end();
                               },
                               options, casters);

    // Frame values the caster shader reads (getBaseColor, the A1.7 dither); the dither pattern stays fixed.
    commandGenerator->setSunShadow(parameters);
    commandGenerator->setAsoboAlphaDither(vp.settings.asobo.applies("ASOBO_material_alphamode_dither"));
    commandGenerator->setAsoboDitherFrame(0.0f);
    commandGenerator->setAsoboUVOptions(vp.settings.asobo.applies("ASOBO_material_UV_options"));
    commandGenerator->setAsoboDetailMap(vp.settings.asobo.applies("ASOBO_material_detail_map"));
    commandGenerator->setAsoboWindshieldVertexColorMask(vp.settings.asobo.applies("ASOBO_material_windshield_v3"));

    int currentLayer = 0;
    shadowCasterLayer->prePass = [&vp, &currentLayer] { vp.sunShadowMap.bindLayer(currentLayer, false); };
    shadowCasterLayer->postPass = [&vp, vpW, vpH] {
        vp.sceneFbo.bind();
        glViewport(0, 0, vpW, vpH);
    };
    for (int c = 0; c < setup.count; ++c) {
        const ux3d::renderer::ShadowCascade& cascade = setup.cascades[static_cast<size_t>(c)];
        currentLayer = c;
        sunShadowCamera->setTransform(ux3d::math::mat4x4(glm::inverse(cascade.lightView)));
        sunShadowCamera->setProjection(ux3d::math::mat4x4(cascade.lightProjection));
        ux3d::renderer::RendererScene shadowScene;
        shadowScene.camera = sunShadowCamera.get();
        shadowScene.environmentMap = casterScene.environmentMap;
        shadowScene.lights = casterScene.lights;
        shadowScene.drawables = casters;
        gltfLayer->cullToClip(shadowScene.drawables, casterMapping, cascade.lightProjection * cascade.lightView);
        // Cleared here: a cascade without casters runs no pass, but must not keep an old map.
        vp.sunShadowMap.bindLayer(c, true);
        commandGenerator->update(shadowScene);
        glRenderer->setGpuFrameContext(frame.number, vp.id, "shadow");
        glRenderer->draw({ commandGenerator.get() });
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    vp.sceneFbo.bind();
    glViewport(0, 0, vpW, vpH);

    parameters.enabled = true;
    parameters.texture = vp.sunShadowMap.texture();
    parameters.cascadeCount = setup.count;
    parameters.filter = static_cast<int>(quality.shadowFilter);
    parameters.mapSize = static_cast<float>(input.resolution);
    parameters.normalBias = quality.shadowNormalBias;
    for (int c = 0; c < setup.count; ++c) {
        const auto& cascade = setup.cascades[static_cast<size_t>(c)];
        parameters.matrices[static_cast<size_t>(c)] = cascade.textureFromWorld;
        parameters.splits[c] = cascade.splitFar;
        parameters.texelSizes[c] = cascade.texelWorld;
    }
    parameters.nearDepth = setup.nearDepth;
    parameters.fadeStart = setup.fadeStart;
    parameters.fadeEnd = setup.fadeEnd;
    commandGenerator->setSunShadow(parameters);
    vp.sunShadowSetup = setup;
    vp.sunShadowActive = true;
}

nlohmann::json Application::sunShadowStateJson() const
{
    const ux3d::imgui::RenderQualitySettings configured = ux3d::imgui::RenderQuality::settings();
    const ux3d::imgui::RenderQualitySettings quality = ux3d::imgui::RenderQuality::effective();
    // Settings scopes: enabled = the scene's switch and the quality's gate (a viewport may override the scene's).
    auto& self = const_cast<Application&>(*this);
    const bool sceneShadows = self.getSceneEnvironment(self.sceneEnvironments, self.gltfModel, self.activeScene).effects.shadows;
    nlohmann::json out = {{"enabled", configured.shadows && sceneShadows},
                          {"scene", sceneShadows},
                          {"allowed", configured.shadows},
                          {"resolution", quality.shadowResolution},
                          {"cascadeCount", quality.shadowCascades},
                          {"maxDistance", agent::floatJson(quality.shadowDistance)},
                          {"filter", ux3d::imgui::RenderQuality::shadowFilterName(quality.shadowFilter)},
                          {"splitLambda", agent::floatJson(quality.shadowSplitLambda)},
                          {"normalBias", agent::floatJson(quality.shadowNormalBias)},
                          {"slopeBias", agent::floatJson(quality.shadowSlopeBias)},
                          {"constantBias", agent::floatJson(quality.shadowConstantBias)}};
    nlohmann::json viewportsJson = nlohmann::json::array();
    for (const auto& vp : viewports) {
        if (vp == nullptr) continue;
        nlohmann::json entry = {{"viewport", vp->id}, {"active", vp->sunShadowActive},
                                {"override", ux3d::imgui::effectOverrideName(vp->settings.shadowsOverride)},
                                {"enabled", vp->settings.sunShadows && configured.shadows},
                                {"memoryBytes", vp->sunShadowMap.bytes()}};
        if (vp->sunShadowActive) {
            // A6.6: which light casts the shadows (the sun, or the moon at night).
            entry["light"] = vp->sunShadowFromMoon ? "moon" : "sun";
            entry["lightDirection"] = {agent::floatJson(vp->sunShadowDirection.x), agent::floatJson(vp->sunShadowDirection.y),
                                       agent::floatJson(vp->sunShadowDirection.z)};
        }
        if (vp->sunShadowActive) {
            nlohmann::json cascades = nlohmann::json::array();
            for (int c = 0; c < vp->sunShadowSetup.count; ++c) {
                const auto& cascade = vp->sunShadowSetup.cascades[static_cast<size_t>(c)];
                cascades.push_back({{"near", agent::floatJson(cascade.splitNear)},
                                    {"far", agent::floatJson(cascade.splitFar)},
                                    {"radius", agent::floatJson(cascade.radius)},
                                    {"texelWorld", agent::floatJson(cascade.texelWorld)},
                                    {"depthRange", agent::floatJson(cascade.depthRange)},
                                    {"lightOrigin", {agent::floatJson(cascade.lightOrigin.x), agent::floatJson(cascade.lightOrigin.y)}}});
            }
            entry["cascades"] = cascades;
            entry["casters"] = shadowCasterLayer != nullptr ? shadowCasterLayer->getStats().casters : 0;
        }
        viewportsJson.push_back(entry);
    }
    out["viewports"] = viewportsJson;
    if (shadowCasterLayer != nullptr) {
        const auto& stats = shadowCasterLayer->getStats();
        out["excluded"] = {{"notOpaquePass", stats.notOpaquePass}, {"decals", stats.decals}, {"glass", stats.glass},
                           {"invisible", stats.invisible}, {"noCastShadow", stats.noCastShadow}};
        out["materialCopies"] = shadowCasterLayer->getMaterialCopies();
    }
    return out;
}

void Application::updateSkyOcclusion(const ux3d::imgui::SceneEnvironmentState& environment, FrameContext& frame)
{
    skyOcclusionVolume.pollTimers();
    bool anyOpen = false;
    for (const auto& vp : viewports) {
        anyOpen = anyOpen || (vp != nullptr && vp->isOpen);
    }
    // It attenuates the IBL only: nothing to do without it (the Low quality preset has no IBL).
    const bool wanted = ux3d::imgui::effectiveSkyOcclusion(environment) && environment.iblEnabled &&
                        ux3d::imgui::RenderQuality::allowIbl() && anyOpen && gltfLayer != nullptr && commandGenerator != nullptr &&
                        glRenderer != nullptr && skyOcclusionCasterLayer != nullptr && skyOcclusionCamera != nullptr &&
                        activeScene != nullptr && !skyOcclusionVolume.failed();
    // The viewports' render keys carry the volume in use (ViewportRenderKey::skyOcclusion), so a swap or a drop
    // redraws them without a scene revision (a RenderSettings mark would resynchronise the whole model).
    if (!wanted) {
        skyOcclusionWanted = false;
        // Nothing allocated while it is off (the programs are kept for the next bake).
        if (skyOcclusionVolume.busy()) skyOcclusionVolume.cancelJob();
        skyOcclusionVolume.dropFront();
        skyOcclusionModelKey = 0;
        skyOcclusionKey = 0;
        skyOcclusionEmptyKey = 0;
        return;
    }
    skyOcclusionWanted = true;

    // The model the volume belongs to (a new model drops the old volume at once) and everything the bake depends on.
    const auto mix = [](uint64_t hash, uint64_t value) { return (hash ^ value) * 0x100000001B3ULL + 0x9E3779B97F4A7C15ULL; };
    uint64_t modelKey = mix(0xCBF29CE484222325ULL, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(activeScene)));
    modelKey = mix(modelKey, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gltfLayer.get())));
    modelKey = mix(modelKey, sceneRevisions.revision(ux3d::imgui::SceneChange::Topology));
    if (modelKey != skyOcclusionModelKey) {
        if (skyOcclusionVolume.busy()) skyOcclusionVolume.cancelJob();
        skyOcclusionVolume.dropFront();
        skyOcclusionModelKey = modelKey;
        skyOcclusionEmptyKey = 0;
    }
    const auto quality = static_cast<ux3d::imgui::SkyOcclusionQuality>(
        std::clamp(ux3d::imgui::RenderQuality::settings().skyOcclusionQuality, 0, 2));
    uint64_t key = modelKey;
    for (const auto change : { ux3d::imgui::SceneChange::Visibility, ux3d::imgui::SceneChange::Transform,
                               ux3d::imgui::SceneChange::Material, ux3d::imgui::SceneChange::Extension }) {
        key = mix(key, sceneRevisions.revision(change));
    }
    key = mix(key, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gltfLayer->getIsolationRoot())));
    key = mix(key, gltfLayer->getIsolationIncludeChildren() ? 1u : 0u);
    key = mix(key, static_cast<uint64_t>(quality) + 1u);
    key = mix(key, environment.skyOcclusionOccluders ? 1u : 0u);
    skyOcclusionKey = key;
    // A key with nothing to bake: no volume (dropped below when it was found) and no per-frame bake scene.
    if (key == skyOcclusionEmptyKey) return;
    const bool upToDate = skyOcclusionVolume.ready() && skyOcclusionVolume.frontKey() == key;
    if (upToDate) {
        // Back to the state of the front volume while a newer bake ran: keep the front one.
        if (skyOcclusionVolume.busy()) skyOcclusionVolume.cancelJob();
        return;
    }

    // The bake scene: every primitive the traversal keeps (no culling), with the invisible materials and the environment
    // occluders included, so the caster layer decides. The viewports build their own scenes after this.
    ux3d::renderer::AsoboOptions options;
    options.setApplied("ASOBO_material_invisible", false);
    options.setApplied("ASOBO_material_environment_occluder", false);
    const ux3d::renderer::AsoboOptions savedAsobo = gltfLayer->asoboOptions;
    const ux3d::renderer::MaterialExtensionOptions savedExtensions = gltfLayer->materialExtensionOptions;
    const bool savedCulling = gltfLayer->frustumCulling;
    gltfLayer->frustumCulling = false;
    gltfLayer->asoboOptions = options;
    gltfLayer->materialExtensionOptions = ux3d::renderer::MaterialExtensionOptions();
    auto [bakeScene, bakeMapping] = gltfLayer->createScene(activeScene);
    ++skyOcclusionSceneBuilds;
    gltfLayer->asoboOptions = savedAsobo;
    gltfLayer->materialExtensionOptions = savedExtensions;
    gltfLayer->frustumCulling = savedCulling;

    // Casters: the sun's shadow caster rules (OPAQUE / MASK / dithered; no decals, glass, windshields or invisible
    // materials) without ASOBO_material_shadow_options (a part that casts no sun shadow still hides the sky), plus the
    // environment occluders.
    ux3d::renderer::ShadowCasterLayer::Options casterOptions;
    casterOptions.asoboEnabled = true;
    casterOptions.applyShadowOptions = false;
    casterOptions.applyMaterialCode = true;
    casterOptions.slopeBias = 0.0f;
    casterOptions.constantBias = 0.0f;
    casterOptions.includeEnvironmentOccluders = environment.skyOcclusionOccluders;
    uint64_t sourceRevision = gltfLayer->getGltfGeneration();
    for (const auto change : { ux3d::imgui::SceneChange::Material, ux3d::imgui::SceneChange::Extension }) {
        sourceRevision = sourceRevision * 0x9E3779B97F4A7C15ULL + sceneRevisions.revision(change);
    }
    casterOptions.sourceRevision = sourceRevision;
    casterOptions.modelGeneration = gltfLayer->getSceneGeneration();
    ux3d::core::LinkedList<std::tuple<ux3d::renderer::Mesh*, ux3d::renderer::Material*>> casters;
    skyOcclusionCasterLayer->collect(bakeScene.drawables,
                                     [&bakeMapping](const ux3d::renderer::Mesh* mesh) {
                                         return bakeMapping.find(const_cast<ux3d::renderer::Mesh*>(mesh)) != bakeMapping.end();
                                     },
                                     casterOptions, casters);
    skyOcclusionLastCasters = skyOcclusionCasterLayer->getStats().casters;
    skyOcclusionLastOccluders = skyOcclusionCasterLayer->getStats().occluders;

    if (!skyOcclusionVolume.busy() || skyOcclusionVolume.jobKey() != key) {
        // The volume covers the static POSITION boxes of the scene's drawables (the loose node extents for the ones that
        // cannot be bounded that way: skinned, morphed, quantized).
        ux3d::math::vec3 lo(0.0f);
        ux3d::math::vec3 hi(0.0f);
        bool bounded = false;
        const size_t unbounded = gltfLayer->boundDrawables(bakeScene.drawables, bakeMapping, lo, hi, bounded);
        glm::vec3 sceneMin(lo);
        glm::vec3 sceneMax(hi);
        if (unbounded > 0 || !bounded) {
            const ux3d::gltf::Extends& extents = getCachedSceneExtents(activeScene, gltfModel, gltfLayer.get());
            const glm::vec3 extentsMin(extents.min);
            const glm::vec3 extentsMax(extents.max);
            sceneMin = bounded ? glm::min(sceneMin, extentsMin) : extentsMin;
            sceneMax = bounded ? glm::max(sceneMax, extentsMax) : extentsMax;
        }
        if (!std::isfinite(sceneMin.x + sceneMin.y + sceneMin.z + sceneMax.x + sceneMax.y + sceneMax.z) ||
            sceneMax.x < sceneMin.x || sceneMax.y < sceneMin.y || sceneMax.z < sceneMin.z || casters.empty()) {
            // Nothing to bake (an empty scene, or no casters, e.g. an isolated glass part): no volume, the sky stays
            // open. The volume of the previous key is dropped (it belongs to geometry that is no longer there) and the
            // key is remembered, so the bake scene is not rebuilt every frame until something changes.
            if (skyOcclusionVolume.busy()) skyOcclusionVolume.cancelJob();
            skyOcclusionVolume.dropFront();
            skyOcclusionEmptyKey = key;
            return;
        }
        const ux3d::imgui::SkyOcclusionSpec spec = ux3d::imgui::skyOcclusionSpec(quality);
        skyOcclusionVolume.startJob(key, ux3d::imgui::SkyOcclusionVolume::fitBounds(sceneMin, sceneMax, spec), quality);
    }

    // Frame values the caster shader reads (getBaseColor, the A1.7 dither with a fixed pattern).
    commandGenerator->setSunShadow(ux3d::renderer::CommandGenerator::SunShadowParameters());
    commandGenerator->setAsoboAlphaDither(true);
    commandGenerator->setAsoboDitherFrame(0.0f);
    commandGenerator->setAsoboUVOptions(true);
    commandGenerator->setAsoboDetailMap(true);
    commandGenerator->setAsoboWindshieldVertexColorMask(true);

    GLint previousFramebuffer = 0;
    GLint previousViewport[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    int currentLayer = 0;
    skyOcclusionCasterLayer->prePass = [this, &currentLayer] { skyOcclusionVolume.bindDepthLayer(currentLayer, false); };
    skyOcclusionCasterLayer->postPass = [] {};
    const bool completed = skyOcclusionVolume.runFrame(
        [&](int layer, const ux3d::imgui::SkyOcclusionVolume::DirectionMatrices& matrices) {
            currentLayer = layer;
            skyOcclusionCamera->setTransform(ux3d::math::mat4x4(glm::inverse(matrices.view)));
            skyOcclusionCamera->setProjection(ux3d::math::mat4x4(matrices.projection));
            ux3d::renderer::RendererScene depthScene;
            depthScene.camera = skyOcclusionCamera.get();
            depthScene.environmentMap = bakeScene.environmentMap;
            depthScene.lights = bakeScene.lights;
            depthScene.drawables = casters;
            commandGenerator->update(depthScene);
            glRenderer->setGpuFrameContext(frame.number, 0, "skyocclusion");
            glRenderer->draw({ commandGenerator.get() });
        });
    skyOcclusionCasterLayer->prePass = nullptr;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    (void)completed; // the viewports see the new front volume through their render keys
}

nlohmann::json Application::skyOcclusionStateJson(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    const auto quality = static_cast<ux3d::imgui::SkyOcclusionQuality>(
        std::clamp(ux3d::imgui::RenderQuality::settings().skyOcclusionQuality, 0, 2));
    const ux3d::imgui::SkyOcclusionSpec spec = ux3d::imgui::skyOcclusionSpec(quality);
    const auto& stats = skyOcclusionVolume.stats();
    const auto vec3Json = [](const glm::vec3& v) {
        return nlohmann::json{agent::floatJson(v.x), agent::floatJson(v.y), agent::floatJson(v.z)};
    };
    nlohmann::json out = {
        {"setting", environment.skyOcclusion < 0 ? "auto" : environment.skyOcclusion != 0 ? "on" : "off"},
        {"enabled", ux3d::imgui::effectiveSkyOcclusion(environment)},
        {"active", skyOcclusionApplies()},
        {"quality", ux3d::imgui::skyOcclusionQualityName(quality)},
        {"directions", spec.directions},
        {"maxCells", spec.maxCells},
        {"mapSize", spec.mapSize},
        {"directionsPerFrame", spec.directionsPerFrame},
        {"occluders", environment.skyOcclusionOccluders},
        {"normalOffsetCells", ux3d::imgui::SkyOcclusionVolume::kNormalOffsetCells},
        {"openFraction", ux3d::imgui::SkyOcclusionVolume::kOpenFraction},
        {"baking", skyOcclusionVolume.busy()},
        {"progress", skyOcclusionVolume.busy() && skyOcclusionVolume.jobDirections() > 0
                         ? static_cast<double>(skyOcclusionVolume.jobDirectionsDone()) / skyOcclusionVolume.jobDirections()
                         : (skyOcclusionVolume.ready() ? 1.0 : 0.0)},
        {"jobMemoryBytes", skyOcclusionVolume.jobBytes()},
        {"casters", skyOcclusionLastCasters},
        {"occluderCasters", skyOcclusionLastOccluders},
        // The current key's state: a front volume of this key, or a key with nothing to bake (no volume).
        {"upToDate", skyOcclusionWanted && skyOcclusionVolume.ready() && skyOcclusionVolume.frontKey() == skyOcclusionKey},
        {"noCasters", skyOcclusionWanted && skyOcclusionKey != 0 && skyOcclusionKey == skyOcclusionEmptyKey},
        {"sceneBuilds", skyOcclusionSceneBuilds}};
    if (skyOcclusionVolume.ready()) {
        const auto& b = skyOcclusionVolume.frontBounds();
        out["volume"] = {{"dims", {b.dims.x, b.dims.y, b.dims.z}},
                         {"cells", static_cast<uint64_t>(b.dims.x) * b.dims.y * b.dims.z},
                         {"cell", agent::floatJson(b.cell)},
                         {"origin", vec3Json(b.origin)},
                         {"max", vec3Json(b.origin + glm::vec3(b.dims) * b.cell)},
                         {"quality", ux3d::imgui::skyOcclusionQualityName(skyOcclusionVolume.frontQuality())},
                         {"memoryBytes", skyOcclusionVolume.frontBytes()}};
    } else {
        out["volume"] = nullptr;
    }
    out["bake"] = {{"completed", stats.bakes},
                   {"aborted", stats.aborted},
                   {"frames", stats.lastFrames},
                   {"cpuMs", stats.lastCpuMs},
                   {"gpuMs", stats.lastGpuMs >= 0.0 ? nlohmann::json(stats.lastGpuMs) : nlohmann::json(nullptr)},
                   {"maxFrameCpuMs", stats.lastMaxFrameCpuMs},
                   {"maxFrameGpuMs", stats.lastMaxFrameGpuMs >= 0.0 ? nlohmann::json(stats.lastMaxFrameGpuMs) : nlohmann::json(nullptr)},
                   {"finalizeCpuMs", stats.lastFinalizeCpuMs}};
    nlohmann::json viewportsJson = nlohmann::json::array();
    for (const auto& vp : viewports) {
        if (vp == nullptr) continue;
        viewportsJson.push_back({{"viewport", vp->id}, {"applied", vp->skyOcclusionActive}});
    }
    out["viewports"] = viewportsJson;
    return out;
}

void Application::releaseDecalBuffer()
{
    decalBuffer.release();
    if (decalPrepassLayer != nullptr) {
        decalPrepassLayer->setDebugTargets(0, 0, 0, 0);
    }
    if (decalAccumulationLayer != nullptr) {
        decalAccumulationLayer->setG0Target(0);
        decalAccumulationLayer->setDepthTarget(0);
    }
    if (commandGenerator != nullptr) {
        commandGenerator->setDecalTargets(0, 0, 0, 0, 0, 0);
    }
}

} // namespace ux3d::imgui::application
