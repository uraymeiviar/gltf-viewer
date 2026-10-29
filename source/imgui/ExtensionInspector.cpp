// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ExtensionInspector.h"
#include "imgui/EmbeddedVFS.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/TextureUtils.h"
#include "imgui/Theme.h"
#include "imgui/UiProbe.h"
#include "imgui/ViewportInstance.h"
#include "imgui/GltfExtensionSettings.h"
#include "gltf/Material.h"
#include "gltf/Texture.h"
#include "gltf/Image.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"

#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <unordered_set>

namespace ux3d::imgui {

ExtensionInspector& ExtensionInspector::instance()
{
    static ExtensionInspector s_instance;
    return s_instance;
}

ExtensionInspector::ExtensionInspector()
{
    loadAllSchemas();
}

void ExtensionInspector::loadAllSchemas()
{
    if (m_schemasLoaded) {
        return;
    }
    m_schemasLoaded = true;

    auto& vfs = EmbeddedVFS::instance();
    if (!vfs.isInitialized()) {
        vfs.initialize();
    }

    // Known list of MSFS extension names found in SDK Schemas
    static const std::vector<std::string> knownAsoboExtensions = {
        "ASOBO_animation_retargeting",
        "ASOBO_collision_object",
        "ASOBO_extra_occlusion_texture",
        "ASOBO_gizmo_object",
        "ASOBO_macro_light",
        "ASOBO_material_alphamode_dither",
        "ASOBO_material_anisotropic",
        "ASOBO_material_anisotropic_v2",
        "ASOBO_material_antialiasing_options",
        "ASOBO_material_blend_gbuffer",
        "ASOBO_material_clear_coat",
        "ASOBO_material_clear_coat_v2",
        "ASOBO_material_detail_map",
        "ASOBO_material_disable_motion_blur",
        "ASOBO_material_draw_order",
        "ASOBO_material_emissive",
        "ASOBO_material_environment_occluder",
        "ASOBO_material_fake_terrain",
        "ASOBO_material_fresnel_fade",
        "ASOBO_material_geometry_decal",
        "ASOBO_material_ghost_effect",
        "ASOBO_material_glass_v2",
        "ASOBO_material_glass",
        "ASOBO_material_kitty_glass",
        "ASOBO_material_invisible",
        "ASOBO_material_iridescent",
        "ASOBO_material_parallax_window",
        "ASOBO_material_pearlescent",
        "ASOBO_material_rain_options",
        "ASOBO_material_sail",
        "ASOBO_material_shadow_options",
        "ASOBO_material_SSS",
        "ASOBO_material_UV_options",
        "ASOBO_material_wind_flex",
        "ASOBO_material_windshield",
        "ASOBO_material_windshield_v3",
        "ASOBO_normal_map_convention",
        "ASOBO_occlusion_strength",
        "ASOBO_property_animation",
        "ASOBO_sky_portal",
        "ASOBO_softbody_mesh"
    };

    for (const auto& extName : knownAsoboExtensions) {
        // Try candidate paths in EmbeddedVFS
        std::string p1 = "schemas/asobo/" + extName + "/gltf." + extName + ".schema.json";
        std::string p2 = "schemas/asobo/" + extName + "/" + extName + ".schema.json";

        if (vfs.hasFile(p1)) {
            parseSchemaFile(p1, extName);
        } else if (vfs.hasFile(p2)) {
            parseSchemaFile(p2, extName);
        }
    }

    std::sort(m_availableNames.begin(), m_availableNames.end());
}

bool ExtensionInspector::parseSchemaFile(const std::string& vfsPath, const std::string& extName)
{
    auto vfsFile = EmbeddedVFS::instance().getFile(vfsPath);
    if (!vfsFile.isValid()) {
        return false;
    }

    try {
        nlohmann::json root = nlohmann::json::parse(vfsFile.asStringView());

        ExtensionSchemaInfo info;
        info.name = extName;
        info.title = root.value("title", extName);
        info.description = root.value("description", "");
        info.schemaJson = root;

        if (root.contains("properties") && root["properties"].is_object()) {
            for (const auto& [propKey, propVal] : root["properties"].items()) {
                if (propKey == "extensions" || propKey == "extras") {
                    continue;
                }

                ExtensionSchemaProperty p;
                p.key = propKey;
                p.description = propVal.value("description", "");

                // Check texture reference
                bool isTex = false;
                if (propVal.contains("$ref")) {
                    std::string ref = propVal.value("$ref", "");
                    if (ref.find("textureInfo") != std::string::npos) {
                        isTex = true;
                    }
                }
                if (propVal.contains("allOf") && propVal["allOf"].is_array()) {
                    for (const auto& item : propVal["allOf"]) {
                        if (item.contains("$ref")) {
                            std::string ref = item.value("$ref", "");
                            if (ref.find("textureInfo") != std::string::npos) {
                                isTex = true;
                                break;
                            }
                        }
                    }
                }
                if (propKey.find("Texture") != std::string::npos ||
                    propKey.find("Mask") != std::string::npos) {
                    isTex = true;
                }
                p.isTexture = isTex;

                if (isTex) {
                    p.type = "texture";
                } else if (propVal.contains("type") && propVal["type"].is_string()) {
                    p.type = propVal["type"].get<std::string>();
                } else {
                    p.type = "number"; // default fallback
                }

                // Check enum values
                if (propVal.contains("enum") && propVal["enum"].is_array()) {
                    for (const auto& ev : propVal["enum"]) {
                        if (ev.is_string()) {
                            p.enumValues.push_back(ev.get<std::string>());
                        }
                    }
                } else if (propVal.contains("anyOf") && propVal["anyOf"].is_array()) {
                    for (const auto& branch : propVal["anyOf"]) {
                        if (branch.contains("enum") && branch["enum"].is_array()) {
                            for (const auto& ev : branch["enum"]) {
                                if (ev.is_string()) {
                                    p.enumValues.push_back(ev.get<std::string>());
                                }
                            }
                        }
                    }
                }

                // Check bounds & defaults
                if (propVal.contains("minimum") && propVal["minimum"].is_number()) {
                    p.minimum = propVal["minimum"].get<double>();
                    p.hasMinimum = true;
                }
                if (propVal.contains("maximum") && propVal["maximum"].is_number()) {
                    p.maximum = propVal["maximum"].get<double>();
                    p.hasMaximum = true;
                }
                if (propVal.contains("default")) {
                    p.defaultValue = propVal["default"];
                }

                info.properties[propKey] = p;
                info.orderedPropertyKeys.push_back(propKey);
            }
        }

        m_schemas[extName] = info;
        m_availableNames.push_back(extName);
        return true;
    } catch (...) {
        return false;
    }
}

bool ExtensionInspector::hasSchema(const std::string& extensionName)
{
    loadAllSchemas();
    return m_schemas.find(extensionName) != m_schemas.end();
}

const ExtensionSchemaInfo* ExtensionInspector::getSchema(const std::string& extensionName)
{
    loadAllSchemas();
    auto it = m_schemas.find(extensionName);
    if (it != m_schemas.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::vector<std::string>& ExtensionInspector::getAvailableExtensionNames()
{
    loadAllSchemas();
    return m_availableNames;
}

std::string ExtensionInspector::formatExtensionLabel(const std::string& extensionName)
{
    std::string s = extensionName;

    if (s.rfind("ASOBO_material_", 0) == 0) {
        std::string stem = s.substr(15);
        for (char& c : stem) if (c == '_') c = ' ';
        if (!stem.empty()) stem[0] = static_cast<char>(std::toupper(stem[0]));
        return stem + " (ASOBO)";
    }
    if (s.rfind("ASOBO_", 0) == 0) {
        std::string stem = s.substr(6);
        for (char& c : stem) if (c == '_') c = ' ';
        if (!stem.empty()) stem[0] = static_cast<char>(std::toupper(stem[0]));
        return stem + " (ASOBO)";
    }

    return s;
}

void ExtensionInspector::helpMarker(const std::string& desc)
{
    if (desc.empty()) return;
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(desc.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

namespace {
const ux3d::gltf::asobo::PropertySpec* findAsoboPropertySpec(const ux3d::gltf::asobo::ExtensionSpec* extSpec,
                                                             std::string_view propKey)
{
    if (extSpec == nullptr) {
        return nullptr;
    }
    for (const auto& prop : extSpec->properties) {
        if (prop.name == propKey) {
            return &prop;
        }
    }
    return nullptr;
}

nlohmann::json asoboDefaultToJson(const ux3d::gltf::asobo::PropertySpec& prop)
{
    using namespace ux3d::gltf::asobo;
    return std::visit([](const auto& val) -> nlohmann::json {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return nullptr;
        } else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, double> || std::is_same_v<T, std::string>) {
            return val;
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            nlohmann::json arr = nlohmann::json::array();
            for (double d : val) {
                arr.push_back(d);
            }
            return arr;
        } else {
            return nullptr;
        }
    }, prop.defaultValue);
}

bool isColorPropertyKey(std::string_view propKey, const ux3d::imgui::ExtensionSchemaProperty* propDef)
{
    if (propKey.find("Color") != std::string_view::npos || propKey.find("color") != std::string_view::npos) {
        return true;
    }
    if (propDef != nullptr) {
        if (propDef->description.find("color") != std::string::npos ||
            propDef->description.find("Color") != std::string::npos ||
            propDef->description.find("RGB") != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool isIntegerProperty(std::string_view propKey, const ux3d::imgui::ExtensionSchemaProperty* propDef)
{
    if (propDef != nullptr && propDef->type == "integer") {
        return true;
    }
    return propKey == "drawOrderOffset" || propKey == "index" || propKey == "texCoord";
}
} // namespace

bool ExtensionInspector::setExtensionParameter(ux3d::gltf::GltfObject* object,
                                               const std::string& extName,
                                               const std::string& paramName,
                                               const nlohmann::json& paramValue,
                                               SceneRevisions& revisions)
{
    if (object == nullptr || extName.empty() || paramName.empty()) {
        return false;
    }

    const auto& unknownExts = object->getUnknownExtensions();
    auto it = unknownExts.find(extName.c_str());
    if (it == unknownExts.end()) {
        return false;
    }

    nlohmann::json extJson;
    try {
        extJson = nlohmann::json::parse(it->second.c_str());
    } catch (...) {
        return false;
    }
    if (!extJson.is_object()) {
        return false;
    }

    // Vendor edits may replace existing values, never introduce keys. Typed ASOBO
    // defaults retain their established setter path.
    if (!ux3d::gltf::asobo::isAsoboExtension(extName) && !extJson.contains(paramName)) return false;
    // UV offset aliases share one control. Prefer the exporter key when both exist;
    // preserve an alias-only file's spelling when editing it.
    std::string writeKey = paramName;
    if (extName == "ASOBO_material_UV_options") {
        if (const auto* spec = ux3d::gltf::asobo::findExtensionSpec(extName)) {
            if (const auto* property = spec->findProperty(paramName)) {
                writeKey = std::string(property->name);
                if (!extJson.contains(writeKey) && !property->alias.empty()
                    && extJson.contains(std::string(property->alias))) writeKey = std::string(property->alias);
            }
        }
    }
    if (extJson.contains(writeKey) && extJson[writeKey] == paramValue) return false;
    extJson[writeKey] = paramValue;
    object->setUnknownExtension(extName.c_str(), extJson.dump().c_str());
    if (dynamic_cast<ux3d::gltf::Material*>(object) != nullptr) revisions.mark(SceneChange::Material);
    revisions.mark(SceneChange::Extension);
    return true;
}

bool ExtensionInspector::setMaterialAsoboParameter(ux3d::gltf::Gltf& gltfModel,
                                                   const std::string& materialName,
                                                   const std::string& extName,
                                                   const std::string& paramName,
                                                   const std::string& rawValue,
                                                   SceneRevisions& revisions)
{
    loadAllSchemas();

    ux3d::gltf::GltfObject* targetObject = nullptr;
    if (materialName == "$asset") {
        targetObject = &gltfModel.getAsset();
    } else {
        for (auto& mat : gltfModel.getElements<ux3d::gltf::Material>()) {
            if (mat.hasName() && mat.getName() == materialName.c_str()) {
                targetObject = &mat;
                break;
            }
        }
        if (targetObject == nullptr) {
            for (auto& mat : gltfModel.getElements<ux3d::gltf::Material>()) {
                if (std::to_string(mat.m_ID) == materialName) {
                    targetObject = &mat;
                    break;
                }
            }
        }
    }
    if (targetObject == nullptr || !ux3d::gltf::asobo::isAsoboExtension(extName)) {
        return false;
    }

    const auto* extSpec = ux3d::gltf::asobo::findExtensionSpec(extName);
    const auto* propSpec = findAsoboPropertySpec(extSpec, paramName);
    const auto* schema = getSchema(extName);
    const ExtensionSchemaProperty* propDef = nullptr;
    if (schema != nullptr) {
        auto it = schema->properties.find(paramName);
        if (it != schema->properties.end()) {
            propDef = &it->second;
        }
    }

    nlohmann::json parsedValue;
    if (propSpec != nullptr) {
        using ux3d::gltf::asobo::ValueType;
        switch (propSpec->type) {
        case ValueType::Boolean:
            if (rawValue == "true" || rawValue == "1" || rawValue == "on") {
                parsedValue = true;
            } else if (rawValue == "false" || rawValue == "0" || rawValue == "off") {
                parsedValue = false;
            } else {
                return false;
            }
            break;
        case ValueType::Number:
            try {
                if (isIntegerProperty(paramName, propDef)) {
                    parsedValue = std::stoll(rawValue);
                } else {
                    parsedValue = std::stod(rawValue);
                }
            } catch (...) {
                return false;
            }
            break;
        case ValueType::String:
            parsedValue = rawValue;
            break;
        case ValueType::NumberArray:
            try {
                if (!rawValue.empty() && rawValue.front() == '[') {
                    parsedValue = nlohmann::json::parse(rawValue);
                } else {
                    nlohmann::json arr = nlohmann::json::array();
                    std::stringstream ss(rawValue);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        arr.push_back(std::stod(item));
                    }
                    parsedValue = std::move(arr);
                }
                if (!parsedValue.is_array()) {
                    return false;
                }
            } catch (...) {
                return false;
            }
            break;
        default:
            return false;
        }
    } else {
        try {
            parsedValue = nlohmann::json::parse(rawValue);
        } catch (...) {
            parsedValue = rawValue;
        }
    }

    return setExtensionParameter(targetObject, extName, paramName, parsedValue, revisions);
}

bool ExtensionInspector::renderExtensions(ux3d::gltf::GltfObject* object,
                                          ux3d::gltf::Gltf& gltfModel,
                                          SceneRevisions& revisions)
{
    if (object == nullptr) {
        return false;
    }

    loadAllSchemas();

    auto unknownExts = object->getUnknownExtensions();
    if (unknownExts.empty()) {
        return false;
    }

    bool anyModified = false;

    for (const auto& [extNameStr, jsonStr] : unknownExts) {
        const std::string extName = extNameStr.c_str();
        ImGui::PushID(extName.c_str());

        nlohmann::json extJson;
        bool parseOk = true;
        try {
            extJson = nlohmann::json::parse(jsonStr.c_str());
        } catch (...) {
            parseOk = false;
        }

        const ExtensionSchemaInfo* schema = getSchema(extName);
        std::string label = formatExtensionLabel(extName);

        bool headerOpen = ImGui::TreeNodeEx(label.c_str(),
                                           ImGuiTreeNodeFlags_DefaultOpen |
                                           ImGuiTreeNodeFlags_Framed |
                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                           ImGuiTreeNodeFlags_AllowOverlap);

        namespace asobo = ux3d::gltf::asobo;
        bool hasViewportSwitch = false;
        if (asobo::isAsoboExtension(extName) || extName == "MSFT_texture_dds") {
            const auto info = asobo::supportOf(extName);
            const char* status = asobo::supportLabel(info.status);
            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            switch (info.status) {
                case asobo::Support::Rendered: color = ImVec4(0.13f, 0.67f, 0.42f, 1.0f); break;
                case asobo::Support::Partial: color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); break;
                case asobo::Support::ViewerSetting:
                case asobo::Support::Metadata: color = ImVec4(0.55f, 0.65f, 0.75f, 1.0f); break;
                default: break;
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::TextColored(color, "[%s]", status);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(status);
                if (!info.phase.empty()) ImGui::Text("Phase: %.*s", static_cast<int>(info.phase.size()), info.phase.data());
                if (!info.note.empty()) ImGui::TextUnformatted(info.note.data(), info.note.data() + info.note.size());
                ImGui::EndTooltip();
            }

            if (asobo::isAsoboExtension(extName) &&
                (info.status == asobo::Support::Rendered || info.status == asobo::Support::Partial) &&
                info.viewportSwitch) {
                hasViewportSwitch = true;
            }
        }

        if (hasViewportSwitch) {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x * 2.0f);
            // The switch is the app setting of File > Settings > glTF Extensions (every viewport, saved).
            auto& extensionSettings = GltfExtensionSettings::instance();
            bool applied = extensionSettings.enabled(extName);
            const bool switchEnabled = extensionSettings.enabled(GltfExtensionSettings::vendorOf(extName));

            if (!switchEnabled) {
                ImGui::BeginDisabled();
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
            if (ImGui::Checkbox("##viewportToggle", &applied)) {
                extensionSettings.setEnabled(extName, applied, true);
                revisions.mark(SceneChange::ViewportSettings);
            }
            ImGui::PopStyleVar();
            if (!switchEnabled) {
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Display only: applies or ignores this extension in every viewport (app setting, saved;\nalso in File > Settings > glTF Extensions). The file is unchanged.");
            }
        }

        if (headerOpen) {
            if (schema != nullptr && !schema->description.empty()) {
                ImGui::TextDisabled("%s", schema->description.c_str());
                ImGui::Separator();
            }

            if (!parseOk) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Malformed JSON syntax");
                ImGui::TextWrapped("%s", jsonStr.c_str());
            } else {
                if (renderSingleExtension(object, extName, extJson, schema, gltfModel, revisions)) {
                    anyModified = true;
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    return anyModified;
}

bool ExtensionInspector::renderSingleExtension(ux3d::gltf::GltfObject* object,
                                               const std::string& extName,
                                               nlohmann::json& extJson,
                                               const ExtensionSchemaInfo* schema,
                                               ux3d::gltf::Gltf& gltfModel,
                                               SceneRevisions& revisions)
{
    if (!extJson.is_object()) {
        ImGui::TextDisabled("Value is not a JSON object: %s", extJson.dump().c_str());
        return false;
    }

    const bool isAsobo = ux3d::gltf::asobo::isAsoboExtension(extName);
    const auto* extSpec = isAsobo ? ux3d::gltf::asobo::findExtensionSpec(extName) : nullptr;

    if (!isAsobo) {
        if (extJson.empty()) {
            ImGui::TextDisabled("(No parameters)");
            return false;
        }

        bool modified = false;
        // Work on a copy and submit each changed top-level value through the same
        // setter as ASOBO. Nested widgets cannot add or remove keys either.
        auto editValue = [&](auto&& self, const std::string& key, nlohmann::json& value,
                             const ExtensionSchemaProperty* definition) -> bool {
            const bool numericArray = value.is_array() && value.size() >= 2 && value.size() <= 4
                && std::all_of(value.begin(), value.end(), [](const auto& v) { return v.is_number(); });
            if (value.is_boolean() || value.is_number() || numericArray
                || (value.is_string() && definition != nullptr && !definition->enumValues.empty())) {
                return renderAsoboPropertyWidget(extName, key, value, nullptr, definition);
            }
            if (value.is_object() || value.is_array()) {
                bool changed = false;
                if (ImGui::TreeNode(key.c_str())) {
                    for (auto& [child, v] : value.items()) {
                        changed |= self(self, key + "." + child, v, nullptr);
                    }
                    ImGui::TreePop();
                }
                return changed;
            }
            renderPropertyReadOnly(key, value, definition, gltfModel);
            return false;
        };
        std::vector<std::string> keys;
        if (schema != nullptr) {
            for (const auto& key : schema->orderedPropertyKeys) {
                if (extJson.contains(key)) keys.push_back(key);
            }
        }
        for (const auto& [key, value] : extJson.items()) {
            if (key != "extensions" && key != "extras"
                && std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
        }
        for (const auto& key : keys) {
            const ExtensionSchemaProperty* definition = nullptr;
            if (schema != nullptr) {
                const auto it = schema->properties.find(key);
                if (it != schema->properties.end()) definition = &it->second;
            }
            auto value = extJson.at(key);
            if (editValue(editValue, key, value, definition)
                && setExtensionParameter(object, extName, key, value, revisions)) {
                extJson[key] = value;
                modified = true;
            }
        }
        return modified;
    }

    // ASOBO extension: make every typed parameter of the ASOBO model editable.
    // Build the ordered list of property keys from the schema + AsoboExtensionSpecs + extJson.
    std::vector<std::string> orderedKeys;
    std::unordered_set<std::string> seenKeys;
    auto addKey = [&](std::string key) {
        if (key == "extensions" || key == "extras") {
            return;
        }
        if (extName == "ASOBO_material_UV_options" && extSpec != nullptr) {
            if (const auto* property = extSpec->findProperty(key)) key = std::string(property->name);
        }
        if (seenKeys.insert(key).second) {
            orderedKeys.push_back(key);
        }
    };

    if (schema != nullptr) {
        for (const auto& k : schema->orderedPropertyKeys) {
            addKey(k);
        }
    }
    if (extSpec != nullptr) {
        for (const auto& p : extSpec->properties) {
            addKey(std::string(p.name));
        }
    }
    for (const auto& [k, _] : extJson.items()) {
        addKey(k);
    }

    bool anyRendered = false;
    bool modified = false;

    for (const auto& propKey : orderedKeys) {
        const auto* propSpec = findAsoboPropertySpec(extSpec, propKey);
        const ExtensionSchemaProperty* propDef = nullptr;
        if (schema != nullptr) {
            auto it = schema->properties.find(propKey);
            if (it != schema->properties.end()) {
                propDef = &it->second;
            }
        }

        const bool isTex = (propSpec != nullptr && propSpec->type == ux3d::gltf::asobo::ValueType::Texture) ||
                           (propDef != nullptr && propDef->isTexture) ||
                           propKey.find("Texture") != std::string::npos ||
                           (propKey.find("Mask") != std::string::npos &&
                            (propSpec == nullptr || propSpec->type == ux3d::gltf::asobo::ValueType::Texture));

        if (isTex) {
            if (extJson.contains(propKey)) {
                anyRendered = true;
                renderTextureReadOnly(propKey, extJson.at(propKey), propDef ? propDef->description : "", gltfModel);
            }
            continue;
        }

        nlohmann::json currentVal;
        if (extJson.contains(propKey)) {
            currentVal = extJson.at(propKey);
        } else if (propSpec != nullptr && !propSpec->alias.empty()
                   && extJson.contains(std::string(propSpec->alias))) {
            currentVal = extJson.at(std::string(propSpec->alias));
        } else if (propSpec != nullptr) {
            currentVal = asoboDefaultToJson(*propSpec);
            if (currentVal.is_null() && propDef != nullptr && !propDef->defaultValue.is_null()) {
                currentVal = propDef->defaultValue;
            }
        } else if (propDef != nullptr && !propDef->defaultValue.is_null()) {
            currentVal = propDef->defaultValue;
        }

        if (currentVal.is_null()) {
            continue;
        }

        anyRendered = true;
        if (renderAsoboPropertyWidget(extName, propKey, currentVal, propSpec, propDef)) {
            if (setExtensionParameter(object, extName, propKey, currentVal, revisions)) {
                extJson[propKey] = currentVal;
                modified = true;
            }
        }
    }

    if (!anyRendered) {
        ImGui::TextDisabled("(No parameters)");
    }

    return modified;
}

bool ExtensionInspector::renderAsoboPropertyWidget(const std::string& extName,
                                                   const std::string& propKey,
                                                   nlohmann::json& propVal,
                                                   const ux3d::gltf::asobo::PropertySpec* propSpec,
                                                   const ExtensionSchemaProperty* propDef)
{
    ImGui::PushID(propKey.c_str());
    bool changed = false;
    const std::string desc = propDef ? propDef->description : "";
    const std::string probeId = (ux3d::gltf::asobo::isAsoboExtension(extName) ? "asobo." : "ext.") + extName + "." + propKey;

    using ux3d::gltf::asobo::ValueType;
    const ValueType vtype = propSpec != nullptr ? propSpec->type : (
        propVal.is_boolean() ? ValueType::Boolean :
        propVal.is_number() ? ValueType::Number :
        propVal.is_string() ? ValueType::String :
        propVal.is_array() ? ValueType::NumberArray : ValueType::Texture);

    if (vtype == ValueType::Boolean && propVal.is_boolean()) {
        bool v = propVal.get<bool>();
        if (ImGui::Checkbox(propKey.c_str(), &v)) {
            propVal = v;
            changed = true;
        }
        UiProbe::item(probeId.c_str(), 0);
        helpMarker(desc);
        ImGui::PopID();
        return changed;
    }

    if (vtype == ValueType::Number && propVal.is_number()) {
        const bool hasMin = (propSpec != nullptr && propSpec->minimum.has_value()) || (propDef != nullptr && propDef->hasMinimum);
        const bool hasMax = (propSpec != nullptr && propSpec->maximum.has_value()) || (propDef != nullptr && propDef->hasMaximum);
        const double minD = (propSpec != nullptr && propSpec->minimum.has_value()) ? *propSpec->minimum
                          : (propDef != nullptr && propDef->hasMinimum) ? propDef->minimum : 0.0;
        const double maxD = (propSpec != nullptr && propSpec->maximum.has_value()) ? *propSpec->maximum
                          : (propDef != nullptr && propDef->hasMaximum) ? propDef->maximum : 0.0;

        ImGui::PushFont(getMonoFont());
        if (isIntegerProperty(propKey, propDef)) {
            int v = static_cast<int>(std::lround(propVal.get<double>()));
            int vMin = hasMin ? static_cast<int>(std::lround(minD)) : (propKey == "drawOrderOffset" ? -999 : 0);
            int vMax = hasMax ? static_cast<int>(std::lround(maxD)) : (propKey == "drawOrderOffset" ? 999 : 0);
            if (ImGui::DragInt(propKey.c_str(), &v, 1.0f, vMin, vMax)) {
                if (vMin < vMax) {
                    v = std::clamp(v, vMin, vMax);
                }
                propVal = v;
                changed = true;
            }
        } else {
            float v = static_cast<float>(propVal.get<double>());
            float vMin = 0.0f;
            float vMax = 0.0f;
            if (hasMin && hasMax) {
                vMin = static_cast<float>(minD);
                vMax = static_cast<float>(maxD);
            } else if (hasMin) {
                vMin = static_cast<float>(minD);
                vMax = FLT_MAX;
            } else if (hasMax) {
                vMin = -FLT_MAX;
                vMax = static_cast<float>(maxD);
            }
            float speed = 0.01f;
            if (hasMin && hasMax && vMax > vMin) {
                speed = std::max(0.001f, (vMax - vMin) * 0.005f);
            } else if (propKey == "glassWidth") {
                speed = 0.002f;
            }
            const char* fmt = (propKey == "glassWidth") ? "%.4f" : "%.3f";
            if (ImGui::DragFloat(propKey.c_str(), &v, speed, vMin, vMax, fmt)) {
                if (hasMin && v < vMin) v = vMin;
                if (hasMax && v > vMax) v = vMax;
                propVal = static_cast<double>(v);
                changed = true;
            }
        }
        ImGui::PopFont();
        UiProbe::item(probeId.c_str(), 0);
        helpMarker(desc);
        ImGui::PopID();
        return changed;
    }

    if (vtype == ValueType::String && propVal.is_string()) {
        std::string cur = propVal.get<std::string>();
        std::vector<std::string> enums;
        if (propDef != nullptr && !propDef->enumValues.empty()) {
            enums = propDef->enumValues;
        } else if (extName == "ASOBO_normal_map_convention" && propKey == "tangent_space_convention") {
            enums = { "DirectX", "OpenGL" };
        } else if (extName == "ASOBO_material_geometry_decal" && propKey == "mode") {
            enums = { "default", "frosted", "blendMasked" };
        }

        if (!enums.empty()) {
            if (ImGui::BeginCombo(propKey.c_str(), cur.c_str())) {
                for (const auto& opt : enums) {
                    bool selected = (cur == opt);
                    if (ImGui::Selectable(opt.c_str(), selected)) {
                        propVal = opt;
                        changed = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            char buf[256] = {};
            std::strncpy(buf, cur.c_str(), sizeof(buf) - 1);
            if (ImGui::InputText(propKey.c_str(), buf, sizeof(buf))) {
                propVal = std::string(buf);
                changed = true;
            }
        }
        UiProbe::item(probeId.c_str(), 0);
        helpMarker(desc);
        ImGui::PopID();
        return changed;
    }

    if (vtype == ValueType::NumberArray && propVal.is_array()) {
        const size_t n = propVal.size();
        const bool isColor = isColorPropertyKey(propKey, propDef);
        if (isColor && (n == 3 || n == 4)) {
            float col[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            for (size_t i = 0; i < n; ++i) {
                if (propVal[i].is_number()) {
                    col[i] = static_cast<float>(propVal[i].get<double>());
                }
            }
            bool colChanged = (n == 3)
                ? ImGui::ColorEdit3(propKey.c_str(), col)
                : ImGui::ColorEdit4(propKey.c_str(), col);
            if (colChanged) {
                nlohmann::json arr = nlohmann::json::array();
                for (size_t i = 0; i < n; ++i) {
                    arr.push_back(static_cast<double>(col[i]));
                }
                propVal = std::move(arr);
                changed = true;
            }
        } else if (n >= 2 && n <= 4) {
            float vals[4] = {};
            for (size_t i = 0; i < n; ++i) {
                if (propVal[i].is_number()) {
                    vals[i] = static_cast<float>(propVal[i].get<double>());
                }
            }
            ImGui::PushFont(getMonoFont());
            bool arrChanged = false;
            if (n == 2) arrChanged = ImGui::DragFloat2(propKey.c_str(), vals, 0.01f, 0.0f, 0.0f, "%.3f");
            else if (n == 3) arrChanged = ImGui::DragFloat3(propKey.c_str(), vals, 0.01f, 0.0f, 0.0f, "%.3f");
            else if (n == 4) arrChanged = ImGui::DragFloat4(propKey.c_str(), vals, 0.01f, 0.0f, 0.0f, "%.3f");
            ImGui::PopFont();
            if (arrChanged) {
                nlohmann::json arr = nlohmann::json::array();
                for (size_t i = 0; i < n; ++i) {
                    arr.push_back(static_cast<double>(vals[i]));
                }
                propVal = std::move(arr);
                changed = true;
            }
        }
        UiProbe::item(probeId.c_str(), 0);
        helpMarker(desc);
        ImGui::PopID();
        return changed;
    }

    ImGui::PopID();
    return false;
}

void ExtensionInspector::renderPropertyReadOnly(const std::string& propKey,
                                                const nlohmann::json& propVal,
                                                const ExtensionSchemaProperty* propDef,
                                                ux3d::gltf::Gltf& gltfModel)
{
    std::string label = propKey;
    std::string desc = propDef ? propDef->description : "";

    // Texture property
    if ((propDef && propDef->isTexture) ||
        propKey.find("Texture") != std::string::npos ||
        propKey.find("Mask") != std::string::npos) {
        renderTextureReadOnly(label, propVal, desc, gltfModel);
        return;
    }

    // Boolean
    if (propVal.is_boolean()) {
        ImGui::Text("%s: %s", label.c_str(), propVal.get<bool>() ? "true" : "false");
        helpMarker(desc);
        return;
    }

    // Number
    if (propVal.is_number()) {
        char buf[64];
        if (propDef && propDef->type == "integer") {
            std::snprintf(buf, sizeof(buf), "%lld", propVal.get<long long>());
        } else if (propVal.is_number_integer() && (propDef == nullptr || propDef->type != "number")) {
            std::snprintf(buf, sizeof(buf), "%lld", propVal.get<long long>());
        } else {
            std::snprintf(buf, sizeof(buf), "%.3f", propVal.get<double>());
        }
        ImGui::Text("%s: %s", label.c_str(), buf);
        helpMarker(desc);
        return;
    }

    // String
    if (propVal.is_string()) {
        ImGui::Text("%s: %s", label.c_str(), propVal.get<std::string>().c_str());
        helpMarker(desc);
        return;
    }

    // Array
    if (propVal.is_array()) {
        bool allPrimitives = true;
        for (const auto& el : propVal) {
            if (el.is_object() || el.is_array()) {
                allPrimitives = false;
                break;
            }
        }

        if (allPrimitives) {
            std::ostringstream ss;
            ss << "[";
            for (size_t i = 0; i < propVal.size(); ++i) {
                if (i > 0) ss << ", ";
                const auto& el = propVal[i];
                if (el.is_number()) {
                    char nbuf[32];
                    if (el.is_number_float() || (propDef && propDef->type == "number")) {
                        std::snprintf(nbuf, sizeof(nbuf), "%.3f", el.get<double>());
                    } else {
                        std::snprintf(nbuf, sizeof(nbuf), "%lld", el.get<long long>());
                    }
                    ss << nbuf;
                } else if (el.is_boolean()) {
                    ss << (el.get<bool>() ? "true" : "false");
                } else if (el.is_string()) {
                    ss << "\"" << el.get<std::string>() << "\"";
                } else {
                    ss << el.dump();
                }
            }
            ss << "]";
            ImGui::Text("%s: %s", label.c_str(), ss.str().c_str());
            helpMarker(desc);
        } else {
            if (ImGui::TreeNode(label.c_str())) {
                helpMarker(desc);
                for (size_t i = 0; i < propVal.size(); ++i) {
                    std::string itemKey = "[" + std::to_string(i) + "]";
                    renderPropertyReadOnly(itemKey, propVal[i], nullptr, gltfModel);
                }
                ImGui::TreePop();
            }
        }
        return;
    }

    // Generic Object
    if (propVal.is_object()) {
        if (ImGui::TreeNode(label.c_str())) {
            helpMarker(desc);
            for (const auto& [subK, subV] : propVal.items()) {
                renderPropertyReadOnly(subK, subV, nullptr, gltfModel);
            }
            ImGui::TreePop();
        }
        return;
    }

    // Null
    if (propVal.is_null()) {
        ImGui::Text("%s: null", label.c_str());
        helpMarker(desc);
        return;
    }

    ImGui::Text("%s: %s", label.c_str(), propVal.dump().c_str());
    helpMarker(desc);
}

void ExtensionInspector::renderTextureReadOnly(const std::string& label,
                                               const nlohmann::json& texVal,
                                               const std::string& tooltip,
                                               ux3d::gltf::Gltf& gltfModel)
{
    std::string desc = tooltip;
    bool hasTex = false;
    uint32_t currentTexId = 0;
    int texCoord = -1;

    if (texVal.is_object()) {
        if (texVal.contains("index") && texVal["index"].is_number()) {
            hasTex = true;
            currentTexId = texVal["index"].get<uint32_t>();
        }
        if (texVal.contains("texCoord") && texVal["texCoord"].is_number()) {
            texCoord = texVal["texCoord"].get<int>();
        }
    } else if (texVal.is_number()) {
        hasTex = true;
        currentTexId = texVal.get<uint32_t>();
    }

    if (!hasTex) {
        ImGui::Text("%s: None", label.c_str());
        helpMarker(desc);
        return;
    }

    const ux3d::gltf::Texture* foundTex = nullptr;
    for (const auto& t : gltfModel.getElements<ux3d::gltf::Texture>()) {
        if (t.m_ID == currentTexId) {
            foundTex = &t;
            break;
        }
    }

    std::string texDisplay;
    if (foundTex != nullptr) {
        std::string nameOrUri;
        if (foundTex->getSource() != nullptr && !foundTex->getSource()->getUri().empty()) {
            nameOrUri = foundTex->getSource()->getUri().c_str();
        } else if (foundTex->getSource() != nullptr && foundTex->getSource()->hasName() && !foundTex->getSource()->getName().empty()) {
            nameOrUri = foundTex->getSource()->getName().c_str();
        } else {
            nameOrUri = getTextureDisplayName(foundTex);
        }
        texDisplay = nameOrUri + " [Texture " + std::to_string(currentTexId) + "]";
    } else {
        texDisplay = "[Texture " + std::to_string(currentTexId) + "]";
    }

    if (texCoord >= 0) {
        texDisplay += " (UV Set " + std::to_string(texCoord) + ")";
    }

    ImGui::Text("%s: %s", label.c_str(), texDisplay.c_str());
    helpMarker(desc);
}

} // namespace ux3d::imgui
