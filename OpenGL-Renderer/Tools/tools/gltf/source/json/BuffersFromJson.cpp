#include "gltf/json/BuffersFromJson.h"
#include "gltf/EmbeddedData.h"
#include "gltf/Gltf.h"
#include "gltf/Utils.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"

ux3d::gltf::ConversionResult ux3d::gltf::BuffersFromJson::convert(const nlohmann::json& _jsonBuffer)
{
    Buffer* pBuffer = m_gltf.create<Buffer>(getID(_jsonBuffer));
    if (pBuffer == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pBuffer)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonBuffer.items()) {
        if (key == "uri") {
            if (value.is_string()) {
                std::string uri = value.get<std::string>();
                if (!gltf::embedded_data::isDataUri(uri)) {
                    utils::searchAndReplace(uri, "%20", " ");
                }
                pBuffer->setUri(uri);
            } else {
                core::logWarning("buffer.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "name") {
            if (value.is_string()) {
                pBuffer->setName(value.get<std::string>());
            } else {
                core::logWarning("buffer.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "byteLength") {
            if (value.is_number_unsigned()) {
                pBuffer->setByteLength(value);
            } else {
                core::logWarning("buffer.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "ux3d_gestaltor") {
                        if (extension.is_object()) {
                            if (core::Failed(convertStorageFormat(extension, *pBuffer))) {
                                containsErrors = true;
                            }
                        } else {
                            core::logWarning("extensions.%s with value '%s' has invalid type", extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    }
                }
            }
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonBuffer, pBuffer, "buffer")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonBuffer, pBuffer);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ux3d::gltf::ConversionResult ux3d::gltf::BuffersFromJson::convertStorageFormat(const nlohmann::json& _editorExtension, Buffer& _buffer)
{
    if (_editorExtension.contains("storageFormat") == false) {
        return ConversionResult::Success;
    }

    auto jsonFormat = _editorExtension["storageFormat"];
    if (jsonFormat.is_string() == false) {
        core::logWarning("ux3d_gestaltor.storageFormat must be a string");
        return ConversionError::InvalidType;
    }

    _buffer.setStorageFormat(storageFormatFromString(jsonFormat.get<core::string>()));
    return ConversionResult::Success;
}
