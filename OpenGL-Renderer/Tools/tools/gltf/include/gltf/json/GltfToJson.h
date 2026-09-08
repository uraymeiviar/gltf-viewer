// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "gltf/ExportOptions.h"
#include "gltf/Extensions.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/ConversionResult.h"
#include "nlohmann/json.hpp"

namespace ux3d::core {
template <typename T>
class LinkedList;
}

namespace ux3d::gltf {
class Gltf;
class ResourceCollector;
class ChildOfRoot;
class MaterialVariants;

class GltfToJson {
public:
    explicit GltfToJson(const Gltf& _gltf, ResourceCollector& _resourceCollector, const ExportOptions& _exportOptions)
        : m_gltf(_gltf)
        , m_exportOptions(_exportOptions)
        , m_indices(_gltf, _exportOptions.exportCandidates)
        , m_resourceCollector(_resourceCollector)
    {
    }

    ConversionResult convert();
    const nlohmann::json& getJson();

private:
    const Gltf& m_gltf;
    const ExportOptions& m_exportOptions;
    nlohmann::json m_json;
    IndexLookup m_indices;
    ResourceCollector& m_resourceCollector;

    void convertAssetMetaData();
    void convertDefaultScene();

    template <typename GltfType, typename Converter>
    void convertElements(
        const core::LinkedList<GltfType>& _elements,
        Converter& _converter,
        const core::string& _listName,
        const core::string& extensionName = "");

    void convertScenes();
    void convertNodes();
    void convertCameras();
    void convertMeshes();
    void convertMaterials();
    void convertTextures();
    void convertImages();
    void convertSamplers();
    void convertAccessors();
    void convertBufferViews();
    void convertBuffers();
    void convertSkins();
    void convertAnimations();

    // extensions
    void unknownExtensionsToNames(const std::unordered_map<core::string, core::string>& _inUnknownExtensions, core::Vector<core::string>& _outNames);
    void convertUsedExtensions();

    void convertLights();
    void convertMaterialVariants();
    void convertXMP();
    void convertAudio();

    bool isExportElement(const ChildOfRoot* _element);

    ConversionResult checkValidity(const Gltf& _gltf) const;
    template <typename GltfType>
    ConversionResult checkValidity(const core::LinkedList<GltfType>& _elements) const;
};
}
