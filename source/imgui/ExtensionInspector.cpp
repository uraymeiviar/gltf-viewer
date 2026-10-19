// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ExtensionInspector.h"
#include "imgui/EmbeddedVFS.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "gltf/Texture.h"
#include "gltf/Image.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <sstream>

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
        // Replace underscores with spaces
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
                                          bool& isSceneDirty)
{
    if (object == nullptr) {
        return false;
    }

    loadAllSchemas();

    auto unknownExts = object->getUnknownExtensions();
    if (unknownExts.empty()) {
        return false;
    }

    bool modified = false;
    std::string extToRemove = "";

    for (auto& [extName, jsonStr] : unknownExts) {
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
                                           ImGuiTreeNodeFlags_SpanAvailWidth);

        // Remove extension button on right click or small trash icon
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Remove Extension")) {
                extToRemove = extName;
            }
            ImGui::EndPopup();
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
                if (renderSingleExtension(extName, extJson, schema, gltfModel)) {
                    object->setUnknownExtension(extName, extJson.dump(4));
                    modified = true;
                    isSceneDirty = true;
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    if (!extToRemove.empty()) {
        object->removeUnknownExtension(extToRemove);
        modified = true;
        isSceneDirty = true;
    }

    return modified;
}

bool ExtensionInspector::renderSingleExtension(const std::string& extName,
                                               nlohmann::json& extJson,
                                               const ExtensionSchemaInfo* schema,
                                               ux3d::gltf::Gltf& gltfModel)
{
    bool modified = false;

    if (!extJson.is_object()) {
        ImGui::TextDisabled("Value is not a JSON object: %s", extJson.dump().c_str());
        return false;
    }

    std::unordered_set<std::string> renderedKeys;

    // 1. Render properties defined by the schema in order
    if (schema != nullptr) {
        for (const auto& propKey : schema->orderedPropertyKeys) {
            auto itProp = schema->properties.find(propKey);
            if (itProp == schema->properties.end()) continue;

            const auto& propDef = itProp->second;
            renderedKeys.insert(propKey);

            // If property doesn't exist yet in the JSON, provide default or initialize if edited
            nlohmann::json currentVal;
            if (extJson.contains(propKey)) {
                currentVal = extJson[propKey];
            } else if (!propDef.defaultValue.is_null()) {
                currentVal = propDef.defaultValue;
            } else if (propDef.isTexture) {
                currentVal = nlohmann::json::object();
            } else if (propDef.type == "boolean") {
                currentVal = false;
            } else if (propDef.type == "number") {
                currentVal = 0.0;
            } else if (propDef.type == "integer") {
                currentVal = 0;
            } else if (propDef.type == "string") {
                currentVal = propDef.enumValues.empty() ? "" : propDef.enumValues.front();
            }

            if (renderPropertyWidget(propKey, currentVal, &propDef, gltfModel)) {
                extJson[propKey] = currentVal;
                modified = true;
            }
        }
    }

    // 2. Render any remaining extra properties not described in the schema
    for (auto& [k, v] : extJson.items()) {
        if (renderedKeys.count(k) != 0 || k == "extensions" || k == "extras") {
            continue;
        }

        if (renderGenericValue(k, v, gltfModel)) {
            modified = true;
        }
    }

    return modified;
}

bool ExtensionInspector::renderPropertyWidget(const std::string& propKey,
                                              nlohmann::json& propVal,
                                              const ExtensionSchemaProperty* propDef,
                                              ux3d::gltf::Gltf& gltfModel)
{
    bool modified = false;
    ImGui::PushID(propKey.c_str());

    std::string label = propKey;
    std::string desc = propDef ? propDef->description : "";

    // Texture property
    if (propDef && propDef->isTexture) {
        if (!propVal.is_object()) {
            propVal = nlohmann::json::object();
        }
        if (renderTexturePicker(label.c_str(), propVal, desc, gltfModel)) {
            modified = true;
        }
        ImGui::PopID();
        return modified;
    }

    // String with Enum values
    if (propDef && !propDef->enumValues.empty()) {
        std::string currentVal = propVal.is_string() ? propVal.get<std::string>() : (propDef->enumValues.empty() ? "" : propDef->enumValues.front());
        if (ImGui::BeginCombo(label.c_str(), currentVal.c_str())) {
            for (const auto& opt : propDef->enumValues) {
                bool isSel = (opt == currentVal);
                if (ImGui::Selectable(opt.c_str(), isSel)) {
                    propVal = opt;
                    modified = true;
                }
                if (isSel) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        helpMarker(desc);
        ImGui::PopID();
        return modified;
    }

    // Boolean
    if ((propDef && propDef->type == "boolean") || propVal.is_boolean()) {
        bool b = propVal.is_boolean() ? propVal.get<bool>() : false;
        if (ImGui::Checkbox(label.c_str(), &b)) {
            propVal = b;
            modified = true;
        }
        helpMarker(desc);
        ImGui::PopID();
        return modified;
    }

    // Integer
    if ((propDef && propDef->type == "integer") || propVal.is_number_integer()) {
        int iVal = propVal.is_number() ? propVal.get<int>() : 0;
        int minVal = (propDef && propDef->hasMinimum) ? static_cast<int>(propDef->minimum) : -10000;
        int maxVal = (propDef && propDef->hasMaximum) ? static_cast<int>(propDef->maximum) : 10000;

        if (propDef && propDef->hasMinimum && propDef->hasMaximum) {
            if (ImGui::SliderInt(label.c_str(), &iVal, minVal, maxVal)) {
                propVal = iVal;
                modified = true;
            }
        } else {
            if (ImGui::DragInt(label.c_str(), &iVal, 1.0f, minVal, maxVal)) {
                propVal = iVal;
                modified = true;
            }
        }
        helpMarker(desc);
        ImGui::PopID();
        return modified;
    }

    // Number / Float
    if ((propDef && propDef->type == "number") || propVal.is_number()) {
        float fVal = propVal.is_number() ? propVal.get<float>() : 0.0f;
        float minVal = (propDef && propDef->hasMinimum) ? static_cast<float>(propDef->minimum) : 0.0f;
        float maxVal = (propDef && propDef->hasMaximum) ? static_cast<float>(propDef->maximum) : 1.0f;

        if (propDef && propDef->hasMinimum && propDef->hasMaximum && (maxVal - minVal) <= 10.0f) {
            if (ImGui::SliderFloat(label.c_str(), &fVal, minVal, maxVal, "%.3f")) {
                propVal = static_cast<double>(fVal);
                modified = true;
            }
        } else {
            float step = (propKey.find("Scale") != std::string::npos || propKey.find("Tiling") != std::string::npos) ? 0.1f : 0.01f;
            if (ImGui::DragFloat(label.c_str(), &fVal, step, 0.0f, 0.0f, "%.3f")) {
                propVal = static_cast<double>(fVal);
                modified = true;
            }
        }
        helpMarker(desc);
        ImGui::PopID();
        return modified;
    }

    // String
    if ((propDef && propDef->type == "string") || propVal.is_string()) {
        std::string sVal = propVal.is_string() ? propVal.get<std::string>() : "";
        char buf[256];
        std::strncpy(buf, sVal.c_str(), sizeof(buf));
        buf[sizeof(buf) - 1] = '\0';

        if (ImGui::InputText(label.c_str(), buf, sizeof(buf))) {
            propVal = std::string(buf);
            modified = true;
        }
        helpMarker(desc);
        ImGui::PopID();
        return modified;
    }

    // Array (e.g. Color or Vector)
    if (propVal.is_array()) {
        size_t arrSize = propVal.size();
        bool allNumbers = true;
        for (const auto& el : propVal) {
            if (!el.is_number()) { allNumbers = false; break; }
        }

        if (allNumbers && arrSize == 3 && propKey.find("Color") != std::string::npos) {
            float col[3] = { propVal[0].get<float>(), propVal[1].get<float>(), propVal[2].get<float>() };
            if (ImGui::ColorEdit3(label.c_str(), col)) {
                propVal[0] = col[0];
                propVal[1] = col[1];
                propVal[2] = col[2];
                modified = true;
            }
            helpMarker(desc);
            ImGui::PopID();
            return modified;
        }
        if (allNumbers && arrSize == 4 && propKey.find("Color") != std::string::npos) {
            float col[4] = { propVal[0].get<float>(), propVal[1].get<float>(), propVal[2].get<float>(), propVal[3].get<float>() };
            if (ImGui::ColorEdit4(label.c_str(), col)) {
                propVal[0] = col[0];
                propVal[1] = col[1];
                propVal[2] = col[2];
                propVal[3] = col[3];
                modified = true;
            }
            helpMarker(desc);
            ImGui::PopID();
            return modified;
        }
        if (allNumbers && (arrSize >= 2 && arrSize <= 4)) {
            float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            for (size_t i = 0; i < arrSize; ++i) v[i] = propVal[i].get<float>();
            bool changed = false;
            if (arrSize == 2) changed = ImGui::DragFloat2(label.c_str(), v, 0.01f);
            else if (arrSize == 3) changed = ImGui::DragFloat3(label.c_str(), v, 0.01f);
            else if (arrSize == 4) changed = ImGui::DragFloat4(label.c_str(), v, 0.01f);
            if (changed) {
                for (size_t i = 0; i < arrSize; ++i) propVal[i] = v[i];
                modified = true;
            }
            helpMarker(desc);
            ImGui::PopID();
            return modified;
        }

        // Generic array
        if (ImGui::TreeNode(label.c_str())) {
            helpMarker(desc);
            for (size_t i = 0; i < arrSize; ++i) {
                std::string itemKey = "[" + std::to_string(i) + "]";
                if (renderGenericValue(itemKey, propVal[i], gltfModel)) {
                    modified = true;
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        return modified;
    }

    // Generic Object
    if (propVal.is_object()) {
        if (ImGui::TreeNode(label.c_str())) {
            helpMarker(desc);
            for (auto& [subK, subV] : propVal.items()) {
                if (renderGenericValue(subK, subV, gltfModel)) {
                    modified = true;
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        return modified;
    }

    ImGui::PopID();
    return false;
}

bool ExtensionInspector::renderGenericValue(const std::string& key,
                                            nlohmann::json& val,
                                            ux3d::gltf::Gltf& gltfModel)
{
    return renderPropertyWidget(key, val, nullptr, gltfModel);
}

bool ExtensionInspector::renderTexturePicker(const char* label,
                                             nlohmann::json& texObj,
                                             const std::string& tooltip,
                                             ux3d::gltf::Gltf& gltfModel)
{
    bool modified = false;
    auto& textures = gltfModel.getElements<ux3d::gltf::Texture>();

    bool hasTex = texObj.contains("index") && texObj["index"].is_number();
    uint32_t currentTexId = hasTex ? texObj["index"].get<uint32_t>() : 0;

    std::string previewName = "None";
    if (hasTex) {
        for (auto& t : textures) {
            if (t.m_ID == currentTexId) {
                previewName = t.hasName() ? t.getName().c_str() : ("Texture " + std::to_string(t.m_ID));
                break;
            }
        }
    }

    if (ImGui::BeginCombo(label, previewName.c_str())) {
        bool isNoneSelected = !hasTex;
        if (ImGui::Selectable("None", isNoneSelected)) {
            texObj.erase("index");
            modified = true;
        }
        if (isNoneSelected) {
            ImGui::SetItemDefaultFocus();
        }

        for (auto& tex : textures) {
            bool isSelected = hasTex && (currentTexId == tex.m_ID);
            std::string itemLabel = tex.hasName() ? tex.getName().c_str() : ("Texture " + std::to_string(tex.m_ID));
            if (ImGui::Selectable(itemLabel.c_str(), isSelected)) {
                texObj["index"] = tex.m_ID;
                modified = true;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    helpMarker(tooltip);

    // If texture is selected, show UV coordinate set if user wants to customize
    if (hasTex) {
        ImGui::Indent(15.0f);
        int tc = texObj.value("texCoord", 0);
        std::string tcLabel = std::string(label) + " UV Set";
        if (ImGui::SliderInt(tcLabel.c_str(), &tc, 0, 3)) {
            texObj["texCoord"] = tc;
            modified = true;
        }
        ImGui::Unindent(15.0f);
    }

    return modified;
}

} // namespace ux3d::imgui
