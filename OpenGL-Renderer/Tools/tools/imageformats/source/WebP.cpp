// Copyright © 2024 UX3D GmbH. All rights reserved.
// Authors: Phillip Hohenester

#include "core/Platform.h"
#include "imageformats/imageformats.h"
#include "webp/decode.h"

#include "imageformats/WebP.h"

#include <algorithm>
#include <cstring>

using namespace ux3d;
using namespace ux3d::imageformats;

ImageDecodingResult imageformats::decodeWebP(const core::Vector<uint8_t>& _buffer, image::Image& _image)
{
    // get image dimensions from webp data
    int width = 0;
    int height = 0;
    if (!WebPGetInfo(_buffer.data(), _buffer.size(), &width, &height)) {
        throw std::runtime_error("Could not get image info from webp data");
    }

    WebPBitstreamFeatures features;
    if (WebPGetFeatures(_buffer.data(), _buffer.size(), &features) != VP8_STATUS_OK) {
        throw std::runtime_error("Could not get image features from webp data");
    }

    // decode webp data
    std::vector<uint8_t>
        decodedData;

    gpu::Format format = gpu::Format::R8G8B8A8_UNORM;

    if (features.has_alpha) {
        size_t size = width * height * 4;
        (void)decodedData.resize(size);
        uint8_t* outputBuffer = WebPDecodeRGBAInto(_buffer.data(), _buffer.size(), decodedData.data(), size, width * 4);

        if (outputBuffer == nullptr) {
            throw std::runtime_error("Could not decode webp data");
        }
    } else {
        size_t size = width * height * 3;
        (void)decodedData.resize(size);
        uint8_t* outputBuffer = WebPDecodeRGBInto(_buffer.data(), _buffer.size(), decodedData.data(), size, width * 3);

        format = gpu::Format::R8G8B8_UNORM;

        if (outputBuffer == nullptr) {
            throw std::runtime_error("Could not decode webp data");
        }
    }

    _image.initializeLayout(width, height, 1, 1, 1, 1, format);

    if (false == _image.initialize(std::move(decodedData))) {
        core::debugLog(core::LogLevel::Error, "Failed to initialize image");
        return ImageDecodingError::InitializeFailure;
    }

    return ImageDecodingResult::Success;
}
