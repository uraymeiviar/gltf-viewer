// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ImageDecodingResult.h"
#include "ImageEncodingResult.h"

#include "WebP.h"
#include "core/Result.h"
#include "core/Slice.h"
#include "core/String.h"
#include "core/Vector.h"
#include "filesystem/File.h"
#include "image/Image.h"
#include "ktx2.h"

#include <vector>

namespace ux3d {
namespace slimktx2 {
    enum class Format : uint32_t;
}
namespace imageformats {
    enum class ImageEncoding : uint32_t {
        Undefined,
        PNG,
        JPEG,
        HDR,
        KTX2,
        BMP,
        TGA,
        WEBP
    };

    ImageDecodingResult decodeImageFromFile(const filesystem::File& _inFile, image::Image& _outImage, core::IAllocator* _inAllocator = core::HeapAllocator::defaultInstance(), const uint32_t _inRequiredChannels = 0);

    core::string getFileExtension(ImageEncoding _imageEncoding);

    core::string imageEncodingToString(ImageEncoding _imageEncoding);
    ImageEncoding imageEncodingFromString(const std::string& _mimeType);

    ImageEncoding getEncodingFromUri(const core::string& _inUri);

    ImageEncoding getEncodingFromBuffer(const core::Vector<uint8_t>& _inBuffer);
    ImageEncoding getEncodingFromBuffer(const std::vector<uint8_t>& _inBuffer);

    ImageDecodingResult decodeImage(const core::Vector<uint8_t>& _inBuffer, image::Image& _outImage, const uint32_t _inRequiredChannels = 0);

    ImageEncodingResult encodePNG(const image::Image& image, core::Vector<uint8_t>& buffer);
    ImageEncodingResult encodeJPEG(const image::Image& image, const uint32_t quality, core::Vector<uint8_t>& buffer);
    ImageEncodingResult encodeHDR(const image::Image& image, core::Vector<uint8_t>& buffer);
    ImageEncodingResult encodeImage(const image::Image& _inImage, const imageformats::ImageEncoding _inImageEncoding, core::Vector<uint8_t>& _outBuffer, const image::ImageMetaData& _metaData);

    ImageEncodingResult scaleImage(const image::Image& _inImage, core::Vector<uint8_t>& _outBuffer, const uint32_t _width, const uint32_t _height);

    [[deprecated("isSRGB of image is never set to true currently. Use convertToVKFormat(const image::Image& _inImage, bool _isSRGB) instead")]] ux3d::gpu::Format convertToVKFormat(const image::Image& _inImage);

    ux3d::gpu::Format convertToVKFormat(const image::Image& _inImage, bool _isSRGB);
}
} // !ux3d
