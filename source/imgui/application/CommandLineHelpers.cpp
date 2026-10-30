// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Command-line helpers: argument parsers, node lookup by name / index (--select-node, --frame-node, ...) and
// --asobo-report with its ASOBO value JSON.

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
#include "imgui/ComputeTest.h"
#include "imgui/Theme.h"
#include "imageformats/imageformats.h"
#include "gltf/GltfParsing.h"
#include "gltf/ImportOptions.h"
#include "gltf/Material.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"
#include "renderer/Skin.h"
#include <algorithm>
#include <unordered_map>
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

// Shared with get_material_param (CommandsHandlers.cpp).
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

    // A3.7: the sky portals as the renderer reads them: radius in metres (source_radius cm / 100), the preview's inner
    // circle, the cone angles, and the world centre and direction into the cabin (+Z) of the node's rest pose.
    report["skyPortals"] = nlohmann::json::array();
    {
        std::unordered_map<const ux3d::gltf::Node*, const ux3d::gltf::Node*> parents;
        for (const auto& node : gltf.getElements<ux3d::gltf::Node>()) {
            for (const auto* child : node.getChildren()) parents[child] = &node;
        }
        size_t index = 0;
        for (const auto& node : gltf.getElements<ux3d::gltf::Node>()) {
            const size_t nodeIndex = index++;
            const std::optional<asobo::SkyPortal> portal =
                asobo::readSkyPortal(asobo::parseExtensions(gltf, node, asobo::Scope::Node));
            if (!portal.has_value()) continue;
            ux3d::math::dmat4x4 world = node.getTransformMatrix();
            for (auto it = parents.find(&node); it != parents.end(); it = parents.find(it->second)) {
                world = it->second->getTransformMatrix() * world;
            }
            const ux3d::math::dvec3 position(world[3]);
            const ux3d::math::dvec3 direction = ux3d::math::normalize(ux3d::math::dvec3(world[2]));
            report["skyPortals"].push_back({{"node", nodeIndex},
                                            {"name", node.hasName() ? nlohmann::json(std::string(node.getName().c_str())) : nlohmann::json(nullptr)},
                                            {"shapeType", portal->shapeType},
                                            {"sourceRadiusCm", portal->sourceRadiusCm},
                                            {"radiusMetres", portal->radiusMetres()},
                                            {"innerCircleRadiusMetres", portal->innerCircleRadiusMetres()},
                                            {"innerConeDeg", portal->innerConeDeg},
                                            {"outerConeDeg", portal->outerConeDeg},
                                            {"position", {position.x, position.y, position.z}},
                                            {"direction", {direction.x, direction.y, direction.z}}});
        }
    }

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

} // namespace ux3d::imgui::application
