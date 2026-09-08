// Copyright (c) 2022 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class AudioEmitter;

class AudioEmitterFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& jsonAudioEmitter) override;
    ConversionResult resolve(const nlohmann::json& jsonAudioEmitter, AudioEmitter& audioEmitter);

private:
    /**
     * @brief Returns true if errors occured during conversion.
     *
     * @param jsonPositional
     * @param audioEmitter
     * @return true
     * @return false
     */
    bool convertPositional(const nlohmann::json& jsonPositional, AudioEmitter& audioEmitter);
};
}
