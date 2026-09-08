// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Image;
class ResourceCollector;
class Gltf;

class ImagesToJson : ChildOfRootToJson {
public:
    explicit ImagesToJson(const IndexLookup& _indexLookup, ResourceCollector& _resourceCollector, const ExportOptions& _exportOptions, const Gltf& _gltf)
        : ChildOfRootToJson(_indexLookup, _exportOptions)
        , m_resourceCollector(_resourceCollector)
        , m_gltf(_gltf)
    {
    }

    void convert(const Image& _image, nlohmann::json& _jsonImage, const JsonPath& _jsonPath) const;

private:
    void convertName(const Image& _image, nlohmann::json& _jsonImage) const;
    void convertUri(const Image& _image, nlohmann::json& _jsonImage) const;
    void convertMimeType(const Image& _image, nlohmann::json& _jsonImage) const;
    void convertBufferView(const Image& _image, nlohmann::json& _jsonImage) const;
    void convertBackupUri(const Image& _image, nlohmann::json& _jsonImage) const;
    void convertStorageFormat(const Image& _image, nlohmann::json& _jsonImage) const;
    void convertImageSize(const Image& _image, nlohmann::json& _jsonImage) const;
    void convertImageCompression(const Image& _image, nlohmann::json& _jsonImage) const;
    void convertMapId(const Image& _image, nlohmann::json& _jsonImage) const;
    void convertXMP(const Image& _image, nlohmann::json& _jsonImage) const;

    ResourceCollector& m_resourceCollector;
    const Gltf& m_gltf;
};
}
