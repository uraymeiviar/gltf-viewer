// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Scene environment evaluation shared by rendering and get_state: the effective sun and directional light, the sun
// disk, the moon, the exposure plan and night state, the procedural sky (A6.2 / A6.3) and the time-of-day clock
// (A6.1).

#include "CommandsInternal.h"

namespace ux3d::imgui::application {

EffectiveSun Application::effectiveSun(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    EffectiveSun sun;
    const auto source = ux3d::imgui::effectiveSunSource(environment);
    if (environment.environmentSource == static_cast<int>(ux3d::imgui::EnvironmentSource::ProceduralSky)) {
        // A6.2: the sun is always on; its direction is the time-of-day sun (or the Manual angles), its colour and
        // illuminance the top-of-atmosphere sun through the atmosphere towards it (reddened and dimmed near the
        // horizon), times the A6.1 fade while the clock drives it.
        float fade = 1.0f;
        if (source == ux3d::imgui::SunSource::Clock) {
            const ux3d::imgui::TimeOfDayEvaluation tod = ux3d::imgui::evaluateTimeOfDay(environment.clock);
            sun.direction = ux3d::imgui::sunDirectionFromAngles(tod.gliderAzimuth, tod.gliderElevation);
            fade = tod.fade;
        } else {
            sun.direction = ux3d::imgui::sunDirectionFromAngles(environment.sunAzimuth, environment.sunElevation);
        }
        sun.illuminance = ux3d::imgui::skySunIlluminance(skyAtmosphere(environment), sun.direction) * fade;
        sun.enabled = 0.2126f * sun.illuminance.r + 0.7152f * sun.illuminance.g + 0.0722f * sun.illuminance.b > 0.0f;
        return sun;
    }
    if (source == ux3d::imgui::SunSource::Clock) {
        // A6.1: the solar position through the heading; colour and intensity are the Manual sun's, faded to 0
        // across -1..+1 degrees of lighting elevation (the sun is disabled once the fade reaches 0).
        const ux3d::imgui::TimeOfDayEvaluation tod = ux3d::imgui::evaluateTimeOfDay(environment.clock);
        sun.direction = ux3d::imgui::sunDirectionFromAngles(tod.gliderAzimuth, tod.gliderElevation);
        sun.illuminance = environment.sunColor * (environment.sunIntensity * tod.fade);
        sun.enabled = tod.fade > 0.0f;
        return sun;
    }
    if (source == ux3d::imgui::SunSource::Off) return sun;
    if (source == ux3d::imgui::SunSource::FromHdr) {
        if (gltfLayer == nullptr || commandGenerator == nullptr) return sun;
        const ux3d::renderer::EnvironmentSun& found = gltfLayer->getEnvironmentSun();
        if (!found.found) return sun;
        // The shaders sample the cube map at u_EnvRotation * worldDirection, so the world direction of
        // the panorama's sun is the transpose (inverse) rotation of its cube-map direction.
        const glm::mat3 rotation = ux3d::renderer::CommandGenerator::environmentRotationMatrix(environment.hdrRotation);
        sun.direction = glm::normalize(glm::transpose(rotation) * found.direction);
        sun.illuminance = found.color * (found.illuminance * environment.hdrIntensity);
    } else {
        sun.direction = ux3d::imgui::sunDirectionFromAngles(environment.sunAzimuth, environment.sunElevation);
        sun.illuminance = environment.sunColor * environment.sunIntensity;
    }
    sun.enabled = true;
    return sun;
}

ux3d::imgui::SunDiskEvaluation Application::sunDiskEvaluation(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    const EffectiveSun sun = effectiveSun(environment);
    const bool background = ux3d::imgui::RenderQuality::allowSkybox(environment.showBackground);
    return ux3d::imgui::evaluateSunDisk(environment.sunDisk, sun.enabled && background, sun.illuminance,
                                        environment.sunAngularRadius);
}

ux3d::imgui::night::MoonEvaluation Application::moonEvaluation(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    using namespace ux3d::imgui;
    const bool procedural = environment.environmentSource == static_cast<int>(EnvironmentSource::ProceduralSky);
    // The moon needs the clock (its position, and its phase against the clock's sun); with the Manual sun there is none.
    if (!procedural || !environment.clock.enabled) return night::MoonEvaluation();
    const TimeOfDayEvaluation tod = evaluateTimeOfDay(environment.clock);
    return night::evaluateMoon(environment.clock, tod, skyAtmosphere(environment), environment.nightSky.moonEnabled);
}

EffectiveSun Application::effectiveDirectionalLight(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    EffectiveSun light = effectiveSun(environment);
    light.angularRadius = glm::radians(std::clamp(environment.sunAngularRadius, 0.0f, 10.0f));
    const ux3d::imgui::night::MoonEvaluation moon = moonEvaluation(environment);
    if (!moon.available || !moon.aboveHorizon) return light;
    const glm::vec3 moonlight = moon.illuminance * std::max(environment.nightSky.moonlightScale, 0.0f);
    auto luminance = [](const glm::vec3& c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; };
    const float sunLuminance = light.enabled ? luminance(light.illuminance) : 0.0f;
    // One shadowed directional light at a time: the brighter of the two (the sun by day, the moon once the sun has
    // set; at the switch both are below 1e-4 of the sky, so it is not visible).
    if (luminance(moonlight) <= sunLuminance || luminance(moonlight) <= 0.0f) return light;
    EffectiveSun out;
    out.enabled = true;
    out.moon = true;
    out.direction = moon.direction;
    out.illuminance = moonlight;
    out.angularRadius = moon.angularRadius;
    return out;
}

ExposurePlan Application::exposurePlan(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    using namespace ux3d::imgui;
    ExposurePlan plan;
    plan.mode = effectiveExposureMode(environment);
    const bool procedural = environment.environmentSource == static_cast<int>(EnvironmentSource::ProceduralSky);
    // The physical modes need calibrated units, i.e. the procedural sky. With the HDR, Auto is the metered auto exposure
    // of before and Curve is Manual (an HDR panorama has no time of day).
    if (!procedural || plan.mode == ExposureMode::Manual) return plan;
    // The time-of-day curve: horizontal illuminance from the sun (direct), the clear sky (A6.2 table), the moonlight
    // and the night sky glow, read by an incident meter.
    const glm::vec3 sunDirection = skySunDirection(environment);
    const float illuminance = outdoorIlluminance(environment);
    const float sky = night::clearSkyIlluminance(glm::degrees(std::asin(std::clamp(sunDirection.y, -1.0f, 1.0f)))) +
                      night::airglowIlluminance(environment.nightSky.airglow);
    plan.horizontalIlluminance = illuminance;
    // The meter also counts the sky in view (NightSky.h kCurveSkyWeight) and the curve's key offset.
    plan.curveEv100 = night::ev100FromIlluminance(illuminance + night::kCurveSkyWeight * sky);
    plan.curveEv = night::gliderEvFromEv100(plan.curveEv100, environment.exposureMinEV100, environment.exposureMaxEV100,
                                            night::kCurveKeyOffsetEv);
    plan.physical = true;
    plan.preExposure = std::exp2(plan.curveEv);
    return plan;
}

float Application::emissiveNightBlend(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    using namespace ux3d::imgui;
    if (environment.timeOfDay == static_cast<int>(DayNightMode::Automatic)) {
        const TimeOfDayEvaluation tod = evaluateTimeOfDay(environment.clock);
        return night::nightBlend(tod.lightElevation, environment.clock.nightThreshold);
    }
    return environment.timeOfDay == static_cast<int>(DayNightMode::Night) ? 1.0f : 0.0f;
}

json Application::nightStateJson(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    using namespace ux3d::imgui;
    auto vec3Json = [](const glm::vec3& v) { return json{agent::floatJson(v.x), agent::floatJson(v.y), agent::floatJson(v.z)}; };
    const bool procedural = environment.environmentSource == static_cast<int>(EnvironmentSource::ProceduralSky);
    const ExposurePlan plan = exposurePlan(environment);
    json exposure = {{"mode", exposureModeName(static_cast<ExposureMode>(std::clamp(environment.exposureMode, -1, 2)))},
                     {"effectiveMode", exposureModeName(plan.mode)},
                     {"physical", plan.physical},
                     {"ev100Offset", agent::floatJson(night::kEv100Offset)},
                     {"minEv100", agent::floatJson(environment.exposureMinEV100)},
                     {"maxEv100", agent::floatJson(environment.exposureMaxEV100)},
                     {"compensation", agent::floatJson(environment.exposureCompensationEV)},
                     {"curveIlluminance", agent::floatJson(plan.horizontalIlluminance)},
                     {"curveIlluminanceLux", agent::floatJson(plan.horizontalIlluminance * static_cast<float>(night::kLuxPerUnit))},
                     {"curveEv100", agent::floatJson(plan.curveEv100)},
                     {"curveEv", agent::floatJson(plan.curveEv)},
                     {"preExposure", agent::floatJson(plan.preExposure)},
                     {"metering", environment.exposureMetering == static_cast<int>(night::ExposureMetering::Average) ? "average"
                                                                                                                       : "highlight"},
                     {"toneMapping", effectiveToneMapping(environment)}};
    json viewportsJson = json::array();
    for (const auto& vp : viewports) {
        if (vp == nullptr) continue;
        json entry = {{"viewport", vp->id},
                      {"preExposure", agent::floatJson(vp->preExposure)},
                      {"exposureEv", agent::floatJson(vp->displayedExposureEv)},
                      {"meteredLuminance", agent::floatJson(vp->antiAliasing.getMeteredLuminance())},
                      {"adapting", vp->antiAliasing.isExposureAdapting()},
                      // TAA history brought to a changed pre-exposure (last pass's factor, and how many passes needed one).
                      {"taaHistoryScale", agent::floatJson(vp->antiAliasing.getLastHistoryScale())},
                      {"taaHistoryRescales", vp->antiAliasing.getHistoryRescales()}};
        // A6.4b: the physically based Auto exposure's two meters (Glider EV, compensation included).
        const auto& meter = vp->antiAliasing.getMeterReport();
        if (plan.physical && plan.mode == ExposureMode::Auto && meter.valid) {
            entry["meter"] = {{"averageEv", agent::floatJson(meter.averageEv)},
                              {"highlightEv", agent::floatJson(meter.highlightEv)},
                              {"highlightLuminance", agent::floatJson(meter.highlightLuminance)},
                              {"targetEv", agent::floatJson(meter.targetEv)},
                              {"surfaceFraction", agent::floatJson(meter.surfaceFraction)},
                              {"limitedBy", meter.highlightLimited ? "highlight" : "average"}};
        }
        viewportsJson.push_back(std::move(entry));
    }
    exposure["viewports"] = viewportsJson;

    const night::MoonEvaluation moon = moonEvaluation(environment);
    const EffectiveSun light = effectiveDirectionalLight(environment);
    float moonAzimuth = 0.0f, moonElevation = 0.0f;
    sunAnglesFromDirection(moon.direction, moonAzimuth, moonElevation);
    const float moonLux = (0.2126f * moon.illuminance.r + 0.7152f * moon.illuminance.g + 0.0722f * moon.illuminance.b) *
                          static_cast<float>(night::kLuxPerUnit);
    json moonJson = {{"enabled", environment.nightSky.moonEnabled},
                     {"available", moon.available},
                     {"aboveHorizon", moon.aboveHorizon},
                     {"direction", vec3Json(moon.direction)},
                     {"azimuth", agent::floatJson(moonAzimuth)},
                     {"elevation", agent::floatJson(moonElevation)},
                     {"sunFromMoon", vec3Json(moon.sunFromMoon)},
                     {"angularRadius", agent::floatJson(glm::degrees(moon.angularRadius))},
                     {"phaseAngle", moon.phaseAngle},
                     {"illuminatedFraction", moon.illuminatedFraction},
                     {"distanceKm", moon.distanceKm},
                     {"transmittance", vec3Json(moon.transmittance)},
                     {"illuminanceTop", vec3Json(moon.illuminanceTop)},
                     {"illuminance", vec3Json(moon.illuminance)},
                     {"illuminanceLux", agent::floatJson(moonLux)},
                     {"diskScale", vec3Json(moon.diskScale)},
                     {"lightScale", agent::floatJson(environment.nightSky.moonlightScale)},
                     {"lighting", light.moon}};
    json starsJson = {{"enabled", environment.nightSky.starsEnabled},
                      {"density", agent::floatJson(environment.nightSky.starDensity)},
                      {"fade", 0.0f}};
    if (procedural && environment.clock.enabled) {
        const TimeOfDayEvaluation tod = evaluateTimeOfDay(environment.clock);
        const glm::mat3 rotation = night::worldToEquatorial(environment.clock, tod);
        // Columns of the transpose: the world directions of the equatorial axes.
        const glm::mat3 toWorld = glm::transpose(rotation);
        starsJson["fade"] = agent::floatJson(night::starFade(tod.lightElevation));
        starsJson["celestialPole"] = vec3Json(toWorld[2]);
        starsJson["vernalEquinox"] = vec3Json(toWorld[0]);
        const double gmst = solar::detail::greenwichSiderealDegrees(tod.julianDayUt);
        starsJson["localSiderealDegrees"] = solar::normalizeDegrees(gmst + environment.clock.longitude);
    }
    json lightJson = {{"source", light.moon ? "moon" : "sun"},
                      {"enabled", light.enabled},
                      {"direction", vec3Json(light.direction)},
                      {"illuminance", vec3Json(light.illuminance)}};
    return {{"exposure", exposure},
            {"moon", moonJson},
            {"stars", starsJson},
            {"airglow", agent::floatJson(environment.nightSky.airglow)},
            {"nightVision", agent::floatJson(environment.nightSky.nightVision)},
            {"airglowZenith", vec3Json(night::airglowRadiance(environment.nightSky.airglow))},
            {"directionalLight", lightJson},
            {"emissiveNightBlend", agent::floatJson(emissiveNightBlend(environment))}};
}

ux3d::imgui::AtmosphereParameters Application::skyAtmosphere(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    ux3d::imgui::AtmosphereParameters p;
    p.mieDensity = std::max(environment.skyMieDensity, 0.0f);
    p.airglow = std::max(environment.nightSky.airglow, 0.0f);
    p.ozone = environment.skyOzone;
    if (environment.skyGroundAlbedo >= 0.0f) {
        p.groundAlbedo = glm::vec3(std::min(environment.skyGroundAlbedo, 1.0f));
    } else if (const ux3d::imgui::ViewportInstance* ground = skyGroundViewport()) {
        // A3.6: the ground viewport's plane albedo (ViewportInstance::groundAlbedo holds the same value after its
        // render; the settings give it before the first render too, so a startup capture needs no second filter).
        p.groundAlbedo = glm::clamp(ground->settings.ground.averageAlbedo(), glm::vec3(0.0f), glm::vec3(1.0f));
    } else {
        p.groundAlbedo = glm::vec3(ux3d::imgui::sky::kNeutralGroundAlbedo);
    }
    return p;
}

const ux3d::imgui::ViewportInstance* Application::skyGroundViewport() const
{
    // The sky's ground albedo is a scene value (one IBL for every viewport), so it must not follow the keyboard focus
    // (a focus click between a plane and a grid viewport re-filtered the whole sky). Rule: the open viewport with the
    // lowest id that draws a ground surface, i.e. the A3.6 plane or the A6.8 ocean in its shaded view (Viewports.cpp
    // groundPlane / oceanSurface: render mode None, which excludes wireframe and the debug views). None: neutral.
    for (const auto& vp : viewports) {
        if (vp == nullptr || !vp->isOpen) continue;
        if ((vp->settings.ground.mode == ux3d::imgui::GroundMode::Plane ||
             vp->settings.ground.mode == ux3d::imgui::GroundMode::Ocean) &&
            vp->settings.renderMode == ux3d::renderer::RenderMode::None) {
            return vp.get();
        }
    }
    return nullptr;
}

bool Application::skyGroundFromPlane() const
{
    return skyGroundViewport() != nullptr;
}

glm::vec3 Application::skySunDirection(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    if (ux3d::imgui::effectiveSunSource(environment) == ux3d::imgui::SunSource::Clock) {
        const ux3d::imgui::TimeOfDayEvaluation tod = ux3d::imgui::evaluateTimeOfDay(environment.clock);
        return ux3d::imgui::sunDirectionFromAngles(tod.gliderAzimuth, tod.gliderElevation);
    }
    return ux3d::imgui::sunDirectionFromAngles(environment.sunAzimuth, environment.sunElevation);
}

void Application::updateSkyEnvironment(const ux3d::imgui::SceneEnvironmentState& environment, FrameContext& frame)
{
    using ux3d::imgui::SkyEnvironment;
    SkyEnvironment::Inputs in;
    in.atmosphere = skyAtmosphere(environment);
    in.sunDirection = skySunDirection(environment);
    // The effective render-quality IBL fields (RenderQuality::applyIblParameters keeps them in the layer).
    in.ibl.cubeSize = gltfLayer->iblParameters.textureSize;
    in.ibl.lambertianSize = gltfLayer->iblParameters.lambertianSize;
    in.ibl.lutSize = gltfLayer->iblParameters.lutResolution;
    in.ibl.halfFloat = gltfLayer->iblParameters.halfFloat;
    in.ibl.ggxSamples = gltfLayer->iblSampling.ggxSampleCount;
    in.ibl.lambertianSamples = gltfLayer->iblSampling.lambertianSampleCount;
    in.ibl.sheenSamples = gltfLayer->iblSampling.sheenSampleCount;
    in.ibl.lodBias = gltfLayer->iblSampling.lodBias;
    in.timeSlicing = s_skyTimeSlicing;
    in.budgetMs = s_skyBudgetMs;
    in.frame = frame.number;
    const auto start = ux3d::imgui::profiling::Clock::now();
    const SkyEnvironment::Result result = skyEnvironment.update(in);
    frame.markStage("sky.update", start);
    if (result == SkyEnvironment::Result::Failed) {
        if (!s_skyFailureLogged) {
            ux3d::core::logError("Procedural sky: the sky or IBL filter programs could not be created");
            s_skyFailureLogged = true;
        }
        return;
    }
    if (result == SkyEnvironment::Result::Reattach || !gltfLayer->isExternalEnvironment()) {
        gltfLayer->setExternalEnvironment(skyEnvironment.frontLambertian(), skyEnvironment.frontGgx(), skyEnvironment.frontSheen(),
                                          skyEnvironment.ggxLut(), skyEnvironment.charlieLut(), skyEnvironment.sheenLut(),
                                          skyEnvironment.frontMipCount());
        scenarioEnvironmentPath = "procedural sky";
        scenarioEnvironmentLoaded = true;
        sceneRevisions.mark(ux3d::imgui::SceneChange::RenderSettings);
    } else if (result == SkyEnvironment::Result::Retarget) {
        gltfLayer->retargetExternalEnvironment(skyEnvironment.frontLambertian(), skyEnvironment.frontGgx(),
                                               skyEnvironment.frontSheen(), skyEnvironment.frontMipCount());
        // The renders read the retargeted handles directly: redraw (render key, TAA) without a model resync.
        sceneRevisions.mark(ux3d::imgui::SceneChange::Environment);
    }
    gltfLayer->getResourceManager().setSkyViewTexture(skyEnvironment.skyViewLut());
    auto* skyMaterial = std::get<1>(gltfLayer->getResourceManager().getCubeMapTuple());
    if (skyMaterial != nullptr) {
        skyMaterial->setUniform("u_SkySunDirection", ux3d::math::vec3(in.sunDirection.x, in.sunDirection.y, in.sunDirection.z));
        skyMaterial->setUniform("u_SkyParams", ux3d::math::vec4(static_cast<float>(ux3d::imgui::sky::kBottomRadiusKm),
                                                                static_cast<float>(ux3d::imgui::sky::kTopRadiusKm),
                                                                static_cast<float>(ux3d::imgui::sky::kBottomRadiusKm +
                                                                                   ux3d::imgui::sky::kObserverAltitudeKm),
                                                                ux3d::imgui::sky::kSolarIlluminance));
    }
}

void Application::releaseSkyEnvironment()
{
    if (gltfLayer != nullptr) {
        if (gltfLayer->isExternalEnvironment()) gltfLayer->retargetExternalEnvironment(0, 0, 0, 1);
        gltfLayer->getResourceManager().setSkyViewTexture(0);
    }
    skyEnvironment.release();
}

ux3d::imgui::SkyStatus Application::skyStatus(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    ux3d::imgui::SkyStatus status;
    const bool procedural = environment.environmentSource == static_cast<int>(ux3d::imgui::EnvironmentSource::ProceduralSky);
    status.active = procedural && skyEnvironment.active();
    status.state = status.active ? skyEnvironment.state() : ux3d::imgui::SkyRefineState::Inactive;
    status.progress = skyEnvironment.progress();
    status.cubeSize = skyEnvironment.frontSize();
    status.targetSize = skyEnvironment.config().cubeSize;
    status.gpuMsLast = skyEnvironment.stats().gpuMsLast;
    status.gpuMsMax = skyEnvironment.stats().gpuMsMax;
    status.budgetMs = s_skyBudgetMs;
    status.groundAlbedo = skyAtmosphere(environment).groundAlbedo;
    status.groundFromPlane = environment.skyGroundAlbedo < 0.0f && skyGroundFromPlane();
    status.skyOcclusionActive = skyOcclusionApplies();
    status.skyOcclusionBaking = skyOcclusionVolume.busy();
    status.skyOcclusionProgress = skyOcclusionVolume.busy() && skyOcclusionVolume.jobDirections() > 0
        ? static_cast<float>(skyOcclusionVolume.jobDirectionsDone()) / static_cast<float>(skyOcclusionVolume.jobDirections())
        : 0.0f;
    if (skyOcclusionVolume.ready()) {
        const auto& bounds = skyOcclusionVolume.frontBounds();
        status.skyOcclusionDims[0] = bounds.dims.x;
        status.skyOcclusionDims[1] = bounds.dims.y;
        status.skyOcclusionDims[2] = bounds.dims.z;
        status.skyOcclusionCell = bounds.cell;
        status.skyOcclusionMemoryMiB = static_cast<double>(skyOcclusionVolume.frontBytes()) / (1024.0 * 1024.0);
        status.skyOcclusionBakeMs = skyOcclusionVolume.stats().lastCpuMs;
    }
    return status;
}

json Application::skyStateJson(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    using namespace ux3d::imgui;
    auto vec3Json = [](const glm::vec3& v) { return json{agent::floatJson(v.x), agent::floatJson(v.y), agent::floatJson(v.z)}; };
    const bool procedural = environment.environmentSource == static_cast<int>(EnvironmentSource::ProceduralSky);
    const AtmosphereParameters atmosphere = skyAtmosphere(environment);
    const glm::vec3 sunDirection = skySunDirection(environment);
    const glm::vec3 transmittance = atmosphereTransmittance(atmosphere, sunDirection);
    const SkyEnvironment::Stats& stats = skyEnvironment.stats();
    const char* albedoSource = environment.skyGroundAlbedo >= 0.0f ? "override"
                               : skyGroundFromPlane() ? "plane" : "neutral";
    float sunAzimuth = 0.0f, sunElevation = 0.0f;
    sunAnglesFromDirection(sunDirection, sunAzimuth, sunElevation);
    const SkyRefineState refineState = procedural && skyEnvironment.active() ? skyEnvironment.state() : SkyRefineState::Inactive;
    json out = {
        {"source", procedural ? "sky" : "hdr"},
        {"active", procedural && skyEnvironment.active()},
        {"model", "Hillaire 2020 (transmittance, multiple-scattering and sky-view LUTs), Bruneton 2017 Earth atmosphere"},
        {"mieDensity", agent::floatJson(atmosphere.mieDensity)},
        {"ozone", atmosphere.ozone},
        {"groundAlbedo", vec3Json(atmosphere.groundAlbedo)},
        {"groundAlbedoSource", albedoSource},
        {"observerAltitudeKm", sky::kObserverAltitudeKm},
        {"units", {{"luxPerUnit", sky::kLuxPerEnvironmentUnit},
                   {"solarIlluminanceLux", sky::kSolarIlluminanceLux},
                   {"solarIlluminance", agent::floatJson(sky::kSolarIlluminance)}}},
        {"sunDirection", vec3Json(sunDirection)},
        {"sunAzimuth", agent::floatJson(sunAzimuth)},
        {"sunElevation", agent::floatJson(sunElevation)},
        {"sunTransmittance", vec3Json(transmittance)},
        {"sunIlluminance", vec3Json(transmittance * sky::kSolarIlluminance)},
        {"timeSlicing", s_skyTimeSlicing},
        {"budgetMs", agent::floatJson(s_skyBudgetMs)},
        {"statedBudgetMs", SkyEnvironment::kStatedBudgetMs},
        {"refine", {{"state", skyRefineStateName(refineState)},
                    {"progress", agent::floatJson(skyEnvironment.progress())},
                    {"frontSize", skyEnvironment.frontSize()},
                    {"frontMipCount", skyEnvironment.frontMipCount()},
                    {"targetSize", skyEnvironment.config().cubeSize},
                    {"skyVersion", skyEnvironment.skyVersion()},
                    {"frontVersion", skyEnvironment.frontVersion()}}},
        {"stats", {{"synchronousBuilds", stats.synchronousBuilds},
                   {"lastSynchronousMs", stats.lastSynchronousMs},
                   {"fastJobs", stats.fastJobs},
                   {"refineJobs", stats.refineJobs},
                   {"abortedRefines", stats.abortedRefines},
                   {"swaps", stats.swaps},
                   {"incompleteSwaps", stats.incompleteSwaps},
                   {"lutAtmosphereBuilds", stats.lutAtmosphereBuilds},
                   {"lutSkyViewBuilds", stats.lutSkyViewBuilds},
                   {"slicedFrames", stats.slicedFrames},
                   {"overBudgetFrames", stats.overBudgetFrames},
                   {"gpuMsLast", stats.gpuMsLast},
                   {"gpuMsMax", stats.gpuMsMax},
                   {"gpuMsMean", stats.slicedFrames > 0 ? stats.gpuMsSum / static_cast<double>(stats.slicedFrames) : 0.0},
                   {"refineFrames", stats.refineFrames},
                   {"refineGpuMsSum", stats.refineGpuMsSum},
                   {"msPerMegaUnit", stats.msPerMegaUnit},
                   {"skyViewLutMs", stats.skyViewLutMs},
                   {"atmosphereLutsMs", stats.atmosphereLutsMs},
                   {"fastSourceMs", stats.fastSourceMs},
                   {"radianceScaleReadbacks", stats.radianceScaleReadbacks},
                   {"radianceScaleMsLast", stats.radianceScaleMsLast},
                   {"radianceScaleMsMax", stats.radianceScaleMsMax},
                   {"radianceScaleMsMean", stats.radianceScaleReadbacks > 0
                                               ? stats.radianceScaleMsSum / static_cast<double>(stats.radianceScaleReadbacks) : 0.0}}},
        {"frontRadianceScale", agent::floatJson(1.0f / skyEnvironment.frontRadianceDecode())},
        {"memoryBytes", skyEnvironment.memoryBytes()}};
    return out;
}

int Application::effectiveDayNight(const ux3d::imgui::SceneEnvironmentState& environment)
{
    if (environment.timeOfDay == static_cast<int>(ux3d::imgui::DayNightMode::Automatic)) {
        return ux3d::imgui::evaluateTimeOfDay(environment.clock).night ? 1 : 0;
    }
    return environment.timeOfDay == static_cast<int>(ux3d::imgui::DayNightMode::Night) ? 1 : 0;
}

void Application::advanceTimeOfDay(float deltaSeconds)
{
    auto& clock = getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).clock;
    if (!clock.playing) return;
    ux3d::imgui::advanceClock(clock, static_cast<double>(deltaSeconds) * static_cast<double>(clock.timeScale));
    // Every render reads the clock (the render key carries the environment): redraw without a model resync.
    sceneRevisions.mark(ux3d::imgui::SceneChange::Environment);
}

json Application::timeOfDayStateJson(const ux3d::imgui::SceneEnvironmentState& environment) const
{
    using namespace ux3d::imgui;
    const TimeOfDayState& clock = environment.clock;
    const TimeOfDayEvaluation tod = evaluateTimeOfDay(clock);
    double windowStart = 0.0;
    TimeOfDayState local = clock;
    local.hours = tod.localHours;
    local.preset = static_cast<int>(TimePreset::Custom);
    local.year = tod.year;
    local.month = tod.month;
    local.day = tod.day;
    const solar::SunDay events = timeOfDaySunDay(local, windowStart);
    auto vec3Json = [](const glm::vec3& v) { return json{agent::floatJson(v.x), agent::floatJson(v.y), agent::floatJson(v.z)}; };
    auto eventJson = [&](bool has, double julian) -> json {
        if (!has) return nullptr;
        const double hours = hoursSince(julian, windowStart);
        return json{{"hours", hours}, {"time", formatTime(hours)}};
    };
    // UTC date and time of the instant.
    const long long utcDays = static_cast<long long>(std::floor(tod.julianDayUt - 2440587.5));
    int utcYear = 0, utcMonth = 0, utcDay = 0;
    solar::civilFromDays(utcDays, utcYear, utcMonth, utcDay);
    const double utcHours = (tod.julianDayUt - 2440587.5 - static_cast<double>(utcDays)) * 24.0;

    const glm::vec3 sunDirection = sunDirectionFromAngles(tod.gliderAzimuth, tod.gliderElevation);
    const glm::vec3 moonDirection =
        sunDirectionFromAngles(tod.moonGliderAzimuth, static_cast<float>(tod.moon.horizontal.apparentElevation));
    json out = {
        {"enabled", clock.enabled},
        {"date", formatDate(tod.year, tod.month, tod.day)},
        {"time", formatTime(tod.localHours)},
        {"hours", tod.localHours},
        {"utcOffset", agent::floatJson(clock.utcOffsetHours)},
        {"latitude", agent::floatJson(clock.latitude)},
        {"longitude", agent::floatJson(clock.longitude)},
        {"heading", agent::floatJson(clock.heading)},
        {"preset", kTimePresetNames[static_cast<size_t>(std::clamp(clock.preset, 0, static_cast<int>(TimePreset::Count) - 1))]},
        {"refraction", clock.refraction},
        {"playing", clock.playing},
        {"timeScale", agent::floatJson(clock.timeScale)},
        {"utc", {{"date", formatDate(utcYear, utcMonth, utcDay)}, {"time", formatTime(utcHours)}}},
        {"julianDay", tod.julianDayUt},
        {"sun", {{"azimuth", tod.sun.azimuth},
                 {"elevation", tod.sun.elevation},
                 {"apparentElevation", tod.sun.apparentElevation},
                 {"lightElevation", tod.lightElevation},
                 {"hourAngle", tod.sun.hourAngle},
                 {"rightAscension", tod.sunCoordinates.rightAscension},
                 {"declination", tod.sunCoordinates.declination},
                 {"eclipticLongitude", tod.sunCoordinates.eclipticLongitude},
                 {"distanceAu", tod.sunCoordinates.distanceAu},
                 {"equationOfTimeMinutes", tod.sunCoordinates.equationOfTimeMinutes},
                 {"fade", agent::floatJson(tod.fade)},
                 {"gliderAzimuth", agent::floatJson(tod.gliderAzimuth)},
                 {"gliderElevation", agent::floatJson(tod.gliderElevation)},
                 {"direction", vec3Json(sunDirection)}}},
        {"events", {{"sunrise", eventJson(events.hasSunrise, events.sunrise)},
                    {"sunset", eventJson(events.hasSunset, events.sunset)},
                    {"transit", eventJson(events.hasTransit, events.transit)},
                    {"solarMidnight", eventJson(events.hasMidnight, events.midnight)},
                    {"polarDay", events.alwaysUp},
                    {"polarNight", events.alwaysDown},
                    {"peakElevation", events.peakElevation},
                    {"lowestElevation", events.lowestElevation}}},
        {"moon", {{"azimuth", tod.moon.horizontal.azimuth},
                  {"elevation", tod.moon.horizontal.elevation},
                  {"apparentElevation", tod.moon.horizontal.apparentElevation},
                  {"rightAscension", tod.moon.rightAscension},
                  {"declination", tod.moon.declination},
                  {"eclipticLongitude", tod.moon.eclipticLongitude},
                  {"eclipticLatitude", tod.moon.eclipticLatitude},
                  {"distanceKm", tod.moon.distanceKm},
                  {"elongation", tod.moon.elongation},
                  {"phaseAngle", tod.moon.phaseAngle},
                  {"illuminatedFraction", tod.moon.illuminatedFraction},
                  {"phase", tod.moon.phase},
                  {"ageDays", tod.moon.ageDays},
                  {"aboveHorizon", tod.moon.horizontal.apparentElevation > -0.833},
                  {"gliderAzimuth", agent::floatJson(tod.moonGliderAzimuth)},
                  {"direction", vec3Json(moonDirection)},
                  {"rendered", environment.environmentSource == 1 && environment.nightSky.moonEnabled}}},
        {"dayNight", {{"mode", environment.timeOfDay == 2 ? "auto" : environment.timeOfDay == 1 ? "night" : "day"},
                      {"effective", effectiveDayNight(environment) == 1 ? "night" : "day"},
                      {"nightBlend", agent::floatJson(emissiveNightBlend(environment))},
                      {"threshold", agent::floatJson(clock.nightThreshold)}}}};
    return out;
}

} // namespace ux3d::imgui::application
