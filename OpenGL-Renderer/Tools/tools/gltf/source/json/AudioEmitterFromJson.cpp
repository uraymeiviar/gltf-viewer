#include "gltf/json/AudioEmitterFromJson.h"
#include "gltf/AudioEmitter.h"
#include "gltf/AudioSource.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/ParsingUtils.h"
#include "gltf/json/PointerLookup.h"

using namespace ux3d::gltf;

ConversionResult AudioEmitterFromJson::convert(const nlohmann::json& jsonAudioEmitter)
{
    AudioEmitter* pAudioEmitter = m_gltf.create<AudioEmitter>();
    if (pAudioEmitter == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pAudioEmitter)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : jsonAudioEmitter.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pAudioEmitter->setName(value.get<std::string>());
            } else {
                core::logWarning("audioEmitter.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "type") {
            if (value.is_string()) {
                std::string type = value.get<std::string>();
                if (type == "positional") {
                    pAudioEmitter->setType(AudioEmitter::EmitterType::Positional);
                } else if (type == "global") {
                    pAudioEmitter->setType(AudioEmitter::EmitterType::Global);
                } else {
                    core::logWarning("audioEmitter.%s with value '%s' has invalid value", key.c_str(), value.dump().c_str());
                    containsErrors = true;
                }
            } else {
                core::logWarning("audioEmitter.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "gain") {
            if (value.is_number()) {
                pAudioEmitter->setGain(value.get<double>());
            } else {
                core::logWarning("audioEmitter.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "loop") {
            if (value.is_boolean()) {
                pAudioEmitter->setLoop(value.get<bool>());
            } else {
                core::logWarning("audioEmitter.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "playing") {
            if (value.is_boolean()) {
                pAudioEmitter->setPlaying(value.get<bool>());
            } else {
                core::logWarning("audioEmitter.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "positional") {
            if (value.is_object()) {
                containsErrors |= convertPositional(value, *pAudioEmitter); // returns true if errors occured during loading
            } else {
                core::logWarning("audioEmitter.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(jsonAudioEmitter, pAudioEmitter, "audio source")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(jsonAudioEmitter, pAudioEmitter);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult AudioEmitterFromJson::resolve(const nlohmann::json& jsonAudioEmitter, AudioEmitter& audioEmitter)
{
    bool containsErrors = false;

    if (jsonAudioEmitter.contains("source")) {
        if (core::Succeeded(checkIndex(jsonAudioEmitter.at("source"), m_pointerLookup.AudioSources.size(), "audioEmitter.source"))) {
            AudioSource* pAudioSource = m_pointerLookup.AudioSources[jsonAudioEmitter.at("source").get<uint32_t>()];
            audioEmitter.setSource(pAudioSource);
        } else {
            containsErrors = true;
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

bool AudioEmitterFromJson::convertPositional(const nlohmann::json& jsonPositional, AudioEmitter& audioEmitter)
{
    audioEmitter.createPositional();
    AudioEmitterPositional& positional = audioEmitter.getPositional();

    bool containsErrors = false;
    for (const auto& [key, value] : jsonPositional.items()) {
        if (key == "coneInnerAngle") {
            if (value.is_number()) {
                positional.setConeInnerAngle(value.get<double>());
            } else {
                core::logWarning("audioEmitter.positional.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "coneOuterAngle") {
            if (value.is_number()) {
                positional.setConeOuterAngle(value.get<double>());
            } else {
                core::logWarning("audioEmitter.positional.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "coneOuterGain") {
            if (value.is_number()) {
                positional.setConeOuterGain(value.get<double>());
            } else {
                core::logWarning("audioEmitter.positional.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "distanceModel") {
            if (value.is_string()) {
                std::string distanceModel = value.get<std::string>();
                if (distanceModel == "Linear") {
                    positional.setDistanceModel(AudioEmitterPositional::EmitterDistanceModel::Linear);
                } else if (distanceModel == "inverse") {
                    positional.setDistanceModel(AudioEmitterPositional::EmitterDistanceModel::Inverse);
                } else if (distanceModel == "exponential") {
                    positional.setDistanceModel(AudioEmitterPositional::EmitterDistanceModel::Exponential);
                } else {
                    core::logWarning("audioEmitter.positional.%s with value '%s' has invalid value", key.c_str(), value.dump().c_str());
                    containsErrors = true;
                }
            } else {
                core::logWarning("audioEmitter.positional.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "maxDistance") {
            if (value.is_number()) {
                positional.setMaxDistance(value.get<double>());
            } else {
                core::logWarning("audioEmitter.positional.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "refDistance") {
            if (value.is_number()) {
                positional.setRefDistance(value.get<double>());
            } else {
                core::logWarning("audioEmitter.positional.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "rolloffFactor") {
            if (value.is_number()) {
                positional.setRolloffFactor(value.get<double>());
            } else {
                core::logWarning("audioEmitter.positional.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(jsonPositional, &positional, "audio source")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(jsonPositional, &positional);

    return containsErrors;
}