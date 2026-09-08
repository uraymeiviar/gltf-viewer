#include "gltf/json/AudioSourceFromJson.h"
#include "gltf/AudioSource.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/ParsingUtils.h"
#include "gltf/json/PointerLookup.h"

using namespace ux3d::gltf;

ConversionResult AudioSourceFromJson::convert(const nlohmann::json& jsonAudioSource)
{
    AudioSource* pAudioSource = m_gltf.create<AudioSource>();
    if (pAudioSource == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pAudioSource)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : jsonAudioSource.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pAudioSource->setName(value.get<std::string>());
            } else {
                core::logWarning("audioSource.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "uri") {
            if (value.is_string()) {
                std::string uri = value.get<std::string>();
                pAudioSource->setUri(uri);
            } else {
                core::logWarning("audioSource.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "mimeType") {
            if (value.is_string()) {
                std::string mimeType = value.get<std::string>();
                pAudioSource->setMimeType(mimeType);
            } else {
                core::logWarning("audioSource.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(jsonAudioSource, pAudioSource, "audio source")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(jsonAudioSource, pAudioSource);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult AudioSourceFromJson::resolve(const nlohmann::json& jsonAudioSource, AudioSource& audioSource)
{
    bool containsErrors = false;

    if (jsonAudioSource.contains("bufferView")) {
        if (core::Succeeded(checkIndex(jsonAudioSource.at("bufferView"), m_pointerLookup.BufferViews.size(), "audioSource.bufferView"))) {
            BufferView* pBufferView = m_pointerLookup.BufferViews[jsonAudioSource.at("bufferView").get<uint32_t>()];
            audioSource.setBufferView(*pBufferView);
        } else {
            containsErrors = true;
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
