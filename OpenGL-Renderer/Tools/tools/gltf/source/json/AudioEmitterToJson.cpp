// Copyright (c) 2022 UX3D GmbH. All rights reserved.

#include "gltf/json/AudioEmitterToJson.h"
#include "gltf/AudioEmitter.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/GltfObjectToJson.h"

using namespace ux3d::gltf;

AudioEmitterToJson::AudioEmitterToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions)
    : ChildOfRootToJson(_indices, _exportOptions)
{
}

void AudioEmitterToJson::convert(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const
{
    convertBase(audioEmitter, jsonAudioEmitter);
    convertType(audioEmitter, jsonAudioEmitter);
    convertGain(audioEmitter, jsonAudioEmitter);
    convertLoop(audioEmitter, jsonAudioEmitter);
    convertPlaying(audioEmitter, jsonAudioEmitter);
    convertSource(audioEmitter, jsonAudioEmitter);
    convertPositional(audioEmitter, jsonAudioEmitter);
}

void AudioEmitterToJson::convertType(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const
{
    if (audioEmitter.getType() == AudioEmitter::EmitterType::Positional) {
        jsonAudioEmitter["type"] = "positional";
    } else if (audioEmitter.getType() == AudioEmitter::EmitterType::Global) {
        jsonAudioEmitter["type"] = "global";
    }
}

void AudioEmitterToJson::convertGain(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const
{
    if (audioEmitter.hasGain() == false) {
        return;
    }

    jsonAudioEmitter["gain"] = audioEmitter.getGain();
}

void AudioEmitterToJson::convertLoop(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const
{
    if (audioEmitter.hasLoop() == false) {
        return;
    }

    jsonAudioEmitter["loop"] = audioEmitter.isLoop();
}

void AudioEmitterToJson::convertPlaying(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const
{
    if (audioEmitter.hasPlaying() == false) {
        return;
    }

    jsonAudioEmitter["playing"] = audioEmitter.isPlaying();
}

void AudioEmitterToJson::convertSource(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const
{
    if (audioEmitter.getSource() == nullptr) {
        return;
    }

    size_t index = m_indices[audioEmitter.getSource()];
    jsonAudioEmitter["source"] = index;
}

void AudioEmitterToJson::convertPositional(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const
{
    if (audioEmitter.hasPositional() == false) {
        return;
    }

    const AudioEmitterPositional& gltfPositional = audioEmitter.getPositional();

    nlohmann::json positional = nlohmann::json::object();

    if (gltfPositional.hasConeInnerAngle()) {
        positional["coneInnerAngle"] = gltfPositional.getConeInnerAngle();
    }
    if (gltfPositional.hasConeOuterAngle()) {
        positional["coneOuterAngle"] = gltfPositional.getConeOuterAngle();
    }
    if (gltfPositional.hasConeOuterGain()) {
        positional["coneOuterGain"] = gltfPositional.getConeOuterGain();
    }

    if (gltfPositional.getDistanceModel() == AudioEmitterPositional::EmitterDistanceModel::Linear) {
        jsonAudioEmitter["distanceModel"] = "linear";
    } else if (gltfPositional.getDistanceModel() == AudioEmitterPositional::EmitterDistanceModel::Inverse) {
        jsonAudioEmitter["distanceModel"] = "inverse";
    } else if (gltfPositional.getDistanceModel() == AudioEmitterPositional::EmitterDistanceModel::Exponential) {
        jsonAudioEmitter["distanceModel"] = "exponential";
    }

    if (gltfPositional.hasMaxDistance()) {
        positional["maxDistance"] = gltfPositional.getMaxDistance();
    }
    if (gltfPositional.hasRefDistance()) {
        positional["refDistance"] = gltfPositional.getRefDistance();
    }
    if (gltfPositional.hasRolloffFactor()) {
        positional["rolloffFactor"] = gltfPositional.getRolloffFactor();
    }

    jsonAudioEmitter["positional"] = positional;
}
