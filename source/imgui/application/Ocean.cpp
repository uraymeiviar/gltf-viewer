// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
//
// A6.8: the ocean ground (docs/ASOBO_EXTENSIONS_PHASE_6.md): its clock, the material values of a viewport's ocean draw
// and its get_state object. The simulation itself is ux3d::imgui::OceanSimulation.

#include "Application.h"
#include "imgui/OceanSimulation.h"
#include "imgui/RenderQuality.h"
#include <algorithm>
#include <cmath>

namespace ux3d::imgui::application {

namespace {
constexpr float kOceanRings = 192.0f; // GltfResourceManager::initOcean
}

double Application::oceanTimeFor(const ux3d::imgui::ViewportInstance& vp) const
{
    const auto& ocean = vp.settings.ground.ocean;
    return ocean.time >= 0.0f ? static_cast<double>(ocean.time) : vp.oceanClock;
}

void Application::updateOceanClock(float deltaTime)
{
    // Capture runs are deterministic: the clock moves only by --fixed-frame-time steps.
    if (s_deterministicExposure && s_fixedFrameTime < 0.0f) return;
    for (auto& vp : viewports) {
        const auto& ocean = vp->settings.ground.ocean;
        if (vp->isOpen && vp->settings.ground.mode == ux3d::imgui::GroundMode::Ocean && ocean.play && ocean.time < 0.0f) {
            vp->oceanClock += static_cast<double>(deltaTime);
        }
    }
}

void Application::setOceanUniforms(const ux3d::imgui::ViewportInstance& vp, ux3d::renderer::Material& material,
                                   const ux3d::imgui::SceneEnvironmentState& environment, float seaLevel,
                                   const glm::vec3& cameraEye, bool skyOcclusion)
{
    const auto& simulation = *vp.ocean;
    const auto& ocean = vp.settings.ground.ocean;
    const auto& ground = vp.settings.ground;
    using ux3d::math::vec3;
    using ux3d::math::vec4;

    // The polar grid: rings scaled by the camera's height above the sea, out to 2000 heights (at least 100 km), so its
    // edge lies within 0.03 degrees of the horizon.
    const float height = std::abs(cameraEye.y - seaLevel);
    const float scale = 0.5f * std::max(height, 1.0f);
    const float reach = std::max(1.0e5f, 2000.0f * height);
    material.setUniform("u_OceanGrid", vec4(seaLevel, scale, std::log(1.0f + reach / scale), kOceanRings - 1.0f));

    const int cascades = simulation.cascadeCount();
    material.setUniform("u_OceanInfo", vec4(static_cast<float>(cascades), static_cast<float>(simulation.resolution()),
                                            0.0f, simulation.tailSlopeVariance()));
    const auto foam = ux3d::imgui::oceanFoamParameters(ocean, simulation.key().choppiness, simulation.key().windSpeed,
                                          simulation.resolvedSlopeVariance());
    float totalSlope = 0.0f;
    for (int c = 0; c < cascades; ++c) totalSlope += simulation.cascade(c).slopeVariance;
    material.setUniform("u_OceanFoam", vec4(static_cast<float>(foam.z), static_cast<float>(foam.sigma),
                                            1.0f / std::max(totalSlope, 1.0e-6f), 0.7f));
    static const char* const cascadeNames[] = { "u_OceanCascade0", "u_OceanCascade1", "u_OceanCascade2", "u_OceanCascade3" };
    static const char* const slopeA[] = { "u_OceanSlopeA0", "u_OceanSlopeA1", "u_OceanSlopeA2", "u_OceanSlopeA3" };
    static const char* const slopeB[] = { "u_OceanSlopeB0", "u_OceanSlopeB1", "u_OceanSlopeB2", "u_OceanSlopeB3" };
    const double time = vp.oceanTime;
    const int frames = simulation.frames();
    for (int c = 0; c < ux3d::imgui::OceanSimulation::kMaxCascades; ++c) {
        if (c >= cascades) {
            material.setUniform(cascadeNames[c], vec4(0.0f));
            material.setUniform(slopeA[c], vec4(0.0f));
            material.setUniform(slopeB[c], vec4(0.0f));
            continue;
        }
        const auto& cascade = simulation.cascade(c);
        const double position = simulation.framePosition(c, time);
        const int frameA = std::min(static_cast<int>(std::floor(position)), frames - 1);
        const int frameB = (frameA + 1) % frames;
        const float blend = static_cast<float>(position - frameA);
        material.setUniform(cascadeNames[c], vec4(1.0f / cascade.patchSize, static_cast<float>(c * frames + frameA),
                                                  static_cast<float>(c * frames + frameB), blend));
        const auto& u = cascade.unresolvedSlopeVariance;
        material.setUniform(slopeA[c], vec4(u[0], u[1], u[2], u[3]));
        material.setUniform(slopeB[c], vec4(u[4], u[5], u[6], cascade.slopeVariance));
    }
    material.setUniform("u_OceanColor", vec3(ocean.color.r, ocean.color.g, ocean.color.b));
    material.setUniform("u_OceanParams", vec4(simulation.significantWaveHeight(), 0.0f, 0.0f, 0.0f));

    // Model interiors without the sky-visibility volume: the model's bounds while the camera is inside them.
    vec4 interiorMin(0.0f);
    vec4 interiorMax(0.0f);
    if (!skyOcclusion && activeScene != nullptr && gltfLayer != nullptr) {
        const ux3d::gltf::Extends& extents = getCachedSceneExtents(activeScene, gltfModel, gltfLayer.get());
        const glm::vec3 lo(extents.min);
        const glm::vec3 hi(extents.max);
        const bool valid = std::isfinite(lo.x + lo.y + lo.z + hi.x + hi.y + hi.z) && hi.x >= lo.x && hi.y >= lo.y && hi.z >= lo.z;
        const bool inside = valid && cameraEye.x > lo.x && cameraEye.y > lo.y && cameraEye.z > lo.z && cameraEye.x < hi.x &&
                            cameraEye.y < hi.y && cameraEye.z < hi.z;
        if (inside) {
            interiorMin = vec4(lo.x, lo.y, lo.z, 1.0f);
            interiorMax = vec4(hi.x, hi.y, hi.z, 0.0f);
        }
    }
    material.setUniform("u_OceanInteriorMin", interiorMin);
    material.setUniform("u_OceanInteriorMax", interiorMax);

    // Horizon fog and environment as the ground plane (Viewports.cpp).
    const auto* camera = gltfLayer->getActiveCamera();
    const float farPlane = camera != nullptr ? std::max(camera->getZFarPlane(), 1.0e-3f) : 1000.0f;
    const float visibility = ground.haze > 0.0f ? ground.haze : 0.9f * farPlane;
    const float extinction = 3.912f / std::max(visibility, 1.0e-3f);
    const bool hasEnvironment = gltfLayer->getActiveEnvironmentMap() != nullptr;
    const bool hazeFromEnvironment = hasEnvironment && ux3d::imgui::RenderQuality::allowSkybox(environment.showBackground);
    material.defines.defineIf(hasEnvironment, "GROUND_ENV");
    material.defines.defineIf(skyOcclusion, "SKY_OCCLUSION");
    material.setUniform("u_GroundHaze", ux3d::math::vec2(extinction, hazeFromEnvironment ? 1.0f : 0.0f));
    material.setUniform("u_GroundHazeColor", vec3(environment.clearColor.r, environment.clearColor.g, environment.clearColor.b));
}

nlohmann::json Application::oceanStateJson(const ux3d::imgui::ViewportInstance& vp) const
{
    namespace agent = ux3d::imgui::agent;
    using json = nlohmann::json;
    const auto& ocean = vp.settings.ground.ocean;
    static const char* const qualities[] = { "low", "medium", "high" };
    const auto info = ux3d::imgui::OceanSimulation::qualityInfo(ocean.quality, ocean.live);
    json out = {{"available", ux3d::imgui::oceanAvailable()},
                {"active", vp.oceanActive},
                {"mode", ocean.live ? "live" : "loop"},
                {"quality", qualities[static_cast<size_t>(ocean.quality)]},
                {"windSpeed", agent::floatJson(ocean.windSpeed)},
                {"windDirection", agent::floatJson(ocean.windDirection)},
                {"fetch", agent::floatJson(ocean.fetch)},
                {"choppiness", agent::floatJson(ocean.choppiness)},
                {"swell", agent::floatJson(ocean.swell)},
                {"foam", agent::floatJson(ocean.foam)},
                {"color", {agent::floatJson(ocean.color.r), agent::floatJson(ocean.color.g), agent::floatJson(ocean.color.b)}},
                {"play", ocean.play},
                {"time", vp.oceanTime},
                {"timePinned", ocean.time >= 0.0f},
                {"period", agent::floatJson(ocean.period)},
                {"cascades", info.cascades},
                {"resolution", info.resolution},
                {"frames", info.frames},
                {"reflections", "sky/IBL only (no ocean SSR)"}};
    const auto* simulation = vp.ocean.get();
    if (simulation == nullptr) {
        out["ready"] = false;
        out["busy"] = false;
        out["progress"] = 0.0;
        out["memoryBytes"] = 0;
        return out;
    }
    const auto& stats = simulation->stats();
    out["ready"] = simulation->ready() && !simulation->busy();
    out["busy"] = simulation->busy();
    out["progress"] = simulation->progress();
    out["version"] = simulation->version();
    out["memoryBytes"] = simulation->memoryBytes();
    out["liveSet"] = simulation->live();
    out["significantWaveHeight"] = agent::floatJson(simulation->significantWaveHeight());
    out["tailSlopeVariance"] = agent::floatJson(simulation->tailSlopeVariance());
    out["resolvedSlopeVariance"] = agent::floatJson(simulation->resolvedSlopeVariance());
    if (simulation->ready()) {
        const auto foam = ux3d::imgui::oceanFoamParameters(ocean, simulation->key().choppiness, simulation->key().windSpeed,
                                                          simulation->resolvedSlopeVariance());
        out["foamParameters"] = {{"coverageTarget", agent::floatJson(static_cast<float>(foam.coverage))},
                                 {"sigma", agent::floatJson(static_cast<float>(foam.sigma))},
                                 {"z", agent::floatJson(static_cast<float>(foam.z))},
                                 {"lambda", agent::floatJson(static_cast<float>(foam.lambda))}};
    }
    json cascades = json::array();
    for (int c = 0; c < simulation->cascadeCount(); ++c) {
        const auto& cascade = simulation->cascade(c);
        cascades.push_back({{"patchSize", agent::floatJson(cascade.patchSize)},
                            {"kLow", agent::floatJson(cascade.kLow)},
                            {"kHigh", agent::floatJson(cascade.kHigh)},
                            {"loopDivisor", cascade.loopDivisor},
                            {"slopeVariance", agent::floatJson(cascade.slopeVariance)},
                            {"heightVariance", agent::floatJson(cascade.heightVariance)},
                            {"framePosition", simulation->framePosition(c, vp.oceanTime)}});
    }
    out["cascadeInfo"] = cascades;
    out["precompute"] = {{"wallMs", stats.lastPrecomputeMs},
                         {"gpuMs", stats.lastPrecomputeGpuMs},
                         {"appFrames", stats.lastPrecomputeFrames},
                         {"sets", stats.sets},
                         {"aborted", stats.aborted},
                         {"lastSliceGpuMs", stats.lastSliceGpuMs},
                         {"maxSliceGpuMs", stats.maxSliceGpuMs},
                         {"slicesGpuMs", stats.slicesGpuMs},
                         {"lastSliceItems", stats.lastSliceItems}};
    out["live"] = {{"steps", stats.liveSteps},
                   {"measured", stats.liveMeasured},
                   {"gpuMs", stats.liveGpuMs},
                   {"gpuAverageMs", stats.liveGpuAverageMs},
                   {"gpuMaxMs", stats.liveGpuMaxMs}};
    return out;
}

nlohmann::json Application::groundStateJson(const ux3d::imgui::ViewportInstance& vp)
{
    namespace agent = ux3d::imgui::agent;
    using json = nlohmann::json;
    // A3.6: the viewport's ground. height/albedo are what the last render used (the albedo is what A6.2 reads);
    // modelBase is the scene's lowest point whatever the height mode.
    const auto& ground = vp.settings.ground;
    static const char* const modes[] = {"grid", "plane", "ocean"};
    static const char* const heightModes[] = {"model_base", "zero", "custom"};
    float modelBase = 0.0f;
    const bool hasBase = activeScene != nullptr && getCachedModelBaseHeight(activeScene, gltfModel, gltfLayer.get(), modelBase);
    json out = {{"mode", modes[static_cast<size_t>(ground.mode)]},
                {"heightMode", heightModes[static_cast<size_t>(ground.heightMode)]},
                {"preset", ground.preset == ux3d::imgui::GroundPreset::Custom
                               ? json("custom")
                               : json(std::string(ux3d::imgui::groundPresets()[static_cast<size_t>(ground.preset)].key))},
                {"active", vp.groundPlaneActive},
                {"height", agent::floatJson(vp.groundPlaneHeight)},
                {"modelBase", hasBase ? json(agent::floatJson(modelBase)) : json(nullptr)},
                {"albedo", {agent::floatJson(ground.albedo.r), agent::floatJson(ground.albedo.g), agent::floatJson(ground.albedo.b)}},
                {"roughness", agent::floatJson(ground.roughness)},
                {"gridOnGround", ground.showGrid}};
    out["ocean"] = oceanStateJson(vp); // A6.8
    return out;
}

} // namespace ux3d::imgui::application
