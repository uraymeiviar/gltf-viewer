// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "gltf/GltfObject.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace ux3d::imgui {

struct ExtensionSchemaProperty {
    std::string key;
    std::string type;          // "number", "integer", "boolean", "string", "object", "array", "texture"
    std::string description;
    std::vector<std::string> enumValues;
    double minimum = -1e9;
    double maximum = 1e9;
    bool hasMinimum = false;
    bool hasMaximum = false;
    nlohmann::json defaultValue;
    bool isTexture = false;
};

struct ExtensionSchemaInfo {
    std::string name;
    std::string title;
    std::string description;
    nlohmann::json schemaJson;
    std::unordered_map<std::string, ExtensionSchemaProperty> properties;
    std::vector<std::string> orderedPropertyKeys;
};

class ExtensionInspector {
public:
    static ExtensionInspector& instance();

    // Renders all vendor and unknown extensions found on a gltfObject.
    // Returns true if any property was modified.
    bool renderExtensions(ux3d::gltf::GltfObject* object,
                          ux3d::gltf::Gltf& gltfModel,
                          bool& isSceneDirty);

    // Checks if a schema is available for this extension name
    bool hasSchema(const std::string& extensionName);
    const ExtensionSchemaInfo* getSchema(const std::string& extensionName);

    // Returns a list of all registered known extension names (e.g. for "Add Extension" menu)
    const std::vector<std::string>& getAvailableExtensionNames();

    // Formats extension name for human-friendly display (e.g. "ASOBO_material_detail_map" -> "Detail Map (ASOBO)")
    static std::string formatExtensionLabel(const std::string& extensionName);

private:
    ExtensionInspector();
    ~ExtensionInspector() = default;

    ExtensionInspector(const ExtensionInspector&) = delete;
    ExtensionInspector& operator=(const ExtensionInspector&) = delete;

    void loadAllSchemas();
    bool parseSchemaFile(const std::string& vfsPath, const std::string& extName);

    bool renderSingleExtension(const std::string& extName,
                               nlohmann::json& extJson,
                               const ExtensionSchemaInfo* schema,
                               ux3d::gltf::Gltf& gltfModel);

    bool renderPropertyWidget(const std::string& propKey,
                              nlohmann::json& propVal,
                              const ExtensionSchemaProperty* propDef,
                              ux3d::gltf::Gltf& gltfModel);

    bool renderGenericValue(const std::string& key,
                            nlohmann::json& val,
                            ux3d::gltf::Gltf& gltfModel);

    bool renderTexturePicker(const char* label,
                             nlohmann::json& texObj,
                             const std::string& tooltip,
                             ux3d::gltf::Gltf& gltfModel);

    static void helpMarker(const std::string& desc);

    bool m_schemasLoaded = false;
    std::unordered_map<std::string, ExtensionSchemaInfo> m_schemas;
    std::vector<std::string> m_availableNames;
};

} // namespace ux3d::imgui
