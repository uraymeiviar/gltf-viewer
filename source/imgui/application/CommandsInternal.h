// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Private to the Commands*.cpp files (agent control channel, docs/AGENT_CONTROL_PLAN.md): the includes and
// the file-local helpers they share (schema builders, setting templates, name/path helpers). Each including
// file gets its own internal copy of the anonymous-namespace helpers.

#pragma once

#include "Application.h"
#include "imgui/CaptureSettings.h"
#include "imgui/EmbeddedVFS.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/GltfExtensionSettings.h"
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

nlohmann::json asoboValueToJson(const ux3d::gltf::asobo::Value& value); // CommandLineHelpers.cpp

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

} // namespace ux3d::imgui::application
