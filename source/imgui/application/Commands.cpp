// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Agent control channel (docs/AGENT_CONTROL_PLAN.md): the command and setting registry shared
// by the CLI, --ui-script / --ui-script-follow and the control pipe, the pipe pump, the readback
// probes and --agent-guide.

#include "Application.h"
#include "imgui/CaptureSettings.h"
#include "imgui/EmbeddedVFS.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/GltfValidator.h"
#include "imgui/GlCaps.h"
#include "imgui/OceanSimulation.h"
#include "imgui/RenderQuality.h"
#include "imgui/SceneHierarchyPanel.h"
#include "gltf/Material.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ux3d::imgui::application {

nlohmann::json asoboValueToJson(const ux3d::gltf::asobo::Value& value); // CommandLine.cpp

namespace {

namespace agent = ux3d::imgui::agent;
using json = nlohmann::json;
using agent::CommandReply;
using agent::SettingScope;
using agent::SettingSpec;
using agent::SettingType;

constexpr int kAgentProtocolVersion = 1;
constexpr int kSettleAfterLoadFrames = 4; // the Phase 0 `open` wait

json prop(const char* type, const char* description)
{
    json p = {{"description", description}};
    if (type != nullptr && *type != '\0') p["type"] = type;
    return p;
}

json withDefault(json p, json value)
{
    p["default"] = std::move(value);
    return p;
}

json objectSchema(std::vector<std::pair<std::string, json>> properties, std::vector<std::string> required = {})
{
    json schema = {{"type", "object"}, {"properties", json::object()}};
    for (auto& [key, value] : properties) schema["properties"][key] = std::move(value);
    if (!required.empty()) schema["required"] = required;
    return schema;
}

json viewProp()
{
    json p = prop("string", "Preset direction: front, back, left, right, top, bottom or iso.");
    p["enum"] = {"front", "back", "left", "right", "top", "bottom", "iso"};
    return p;
}

json viewportProp()
{
    return prop("integer", "Viewport id (default: the active viewport).");
}

std::string nodeName(const ux3d::gltf::Node* node)
{
    if (node == nullptr) return {};
    return node->hasName() ? std::string(node->getName()) : "Node " + std::to_string(node->m_ID);
}

json nodeRef(const ux3d::gltf::Node* node)
{
    if (node == nullptr) return nullptr;
    return {{"index", node->m_ID}, {"name", nodeName(node)}};
}

std::string lowerText(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string hex64(uint64_t value)
{
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(value));
    return buffer;
}

// Parses "x,y,z" or JSON arrays into three floats.
bool vec3Arg(const json& value, glm::vec3& out)
{
    if (!value.is_array() || value.size() != 3) return false;
    for (size_t i = 0; i < 3; ++i) {
        if (!value[i].is_number()) return false;
        out[static_cast<int>(i)] = static_cast<float>(value[i].get<double>());
    }
    return true;
}

template <typename State>
SettingSpec boolSetting(std::string key, SettingScope scope, bool State::*member, std::string help, std::string cli = {})
{
    SettingSpec spec;
    spec.key = std::move(key);
    spec.scope = scope;
    spec.type = SettingType::Bool;
    spec.help = std::move(help);
    spec.cliFlag = std::move(cli);
    spec.get = [member](void* target) { return json(static_cast<State*>(target)->*member); };
    spec.set = [member](void* target, const json& value, std::string&) {
        static_cast<State*>(target)->*member = value.get<bool>();
        return true;
    };
    return spec;
}

template <typename State>
SettingSpec floatSetting(std::string key, SettingScope scope, float State::*member, std::string help,
                         std::string cli = {}, std::optional<std::pair<double, double>> range = std::nullopt)
{
    SettingSpec spec;
    spec.key = std::move(key);
    spec.scope = scope;
    spec.type = SettingType::Float;
    spec.help = std::move(help);
    spec.cliFlag = std::move(cli);
    if (range) {
        spec.hasRange = true;
        spec.minimum = range->first;
        spec.maximum = range->second;
    }
    spec.get = [member](void* target) { return agent::floatJson(static_cast<State*>(target)->*member); };
    spec.set = [member](void* target, const json& value, std::string&) {
        static_cast<State*>(target)->*member = static_cast<float>(value.get<double>());
        return true;
    };
    return spec;
}

// An enum stored as an int index into values.
template <typename State>
SettingSpec indexEnumSetting(std::string key, SettingScope scope, int State::*member, std::vector<std::string> values,
                             std::string help, std::string cli = {})
{
    SettingSpec spec;
    spec.key = std::move(key);
    spec.scope = scope;
    spec.type = SettingType::Enum;
    spec.help = std::move(help);
    spec.cliFlag = std::move(cli);
    spec.enumValues = values;
    spec.get = [member, values](void* target) {
        const int index = static_cast<State*>(target)->*member;
        return index >= 0 && index < static_cast<int>(values.size()) ? json(values[static_cast<size_t>(index)]) : json(index);
    };
    spec.set = [member, values](void* target, const json& value, std::string&) {
        const auto it = std::find(values.begin(), values.end(), value.get<std::string>());
        static_cast<State*>(target)->*member = static_cast<int>(it - values.begin());
        return true;
    };
    return spec;
}

ux3d::gltf::Material* findMaterial(ux3d::gltf::Gltf& gltf, const std::string& spec)
{
    for (auto& material : gltf.getElements<ux3d::gltf::Material>()) {
        if (material.hasName() && std::string(material.getName()) == spec) return &material;
    }
    std::string text = spec;
    if (!text.empty() && text.front() == '#') text.erase(text.begin());
    for (auto& material : gltf.getElements<ux3d::gltf::Material>()) {
        if (std::to_string(material.m_ID) == text) return &material;
    }
    return nullptr;
}

std::string materialName(const ux3d::gltf::Material* material)
{
    return material->hasName() ? std::string(material->getName()) : "Material " + std::to_string(material->m_ID);
}

std::string pathText(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

std::filesystem::path agentTempDirectory()
{
    std::error_code error;
    std::filesystem::path base = std::filesystem::temp_directory_path(error);
    if (error) base = std::filesystem::current_path(error);
#ifdef _WIN32
    const unsigned long pid = GetCurrentProcessId();
#else
    const unsigned long pid = 0;
#endif
    return base / "glider-agent" / std::to_string(pid);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Shared actions (CLI flags and registry commands)
// ---------------------------------------------------------------------------------------------

void Application::selectNodeAction(ux3d::gltf::Node* node)
{
    selection.selectNode(node);
    ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
}

void Application::isolateNodeAction(ux3d::gltf::Node* node, bool withChildren)
{
    gltfLayer->setIsolation(node, withChildren);
    sceneRevisions.mark(ux3d::imgui::SceneChange::Visibility);
}

void Application::frameNodeAction(ux3d::gltf::Node* node, std::optional<ux3d::editor::CameraOrientation> view,
                                  float margin)
{
    if (activeViewport == nullptr) return;
    if (view.has_value()) activeViewport->cameraActor.orientUserCamera(view.value());
    focusCameraOnNode(node, gltfModel, gltfLayer.get(), activeViewport->cameraActor, activeViewport->antiAliasing,
                      sceneRevisions, margin);
}

bool Application::setMaterialParamAction(const std::string& material, const std::string& extension,
                                         const std::string& param, const json& value, std::string& error)
{
    // Text, as --set-asobo-param passes it; JSON numbers keep their exact decimal form.
    std::string raw;
    if (value.is_string()) raw = value.get<std::string>();
    else if (value.is_boolean()) raw = value.get<bool>() ? "true" : "false";
    else raw = value.dump();
    auto& inspector = ux3d::imgui::ExtensionInspector::instance();
    if (ux3d::gltf::asobo::isAsoboExtension(extension)) {
        if (!inspector.setMaterialAsoboParameter(gltfModel, material, extension, param, raw, sceneRevisions)) {
            error = "cannot set " + extension + "." + param + " = " + raw + " on material '" + material +
                    "' (unknown material, extension not on the material, or a value of the wrong type)";
            return false;
        }
        return true;
    }
    ux3d::gltf::Material* target = findMaterial(gltfModel, material);
    if (target == nullptr) {
        error = "material not found: " + material;
        return false;
    }
    bool present = false;
    for (const auto& [name, text] : target->getUnknownExtensions()) {
        present = present || std::string(name.c_str()) == extension;
    }
    if (!present) {
        error = "material '" + material + "' has no " + extension + " (extensions are never added)";
        return false;
    }
    json parsed = value;
    if (value.is_string()) {
        try { parsed = json::parse(value.get<std::string>()); } catch (...) { parsed = value; }
    }
    if (!inspector.setExtensionParameter(target, extension, param, parsed, sceneRevisions)) {
        error = "cannot set " + extension + "." + param;
        return false;
    }
    return true;
}

bool Application::applySettingValue(SettingScope scope, const std::string& key, const json& value,
                                    ux3d::imgui::ViewportSettingsState* viewport,
                                    ux3d::imgui::SceneEnvironmentState* environment, std::string& error)
{
    registerAgentCommands();
    const SettingSpec* spec = agentRegistry.findSetting(scope, key);
    if (spec == nullptr) {
        error = "unknown " + std::string(agent::CommandRegistry::scopeName(scope)) + " setting: " + key;
        return false;
    }
    json coerced;
    if (!agent::CommandRegistry::coerceSetting(*spec, value, coerced, error)) return false;
    void* target = scope == SettingScope::Viewport ? static_cast<void*>(viewport)
                   : scope == SettingScope::Environment ? static_cast<void*>(environment)
                                                        : nullptr;
    if (scope != SettingScope::Renderer && target == nullptr) {
        error = "no target for setting " + key;
        return false;
    }
    return spec->set(target, coerced, error);
}

bool Application::applyCliSetting(SettingScope scope, const std::string& key, const std::string& value, const char* flag)
{
    std::string error;
    auto* environment = scope == SettingScope::Environment ? &getSceneEnvironment(sceneEnvironments, gltfModel, activeScene)
                                                           : nullptr;
    if (!applySettingValue(scope, key, value, activeViewport ? &activeViewport->settings : nullptr, environment, error)) {
        std::fprintf(stderr, "Invalid %s %s: %s\n", flag, value.c_str(), error.c_str());
        return false;
    }
    if (scope == SettingScope::Environment) cliEnvironmentSettings.emplace_back(key, value);
    return true;
}

ux3d::imgui::ViewportInstance* Application::findViewportById(uint32_t id)
{
    for (const auto& vp : viewports) {
        if (vp && vp->id == id) return vp.get();
    }
    return nullptr;
}

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
        list.push_back({{"id", vp->id},
                        {"name", vp->name},
                        {"open", vp->isOpen},
                        {"width", vp->sceneFbo.getWidth()},
                        {"height", vp->sceneFbo.getHeight()},
                        {"renderMode", renderModeName(vp->settings.renderMode)},
                        {"attachedCamera", vp->isAttachedToCamera ? json(vp->attachedCameraNodeName) : json(nullptr)}});
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

// ---------------------------------------------------------------------------------------------
// Readback (probe / probe_region): 1x1 or rectangle reads of existing targets on demand only.
// ---------------------------------------------------------------------------------------------

json Application::probePixel(ux3d::imgui::ViewportInstance* vp, int x, int y, bool withPick)
{
    const int width = vp->sceneFbo.getWidth();
    const int height = vp->sceneFbo.getHeight();
    const int glY = height - 1 - y;
    GLint previousRead = 0, previousDraw = 0, previousReadBuffer = GL_BACK, previousPack = 4;
    GLint previousViewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPack);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    float hdr[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->sceneFbo.getResolveFbo());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(x, glY, 1, 1, GL_RGBA, GL_FLOAT, hdr);
    uint8_t ldr[4] = {0, 0, 0, 0};
    const bool haveLdr = vp->overlayFbo.isValid() && vp->overlayFbo.getWidth() == width &&
                         vp->overlayFbo.getHeight() == height;
    if (haveLdr) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->overlayFbo.getResolveFbo());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(x, glY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, ldr);
    }

    // A6.4: report linear radiance (environment units); the scene target holds it times the viewport's pre-exposure.
    if (vp->preExposure != 1.0f) {
        for (int c = 0; c < 3; ++c) hdr[c] /= vp->preExposure;
    }
    json out;
    out["x"] = x;
    out["y"] = y;
    out["viewport"] = vp->id;
    out["hdr"] = {hdr[0], hdr[1], hdr[2], hdr[3]};
    out["hdrLuminance"] = 0.2126 * hdr[0] + 0.7152 * hdr[1] + 0.0722 * hdr[2];
    out["hdrSource"] = vp->sceneFbo.isLinearFloat() ? "scene target, linear RGBA16F before TAA, bloom and tone mapping"
                                                    : "scene target (8-bit fallback, clamped)";
    out["ldr"] = haveLdr ? json{ldr[0], ldr[1], ldr[2], ldr[3]} : json(nullptr);

    if (withPick) {
        json pick = nullptr;
        float depth = 1.0f;
        auto* camera = gltfLayer ? gltfLayer->getActiveCamera() : nullptr;
        if (camera != nullptr && activeScene != nullptr) {
            pick = performPickAtPixel(x, y, vp);
            // A picking id without a glTF mesh behind it is background too.
            if (pick.is_object() && pick.value("nodeIndex", -1) < 0 && pick.value("mesh", std::string()).empty()) {
                pick = nullptr;
            }
            // The picking pass draws with the scene's own depth (only x/y are rescaled), so
            // its depth is the unjittered scene depth at this pixel.
            glBindFramebuffer(GL_READ_FRAMEBUFFER, pickingTarget.getFbo());
            glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        }
        out["pick"] = pick;
        out["depth"] = depth;
        if (camera != nullptr && !pick.is_null() && depth < 1.0f) {
            const glm::mat4 viewProj = glm::mat4(camera->getProjectionMatrix()) * glm::mat4(camera->getViewMatrix());
            const glm::vec4 ndc((static_cast<float>(x) + 0.5f) / static_cast<float>(width) * 2.0f - 1.0f,
                                (static_cast<float>(glY) + 0.5f) / static_cast<float>(height) * 2.0f - 1.0f,
                                depth * 2.0f - 1.0f, 1.0f);
            glm::vec4 world = glm::inverse(viewProj) * ndc;
            world /= world.w;
            out["world"] = {world.x, world.y, world.z};
        } else {
            out["world"] = nullptr;
        }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousRead));
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDraw));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    glPixelStorei(GL_PACK_ALIGNMENT, previousPack);
    return out;
}

json Application::probeRegion(ux3d::imgui::ViewportInstance* vp, int x0, int y0, int x1, int y1)
{
    const int width = vp->sceneFbo.getWidth();
    const int height = vp->sceneFbo.getHeight();
    const int left = std::max(0, std::min(x0, x1));
    const int right = std::min(width, std::max(x0, x1));
    const int top = std::max(0, std::min(y0, y1));
    const int bottom = std::min(height, std::max(y0, y1));
    const int w = right - left;
    const int h = bottom - top;
    if (w <= 0 || h <= 0) return nullptr;

    GLint previousRead = 0, previousReadBuffer = GL_BACK, previousPack = 4;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPack);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    std::vector<float> pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->sceneFbo.getResolveFbo());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(left, height - bottom, w, h, GL_RGBA, GL_FLOAT, pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousRead));
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));
    glPixelStorei(GL_PACK_ALIGNMENT, previousPack);
    if (vp->preExposure != 1.0f) {
        // A6.4: linear radiance without the viewport's pre-exposure.
        for (size_t i = 0; i < pixels.size(); ++i) {
            if (i % 4u != 3u) pixels[i] /= vp->preExposure;
        }
    }

    double minRgb[3] = {1e30, 1e30, 1e30}, maxRgb[3] = {-1e30, -1e30, -1e30}, sumRgb[3] = {0, 0, 0};
    double minLum = 1e30, maxLum = -1e30, sumLum = 0.0;
    const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h);
    for (size_t i = 0; i < count; ++i) {
        const float* p = &pixels[i * 4u];
        for (int c = 0; c < 3; ++c) {
            minRgb[c] = std::min(minRgb[c], static_cast<double>(p[c]));
            maxRgb[c] = std::max(maxRgb[c], static_cast<double>(p[c]));
            sumRgb[c] += p[c];
        }
        const double lum = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
        minLum = std::min(minLum, lum);
        maxLum = std::max(maxLum, lum);
        sumLum += lum;
    }
    const double n = static_cast<double>(count);
    json out;
    out["rect"] = {left, top, right, bottom};
    out["pixels"] = count;
    out["viewport"] = vp->id;
    out["luminance"] = {{"min", minLum}, {"max", maxLum}, {"mean", sumLum / n}};
    out["rgb"] = {{"min", {minRgb[0], minRgb[1], minRgb[2]}},
                  {"max", {maxRgb[0], maxRgb[1], maxRgb[2]}},
                  {"mean", {sumRgb[0] / n, sumRgb[1] / n, sumRgb[2] / n}}};
    out["hdrSource"] = "scene target, linear, before TAA, bloom and tone mapping";
    return out;
}

// ---------------------------------------------------------------------------------------------
// Settings registry
// ---------------------------------------------------------------------------------------------

void Application::registerAgentSettings()
{
    using VS = ux3d::imgui::ViewportSettingsState;
    using ES = ux3d::imgui::SceneEnvironmentState;
    constexpr SettingScope V = SettingScope::Viewport;
    constexpr SettingScope E = SettingScope::Environment;
    constexpr SettingScope R = SettingScope::Renderer;
    // Defaults come from default-constructed state, so they never drift from the structs.
    auto addSetting = [this](SettingSpec spec) {
        if (spec.scope == SettingScope::Viewport) {
            VS defaults;
            spec.defaultValue = spec.get(&defaults);
        } else if (spec.scope == SettingScope::Environment) {
            ES defaults;
            spec.defaultValue = spec.get(&defaults);
        }
        agentRegistry.addSetting(std::move(spec));
    };

    // --- Viewport: anti-aliasing ---
    {
        SettingSpec spec;
        spec.key = "aa";
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Legacy AA preset: sets MSAA and post AA together (msaa4 = 4x + none, taa = off + TAA).";
        spec.cliFlag = "--aa <none|msaa2|msaa4|msaa8|fxaa|taa>";
        spec.enumValues = {"none", "msaa2", "msaa4", "msaa8", "fxaa", "taa"};
        spec.get = [values = spec.enumValues](void* t) {
            const auto* s = static_cast<VS*>(t);
            if (s->msaaIdx >= 0 || s->postAAIdx >= 0) return json("custom");
            return json(s->antiAliasingIdx >= 0 && s->antiAliasingIdx < 6 ? values[static_cast<size_t>(s->antiAliasingIdx)] : "custom");
        };
        spec.set = [values = spec.enumValues](void* t, const json& v, std::string&) {
            const auto it = std::find(values.begin(), values.end(), v.get<std::string>());
            static_cast<VS*>(t)->setLegacyAA(static_cast<int>(it - values.begin()));
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "msaa";
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Geometry multisampling (0 = off).";
        spec.cliFlag = "--msaa <0|2|4|8>";
        spec.enumValues = {"0", "2", "4", "8"};
        spec.enumAliases = {{"off", "0"}, {"2x", "2"}, {"4x", "4"}, {"8x", "8"}};
        spec.get = [values = spec.enumValues](void* t) {
            return json(values[static_cast<size_t>(std::clamp(static_cast<VS*>(t)->effectiveMsaaIdx(), 0, 3))]);
        };
        spec.set = [values = spec.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->msaaIdx = static_cast<int>(std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "post_aa";
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Post-process anti-aliasing.";
        spec.cliFlag = "--post-aa <none|fxaa|taa>";
        spec.enumValues = {"none", "fxaa", "taa"};
        spec.get = [values = spec.enumValues](void* t) {
            return json(values[static_cast<size_t>(std::clamp(static_cast<VS*>(t)->effectivePostAAIdx(), 0, 2))]);
        };
        spec.set = [values = spec.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->postAAIdx = static_cast<int>(std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(spec));
    }

    // --- Viewport: shading and overlays ---
    {
        SettingSpec spec;
        spec.key = "render_mode";
        spec.aliases = {"debug_view"};
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Shaded, wireframe or a debug render mode (View menu names; spaces, _ and - are ignored).";
        spec.cliFlag = "--debug-view <name> / --render-mode <shaded|wireframe>";
        spec.enumValues = renderModeNames();
        spec.enumAliases = {{"default", "shaded"}, {"none", "shaded"}};
        spec.get = [this](void* t) { return json(renderModeName(static_cast<VS*>(t)->renderMode)); };
        spec.set = [this](void* t, const json& v, std::string& error) {
            const auto mode = getRenderModeFromName(v.get<std::string>());
            if (!mode.has_value()) {
                error = "unknown render mode " + v.get<std::string>();
                return false;
            }
            static_cast<VS*>(t)->renderMode = mode.value();
            static_cast<VS*>(t)->renderModeIdx = getIndexFromRenderMode(mode.value());
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec = boolSetting<VS>("grid", V, &VS::showGrid, "Ground grid.", "--hide-grid");
        spec.aliases = {"show_grid"};
        addSetting(std::move(spec));
    }
    // --- Viewport: ground (A3.6: grid or a ground plane; docs/ASOBO_EXTENSIONS_PHASE_3.md) ---
    {
        using GS = ux3d::imgui::GroundSettings;
        SettingSpec spec;
        spec.key = "ground";
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "What the viewport draws on the ground: the editor grid (default), an infinite ground plane "
                    "that receives IBL and the sun's shadow, or an animated FFT ocean (A6.8; needs OpenGL 4.3, "
                    "otherwise refused with a warning and the ground is kept).";
        spec.cliFlag = "--ground <grid|plane|ocean>";
        spec.enumValues = {"grid", "plane", "ocean"};
        spec.get = [values = spec.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<VS*>(t)->ground.mode)]);
        };
        spec.set = [this, values = spec.enumValues](void* t, const json& v, std::string&) {
            const auto mode = static_cast<ux3d::imgui::GroundMode>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            // A6.8: with a known context below 4.3 the ocean is refused here (before the context exists, the
            // viewport refuses it at its first render, Viewports.cpp).
            if (mode == ux3d::imgui::GroundMode::Ocean && ux3d::imgui::glCaps().major > 0 && !ux3d::imgui::oceanAvailable()) {
                if (!oceanUnavailableWarned) {
                    ux3d::core::logWarning("Ocean ground needs an OpenGL 4.3 context (compute shaders); keeping the current ground");
                    oceanUnavailableWarned = true;
                }
                return true;
            }
            static_cast<VS*>(t)->ground.mode = mode;
            return true;
        };
        addSetting(std::move(spec));

        SettingSpec heightMode;
        heightMode.key = "ground_height_mode";
        heightMode.scope = V;
        heightMode.type = SettingType::Enum;
        heightMode.help = "Plane height: the scene's lowest point (model_base; static pose, animation does not move it), "
                          "Y = 0 (zero) or ground_height (custom).";
        heightMode.cliFlag = "--ground-height <model-base|zero|number>";
        heightMode.enumValues = {"model_base", "zero", "custom"};
        heightMode.enumAliases = {{"model-base", "model_base"}, {"modelbase", "model_base"}, {"base", "model_base"}};
        heightMode.get = [values = heightMode.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<VS*>(t)->ground.heightMode)]);
        };
        heightMode.set = [values = heightMode.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->ground.heightMode = static_cast<ux3d::imgui::GroundHeightMode>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(heightMode));

        const auto groundFloat = [&](const char* key, float GS::*member, const char* help, const char* cli,
                                     std::optional<std::pair<double, double>> range, bool editsSurface) {
            SettingSpec f;
            f.key = key;
            f.scope = V;
            f.type = SettingType::Float;
            f.help = help;
            f.cliFlag = cli;
            if (range) {
                f.hasRange = true;
                f.minimum = range->first;
                f.maximum = range->second;
            }
            f.get = [member](void* t) { return agent::floatJson(static_cast<VS*>(t)->ground.*member); };
            f.set = [member, editsSurface](void* t, const json& v, std::string&) {
                auto& ground = static_cast<VS*>(t)->ground;
                ground.*member = static_cast<float>(v.get<double>());
                if (editsSurface) ground.preset = ux3d::imgui::GroundPreset::Custom;
                return true;
            };
            addSetting(std::move(f));
        };
        groundFloat("ground_height", &GS::customHeight, "Plane height in scene units for ground_height_mode custom.",
                    "--ground-height <number>", std::nullopt, false);
        groundFloat("ground_roughness", &GS::roughness, "Plane perceptual roughness 0..1 (marks the preset custom).",
                    "--ground-roughness <0..1>", std::pair{0.0, 1.0}, true);
        groundFloat("ground_detail", &GS::detail, "Amount of procedural albedo variation 0..1 (marks the preset custom).",
                    "--ground-detail <0..1>", std::pair{0.0, 1.0}, true);
        groundFloat("ground_detail_scale", &GS::detailScale,
                    "Size of the largest procedural feature in scene units (marks the preset custom).",
                    "--ground-detail-scale <units>", std::pair{0.0001, 1000000.0}, true);
        groundFloat("ground_joint_spacing", &GS::jointSpacing,
                    "Slab joint spacing in scene units, 0 = none (marks the preset custom).",
                    "--ground-joints <units>", std::pair{0.0, 1000000.0}, true);
        groundFloat("ground_haze", &GS::haze,
                    "Horizon fog visibility distance in scene units (distance of 2% contrast); 0 = automatic (0.9 x far plane).",
                    "--ground-haze <units>", std::pair{0.0, 1.0e9}, false);

        SettingSpec preset;
        preset.key = "ground_preset";
        preset.scope = V;
        preset.type = SettingType::Enum;
        preset.help = "Plane surface preset: sets colour, roughness and detail (concrete, grass, sand, grey); "
                      "custom keeps the current values.";
        preset.cliFlag = "--ground-preset <concrete|grass|sand|grey>";
        preset.enumValues = {"concrete", "grass", "sand", "grey", "custom"};
        preset.enumAliases = {{"tarmac", "concrete"}, {"gray", "grey"}};
        preset.get = [values = preset.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<VS*>(t)->ground.preset)]);
        };
        preset.set = [values = preset.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->ground.applyPreset(static_cast<ux3d::imgui::GroundPreset>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin()));
            return true;
        };
        addSetting(std::move(preset));

        SettingSpec color;
        color.key = "ground_color";
        color.scope = V;
        color.type = SettingType::Color3;
        color.help = "Plane albedo (linear base colour, also the average albedo A6.2 reads); marks the preset custom.";
        color.cliFlag = "--ground-color <r,g,b>";
        color.get = [](void* t) {
            const glm::vec3& c = static_cast<VS*>(t)->ground.albedo;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b)};
        };
        color.set = [](void* t, const json& v, std::string&) {
            auto& ground = static_cast<VS*>(t)->ground;
            ground.albedo = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
            ground.preset = ux3d::imgui::GroundPreset::Custom;
            return true;
        };
        addSetting(std::move(color));

        SettingSpec onGround = boolSetting<VS>("ground_grid", V, &VS::showGrid, "", "--ground-grid <on|off>");
        onGround.help = "Draw the grid on top of the ground plane (needs the viewport grid switch on).";
        onGround.get = [](void* t) { return json(static_cast<VS*>(t)->ground.showGrid); };
        onGround.set = [](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->ground.showGrid = v.get<bool>();
            return true;
        };
        addSetting(std::move(onGround));

        // A6.8: the ocean (all its settings are in OceanSettings, per viewport, not persisted).
        using OS = ux3d::imgui::OceanSettings;
        const auto oceanFloat = [&](const char* key, float OS::*member, const char* help, const char* cli,
                                    std::pair<double, double> range) {
            SettingSpec f;
            f.key = key;
            f.scope = V;
            f.type = SettingType::Float;
            f.help = help;
            f.cliFlag = cli;
            f.hasRange = true;
            f.minimum = range.first;
            f.maximum = range.second;
            f.get = [member](void* t) { return agent::floatJson(static_cast<VS*>(t)->ground.ocean.*member); };
            f.set = [member](void* t, const json& v, std::string&) {
                static_cast<VS*>(t)->ground.ocean.*member = static_cast<float>(v.get<double>());
                return true;
            };
            addSetting(std::move(f));
        };
        oceanFloat("ocean_wind_speed", &OS::windSpeed, "Ocean wind speed 10 m above the sea, m/s (recomputes the loop).",
                   "--ocean-wind-speed <m/s>", {0.5, 40.0});
        oceanFloat("ocean_wind_dir", &OS::windDirection,
                   "Direction the wind comes from, degrees (the sun's azimuth convention: around +Y from +Z towards +X); "
                   "the waves travel the opposite way (recomputes the loop).",
                   "--ocean-wind-dir <deg>", {0.0, 360.0});
        oceanFloat("ocean_time", &OS::time,
                   "Pinned ocean time in seconds for captures; -1 follows the application clock.",
                   "--ocean-time <s>", {-1.0, 1.0e6});
        oceanFloat("ocean_fetch", &OS::fetch,
                   "Distance in metres over which the wind has blown (JONSWAP); beyond full development (about 22000 "
                   "x U^2 / g) it changes nothing (recomputes the loop).",
                   "--ocean-fetch <m>", {1000.0, 1.0e7});
        oceanFloat("ocean_choppiness", &OS::choppiness,
                   "Horizontal displacement factor (Tessendorf's lambda) 0..2: sharper crests, more foam (recomputes the loop).",
                   "--ocean-choppiness <0..2>", {0.0, 2.0});
        oceanFloat("ocean_swell", &OS::swell,
                   "Swell 0..1: narrows the directional spread of the waves (Horvath 2015; recomputes the loop).",
                   "--ocean-swell <0..1>", {0.0, 1.0});
        oceanFloat("ocean_foam", &OS::foam,
                   "Foam amount 0..1: 0 none, 0.5 the whitecap coverage of the wind speed (Monahan 1980: 0.02% at 3 m/s, "
                   "1% at 10, 10% at 20), 1 four times that.",
                   "--ocean-foam <0..1>", {0.0, 1.0});
        oceanFloat("ocean_period", &OS::period,
                   "Loop period in seconds of the precomputed (looping) mode: the surface repeats exactly after it "
                   "(recomputes the loop; ignored in live mode).",
                   "--ocean-period <s>", {5.0, 120.0});
        {
            SettingSpec c;
            c.key = "ocean_color";
            c.scope = V;
            c.type = SettingType::Color3;
            c.help = "Water body colour, linear RGB 0..1: the light scattered back out of the water (times the sky's "
                     "diffuse light and the sun). The ocean's average albedo (0.066 + colour) feeds A6.2.";
            c.cliFlag = "--ocean-color <r,g,b>";
            c.hasRange = true;
            c.minimum = 0.0;
            c.maximum = 1.0;
            c.get = [](void* t) {
                const glm::vec3& v = static_cast<VS*>(t)->ground.ocean.color;
                return json{agent::floatJson(v.r), agent::floatJson(v.g), agent::floatJson(v.b)};
            };
            c.set = [](void* t, const json& v, std::string&) {
                static_cast<VS*>(t)->ground.ocean.color = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
                return true;
            };
            addSetting(std::move(c));
        }
        {
            SettingSpec mode;
            mode.key = "ocean_mode";
            mode.scope = V;
            mode.type = SettingType::Enum;
            mode.help = "Ocean mode: loop (default; a precomputed set that repeats every ocean_period, per-frame cost is "
                        "texture sampling) or live (the FFT runs every frame; 512^2 at high quality). A mode change builds "
                        "the new set at once.";
            mode.cliFlag = "--ocean-mode <loop|live>";
            mode.enumValues = {"loop", "live"};
            mode.enumAliases = {{"looping", "loop"}, {"precomputed", "loop"}};
            mode.get = [](void* t) { return json(static_cast<VS*>(t)->ground.ocean.live ? "live" : "loop"); };
            mode.set = [](void* t, const json& v, std::string&) {
                static_cast<VS*>(t)->ground.ocean.live = v.get<std::string>() == "live";
                return true;
            };
            addSetting(std::move(mode));
        }
        {
            SettingSpec play = boolSetting<VS>("ocean_play", V, &VS::showGrid, "", "--ocean-play <on|off>");
            play.help = "Whether the ocean animates (on, default) or holds its current time (off; a pinned ocean_time "
                        "holds regardless).";
            play.get = [](void* t) { return json(static_cast<VS*>(t)->ground.ocean.play); };
            play.set = [](void* t, const json& v, std::string&) {
                static_cast<VS*>(t)->ground.ocean.play = v.get<bool>();
                return true;
            };
            addSetting(std::move(play));
        }

        SettingSpec quality;
        quality.key = "ocean_quality";
        quality.scope = V;
        quality.type = SettingType::Enum;
        quality.help = "Ocean simulation quality: low (2 cascades of 128^2, 32 frames, 12 MiB), medium (3 x 256^2, 48 frames, "
                       "101 MiB), high (4 x 256^2, 64 frames, 176 MiB); live mode: 2 x 128^2, 3 x 256^2, 4 x 512^2; "
                       "recomputes the loop.";
        quality.cliFlag = "--ocean-quality <low|medium|high>";
        quality.enumValues = {"low", "medium", "high"};
        quality.enumAliases = {{"med", "medium"}, {"mid", "medium"}};
        quality.get = [values = quality.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<VS*>(t)->ground.ocean.quality)]);
        };
        quality.set = [values = quality.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->ground.ocean.quality = static_cast<ux3d::imgui::OceanQuality>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(quality));
    }
    addSetting(boolSetting<VS>("scene_bounding_box", V, &VS::showSceneBoundingBox, "Yellow AABB around the scene."));
    addSetting(boolSetting<VS>("selection_box", V, &VS::showSelectedBoundingBox, "Orange OBB around the selected node."));
    addSetting(boolSetting<VS>("node_gizmo", V, &VS::showNodeCoordinateGizmo, "Axis gizmo at the selected node (UI overlay, not in captures)."));
    addSetting(boolSetting<VS>("force_double_sided", V, &VS::forceDoubleSided, "Disable back-face culling.", "--force-double-sided"));
    addSetting(boolSetting<VS>("invert_winding", V, &VS::invertWinding, "Flip front/back face winding.", "--invert-winding"));
    addSetting(boolSetting<VS>("wireframe_overlay", V, &VS::wireframeOverlay, "Mesh edges over the shaded image.", "--wireframe-overlay"));
    {
        SettingSpec spec;
        spec.key = "wireframe_color";
        spec.scope = V;
        spec.type = SettingType::Color4;
        spec.help = "Wireframe line colour and opacity (0..1).";
        spec.cliFlag = "--wireframe-color <r,g,b[,a]>";
        spec.get = [](void* t) {
            const glm::vec4& c = static_cast<VS*>(t)->wireframeColor;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b), agent::floatJson(c.a)};
        };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->wireframeColor = glm::vec4(v[0].get<float>(), v[1].get<float>(), v[2].get<float>(), v[3].get<float>());
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "wireframe_blend";
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Wireframe line blend mode.";
        spec.cliFlag = "--wireframe-blend <normal|additive|invert|xor>";
        spec.enumValues = {"normal", "additive", "invert", "xor"};
        spec.get = [values = spec.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<VS*>(t)->wireframeBlend)]);
        };
        spec.set = [values = spec.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->wireframeBlend = static_cast<ux3d::imgui::WireframeBlendMode>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(spec));
    }

    // --- Viewport: MSFS extensions ---
    addSetting(boolSetting<VS>("ssr", V, &VS::screenSpaceReflections, "Windshield screen-space reflections.", "--ssr <on|off>"));
    addSetting(indexEnumSetting<VS>("ssr_quality", V, &VS::ssrQuality, {"low", "medium", "high"},
                                                  "SSR sample budget 256/512/1024.", "--ssr-quality <low|medium|high>"));
    addSetting(floatSetting<VS>("interior_sky_reflection", V, &VS::interiorSkyReflection,
                                              "Windshield IBL fallback inside environment occluders.",
                                              "--interior-sky-reflection <0..1>", std::pair{0.0, 1.0}));
    addSetting(floatSetting<VS>("windshield_icing", V, &VS::windshieldIcing,
                                              "Windshield icing and frosted decal coverage.", "--windshield-icing <0..1>",
                                              std::pair{0.0, 1.0}));
    // A3.7: ASOBO_sky_portal (its on/off switch is the generic ASOBO_sky_portal key below).
    addSetting(floatSetting<VS>("sky_portal_strength", V, &VS::skyPortalStrength,
                                              "Scale of the sky-portal window light (added only where the IBL is occluded).",
                                              "--sky-portal-strength <0..4>", std::pair{0.0, 4.0}));
    addSetting(boolSetting<VS>("show_sky_portals", V, &VS::showSkyPortals,
                               "Draw every sky portal's gizmo (UI overlay, not in captures); a selected portal always shows.",
                               "--show-sky-portals"));
    addSetting(floatSetting<VS>("decal_tolerance_mm", V, &VS::decalDepthToleranceMm,
                                              "Decal depth tolerance along the receiver normal (mm).",
                                              "--decal-tolerance <mm>", std::pair{0.0, 10.0}));
    // --- Viewport: lens effect (A6.7; docs/ASOBO_EXTENSIONS_PHASE_6.md) ---
    {
        SettingSpec spec;
        spec.key = "lens_flare";
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Lens effect from the bloom mips: ghosts + halo, an anamorphic streak, or both (on = both). Off by "
                    "default; part of the bloom chain, so bloom off removes it.";
        spec.cliFlag = "--lens-flare <off|on|ghosts|streak|both>";
        spec.enumValues = {"off", "ghosts", "streak", "both"};
        spec.enumAliases = {{"on", "both"}, {"none", "off"}, {"starburst", "ghosts"}, {"anamorphic", "streak"}};
        spec.get = [values = spec.enumValues](void* t) {
            return json(values[static_cast<size_t>(std::clamp(static_cast<VS*>(t)->lensFlare, 0, 3))]);
        };
        spec.set = [values = spec.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->lensFlare =
                static_cast<int>(std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<VS>("lens_flare_intensity", V, &VS::lensFlareIntensity,
                                "Strength of the lens effect (1 = default).", "--lens-flare-intensity <x>",
                                std::pair{0.0, 10.0}));
    // --- Viewport: ambient occlusion (A3.5; docs/ASOBO_EXTENSIONS_PHASE_3.md) ---
    {
        SettingSpec enabled;
        enabled.key = "ssao";
        enabled.scope = V;
        enabled.type = SettingType::Bool;
        enabled.help = "Screen-space ambient occlusion (GTAO) of the sky light: IBL diffuse/specular/sheen/clearcoat "
                       "only, min() with the authored occlusion. Off by default; Auto quality never changes it.";
        enabled.cliFlag = "--ssao <on|off>";
        enabled.get = [](void* t) { return json(static_cast<VS*>(t)->ssao.enabled); };
        enabled.set = [](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->ssao.enabled = v.get<bool>();
            return true;
        };
        addSetting(std::move(enabled));

        const auto ssaoFloat = [&](const char* key, float ux3d::imgui::SsaoSettings::*member, const char* help,
                                   const char* cli, double minimum, double maximum) {
            SettingSpec f;
            f.key = key;
            f.scope = V;
            f.type = SettingType::Float;
            f.help = help;
            f.cliFlag = cli;
            f.hasRange = true;
            f.minimum = minimum;
            f.maximum = maximum;
            f.get = [member](void* t) { return agent::floatJson(static_cast<VS*>(t)->ssao.*member); };
            f.set = [member](void* t, const json& v, std::string&) {
                static_cast<VS*>(t)->ssao.*member = static_cast<float>(v.get<double>());
                return true;
            };
            addSetting(std::move(f));
        };
        ssaoFloat("ssao_radius", &ux3d::imgui::SsaoSettings::radius,
                  "SSAO radius in world units (metres in MSFS assets); occluders farther away do not count.",
                  "--ssao-radius <x>", 0.01, 100.0);
        ssaoFloat("ssao_intensity", &ux3d::imgui::SsaoSettings::intensity,
                  "Exponent on the SSAO visibility (1 = the GTAO value, 0 = no occlusion).", "--ssao-intensity <x>",
                  0.0, 4.0);

        SettingSpec quality;
        quality.key = "ssao_quality";
        quality.scope = V;
        quality.type = SettingType::Enum;
        quality.help = "SSAO samples per pixel: low 2 slices x 4 steps, medium 3 x 6, high 4 x 8 (per side).";
        quality.cliFlag = "--ssao-quality <low|medium|high>";
        quality.enumValues = {"low", "medium", "high"};
        quality.get = [values = quality.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<VS*>(t)->ssao.quality)]);
        };
        quality.set = [values = quality.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->ssao.quality = static_cast<ux3d::imgui::SsaoQuality>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(quality));

        SettingSpec resolution;
        resolution.key = "ssao_resolution";
        resolution.scope = V;
        resolution.type = SettingType::Enum;
        resolution.help = "SSAO resolution: full, or half with a depth-aware upsample.";
        resolution.cliFlag = "--ssao-resolution <full|half>";
        resolution.enumValues = {"full", "half"};
        resolution.get = [](void* t) { return json(static_cast<VS*>(t)->ssao.halfResolution ? "half" : "full"); };
        resolution.set = [](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->ssao.halfResolution = v.get<std::string>() == "half";
            return true;
        };
        addSetting(std::move(resolution));
    }
    {
        SettingSpec spec;
        spec.key = "asobo";
        spec.scope = V;
        spec.type = SettingType::Bool;
        spec.help = "Master switch for MSFS (ASOBO) extensions in this viewport.";
        spec.cliFlag = "--asobo <on|off>";
        spec.get = [](void* t) { return json(static_cast<VS*>(t)->asobo.enabled); };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->asobo.enabled = v.get<bool>();
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "show_invisible_materials";
        spec.scope = V;
        spec.type = SettingType::Bool;
        spec.help = "Also draw ASOBO_material_invisible and environment-occluder materials.";
        spec.cliFlag = "--show-invisible-materials";
        spec.get = [](void* t) {
            const auto& asobo = static_cast<VS*>(t)->asobo;
            return json(!asobo.applies("ASOBO_material_invisible") &&
                        !asobo.applies("ASOBO_material_environment_occluder") && asobo.enabled);
        };
        spec.set = [](void* t, const json& v, std::string&) {
            auto& asobo = static_cast<VS*>(t)->asobo;
            asobo.setApplied("ASOBO_material_invisible", !v.get<bool>());
            asobo.setApplied("ASOBO_material_environment_occluder", !v.get<bool>());
            return true;
        };
        addSetting(std::move(spec));
    }
    // One switch per ASOBO extension with a viewport switch (the same rows as the Viewport
    // Settings panel), keyed by the extension name.
    for (const ux3d::gltf::asobo::SupportInfo& info : ux3d::gltf::asobo::supportTable()) {
        if (!ux3d::gltf::asobo::isAsoboExtension(info.name) || !info.viewportSwitch) continue;
        if (info.status != ux3d::gltf::asobo::Support::Rendered && info.status != ux3d::gltf::asobo::Support::Partial) continue;
        const std::string extension(info.name);
        SettingSpec spec;
        spec.key = extension;
        spec.scope = V;
        spec.type = SettingType::Bool;
        spec.help = "Apply " + extension + " in this viewport.";
        spec.cliFlag = "--asobo-disable " + extension;
        spec.get = [extension](void* t) {
            const auto& disabled = static_cast<VS*>(t)->asobo.disabled;
            return json(std::find(disabled.begin(), disabled.end(), extension) == disabled.end());
        };
        spec.set = [extension](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->asobo.setApplied(extension, v.get<bool>());
            return true;
        };
        addSetting(std::move(spec));
    }

    // --- Environment (one per glTF scene, shared by every viewport) ---
    addSetting(floatSetting<ES>("exposure", E, &ES::exposureEV, "Manual exposure in EV (2^EV).", "--exposure <ev>"));
    {
        // A6.4: "default" (index -1) = ACES Hill with the HDR source (as before), PBR Neutral with the procedural sky.
        SettingSpec spec;
        spec.key = "tone_mapping";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.help = "Tone mapping operator (Khronos PBR Neutral, ACES Hill, ACES Narkowicz); default = ACES Hill with the "
                    "HDR source, PBR Neutral with the procedural sky.";
        spec.cliFlag = "--tone-mapping <default|neutral|hill|narkowicz>";
        static const std::vector<std::string> names = {"default", "neutral", "hill", "narkowicz"};
        spec.enumValues = names;
        spec.enumAliases = {{"pbr_neutral", "neutral"}, {"aces_hill", "hill"}, {"aces_narkowicz", "narkowicz"}};
        spec.get = [](void* t) {
            const int index = std::clamp(static_cast<ES*>(t)->toneMappingIdx, -1, 2);
            return json(names[static_cast<size_t>(index + 1)]);
        };
        spec.set = [](void* t, const json& v, std::string&) {
            const auto it = std::find(names.begin(), names.end(), v.get<std::string>());
            static_cast<ES*>(t)->toneMappingIdx = static_cast<int>(it - names.begin()) - 1;
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        // A6.4 exposure mode (NightSky.h). auto_exposure below is the same state seen as a switch.
        SettingSpec spec;
        spec.key = "exposure_mode";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.help = "Exposure: manual (the exposure EV), auto (eye adaptation: metered; with the procedural sky in EV100 "
                    "with the key curve and exposure_min/max_ev100), curve (procedural sky: from the sun/moon/sky "
                    "illuminance of the time of day; Manual with the HDR) or default (Manual with the HDR source, auto "
                    "with the procedural sky).";
        spec.cliFlag = "--exposure-mode <default|manual|auto|curve>";
        static const std::vector<std::string> modes = {"default", "manual", "auto", "curve"};
        spec.enumValues = modes;
        spec.enumAliases = {{"eye_adaptation", "auto"}, {"time_of_day", "curve"}};
        spec.get = [](void* t) {
            const int index = std::clamp(static_cast<ES*>(t)->exposureMode, -1, 2);
            return json(modes[static_cast<size_t>(index + 1)]);
        };
        spec.set = [](void* t, const json& v, std::string&) {
            const auto it = std::find(modes.begin(), modes.end(), v.get<std::string>());
            static_cast<ES*>(t)->exposureMode = static_cast<int>(it - modes.begin()) - 1;
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("exposure_min_ev100", E, &ES::exposureMinEV100,
                                "Procedural sky, Auto/Curve exposure: darkest scene exposed for, in EV100 (default -12, a "
                                "starlit night; Glider EV = 13.38 - EV100 + key).",
                                "--exposure-min-ev100 <ev>", std::pair{-20.0, 25.0}));
    addSetting(floatSetting<ES>("exposure_max_ev100", E, &ES::exposureMaxEV100,
                                "Procedural sky, Auto/Curve exposure: brightest scene exposed for, in EV100 (default 18).",
                                "--exposure-max-ev100 <ev>", std::pair{-20.0, 25.0}));
    addSetting(indexEnumSetting<ES>("exposure_metering", E, &ES::exposureMetering, {"average", "highlight"},
                                    "Procedural sky, Auto exposure: average (trimmed centre-weighted log average with the "
                                    "key curve) or highlight (default: the darker of the average and an exposure that keeps "
                                    "the brightest lit surfaces (0.3% of the image) below the tone mapper's shoulder; lit "
                                    "screens stay readable; the sky, moon, stars and single lights are ignored).",
                                    "--exposure-metering <average|highlight>"));
    addSetting(floatSetting<ES>("hdr_rotation", E, &ES::hdrRotation, "Environment rotation in degrees.", "--hdr-rotation <deg>"));
    addSetting(floatSetting<ES>("hdr_intensity", E, &ES::hdrIntensity, "Environment (IBL) intensity."));
    addSetting(floatSetting<ES>("hdr_blur", E, &ES::hdrBlur, "Background blur.", {}, std::pair{0.0, 1.0}));
    {
        SettingSpec spec = indexEnumSetting<ES>("time_of_day", E, &ES::timeOfDay, {"day", "night", "auto"},
                                                "ASOBO_material_emissive day/night multiplier: day, night, or auto "
                                                "(night while the sun's apparent elevation is below night_threshold). "
                                                "Not the clock: see time_of_day_mode.",
                                                "--time-of-day <day|night|auto>");
        spec.enumAliases = {{"automatic", "auto"}};
        addSetting(std::move(spec));
    }
    {
        using ux3d::imgui::TimeOfDayState;
        using ux3d::imgui::TimePreset;
        auto clockBool = [&](const char* key, bool TimeOfDayState::*member, const char* help, const char* cli) {
            SettingSpec spec;
            spec.key = key;
            spec.scope = E;
            spec.type = SettingType::Bool;
            spec.help = help;
            spec.cliFlag = cli;
            spec.get = [member](void* t) { return json(static_cast<ES*>(t)->clock.*member); };
            spec.set = [member, sunMode = std::string(key) == "time_of_day_mode"](void* t, const json& v, std::string&) {
                auto& env = *static_cast<ES*>(t);
                env.clock.*member = v.get<bool>();
                if (sunMode) {
                    // On = the Inspector's Sun Source "Time of Day" (selectSunSource: the HDR sun is enabled, so a later
                    // "off" restores the retained HDR mode, Manual or From HDR, exactly as after the UI selection).
                    if (v.get<bool>()) env.sunEnabled = true;
                    env.sunSource = v.get<bool>() ? ux3d::imgui::SunSource::Clock : !env.sunEnabled
                        ? ux3d::imgui::SunSource::Off : env.sunMode == static_cast<int>(ux3d::imgui::SunMode::FromHdr)
                        ? ux3d::imgui::SunSource::FromHdr : ux3d::imgui::SunSource::Manual;
                    env.skySunSource = static_cast<int>(v.get<bool>() ? ux3d::imgui::SunSource::Clock : ux3d::imgui::SunSource::Manual);
                }
                return true;
            };
            addSetting(std::move(spec));
        };
        auto clockFloat = [&](const char* key, float TimeOfDayState::*member, const char* help, const char* cli,
                              double minimum, double maximum, bool wrapDegrees = false) {
            SettingSpec spec;
            spec.key = key;
            spec.scope = E;
            spec.type = SettingType::Float;
            spec.help = help;
            spec.cliFlag = cli;
            spec.hasRange = true;
            spec.minimum = minimum;
            spec.maximum = maximum;
            spec.get = [member](void* t) { return agent::floatJson(static_cast<ES*>(t)->clock.*member); };
            spec.set = [member, wrapDegrees](void* t, const json& v, std::string&) {
                float value = static_cast<float>(v.get<double>());
                if (wrapDegrees) value = static_cast<float>(ux3d::imgui::solar::normalizeDegrees(value));
                static_cast<ES*>(t)->clock.*member = value;
                return true;
            };
            addSetting(std::move(spec));
        };
        clockBool("time_of_day_mode", &TimeOfDayState::enabled,
                  "Legacy source selection for both environments: on = Clock; off restores the retained HDR enable/mode "
                  "and sky Manual. Later source selections win. Not emissive day/night: see time_of_day.",
                  "--time-of-day-mode <on|off>");
        {
            SettingSpec spec;
            spec.key = "date";
            spec.scope = E;
            spec.type = SettingType::String;
            spec.help = "Local calendar date at the location, YYYY-MM-DD (1900..2100).";
            spec.cliFlag = "--date YYYY-MM-DD";
            spec.get = [](void* t) {
                const auto& c = static_cast<ES*>(t)->clock;
                return json(ux3d::imgui::formatDate(c.year, c.month, c.day));
            };
            spec.set = [](void* t, const json& v, std::string& error) {
                int y = 0, m = 0, d = 0;
                if (!ux3d::imgui::parseDate(v.get<std::string>(), y, m, d, error)) return false;
                auto& c = static_cast<ES*>(t)->clock;
                c.year = y;
                c.month = m;
                c.day = d;
                return true;
            };
            addSetting(std::move(spec));
        }
        {
            SettingSpec spec;
            spec.key = "time";
            spec.scope = E;
            spec.type = SettingType::String;
            spec.help = "Local clock time, HH:MM[:SS] (or decimal hours). Selects Custom: the time_preset is dropped.";
            spec.cliFlag = "--time HH:MM[:SS]";
            spec.get = [](void* t) {
                return json(ux3d::imgui::formatTime(ux3d::imgui::effectiveLocalHours(static_cast<ES*>(t)->clock)));
            };
            spec.set = [](void* t, const json& v, std::string& error) {
                double hours = 0.0;
                if (!ux3d::imgui::parseTime(v.get<std::string>(), hours, error)) return false;
                auto& c = static_cast<ES*>(t)->clock;
                c.hours = hours;
                c.preset = static_cast<int>(TimePreset::Custom);
                return true;
            };
            addSetting(std::move(spec));
        }
        clockFloat("utc_offset", &TimeOfDayState::utcOffsetHours,
                   "Hours the local clock is ahead of UTC (UTC = time - utc_offset), e.g. -7 for Pacific daylight time.",
                   "--utc-offset <hours>", -12.0, 14.0);
        clockFloat("latitude", &TimeOfDayState::latitude, "Latitude in degrees, positive north.", "--latitude <deg>", -90.0, 90.0);
        clockFloat("longitude", &TimeOfDayState::longitude, "Longitude in degrees, positive east.", "--longitude <deg>", -180.0, 180.0);
        clockFloat("heading", &TimeOfDayState::heading,
                   "True compass direction the model's nose points to, degrees (0 = north, 90 = east); MSFS aircraft "
                   "have their nose on glTF +Z and their left side on +X.",
                   "--heading <deg>", -3600.0, 3600.0, true);
        {
            SettingSpec spec;
            spec.key = "time_preset";
            spec.scope = E;
            spec.type = SettingType::Enum;
            spec.help = "dawn / morning / noon / afternoon / dusk / night for the current date and place (dawn and dusk: "
                        "the sun 3 degrees up; noon: solar transit; night: solar midnight; morning and afternoon: halfway). "
                        "A preset follows date, latitude, longitude and utc_offset; custom keeps the current time.";
            spec.cliFlag = "--time-preset <dawn|morning|noon|afternoon|dusk|night>";
            spec.enumValues.assign(ux3d::imgui::kTimePresetNames.begin(), ux3d::imgui::kTimePresetNames.end());
            spec.get = [](void* t) {
                const int preset = std::clamp(static_cast<ES*>(t)->clock.preset, 0, static_cast<int>(TimePreset::Count) - 1);
                return json(ux3d::imgui::kTimePresetNames[static_cast<size_t>(preset)]);
            };
            spec.set = [](void* t, const json& v, std::string&) {
                auto& c = static_cast<ES*>(t)->clock;
                const std::string name = v.get<std::string>();
                int index = 0;
                for (size_t i = 0; i < ux3d::imgui::kTimePresetNames.size(); ++i) {
                    if (name == ux3d::imgui::kTimePresetNames[i]) index = static_cast<int>(i);
                }
                if (index == static_cast<int>(TimePreset::Custom)) ux3d::imgui::materializePreset(c);
                else c.preset = index;
                return true;
            };
            addSetting(std::move(spec));
        }
        clockBool("sun_refraction", &TimeOfDayState::refraction,
                  "Atmospheric refraction raises the lit sun's elevation (about 0.5 degrees at the horizon).",
                  "--sun-refraction <on|off>");
        clockFloat("night_threshold", &TimeOfDayState::nightThreshold,
                   "time_of_day auto: night while the sun's elevation is below this many degrees (-6 = civil twilight).",
                   "--night-threshold <deg>", -90.0, 90.0);
        {
            SettingSpec spec;
            spec.key = "time_playing";
            spec.scope = E;
            spec.type = SettingType::Bool;
            spec.help = "Advance the clock by time_scale x real time (session state, not saved).";
            spec.cliFlag = "--time-play <on|off>";
            spec.get = [](void* t) { return json(static_cast<ES*>(t)->clock.playing); };
            spec.set = [](void* t, const json& v, std::string&) {
                auto& c = static_cast<ES*>(t)->clock;
                c.playing = v.get<bool>();
                if (c.playing) ux3d::imgui::materializePreset(c);
                return true;
            };
            addSetting(std::move(spec));
        }
        clockFloat("time_scale", &TimeOfDayState::timeScale,
                   "Simulated seconds per real second while time_playing (600 = 10 minutes per second).",
                   "--time-scale <x>", 0.0, 1.0e6);
    }
    {
        auto spec = floatSetting<ES>("msfs_emissive_scale", E, &ES::msfsEmissiveScale,
            "Effective ASOBO emissive scale: sky 1/12800 (cd/m^2 to environment units), HDR 1/1000. "
            "Setting a value overrides both sources. Core glTF, including KHR emissive_strength, is unchanged.",
            "--msfs-emissive-scale <x>");
        spec.get = [](void* t) { return json(ux3d::imgui::effectiveMsfsEmissiveScale(*static_cast<ES*>(t))); };
        spec.set = [](void* t, const json& v, std::string&) {
            auto& environment = *static_cast<ES*>(t);
            environment.msfsEmissiveScale = static_cast<float>(v.get<double>());
            environment.msfsEmissiveScaleOverride = true;
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(boolSetting<ES>("bloom", E, &ES::bloomEnabled, "Physically based bloom.", "--bloom <on|off>"));
    addSetting(floatSetting<ES>("bloom_intensity", E, &ES::bloomIntensity, "Bloom intensity.", "--bloom-intensity <x>"));
    addSetting(floatSetting<ES>("bloom_radius", E, &ES::bloomRadius, "Bloom radius.", "--bloom-radius <x>"));
    {
        // The exposure mode as a switch: on = auto, off = manual (also with the procedural sky, so scripts that pass
        // --auto-exposure off keep the manual EV); get is true while Auto is in effect.
        SettingSpec spec;
        spec.key = "auto_exposure";
        spec.scope = E;
        spec.type = SettingType::Bool;
        spec.help = "Auto exposure (metered): on = exposure_mode auto, off = exposure_mode manual.";
        spec.cliFlag = "--auto-exposure <on|off>";
        spec.get = [](void* t) {
            return json(ux3d::imgui::effectiveExposureMode(*static_cast<ES*>(t)) == ux3d::imgui::ExposureMode::Auto);
        };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->exposureMode =
                static_cast<int>(v.get<bool>() ? ux3d::imgui::ExposureMode::Auto : ux3d::imgui::ExposureMode::Manual);
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("exposure_compensation", E, &ES::exposureCompensationEV,
                                              "Auto-exposure compensation in EV.", "--exposure-compensation <ev>"));
    addSetting(boolSetting<ES>("background", E, &ES::showBackground, "Skybox background (off: clear colour).", "--clear-color disables it"));
    {
        SettingSpec spec;
        spec.key = "clear_color";
        spec.scope = E;
        spec.type = SettingType::Color3;
        spec.help = "Background colour (linear) used while background is off.";
        spec.cliFlag = "--clear-color <r,g,b>";
        spec.get = [](void* t) {
            const glm::vec4& c = static_cast<ES*>(t)->clearColor;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b)};
        };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->clearColor = glm::vec4(v[0].get<float>(), v[1].get<float>(), v[2].get<float>(), 1.0f);
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(boolSetting<ES>("ibl", E, &ES::iblEnabled, "Image-based lighting."));
    addSetting(boolSetting<ES>("punctual_lights", E, &ES::punctualLightsEnabled, "KHR_lights_punctual lights."));
    // A3.1 sun (SceneEnvironmentState: conventions there and in docs/ASOBO_EXTENSIONS_PHASE_3.md).
    {
        auto spec = boolSetting<ES>("sun", E, &ES::sunEnabled,
                               "Sun (directional light). While on, the HDR's own sun is removed from IBL and background.",
                               "--sun <on|off>");
        spec.set = [](void* t, const json& v, std::string&) {
            auto& env = *static_cast<ES*>(t);
            env.sunEnabled = v.get<bool>();
            env.sunSource = !env.sunEnabled ? ux3d::imgui::SunSource::Off
                : env.sunMode == static_cast<int>(ux3d::imgui::SunMode::FromHdr) ? ux3d::imgui::SunSource::FromHdr
                                                                                 : ux3d::imgui::SunSource::Manual;
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sun_source";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.enumValues = {"off", "hdr", "manual", "clock"};
        spec.cliFlag = "--sun-source <off|hdr|manual|clock>";
        spec.help = "Sun Source: HDR supports off/hdr/manual/clock; sky supports manual/clock. Sky keeps its own "
                    "choice; invalid sky choices map to manual. HDR is locked to the panorama, never moved by the clock. "
                    "Applied in order: legacy sun/sun_mode target HDR, time_of_day_mode selects both environments, "
                    "sun_source selects the current environment (last applicable selection wins).";
        spec.get = [](void* t) { return json(ux3d::imgui::sunSourceName(ux3d::imgui::effectiveSunSource(*static_cast<ES*>(t)))); };
        spec.set = [](void* t, const json& v, std::string&) {
            const std::string value = v.get<std::string>();
            ux3d::imgui::selectSunSource(*static_cast<ES*>(t), value == "off" ? ux3d::imgui::SunSource::Off
                : value == "hdr" ? ux3d::imgui::SunSource::FromHdr : value == "clock" ? ux3d::imgui::SunSource::Clock
                                                                                  : ux3d::imgui::SunSource::Manual);
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec = indexEnumSetting<ES>("sun_mode", E, &ES::sunMode, {"manual", "hdr"},
                                                "Sun from the manual angles/colour/intensity, or found in the HDR (turned with "
                                                "hdr_rotation, scaled by hdr_intensity).",
                                                "--sun-mode <manual|hdr>");
        spec.enumAliases = {{"from_hdr", "hdr"}, {"fromhdr", "hdr"}};
        spec.set = [](void* t, const json& v, std::string&) {
            auto& env = *static_cast<ES*>(t);
            env.sunMode = v.get<std::string>() == "hdr" ? 1 : 0;
            if (env.sunEnabled || env.sunSource == ux3d::imgui::SunSource::Clock) {
                env.sunSource = env.sunMode != 0 ? ux3d::imgui::SunSource::FromHdr : ux3d::imgui::SunSource::Manual;
                env.sunEnabled = true;
            }
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sun_dir";
        spec.aliases = {"sun_direction"};
        spec.scope = E;
        spec.type = SettingType::Vec2;
        spec.help = "Manual sun azimuth and elevation in degrees: azimuth around +Y from +Z towards +X, elevation above "
                    "the horizon (glTF Y-up).";
        spec.cliFlag = "--sun-dir <az,el>";
        spec.get = [](void* t) {
            const auto* env = static_cast<ES*>(t);
            return json{agent::floatJson(env->sunAzimuth), agent::floatJson(env->sunElevation)};
        };
        spec.set = [](void* t, const json& v, std::string& error) {
            const float elevation = v[1].get<float>();
            if (elevation < -90.0f || elevation > 90.0f) {
                error = "sun_dir elevation must be in [-90, 90]";
                return false;
            }
            static_cast<ES*>(t)->sunAzimuth = v[0].get<float>();
            static_cast<ES*>(t)->sunElevation = elevation;
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("sun_azimuth", E, &ES::sunAzimuth, "Manual sun azimuth in degrees (from +Z towards +X).",
                                "--sun-dir <az,el>"));
    addSetting(floatSetting<ES>("sun_elevation", E, &ES::sunElevation, "Manual sun elevation in degrees above the horizon.",
                                "--sun-dir <az,el>", std::pair{-90.0, 90.0}));
    {
        SettingSpec spec;
        spec.key = "sun_color";
        spec.scope = E;
        spec.type = SettingType::Color3;
        spec.help = "Manual sun colour, linear RGB (luminance 1 keeps sun_intensity the illuminance).";
        spec.cliFlag = "--sun-color <r,g,b>";
        spec.hasRange = true;
        spec.minimum = 0.0;
        spec.maximum = 1000.0;
        spec.get = [](void* t) {
            const glm::vec3& c = static_cast<ES*>(t)->sunColor;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b)};
        };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->sunColor = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("sun_intensity", E, &ES::sunIntensity,
                                "Manual sun illuminance (environment radiance units x sr; a white Lambert surface facing "
                                "the sun reflects colour x intensity / pi).",
                                "--sun-intensity <x>", std::pair{0.0, 1.0e6}));
    addSetting(floatSetting<ES>("sun_angular_radius", E, &ES::sunAngularRadius,
                                "Sun angular radius in degrees (widens the specular highlight; 0.27 = the real sun).",
                                "--sun-angular-radius <deg>", std::pair{0.0, 10.0}));
    // A3.8 viewer ambient light (SceneEnvironment.h kAmbientUnitLuminance): not part of the glTF.
    {
        SettingSpec spec;
        spec.key = "ambient";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.enumValues = {"auto", "on", "off"};
        spec.enumAliases = {{"true", "on"}, {"false", "off"}, {"1", "on"}, {"0", "off"}};
        spec.help = "Viewer ambient: auto (default) = enabled with sky, disabled with HDR; on/off force it. "
                    "Legacy on selects Manual when no intensity mode was specified. Never part of the glTF.";
        spec.cliFlag = "--ambient <auto|on|off>";
        spec.get = [](void* t) { const int a = static_cast<ES*>(t)->ambient; return json(a < 0 ? "auto" : a ? "on" : "off"); };
        spec.set = [](void* t, const json& v, std::string&) {
            auto& env = *static_cast<ES*>(t);
            const std::string value = v.get<std::string>();
            env.ambient = value == "auto" ? -1 : value == "on" ? 1 : 0;
            if (value == "on" && env.ambientMode < 0) env.ambientMode = 0;
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        auto spec = indexEnumSetting<ES>("ambient_mode", E, &ES::ambientMode, {"manual", "auto"},
            "Ambient intensity: Manual uses cd/m^2; Auto uses white L = fill * outdoor horizontal illuminance / pi, "
            "weighted by hidden sky where a visibility volume is active (uniform without a volume).",
            "--ambient-mode <auto|manual>");
        spec.get = [](void* t) { return json(ux3d::imgui::effectiveAmbientAuto(*static_cast<ES*>(t)) ? "auto" : "manual"); };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("ambient_fill", E, &ES::ambientFill, "Auto ambient fill fraction k (0..1).",
                                "--ambient-fill <k>", std::pair{0.0, 1.0}));
    {
        SettingSpec spec;
        spec.key = "ambient_color";
        spec.scope = E;
        spec.type = SettingType::Color3;
        spec.help = "Ambient light colour, linear RGB (multiplies ambient_intensity; white = luminance 1).";
        spec.cliFlag = "--ambient-color <r,g,b>";
        spec.hasRange = true;
        spec.minimum = 0.0;
        spec.maximum = 1000.0;
        spec.get = [](void* t) {
            const glm::vec3& c = static_cast<ES*>(t)->ambientColor;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b)};
        };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->ambientColor = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("ambient_intensity", E, &ES::ambientLuminance,
                                "Ambient luminance in cd/m^2 (1 environment unit = 12,800 cd/m^2 with both sources; the "
                                "HDR has no absolute scale, so the same unit is used for it).",
                                "--ambient-intensity <cd/m^2>", std::pair{0.0, static_cast<double>(ux3d::imgui::kAmbientMaxLuminance)}));
    // A3.9 sky occlusion inside the model (SkyOcclusion.h): scene-level, like the environment it occludes.
    {
        SettingSpec spec;
        spec.key = "sky_occlusion";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.help = "Sky occlusion from the model's own geometry (a baked sky-visibility volume) for the IBL terms: auto "
                    "(default) = on with the procedural sky, off with the HDR. The sun, punctual lights, emissive, the "
                    "Manual ambient light and the background are unchanged. Auto ambient fills the hidden-sky fraction.";
        spec.cliFlag = "--sky-occlusion <auto|on|off>";
        spec.enumValues = {"auto", "on", "off"};
        spec.get = [](void* t) {
            const int value = static_cast<ES*>(t)->skyOcclusion;
            return json(value < 0 ? "auto" : value != 0 ? "on" : "off");
        };
        spec.set = [](void* t, const json& v, std::string&) {
            const std::string value = v.get<std::string>();
            static_cast<ES*>(t)->skyOcclusion = value == "auto" ? -1 : value == "on" ? 1 : 0;
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(indexEnumSetting<ES>("sky_occlusion_quality", E, &ES::skyOcclusionQuality, {"low", "medium", "high"},
                                    "Sky occlusion bake: low 64 directions x up to 64 cells per axis, medium (default) "
                                    "128 x 96, high 256 x 128.",
                                    "--sky-occlusion-quality <low|medium|high>"));
    addSetting(boolSetting<ES>("sky_occlusion_occluders", E, &ES::skyOcclusionOccluders,
                               "Environment occluders (ASOBO_material_environment_occluder) block the sky in the sky "
                               "occlusion bake (default on; they stay hidden in the image either way).",
                               "--sky-occlusion-occluders <on|off>"));
    // A6.7 sun disk (SunDisk.h): background only, never in the IBL input.
    {
        SettingSpec spec;
        spec.key = "sun_disk";
        spec.scope = E;
        spec.type = SettingType::Bool;
        spec.help = "Sun disk in the background (both environment sources) while the sun is on: mean radiance "
                    "E / (pi sin^2 R), limb-darkened, never in the IBL input.";
        spec.cliFlag = "--sun-disk <on|off>";
        spec.get = [](void* t) { return json(static_cast<ES*>(t)->sunDisk.enabled); };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->sunDisk.enabled = v.get<bool>();
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sun_disk_scale";
        spec.scope = E;
        spec.type = SettingType::Float;
        spec.help = "Artistic size multiplier of the sun disk's angular radius (1 = sun_angular_radius); the disk keeps "
                    "the sun's illuminance, so a larger disk is dimmer. The A3.1 highlight is not scaled.";
        spec.cliFlag = "--sun-disk-scale <x>";
        spec.hasRange = true;
        spec.minimum = 0.0;
        spec.maximum = ux3d::imgui::sundisk::kMaxScale;
        spec.get = [](void* t) { return agent::floatJson(static_cast<ES*>(t)->sunDisk.scale); };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->sunDisk.scale = static_cast<float>(v.get<double>());
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sun_disk_limb_darkening";
        spec.scope = E;
        spec.type = SettingType::Bool;
        spec.help = "Limb darkening of the sun disk (Hestroffer & Magnan power law; off = uniform disk, same mean).";
        spec.cliFlag = "--sun-disk-limb-darkening <on|off>";
        spec.get = [](void* t) { return json(static_cast<ES*>(t)->sunDisk.limbDarkening); };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->sunDisk.limbDarkening = v.get<bool>();
            return true;
        };
        addSetting(std::move(spec));
    }
    // A6.2 procedural sky (SceneEnvironmentState; model and units in SkyAtmosphere.h).
    {
        SettingSpec spec = indexEnumSetting<ES>("environment", E, &ES::environmentSource, {"hdr", "sky"},
                                                "Environment source: the HDR panorama, or the procedural sky (Hillaire 2020 "
                                                "atmosphere: background, IBL filtered from it, sun colour and illuminance from "
                                                "its transmittance; the sun is always on).",
                                                "--environment <hdr|sky>");
        spec.aliases = {"environment_source"};
        spec.enumAliases = {{"procedural", "sky"}, {"panorama", "hdr"}};
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("sky_mie", E, &ES::skyMieDensity,
                                "Procedural sky: Mie (aerosol) density multiplier; 1 = clear sky, larger = hazier.",
                                "--sky-mie <x>", std::pair{0.0, 20.0}));
    addSetting(boolSetting<ES>("sky_ozone", E, &ES::skyOzone, "Procedural sky: ozone absorption.", "--sky-ozone <on|off>"));
    addSetting(floatSetting<ES>("sky_ground_albedo", E, &ES::skyGroundAlbedo,
                                "Procedural sky: grey ground albedo below the horizon; -1 = automatic (the plane or ocean "
                                "albedo of the lowest-id open viewport that draws one, else 0.3).",
                                "--sky-ground-albedo <x|auto>", std::pair{-1.0, 1.0}));
    // A6.6 night sky (NightSky.h): moon and stars need the A6.1 clock; the glow is part of the sky (and its IBL).
    {
        auto nightBool = [&](const char* key, bool ux3d::imgui::night::NightSettings::*member, const char* help, const char* cli) {
            SettingSpec spec;
            spec.key = key;
            spec.scope = E;
            spec.type = SettingType::Bool;
            spec.help = help;
            spec.cliFlag = cli;
            spec.get = [member](void* t) { return json(static_cast<ES*>(t)->nightSky.*member); };
            spec.set = [member](void* t, const json& v, std::string&) {
                static_cast<ES*>(t)->nightSky.*member = v.get<bool>();
                return true;
            };
            addSetting(std::move(spec));
        };
        auto nightFloat = [&](const char* key, float ux3d::imgui::night::NightSettings::*member, const char* help, const char* cli,
                              double lo, double hi) {
            SettingSpec spec;
            spec.key = key;
            spec.scope = E;
            spec.type = SettingType::Float;
            spec.help = help;
            spec.cliFlag = cli;
            spec.hasRange = true;
            spec.minimum = lo;
            spec.maximum = hi;
            spec.get = [member](void* t) { return agent::floatJson(static_cast<ES*>(t)->nightSky.*member); };
            spec.set = [member](void* t, const json& v, std::string&) {
                static_cast<ES*>(t)->nightSky.*member = static_cast<float>(v.get<double>());
                return true;
            };
            addSetting(std::move(spec));
        };
        nightBool("moon", &ux3d::imgui::night::NightSettings::moonEnabled,
                  "Procedural sky with the clock: the moon disk (A6.1 position and phase) and the moonlight (the directional "
                  "light with the A3.2 shadows while it is brighter than the sun).",
                  "--moon <on|off>");
        nightFloat("moonlight_scale", &ux3d::imgui::night::NightSettings::moonlightScale,
                   "Multiplier of the moonlight (full moon about 0.25 lux); the disk is not scaled.", "--moonlight-scale <x>",
                   0.0, 1000.0);
        nightBool("stars", &ux3d::imgui::night::NightSettings::starsEnabled,
                  "Procedural sky with the clock: procedural stars turning with the sidereal time, fading in below civil "
                  "twilight (background only).",
                  "--stars <on|off>");
        nightFloat("star_density", &ux3d::imgui::night::NightSettings::starDensity,
                   "Multiplier of the number of stars (1 = about 8,900 down to magnitude 6.5).", "--star-density <x>", 0.0, 10.0);
        nightFloat("night_vision", &ux3d::imgui::night::NightSettings::nightVision,
                   "Procedural sky with Auto/Curve exposure: largest weight of the per-pixel rod-vision shift (dim pixels "
                   "desaturate towards blue-grey below 3 cd/m^2; Jensen et al. 2000); 0 = off.", "--night-vision <0..1>", 0.0, 1.0);
        nightFloat("sky_airglow", &ux3d::imgui::night::NightSettings::airglow,
                   "Night sky glow level of the procedural sky (1 = 2e-4 cd/m^2 at the zenith, a dark moonless sky).",
                   "--sky-airglow <x>", 0.0, 1000.0);
    }

    // --- Renderer-wide ---
    {
        SettingSpec spec;
        spec.key = "sky_time_slicing";
        spec.scope = R;
        spec.type = SettingType::Bool;
        spec.help = "Procedural sky: spread IBL re-filtering over frames (reduced size while the sky changes, refined when "
                    "it stops). Off filters every change at the configured quality before the frame (reference path).";
        spec.cliFlag = "--sky-time-slicing <on|off>";
        spec.defaultValue = true;
        spec.get = [this](void*) { return json(s_skyTimeSlicing); };
        spec.set = [this](void*, const json& v, std::string&) {
            s_skyTimeSlicing = v.get<bool>();
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sky_budget_ms";
        spec.scope = R;
        spec.type = SettingType::Float;
        spec.help = "Procedural sky: GPU milliseconds per frame the time-sliced IBL work aims at (stated limit 4 ms).";
        spec.cliFlag = "--sky-budget-ms <ms>";
        spec.hasRange = true;
        spec.minimum = 0.1;
        spec.maximum = 100.0;
        spec.defaultValue = 2.5;
        spec.get = [this](void*) { return agent::floatJson(s_skyBudgetMs); };
        spec.set = [this](void*, const json& v, std::string&) {
            s_skyBudgetMs = static_cast<float>(v.get<double>());
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "msfs_emissive";
        spec.scope = R;
        spec.type = SettingType::Bool;
        spec.help = "MSFS emissive mapping for ASOBO assets (off: clamped core emissive factor).";
        spec.cliFlag = "--msfs-emissive <on|off>";
        spec.defaultValue = true;
        spec.get = [this](void*) { return json(!s_disableMsfsEmissive); };
        spec.set = [this](void*, const json& v, std::string&) {
            s_disableMsfsEmissive = !v.get<bool>();
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "grid_mode";
        spec.scope = R;
        spec.type = SettingType::Enum;
        spec.help = "Ground grid technique (renderer-wide).";
        spec.cliFlag = "--grid-mode <ray|plane>";
        spec.enumValues = {"ray", "plane"};
        spec.enumAliases = {{"screenray", "ray"}, {"planemesh", "plane"}};
        spec.persisted = true;
        spec.defaultValue = "ray";
        spec.get = [](void*) {
            return json(ux3d::imgui::RenderQuality::settings().gridMode == ux3d::imgui::GridMode::PlaneMesh ? "plane" : "ray");
        };
        spec.set = [](void*, const json& v, std::string&) {
            ux3d::imgui::RenderQuality::settings().gridMode =
                v.get<std::string>() == "plane" ? ux3d::imgui::GridMode::PlaneMesh : ux3d::imgui::GridMode::ScreenRay;
            return true;
        };
        addSetting(std::move(spec));
    }

    // --- Renderer-wide: A3.2 sun shadows (RenderQualitySettings; saved preferences like grid_mode) ---
    {
        using RQS = ux3d::imgui::RenderQualitySettings;
        const RQS defaults;
        auto shadowSpec = [](std::string key, SettingType type, std::string help, std::string cli) {
            SettingSpec spec;
            spec.key = std::move(key);
            spec.scope = SettingScope::Renderer;
            spec.type = type;
            spec.help = std::move(help);
            spec.cliFlag = std::move(cli);
            spec.persisted = true;
            return spec;
        };
        {
            SettingSpec spec = shadowSpec("shadows", SettingType::Bool,
                                          "Cascaded sun shadow maps (drawn only while the sun is on).", "--shadows <on|off>");
            spec.defaultValue = defaults.shadows;
            spec.get = [](void*) { return json(ux3d::imgui::RenderQuality::settings().shadows); };
            spec.set = [](void*, const json& v, std::string&) {
                ux3d::imgui::RenderQuality::settings().shadows = v.get<bool>();
                ux3d::imgui::RenderQuality::pinShadows();
                return true;
            };
            addSetting(std::move(spec));
        }
        {
            SettingSpec spec = shadowSpec("shadow_resolution", SettingType::Enum,
                                          "Shadow map size per cascade (texels per side).", "--shadow-resolution <1024|2048|4096>");
            spec.enumValues = {"1024", "2048", "4096"};
            spec.defaultValue = std::to_string(defaults.shadowResolution);
            spec.get = [](void*) { return json(std::to_string(ux3d::imgui::RenderQuality::settings().shadowResolution)); };
            spec.set = [](void*, const json& v, std::string&) {
                ux3d::imgui::RenderQuality::settings().shadowResolution =
                    static_cast<uint32_t>(std::stoul(v.get<std::string>()));
                ux3d::imgui::RenderQuality::pinShadows();
                return true;
            };
            addSetting(std::move(spec));
        }
        {
            SettingSpec spec = shadowSpec("shadow_cascades", SettingType::Int, "Number of shadow cascades.",
                                          "--shadow-cascades <2..4>");
            spec.hasRange = true;
            spec.minimum = 2.0;
            spec.maximum = 4.0;
            spec.defaultValue = defaults.shadowCascades;
            spec.get = [](void*) { return json(ux3d::imgui::RenderQuality::settings().shadowCascades); };
            spec.set = [](void*, const json& v, std::string&) {
                ux3d::imgui::RenderQuality::settings().shadowCascades = v.get<int>();
                ux3d::imgui::RenderQuality::pinShadows();
                return true;
            };
            addSetting(std::move(spec));
        }
        {
            SettingSpec spec = shadowSpec("shadow_filter", SettingType::Enum,
                                          "Shadow filter: hard (1 bilinear compare), pcf (3x3), soft (16 rotated Poisson).",
                                          "--shadow-filter <hard|pcf|soft>");
            spec.enumValues = {"hard", "pcf", "soft"};
            spec.defaultValue = ux3d::imgui::RenderQuality::shadowFilterName(defaults.shadowFilter);
            spec.get = [](void*) {
                return json(ux3d::imgui::RenderQuality::shadowFilterName(ux3d::imgui::RenderQuality::settings().shadowFilter));
            };
            spec.set = [](void*, const json& v, std::string&) {
                const std::string name = v.get<std::string>();
                ux3d::imgui::RenderQuality::settings().shadowFilter = name == "hard" ? ux3d::imgui::ShadowFilter::Hard
                    : name == "soft" ? ux3d::imgui::ShadowFilter::Soft : ux3d::imgui::ShadowFilter::Pcf;
                ux3d::imgui::RenderQuality::pinShadows();
                return true;
            };
            addSetting(std::move(spec));
        }
        const auto floatShadow = [&](std::string key, float RQS::*member, std::string help, std::string cli, double lo, double hi) {
            SettingSpec spec = shadowSpec(std::move(key), SettingType::Float, std::move(help), std::move(cli));
            spec.hasRange = true;
            spec.minimum = lo;
            spec.maximum = hi;
            spec.defaultValue = agent::floatJson(defaults.*member);
            spec.get = [member](void*) { return agent::floatJson(ux3d::imgui::RenderQuality::settings().*member); };
            spec.set = [member](void*, const json& v, std::string&) {
                ux3d::imgui::RenderQuality::settings().*member = v.get<float>();
                ux3d::imgui::RenderQuality::pinShadows();
                return true;
            };
            addSetting(std::move(spec));
        };
        floatShadow("shadow_distance", &RQS::shadowDistance, "View depth (metres) where the sun shadows end.",
                    "--shadow-distance <m>", 1.0, 100000.0);
        floatShadow("shadow_split_lambda", &RQS::shadowSplitLambda,
                    "Cascade splits: 0 uniform .. 1 logarithmic (practical split scheme).", "--shadow-split-lambda <0..1>",
                    0.0, 1.0);
        floatShadow("shadow_normal_bias", &RQS::shadowNormalBias, "Receiver normal offset in shadow-map texels.",
                    "--shadow-normal-bias <x>", 0.0, 100.0);
        floatShadow("shadow_slope_bias", &RQS::shadowSlopeBias, "Caster slope-scaled depth bias (glPolygonOffset factor).",
                    "--shadow-slope-bias <x>", 0.0, 100.0);
        floatShadow("shadow_constant_bias", &RQS::shadowConstantBias, "Caster constant depth bias (glPolygonOffset units).",
                    "--shadow-constant-bias <x>", 0.0, 100.0);
    }
}

// ---------------------------------------------------------------------------------------------
// Command registry
// ---------------------------------------------------------------------------------------------

void Application::registerAgentCommands()
{
    if (agentRegistryReady) return;
    agentRegistryReady = true;
    registerAgentSettings();
    using agent::CommandKind;
    using agent::CommandOutcome;
    using agent::CommandSpec;
    using agent::CommandContext;

    // Resolves args[key] to a node (exact, case-insensitive, unique substring or #index).
    auto resolveNode = [this](const json& args, const char* key, std::optional<CommandReply>& failure) -> ux3d::gltf::Node* {
        const std::string spec = args.value(key, std::string());
        std::vector<std::string> candidates;
        ux3d::gltf::Node* node = findNode(spec, &candidates);
        if (node == nullptr) {
            std::string text = "node not found: " + spec;
            if (!candidates.empty()) {
                text += " (candidates: ";
                for (size_t i = 0; i < candidates.size(); ++i) text += (i ? ", " : "") + candidates[i];
                text += ")";
            }
            failure = CommandReply::failure("node_not_found", text, {{"candidates", candidates}});
        }
        return node;
    };
    auto resolveViewport = [this](const json& args, std::optional<CommandReply>& failure) -> ux3d::imgui::ViewportInstance* {
        if (args.contains("viewport") && !args["viewport"].is_null()) {
            auto* vp = findViewportById(static_cast<uint32_t>(args["viewport"].get<long long>()));
            if (vp == nullptr) failure = CommandReply::failure("viewport_not_found", "no viewport " + args["viewport"].dump());
            return vp;
        }
        if (activeViewport == nullptr) failure = CommandReply::failure("no_viewport", "no active viewport");
        return activeViewport;
    };
    auto windowMinimized = [this] {
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        return w <= 0 || h <= 0 || glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
    };
    auto markViewport = [this](ux3d::imgui::ViewportInstance* vp) {
        vp->antiAliasing.resetHistory();
        sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
    };

    // ---- State and session ----
    {
        CommandSpec spec;
        spec.name = "describe";
        spec.help = "List every command with its JSON-schema arguments, kind and script form.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            json out;
            out["app"] = "Glider";
            out["protocol"] = kAgentProtocolVersion;
            out["guideVersion"] = kAgentProtocolVersion;
            out["pipe"] = controlPipe ? json(controlPipe->name()) : json(nullptr);
            out["commands"] = agentRegistry.describe();
            out["settingCount"] = agentRegistry.settings().size();
            return CommandReply::success(out);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "guide";
        spec.help = "The agent guide (Markdown) with the generated command and settings reference.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            return CommandReply::success({{"text", buildAgentGuide()}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_state";
        spec.aliases = {"state"};
        spec.help = "Model, counts, selection, isolation, viewports, camera, render mode, validation summary, loading state.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome { return CommandReply::success(agentStateJson()); };
        spec.legacyResult = [this](const json&, const json&) { return legacyStateJson(); };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "list_nodes";
        spec.help = "Nodes with index, name, parent, children, mesh, materials and visibility.";
        spec.args = objectSchema({{"filter", prop("string", "Case-insensitive substring of the node name.")},
                                  {"limit", withDefault(prop("integer", "Maximum number of nodes returned."), 200)}});
        spec.positional = {"filter"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            const std::string filter = lowerText(args.value("filter", std::string()));
            const long long limit = args.value("limit", 200LL);
            std::map<uint32_t, uint32_t> parents;
            for (const auto& node : gltfModel.getNodes()) {
                for (const auto* child : node.getChildren()) parents[child->m_ID] = node.m_ID;
            }
            json nodes = json::array();
            size_t matched = 0;
            for (const auto& node : gltfModel.getNodes()) {
                if (!filter.empty() && lowerText(nodeName(&node)).find(filter) == std::string::npos) continue;
                ++matched;
                if (static_cast<long long>(nodes.size()) >= limit) continue;
                json entry = {{"index", node.m_ID}, {"name", nodeName(&node)}, {"visible", node.getVisible()},
                              {"children", node.getChildren().size()}};
                const auto parent = parents.find(node.m_ID);
                entry["parent"] = parent != parents.end() ? json(parent->second) : json(nullptr);
                if (const auto* mesh = node.getMesh()) {
                    entry["mesh"] = mesh->hasName() ? std::string(mesh->getName()) : "Mesh " + std::to_string(mesh->m_ID);
                    json materials = json::array();
                    for (const auto& primitive : mesh->getPrimitives()) {
                        const auto* material = primitive.getMaterial();
                        materials.push_back(material ? json(materialName(material)) : json(nullptr));
                    }
                    entry["materials"] = materials;
                }
                if (node.getCamera() != nullptr) entry["camera"] = true;
                nodes.push_back(std::move(entry));
            }
            return CommandReply::success({{"total", gltfModel.getNodes().size()}, {"matched", matched}, {"nodes", nodes}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "log_tail";
        spec.help = "Recent logger lines.";
        spec.args = objectSchema({{"lines", withDefault(prop("integer", "Number of lines."), 20)},
                                  {"level", prop("string", "Minimum level: info, warning or error.")}});
        spec.positional = {"lines"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            const long long lines = std::max(1LL, args.value("lines", 20LL));
            const std::string level = lowerText(args.value("level", std::string("info")));
            const int minimum = level == "error" ? 2 : level == "warning" ? 1 : 0;
            json out = json::array();
            const auto& entries = s_consoleLogs.entries();
            for (auto it = entries.rbegin(); it != entries.rend() && static_cast<long long>(out.size()) < lines; ++it) {
                const int severity = (it->level == ux3d::core::LogLevel::Error || it->level == ux3d::core::LogLevel::Fatal) ? 2
                                     : it->level == ux3d::core::LogLevel::Warning ? 1 : 0;
                if (severity < minimum) continue;
                out.insert(out.begin(), json{{"level", severity == 2 ? "error" : severity == 1 ? "warning" : "info"}, {"text", it->text}});
            }
            return CommandReply::success({{"lines", out}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_persist";
        spec.help = "Allow this pipe client's setting changes to be saved to settings.ini (default off).";
        spec.args = objectSchema({{"enabled", prop("boolean", "true allows saving.")}}, {"enabled"});
        spec.positional = {"enabled"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            agentPersist = args["enabled"].get<bool>();
            return CommandReply::success({{"persist", agentPersist}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "quit";
        spec.help = "Close Glider.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return CommandReply::success({{"quitting", true}});
        };
        spec.legacyResult = [](const json&, const json&) { return json("quitting"); };
        agentRegistry.add(std::move(spec));
    }

    // ---- Selection and visibility ----
    {
        CommandSpec spec;
        spec.name = "select";
        spec.help = "Select a node, like clicking it in the hierarchy.";
        spec.args = objectSchema({{"node", prop("string", "Node name (exact, case-insensitive, unique substring) or #index.")}}, {"node"});
        spec.positional = {"node"};
        spec.run = [this, resolveNode](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* node = resolveNode(args, "node", failure);
            if (failure) return *failure;
            selectNodeAction(node);
            sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
            return CommandReply::success({{"node", nodeRef(node)}});
        };
        spec.legacyResult = [](const json& args, const json&) { return args["node"]; };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "clear_selection";
        spec.help = "Clear the selection.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            selection.type = ux3d::imgui::SelectionType::None;
            selection.selectedNode = nullptr;
            ++selection.revision;
            selection.returnMaterial = nullptr;
            sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
            return CommandReply::success({{"selection", "none"}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "isolate";
        spec.help = "Isolate a node (Object > Isolate): only it, optionally with its children, is drawn. Selects it.";
        spec.args = objectSchema({{"node", prop("string", "Node name or #index.")},
                                  {"with_children", withDefault(prop("boolean", "Also draw its descendants."), false)}},
                                 {"node"});
        spec.positional = {"node", "with_children"};
        spec.parseLegacy = [](const std::vector<std::string>& tokens, const std::string& line, std::string& error) -> std::optional<json> {
            if (tokens.size() < 2) {
                error = "missing arguments: " + line;
                return std::nullopt;
            }
            const bool children = tokens.size() > 2 && (tokens[2] == "children" || tokens[2] == "childs" ||
                                                         tokens[2] == "with_children" || tokens[2] == "1" || tokens[2] == "true");
            return json{{"node", tokens[1]}, {"with_children", children}};
        };
        spec.run = [this, resolveNode](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* node = resolveNode(args, "node", failure);
            if (failure) return *failure;
            if (!gltfLayer) return CommandReply::failure("no_scene", "no render layer");
            const bool children = args.value("with_children", false);
            isolateNodeAction(node, children);
            selectNodeAction(node);
            return CommandReply::success({{"node", nodeRef(node)}, {"withChildren", children}});
        };
        spec.legacyResult = [](const json& args, const json&) { return args["node"]; };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "exit_isolate";
        spec.aliases = {"exitisolate"};
        spec.help = "Leave isolation; the whole scene is drawn again.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            if (!gltfLayer) return CommandReply::failure("no_scene", "exit isolate failed");
            gltfLayer->setIsolation(nullptr, false);
            sceneRevisions.mark(ux3d::imgui::SceneChange::Visibility);
            return CommandReply::success({{"isolation", nullptr}});
        };
        spec.legacyResult = [](const json&, const json&) { return json("none"); };
        agentRegistry.add(std::move(spec));
    }

    // ---- Camera ----
    {
        CommandSpec spec;
        spec.name = "frame_node";
        spec.aliases = {"frame"};
        spec.help = "Frame a node like F, optionally from a preset direction; selects it. Without node: frame the whole scene.";
        spec.args = objectSchema({{"node", prop("string", "Node name or #index (omit for the whole scene).")},
                                  {"view", viewProp()},
                                  {"margin", withDefault(prop("number", "Distance multiplier (2 = twice as far)."), 1.0)}});
        spec.positional = {"node", "view", "margin"};
        spec.parseLegacy = [](const std::vector<std::string>& tokens, const std::string& line, std::string& error) -> std::optional<json> {
            if (tokens.size() < 2) {
                error = "missing arguments: " + line;
                return std::nullopt;
            }
            json args = {{"node", tokens[1]}};
            // Phase 0 ignored an unknown view and an unparsable margin.
            if (tokens.size() > 2 && parseCameraOrientation(tokens[2]).has_value()) args["view"] = tokens[2];
            if (tokens.size() > 3) {
                try { args["margin"] = std::stof(tokens[3]); } catch (...) {}
            }
            return args;
        };
        spec.run = [this, resolveNode](const json& args, const CommandContext&) -> CommandOutcome {
            if (activeViewport == nullptr) return CommandReply::failure("no_viewport", "no active viewport");
            std::optional<ux3d::editor::CameraOrientation> view;
            if (args.contains("view") && !args["view"].is_null()) {
                view = parseCameraOrientation(args["view"].get<std::string>());
                if (!view) return CommandReply::failure("unknown_view", "unknown view: " + args["view"].get<std::string>());
            }
            const float margin = static_cast<float>(args.value("margin", 1.0));
            if (!args.contains("node") || args["node"].is_null() || args["node"].get<std::string>().empty()) {
                if (activeScene == nullptr) return CommandReply::failure("no_scene", "no scene loaded");
                if (view) activeViewport->cameraActor.orientUserCamera(*view);
                activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                activeViewport->antiAliasing.resetHistory();
                sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
                return CommandReply::success({{"node", nullptr}, {"camera", getCameraJson(activeViewport)}});
            }
            std::optional<CommandReply> failure;
            auto* node = resolveNode(args, "node", failure);
            if (failure) return *failure;
            selectNodeAction(node);
            sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
            frameNodeAction(node, view, margin);
            return CommandReply::success({{"node", nodeRef(node)}, {"camera", getCameraJson(activeViewport)}});
        };
        spec.legacyResult = [](const json&, const json& result) { return result["camera"]; };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_view";
        spec.aliases = {"view"};
        spec.help = "Orientation preset around the current target.";
        spec.args = objectSchema({{"view", viewProp()}}, {"view"});
        spec.positional = {"view"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            if (activeViewport == nullptr) return CommandReply::failure("no_viewport", "no active viewport");
            const std::string name = args["view"].get<std::string>();
            const auto view = parseCameraOrientation(name);
            if (!view) return CommandReply::failure("unknown_view", "unknown view: " + name);
            activeViewport->cameraActor.orientUserCamera(*view);
            activeViewport->antiAliasing.resetHistory();
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
            return CommandReply::success({{"camera", getCameraJson(activeViewport)}});
        };
        spec.legacyResult = [](const json& args, const json&) { return args["view"]; };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_camera";
        spec.help = "Exact camera: eye, target, up, fovY, near, far, projection and the CLI flags that reproduce it.";
        spec.args = objectSchema({{"viewport", viewportProp()}});
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            return CommandReply::success(getCameraJson(vp));
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_camera";
        spec.aliases = {"camera"};
        spec.help = "Exact camera (world units, glTF Y-up): eye and target, optional vertical FoV in degrees and clip planes.";
        spec.args = objectSchema({{"eye", prop("array", "[x, y, z]")},
                                  {"target", prop("array", "[x, y, z]")},
                                  {"fov_y", prop("number", "Vertical field of view in degrees.")},
                                  {"near", prop("number", "Near clipping plane.")},
                                  {"far", prop("number", "Far clipping plane.")}},
                                 {"eye", "target"});
        spec.positional = {"eye", "target", "fov_y"};
        spec.parseLegacy = [](const std::vector<std::string>& args, const std::string& line, std::string& error) -> std::optional<json> {
            // camera <ex,ey,ez> <tx,ty,tz> [fov] [near,far]   or   camera ex ey ez tx ty tz [fov] [near far]
            json out;
            bool parsed = false;
            if (args.size() >= 3 && args[1].find(',') != std::string::npos && args[2].find(',') != std::string::npos) {
                float ex = 0, ey = 0, ez = 0, tx = 0, ty = 0, tz = 0;
                char c1, c2, c3, c4;
                std::istringstream s1(args[1]);
                std::istringstream s2(args[2]);
                if ((s1 >> ex >> c1 >> ey >> c2 >> ez) && (s2 >> tx >> c3 >> ty >> c4 >> tz)) {
                    out["eye"] = {ex, ey, ez};
                    out["target"] = {tx, ty, tz};
                    if (args.size() > 3) {
                        try { out["fov_y"] = std::stof(args[3]); } catch (...) {}
                    }
                    if (args.size() > 4 && args[4].find(',') != std::string::npos) {
                        float zn = 0, zf = 0;
                        char c5;
                        std::istringstream s4(args[4]);
                        if (s4 >> zn >> c5 >> zf) {
                            out["near"] = zn;
                            out["far"] = zf;
                        }
                    }
                    parsed = true;
                }
            } else if (args.size() >= 7) {
                try {
                    out["eye"] = {std::stof(args[1]), std::stof(args[2]), std::stof(args[3])};
                    out["target"] = {std::stof(args[4]), std::stof(args[5]), std::stof(args[6])};
                    if (args.size() > 7) out["fov_y"] = std::stof(args[7]);
                    if (args.size() > 9) {
                        out["near"] = std::stof(args[8]);
                        out["far"] = std::stof(args[9]);
                    }
                    parsed = true;
                } catch (...) {}
            }
            if (!parsed) {
                error = "invalid camera arguments: " + line;
                return std::nullopt;
            }
            return out;
        };
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            if (activeViewport == nullptr) return CommandReply::failure("no_viewport", "no active viewport");
            glm::vec3 eye(0.0f), target(0.0f);
            if (!vec3Arg(args["eye"], eye) || !vec3Arg(args["target"], target)) {
                return CommandReply::failure("bad_arguments", "eye and target must be [x, y, z]");
            }
            activeViewport->cameraActor.setCameraEyeAndTarget(eye, target);
            if (auto* cam = activeViewport->cameraActor.getUserCamera()) {
                const bool hasFov = args.contains("fov_y") && !args["fov_y"].is_null();
                const bool hasNear = args.contains("near") && !args["near"].is_null();
                const bool hasFar = args.contains("far") && !args["far"].is_null();
                const float fovDegrees = hasFov ? static_cast<float>(args["fov_y"].get<double>()) : 0.0f;
                const float fovY = (hasFov && fovDegrees > 0.1f) ? glm::radians(fovDegrees) : cam->getYFoV();
                const float zn = hasNear ? static_cast<float>(args["near"].get<double>()) : cam->getZNearPlane();
                const float zf = hasFar ? static_cast<float>(args["far"].get<double>()) : cam->getZFarPlane();
                if (hasFov || hasNear || hasFar) cam->setPerspective(cam->getAspectRatio(), fovY, zn, zf);
            }
            activeViewport->antiAliasing.resetHistory();
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
            return CommandReply::success(getCameraJson(activeViewport));
        };
        agentRegistry.add(std::move(spec));
    }

    // ---- Inspection and capture ----
    {
        CommandSpec spec;
        spec.name = "capture";
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 120.0;
        spec.help = "Render settle_frames frames, then save the viewport image (PNG) to path or a session temp folder; "
                    "encoding png-base64 also returns the bytes.";
        json encoding = withDefault(prop("string", "path: return the file path; png-base64: also return the PNG bytes."), "path");
        encoding["enum"] = {"path", "png-base64"};
        spec.args = objectSchema({{"path", prop("string", "Output .png (default: %TEMP%/glider-agent/<pid>/capture_<n>.png).")},
                                  {"settle_frames", withDefault(prop("integer", "Frames rendered before reading (like --dump-frame)."), 8)},
                                  {"encoding", encoding},
                                  {"overlays", withDefault(prop("boolean", "false hides grid, scene box and selection box for this capture."), true)},
                                  {"viewport", viewportProp()}});
        spec.positional = {"path", "settle_frames"};
        spec.run = [this, resolveViewport, windowMinimized](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            if (windowMinimized()) return CommandReply::failure("window_minimized", "the window is minimised; restore it to capture");
            struct CaptureJob {
                uint32_t viewport = 0;
                std::string path;
                int settle = 8;
                int elapsed = 0;
                bool base64 = false;
                bool restoreOverlays = false;
                bool grid = false, sceneBox = false, selectionBox = false;
            };
            auto job = std::make_shared<CaptureJob>();
            job->viewport = vp->id;
            job->settle = std::max<int>(1, static_cast<int>(args.value("settle_frames", 8LL)));
            job->base64 = args.value("encoding", std::string("path")) == "png-base64";
            if (args.contains("path") && !args["path"].is_null() && !args["path"].get<std::string>().empty()) {
                job->path = args["path"].get<std::string>();
            } else {
                job->path = pathText(agentTempDirectory() / ("capture_" + std::to_string(++agentCaptureSerial) + ".png"));
            }
            if (!args.value("overlays", true)) {
                job->restoreOverlays = true;
                job->grid = vp->settings.showGrid;
                job->sceneBox = vp->settings.showSceneBoundingBox;
                job->selectionBox = vp->settings.showSelectedBoundingBox;
                vp->settings.showGrid = vp->settings.showSceneBoundingBox = vp->settings.showSelectedBoundingBox = false;
                sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
            }
            return agent::JobStep([this, job, windowMinimized]() -> std::optional<CommandReply> {
                if (++job->elapsed < job->settle) return std::nullopt;
                auto* target = findViewportById(job->viewport);
                if (target == nullptr) return CommandReply::failure("viewport_not_found", "the viewport was closed");
                auto restore = [&] {
                    if (!job->restoreOverlays) return;
                    target->settings.showGrid = job->grid;
                    target->settings.showSceneBoundingBox = job->sceneBox;
                    target->settings.showSelectedBoundingBox = job->selectionBox;
                    sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
                };
                if (windowMinimized()) {
                    restore();
                    return CommandReply::failure("window_minimized", "the window is minimised; restore it to capture");
                }
                GLuint texture = 0;
                int width = 0, height = 0;
                viewportImageSource(target, texture, width, height);
                ux3d::imgui::profiling::CaptureRecord record;
                const bool ok = captureViewportTexture(texture, width, height, job->path, "viewport-texture", frameCount,
                                                       false, 0.0, record);
                restore();
                if (!ok) return CommandReply::failure("capture_failed", "capture failed: " + job->path);
                json out;
                out["path"] = job->path;
                out["width"] = width;
                out["height"] = height;
                out["viewport"] = target->id;
                out["frame"] = frameCount;
                out["rgbaHash"] = hex64(record.rgbaHash);
                out["camera"] = getCameraJson(target);
                if (job->base64) {
                    std::ifstream file(std::filesystem::u8path(job->path), std::ios::binary);
                    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    out["encoding"] = "png-base64";
                    out["mimeType"] = "image/png";
                    out["data"] = agent::base64Encode(bytes);
                } else {
                    out["encoding"] = "path";
                }
                return CommandReply::success(out);
            });
        };
        spec.legacyResult = [](const json&, const json& result) {
            return json{{"path", result["path"]}, {"camera", result["camera"]}};
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "save_viewport_image";
        spec.help = "Save the viewport's current image (the same pixels as capture and the Viewport menu) as a PNG. "
                    "Without path: the capture folder (File > Settings > Captures, default <Pictures>/Glider) with a unique "
                    "Glider_<model>_<yyyyMMdd-HHmmss>.png name. Never opens a dialog. Returns the path.";
        spec.args = objectSchema({{"path", prop("string", "Output .png (default: unique name in the capture folder).")},
                                  {"viewport", viewportProp()}});
        spec.positional = {"path"};
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            std::string path;
            if (args.contains("path") && !args["path"].is_null()) path = args["path"].get<std::string>();
            ViewportImageResult result;
            std::string error;
            if (!saveViewportImage(vp, path, result, error)) return CommandReply::failure("capture_failed", error);
            json out;
            out["path"] = result.path;
            out["width"] = result.width;
            out["height"] = result.height;
            out["viewport"] = result.viewport;
            out["frame"] = frameCount;
            out["rgbaHash"] = hex64(result.hash);
            return CommandReply::success(out);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "copy_viewport_image";
        spec.help = "Copy the viewport's current image to the Windows clipboard as a bitmap (CF_DIB) and as PNG. "
                    "The same pixels as capture and the Viewport menu; never opens a dialog.";
        spec.args = objectSchema({{"viewport", viewportProp()}});
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            ViewportImageResult result;
            std::string error;
            if (!copyViewportImage(vp, result, error)) return CommandReply::failure("copy_failed", error);
            json out;
            out["width"] = result.width;
            out["height"] = result.height;
            out["viewport"] = result.viewport;
            out["frame"] = frameCount;
            out["rgbaHash"] = hex64(result.hash);
            out["formats"] = {"CF_DIB", "PNG"};
            return CommandReply::success(out);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "node_screen_rect";
        spec.aliases = {"noderect"};
        spec.help = "Projected OBB rectangle [x0, y0, x1, y1] in capture pixels with onScreen/clipped/behindCamera.";
        spec.args = objectSchema({{"node", prop("string", "Node name or #index.")}, {"viewport", viewportProp()}}, {"node"});
        spec.positional = {"node"};
        spec.run = [this, resolveNode, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* node = resolveNode(args, "node", failure);
            if (failure) return *failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            return CommandReply::success(computeNodeScreenRect(node, vp));
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "pick";
        spec.help = "Node, mesh, primitive and material at a pixel in capture-image coordinates (null: background).";
        spec.args = objectSchema({{"x", prop("integer", "Pixel column (0 = left).")},
                                  {"y", prop("integer", "Pixel row (0 = top).")},
                                  {"viewport", viewportProp()}},
                                 {"x", "y"});
        spec.positional = {"x", "y"};
        spec.parseLegacy = [](const std::vector<std::string>& tokens, const std::string& line, std::string& error) -> std::optional<json> {
            if (tokens.size() < 3) {
                error = "missing arguments: " + line;
                return std::nullopt;
            }
            try {
                return json{{"x", std::stoi(tokens[1])}, {"y", std::stoi(tokens[2])}};
            } catch (...) {
                error = "invalid coordinates: " + line;
                return std::nullopt;
            }
        };
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            GLint previousDraw = 0;
            GLint previousViewport[4] = {0, 0, 0, 0};
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
            glGetIntegerv(GL_VIEWPORT, previousViewport);
            json result = performPickAtPixel(static_cast<int>(args["x"].get<long long>()), static_cast<int>(args["y"].get<long long>()), vp);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDraw));
            glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
            return CommandReply::success(result);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "probe";
        spec.help = "At a pixel (capture-image coordinates): linear HDR scene colour before tone mapping, final LDR colour, "
                    "depth, world position and the picked node/mesh/primitive/material. 1x1 reads; no rendering change.";
        spec.args = objectSchema({{"x", prop("integer", "Pixel column (0 = left).")},
                                  {"y", prop("integer", "Pixel row (0 = top).")},
                                  {"viewport", viewportProp()}},
                                 {"x", "y"});
        spec.positional = {"x", "y"};
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            const int x = static_cast<int>(args["x"].get<long long>());
            const int y = static_cast<int>(args["y"].get<long long>());
            if (!vp->sceneFbo.isValid() || x < 0 || y < 0 || x >= vp->sceneFbo.getWidth() || y >= vp->sceneFbo.getHeight()) {
                return CommandReply::failure("out_of_bounds", "pixel outside the viewport image",
                                             {{"width", vp->sceneFbo.getWidth()}, {"height", vp->sceneFbo.getHeight()}});
            }
            return CommandReply::success(probePixel(vp, x, y, true));
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "probe_region";
        spec.help = "Min/max/mean linear HDR luminance and RGB over the rectangle [x0, x1) x [y0, y1) (capture-image coordinates).";
        spec.args = objectSchema({{"x0", prop("integer", "Left.")}, {"y0", prop("integer", "Top.")},
                                  {"x1", prop("integer", "Right (exclusive).")}, {"y1", prop("integer", "Bottom (exclusive).")},
                                  {"viewport", viewportProp()}},
                                 {"x0", "y0", "x1", "y1"});
        spec.positional = {"x0", "y0", "x1", "y1"};
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            if (!vp->sceneFbo.isValid()) return CommandReply::failure("no_image", "the viewport has not rendered yet");
            json out = probeRegion(vp, static_cast<int>(args["x0"].get<long long>()), static_cast<int>(args["y0"].get<long long>()),
                                   static_cast<int>(args["x1"].get<long long>()), static_cast<int>(args["y1"].get<long long>()));
            if (out.is_null()) return CommandReply::failure("out_of_bounds", "empty rectangle inside the viewport image");
            return CommandReply::success(out);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_render_mode";
        spec.aliases = {"debugview"};
        spec.help = "Shaded, wireframe or a debug render mode (same names as --debug-view).";
        spec.args = objectSchema({{"mode", prop("string", "e.g. shaded, wireframe, roughness, shadingnormal, ssr.")},
                                  {"viewport", viewportProp()}},
                                 {"mode"});
        spec.positional = {"mode"};
        spec.run = [this, resolveViewport, markViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            const std::string mode = args["mode"].get<std::string>();
            std::string error;
            if (!getRenderModeFromName(mode).has_value() ||
                !applySettingValue(SettingScope::Viewport, "render_mode", mode, &vp->settings, nullptr, error)) {
                return CommandReply::failure("unknown_render_mode", "unknown debug view: " + mode);
            }
            markViewport(vp);
            return CommandReply::success({{"renderMode", renderModeName(vp->settings.renderMode)}, {"viewport", vp->id}});
        };
        spec.legacyResult = [](const json& args, const json&) { return args["mode"]; };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "open_model";
        spec.aliases = {"open"};
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 600.0;
        spec.help = "Open a glTF/GLB (File > Open); replies when the first model frames have rendered. Environment settings reset with the model.";
        spec.args = objectSchema({{"path", prop("string", "Model path.")},
                                  {"without_textures", withDefault(prop("boolean", "Skip image reads (File > Open without texture)."), false)}},
                                 {"path"});
        spec.positional = {"path"};
        spec.restIsText = true;
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            const std::string path = args["path"].get<std::string>();
            std::error_code error;
            if (!std::filesystem::is_regular_file(std::filesystem::u8path(path), error)) {
                return CommandReply::failure("file_not_found", "failed to open: " + path);
            }
            // Inside a frame the load is queued; the next frame runs it (as File > Open does).
            loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(),
                          animationPlayer, sceneRevisions, sceneEnvironments, false, args.value("without_textures", false));
            queuedLoadNoPersist = agentNoPersistScope;
            struct OpenJob {
                int frames = 0;
                bool loaded = false;
                ux3d::imgui::profiling::Clock::time_point start = ux3d::imgui::profiling::Clock::now();
                double loadMs = 0.0;
            };
            auto job = std::make_shared<OpenJob>();
            return agent::JobStep([this, path, job]() -> std::optional<CommandReply> {
                if (!queuedLoadPath.empty() || loadingActive) return std::nullopt;
                if (!job->loaded) {
                    job->loaded = true;
                    job->loadMs = ux3d::imgui::profiling::elapsedMs(job->start);
                    if (!modelLoaded) return CommandReply::failure("load_failed", "failed to open: " + path);
                }
                if (++job->frames < kSettleAfterLoadFrames) return std::nullopt;
                return CommandReply::success({{"path", path}, {"loaded", true}, {"loadMs", job->loadMs}, {"state", agentStateJson()}});
            });
        };
        spec.legacyResult = [this](const json&, const json& result) {
            return json{{"path", result["path"]}, {"loaded", true}, {"state", legacyStateJson()}};
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "reset";
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 600.0;
        spec.help = "Known state: reload the model (or the default sample), default camera, no selection or isolation, "
                    "viewport settings and environment as at startup, fresh TAA/exposure history.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            const std::string path = currentFilePath.empty() ? resolveSampleModelPath() : currentFilePath;
            // The reload is queued for the next frame; the reset state is applied after it.
            loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(),
                          animationPlayer, sceneRevisions, sceneEnvironments);
            queuedLoadNoPersist = agentNoPersistScope;
            auto frames = std::make_shared<int>(-1);
            return agent::JobStep([this, path, frames]() -> std::optional<CommandReply> {
                if (!queuedLoadPath.empty() || loadingActive) return std::nullopt;
                if (*frames < 0) {
                    if (!modelLoaded) return CommandReply::failure("load_failed", "failed to reload: " + path);
                    for (auto& env : sceneEnvironments) env = agentStartupEnvironment;
                    for (const auto& vp : viewports) {
                        if (!vp) continue;
                        vp->settings = agentStartupViewportSettings;
                        if (activeScene != nullptr && !vp->isAttachedToCamera) vp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                        vp->antiAliasing.resetHistory();
                    }
                    gltfLayer->setIsolation(nullptr, false);
                    selection.type = ux3d::imgui::SelectionType::None;
                    selection.selectedNode = nullptr;
                    ++selection.revision;
                    sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
                    sceneRevisions.mark(ux3d::imgui::SceneChange::RenderSettings);
                    sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
                    sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
                    sceneRevisions.mark(ux3d::imgui::SceneChange::Visibility);
                    *frames = 0;
                }
                if (++*frames < kSettleAfterLoadFrames) return std::nullopt;
                return CommandReply::success(agentStateJson());
            });
        };
        agentRegistry.add(std::move(spec));
    }

    // ---- Settings (Phase 3) ----
    auto settingResult = [](const SettingSpec& spec, void* target) {
        return json{{"key", spec.key}, {"value", spec.get(target)}, {"default", spec.defaultValue}};
    };
    auto setSetting = [this, settingResult](SettingScope scope, const json& args, const CommandContext& context,
                                            ux3d::imgui::ViewportInstance* vp) -> CommandOutcome {
        const std::string key = args["key"].get<std::string>();
        const SettingSpec* spec = agentRegistry.findSetting(scope, key);
        if (spec == nullptr && scope == SettingScope::Viewport) {
            spec = agentRegistry.findSetting(SettingScope::Renderer, key);
            if (spec != nullptr) scope = SettingScope::Renderer;
        }
        if (spec == nullptr) {
            return CommandReply::failure("unknown_setting", "unknown setting: " + key + " (list_settings shows them all)");
        }
        if (spec->persisted && context.source == agent::CommandSource::Pipe && !context.persist) {
            return CommandReply::failure("persist_required", spec->key + " is a saved preference; call set_persist(true) first");
        }
        std::string error;
        auto* environment = &getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
        if (!applySettingValue(scope, spec->key, args["value"], vp ? &vp->settings : nullptr, environment, error)) {
            return CommandReply::failure("bad_value", error);
        }
        void* target = scope == SettingScope::Viewport ? static_cast<void*>(&vp->settings)
                       : scope == SettingScope::Environment ? static_cast<void*>(environment)
                                                            : nullptr;
        if (scope == SettingScope::Viewport) {
            vp->antiAliasing.resetHistory();
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
        } else {
            sceneRevisions.mark(ux3d::imgui::SceneChange::RenderSettings);
            for (const auto& other : viewports) {
                if (other) other->antiAliasing.resetHistory();
            }
        }
        if (spec->persisted && context.source == agent::CommandSource::Pipe && context.persist) {
            ux3d::imgui::RenderQuality::save();
        }
        json out = settingResult(*spec, target);
        out["scope"] = agent::CommandRegistry::scopeName(scope);
        if (scope == SettingScope::Viewport) out["viewport"] = vp->id;
        return CommandReply::success(out);
    };
    auto getSettings = [this, settingResult](SettingScope scope, const json& args, ux3d::imgui::ViewportInstance* vp) -> CommandOutcome {
        auto* environment = &getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
        auto targetFor = [&](SettingScope s) -> void* {
            return s == SettingScope::Viewport ? static_cast<void*>(&vp->settings)
                   : s == SettingScope::Environment ? static_cast<void*>(environment) : nullptr;
        };
        if (args.contains("key") && !args["key"].is_null()) {
            const std::string key = args["key"].get<std::string>();
            const SettingSpec* spec = agentRegistry.findSetting(scope, key);
            SettingScope found = scope;
            if (spec == nullptr && scope == SettingScope::Viewport) {
                spec = agentRegistry.findSetting(SettingScope::Renderer, key);
                found = SettingScope::Renderer;
            }
            if (spec == nullptr) return CommandReply::failure("unknown_setting", "unknown setting: " + key);
            return CommandReply::success(settingResult(*spec, targetFor(found)));
        }
        json all = json::object();
        for (const SettingSpec& spec : agentRegistry.settings()) {
            if (spec.scope == scope || (scope == SettingScope::Viewport && spec.scope == SettingScope::Renderer)) {
                all[spec.key] = spec.get(targetFor(spec.scope));
            }
        }
        return CommandReply::success({{"values", all}});
    };
    {
        CommandSpec spec;
        spec.name = "set_viewport_setting";
        spec.help = "Set a viewport (or renderer-wide) setting: AA, SSR, grid, render mode, wireframe, ASOBO switches... "
                    "Same code path as the matching CLI flag.";
        spec.args = objectSchema({{"key", prop("string", "Setting key (list_settings).")},
                                  {"value", prop("", "New value: bool, number, enum name or [r, g, b(, a)].")},
                                  {"viewport", viewportProp()}},
                                 {"key", "value"});
        spec.positional = {"key", "value"};
        spec.run = [resolveViewport, setSetting](const json& args, const CommandContext& context) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            return setSetting(SettingScope::Viewport, args, context, vp);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_viewport_setting";
        spec.help = "One viewport/renderer setting (key) or all of them.";
        spec.args = objectSchema({{"key", prop("string", "Setting key (omit for all).")}, {"viewport", viewportProp()}});
        spec.positional = {"key"};
        spec.run = [resolveViewport, getSettings](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            return getSettings(SettingScope::Viewport, args, vp);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_environment";
        spec.help = "Set a scene environment setting: exposure, tone mapping, HDR rotation, time of day, emissive scale, bloom...";
        spec.args = objectSchema({{"key", prop("string", "Setting key (list_settings).")},
                                  {"value", prop("", "New value.")}},
                                 {"key", "value"});
        spec.positional = {"key", "value"};
        spec.run = [this, setSetting](const json& args, const CommandContext& context) -> CommandOutcome {
            return setSetting(SettingScope::Environment, args, context, activeViewport);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_environment";
        spec.help = "One environment setting (key) or all of them.";
        spec.args = objectSchema({{"key", prop("string", "Setting key (omit for all).")}});
        spec.positional = {"key"};
        spec.run = [this, getSettings](const json& args, const CommandContext&) -> CommandOutcome {
            return getSettings(SettingScope::Environment, args, activeViewport);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "list_settings";
        spec.help = "Every setting key with scope, type, range or values, current value, default and CLI flag.";
        spec.args = objectSchema({{"scope", prop("string", "viewport, environment or renderer (default: all).")},
                                  {"viewport", viewportProp()}});
        spec.positional = {"scope"};
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            const std::string scope = lowerText(args.value("scope", std::string()));
            auto* environment = &getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
            json list = json::array();
            for (const SettingSpec& spec : agentRegistry.settings()) {
                const std::string name = agent::CommandRegistry::scopeName(spec.scope);
                if (!scope.empty() && scope != name) continue;
                json entry = {{"key", spec.key}, {"scope", name}, {"type", agent::CommandRegistry::typeName(spec.type)},
                              {"default", spec.defaultValue}, {"help", spec.help}, {"persisted", spec.persisted}};
                if (!spec.aliases.empty()) entry["aliases"] = spec.aliases;
                if (!spec.cliFlag.empty()) entry["cli"] = spec.cliFlag;
                if (spec.type == SettingType::Enum) entry["values"] = spec.enumValues;
                if (spec.hasRange) entry["range"] = {spec.minimum, spec.maximum};
                void* target = spec.scope == SettingScope::Viewport ? (vp ? static_cast<void*>(&vp->settings) : nullptr)
                               : spec.scope == SettingScope::Environment ? static_cast<void*>(environment) : nullptr;
                entry["value"] = (spec.scope == SettingScope::Viewport && vp == nullptr) ? json(nullptr) : spec.get(target);
                list.push_back(std::move(entry));
            }
            return CommandReply::success({{"settings", list}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_material_param";
        spec.help = "Edit an existing material extension parameter in memory (Inspector / --set-asobo-param path). "
                    "Extensions are never added or removed.";
        spec.args = objectSchema({{"material", prop("string", "Material name or index.")},
                                  {"extension", prop("string", "Extension present on the material, e.g. ASOBO_material_fresnel_fade.")},
                                  {"param", prop("string", "Parameter name.")},
                                  {"value", prop("", "New value (number, bool, string or array).")}},
                                 {"material", "extension", "param", "value"});
        spec.positional = {"material", "extension", "param", "value"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            std::string error;
            if (!setMaterialParamAction(args["material"].get<std::string>(), args["extension"].get<std::string>(),
                                        args["param"].get<std::string>(), args["value"], error)) {
                return CommandReply::failure("material_param_failed", error);
            }
            return agentRegistry.find("get_material_param")->run(args, CommandContext{});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_material_param";
        spec.help = "Values of a material extension (typed ASOBO values with defaults, or the vendor JSON).";
        spec.args = objectSchema({{"material", prop("string", "Material name or index.")},
                                  {"extension", prop("string", "Extension name.")},
                                  {"param", prop("string", "Parameter (omit for all).")},
                                  {"value", prop("", "Ignored (set_material_param passes it through).")}},
                                 {"material", "extension"});
        spec.positional = {"material", "extension", "param"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            const std::string materialSpec = args["material"].get<std::string>();
            const std::string extension = args["extension"].get<std::string>();
            const std::string param = args.contains("param") && !args["param"].is_null() ? args["param"].get<std::string>() : std::string();
            ux3d::gltf::Material* material = findMaterial(gltfModel, materialSpec);
            if (material == nullptr) return CommandReply::failure("material_not_found", "material not found: " + materialSpec);
            json values = json::object();
            json explicitKeys = json::array();
            bool found = false;
            if (ux3d::gltf::asobo::isAsoboExtension(extension)) {
                const auto set = ux3d::gltf::asobo::parseExtensions(gltfModel, *material, ux3d::gltf::asobo::Scope::Material,
                                                                    "/materials/" + std::to_string(material->m_ID));
                for (const auto& data : set.all()) {
                    if (std::string(data.name()) != extension || data.spec() == nullptr) continue;
                    found = true;
                    for (const auto& property : data.spec()->properties) {
                        const std::string key(property.name);
                        if (const auto* value = data.value(key)) values[key] = asoboValueToJson(*value);
                        if (data.isExplicit(key)) explicitKeys.push_back(key);
                    }
                }
            }
            if (!found) {
                for (const auto& [name, text] : material->getUnknownExtensions()) {
                    if (std::string(name.c_str()) != extension) continue;
                    found = true;
                    try { values = json::parse(text.c_str()); } catch (...) { values = json::object(); }
                    for (auto it = values.begin(); it != values.end(); ++it) explicitKeys.push_back(it.key());
                }
            }
            if (!found) return CommandReply::failure("extension_not_found", "material '" + materialSpec + "' has no " + extension);
            json out = {{"material", materialName(material)}, {"extension", extension}};
            if (!param.empty()) {
                if (!values.is_object() || !values.contains(param)) {
                    return CommandReply::failure("param_not_found", extension + " has no parameter " + param);
                }
                out["param"] = param;
                out["value"] = values[param];
                out["explicit"] = std::find(explicitKeys.begin(), explicitKeys.end(), json(param)) != explicitKeys.end();
            } else {
                out["values"] = values;
                out["explicit"] = explicitKeys;
            }
            return CommandReply::success(out);
        };
        agentRegistry.add(std::move(spec));
    }
}

// ---------------------------------------------------------------------------------------------
// Dispatch: scripts, pipe and the frame pump
// ---------------------------------------------------------------------------------------------

bool Application::runScriptCommand(const std::string& cmd, const std::vector<std::string>& tokens, const std::string& line,
                                   std::function<void(bool, const std::string&, const json&)> done)
{
    registerAgentCommands();
    const agent::CommandSpec* spec = agentRegistry.find(cmd);
    if (spec == nullptr) return false;
    const bool legacy = cmd != spec->name;
    std::string error;
    std::optional<json> args = legacy && spec->parseLegacy ? spec->parseLegacy(tokens, line, error)
                                                           : agent::CommandRegistry::parseText(*spec, tokens, line, error);
    if (!args) {
        done(false, error, nullptr);
        return true;
    }
    agent::CommandContext context;
    context.source = agent::CommandSource::Script;
    ++agentCommandsRun;
    const json argsCopy = *args;
    agentRegistry.dispatch(*spec, *args, context, [spec, legacy, argsCopy, done](const agent::CommandReply& reply) {
        if (!reply.ok) {
            done(false, reply.error.message, nullptr);
            return;
        }
        done(true, "", legacy && spec->legacyResult ? spec->legacyResult(argsCopy, reply.result) : reply.result);
    });
    return true;
}

void Application::runPipeRequest(const ux3d::imgui::ControlPipe::Request& request)
{
    const uint64_t session = request.session;
    if (session != agentPipeSession) {
        // A new client starts without persistence.
        agentPipeSession = session;
        agentPersist = false;
    }
    json id = nullptr;
    auto send = [this, session](json id, const agent::CommandReply& reply) {
        json out = {{"id", std::move(id)}, {"ok", reply.ok}};
        if (reply.ok) {
            out["result"] = reply.result;
        } else {
            out["error"] = {{"code", reply.error.code}, {"message", reply.error.message}};
            for (auto it = reply.error.details.begin(); it != reply.error.details.end(); ++it) out["error"][it.key()] = it.value();
        }
        if (controlPipe) controlPipe->reply(session, out.dump(-1, ' ', false, json::error_handler_t::replace));
    };
    json message;
    try {
        message = json::parse(request.line);
    } catch (const std::exception& e) {
        send(nullptr, agent::CommandReply::failure("bad_request", std::string("not JSON: ") + e.what()));
        return;
    }
    if (!message.is_object()) {
        send(nullptr, agent::CommandReply::failure("bad_request", "a request is a JSON object {\"id\", \"cmd\", \"args\"}"));
        return;
    }
    id = message.contains("id") ? message["id"] : json(nullptr);
    const std::string cmd = message.value("cmd", std::string());
    const agent::CommandSpec* spec = agentRegistry.find(cmd);
    if (spec == nullptr) {
        send(id, agent::CommandReply::failure("unknown_command", "unknown command: " + cmd + " (describe lists them)"));
        return;
    }
    agent::CommandContext context;
    context.source = agent::CommandSource::Pipe;
    context.persist = agentPersist;
    ++agentCommandsRun;
    // Pipe commands never write settings.ini (recent files included) unless set_persist(true).
    agentNoPersistScope = !agentPersist;
    agentRegistry.dispatch(*spec, message.contains("args") ? message["args"] : json::object(), context,
                           [send, id](const agent::CommandReply& reply) { send(id, reply); });
    agentNoPersistScope = false;
}

bool Application::agentWorkPending() const
{
    return agentRegistry.busy() || !agentPipeBacklog.empty();
}

void Application::pumpAgentCommands()
{
    if (!agentRegistryReady) return;
    // A pipe job keeps its session's no-persist scope while it runs.
    agentNoPersistScope = controlPipe != nullptr && !agentPersist && agentRegistry.busy();
    agentRegistry.tick();
    agentNoPersistScope = false;
    if (controlPipe == nullptr) return;
    for (auto& request : controlPipe->takeRequests()) agentPipeBacklog.push_back(std::move(request));
    // Commands run strictly in order: a multi-frame job holds back the requests behind it.
    while (!agentPipeBacklog.empty() && !agentRegistry.busy()) {
        const ux3d::imgui::ControlPipe::Request request = std::move(agentPipeBacklog.front());
        agentPipeBacklog.pop_front();
        runPipeRequest(request);
    }
}

bool Application::startControlPipe()
{
    registerAgentCommands();
    if (controlPipe && controlPipe->running()) return true;
    controlPipe = std::make_unique<ux3d::imgui::ControlPipe>();
    std::string error;
    if (!controlPipe->start([] { glfwPostEmptyEvent(); }, error)) {
        controlPipeError = error;
        logMessage("Agent control pipe failed: " + error, ImVec4(1.0f, 0.4f, 0.4f, 1.0f), ux3d::core::LogLevel::Error);
        std::fprintf(stderr, "Agent control pipe failed: %s\n", error.c_str());
        controlPipe.reset();
        return false;
    }
    controlPipeError.clear();
    logMessage("Agent control pipe: " + controlPipe->name());
    std::printf("Agent control pipe: %s\n", controlPipe->name().c_str());
    std::fflush(stdout);
    return true;
}

void Application::stopControlPipe()
{
    if (!controlPipe) return;
    controlPipe->stop();
    controlPipe.reset();
    agentPipeBacklog.clear();
    agentPersist = false;
    logMessage("Agent control pipe closed");
}

void Application::drawAgentControlSettings()
{
    bool enabled = controlPipe != nullptr && controlPipe->running();
    if (ImGui::Checkbox("Enable control pipe", &enabled)) {
        if (enabled) startControlPipe();
        else stopControlPipe();
    }
    ImGui::TextDisabled("Session only; not saved. --control-pipe enables it at startup.");
    ImGui::Separator();
    if (controlPipe != nullptr) {
        ImGui::Text("Pipe: %s", controlPipe->name().c_str());
        ImGui::Text("Client: %s", controlPipe->clientConnected() ? "connected" : "none");
        ImGui::Text("Clients accepted: %llu, refused: %llu", static_cast<unsigned long long>(controlPipe->clientsAccepted()),
                    static_cast<unsigned long long>(controlPipe->clientsRejected()));
        ImGui::Text("Agent changes saved to settings.ini: %s", agentPersist ? "yes (set_persist)" : "no");
    } else if (!controlPipeError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", controlPipeError.c_str());
    }
    ImGui::Text("Commands run: %llu", static_cast<unsigned long long>(agentCommandsRun));
    ImGui::Separator();
    ImGui::TextWrapped("Agents connect with scripts/glider_mcp.py (MCP) or scripts/glider_ctl.py. "
                       "Glider.exe --agent-guide <file> writes the guide and command reference.");
}

// ---------------------------------------------------------------------------------------------
// --agent-guide
// ---------------------------------------------------------------------------------------------

std::string Application::buildAgentGuide()
{
    registerAgentCommands();
    std::string text;
    const auto file = ux3d::imgui::EmbeddedVFS::instance().getFile("agent/GLIDER_AGENT_GUIDE.md");
    if (file.isValid()) {
        text.assign(file.asStringView());
    } else {
        text = "# Glider agent control\n\n(The embedded guide is missing from this build.)\n";
    }
    if (!text.empty() && text.back() != '\n') text += '\n';
    text += "\n" + agentRegistry.markdownReference();
    return text;
}

int Application::writeAgentGuide(const std::string& path, const std::string& format)
{
    std::string text = buildAgentGuide();
    std::filesystem::path target;
    if (format == "skill") {
        if (path.empty()) {
            std::fprintf(stderr, "--agent-guide --format skill needs a directory\n");
            return 1;
        }
        target = std::filesystem::u8path(path) / "glider-control" / "SKILL.md";
    } else if (!path.empty()) {
        target = std::filesystem::u8path(path);
    }
    if (!target.empty()) {
        std::error_code error;
        if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), error);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        out << text;
        if (!out) {
            std::fprintf(stderr, "Cannot write the agent guide to %s\n", pathText(target).c_str());
            return 1;
        }
        return 0;
    }
#ifdef _WIN32
    // Win32 GUI subsystem: use redirected stdout when there is one, else the parent console.
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    const DWORD type = (handle != nullptr && handle != INVALID_HANDLE_VALUE) ? GetFileType(handle) : FILE_TYPE_UNKNOWN;
    if (type == FILE_TYPE_UNKNOWN && AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
    }
#endif
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
    return 0;
}

} // namespace ux3d::imgui::application
