// Copyright © 2022 UX3D GmbH. All rights reserved.

#include "Ktx2.h"
#include "ImageFormat.h"
#include "reactor/GpuTypes.h"
#include "reactor/ImageOperations.h"

// UX3D_BEGIN_IGNORE_WARNINGS
#include <ktx.h>
// UX3D_END_IGNORE_WARNINGS

#include <algorithm>
#include <cstring>
#include <thread>

using namespace ux3d;
using namespace ux3d::reactor;
using namespace ux3d::reactor::image;

// START Copy from KTX-Software utils/scapp.h

template <typename T>
struct clampedOption {
    clampedOption(T& option, T min_v, T max_v)
        : option(option)
        , min(min_v)
        , max(max_v)
    {
    }

    void clear()
    {
        option = 0;
    }

    operator T() const
    {
        return option;
    }

    T operator=(T v)
    {
        option = std::clamp<T>(v, min, max);
        return option;
    }

    T& option;
    T min;
    T max;
};

struct basisOptions : public ktxBasisParams {
    // The remaining numeric fields are clamped within the Basis
    // library.
    clampedOption<ktx_uint32_t> threadCount;
    clampedOption<ktx_uint32_t> qualityLevel;
    clampedOption<ktx_uint32_t> maxEndpoints;
    clampedOption<ktx_uint32_t> maxSelectors;
    clampedOption<ktx_uint32_t> uastcRDODictSize;
    clampedOption<float> uastcRDOQualityScalar;
    clampedOption<float> uastcRDOMaxSmoothBlockErrorScale;
    clampedOption<float> uastcRDOMaxSmoothBlockStdDev;

    basisOptions()
        : threadCount(ktxBasisParams::threadCount, 1, 10000)
        , qualityLevel(ktxBasisParams::qualityLevel, 1, 255)
        , maxEndpoints(ktxBasisParams::maxEndpoints, 1, 16128)
        , maxSelectors(ktxBasisParams::maxSelectors, 1, 16128)
        , uastcRDODictSize(ktxBasisParams::uastcRDODictSize, 256, 65536)
        , uastcRDOQualityScalar(ktxBasisParams::uastcRDOQualityScalar,
              0.001f, 50.0f)
        , uastcRDOMaxSmoothBlockErrorScale(
              ktxBasisParams::uastcRDOMaxSmoothBlockErrorScale,
              1.0f, 300.0f)
        , uastcRDOMaxSmoothBlockStdDev(
              ktxBasisParams::uastcRDOMaxSmoothBlockStdDev,
              0.01f, 65536.0f)
    {
        uint32_t tc = std::thread::hardware_concurrency();
        if (tc == 0)
            tc = 1;
        threadCount.max = tc;
        threadCount = tc;

        structSize = sizeof(ktxBasisParams);
        // - 1 is to match what basisu_tool does (since 1.13).
        compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL - 1;
        qualityLevel.clear();
        maxEndpoints.clear();
        endpointRDOThreshold = 0.0f;
        maxSelectors.clear();
        selectorRDOThreshold = 0.0f;
        normalMap = false;
        separateRGToRGB_A = false;
        preSwizzle = false;
        noEndpointRDO = false;
        noSelectorRDO = false;
        uastc = false; // Default to ETC1S.
        uastcRDO = false;
        uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;
        uastcRDODictSize.clear();
        uastcRDOQualityScalar.clear();
        uastcRDODontFavorSimplerModes = false;
        uastcRDONoMultithreading = false;
        noSSE = false;
        verbose = false; // Default to quiet operation.
        for (int i = 0; i < 4; i++)
            inputSwizzle[i] = 0;
    }
};

// END Copy from KTX-Software utils/scapp.h

ImageData image::decodeKTX(const std::vector<uint8_t>& rawData)
{
    ImageData imageData;
    ktxTexture2* texture;
    KTX_error_code result;
    result = ktxTexture2_CreateFromMemory(rawData.data(), rawData.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
    if (result != KTX_SUCCESS) {
        if (result == KTX_UNSUPPORTED_TEXTURE_TYPE) {
            throw std::runtime_error("Could not parse KTX2. Maybe it's outdated? You can validate it with the ktx2check tool.");
        } else {
            throw std::runtime_error("Failed to decode KTX2 to memory");
        }
    }

    if (ktxTexture2_NeedsTranscoding(texture)) {
        // always transcode to non block compressed (or plain) RGBA format,
        // as neither renderer nor viewer can currently handle compressed gl textures.
        // Also this is the only way to only reflect what is present in the gltf as the
        // transcoding target format is always decided by the renderers
        ktx_texture_transcode_fmt_e tf = KTX_TTF_RGBA32;
        result = ktxTexture2_TranscodeBasis(texture, tf, 0);
        if (result != KTX_SUCCESS) {
            throw std::runtime_error("Failed to transcode texture");
        }
    }

    std::vector<uint8_t> decodedData;
    (void)decodedData.resize(texture->dataSize);

    gpu::Format vkFormat = static_cast<gpu::Format>(texture->vkFormat);

    imageData.info["width"] = texture->baseWidth;
    imageData.info["height"] = texture->baseHeight;
    imageData.info["depth"] = texture->baseDepth;
    imageData.info["numFaces"] = texture->numFaces;
    imageData.info["numLayers"] = texture->numLayers;
    imageData.info["numLevels"] = texture->numLevels;
    imageData.info["vkFormat"] = vkFormat;
    imageData.info["pixelByteSize"] = getFormatSize(vkFormat);
    imageData.info["channels"] = getChannelCount(vkFormat);

    // faces of a cubemap
    for (ktx_uint32_t face = 0; face < texture->numFaces; ++face) {
        // layers of a texture array
        for (ktx_uint32_t layer = 0; layer < texture->numLayers; ++layer) {
            // mipmap level
            for (ktx_uint32_t level = 0; level < texture->numLevels; ++level) {
                ktx_size_t offset;
                result = ktxTexture_GetImageOffset(ktxTexture(texture), level, layer, face, &offset);
                if (result != KTX_SUCCESS) {
                    throw std::runtime_error("Could not get offset from ktx texture");
                }

                uint64_t subimageSize = image::getFaceSize(vkFormat, level, texture->baseWidth, texture->baseHeight, texture->baseDepth);

                size_t imageOffset = image::getOffset(imageData.info, 0u, 0u, 0u, face, layer, level);

                std::memcpy(decodedData.data() + imageOffset, ktxTexture_GetData(ktxTexture(texture)) + offset, subimageSize);
            }
        }
    }

    ktxTexture_Destroy(ktxTexture(texture));

    imageData.data = decodedData;

    return imageData;
}

nlohmann::json image::decodeKTXInfo(const std::vector<uint8_t>& rawData)
{
    ktxTexture2* texture;
    KTX_error_code result;
    result = ktxTexture2_CreateFromMemory(rawData.data(), rawData.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
    if (result != KTX_SUCCESS) {
        if (result == KTX_UNSUPPORTED_TEXTURE_TYPE) {
            throw std::runtime_error("Could not parse KTX2. Maybe it's outdated? You can validate it with the ktx2check tool.");
        } else {
            throw std::runtime_error("Failed to decode KTX2 to memory");
        }
    }

    if (ktxTexture2_NeedsTranscoding(texture)) {
        // always transcode to non block compressed (or plain) RGBA format,
        // as neither renderer nor viewer can currently handle compressed gl textures.
        // Also this is the only way to only reflect what is present in the gltf as the
        // transcoding target format is always decided by the renderers
        ktx_texture_transcode_fmt_e tf = KTX_TTF_RGBA32;
        result = ktxTexture2_TranscodeBasis(texture, tf, 0);
        if (result != KTX_SUCCESS) {
            throw std::runtime_error("Failed to transcode texture");
        }
    }

    nlohmann::json info;
    gpu::Format vkFormat = static_cast<gpu::Format>(texture->vkFormat);
    info["mimeType"] = "image/ktx";
    info["width"] = texture->baseWidth;
    info["height"] = texture->baseHeight;
    info["depth"] = texture->baseDepth;
    info["numFaces"] = texture->numFaces;
    info["numLayers"] = texture->numLayers;
    info["numLevels"] = texture->numLevels;
    info["vkFormat"] = vkFormat;
    info["pixelByteSize"] = getFormatSize(vkFormat);
    info["channels"] = getChannelCount(vkFormat);

    ktxTexture_Destroy(ktxTexture(texture));

    return info;
}

std::vector<uint8_t> image::encodeKTX(const ImageData& image, const nlohmann::json& basisUCompressionConfig)
{
    gpu::Format vkFormat = gpu::Format::UNDEFINED;
    bool isSRGB = true;
    if (basisUCompressionConfig.contains("transferFunction")) {
        isSRGB = basisUCompressionConfig.at("transferFunction").get<uint32_t>() == 2;
    } else if (image.info.contains("transferFunction")) {
        isSRGB = image.info.at("transferFunction").get<uint32_t>() == 2;
    }
    if (isSRGB) {
        if (image.info.at("channels").get<uint32_t>() == 1) {
            vkFormat = gpu::Format::R8_SRGB;
        }
        if (image.info.at("channels").get<uint32_t>() == 2) {
            vkFormat = gpu::Format::R8G8_SRGB;
        }
        if (image.info.at("channels").get<uint32_t>() == 3) {
            vkFormat = gpu::Format::R8G8B8_SRGB;
        }
        if (image.info.at("channels").get<uint32_t>() == 4) {
            vkFormat = gpu::Format::R8G8B8A8_SRGB;
        }
    } else {
        if (image.info.at("channels").get<uint32_t>() == 1) {
            vkFormat = gpu::Format::R8_UNORM;
        }
        if (image.info.at("channels").get<uint32_t>() == 2) {
            vkFormat = gpu::Format::R8G8_UNORM;
        }
        if (image.info.at("channels").get<uint32_t>() == 3) {
            vkFormat = gpu::Format::R8G8B8_UNORM;
        }
        if (image.info.at("channels").get<uint32_t>() == 4) {
            vkFormat = gpu::Format::R8G8B8A8_UNORM;
        }
    }

    // fill the create info for ktx2 (we don't support ktx 1)
    ktxTextureCreateInfo createInfo;
    createInfo.vkFormat = static_cast<ktx_uint32_t>(vkFormat);

    createInfo.baseWidth = image.info.value("width", 0);
    createInfo.baseHeight = image.info.value("height", 0);
    createInfo.baseDepth = image.info.value("depth", 1);
    createInfo.numDimensions = 2u; // at the moment 3d textures are not supported
    createInfo.numLevels = image.info.value("numLevels", 1);
    createInfo.numLayers = image.info.value("numLayers", 1);
    createInfo.numFaces = image.info.value("numFaces", 1);
    createInfo.isArray = static_cast<ktx_bool_t>(createInfo.numLayers != 1);
    createInfo.generateMipmaps = KTX_FALSE;

    KTX_error_code result;
    ktxTexture2* texture;
    result = ktxTexture2_Create(&createInfo,
        KTX_TEXTURE_CREATE_ALLOC_STORAGE,
        &texture);

    if (result != KTX_SUCCESS) {
        throw std::runtime_error("Could not create ktx2 texture");
    }

    // faces of a cubemap
    for (ktx_uint32_t face = 0; face < image.info.value("numFaces", 1); ++face) {
        // layers of a texture array
        for (ktx_uint32_t layer = 0; layer < image.info.value("numLayers", 1); ++layer) {
            // mipmap level
            for (ktx_uint32_t level = 0; level < image.info.value("numLevels", 1); ++level) {
                size_t blockOffset = image::getOffset(image.info, 0u, 0u, 0u, face, layer, level);
                size_t blockSize = image::getFaceSize(static_cast<gpu::Format>(createInfo.vkFormat), level, image.info.value("width", 0), image.info.value("height", 0));

                if (blockOffset + blockSize > image.data.size()) {
                    throw std::runtime_error("Out of bounds of image while encoding pixels to ktx image");
                }

                const ktx_uint8_t* blockBegin = image.data.data();
                result = ktxTexture_SetImageFromMemory(ktxTexture(texture), level, layer, face, blockBegin + blockOffset, blockSize);

                if (result != KTX_SUCCESS) {
                    throw std::runtime_error("Could not create ktx2 texture");
                }
            }
        }
    }

    ktxHashList* ht = &texture->kvDataHead;
    char orientation[10];
    orientation[0] = 'r';
    orientation[1] = 'd';
    orientation[2] = 0;

    ktxHashList_AddKVPair(ht, KTX_ORIENTATION_KEY,
        (unsigned int)strlen(orientation) + 1,
        orientation);

    // Add required writer metadata.
    std::string id = "Gestaltor";
    ktxHashList_AddKVPair(&texture->kvDataHead, KTX_WRITER_KEY,
        (ktx_uint32_t)id.length() + 1,
        id.c_str());

    basisOptions params;

    if (basisUCompressionConfig.at("compression").get<std::string>() == std::string("ETC1S")) {
        params.uastc = false;
        params.uastcRDOMaxSmoothBlockErrorScale = 0;
        params.uastcRDOMaxSmoothBlockStdDev = 0;
    }

    if (basisUCompressionConfig.at("compression").get<std::string>() == "UASTC") {
        params.uastc = true;
        params.uastcRDO = basisUCompressionConfig.at("uastcRDO").get<double>();
        params.uastcFlags = std::clamp<uint32_t>(basisUCompressionConfig.at("uastcQualityLevel").get<uint64_t>(), 0, KTX_PACK_UASTC_MAX_LEVEL);

        if (basisUCompressionConfig.at("superCompression").get<std::string>() != "NONE" && basisUCompressionConfig.at("superCompression").get<std::string>() != "ZSTANDARD") {
            throw std::runtime_error("KTX2 Supercompression misconfiguration");
        }
    }

    params.compressionLevel = basisUCompressionConfig.at("etc1sCompressionLevel").get<uint64_t>();
    params.qualityLevel = basisUCompressionConfig.at("etc1sQualityLevel").get<uint64_t>();

    result = ktxTexture2_CompressBasisEx(texture, &params);

    if (result != KTX_SUCCESS) {
        throw std::runtime_error("Unable to compress KTX texture");
    }

    if (basisUCompressionConfig.at("superCompression").get<std::string>() == "ZSTANDARD") {
        result = ktxTexture2_DeflateZstd(texture, basisUCompressionConfig.at("zstdCompressionLevel").get<uint64_t>());
        if (KTX_SUCCESS != result) {
            throw std::runtime_error("Unable to compress KTX texture");
        }
    }

    // write the ktx2 file data to memory that is allocated by libktx
    ktx_size_t textureSize;
    ktx_uint8_t* textureBytes = nullptr;
    result = ktxTexture_WriteToMemory(ktxTexture(texture), &textureBytes, &textureSize);
    if (result != KTX_SUCCESS) {
        throw std::runtime_error("Could not write ktx2 encoded data");
    }

    // copy the data from the libktx allocated memory to our output vector
    std::vector<uint8_t> output;
    output.resize(textureSize);
    std::memcpy(output.data(), textureBytes, output.size());

    // we need to cleanup the libktx allocated memory ourselves
    free(textureBytes);
    ktxTexture_Destroy(ktxTexture(texture));

    return output;
}
