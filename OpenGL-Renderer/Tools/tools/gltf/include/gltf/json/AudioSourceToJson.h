// Copyright (c) 2022 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class AudioSource;
class Gltf;
class ResourceCollector;

class AudioSourceToJson : ChildOfRootToJson {
public:
    using ChildOfRootToJson::ChildOfRootToJson;
    AudioSourceToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions, ResourceCollector& _resourceCollector);
    void convert(const AudioSource& audioSource, nlohmann::json& jsonAudioSource) const;

private:
    void convertURI(const AudioSource& audioSource, nlohmann::json& jsonAudioSource) const;
    void convertMimeType(const AudioSource& audioSource, nlohmann::json& jsonAudioSource) const;
    void convertBufferView(const AudioSource& audioSource, nlohmann::json& jsonAudioSource) const;

    ResourceCollector& m_resourceCollector;
};
}
