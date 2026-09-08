#include "gltf/json/ImagesFromJson.h"
#include "gltf/EmbeddedData.h"
#include "gltf/Gltf.h"
#include "gltf/Image.h"
#include "gltf/Ktx2.h"
#include "gltf/Utils.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"
#include <gltf/json/ConversionResult.h>

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult ImagesFromJson::convert(const nlohmann::json& _jsonImage)
{
    Image* pImage = m_gltf.create<Image>(getID(_jsonImage));
    if (pImage == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pImage)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonImage.items()) {
        if (key == "uri") {
            if (value.is_string()) {
                std::string uri = value.get<std::string>();
                if (!gltf::embedded_data::isDataUri(uri)) {
                    utils::searchAndReplace(uri, "%20", " ");
                }
                pImage->setUri(uri);
            } else {
                core::logWarning("image.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "mimeType") {
            if (value.is_string()) {
                if (core::Failed(pImage->setMimeType(imageformats::imageEncodingFromString(value.get<std::string>())))) {
                    core::logWarning("image.%s with value '%s' is invalid", key.c_str(), value.dump().c_str());
                    containsErrors = true;
                }
            } else {
                core::logWarning("image.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "name") {
            if (value.is_string()) {
                pImage->setName(value.get<std::string>());
            } else {
                core::logWarning("image.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "ux3d_gestaltor") {
                        if (extension.is_object()) {
                            if (core::Failed(convertStorageFormat(extension, *pImage))) {
                                containsErrors = true;
                            }
                            if (core::Failed(convertBackupUri(extension, *pImage))) {
                                containsErrors = true;
                            }
                            if (core::Failed(convertJPEGMetaData(extension, *pImage))) {
                                containsErrors = true;
                            }
                            if (core::Failed(convertKTXMetaData(extension, *pImage))) {
                                containsErrors = true;
                            }
                            if (core::Failed(convertImageDimensions(extension, *pImage))) {
                                containsErrors = true;
                            }
                            if (core::Failed(convertMapId(extension, *pImage))) {
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

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonImage, pImage, "image")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonImage, pImage);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult ImagesFromJson::resolve(const nlohmann::json& _jsonImage, Image& _image)
{
    ConversionResult result = ConversionResult::Success;

    for (const auto& [key, value] : _jsonImage.items()) {
        if (key == "bufferView") {
            if (value.is_number_unsigned() == false) {
                core::logWarning("image.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                result = ConversionError::InvalidType;
                continue;
            }

            uint32_t index = value.get<uint32_t>();

            if (index >= m_pointerLookup.BufferViews.size()) {
                core::logWarning("image.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                result = ConversionError::IndexOutOfRange;
                continue;
            }

            BufferView* pBufferView = m_pointerLookup.BufferViews[index];
            _image.setBufferView(*pBufferView);
            _image.m_bufferViewIndex = index;
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "KHR_xmp_json_ld") {
                        if (extension.is_object()) {
                            for (const auto& [xmpKey, xmpValue] : extension.items()) {
                                if (xmpKey == "packet") {
                                    if (m_gltf.getXMP().has_value() == false) {
                                        core::logWarning("ImagesFromJson: Image.XMP is defined, but the root extension KHR_xmp_json_ld is missing");
                                        result = ConversionError::InvalidJson;
                                        continue;
                                    }

                                    XMP& xmp = m_gltf.getXMP().value();
                                    core::LinkedList<Packet>& packets = xmp.getPackets();
                                    const size_t index = xmpValue.get<size_t>();

                                    Packet* pPacket = &packets.at(index);
                                    _image.setXMPPacket(pPacket);
                                }
                            }
                            if (_image.hasXMPPacket()) {
                                if (!GltfObjectFromJson::convertUnknownExtensions(extension, &_image.getXMPPacket().value(), "image.extensions.KHR_xmp_json_ld")) {
                                    result = ConversionError::InvalidJson;
                                }
                                GltfObjectFromJson::convertExtras(extension, &_image.getXMPPacket().value());
                            }
                        } else {
                            core::logWarning("image.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            result = ConversionError::InvalidType;
                        }
                    }
                }
            }
        }
    }

    return result;
}

ConversionResult ImagesFromJson::convertStorageFormat(const nlohmann::json& _editorExtension, Image& _image)
{
    if (_editorExtension.contains("storageFormat") == false) {
        return ConversionResult::Success;
    }

    auto& jsonFormat = _editorExtension["storageFormat"];
    if (jsonFormat.is_string() == false) {
        core::logWarning("ux3d_gestaltor.storageFormat must be a string");
        return ConversionError::InvalidType;
    }

    _image.setStorageFormat(storageFormatFromString(jsonFormat.get<core::string>()));
    return ConversionResult::Success;
}

ConversionResult ImagesFromJson::convertBackupUri(const nlohmann::json& _editorExtension, Image& _image)
{
    if (_editorExtension.contains("backupUri") == false || !_image.getUri().empty()) {
        return ConversionResult::Success;
    }

    auto& jsonBackupUri = _editorExtension["backupUri"];
    if (jsonBackupUri.is_string() == false) {
        core::logWarning("ux3d_gestaltor.backupUri must be a string");
        return ConversionError::InvalidType;
    }

    _image.setUri(jsonBackupUri.get<core::string>());

    return ConversionResult::Success;
}

ConversionResult ImagesFromJson::convertImageDimensions(const nlohmann::json& _editorExtension, Image& _image)
{
    if (_editorExtension.contains("width") == false || _editorExtension.contains("height") == false) {
        return ConversionResult::Success;
    }

    auto& jsonWidth = _editorExtension["width"];
    auto& jsonHeight = _editorExtension["height"];
    if (jsonWidth.is_number() == false || jsonHeight.is_number() == false) {
        core::logWarning("ux3d_gestaltor.imageDimensions must be numbers");
        return ConversionError::InvalidType;
    }

    _image.m_data.setHeight(jsonHeight.get<uint32_t>());
    _image.m_data.setWidth(jsonWidth.get<uint32_t>());

    return ConversionResult::Success;
}

ConversionResult ImagesFromJson::convertJPEGMetaData(const nlohmann::json& _editorExtension, Image& _image)
{
    if (_editorExtension.contains("jpegCompression") == false) {
        return ConversionResult::Success;
    }

    auto& jsonCompression = _editorExtension["jpegCompression"];
    if (jsonCompression.is_number() == false) {
        core::logWarning("ux3d_gestaltor.jpegCompression must be a number");
        return ConversionError::InvalidType;
    }
    image::JPEGMetaData metaData;
    metaData.compressionRate = jsonCompression.get<uint8_t>();
    _image.m_data.m_metaData = metaData;

    return ConversionResult::Success;
}

ConversionResult ImagesFromJson::convertKTXMetaData(const nlohmann::json& _editorExtension, Image& _image)
{
    image::KTXMetaData metaData;

    if (_editorExtension.contains("ktxEtc1sQualityLevel")) {
        auto& etc1sQualityLevel = _editorExtension["ktxEtc1sQualityLevel"];
        if (etc1sQualityLevel.is_number() == false) {
            core::logWarning("ux3d_gestaltor.ktxCompression must be a number");
            return ConversionError::InvalidType;
        } else {
            metaData.etc1sQualityLevel = etc1sQualityLevel.get<uint32_t>();
        }
    } else {
        return ConversionResult::Success;
    }

    if (_editorExtension.contains("ktxEtc1sCompressionLevel")) {
        auto& etc1sCompressionLevel = _editorExtension["ktxEtc1sCompressionLevel"];
        if (etc1sCompressionLevel.is_number() == false) {
            core::logWarning("ux3d_gestaltor.ktxSupercompression must be a number");
        } else {
            metaData.etc1sCompressionLevel = etc1sCompressionLevel.get<uint32_t>();
        }
    }

    if (_editorExtension.contains("ktxUastcQualityLevel")) {
        auto& uastcQualityLevel = _editorExtension["ktxUastcQualityLevel"];
        if (uastcQualityLevel.is_number() == false) {
            core::logWarning("ux3d_gestaltor.ktxSupercompression must be a number");
        } else {
            metaData.uastcQualityLevel = uastcQualityLevel.get<uint32_t>();
        }
    }

    if (_editorExtension.contains("ktxUastcRDO")) {
        auto& uastcRDO = _editorExtension["ktxUastcRDO"];
        if (uastcRDO.is_number() == false) {
            core::logWarning("ux3d_gestaltor.ktxSupercompression must be a number");
        } else {
            metaData.uastcRDO = uastcRDO.get<float>();
        }
    }

    if (_editorExtension.contains("ktxZstdCompressionLevel")) {
        auto& zstdCompressionLevel = _editorExtension["ktxZstdCompressionLevel"];
        if (zstdCompressionLevel.is_number() == false) {
            core::logWarning("ux3d_gestaltor.ktxSupercompression must be a number");
        } else {
            metaData.zstdCompressionLevel = zstdCompressionLevel.get<uint32_t>();
        }
    }

    if (_editorExtension.contains("ktxColorPrimaries")) {
        auto& ktxColorPrimaries = _editorExtension["ktxColorPrimaries"];
        if (ktxColorPrimaries.is_number() == false) {
            core::logWarning("ux3d_gestaltor.ktxColorPrimaries must be a number");
        } else {
            metaData.colorPrimaries = ktxColorPrimaries.get<uint8_t>();
        }
    }

    if (_editorExtension.contains("ktxSupercompressionScheme")) {
        auto& ktxSupercompressionScheme = _editorExtension["ktxSupercompressionScheme"];
        if (ktxSupercompressionScheme.is_number() == false) {
            core::logWarning("ux3d_gestaltor.ktxSupercompressionScheme must be a number");
        } else {
            metaData.supercompressionSchema = ktxSupercompressionScheme.get<image::KtxSupercompressionScheme>();
        }
    }

    if (_editorExtension.contains("ktxColorModel")) {
        if (_editorExtension["ktxColorModel"].is_number()) {
            metaData.colorModel = _editorExtension["ktxColorModel"].get<image::KtxCompressionScheme>();
        } else {
            core::logWarning("ux3d_gestaltor.ktxColorModel invalid");
        }
    }

    if (_editorExtension.contains("ktxTransferFunction")) {
        if (_editorExtension["ktxTransferFunction"].is_number()) {
            metaData.transferFunction = _editorExtension["ktxTransferFunction"].get<uint8_t>();
        } else {
            core::logWarning("ux3d_gestaltor.ktxTransferFunction invalid");
        }
    }

    _image.m_data.m_metaData = metaData;

    return ConversionResult::Success;
}
ux3d::gltf::ConversionResult ux3d::gltf::ImagesFromJson::convertMapId(const nlohmann::json& _editorExtension, Image& _image)
{
    if (_editorExtension.contains("mapId") == false) {
        return ConversionResult::Success;
    }

    nlohmann::json jsonMapId = _editorExtension["mapId"];
    if (jsonMapId.is_number() == false) {
        core::logWarning("ux3d_gestaltor.mapId must be a number");
        return ConversionError::InvalidType;
    }

    _image.encodedMapID = jsonMapId.get<int>();

    return ConversionResult::Success;
}
