// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/RenderQuality.h"
#include "imgui/AppSettings.h"
#include "imgui/SceneHierarchyPanel.h"
#include "imgui/ResourcesPanel.h"
#include "imgui/ValidationPanel.h"
#include "imgui/GltfValidator.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/SamplerTest.h"
#include "imgui/VaoTest.h"
#include "imgui/PolygonModeTest.h"
#include "imgui/CullWindingTest.h"
#include "imgui/DepthTest.h"
#include "imgui/BlendTest.h"
#include "imgui/Theme.h"
#include "imageformats/imageformats.h"
#include "gltf/GltfParsing.h"
#include "gltf/ImportOptions.h"
#include "gltf/Material.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"
#include "renderer/Skin.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <type_traits>
#include <variant>
#include <glm/gtc/matrix_transform.hpp>

namespace ux3d::imgui::application {

bool Application::parseWindowSize(const std::string& text, int& width, int& height)
{
    std::string normalized = text;
    for (char& c : normalized) {
        if (c == 'x' || c == 'X' || c == ',' || c == ';') c = ' ';
    }
    std::istringstream stream(normalized);
    int w = 0;
    int h = 0;
    if (!(stream >> w >> h)) return false;
    if (w < 320 || h < 240) return false;
    width = w;
    height = h;
    return true;
}

bool Application::parseFloat3(const std::string& text, float& x, float& y, float& z)
{
    std::string normalized = text;
    for (char& c : normalized) {
        if (c == ',' || c == ';') c = ' ';
    }
    std::istringstream stream(normalized);
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    if (!(stream >> px >> py >> pz)) return false;
    x = px;
    y = py;
    z = pz;
    return true;
}

std::string Application::toLowerAscii(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::optional<ux3d::editor::CameraOrientation> Application::parseCameraOrientation(const std::string& str)
{
    std::string s = toLowerAscii(str);
    if (s == "front") return ux3d::editor::CameraOrientation::Front;
    if (s == "back") return ux3d::editor::CameraOrientation::Back;
    if (s == "left") return ux3d::editor::CameraOrientation::Left;
    if (s == "right") return ux3d::editor::CameraOrientation::Right;
    if (s == "top") return ux3d::editor::CameraOrientation::Top;
    if (s == "bottom") return ux3d::editor::CameraOrientation::Bottom;
    if (s == "iso" || s == "isometric") return ux3d::editor::CameraOrientation::Isometric;
    return std::nullopt;
}

ux3d::gltf::Node* Application::findNode(const std::string& spec, std::vector<std::string>* outCandidates)
{
    if (spec.empty()) {
        return nullptr;
    }

    // 1. #index
    if (spec.front() == '#' && spec.size() > 1) {
        bool allDigits = true;
        for (size_t i = 1; i < spec.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(spec[i]))) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            try {
                uint32_t id = static_cast<uint32_t>(std::stoul(spec.substr(1)));
                for (auto& node : gltfModel.getNodes()) {
                    if (node.m_ID == id) {
                        return &node;
                    }
                }
            } catch (...) {}
        }
    }

    // 2. Exact match
    for (auto& node : gltfModel.getNodes()) {
        if (node.hasName() && node.getName() == spec) {
            return &node;
        }
    }

    // 3. Case-insensitive match
    std::string lowerSpec = toLowerAscii(spec);
    ux3d::gltf::Node* ciMatch = nullptr;
    size_t ciCount = 0;
    for (auto& node : gltfModel.getNodes()) {
        if (node.hasName() && toLowerAscii(node.getName()) == lowerSpec) {
            if (!ciMatch) ciMatch = &node;
            ++ciCount;
        }
    }
    if (ciCount >= 1) {
        return ciMatch;
    }

    // 4. Unique substring match
    ux3d::gltf::Node* subMatch = nullptr;
    size_t subCount = 0;
    std::vector<std::string> subCandidates;
    for (auto& node : gltfModel.getNodes()) {
        if (node.hasName()) {
            std::string nName = node.getName();
            if (toLowerAscii(nName).find(lowerSpec) != std::string::npos) {
                if (!subMatch) subMatch = &node;
                ++subCount;
                if (subCandidates.size() < 10) {
                    subCandidates.push_back(nName);
                }
            }
        }
    }
    if (subCount == 1) {
        return subMatch;
    }

    // 5. Failure: log up to 10 candidates
    std::vector<std::string> candidates;
    if (!subCandidates.empty()) {
        candidates = std::move(subCandidates);
    } else {
        for (const auto& node : gltfModel.getNodes()) {
            if (node.hasName()) {
                candidates.push_back(node.getName());
                if (candidates.size() >= 10) break;
            }
        }
    }

    std::string candStr;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (i > 0) candStr += ", ";
        candStr += candidates[i];
    }
    std::fprintf(stderr, "Node not found for '%s'. Candidates (%zu): %s\n",
                 spec.c_str(), candidates.size(), candStr.c_str());
    logMessage("Node not found for '" + spec + "'. Candidates: " + candStr,
               ImVec4(1.0f, 0.8f, 0.2f, 1.0f));

    if (outCandidates) {
        *outCandidates = candidates;
    }
    return nullptr;
}

namespace {
nlohmann::json asoboValueToJson(const ux3d::gltf::asobo::Value& value)
{
    return std::visit([](const auto& v) -> nlohmann::json {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return nullptr;
        } else if constexpr (std::is_same_v<T, ux3d::gltf::asobo::TextureRef>) {
            nlohmann::json texture = {{"index", v.index}, {"texCoord", v.texCoord}, {"scale", v.scale}};
            if (!v.extensions.is_null()) {
                texture["extensions"] = v.extensions;
            }
            return texture;
        } else {
            return nlohmann::json(v);
        }
    }, value);
}

// --asobo-report: the typed ASOBO extension model of the loaded file (A0.1), read by
// scripts/validate_asobo_model.py.
bool writeAsoboReport(const ux3d::gltf::Gltf& gltf, const std::string& path)
{
    namespace asobo = ux3d::gltf::asobo;
    nlohmann::json report;
    std::vector<std::string> declared(gltf.getDeclaredExtensions().begin(), gltf.getDeclaredExtensions().end());
    std::sort(declared.begin(), declared.end());
    report["declared"] = declared;
    report["specCount"] = asobo::extensionSpecs().size();
    report["support"] = nlohmann::json::object();
    for (const asobo::SupportInfo& info : asobo::supportTable()) {
        report["support"][std::string(info.name)] = asobo::supportLabel(info.status);
    }
    report["objects"] = nlohmann::json::array();
    auto add = [&](const ux3d::gltf::GltfObject& object, uint32_t scope, const std::string& pointer) {
        const asobo::ExtensionSet set = asobo::parseExtensions(gltf, object, scope, pointer);
        if (set.empty()) {
            return;
        }
        nlohmann::json extensions = nlohmann::json::array();
        for (const asobo::ExtensionData& data : set.all()) {
            nlohmann::json values = nlohmann::json::object();
            nlohmann::json explicitKeys = nlohmann::json::array();
            if (data.spec() != nullptr) {
                for (const asobo::PropertySpec& property : data.spec()->properties) {
                    const std::string key(property.name);
                    if (const asobo::Value* value = data.value(key)) {
                        values[key] = asoboValueToJson(*value);
                    }
                    if (data.isExplicit(key)) {
                        explicitKeys.push_back(key);
                    }
                }
            }
            extensions.push_back({{"name", std::string(data.name())}, {"hasSpec", data.spec() != nullptr},
                                  {"values", values}, {"explicit", explicitKeys}, {"unknown", data.unknownKeys()}});
        }
        report["objects"].push_back({{"pointer", pointer}, {"extensions", extensions}});
    };
    add(gltf, asobo::Scope::Root, "");
    add(gltf.getAsset(), asobo::Scope::Asset, "/asset");
    auto addAll = [&](const auto& elements, uint32_t scope, const char* collection) {
        size_t index = 0;
        for (const auto& element : elements) {
            add(element, scope, std::string("/") + collection + "/" + std::to_string(index++));
        }
    };
    addAll(gltf.getElements<ux3d::gltf::Material>(), asobo::Scope::Material, "materials");
    addAll(gltf.getElements<ux3d::gltf::Node>(), asobo::Scope::Node, "nodes");
    addAll(gltf.getElements<ux3d::gltf::Mesh>(), asobo::Scope::Mesh, "meshes");
    addAll(gltf.getElements<ux3d::gltf::Animation>(), asobo::Scope::Animation, "animations");

    report["diagnostics"] = nlohmann::json::array();
    for (const asobo::Diagnostic& diagnostic : asobo::validateExtensions(gltf)) {
        const bool warning = diagnostic.severity == asobo::Diagnostic::Severity::Warning;
        report["diagnostics"].push_back({{"severity", warning ? "warning" : "info"}, {"code", diagnostic.code},
                                         {"pointer", diagnostic.pointer}, {"message", diagnostic.message}});
    }
    std::ofstream out(path, std::ios::binary);
    out << report.dump(2) << "\n";
    std::printf("Wrote ASOBO extension report to %s\n", path.c_str());
    return static_cast<bool>(out);
}
}

std::optional<int> Application::configure(int argc, char** argv)
{
    auto& startupProfile = ux3d::imgui::profiling::startupProfile();
    modelToLoad = resolveSampleModelPath();
    std::string reloadModelArg;
    bool withoutTextures = false;
    std::string asoboReportPath;
    struct AsoboParamEditArg {
        std::string material;
        std::string extension;
        std::string param;
        std::string value;
    };
    std::vector<AsoboParamEditArg> setAsoboParamArgs;
    std::vector<std::string> searchPathArgs;
    std::string selectNodeArg;
    std::string isolateNodeArg;
    std::string hideNodeArg;
    std::string renameNodeArg;
    std::string renameMeshArg;
    std::string renameMaterialArg;
    bool hierarchyDisplayProvided = false;
    bool resourceDisplayProvided = false;
    ux3d::imgui::HierarchyDisplayOptions hierarchyDisplayValue;
    ux3d::imgui::ResourceDisplayOptions resourceDisplayValue;
    std::string selectMaterialArg;
    std::string selectImageArg;
    std::string selectTextureArg;
    uint32_t selectViewportArg = 0;
    std::string newViewportCamArg;
    bool showValidationArg = false;
    bool showSearchPathsArg = false;
    bool showStatisticsArg = false;
    bool showLoggerArg = false;
    bool loggerVariableEntriesArg = false;
    size_t logTestCountArg = 0;
    float animTimeArg = -1.0f;
    std::string conflictSelfCheckOutputPath;
    std::string validationCacheSelfCheckOutputPath;
    std::string cacheInvalidationSelfCheckOutputPath;
    std::string loggerLayoutSelfCheckOutputPath;
    std::string revisionSelfCheckOutputPath;
    std::string validateFileArg; // P6.1 test hook: validate a file without loading it, then exit
    std::string aaTargetSelfCheckOutputPath;
    std::string samplerSelfCheckOutputPath;
    std::string samplerSelfCheckCase = "all";
    std::string samplerDumpPatternPath;
    std::string vaoSelfCheckOutputPath;
    std::string vaoSelfCheckCase = "all";
    std::string polygonModeSelfCheckOutputPath;
    std::string polygonModeSelfCheckCase = "all";
    std::string cullWindingSelfCheckOutputPath;
    std::string cullWindingSelfCheckCase = "all";
    std::string depthSelfCheckOutputPath;
    std::string depthSelfCheckCase = "all";
    std::string blendSelfCheckOutputPath;
    std::string blendSelfCheckCase = "all";
    bool selfCheckForceFailure = false;
    bool cameraYawProvided = false;
    bool cameraPitchProvided = false;
    bool cameraDistanceProvided = false;
    bool cameraTargetProvided = false;
    bool cameraFovProvided = false;
    float cameraYawArg = 0.0f;
    float cameraPitchArg = 0.0f;
    float cameraDistanceArg = 0.0f;
    float cameraTargetX = 0.0f;
    float cameraTargetY = 0.0f;
    float cameraTargetZ = 0.0f;
    float cameraFovArg = 0.0f;
    std::string cameraProjectionArg;
    std::string debugViewArg;
    bool cliClearProvided = false;
    glm::vec4 cliClearColor(1.0f);
    bool cliHdrProvided = false;
    std::string cliHdrPath;
    bool cliHdrRotationProvided = false;
    float cliHdrRotation = 0.0f;
    bool cliExposureProvided = false;
    float cliExposureEV = 0.0f;
    bool cliToneMappingProvided = false;
    int cliToneMappingIdx = 1;
    bool cliBloomProvided = false;
    bool cliBloomEnabled = true;
    bool cliBloomIntensityProvided = false;
    float cliBloomIntensity = 0.04f;
    bool cliBloomRadiusProvided = false;
    float cliBloomRadius = 1.0f;
    bool cliAutoExposureProvided = false;
    bool cliAutoExposure = false;
    bool cliExposureCompensationProvided = false;
    float cliExposureCompensationEV = 0.0f;
    bool cliMsfsEmissiveScaleProvided = false;
    float cliMsfsEmissiveScale = 0.001f;
    bool cliTimeOfDayProvided = false;
    int cliTimeOfDay = 0;

    auto parseDisplayOptions = [](const std::string& spec, ux3d::imgui::HierarchyDisplayOptions& options) {
        std::stringstream stream(spec);
        std::string token;
        int part = 0;
        while (std::getline(stream, token, ',') && part < 4) {
            const bool enabled = token == "1" || toLowerAscii(token) == "true" || toLowerAscii(token) == "on";
            switch (part) {
            case 0:
                options.showIcon = enabled;
                break;
            case 1:
                options.showTitle = enabled;
                break;
            case 2:
                options.showIndex = enabled;
                break;
            case 3:
                options.showName = enabled;
                break;
            default:
                break;
            }
            ++part;
        }
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dump-frame" && i + 1 < argc) {
            dumpFramePath = argv[++i];
            dumpFrameCountdown = 8;
        } else if (arg == "--dump-next-frame" && i + 1 < argc) {
            dumpNextFramePath = argv[++i];
        } else if (arg == "--decal-prepass-check" && i + 1 < argc) {
            // A2.1 diagnostic: read the prepass depth/G0 and the forward depth/geometry
            // normal after the warm-up frames, write a JSON report, then exit. Redraw
            // skipping would otherwise leave the warm-up frame uncaptured.
            s_decalPrepassCheckPath = argv[++i];
            s_disableRedrawSkip = true;
        } else if (arg == "--capture-viewport" && i + 1 < argc) {
            s_captureViewportPath = argv[++i];
        } else if (arg == "--without-textures") {
            withoutTextures = true;
        } else if (arg == "--capture-loading" && i + 1 < argc) {
            loadingCapturePath = argv[++i];
        } else if (arg == "--capture-frame" && i + 1 < argc) {
            s_captureFrame = static_cast<uint64_t>(std::stoull(argv[++i]));
            s_captureFrameProvided = true;
        } else if (arg == "--capture-time" && i + 1 < argc) {
            s_captureTimeSeconds = std::stod(argv[++i]);
            s_captureTimeProvided = true;
        } else if (arg == "--capture-first-frame" && i + 1 < argc) {
            s_captureFirstFramePath = argv[++i];
        } else if (arg == "--exit-after-dump") {
            exitAfterDump = true;
        } else if (arg == "--profile") {
            s_profilingEnabled = true;
        } else if (arg == "--no-profile") {
            s_profilingEnabled = false;
        } else if (arg == "--profile-report" && i + 1 < argc) {
            s_profileReportPath = argv[++i];
            s_profilingEnabled = true;
        } else if (arg == "--exit-after-profile") {
            s_exitAfterProfile = true;
        } else if (arg == "--benchmark") {
            s_benchmarkEnabled = true;
        } else if (arg == "--benchmark-output" && i + 1 < argc) {
            s_benchmarkOutputPath = argv[++i];
            s_benchmarkEnabled = true;
        } else if (arg == "--benchmark-warmup" && i + 1 < argc) {
            s_benchmarkWarmupFrames = static_cast<uint64_t>(std::stoull(argv[++i]));
            s_benchmarkEnabled = true;
        } else if (arg == "--benchmark-frames" && i + 1 < argc) {
            const uint64_t requested = static_cast<uint64_t>(std::stoull(argv[++i]));
            if (requested == 0) {
                std::fprintf(stderr, "--benchmark-frames must be at least 1\n");
                return 1;
            }
            s_benchmarkMeasuredFrames = requested;
            s_benchmarkEnabled = true;
        } else if (arg == "--exit-after-benchmark") {
            s_exitAfterBenchmark = true;
            s_benchmarkEnabled = true;
        } else if (arg == "--vsync" && i + 1 < argc) {
            ++i; // Applied by the pre-scan before window creation.
        } else if (arg == "--fixed-frame-time" && i + 1 < argc) {
            s_fixedFrameTime = std::stof(argv[++i]);
        } else if (arg == "--force-full-sync") {
            s_forceFullSync = true;
        } else if (arg == "--material-edit-emissive" && i + 1 < argc) {
            // A2.2: edits this material's emissive factor every frame (a material-only
            // SceneChange::Material edit) for the per-edit cost measurement.
            s_materialEditEmissiveMaterial = argv[++i];
        } else if (arg == "--disable-transform-reuse") {
            // P4.7 A/B reference: every traversal recomputes and pushes every node.
            ux3d::renderer::TransformHierarchyNode::setReuseEnabled(false);
            ux3d::renderer::Skin::s_jointUploadReuse = false;
        } else if (arg == "--always-pick") {
            s_alwaysPick = true;
            s_fullSizePicking = true;
        } else if (arg == "--full-size-picking") {
            s_fullSizePicking = true;
        } else if (arg == "--disable-pick-culling") {
            s_disablePickCulling = true;
        } else if (arg == "--disable-shared-traversal") {
            s_disableSharedTraversal = true;
        } else if (arg == "--show-invisible-materials") {
            activeViewport->settings.asobo.setApplied("ASOBO_material_invisible", false);
            activeViewport->settings.asobo.setApplied("ASOBO_material_environment_occluder", false);
        } else if (arg == "--decal-tolerance" && i + 1 < argc) {
            const std::string value = argv[++i];
            float mm = 0.0f;
            size_t parsed = 0;
            try {
                mm = std::stof(value, &parsed);
            } catch (...) {
                parsed = 0;
            }
            if (parsed != value.size() || !std::isfinite(mm) || mm < 0.0f || mm > 10.0f) {
                std::fprintf(stderr, "--decal-tolerance must be a number from 0 to 10 mm\n");
                return 1;
            }
            activeViewport->settings.decalDepthToleranceMm = mm;
        } else if (arg == "--hide-grid") {
            activeViewport->settings.showGrid = false;
        } else if (arg == "--ssr" && i + 1 < argc) {
            const std::string value = toLowerAscii(argv[++i]);
            if (value != "on" && value != "off") return 1;
            activeViewport->settings.screenSpaceReflections = value == "on";
        } else if (arg == "--ssr-quality" && i + 1 < argc) {
            const std::string value = toLowerAscii(argv[++i]);
            if (value != "low" && value != "medium" && value != "high") return 1;
            activeViewport->settings.ssrQuality = value == "low" ? 0 : value == "high" ? 2 : 1;
        } else if (arg == "--interior-sky-reflection" && i + 1 < argc) {
            const float value = std::stof(argv[++i]);
            if (!std::isfinite(value) || value < 0.0f || value > 1.0f) return 1;
            activeViewport->settings.interiorSkyReflection = value;
        } else if (arg == "--windshield-icing" && i + 1 < argc) {
            const float value = std::stof(argv[++i]);
            if (!std::isfinite(value) || value < 0.0f || value > 1.0f) return 1;
            activeViewport->settings.windshieldIcing = value;
        } else if (arg == "--asobo" && i + 1 < argc) {
            activeViewport->settings.asobo.enabled = toLowerAscii(argv[++i]) != "off";
        } else if (arg == "--asobo-disable" && i + 1 < argc) {
            activeViewport->settings.asobo.setApplied(argv[++i], false);
        } else if (arg == "--set-asobo-param" && i + 4 < argc) {
            AsoboParamEditArg edit;
            edit.material = argv[++i];
            edit.extension = argv[++i];
            edit.param = argv[++i];
            edit.value = argv[++i];
            setAsoboParamArgs.push_back(std::move(edit));
        } else if (arg == "--disable-wireframe-cache") {
            s_disableWireframeCache = true;
        } else if (arg == "--msfs-emissive" && i + 1 < argc) {
            s_disableMsfsEmissive = toLowerAscii(argv[++i]) == "off";
        } else if (arg == "--wireframe-overlay") {
            activeViewport->settings.wireframeOverlay = true;
        } else if (arg == "--wireframe-color" && i + 1 < argc) {
            float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
            const char* val = argv[++i];
            int count = std::sscanf(val, "%f,%f,%f,%f", &r, &g, &b, &a);
            if (count < 3) return 1;
            if (count == 3) a = 1.0f;
            activeViewport->settings.wireframeColor = glm::vec4(r, g, b, a);
        } else if (arg == "--wireframe-blend" && i + 1 < argc) {
            std::string modeStr = toLowerAscii(argv[++i]);
            if (modeStr == "normal") {
                activeViewport->settings.wireframeBlend = ux3d::imgui::WireframeBlendMode::Normal;
            } else if (modeStr == "additive") {
                activeViewport->settings.wireframeBlend = ux3d::imgui::WireframeBlendMode::Additive;
            } else if (modeStr == "invert") {
                activeViewport->settings.wireframeBlend = ux3d::imgui::WireframeBlendMode::Invert;
            } else if (modeStr == "xor") {
                activeViewport->settings.wireframeBlend = ux3d::imgui::WireframeBlendMode::XOR;
            } else {
                std::fprintf(stderr, "Invalid --wireframe-blend: %s\n", modeStr.c_str());
                return 1;
            }
        } else if (arg == "--disable-instancing") {
            s_disableInstancing = true;
            commandGenerator->getCommandMapping().setInstancingEnabled(false);
        } else if (arg == "--disable-redraw-skip") {
            s_disableRedrawSkip = true;
        } else if (arg == "--decode-budget-mib" && i + 1 < argc) {
            s_decodeBudgetMiB = std::stoll(argv[++i]);
        } else if (arg == "--validation-full-read") {
            ux3d::imgui::GltfValidator::s_fullFileRead = true;
        } else if (arg == "--validation-report" && i + 1 < argc) {
            s_validationReportPath = argv[++i];
        } else if (arg == "--validate-file" && i + 1 < argc) {
            validateFileArg = argv[++i];
        } else if (arg == "--disable-culling") {
            s_disableCulling = true;
        } else if (arg == "--disable-idle-wait") {
            s_disableIdleWait = true;
        } else if (arg == "--idle-stats" && i + 1 < argc) {
            s_idleStatsPath = argv[++i];
        } else if (arg == "--disable-command-caches") {
            s_disableCommandCaches = true;
            commandGenerator->getCommandMapping().setProgramCacheEnabled(false);
            commandGenerator->getCommandMapping().setCommandCacheEnabled(false);
        } else if (arg == "--anim-time" && i + 1 < argc) {
            animTimeArg = std::stof(argv[++i]);
        } else if (arg == "--scenario-report" && i + 1 < argc) {
            scenarioReportPath = argv[++i];
        } else if (arg == "--scenario-name" && i + 1 < argc) {
            scenarioName = argv[++i];
        }
        if (arg == "--camera-yaw" && i + 1 < argc) {
            cameraYawArg = std::stof(argv[++i]);
            cameraYawProvided = true;
        } else if (arg == "--camera-pitch" && i + 1 < argc) {
            cameraPitchArg = std::stof(argv[++i]);
            cameraPitchProvided = true;
        } else if (arg == "--camera-distance" && i + 1 < argc) {
            cameraDistanceArg = std::stof(argv[++i]);
            cameraDistanceProvided = true;
        } else if (arg == "--camera-target" && i + 1 < argc) {
            const std::string targetText = argv[++i];
            if (parseFloat3(targetText, cameraTargetX, cameraTargetY, cameraTargetZ)) {
                cameraTargetProvided = true;
            } else {
                std::fprintf(stderr, "Invalid --camera-target '%s' (expected X,Y,Z)\n", targetText.c_str());
            }
        } else if (arg == "--camera-fov" && i + 1 < argc) {
            cameraFovArg = std::stof(argv[++i]);
            cameraFovProvided = true;
        } else if (arg == "--camera-projection" && i + 1 < argc) {
            cameraProjectionArg = toLowerAscii(argv[++i]);
        } else if (arg == "--debug-view" && i + 1 < argc) {
            debugViewArg = argv[++i];
        } else if (arg == "--window-size" && i + 1 < argc) {
            ++i; // Value was applied by the pre-scan before window creation.
        } else if (arg == "--ui-script" && i + 1 < argc) {
            uiScriptPath = argv[++i];
        } else if (arg == "--ui-report" && i + 1 < argc) {
            uiReportPath = argv[++i];
        } else if (arg == "--isolated" || arg == "--no-persist") {
            s_isolatedState = true; // Also applied by the pre-scan.
        } else if (arg == "--ui-script-follow" && i + 1 < argc) {
            uiScriptFollowPath = argv[++i];
            s_isolatedState = true;
        } else if (arg == "--frame-node" && i + 1 < argc) {
            frameNodeArg = argv[++i];
        } else if (arg == "--frame-view" && i + 1 < argc) {
            frameViewArg = parseCameraOrientation(argv[++i]);
        } else if (arg == "--frame-margin" && i + 1 < argc) {
            frameMarginArg = std::stof(argv[++i]);
        } else if (arg == "--report-node" && i + 1 < argc) {
            reportNodeArgs.emplace_back(argv[++i]);
        } else if (arg == "--report-pick" && i + 1 < argc) {
            std::string xyStr = argv[++i];
            int px = 0, py = 0;
            char comma = ',';
            std::istringstream stream(xyStr);
            if (stream >> px >> comma >> py) {
                reportPickArgs.push_back({ px, py });
            } else {
                std::fprintf(stderr, "Invalid --report-pick '%s' (expected X,Y)\n", xyStr.c_str());
            }
        } else if (arg == "--isolate-node" && i + 1 < argc) {
            isolateNodeArg = argv[++i];
        } else if (arg == "--select-node" && i + 1 < argc) {
            selectNodeArg = argv[++i];
        } else if (arg == "--hide-node" && i + 1 < argc) {
            hideNodeArg = argv[++i];
        } else if (arg == "--reload-model" && i + 1 < argc) {
            reloadModelArg = argv[++i];
        } else if (arg == "--asobo-report" && i + 1 < argc) {
            asoboReportPath = argv[++i];
        } else if (arg == "--search-path" && i + 1 < argc) {
            searchPathArgs.emplace_back(argv[++i]);
        } else if (arg == "--rename-node" && i + 1 < argc) {
            renameNodeArg = argv[++i];
        } else if (arg == "--rename-mesh" && i + 1 < argc) {
            renameMeshArg = argv[++i];
        } else if (arg == "--rename-material" && i + 1 < argc) {
            renameMaterialArg = argv[++i];
        } else if (arg == "--hierarchy-display" && i + 1 < argc) {
            hierarchyDisplayProvided = true;
            parseDisplayOptions(argv[++i], hierarchyDisplayValue);
        } else if (arg == "--resource-display" && i + 1 < argc) {
            resourceDisplayProvided = true;
            parseDisplayOptions(argv[++i], resourceDisplayValue);
        } else if (arg == "--select-material" && i + 1 < argc) {
            selectMaterialArg = argv[++i];
        } else if (arg == "--select-image" && i + 1 < argc) {
            selectImageArg = argv[++i];
        } else if (arg == "--select-texture" && i + 1 < argc) {
            selectTextureArg = argv[++i];
        } else if (arg == "--select-viewport" && i + 1 < argc) {
            selectViewportArg = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--create-viewport-cam" && i + 1 < argc) {
            newViewportCamArg = argv[++i];
        } else if (arg == "--show-validation" || arg == "--validate") {
            showValidationArg = true;
        } else if (arg == "--show-search-paths" || arg == "--search-paths") {
            showSearchPathsArg = true;
        } else if (arg == "--show-statistics") {
            showStatisticsArg = true;
        } else if (arg == "--show-logger") {
            showLoggerArg = true;
        } else if (arg == "--logger-wrap") {
            loggerWrapArg = true;
        } else if (arg == "--logger-no-auto-scroll") {
            loggerAutoScrollArg = false;
        } else if (arg == "--log-test-variable") {
            loggerVariableEntriesArg = true;
        } else if (arg == "--log-test" && i + 1 < argc) {
            logTestCountArg = static_cast<size_t>(std::stoull(argv[++i]));
        } else if (arg == "--filter-node" && i + 1 < argc) {
            ux3d::imgui::SceneHierarchyPanel::setSearchFilter(argv[++i]);
        } else if (arg == "--filter-resource" && i + 1 < argc) {
            ux3d::imgui::ResourcesPanel::setSearchFilter(argv[++i]);
        } else if (arg == "--filter-validation" && i + 1 < argc) {
            ux3d::imgui::ValidationPanel::setSearchFilter(argv[++i]);
        } else if (arg == "--validation-severity" && i + 1 < argc) {
            const std::string severity = toLowerAscii(argv[++i]);
            if (severity == "errors" || severity == "error") {
                ux3d::imgui::ValidationPanel::setSeverityFilter(true, false, false);
            } else if (severity == "warnings" || severity == "warning") {
                ux3d::imgui::ValidationPanel::setSeverityFilter(false, true, false);
            } else if (severity == "infos" || severity == "info") {
                ux3d::imgui::ValidationPanel::setSeverityFilter(false, false, true);
            } else {
                ux3d::imgui::ValidationPanel::setSeverityFilter(true, true, true);
            }
        } else if (arg == "--conflict-selfcheck" && i + 1 < argc) {
            conflictSelfCheckOutputPath = argv[++i];
        } else if (arg == "--validation-cache-selfcheck" && i + 1 < argc) {
            validationCacheSelfCheckOutputPath = argv[++i];
        } else if (arg == "--cache-invalidation-selfcheck" && i + 1 < argc) {
            cacheInvalidationSelfCheckOutputPath = argv[++i];
        } else if (arg == "--logger-layout-selfcheck" && i + 1 < argc) {
            loggerLayoutSelfCheckOutputPath = argv[++i];
        } else if (arg == "--revision-selfcheck" && i + 1 < argc) {
            revisionSelfCheckOutputPath = argv[++i];
        } else if (arg == "--aa-target-selfcheck" && i + 1 < argc) {
            aaTargetSelfCheckOutputPath = argv[++i];
        } else if (arg == "--sampler-selfcheck" && i + 1 < argc) {
            samplerSelfCheckOutputPath = argv[++i];
        } else if (arg == "--sampler-selfcheck-case" && i + 1 < argc) {
            samplerSelfCheckCase = argv[++i];
        } else if (arg == "--sampler-dump-pattern" && i + 1 < argc) {
            samplerDumpPatternPath = argv[++i];
        } else if (arg == "--vao-selfcheck" && i + 1 < argc) {
            vaoSelfCheckOutputPath = argv[++i];
        } else if (arg == "--vao-selfcheck-case" && i + 1 < argc) {
            vaoSelfCheckCase = argv[++i];
        } else if (arg == "--polygon-mode-selfcheck" && i + 1 < argc) {
            polygonModeSelfCheckOutputPath = argv[++i];
        } else if (arg == "--polygon-mode-selfcheck-case" && i + 1 < argc) {
            polygonModeSelfCheckCase = argv[++i];
        } else if (arg == "--cull-winding-selfcheck" && i + 1 < argc) {
            cullWindingSelfCheckOutputPath = argv[++i];
        } else if (arg == "--cull-winding-selfcheck-case" && i + 1 < argc) {
            cullWindingSelfCheckCase = argv[++i];
        } else if (arg == "--depth-selfcheck" && i + 1 < argc) {
            depthSelfCheckOutputPath = argv[++i];
        } else if (arg == "--depth-selfcheck-case" && i + 1 < argc) {
            depthSelfCheckCase = argv[++i];
        } else if (arg == "--blend-selfcheck" && i + 1 < argc) {
            blendSelfCheckOutputPath = argv[++i];
        } else if (arg == "--blend-selfcheck-case" && i + 1 < argc) {
            blendSelfCheckCase = argv[++i];
        } else if (arg == "--force-double-sided") {
            activeViewport->settings.forceDoubleSided = true;
        } else if (arg == "--invert-winding") {
            activeViewport->settings.invertWinding = true;
        } else if (arg == "--selfcheck-force-failure") {
            selfCheckForceFailure = true;
        } else if ((arg == "--env" || arg == "--hdr") && i + 1 < argc) {
            std::string customHdr = argv[++i];
            auto path = ux3d::imgui::StdFileSystem::instance().toPath(customHdr.c_str());
            scenarioEnvironmentPath = path.toString().c_str();
            scenarioEnvironmentLoaded = false;
            const auto environmentInitStart = ux3d::imgui::profiling::Clock::now();
            gltfLayer->setEnvironmentMap(path);
            startupProfile.environmentInitMs += ux3d::imgui::profiling::elapsedMs(environmentInitStart);
            const auto environmentFilterStart = ux3d::imgui::profiling::Clock::now();
            if (gltfLayer->filterEnvironmentMap()) {
                startupProfile.environmentFilterMs += ux3d::imgui::profiling::elapsedMs(environmentFilterStart);
                startupProfile.environmentLoads++;
                scenarioEnvironmentLoaded = true;
                cliHdrProvided = true;
                cliHdrPath = customHdr;
                getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).currentHdrPath = customHdr;
                commandGenerator->setIBLEnabled(true);
                logMessage("Loaded environment map: " + customHdr);
            }
        } else if ((arg == "--aa" || arg == "--anti-aliasing") && i + 1 < argc) {
            std::string aaVal = argv[++i];
            if (aaVal == "none" || aaVal == "0")
                activeViewport->settings.setLegacyAA(0);
            else if (aaVal == "msaa2" || aaVal == "1")
                activeViewport->settings.setLegacyAA(1);
            else if (aaVal == "msaa4" || aaVal == "2")
                activeViewport->settings.setLegacyAA(2);
            else if (aaVal == "msaa8" || aaVal == "3")
                activeViewport->settings.setLegacyAA(3);
            else if (aaVal == "fxaa" || aaVal == "4")
                activeViewport->settings.setLegacyAA(4);
            else if (aaVal == "taa" || aaVal == "5")
                activeViewport->settings.setLegacyAA(5);
        } else if ((arg == "--msaa" || arg == "--post-aa" || arg == "--bloom" ||
                    arg == "--bloom-intensity" || arg == "--bloom-radius" ||
                    arg == "--auto-exposure" || arg == "--exposure-compensation" ||
                    arg == "--msfs-emissive-scale" || arg == "--time-of-day") && i + 1 < argc) {
            ++i; // Applied in a second pass, after every legacy --aa argument.
        } else if (arg == "--clear-color" && i + 1 < argc) {
            float r, g, b;
            if (std::sscanf(argv[++i], "%f,%f,%f", &r, &g, &b) != 3) return 1;
            auto& env = getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
            cliClearProvided = true;
            cliClearColor = env.clearColor = glm::vec4(r, g, b, 1.0f);
            env.showBackground = false;
        } else if (arg == "--hdr-rotation" && i + 1 < argc) {
            cliHdrRotation = std::stof(argv[++i]);
            cliHdrRotationProvided = true;
            getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).hdrRotation = cliHdrRotation;
        } else if (arg == "--exposure" && i + 1 < argc) {
            cliExposureEV = std::stof(argv[++i]);
            cliExposureProvided = true;
            getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).exposureEV = cliExposureEV;
        } else if (arg == "--tone-mapping" && i + 1 < argc) {
            std::string tmVal = toLowerAscii(argv[++i]);
            if (tmVal == "pbr_neutral" || tmVal == "neutral" || tmVal == "0")
                cliToneMappingIdx = 0;
            else if (tmVal == "aces_hill" || tmVal == "hill" || tmVal == "1")
                cliToneMappingIdx = 1;
            else if (tmVal == "aces_narkowicz" || tmVal == "narkowicz" || tmVal == "2")
                cliToneMappingIdx = 2;
            cliToneMappingProvided = true;
            getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).toneMappingIdx = cliToneMappingIdx;
        } else if (arg == "--render-mode" && i + 1 < argc) {
            std::string rmVal = toLowerAscii(argv[++i]);
            if (rmVal == "wireframe" || rmVal == "1") {
                activeViewport->settings.renderMode = ux3d::renderer::RenderMode::Wireframe;
                activeViewport->settings.renderModeIdx = 1;
            } else if (rmVal == "default" || rmVal == "shaded" || rmVal == "0" || rmVal == "none") {
                activeViewport->settings.renderMode = ux3d::renderer::RenderMode::None;
                activeViewport->settings.renderModeIdx = 0;
            }
        } else if (std::string qualityError; ux3d::imgui::RenderQuality::parseArgument(argc, argv, i, qualityError)) {
            // Already applied by the pre-scan; this only skips the flag and its value.
        } else if (!arg.empty() && arg[0] != '-') {
            modelToLoad = arg;
        }
    }

    for (int i = 1; i + 1 < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--msaa") {
            const std::string value = argv[++i];
            if (value == "0" || value == "2" || value == "4" || value == "8")
                activeViewport->settings.msaaIdx = value == "0" ? 0 : value == "2" ? 1 : value == "4" ? 2 : 3;
            else { std::fprintf(stderr, "Invalid --msaa: %s\n", value.c_str()); return 1; }
        } else if (arg == "--post-aa") {
            const std::string value = argv[++i];
            if (value == "none" || value == "fxaa" || value == "taa")
                activeViewport->settings.postAAIdx = value == "none" ? 0 : value == "fxaa" ? 1 : 2;
            else { std::fprintf(stderr, "Invalid --post-aa: %s\n", value.c_str()); return 1; }
        } else if (arg == "--bloom") {
            const std::string val = toLowerAscii(argv[++i]);
            if (val == "on" || val == "1" || val == "true") {
                cliBloomEnabled = true;
            } else if (val == "off" || val == "0" || val == "false") {
                cliBloomEnabled = false;
            } else {
                std::fprintf(stderr, "Invalid --bloom: %s\n", val.c_str());
                return 1;
            }
            cliBloomProvided = true;
            getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).bloomEnabled = cliBloomEnabled;
        } else if (arg == "--bloom-intensity") {
            cliBloomIntensity = std::stof(argv[++i]);
            cliBloomIntensityProvided = true;
            getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).bloomIntensity = cliBloomIntensity;
        } else if (arg == "--bloom-radius") {
            cliBloomRadius = std::stof(argv[++i]);
            cliBloomRadiusProvided = true;
            getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).bloomRadius = cliBloomRadius;
        } else if (arg == "--auto-exposure") {
            const std::string val = toLowerAscii(argv[++i]);
            if (val == "on" || val == "1" || val == "true" || val == "auto") {
                cliAutoExposure = true;
            } else if (val == "off" || val == "0" || val == "false" || val == "manual") {
                cliAutoExposure = false;
            } else {
                std::fprintf(stderr, "Invalid --auto-exposure: %s\n", val.c_str());
                return 1;
            }
            cliAutoExposureProvided = true;
            getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).autoExposure = cliAutoExposure;
        } else if (arg == "--exposure-compensation") {
            cliExposureCompensationEV = std::stof(argv[++i]);
            cliExposureCompensationProvided = true;
            getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).exposureCompensationEV = cliExposureCompensationEV;
        } else if (arg == "--msfs-emissive-scale" && i + 1 < argc) {
            cliMsfsEmissiveScale = std::stof(argv[++i]);
            cliMsfsEmissiveScaleProvided = true;
            getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).msfsEmissiveScale = cliMsfsEmissiveScale;
        } else if (arg == "--time-of-day" && i + 1 < argc) {
            const std::string todVal = toLowerAscii(argv[++i]);
            if (todVal == "night" || todVal == "1") {
                cliTimeOfDay = 1;
            } else if (todVal == "day" || todVal == "0") {
                cliTimeOfDay = 0;
            } else {
                std::fprintf(stderr, "Invalid --time-of-day: %s (expected day or night)\n", todVal.c_str());
                return 1;
            }
            cliTimeOfDayProvided = true;
            getSceneEnvironment(sceneEnvironments, gltfModel, activeScene).timeOfDay = cliTimeOfDay;
        }
    }

    ux3d::imgui::profiling::setCountingEnabled(s_profilingEnabled);

    // A2.1 check: the forward depth readback requires a single-sample target. The check
    // also redraws every frame so the warm-up frame is captured (see the argument above).
    if (!s_decalPrepassCheckPath.empty()) {
        for (auto& vp : viewports) {
            if (vp) {
                vp->settings.setLegacyAA(0);
            }
        }
    }

    // P1.5 diagnostic control: append deterministic entries covering every
    // severity without echoing them to stderr. Used to exercise counters, Copy
    // and Clear with the Logger both closed and open.
    for (size_t i = 0; i < logTestCountArg; ++i) {
        ux3d::core::LogLevel level = ux3d::core::LogLevel::Info;
        ImVec4 color(0.85f, 0.85f, 0.88f, 1.0f);
        const char* severity = "Info";
        switch (i % 4) {
        case 1:
            level = ux3d::core::LogLevel::Warning;
            color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            severity = "Warning";
            break;
        case 2:
            level = ux3d::core::LogLevel::Error;
            color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
            severity = "Error";
            break;
        case 3:
            level = ux3d::core::LogLevel::Fatal;
            color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
            severity = "Fatal";
            break;
        default:
            break;
        }
        std::string text = "Log test #" + std::to_string(i) + " [" + severity + "]";
        if (loggerVariableEntriesArg && (i % 3) == 1) {
            text += "\ncontinued test line " + std::to_string(i);
        } else if (loggerVariableEntriesArg && (i % 3) == 2) {
            text += " " + std::string(160, 'W');
        }
        s_consoleLogs.append({std::move(text), color, level});
    }

    if (s_benchmarkEnabled && s_benchmarkOutputPath.empty()) {
        s_benchmarkOutputPath = "glider-benchmark.json";
    }
    if (!s_captureViewportPath.empty() && !s_captureFrameProvided && !s_captureTimeProvided) {
        s_captureFrame = 8;
        s_captureFrameProvided = true;
    }
    if (s_captureFrameProvided && s_captureFrame == 0) {
        std::fprintf(stderr, "--capture-frame must be at least 1\n");
        return 1;
    }
    if (s_captureTimeProvided && s_captureTimeSeconds < 0.0) {
        std::fprintf(stderr, "--capture-time must be non-negative\n");
        return 1;
    }

    if (!conflictSelfCheckOutputPath.empty()) {
        return writeSelfCheckReport(animationPlayer.runConflictSelfCheck(selfCheckForceFailure),
                                    conflictSelfCheckOutputPath, "conflict");
    }

    if (!validationCacheSelfCheckOutputPath.empty()) {
        return writeSelfCheckReport(ux3d::imgui::ValidationPanel::runCacheSelfCheck(selfCheckForceFailure),
                                    validationCacheSelfCheckOutputPath, "validation cache");
    }

    if (!cacheInvalidationSelfCheckOutputPath.empty()) {
        const auto hierarchyResult =
            ux3d::imgui::SceneHierarchyPanel::runCacheInvalidationSelfCheck(selfCheckForceFailure);
        const auto resourcesResult = ux3d::imgui::ResourcesPanel::runCacheInvalidationSelfCheck(selfCheckForceFailure);
        const auto validationResult = ux3d::imgui::ValidationPanel::runReportRevisionSelfCheck(selfCheckForceFailure);

        ux3d::imgui::SelfCheckResult combined;
        combined.success = hierarchyResult.success && resourcesResult.success && validationResult.success;
        std::ostringstream report;
        report << hierarchyResult.report;
        report << "\n";
        report << resourcesResult.report;
        report << "\n";
        report << validationResult.report;
        combined.report = report.str();
        return writeSelfCheckReport(combined, cacheInvalidationSelfCheckOutputPath, "cache invalidation");
    }

    if (!loggerLayoutSelfCheckOutputPath.empty()) {
        return writeSelfCheckReport(runConsoleLogLayoutSelfCheck(selfCheckForceFailure),
                                    loggerLayoutSelfCheckOutputPath, "logger layout");
    }

    if (!validateFileArg.empty()) {
        // Container checks are reachable here for files the loader rejects; validation
        // after a load only ever sees files the loader accepted. A file the loader accepts is
        // validated with its model too, so the model-based checks (ASOBO extensions) run.
        ux3d::gltf::Gltf model;
        const auto loaded = ux3d::gltf::GltfParsing::fromFile(model, ux3d::imgui::StdFileSystem::instance().toFile(validateFileArg.c_str()),
                                                             ux3d::gltf::ImportOptions{});
        if (loaded.derived != ux3d::gltf::GltfFromFileResult::Success) {
            model.reset();
        }
        const auto report = ux3d::imgui::GltfValidator::instance().validate(validateFileArg, model);
        const bool written = s_validationReportPath.empty() ||
                             ux3d::imgui::GltfValidator::writeReportJson(report, s_validationReportPath);
        return written ? 0 : 1;
    }

    if (!revisionSelfCheckOutputPath.empty()) {
        return writeSelfCheckReport(ux3d::imgui::SceneRevisions::runSelfCheck(selfCheckForceFailure),
                                    revisionSelfCheckOutputPath, "scene revision");
    }

    if (!aaTargetSelfCheckOutputPath.empty()) {
        return writeSelfCheckReport(ux3d::imgui::AntiAliasing::runTargetAllocationSelfCheck(selfCheckForceFailure),
                                    aaTargetSelfCheckOutputPath, "AA target allocation");
    }

    if (!samplerSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports,
                                    gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({commandGenerator.get()});
        };
        return writeSelfCheckReport(ux3d::imgui::runSamplerSelfCheck(gladGL.get(), selfCheckForceFailure,
                                                                     samplerSelfCheckCase, samplerDumpPatternPath,
                                                                     glRenderer.get(), reloadAndDraw),
                                    samplerSelfCheckOutputPath, "sampler objects and cache");
    }

    if (!vaoSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports,
                                    gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({commandGenerator.get()});
        };
        return writeSelfCheckReport(ux3d::imgui::runVaoSelfCheck(gladGL.get(), selfCheckForceFailure, vaoSelfCheckCase,
                                                                 glRenderer.get(), commandGenerator.get(),
                                                                 reloadAndDraw),
                                    vaoSelfCheckOutputPath, "VAO reuse and cache");
    }

    if (!polygonModeSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports,
                                    gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({commandGenerator.get()});
        };
        return writeSelfCheckReport(ux3d::imgui::runPolygonModeSelfCheck(gladGL.get(), selfCheckForceFailure,
                                                                         polygonModeSelfCheckCase, glRenderer.get(),
                                                                         commandGenerator.get(), reloadAndDraw),
                                    polygonModeSelfCheckOutputPath, "polygon mode cache");
    }

    if (!cullWindingSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports,
                                    gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({commandGenerator.get()});
        };
        return writeSelfCheckReport(ux3d::imgui::runCullWindingSelfCheck(gladGL.get(), selfCheckForceFailure,
                                                                         cullWindingSelfCheckCase, glRenderer.get(),
                                                                         commandGenerator.get(), reloadAndDraw),
                                    cullWindingSelfCheckOutputPath, "culling and winding cache");
    }

    if (!depthSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports,
                                    gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({commandGenerator.get()});
        };
        return writeSelfCheckReport(ux3d::imgui::runDepthSelfCheck(gladGL.get(), selfCheckForceFailure,
                                                                   depthSelfCheckCase, glRenderer.get(),
                                                                   commandGenerator.get(), reloadAndDraw),
                                    depthSelfCheckOutputPath, "depth test and depth func cache");
    }

    if (!blendSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports,
                                    gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({commandGenerator.get()});
        };
        return writeSelfCheckReport(ux3d::imgui::runBlendSelfCheck(gladGL.get(), selfCheckForceFailure,
                                                                   blendSelfCheckCase, glRenderer.get(),
                                                                   commandGenerator.get(), reloadAndDraw),
                                    blendSelfCheckOutputPath, "blending state cache");
    }

    // --isolated leaves the single settings file untouched: the store is disabled for
    // the whole run, so panel-triggered saves stay in memory.
    ux3d::imgui::AppSettings::instance().loadSettings();
    s_recentFiles = loadRecentFiles();

    {
        const uint64_t mib = 1024ull * 1024ull;
        const uint64_t budget = s_decodeBudgetMiB >= 0
                                    ? static_cast<uint64_t>(s_decodeBudgetMiB) * mib
                                    : std::max<uint64_t>(512ull * mib, ux3d::imgui::physicalMemoryBytes() / 8u);
        ux3d::imageformats::setDecodeMemoryBudget(budget);
    }

    // --search-path: extra texture folders for this run (also with --isolated, which ignores settings.ini).
    for (const std::string& searchPath : searchPathArgs) {
        ux3d::imgui::AppSettings::instance().addSearchPath(searchPath);
    }

    if (hierarchyDisplayProvided) {
        ux3d::imgui::AppSettings::instance().setHierarchyOptions(hierarchyDisplayValue);
    }
    if (resourceDisplayProvided) {
        ux3d::imgui::AppSettings::instance().setResourceOptions(resourceDisplayValue);
    }

    if (ux3d::imgui::StdFileSystem::instance().isFile(
            ux3d::imgui::StdFileSystem::instance().toFile(modelToLoad.c_str()))) {
        modelLoaded = loadGltfModel(modelToLoad, gltfModel, activeScene, selection, currentFilePath, viewports,
                                    gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments, false, withoutTextures);
        updateWindowTitle(window, currentFilePath);

        auto renameNamedElement = [](auto& elements, const std::string& assignment, const char* label) {
            const size_t separator = assignment.find('=');
            if (separator == std::string::npos) {
                return false;
            }
            const std::string oldName = assignment.substr(0, separator);
            const std::string newName = assignment.substr(separator + 1);
            for (auto& element : elements) {
                if (element.hasName() && element.getName() == oldName.c_str()) {
                    element.setName(newName.c_str());
                    std::printf("Renamed %s '%s' to '%s'\n", label, oldName.c_str(), newName.c_str());
                    return true;
                }
            }
            return false;
        };

        if (!renameNodeArg.empty()) {
            renameNamedElement(gltfModel.getNodes(), renameNodeArg, "node");
        }
        if (!renameMeshArg.empty()) {
            renameNamedElement(gltfModel.getMeshes(), renameMeshArg, "mesh");
        }
        if (!renameMaterialArg.empty()) {
            renameNamedElement(gltfModel.getElements<ux3d::gltf::Material>(), renameMaterialArg, "material");
        }

        // --select-node and --isolate-node keep their exact-name matching: existing scripts rely on a
        // non-matching name selecting nothing. Fuzzy matching is for --frame-node and live commands.
        auto findNodeExact = [this](const std::string& name) -> ux3d::gltf::Node* {
            for (auto& candidate : gltfModel.getNodes()) {
                if (candidate.hasName() && candidate.getName() == name) return &candidate;
            }
            return nullptr;
        };
        if (!isolateNodeArg.empty()) {
            ux3d::gltf::Node* node = findNodeExact(isolateNodeArg);
            if (node != nullptr) {
                gltfLayer->setIsolation(node, false);
                sceneRevisions.mark(ux3d::imgui::SceneChange::Visibility);
                // Select and frame it through the --select-node path below, as before.
                selectNodeArg = isolateNodeArg;
            }
        }
        if (!selectNodeArg.empty()) {
            ux3d::gltf::Node* node = findNodeExact(selectNodeArg);
            if (node != nullptr) {
                selection.selectNode(node);
                ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
                if (activeViewport) {
                    focusCameraOnNode(node, gltfModel, gltfLayer.get(), activeViewport->cameraActor,
                                      activeViewport->antiAliasing, sceneRevisions);
                }
            }
        }
        if (!frameNodeArg.empty()) {
            ux3d::gltf::Node* node = findNode(frameNodeArg);
            if (node != nullptr) {
                selection.selectNode(node);
                ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
                if (activeViewport) {
                    if (frameViewArg.has_value()) {
                        activeViewport->cameraActor.orientUserCamera(frameViewArg.value());
                    }
                    focusCameraOnNode(node, gltfModel, gltfLayer.get(), activeViewport->cameraActor,
                                      activeViewport->antiAliasing, sceneRevisions, frameMarginArg);
                }
            }
        }
        if (!hideNodeArg.empty()) {
            std::string target = hideNodeArg;
            if (target.size() >= 2 && target.front() == '"' && target.back() == '"') {
                target = target.substr(1, target.size() - 2);
            }
            for (auto& node : gltfModel.getNodes()) {
                if ((node.hasName() && (node.getName() == target || node.getName() == hideNodeArg)) ||
                    std::to_string(node.m_ID) == hideNodeArg) {
                    node.setVisible(false);
                    sceneRevisions.mark(ux3d::imgui::SceneChange::Visibility);
                    std::printf("Hiding node %u: %s\n", node.m_ID, node.hasName() ? node.getName().c_str() : "");
                    break;
                }
            }
        }
        if (!selectMaterialArg.empty()) {
            for (auto& mat : gltfModel.getElements<ux3d::gltf::Material>()) {
                if ((mat.hasName() && mat.getName() == selectMaterialArg.c_str()) ||
                    std::to_string(mat.m_ID) == selectMaterialArg) {
                    selection.selectMaterial(&mat);
                    break;
                }
            }
        }
        if (!selectImageArg.empty()) {
            for (auto& img : gltfModel.getElements<ux3d::gltf::Image>()) {
                if ((img.hasName() && img.getName() == selectImageArg.c_str()) ||
                    std::to_string(img.m_ID) == selectImageArg ||
                    (!img.getUri().empty() && img.getUri().find(selectImageArg) != std::string::npos)) {
                    selection.selectImage(&img);
                    break;
                }
            }
        }
        if (!selectTextureArg.empty()) {
            for (auto& tex : gltfModel.getElements<ux3d::gltf::Texture>()) {
                if ((tex.hasName() && tex.getName() == selectTextureArg.c_str()) ||
                    std::to_string(tex.m_ID) == selectTextureArg) {
                    selection.selectTexture(&tex);
                    break;
                }
            }
        }
        if (animTimeArg >= 0.0f) {
            animationPlayer.setCurrentTime(animTimeArg, gltfLayer.get());
        }

        if (!newViewportCamArg.empty() && activeScene != nullptr) {
            std::string camTarget = newViewportCamArg;
            if (camTarget.size() >= 2 && camTarget.front() == '"' && camTarget.back() == '"') {
                camTarget = camTarget.substr(1, camTarget.size() - 2);
            }
            std::function<void(const ux3d::gltf::Node*)> checkNode = [&](const ux3d::gltf::Node* n) {
                if (!n) return;
                if (n->getCamera() != nullptr) {
                    std::string cName = n->hasName() ? n->getName() : ("Camera Node " + std::to_string(n->m_ID));
                    if (cName == camTarget || std::to_string(n->m_ID) == camTarget) {
                        uint32_t nextId = 1;
                        for (const auto& v : viewports) {
                            if (v->id >= nextId) nextId = v->id + 1;
                        }
                        std::string vpName = "Camera: " + cName;
                        auto newVp = std::make_unique<ux3d::imgui::ViewportInstance>(nextId, vpName, gladGL.get());
                        newVp->isAttachedToCamera = true;
                        newVp->attachedCameraNode = n;
                        newVp->attachedCameraNodeId = n->m_ID;
                        newVp->attachedCameraNodeName = cName;
                        if (activeViewport != nullptr) {
                            newVp->settings = activeViewport->settings;
                        }
                        newVp->requestFocus = true;
                        activeViewport = newVp.get();
                        viewports.push_back(std::move(newVp));
                        sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportLayout);
                        return;
                    }
                }
                for (auto* child : n->getChildren()) {
                    checkNode(child);
                }
            };
            for (auto* rootNode : activeScene->getNodes()) {
                checkNode(rootNode);
            }
        }

        if (selectViewportArg != 0) {
            for (auto& vp : viewports) {
                if (vp && vp->id == selectViewportArg) {
                    selection.selectViewport(vp->id);
                    break;
                }
            }
        }
    } else {
        updateWindowTitle(window, "untitled.glb");
    }

    if (!reloadModelArg.empty()) {
        modelLoaded = loadGltfModel(reloadModelArg, gltfModel, activeScene, selection, currentFilePath, viewports,
                                    gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments, false, withoutTextures);
        updateWindowTitle(window, currentFilePath);
        std::printf("Reloaded model: %s\n", reloadModelArg.c_str());
    }

    for (const auto& edit : setAsoboParamArgs) {
        if (!ux3d::imgui::ExtensionInspector::instance().setMaterialAsoboParameter(
                gltfModel, edit.material, edit.extension, edit.param, edit.value, sceneRevisions)) {
            std::fprintf(stderr, "Failed --set-asobo-param %s %s %s %s\n", edit.material.c_str(),
                         edit.extension.c_str(), edit.param.c_str(), edit.value.c_str());
            return 1;
        }
    }
    if (!setAsoboParamArgs.empty() && gltfLayer != nullptr) {
        gltfLayer->updateMaterialStates(gltfModel);
    }

    if (!asoboReportPath.empty()) {
        return writeAsoboReport(gltfModel, asoboReportPath) ? 0 : 1;
    }

    if (cliClearProvided || cliHdrProvided || cliHdrRotationProvided || cliExposureProvided || cliToneMappingProvided ||
        cliBloomProvided || cliBloomIntensityProvided || cliBloomRadiusProvided ||
        cliAutoExposureProvided || cliExposureCompensationProvided ||
        cliMsfsEmissiveScaleProvided || cliTimeOfDayProvided) {
        for (auto& env : sceneEnvironments) {
            if (cliClearProvided) { env.showBackground = false; env.clearColor = cliClearColor; }
            if (cliHdrProvided) env.currentHdrPath = cliHdrPath;
            if (cliHdrRotationProvided) env.hdrRotation = cliHdrRotation;
            if (cliExposureProvided) env.exposureEV = cliExposureEV;
            if (cliToneMappingProvided) env.toneMappingIdx = cliToneMappingIdx;
            if (cliBloomProvided) env.bloomEnabled = cliBloomEnabled;
            if (cliBloomIntensityProvided) env.bloomIntensity = cliBloomIntensity;
            if (cliBloomRadiusProvided) env.bloomRadius = cliBloomRadius;
            if (cliAutoExposureProvided) {
                env.autoExposure = cliAutoExposure;
                env.autoExposureStartFromManual = cliAutoExposure && cliExposureProvided;
            }
            if (cliExposureCompensationProvided) env.exposureCompensationEV = cliExposureCompensationEV;
            if (cliMsfsEmissiveScaleProvided) env.msfsEmissiveScale = cliMsfsEmissiveScale;
            if (cliTimeOfDayProvided) env.timeOfDay = cliTimeOfDay;
        }
    }

    // P0.1 explicit camera/projection controls are applied after model load and
    // after --select-node framing so the requested pose always wins.
    const bool cameraOverrideRequested = cameraYawProvided || cameraPitchProvided || cameraDistanceProvided ||
                                         cameraTargetProvided || cameraFovProvided || !cameraProjectionArg.empty();
    if (cameraOverrideRequested && activeViewport != nullptr) {
        auto& viewport = *activeViewport;

        if (cameraFovProvided) {
            if (cameraFovArg > 0.1f && cameraFovArg < 179.0f) {
                auto& camera = viewport.rendererCamera;
                camera.setPerspective(camera.getAspectRatio(), glm::radians(cameraFovArg), camera.getZNearPlane(),
                                      camera.getZFarPlane());
            } else {
                logMessage("Ignoring --camera-fov outside (0.1, 179) degrees", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
        }

        if (cameraTargetProvided) {
            viewport.cameraActor.setTarget(ux3d::math::vec3(cameraTargetX, cameraTargetY, cameraTargetZ));
        }

        if (cameraDistanceProvided) {
            if (cameraDistanceArg > 0.0f) {
                viewport.cameraActor.setZoom(cameraDistanceArg);
            } else {
                logMessage("Ignoring non-positive --camera-distance", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
        }

        if (cameraYawProvided || cameraPitchProvided) {
            const float yaw = cameraYawProvided ? glm::radians(cameraYawArg) : viewport.cameraActor.getRotationX();
            const float pitch =
                cameraPitchProvided ? glm::radians(cameraPitchArg) : viewport.cameraActor.getRotationY();
            viewport.cameraActor.setCameraRotation(yaw, pitch);
        } else {
            viewport.cameraActor.updateUserCameraTransform();
        }

        if (!cameraProjectionArg.empty()) {
            if (cameraProjectionArg == "ortho" || cameraProjectionArg == "orthographic") {
                viewport.cameraActor.changeUserCameraType(ux3d::renderer::Camera::Type::Orthographic);
            } else if (cameraProjectionArg == "perspective" || cameraProjectionArg == "persp") {
                viewport.cameraActor.changeUserCameraType(ux3d::renderer::Camera::Type::Perspective);
            } else {
                logMessage("Unknown --camera-projection (use perspective or orthographic)",
                           ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
        }

        viewport.antiAliasing.resetHistory();
        sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
    }

    if (!debugViewArg.empty() && activeViewport != nullptr) {
        const std::optional<ux3d::renderer::RenderMode> mode = getRenderModeFromName(debugViewArg);
        if (mode.has_value()) {
            activeViewport->settings.renderMode = mode.value();
            activeViewport->settings.renderModeIdx = getIndexFromRenderMode(mode.value());
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
            std::printf("Debug view: %s\n", getRenderModeLabel(mode.value()));
        } else {
            logMessage("Unknown --debug-view '" + debugViewArg + "'", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            std::fprintf(stderr, "Unknown --debug-view '%s'\n", debugViewArg.c_str());
        }
    }

    // UI state
    showInspector = true;
    showHierarchy = true;
    showResources = true;
    showLogger = showLoggerArg;
    showStatistics = showStatisticsArg;
    showValidation = false;
    showSearchPaths = false;
    if (showValidationArg) {
        showValidation = true;
        ux3d::imgui::ValidationPanel::requestFocus();
    }
    if (showSearchPathsArg) {
        showSearchPaths = true;
    }

    return std::nullopt;
}

} // namespace ux3d::imgui::application
