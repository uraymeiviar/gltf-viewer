// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "GltfResult.h"
#include "core/String.h"
#include "imageformats/imageformats.h"
#include "json/ConversionResult.h"

namespace ux3d::gltf {
class Image;
class AudioSource;
class Buffer;

class StorageTypeConverter {
public:
    static GltfResult convertImage(gltf::Image& _image, uint32_t _width, uint32_t _height, imageformats::ImageEncoding _mimeType, const image::ImageMetaData& _metaData);
    static ConversionResult encodeImage(const Image& _image, core::Vector<uint8_t>& _imageData);
    static core::string generateUri(const Image& _image, const uint32_t _index, const filesystem::File& _gltfFile);
    static core::string generateUri(const AudioSource& audioSource, const uint32_t _index, const filesystem::File& _gltfFile);
    static core::string generateUri(const uint32_t _index, const filesystem::File& _gltfFile, const Buffer* _buffer = nullptr, bool _showLog = true);

private:
    static imageformats::ImageEncoding determineEncoding(const Image& _image);
};
}
