// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Image.h"
#include "filesystem/File.h"
#include "filesystem/FileSystem.h"
#include "filesystem/Path.h"
#include "gltf/BufferView.h"
#include "gltf/EmbeddedData.h"
#include "gltf/Gltf.h"
#include "gltf/Ktx2.h"
#include "gltf/StorageTypeConverter.h"
#include "gltf/Utils.h"
#include "imageformats/imageformats.h"

using namespace ux3d;
using namespace ux3d::gltf;

Image::Image(uint32_t _id)
    : ChildOfRoot(_id)
{
}

Image::Image(uint32_t _id, const Image& _source)
    : ChildOfRoot(_id, _source)
    , m_data(_source.m_data)
    , m_historyDataIsDirty(true)
    , encodedMapID(_source.encodedMapID)
    , m_uri(_source.m_uri)
    , m_bufferView(_source.m_bufferView)
    , m_mimeType(_source.m_mimeType)
    , m_xmpPacket(_source.m_xmpPacket)
    , m_storageFormat(_source.m_storageFormat)
    , m_isErrorImage(_source.m_isErrorImage)
    , m_encodedData(_source.m_encodedData)
{
}

GltfResult Image::setBufferView(BufferView& _bufferView)
{
    m_bufferView = &_bufferView;
    setStorageFormat(StorageFormat::BufferView);
    return GltfResult::Success;
}

GltfResult Image::setUri(const core::string& _uri)
{
    m_uri = _uri;
    m_historyDataIsDirty = true;
    return GltfResult::Success;
}

/// Does not change the file encodig nor the file uri
GltfResult Image::setMimeType(imageformats::ImageEncoding _mimeType)
{
    if (!isValidEncoding(_mimeType)) {
        return GltfError::InvalidInput;
    }
    m_mimeType = _mimeType;

    return GltfResult::Success;
}

bool Image::isValidEncoding(imageformats::ImageEncoding _encoding)
{
    return _encoding == imageformats::ImageEncoding::PNG || _encoding == imageformats::ImageEncoding::JPEG || _encoding == imageformats::ImageEncoding::KTX2 || _encoding == imageformats::ImageEncoding::WEBP || _encoding == imageformats::ImageEncoding::DDS;
}

GltfResult Image::loadDataToMemory(const std::optional<filesystem::Dir>& _gltfDir)
{
    if (m_data.getPixels().empty() == false) {
        return GltfError::ObjectAlreadyExists;
    }

    if (m_bufferView != nullptr) {
        setStorageFormat(StorageFormat::BufferView);
        return loadDataFromBufferView();
    }

    if (embedded_data::isDataUri(m_uri)) {
        setStorageFormat(StorageFormat::Embedded);
        return loadDataFromEmbedded();
    }

    if (m_uri.empty() == false) {
        if (_gltfDir.has_value()) {
            setStorageFormat(StorageFormat::File);
            loadDataFromUri(*_gltfDir);
            return GltfResult::Success;
        } else {
            core::logError("Cannot resolve data from files if no base directory is given");
        }
    }

    return GltfError::ObjectNotFound;
}

GltfResult Image::reloadDataToMemory(const filesystem::File& _imageFile)
{
    // this is used for hot reloading an image from the disk. It overrides the current image data

    core::Vector<uint8_t> rawData;
    if (core::Failed(_imageFile.load(rawData))) {
        return GltfError::ObjectNotFound;
    }

    if (rawData.empty()) {
        return GltfError::InvalidInput;
    }

    image::Image decodedImage;
    if (core::Failed(imageformats::decodeImage(rawData, decodedImage))) {
        return GltfError::InvalidInput;
    }

    if (decodedImage.getPixels() == m_data.getPixels()) {
        // The image hasn't changed
        return GltfError::InvalidInput;
    }

    m_data = decodedImage;
    m_isErrorImage = false;

    if (!isValidEncoding(getMimeType())) {
        imageformats::ImageEncoding encoding = imageformats::getEncodingFromBuffer(rawData);
        if (encoding == imageformats::ImageEncoding::BMP || encoding == imageformats::ImageEncoding::TGA) {
            core::Vector<uint8_t> pngImage;
            encoding = imageformats::ImageEncoding::PNG;
            image::ImageMetaData metaData;
            if (core::Failed(imageformats::encodeImage(m_data, encoding, pngImage, metaData))) {
                core::debugLog(core::LogLevel::Error, "Could not convert image to PNG");
                return GltfError::InvalidInput;
            }
            rawData = std::move(pngImage);
            changeUriToCorrectFileExtension(encoding);
        }
        setMimeType(encoding);
    }

    gltf::GltfResult result = setEncodedBytes(rawData);
    if (core::Failed(result)) {
        core::debugLog(core::LogLevel::Error, "Image: failed to set encoded bytes");
    }

    return result;
}

GltfResult Image::loadDataFromBufferView()
{
    if (m_bufferView == nullptr) {
        return GltfError::ObjectNotFound;
    }

    core::MemoryRange<const uint8_t> bufferViewData;
    m_bufferView->getData(bufferViewData);

    if (bufferViewData.empty()) {
        return GltfError::ObjectNotFound;
    }

    core::Vector<uint8_t> rawData;
    if (rawData.assign(bufferViewData, nullptr) == false) {
        return GltfError::AllocationFailure;
    }

    if (!imageformats::decodeImage(rawData, m_data)) {
        core::logError("Cannot decode image from bufferView");
        loadAsErrorImage();
        return GltfError::InvalidInput;
    }

    setEncodedBytes(rawData);
    return GltfResult::Success;
}

void Image::loadAsErrorImage()
{
    m_isErrorImage = true;
    m_Name = "Invalid image " + m_uri;
    m_uri = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAAAXNSR0IA"
            "rs4c6QAAAFBlWElmTU0AKgAAAAgAAgESAAMAAAABAAEAAIdpAAQAAAABAAAAJgAAAAAAA6ABAAMAAAABAA"
            "EAAKACAAQAAAABAAAAAaADAAQAAAABAAAAAQAAAADr/7PgAAABWWlUWHRYTUw6Y29tLmFkb2JlLnhtcAAA"
            "AAAAPHg6eG1wbWV0YSB4bWxuczp4PSJhZG9iZTpuczptZXRhLyIgeDp4bXB0az0iWE1QIENvcmUgNi4wLj"
            "AiPgogICA8cmRmOlJERiB4bWxuczpyZGY9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkvMDIvMjItcmRmLXN5"
            "bnRheC1ucyMiPgogICAgICA8cmRmOkRlc2NyaXB0aW9uIHJkZjphYm91dD0iIgogICAgICAgICAgICB4bW"
            "xuczp0aWZmPSJodHRwOi8vbnMuYWRvYmUuY29tL3RpZmYvMS4wLyI+CiAgICAgICAgIDx0aWZmOk9yaWVu"
            "dGF0aW9uPjE8L3RpZmY6T3JpZW50YXRpb24+CiAgICAgIDwvcmRmOkRlc2NyaXB0aW9uPgogICA8L3JkZj"
            "pSREY+CjwveDp4bXBtZXRhPgoZXuEHAAAADUlEQVQIHWP4z/D/PwAG/gL+TePW1QAAAABJRU5ErkJggg==";

    core::string mimeType = "";
    core::Vector<uint8_t> rawData;
    embedded_data::extractBase64Data(m_uri, mimeType, rawData);
    setMimeType(imageformats::getEncodingFromUri(mimeType));

    if (core::Succeeded(imageformats::decodeImage(rawData, m_data))) {
        setEncodedBytes(rawData);
    }
}

GltfResult Image::loadDataFromUri(const filesystem::Dir& _gltfDir)
{
    core::string uri = m_uri;

    filesystem::File file = _gltfDir.getFilesystem().fromURI(uri);

    if (file.isRelative()) {
        file = _gltfDir / file.toString();
    }

    core::Vector<uint8_t> rawData;
    if (core::Failed(file.load(rawData))) {
        core::logError("Cannot resolve image data from file. File not found at location given by uri: %s", file.toString().c_str());
        loadAsErrorImage();
        return GltfError::ObjectNotFound;
    }

    if (core::Failed(imageformats::decodeImage(rawData, m_data))) {
        core::logError("Unable to decode image");
        loadAsErrorImage();
        return GltfError::InvalidInput;
    }

    if (!isValidEncoding(getMimeType())) {
        imageformats::ImageEncoding encoding = imageformats::getEncodingFromBuffer(rawData);
        if (encoding == imageformats::ImageEncoding::BMP || encoding == imageformats::ImageEncoding::TGA) {
            core::Vector<uint8_t> pngImage;
            encoding = imageformats::ImageEncoding::PNG;
            image::ImageMetaData metaData;
            if (core::Failed(imageformats::encodeImage(m_data, encoding, pngImage, metaData))) {
                core::debugLog(core::LogLevel::Error, "Could not convert image to PNG");
                loadAsErrorImage();
                return GltfError::InvalidInput;
            }
            rawData = std::move(pngImage);
            changeUriToCorrectFileExtension(encoding);
        }
        setMimeType(encoding);
    }
    gltf::GltfResult result = setEncodedBytes(rawData);
    if (core::Failed(result)) {
        core::debugLog(core::LogLevel::Error, "Image: failed to set encoded bytes");
    }

    return result;
}

GltfResult Image::loadDataFromEmbedded()
{
    core::string mimeType = "";
    core::Vector<uint8_t> rawData;
    embedded_data::extractBase64Data(m_uri, mimeType, rawData);
    setMimeType(imageformats::imageEncodingFromString(mimeType));

    if (core::Failed(imageformats::decodeImage(rawData, m_data))) {
        loadAsErrorImage();
        return GltfError::ObjectNotFound;
    }

    setEncodedBytes(rawData);
    return GltfResult::Success;
}

GltfResult Image::setData(image::Image&& _data)
{
    m_data = std::move(_data);
    m_imageChangedForRenderer = true;
    return GltfResult::Success;
}

uint64_t Image::getSizeInBytes()
{
    const core::Vector<uint8_t>* encodedData = getEncodedBytes();
    if (encodedData == nullptr) {
        return 0u;
    } else {
        return encodedData->size();
    }
}

bool Image::isValidBasisUColorSpace() const
{
    return true;
    /*if (!std::holds_alternative<image::KTXMetaData>(m_data.m_metaData))
        {
                return false;
        }
        const image::KTXMetaData& info = std::get<image::KTXMetaData>(m_data.m_metaData);
        //        KHR_DF_PRIMARIES_BT709      KHR_DF_TRANSFER_SRGB            KHR_DF_PRIMARIES_UNSPECIFIED   KHR_DF_TRANSFER_LINEAR
        return (info.colorPrimaries == 1 && info.transferFunction == 2) || (info.colorPrimaries == 0 && info.transferFunction == 1);
        */
}

bool Image::isValidBasisUCompression() const
{
    if (!std::holds_alternative<image::KTXMetaData>(m_data.m_metaData)) {
        return false;
    }
    const image::KTXMetaData& info = std::get<image::KTXMetaData>(m_data.m_metaData);
    return (info.supercompressionSchema == image::KtxSupercompressionScheme::BASISLZ && info.colorModel == image::KtxCompressionScheme::ETC1S) || ((info.supercompressionSchema == image::KtxSupercompressionScheme::NONE || info.supercompressionSchema == image::KtxSupercompressionScheme::ZSTANDARD) && info.colorModel == image::KtxCompressionScheme::UASTC);
}

bool Image::isValidBasisUDimensions() const
{
    return m_data.getHeight() % 4 == 0 && m_data.getWidth() % 4 == 0;
}

bool Image::isValidBasisUTexture() const
{
    return true;
    // return !(m_data.getDepth() != 1 || m_data.getFaceCount() != 1);
}

bool Image::isValid() const
{
    if (m_bufferView != nullptr) {
        if (!isValidEncoding(m_mimeType)) {
            return false;
        }
    }

    if (m_mimeType == imageformats::ImageEncoding::KTX2) {
        if (!isValidBasisUTexture()) {
            core::logWarning("Ktx image is not a 2D Texture");
            return false;
        }
        if (!isValidBasisUDimensions()) {
            core::logWarning("Ktx image does not follow BasisU extension specification: Height or width not multiples of 4");
            return false;
        }

        if (!isValidBasisUColorSpace()) {
            core::logWarning("Ktx image does not follow BasisU extension specification: Wrong color space");
            return false;
        }
        if (!isValidBasisUCompression()) {
            core::logWarning("Ktx image does not follow BasisU extension specification: Wrong compression");
            return false;
        }
    }

    return true;
}

GltfResult Image::getInvalidProperties(core::Vector<core::string>& _invalidProperties) const
{
    GltfResult result = GltfResult::Success;
    if (m_bufferView != nullptr) {
        if (!isValidEncoding(m_mimeType)) {
            if (_invalidProperties.push_back("mimeType") == nullptr) {
                result = GltfError::AllocationFailure;
            }
        }
    }
    // The image format must be jpg or png.
    else if (!isValidEncoding(imageformats::getEncodingFromUri(m_uri))) {
        if (_invalidProperties.push_back("uri") == nullptr) {
            result = GltfError::AllocationFailure;
        }
    }
    if (m_mimeType == imageformats::ImageEncoding::KTX2) {
        if (!isValidBasisUTexture()) {
            if (_invalidProperties.push_back("basisUTexture") == nullptr) {
                result = GltfError::AllocationFailure;
            }
        }
        if (!isValidBasisUDimensions()) {
            if (_invalidProperties.push_back("basisUDimensions") == nullptr) {
                result = GltfError::AllocationFailure;
            }
        }

        if (!isValidBasisUColorSpace()) {
            if (_invalidProperties.push_back("basisUColorSpace") == nullptr) {
                result = GltfError::AllocationFailure;
            }
        }
        if (!isValidBasisUCompression()) {
            if (_invalidProperties.push_back("basisUCompression") == nullptr) {
                result = GltfError::AllocationFailure;
            }
        }
    }
    return result;
}

void Image::changeUriToCorrectFileExtension(imageformats::ImageEncoding _mimeType)
{
    if (m_uri == "" || m_uri.find_last_of(".") == std::string::npos) {
        return;
    }
    core::string fileExtension = imageformats::getFileExtension(_mimeType);
    core::string tmpUri = m_uri.substr(0, m_uri.find_last_of("."));
    m_uri = tmpUri + fileExtension;
}

core::Vector<uint32_t> Image::getResolution() const
{
    return { m_data.getWidth(), m_data.getHeight() };
}

uint32_t Image::m_uniqueIdCounter { 1 };

void Image::convertToStorageFormat(StorageFormat _storageFormat)
{
    if (_storageFormat == m_storageFormat) {
        return;
    }
    // set storage format also sets m_historyDataIsDirty to true
    // which is needed to avoid crahses in the state history when the new uri is not stored in there
    setStorageFormat(_storageFormat);
    if (_storageFormat == StorageFormat::File) {
        imageformats::ImageEncoding imgEncoding = m_mimeType;
        if (m_mimeType == imageformats::ImageEncoding::Undefined) {
            imgEncoding = imageformats::getEncodingFromBuffer(m_encodedData);
            setMimeType(imgEncoding);
        }

        if (m_Name.has_value()) {
            m_uri = m_Name.value() + imageformats::getFileExtension(imgEncoding);
        } else {
            m_uri = createURI();
        }

    } else if (_storageFormat == StorageFormat::Embedded) {
        m_uri = "base64_embedded_data_" + std::to_string(m_uniqueIdCounter++);
    } else {
        m_uri = "";
    }
}

std::string Image::createURI()
{
    std::string newUri = "";
    if (m_storageFormat != StorageFormat::File) {
        return newUri;
    }
    newUri += "image_";
    newUri += std::to_string(m_uniqueIdCounter++);
    newUri += imageformats::getFileExtension(m_mimeType);
    return newUri;
}

std::string Image::createUniqueURI(const gltf::Gltf& gltf)
{
    std::string uniqueUri = "";

    for (auto uri = createURI(); uri != uniqueUri; uri = createURI()) {
        if (gltf::utils::isURIUsed(uri, gltf) == false) {
            return uri;
        }
    }

    return uniqueUri;
}

bool Image::hasTextureChangedForRenderer()
{
    if (m_imageChangedForRenderer) {
        m_imageChangedForRenderer = false;
        return true;
    }
    return false;
}

GltfResult Image::setXMPPacket(Packet* _xmpPacket)
{
    if (m_xmpPacket.has_value() == false) {
        m_xmpPacket.emplace(PacketContainer());
    }
    m_xmpPacket.value().m_packet = _xmpPacket;
    return GltfResult::Success;
}

GltfResult Image::unsetXMPPacket()
{
    m_xmpPacket.reset();
    return GltfResult::Success;
}
