// Copyright (c) 2022 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class AudioEmitter;
class Gltf;

class AudioEmitterToJson : ChildOfRootToJson {
public:
    using ChildOfRootToJson::ChildOfRootToJson;
    AudioEmitterToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions);
    void convert(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const;

private:
    void convertType(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const;
    void convertGain(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const;
    void convertLoop(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const;
    void convertPlaying(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const;
    void convertSource(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const;
    void convertPositional(const AudioEmitter& audioEmitter, nlohmann::json& jsonAudioEmitter) const;
};
}
