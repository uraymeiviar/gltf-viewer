#include "gltf/StorageTypeConverter.h"
#include "filesystem/File.h"
#include "filesystem/FileSystem.h"
#include "gltf/AudioSource.h"
#include "gltf/Buffer.h"
#include "gltf/EmbeddedData.h"
#include "gltf/Image.h"

using namespace ux3d;
using namespace ux3d::gltf;

GltfResult StorageTypeConverter::convertImage(gltf::Image& _image,
    uint32_t _width, uint32_t _height, imageformats::ImageEncoding _encoding, const image::ImageMetaData& _metaData)
{
    // calculate image size
    const size_t pixelSize = static_cast<size_t>((_image.m_data.getChannelSize(image::Channel::RED) + _image.m_data.getChannelSize(image::Channel::GREEN) + _image.m_data.getChannelSize(image::Channel::BLUE) + _image.m_data.getChannelSize(image::Channel::ALPHA)) / 8);

    uint32_t levels = 1 + static_cast<uint32_t>(std::log2(std::max(std::max(_width, _height), _image.m_data.getDepth())));
    levels = std::min(levels, _image.m_data.getLevelCount());
    size_t imageLength = 0;
    for (uint32_t level = 0; level < levels; level++) {
        imageLength += static_cast<size_t>(std::max<uint64_t>(_width >> level, 1ull)) * static_cast<size_t>(std::max<uint64_t>(_height >> level, 1ull)) * static_cast<size_t>(std::max<uint64_t>(_image.m_data.getDepth() >> level, 1ull)) * pixelSize * static_cast<size_t>(_image.m_data.getFaceCount()) * static_cast<size_t>(_image.m_data.getArrayCount());
    }

    // create vector for scaled image data
    core::Vector<uint8_t> scaledData(core::HeapAllocator::defaultInstance());
    if (scaledData.resize(imageLength) == false) {
        core::logError("Cannot set image dimensions");
        return GltfError::InvalidInput;
    }

    // scale image
    bool scalingResult = false;
    scalingResult = imageformats::scaleImage(_image.m_data, scaledData, _width, _height);
    if (scalingResult == false) {
        core::logError("Cannot set image dimensions");
        return GltfError::InvalidInput;
    }

    // initialize scaled image and store in m_data
    {
        image::Image scaledImage;
        scaledImage.initializeLayout(
            _image.m_data.getChannelSize(image::Channel::RED),
            _image.m_data.getChannelSize(image::Channel::GREEN),
            _image.m_data.getChannelSize(image::Channel::BLUE),
            _image.m_data.getChannelSize(image::Channel::ALPHA),
            _image.m_data.getFormat(),
            _image.m_data.isSigned(),
            _image.m_data.isSrgb(),
            _image.m_data.isPacked(),
            _image.m_data.isCompressed(),
            _image.m_data.getChannelIndex(image::Channel::RED),
            _image.m_data.getChannelIndex(image::Channel::GREEN),
            _image.m_data.getChannelIndex(image::Channel::BLUE),
            _image.m_data.getChannelIndex(image::Channel::ALPHA));
        scaledImage.initialize(
            scaledData.data(),
            imageLength,
            _width,
            _height,
            _image.m_data.getDepth(),
            _image.m_data.getFaceCount(),
            _image.m_data.getArrayCount(),
            levels);
        scaledImage.m_metaData = _image.m_data.m_metaData;
        _image.m_data = std::move(scaledImage);
    }

    core::Vector<uint8_t> rawData;
    imageformats::ImageEncodingResult result = imageformats::encodeImage(_image.m_data, _encoding, rawData, _metaData);
    if (core::Failed(result)) {
        core::logError("Cannot set image dimensions");
        return GltfError::InvalidInput;
    }

    image::Image newImageData = image::Image();
    if (!imageformats::decodeImage(rawData, newImageData)) {
        core::logError("Cannot convert image to new mime type");
        return GltfError::InvalidInput;
    }

    _image.m_data = std::move(newImageData);
    _image.changeUriToCorrectFileExtension(_encoding);
    _image.m_data.m_metaData = _metaData;
    _image.setMimeType(_encoding);

    _image.textureChangedForRenderer();

    _image.setEncodedBytes(rawData);
    return GltfResult::Success;
}

ConversionResult StorageTypeConverter::encodeImage(const Image& _image, core::Vector<uint8_t>& _imageData)
{
    const core::Vector<uint8_t>* encodedData = _image.getEncodedBytes();
    if (encodedData != nullptr) {
        return _imageData.assign(*encodedData) ? ConversionResult::Success : ConversionError::AllocationFailure;
    }

    imageformats::ImageEncoding encoding = determineEncoding(_image);

    imageformats::ImageEncodingResult result = imageformats::encodeImage(_image.getData(), encoding, _imageData, _image.getData().m_metaData);

    return core::Succeeded(result) ? ConversionResult::Success : ConversionError::EncodeFailure;
}

core::string StorageTypeConverter::generateUri(const Image& _image, const uint32_t _index, const filesystem::File& _gltfFile)
{
    if (_image.getUri().empty() || embedded_data::isDataUri(_image.getUri())) {
        return _gltfFile.getStem() + "_image_" + std::to_string(_index) + imageformats::getFileExtension(_image.getMimeType());
    }

    filesystem::File newFile = _gltfFile.getFilesystem().fromURI(_image.getUri());
    if (newFile.isRelative()) {
        return newFile.toString();
    } else {
        // we do not store absolute paths, so just use files name
        return newFile.getName();
    }
}

core::string StorageTypeConverter::generateUri(const AudioSource& audioSource, const uint32_t _index, const filesystem::File& _gltfFile)
{
    if (audioSource.getUri().empty() || embedded_data::isDataUri(audioSource.getUri())) {
        return _gltfFile.getStem() + "_audioSource_" + std::to_string(_index) + ".mp3";
    }

    filesystem::File newFile = _gltfFile.getFilesystem().fromURI(audioSource.getUri());
    if (newFile.isRelative()) {
        return newFile.toString();
    } else {
        // we do not store absolute paths, so just use files name
        return newFile.getName();
    }
}

core::string StorageTypeConverter::generateUri(const uint32_t _bufferIndex, const filesystem::File& _gltfFile, const Buffer* _buffer, bool _showLog)
{

    if (_gltfFile.toString() == ".") {
        if (_showLog) {
            core::logWarning("Generate URI: empty FileName");
        }
        return "buffer_" + std::to_string(_bufferIndex);
    }

    if (_buffer != nullptr) {
        if (!_buffer->getUri().empty() && !embedded_data::isDataUri(_buffer->getUri())) {
            filesystem::File newFile = _gltfFile.getFilesystem().fromURI(_buffer->getUri());
            if (newFile.isRelative()) {
                return newFile.toString();
            } else {
                // we do not store absolute paths, so just use files name
                return newFile.getName();
            }
        }
    }

    if (_bufferIndex == 0) {
        return _gltfFile.getStem() + ".bin";
    } else {
        return _gltfFile.getStem() + "_" + std::to_string(_bufferIndex) + ".bin";
    }
}

imageformats::ImageEncoding StorageTypeConverter::determineEncoding(const Image& _image)
{
    if (Image::isValidEncoding(_image.getMimeType())) {
        return _image.getMimeType();
    }
    core::logWarning("Image '%s' has no known mime type, trying to determine from uri...", _image.getUri().c_str());
    imageformats::ImageEncoding fromUri = imageformats::getEncodingFromUri(_image.getUri());
    if (Image::isValidEncoding(fromUri)) {
        return fromUri;
    }
    return imageformats::ImageEncoding::PNG;
}
