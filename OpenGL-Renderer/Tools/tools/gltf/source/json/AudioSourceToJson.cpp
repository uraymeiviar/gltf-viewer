// Copyright (c) 2022 UX3D GmbH. All rights reserved.

#include "gltf/json/AudioSourceToJson.h"
#include "gltf/AudioSource.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/ResourceCollector.h"
#include "gltf/json/GltfObjectToJson.h"

using namespace ux3d::gltf;

AudioSourceToJson::AudioSourceToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions, ResourceCollector& _resourceCollector)
    : ChildOfRootToJson(_indices, _exportOptions)
    , m_resourceCollector(_resourceCollector)
{
}

void AudioSourceToJson::convert(const AudioSource& audioSource, nlohmann::json& jsonAudioSource) const
{
    convertBase(audioSource, jsonAudioSource);
    convertURI(audioSource, jsonAudioSource);
    convertMimeType(audioSource, jsonAudioSource);
    convertBufferView(audioSource, jsonAudioSource);

    m_resourceCollector.add(audioSource);
}

void AudioSourceToJson::convertURI(const AudioSource& audioSource, nlohmann::json& jsonAudioSource) const
{
    if (audioSource.hasUri() == false) {
        return;
    }

    jsonAudioSource["uri"] = audioSource.getUri();
}

void AudioSourceToJson::convertMimeType(const AudioSource& audioSource, nlohmann::json& jsonAudioSource) const
{
    if (audioSource.hasMimeType() == false) {
        return;
    }

    jsonAudioSource["mimeType"] = audioSource.getMimeType();
}

void AudioSourceToJson::convertBufferView(const AudioSource& audioSource, nlohmann::json& jsonAudioSource) const
{
    if (audioSource.hasBufferView() == false) {
        return;
    }

    size_t index = m_indices[audioSource.getBufferView()];
    jsonAudioSource["bufferView"] = index;
}