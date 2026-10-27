// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "imgui/SceneRevisions.h"
#include "gltf/GltfObject.h"
#include "gltf/extensions/AsoboExtensions.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace ux3d::imgui {

struct ViewportInstance;

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

    // Sets the active viewport whose settings are controlled by per-extension display toggles
    void setActiveViewport(ViewportInstance* viewport) { m_activeViewport = viewport; }
    ViewportInstance* getActiveViewport() const { return m_activeViewport; }

    // Renders all vendor and unknown extensions found on a gltfObject.
    // ASOBO typed parameters and existing vendor values are editable in memory.
    // Extension membership and vendor keys are fixed; strings need a schema enum.
    // Returns true if any parameter was modified.
    bool renderExtensions(ux3d::gltf::GltfObject* object,
                          ux3d::gltf::Gltf& gltfModel,
                          SceneRevisions& revisions);

    // Updates a single extension parameter on object in-memory and marks SceneChange::Material
    // and SceneChange::Extension so the next frame re-parses the typed ASOBO model and refreshes
    // renderer uniforms/defines. Shared by the Inspector UI and --set-asobo-param.
    bool setExtensionParameter(ux3d::gltf::GltfObject* object,
                               const std::string& extName,
                               const std::string& paramName,
                               const nlohmann::json& paramValue,
                               SceneRevisions& revisions);

    // Parses rawValue according to the ASOBO property spec/schema and applies it to the named
    // material (or "$asset" for the glTF asset object) via setExtensionParameter.
    bool setMaterialAsoboParameter(ux3d::gltf::Gltf& gltfModel,
                                   const std::string& materialName,
                                   const std::string& extName,
                                   const std::string& paramName,
                                   const std::string& rawValue,
                                   SceneRevisions& revisions);

    // Checks if a schema is available for this extension name
    bool hasSchema(const std::string& extensionName);
    const ExtensionSchemaInfo* getSchema(const std::string& extensionName);

    // Returns a list of all registered known extension names
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

    bool renderSingleExtension(ux3d::gltf::GltfObject* object,
                               const std::string& extName,
                               nlohmann::json& extJson,
                               const ExtensionSchemaInfo* schema,
                               ux3d::gltf::Gltf& gltfModel,
                               SceneRevisions& revisions);

    bool renderAsoboPropertyWidget(const std::string& extName,
                                   const std::string& propKey,
                                   nlohmann::json& propVal,
                                   const ux3d::gltf::asobo::PropertySpec* propSpec,
                                   const ExtensionSchemaProperty* propDef);

    void renderPropertyReadOnly(const std::string& propKey,
                                const nlohmann::json& propVal,
                                const ExtensionSchemaProperty* propDef,
                                ux3d::gltf::Gltf& gltfModel);

    void renderTextureReadOnly(const std::string& label,
                               const nlohmann::json& texVal,
                               const std::string& tooltip,
                               ux3d::gltf::Gltf& gltfModel);

    static void helpMarker(const std::string& desc);

    bool m_schemasLoaded = false;
    std::unordered_map<std::string, ExtensionSchemaInfo> m_schemas;
    std::vector<std::string> m_availableNames;
    ViewportInstance* m_activeViewport = nullptr;
};

} // namespace ux3d::imgui
