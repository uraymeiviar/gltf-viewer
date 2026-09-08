// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#include "imageformats/ktx2.h"
#include "core/Platform.h"
#include "image/ImageMetaData.h"
#include "imageformats/imageformats.h"

UX3D_BEGIN_IGNORE_WARNINGS
#include <ktx.h>
UX3D_END_IGNORE_WARNINGS

#include <algorithm>
#include <cstring>
#include <thread>

using namespace ux3d;
using namespace ux3d::imageformats;

//START Copy from KTX-Software utils/scapp.h

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

//END Copy from KTX-Software utils/scapp.h

void imageformats::fillKTXMetaData(const ktxTexture2* texture, image::Image& _image, const core::Vector<uint8_t>& _buffer)
{
    if (!std::holds_alternative<image::KTXMetaData>(_image.m_metaData)) {
        _image.m_metaData.emplace<image::KTXMetaData>();
    }
    image::KTXMetaData& info = std::get<image::KTXMetaData>(_image.m_metaData);
    //KTX-Software does not correctly detect all schemes. Therefore, we extract it via the buffer.
    uint32_t scheme = _buffer[44] + (static_cast<uint32_t>(_buffer[45]) << 8) + (static_cast<uint32_t>(_buffer[46]) << 16) + (static_cast<uint32_t>(_buffer[47]) << 24);
    info.supercompressionSchema = static_cast<image::KtxSupercompressionScheme>(scheme);
    uint8_t* dfd = reinterpret_cast<uint8_t*>(texture->pDfd);
    info.colorModel = static_cast<image::KtxCompressionScheme>(dfd[12]);
    info.colorPrimaries = dfd[13];
    info.transferFunction = dfd[14];
}

ImageDecodingResult imageformats::decodeKTX(const core::Vector<uint8_t>& _buffer, image::Image& _image)
{
    ktxTexture2* texture;
    KTX_error_code result;
    result = ktxTexture2_CreateFromMemory(_buffer.data(), _buffer.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
    if (result != KTX_SUCCESS) {
        if (result == KTX_UNSUPPORTED_TEXTURE_TYPE) {
            core::debugLog(core::LogLevel::Error, "Could not parse KTX2. Maybe it's outdated? You can validate it with the ktx2check tool.");
        } else {
            core::debugLog(core::LogLevel::Error, "Failed to decode KTX2 to memory");
        }
        return ImageDecodingError::LoadFailure;
    }

    fillKTXMetaData(texture, _image, _buffer);

    if (ktxTexture2_NeedsTranscoding(texture)) {
        // always transcode to non block compressed (or plain) RGBA format,
        // as neither renderer nor viewer can currently handle compressed gl textures.
        // Also this is the only way to only reflect what is present in the gltf as the
        // transcoding target format is always decided by the renderers
        ktx_texture_transcode_fmt_e tf = KTX_TTF_RGBA32;
        result = ktxTexture2_TranscodeBasis(texture, tf, 0);
        if (result != KTX_SUCCESS) {
            core::debugLog(core::LogLevel::Error, "Failed to transcode texture");
            return ImageDecodingError::LoadFailure;
        }
    }

    std::vector<uint8_t> imageData;
    (void)imageData.resize(texture->dataSize);

    gpu::Format vkFormat = static_cast<gpu::Format>(texture->vkFormat);
    _image.initializeLayout(texture->baseWidth, texture->baseHeight, texture->baseDepth, texture->numFaces, texture->numLayers, texture->numLevels, vkFormat);

    // faces of a cubemap
    for (ktx_uint32_t face = 0; face < texture->numFaces; ++face) {
        // layers of a texture array
        for (ktx_uint32_t layer = 0; layer < texture->numLayers; ++layer) {
            // mipmap level
            for (ktx_uint32_t level = 0; level < texture->numLevels; ++level) {
                ktx_size_t offset;
                result = ktxTexture_GetImageOffset(ktxTexture(texture), level, layer, face, &offset);
                if (result != KTX_SUCCESS) {
                    core::debugLog(core::LogLevel::Error, "Could not get offset from ktx texture");
                    return ImageDecodingError::LoadFailure;
                }

                uint64_t subimageSize = image::getFaceSize(vkFormat, level, texture->baseWidth, texture->baseHeight, texture->baseDepth);

                size_t imageOffset = _image.getOffset(0u, 0u, 0u, face, layer, level);

                std::memcpy(imageData.data() + imageOffset, ktxTexture_GetData(ktxTexture(texture)) + offset, subimageSize);
            }
        }
    }

    ktxTexture_Destroy(ktxTexture(texture));

    if (false == _image.initialize(std::move(imageData))) {
        core::debugLog(core::LogLevel::Error, "Failed to initialize image");
        return ImageDecodingError::InitializeFailure;
    }

    return ImageDecodingResult::Success;
}

ImageEncodingResult imageformats::encodeKTX(const image::Image& _image, core::Vector<uint8_t>& _buffer, const image::KTXMetaData& _basisUCompressionConfig)
{
    // fill the create info for ktx2 (we don't support ktx 1)
    ktxTextureCreateInfo createInfo;
    createInfo.vkFormat = static_cast<ktx_uint32_t>(convertToVKFormat(_image, _basisUCompressionConfig.transferFunction == 2));
    createInfo.baseWidth = _image.getWidth();
    createInfo.baseHeight = _image.getHeight();
    createInfo.baseDepth = _image.getDepth();
    createInfo.numDimensions = 2u; // at the moment 3d textures are not supported
    createInfo.numLevels = _image.getLevelCount();
    createInfo.numLayers = _image.getArrayCount();
    createInfo.numFaces = _image.getFaceCount();
    createInfo.isArray = static_cast<ktx_bool_t>(_image.getArrayCount() != 1);
    createInfo.generateMipmaps = KTX_FALSE;

    KTX_error_code result;
    ktxTexture2* texture;
    result = ktxTexture2_Create(&createInfo,
        KTX_TEXTURE_CREATE_ALLOC_STORAGE,
        &texture);

    if (result != KTX_SUCCESS) {
        core::debugLog(core::LogLevel::Error, "Could not create ktx2 texture");
        return ImageEncodingError::WriteFailure;
    }

    // faces of a cubemap
    for (ktx_uint32_t face = 0; face < _image.getFaceCount(); ++face) {
        // layers of a texture array
        for (ktx_uint32_t layer = 0; layer < _image.getArrayCount(); ++layer) {
            // mipmap level
            for (ktx_uint32_t level = 0; level < _image.getLevelCount(); ++level) {
                size_t blockOffset = _image.getOffset(0, 0, 0, face, layer, level);
                size_t blockSize = image::getFaceSize(static_cast<gpu::Format>(createInfo.vkFormat), level, _image.getWidth(), _image.getHeight());

                if (blockOffset + blockSize > _image.getPixels().size()) {
                    core::debugLog(core::LogLevel::Error, "Out of bounds of image while encoding pixels to ktx image");
                    return ImageEncodingError::InvalidInput;
                }

                const ktx_uint8_t* blockBegin = _image.getPixels().data();
                result = ktxTexture_SetImageFromMemory(ktxTexture(texture), level, layer, face, blockBegin + blockOffset, blockSize);

                if (result != KTX_SUCCESS) {
                    core::debugLog(core::LogLevel::Error, "could not encode KTX image");
                    return ImageEncodingError::WriteFailure;
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

    if (_basisUCompressionConfig.colorModel == image::KtxCompressionScheme::ETC1S) {
        params.uastc = false;
        params.uastcRDOMaxSmoothBlockErrorScale = 0;
        params.uastcRDOMaxSmoothBlockStdDev = 0;
    }

    if (_basisUCompressionConfig.colorModel == image::KtxCompressionScheme::UASTC) {
        params.uastc = true;
        params.uastcRDO = _basisUCompressionConfig.uastcRDO;
        params.uastcFlags = std::clamp<uint32_t>(_basisUCompressionConfig.uastcQualityLevel, 0, KTX_PACK_UASTC_MAX_LEVEL);
        if (_basisUCompressionConfig.supercompressionSchema != image::KtxSupercompressionScheme::NONE && _basisUCompressionConfig.supercompressionSchema != image::KtxSupercompressionScheme::ZSTANDARD) {
            core::debugLog(core::LogLevel::Error, "KTX2 Supercompression misconfiguration");
            return ImageEncodingError::CompressionError;
        }
    }

    params.compressionLevel = _basisUCompressionConfig.etc1sCompressionLevel;
    params.qualityLevel = _basisUCompressionConfig.etc1sQualityLevel;

    result = ktxTexture2_CompressBasisEx(texture, &params);

    if (result != KTX_SUCCESS) {
        core::debugLog(core::LogLevel::Error, "Could not compress KTX texture with config");
        return ImageEncodingError::CompressionError;
    }

    if (_basisUCompressionConfig.supercompressionSchema == image::KtxSupercompressionScheme::ZSTANDARD) {
        result = ktxTexture2_DeflateZstd(texture, _basisUCompressionConfig.zstdCompressionLevel);
        if (KTX_SUCCESS != result) {
            core::debugLog(core::LogLevel::Error, "Could not apply supercompression");
        }
    }

    // write the ktx2 file data to memory that is allocated by libktx
    ktx_size_t textureSize;
    ktx_uint8_t* textureBytes = nullptr;
    result = ktxTexture_WriteToMemory(ktxTexture(texture), &textureBytes, &textureSize);
    if (result != KTX_SUCCESS) {
        core::debugLog(core::LogLevel::Error, "Could not write ktx2 encoded data");
        return ImageEncodingError::WriteFailure;
    }

    // copy the data from the libktx allocated memory to our output vector
    (void)_buffer.resize(textureSize);
    std::memcpy(_buffer.data(), textureBytes, _buffer.size());

    // we need to cleanup the libktx allocated memory ourselves
    free(textureBytes);
    ktxTexture_Destroy(ktxTexture(texture));

    return ImageEncodingResult::Success;
}
