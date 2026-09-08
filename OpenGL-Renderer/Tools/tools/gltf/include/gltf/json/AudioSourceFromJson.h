// Copyright (c) 2022 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class AudioSource;

class AudioSourceFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& jsonAudioSource) override;
    ConversionResult resolve(const nlohmann::json& jsonAudioSource, AudioSource& audioSource);
};
}
