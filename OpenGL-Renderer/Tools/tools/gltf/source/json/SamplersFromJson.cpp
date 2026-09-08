#include "gltf/json/SamplersFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"

ux3d::gltf::ConversionResult ux3d::gltf::SamplersFromJson::convert(const nlohmann::json& _jsonSampler)
{
    Sampler* pSampler = m_gltf.create<Sampler>(getID(_jsonSampler));
    if (pSampler == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pSampler)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonSampler.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pSampler->setName(value.get<std::string>());
            } else {
                core::logWarning("sampler.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "magFilter") {
            if (value.is_number_integer()) {
                pSampler->setMagFilter(value);
            } else {
                core::logWarning("sampler.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "minFilter") {
            if (value.is_number_integer()) {
                pSampler->setMinFilter(value);
            } else {
                core::logWarning("sampler.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "wrapS") {
            if (value.is_number_integer()) {
                pSampler->setWrapS(value);
            } else {
                core::logWarning("sampler.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "wrapT") {
            if (value.is_number_integer()) {
                pSampler->setWrapT(value);
            } else {
                core::logWarning("sampler.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonSampler, pSampler, "sampler")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonSampler, pSampler);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
