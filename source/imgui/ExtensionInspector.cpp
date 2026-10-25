// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ExtensionInspector.h"
#include "imgui/EmbeddedVFS.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/TextureUtils.h"
#include "imgui/ViewportInstance.h"
#include "gltf/Texture.h"
#include "gltf/Image.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>
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

    for (const auto& [extName, jsonStr] : unknownExts) {
        ImGui::PushID(extName.c_str());

        nlohmann::json extJson;
        bool parseOk = true;
        try {
            extJson = nlohmann::json::parse(jsonStr);
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
            bool applied = true;
            bool switchEnabled = false;
            if (m_activeViewport != nullptr) {
                applied = std::find(m_activeViewport->settings.asobo.disabled.begin(),
                                    m_activeViewport->settings.asobo.disabled.end(),
                                    extName) == m_activeViewport->settings.asobo.disabled.end();
                switchEnabled = m_activeViewport->settings.asobo.enabled;
            }

            if (!switchEnabled) {
                ImGui::BeginDisabled();
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
            if (ImGui::Checkbox("##viewportToggle", &applied)) {
                if (m_activeViewport != nullptr) {
                    m_activeViewport->settings.asobo.setApplied(extName, applied);
                    revisions.mark(SceneChange::ViewportSettings);
                }
            }
            ImGui::PopStyleVar();
            if (!switchEnabled) {
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Display only: turns this extension off in the active viewport; the file is unchanged.");
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
                renderSingleExtension(extName, extJson, schema, gltfModel);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    return false;
}

void ExtensionInspector::renderSingleExtension(const std::string& extName,
                                               const nlohmann::json& extJson,
                                               const ExtensionSchemaInfo* schema,
                                               ux3d::gltf::Gltf& gltfModel)
{
    if (!extJson.is_object()) {
        ImGui::TextDisabled("Value is not a JSON object: %s", extJson.dump().c_str());
        return;
    }

    if (extJson.empty()) {
        ImGui::TextDisabled("(No parameters)");
        return;
    }

    std::unordered_set<std::string> renderedKeys;

    // 1. Render properties defined by the schema in order
    if (schema != nullptr) {
        for (const auto& propKey : schema->orderedPropertyKeys) {
            auto itProp = schema->properties.find(propKey);
            if (itProp == schema->properties.end()) continue;

            if (!extJson.contains(propKey)) {
                continue;
            }

            renderedKeys.insert(propKey);
            renderPropertyReadOnly(propKey, extJson.at(propKey), &itProp->second, gltfModel);
        }
    }

    // 2. Render any remaining extra properties not described in the schema
    for (const auto& [k, v] : extJson.items()) {
        if (renderedKeys.count(k) != 0 || k == "extensions" || k == "extras") {
            continue;
        }

        renderPropertyReadOnly(k, v, nullptr, gltfModel);
    }
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
