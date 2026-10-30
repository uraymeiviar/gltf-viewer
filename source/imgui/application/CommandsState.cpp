// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// get_state / profile-report JSON builders of the agent control channel: the session state, the global quality,
// the scene effects, SSAO, the ambient light and the sun.

#include "CommandsInternal.h"

namespace ux3d::imgui::application {

json Application::legacyStateJson()
{
    json st;
    st["model"] = currentFilePath.empty() ? modelToLoad : currentFilePath;
    st["selection"] = describeUiSelection(selection);
    st["isolation"] = (gltfLayer && gltfLayer->getIsolationRoot()) ? nodeName(gltfLayer->getIsolationRoot()) : "none";
    st["camera"] = getCameraJson(activeViewport);
    st["nodeCount"] = gltfModel.getNodes().size();
    st["materialCount"] = gltfModel.getElements<ux3d::gltf::Material>().size();
    return st;
}

json Application::agentStateJson()
{
    resolveAllViewportSettings();
    json st = legacyStateJson();
    st["modelLoaded"] = modelLoaded;
    st["loading"] = loadingActive;
    st["meshCount"] = gltfModel.getElements<ux3d::gltf::Mesh>().size();
    st["sceneCount"] = gltfModel.getElements<ux3d::gltf::Scene>().size();
    st["selectedNode"] = selection.type == ux3d::imgui::SelectionType::Node ? nodeRef(selection.selectedNode) : json(nullptr);
    if (gltfLayer && gltfLayer->getIsolationRoot()) {
        st["isolation"] = {{"node", nodeRef(gltfLayer->getIsolationRoot())},
                           {"withChildren", gltfLayer->getIsolationIncludeChildren()}};
    } else {
        st["isolation"] = nullptr;
    }
    st["activeViewport"] = activeViewport ? json(activeViewport->id) : json(nullptr);
    st["captures"] = {{"default", ux3d::imgui::CaptureSettings::defaultFolder()},
                      {"configured", ux3d::imgui::CaptureSettings::configuredFolder()},
                      {"effective", ux3d::imgui::CaptureSettings::effectiveFolder()}};
    json list = json::array();
    for (const auto& vp : viewports) {
        if (!vp) continue;
        const auto effective = ux3d::imgui::RenderQuality::effectiveViewport(vp->settings);
        const bool shadowsAllowed = ux3d::imgui::RenderQuality::effective().shadows;
        static const char* const aaNames[] = {"none", "msaa2", "msaa4", "msaa8"};
        static const char* const postNames[] = {"none", "fxaa", "taa"};
        list.push_back({{"id", vp->id},
                        {"name", vp->name},
                        {"open", vp->isOpen},
                        {"width", vp->sceneFbo.getWidth()},
                        {"height", vp->sceneFbo.getHeight()},
                        {"renderMode", renderModeName(vp->settings.renderMode)},
                        {"attachedCamera", vp->isAttachedToCamera ? json(vp->attachedCameraNodeName) : json(nullptr)},
                        // Settings scopes: this viewport's overrides of the scene's effects and what it renders with.
                        {"overrides", {{"shadows", ux3d::imgui::effectOverrideName(vp->settings.shadowsOverride)},
                                       {"ssr", ux3d::imgui::effectOverrideName(vp->settings.ssrOverride)},
                                       {"ssao", ux3d::imgui::effectOverrideName(vp->settings.ssaoOverride)}}},
                        {"effective", {{"shadows", vp->settings.sunShadows && shadowsAllowed},
                                       {"ssr", effective.screenSpaceReflections},
                                       {"ssao", vp->settings.ssao.enabled},
                                       {"msaa", aaNames[std::clamp(effective.effectiveMsaaIdx(), 0, 3)]},
                                       {"postAA", postNames[std::clamp(effective.effectivePostAAIdx(), 0, 2)]},
                                       {"ssrQuality", effective.ssrQuality},
                                       // The global glTF extension set as this viewport renders with it (always the same).
                                       {"asobo", {{"enabled", vp->settings.asobo.enabled},
                                                  {"disabled", vp->settings.asobo.disabled}}}}}});
    }
    st["viewports"] = list;
    if (activeViewport != nullptr) st["renderMode"] = renderModeName(activeViewport->settings.renderMode);
    const auto& report = ux3d::imgui::GltfValidator::instance().getLastReport();
    st["validation"] = {{"errors", report.errorCount}, {"warnings", report.warningCount}, {"infos", report.infoCount}};
    st["persist"] = agentPersist;
    st["frame"] = frameCount;
    st["glErrors"] = s_glErrorTotal;
    // Renderer synchronisations from the glTF model (full or material-only) and the frames that needed none.
    st["sync"] = {{"syncs", sceneSyncCount},
                  {"skippedSyncs", skippedSceneSyncCount},
                  {"animationOnlySyncs", animationOnlySyncCount},
                  {"marks", sceneRevisions.markCount()}};
    st["sun"] = sunStateJson(getSceneEnvironment(sceneEnvironments, gltfModel, activeScene));
    st["ambient"] = ambientStateJson(getSceneEnvironment(sceneEnvironments, gltfModel, activeScene));
    const auto& environment = getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
    st["msfsEmissive"] = {{"effectiveScale", ux3d::imgui::effectiveMsfsEmissiveScale(environment)},
                          {"overridden", environment.msfsEmissiveScaleOverride},
                          {"overrideScale", environment.msfsEmissiveScaleOverride ? json(environment.msfsEmissiveScale) : json(nullptr)}};
    st["timeOfDay"] = timeOfDayStateJson(getSceneEnvironment(sceneEnvironments, gltfModel, activeScene));
    st["sky"] = skyStateJson(getSceneEnvironment(sceneEnvironments, gltfModel, activeScene));
    st["night"] = nightStateJson(getSceneEnvironment(sceneEnvironments, gltfModel, activeScene));
    st["ssao"] = ssaoStateJson();
    // Settings scopes: renderer-wide quality, the active scene's effects.
    st["quality"] = qualityStateJson();
    st["gltfExtensions"] = ux3d::imgui::GltfExtensionSettings::instance().stateJson();
    st["environment"] = {{"effects", effectsStateJson(getSceneEnvironment(sceneEnvironments, gltfModel, activeScene))}};
    st["skyOcclusion"] = skyOcclusionStateJson(getSceneEnvironment(sceneEnvironments, gltfModel, activeScene));
    if (activeViewport != nullptr) {
        st["ground"] = groundStateJson(*activeViewport);
    }
    // A3.7: the file's sky portals (world centre, direction into the cabin, disc radius) and how many lit the
    // active viewport's last render.
    if (gltfLayer != nullptr && gltfLayer->hasSkyPortals()) {
        json portals = json::array();
        for (const auto& [node, source] : gltfLayer->getSkyPortalSources()) {
            const glm::mat4 world(gltfLayer->getWorldTransform(node));
            const glm::vec3 position(world[3]);
            const glm::vec3 direction = glm::normalize(glm::vec3(world[2]));
            portals.push_back({{"name", node->hasName() ? json(std::string(node->getName().c_str())) : json(nullptr)},
                               {"position", {agent::floatJson(position.x), agent::floatJson(position.y), agent::floatJson(position.z)}},
                               {"direction", {agent::floatJson(direction.x), agent::floatJson(direction.y), agent::floatJson(direction.z)}},
                               {"radius", agent::floatJson(source.radiusMetres)},
                               {"innerRadius", agent::floatJson(source.innerRadiusMetres)}});
        }
        std::sort(portals.begin(), portals.end(), [](const json& a, const json& b) { return a["name"].dump() < b["name"].dump(); });
        st["skyPortals"] = {{"portals", portals},
                            {"active", activeViewport != nullptr ? activeViewport->skyPortalCount : 0}};
    }
    return st;
}

json Application::qualityStateJson() const
{
    using ux3d::imgui::RenderQuality;
    static const char* const levels[] = {"low", "medium", "high"};
    static const char* const aaNames[] = {"0", "2", "4", "8"};
    static const char* const postNames[] = {"none", "fxaa", "taa"};
    const auto describe = [&](const ux3d::imgui::RenderQualitySettings& q) {
        return json{{"msaa", aaNames[std::clamp(q.msaaIdx, 0, 3)]},
                    {"postAA", postNames[std::clamp(q.postAAIdx, 0, 2)]},
                    {"ssrQuality", levels[std::clamp(q.ssrQuality, 0, 2)]},
                    {"ssaoQuality", levels[std::clamp(q.ssaoQuality, 0, 2)]},
                    {"ssaoResolution", q.ssaoHalfResolution ? "half" : "full"},
                    {"skyOcclusionQuality", levels[std::clamp(q.skyOcclusionQuality, 0, 2)]},
                    {"oceanQuality", levels[std::clamp(q.oceanQuality, 0, 2)]},
                    {"renderScale", agent::floatJson(q.renderScale)},
                    {"gridMode", q.gridMode == ux3d::imgui::GridMode::PlaneMesh ? "plane" : "ray"},
                    {"iblCubeSize", q.iblCubeSize},
                    {"shadowsAllowed", q.shadows},
                    {"shadowResolution", q.shadowResolution},
                    {"shadowCascades", q.shadowCascades},
                    {"shadowFilter", RenderQuality::shadowFilterName(q.shadowFilter)}};
    };
    return {{"profile", RenderQuality::profileName(RenderQuality::selectedProfile())},
            {"auto", RenderQuality::autoActive()},
            {"configured", describe(RenderQuality::settings())},
            {"effective", describe(RenderQuality::effective())},
            {"ssrAllowed", RenderQuality::allowSsr()}};
}

json Application::effectsStateJson(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    const auto& fx = environment.effects;
    return {{"shadows", fx.shadows},
            {"ssr", fx.ssr},
            {"interiorSkyReflection", agent::floatJson(fx.interiorSkyReflection)},
            {"ssao", fx.ssao},
            {"ssaoRadius", agent::floatJson(fx.ssaoRadius)},
            {"ssaoIntensity", agent::floatJson(fx.ssaoIntensity)},
            {"windshieldIcing", agent::floatJson(fx.windshieldIcing)},
            {"skyPortalStrength", agent::floatJson(fx.skyPortalStrength)},
            {"decalToleranceMm", agent::floatJson(fx.decalDepthToleranceMm)}};
}

json Application::ssaoStateJson() const
{
    static const char* const qualities[] = {"low", "medium", "high"};
    json list = json::array();
    for (const auto& vp : viewports) {
        if (!vp) continue;
        const auto& settings = vp->settings.ssao;
        const auto& ao = vp->ambientOcclusion;
        list.push_back({{"viewport", vp->id},
                        {"enabled", settings.enabled},
                        {"override", ux3d::imgui::effectOverrideName(vp->settings.ssaoOverride)},
                        {"active", vp->ssaoActive},
                        {"radius", agent::floatJson(settings.radius)},
                        {"intensity", agent::floatJson(settings.intensity)},
                        {"quality", qualities[static_cast<size_t>(settings.quality)]},
                        {"slices", ux3d::imgui::SsaoSettings::sliceCount(settings.quality)},
                        {"steps", ux3d::imgui::SsaoSettings::stepCount(settings.quality)},
                        {"resolution", settings.halfResolution ? "half" : "full"},
                        {"aoWidth", ao.getAoWidth()},
                        {"aoHeight", ao.getAoHeight()},
                        {"ownPrepass", ao.hasPrepassTarget()},
                        {"memoryBytes", ao.getMemoryBytes()},
                        {"computes", ao.getComputeCount()},
                        {"gpuMs", ao.getLastGpuMs() >= 0.0 ? json(ao.getLastGpuMs()) : json(nullptr)},
                        {"gpuMsAverage", ao.getAverageGpuMs() >= 0.0 ? json(ao.getAverageGpuMs()) : json(nullptr)}});
    }
    return {{"viewports", list}};
}

float Application::outdoorIlluminance(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    using namespace ux3d::imgui;
    auto luminance = [](const glm::vec3& c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; };
    const EffectiveSun sun = effectiveSun(environment);
    float illuminance = sun.enabled ? luminance(sun.illuminance) * std::max(sun.direction.y, 0.0f) : 0.0f;
    if (environment.environmentSource == static_cast<int>(EnvironmentSource::ProceduralSky)) {
        const glm::vec3 direction = skySunDirection(environment);
        illuminance += night::clearSkyIlluminance(glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f)))) +
                       night::airglowIlluminance(environment.nightSky.airglow);
        const night::MoonEvaluation moon = moonEvaluation(environment);
        if (moon.available && moon.aboveHorizon) {
            illuminance += luminance(moon.illuminance) * std::max(environment.nightSky.moonlightScale, 0.0f) *
                           std::max(moon.direction.y, 0.0f);
        }
    } else if (gltfLayer != nullptr && !gltfLayer->isExternalEnvironment()) {
        illuminance += luminance(gltfLayer->getEnvironmentHorizontalIlluminance()) * environment.hdrIntensity;
    }
    return std::max(illuminance, 0.0f);
}

json Application::ambientStateJson(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    auto vec3Json = [](const glm::vec3& v) {
        return json{agent::floatJson(v.x), agent::floatJson(v.y), agent::floatJson(v.z)};
    };
    // A3.8: radiance is what the shaders receive (zero while off); luminance in cd/m^2 and in environment units.
    using namespace ux3d::imgui;
    const float outdoor = outdoorIlluminance(environment);
    const bool automatic = effectiveAmbientAuto(environment);
    const float level = automatic ? environment.ambientFill * outdoor / 3.14159265358979323846f :
                                   environment.ambientLuminance / kAmbientUnitLuminance;
    const glm::vec3 radiance = ambientRadiance(environment, outdoor);
    return json{{"enabled", effectiveAmbientEnabled(environment)},
                {"setting", environment.ambient < 0 ? "auto" : environment.ambient != 0 ? "on" : "off"},
                {"mode", automatic ? "auto" : "manual"},
                {"modeSetting", environment.ambientMode < 0 ? "default" : automatic ? "auto" : "manual"},
                {"fill", agent::floatJson(environment.ambientFill)},
                {"outdoorIlluminance", agent::floatJson(outdoor)},
                {"outdoorIlluminanceLux", agent::floatJson(outdoor * kAmbientUnitLuminance)},
                {"interiorWeighted", automatic && skyOcclusionWanted && skyOcclusionVolume.ready()},
                {"color", vec3Json(automatic ? glm::vec3(1.0f) : environment.ambientColor)},
                {"intensity", agent::floatJson(level * kAmbientUnitLuminance)},
                {"intensityEnvironmentUnits", agent::floatJson(level)},
                {"unitLuminance", agent::floatJson(ux3d::imgui::kAmbientUnitLuminance)},
                {"radiance", vec3Json(radiance)}};
}

json Application::sunStateJson(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    auto vec3Json = [](const glm::vec3& v) {
        return json{agent::floatJson(v.x), agent::floatJson(v.y), agent::floatJson(v.z)};
    };
    const EffectiveSun sun = effectiveSun(environment);
    float azimuth = 0.0f, elevation = 0.0f;
    ux3d::imgui::sunAnglesFromDirection(sun.direction, azimuth, elevation);
    const bool proceduralSky = environment.environmentSource == static_cast<int>(ux3d::imgui::EnvironmentSource::ProceduralSky);
    const auto source = ux3d::imgui::effectiveSunSource(environment);
    json out = {{"enabled", source != ux3d::imgui::SunSource::Off},
                {"source", {{"requested", ux3d::imgui::sunSourceName(proceduralSky && environment.skySunSource >= 0
                    ? static_cast<ux3d::imgui::SunSource>(environment.skySunSource) : environment.sunSource)},
                            {"effective", ux3d::imgui::sunSourceName(source)},
                            {"hdr", ux3d::imgui::sunSourceName(environment.sunSource)},
                            {"options", proceduralSky ? json{"manual", "clock"} : json{"off", "hdr", "manual", "clock"}}}},
                {"mode", proceduralSky ? "sky" : source == ux3d::imgui::SunSource::Clock ? "time_of_day"
                         : source == ux3d::imgui::SunSource::FromHdr ? "hdr" : "manual"},
                {"active", sun.enabled},
                {"direction", vec3Json(sun.direction)},
                {"azimuth", agent::floatJson(azimuth)},
                {"elevation", agent::floatJson(elevation)},
                {"illuminance", vec3Json(sun.illuminance)},
                {"angularRadius", agent::floatJson(environment.sunAngularRadius)}};
    json detected = nullptr;
    if (gltfLayer != nullptr) {
        const ux3d::renderer::EnvironmentSun& found = gltfLayer->getEnvironmentSun();
        detected = {{"found", found.found}, {"removed", found.removed},
                    {"skyLevel", agent::floatJson(found.skyLevel)}, {"threshold", agent::floatJson(found.threshold)}};
        if (found.found) {
            float cubeAzimuth = 0.0f, cubeElevation = 0.0f;
            ux3d::imgui::sunAnglesFromDirection(found.direction, cubeAzimuth, cubeElevation);
            const glm::mat3 rotation = ux3d::renderer::CommandGenerator::environmentRotationMatrix(environment.hdrRotation);
            const glm::vec3 world = glm::normalize(glm::transpose(rotation) * found.direction);
            float worldAzimuth = 0.0f, worldElevation = 0.0f;
            ux3d::imgui::sunAnglesFromDirection(world, worldAzimuth, worldElevation);
            detected["cubeDirection"] = vec3Json(found.direction);
            detected["cubeAzimuth"] = agent::floatJson(cubeAzimuth);
            detected["worldDirection"] = vec3Json(world);
            detected["azimuth"] = agent::floatJson(worldAzimuth);
            detected["elevation"] = agent::floatJson(worldElevation);
            detected["color"] = vec3Json(found.color);
            detected["illuminance"] = agent::floatJson(found.illuminance);
            detected["peakLuminance"] = agent::floatJson(found.peakLuminance);
            detected["solidAngle"] = agent::floatJson(found.solidAngle);
            detected["texels"] = found.texelCount;
        }
    }
    out["hdr"] = detected;
    // A6.7: the background sun disk.
    {
        const ux3d::imgui::SunDiskEvaluation disk = sunDiskEvaluation(environment);
        out["disk"] = {{"enabled", environment.sunDisk.enabled},
                       {"visible", disk.visible},
                       {"scale", agent::floatJson(environment.sunDisk.scale)},
                       {"limbDarkening", environment.sunDisk.limbDarkening},
                       {"angularRadius", agent::floatJson(glm::degrees(disk.angularRadius))},
                       {"meanRadiance", vec3Json(disk.meanRadiance)},
                       {"centreRadiance", vec3Json(disk.centreRadiance)},
                       {"rangeScale", agent::floatJson(disk.rangeScale)}};
    }
    // A3.2: the sun shadow settings and each viewport's cascades.
    out["shadows"] = sunShadowStateJson();
    return out;
}

} // namespace ux3d::imgui::application
