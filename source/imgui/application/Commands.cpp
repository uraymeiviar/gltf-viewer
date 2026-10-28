// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Agent control channel (docs/AGENT_CONTROL_PLAN.md): the command and setting registry shared
// by the CLI, --ui-script / --ui-script-follow and the control pipe, the pipe pump, the readback
// probes and --agent-guide.

#include "Application.h"
#include "imgui/CaptureSettings.h"
#include "imgui/EmbeddedVFS.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/GltfValidator.h"
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
    return st;
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
    addSetting(floatSetting<VS>("decal_tolerance_mm", V, &VS::decalDepthToleranceMm,
                                              "Decal depth tolerance along the receiver normal (mm).",
                                              "--decal-tolerance <mm>", std::pair{0.0, 10.0}));
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
        SettingSpec spec = indexEnumSetting<ES>("tone_mapping", E, &ES::toneMappingIdx, {"neutral", "hill", "narkowicz"},
                                                "Tone mapping operator (Khronos PBR Neutral, ACES Hill, ACES Narkowicz).",
                                                "--tone-mapping <neutral|hill|narkowicz>");
        spec.enumAliases = {{"pbr_neutral", "neutral"}, {"aces_hill", "hill"}, {"aces_narkowicz", "narkowicz"}};
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("hdr_rotation", E, &ES::hdrRotation, "Environment rotation in degrees.", "--hdr-rotation <deg>"));
    addSetting(floatSetting<ES>("hdr_intensity", E, &ES::hdrIntensity, "Environment (IBL) intensity."));
    addSetting(floatSetting<ES>("hdr_blur", E, &ES::hdrBlur, "Background blur.", {}, std::pair{0.0, 1.0}));
    addSetting(indexEnumSetting<ES>("time_of_day", E, &ES::timeOfDay, {"day", "night"},
                                                  "ASOBO_material_emissive day/night multiplier.", "--time-of-day <day|night>"));
    addSetting(floatSetting<ES>("msfs_emissive_scale", E, &ES::msfsEmissiveScale,
                                              "MSFS emissive scale for ASOBO assets (default 1/1000).",
                                              "--msfs-emissive-scale <x>"));
    addSetting(boolSetting<ES>("bloom", E, &ES::bloomEnabled, "Physically based bloom.", "--bloom <on|off>"));
    addSetting(floatSetting<ES>("bloom_intensity", E, &ES::bloomIntensity, "Bloom intensity.", "--bloom-intensity <x>"));
    addSetting(floatSetting<ES>("bloom_radius", E, &ES::bloomRadius, "Bloom radius.", "--bloom-radius <x>"));
    addSetting(boolSetting<ES>("auto_exposure", E, &ES::autoExposure, "Auto exposure (metered).", "--auto-exposure <on|off>"));
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

    // --- Renderer-wide ---
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
