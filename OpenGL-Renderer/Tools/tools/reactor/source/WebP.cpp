// Copyright © 2022 UX3D GmbH. All rights reserved.
#include "WebP.h"
#include "ImageFormat.h"
#include "reactor/GpuTypes.h"
#include "reactor/ImageOperations.h"

#include "webp/decode.h"
#include "webp/encode.h"

#include <algorithm>
#include <cstring>
#include <thread>

using namespace ux3d;
using namespace ux3d::reactor;
using namespace ux3d::reactor::image;
#include "webp/decode.h"
ImageData image::decodeWebP(const std::vector<uint8_t>& rawData)
{
    ImageData imageData;

    // get image dimensions from webp data
    int width = 0;
    int height = 0;
    if (!WebPGetInfo(rawData.data(), rawData.size(), &width, &height)) {
        throw std::runtime_error("Could not get image info from webp data");
    }

    WebPBitstreamFeatures features;
    if (WebPGetFeatures(rawData.data(), rawData.size(), &features) != VP8_STATUS_OK) {
        throw std::runtime_error("Could not get image features from webp data");
    }

    // decode webp data
    std::vector<uint8_t>
        decodedData;

    if (features.has_alpha) {
        size_t size = width * height * 4;
        (void)decodedData.resize(size);
        uint8_t* outputBuffer = WebPDecodeRGBAInto(rawData.data(), rawData.size(), decodedData.data(), size, width * 4);

        if (outputBuffer == nullptr) {
            throw std::runtime_error("Could not decode webp data");
        }
    } else {
        size_t size = width * height * 3;
        (void)decodedData.resize(size);
        uint8_t* outputBuffer = WebPDecodeRGBInto(rawData.data(), rawData.size(), decodedData.data(), size, width * 3);

        if (outputBuffer == nullptr) {
            throw std::runtime_error("Could not decode webp data");
        }
    }

    imageData.data = decodedData;
    imageData.info["width"] = width;
    imageData.info["height"] = height;
    if (features.has_alpha) {
        imageData.info["channels"] = 4;
        imageData.info["pixelByteSize"] = 4;
    } else {
        imageData.info["channels"] = 3;
        imageData.info["pixelByteSize"] = 3;
    }

    return imageData;
}

nlohmann::json image::decodeWebPInfo(const std::vector<uint8_t>& rawData)
{

    // get image dimensions from webp data
    int width = 0;
    int height = 0;
    if (!WebPGetInfo(rawData.data(), rawData.size(), &width, &height)) {
        throw std::runtime_error("Could not get image info from webp data");
    }

    WebPBitstreamFeatures features;
    if (WebPGetFeatures(rawData.data(), rawData.size(), &features) != VP8_STATUS_OK) {
        throw std::runtime_error("Could not get image features from webp data");
    }

    nlohmann::json info;
    info["width"] = width;
    info["height"] = height;
    info["mimeType"] = "image/webp";
    if (features.has_alpha) {
        info["channels"] = 4;
        info["pixelByteSize"] = 4;
    } else {
        info["channels"] = 3;
        info["pixelByteSize"] = 3;
    }

    return info;
}

std::vector<uint8_t> image::encodeWebP(const ImageData& image, const nlohmann::json& compressionConfig)
{
    // encode image data to webp
    size_t outputSize = 0;

    int width = image.info["width"];
    int height = image.info["height"];
    int channels = image.info["channels"];

    uint8_t* textureBytes = nullptr;

    int compression = 100;
    try {
        compression = compressionConfig.at("quality");
    } catch (...) {
    }

    if (channels == 4) {
        outputSize = WebPEncodeRGBA(image.data.data(), width, height, width * channels, compression, &textureBytes);
    } else {
        outputSize = WebPEncodeRGB(image.data.data(), width, height, width * channels, compression, &textureBytes);
    }

    if (outputSize == 0) {
        throw std::runtime_error("Could not encode image data to webp");
    }

    // copy the data from the libktx allocated memory to our output vector
    std::vector<uint8_t> output;
    output.resize(outputSize);
    std::memcpy(output.data(), textureBytes, output.size());

    // free the memory allocated by webp
    WebPFree(textureBytes);

    return output;
}
