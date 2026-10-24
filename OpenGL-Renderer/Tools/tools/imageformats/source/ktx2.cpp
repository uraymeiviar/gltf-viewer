// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#include "imageformats/ktx2.h"
#include "core/Platform.h"
#include "image/ImageFormat.h"
#include "image/ImageMetaData.h"
#include "imageformats/imageformats.h"

// The Inspector preview decodes the same BCn blocks the GPU uploads. Dds.cpp already
// compiles bcdec for its own path; with BCDEC_STATIC this translation unit gets its own
// copy, and BCDEC_BC4BC5_PRECISE exposes the signed BC5 decoder.
#define BCDEC_STATIC
#define BCDEC_IMPLEMENTATION
#define BCDEC_BC4BC5_PRECISE
#include "bcdec.h"

UX3D_BEGIN_IGNORE_WARNINGS
#include <ktx.h>
UX3D_END_IGNORE_WARNINGS

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>

using namespace ux3d;
using namespace ux3d::imageformats;

namespace {

constexpr uint8_t ktx2Identifier[12] = { 0xAB, 'K', 'T', 'X', ' ', '2', '0', 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

uint32_t readU32LE(const core::Vector<uint8_t>& _buffer, size_t _offset)
{
    uint32_t value = 0;
    if (_offset + sizeof(value) <= _buffer.size()) {
        std::memcpy(&value, _buffer.data() + _offset, sizeof(value));
    }
    return value;
}

uint64_t readU64LE(const core::Vector<uint8_t>& _buffer, size_t _offset)
{
    uint64_t value = 0;
    if (_offset + sizeof(value) <= _buffer.size()) {
        std::memcpy(&value, _buffer.data() + _offset, sizeof(value));
    }
    return value;
}

// The unsupported-format placeholder is opaque white, the same visual result as the
// previous failed decode path, but it keeps the KTX2 bytes so the renderer can log the file.
bool initializeWhitePlaceholder(image::Image& _image)
{
    const uint8_t white[4] = { 255, 255, 255, 255 };
    if (!_image.initializeLayout(8, 8, 8, 8, image::Format::NORM, false, false, false, false, 0, 1, 2, 3)) {
        return false;
    }
    return _image.initialize(white, sizeof(white), 1, 1, 1, 1, 1, 1);
}

} // namespace

bool imageformats::getKTX2ImageInfo(const core::Vector<uint8_t>& _buffer, KTX2ImageInfo& _info)
{
    if (_buffer.size() < 80u || std::memcmp(_buffer.data(), ktx2Identifier, sizeof(ktx2Identifier)) != 0) {
        return false;
    }

    _info = KTX2ImageInfo();
    _info.format = static_cast<gpu::Format>(readU32LE(_buffer, 12));
    _info.width = readU32LE(_buffer, 20);
    _info.height = readU32LE(_buffer, 24);
    _info.depth = std::max(readU32LE(_buffer, 28), 1u);
    _info.layerCount = std::max(readU32LE(_buffer, 32), 1u);
    _info.faceCount = std::max(readU32LE(_buffer, 36), 1u);
    _info.levelCount = std::clamp(readU32LE(_buffer, 40), 1u, 32u);
    _info.supercompressionScheme = readU32LE(_buffer, 44);
    _info.blockCompressed = image::isCompressed(_info.format);
    return true;
}

bool imageformats::isSupportedCompressedFormat(gpu::Format _format)
{
    switch (_format) {
    case gpu::Format::BC1_RGB_UNORM_BLOCK:
    case gpu::Format::BC1_RGB_SRGB_BLOCK:
    case gpu::Format::BC1_RGBA_UNORM_BLOCK:
    case gpu::Format::BC1_RGBA_SRGB_BLOCK:
    case gpu::Format::BC3_UNORM_BLOCK:
    case gpu::Format::BC3_SRGB_BLOCK:
    case gpu::Format::BC5_SNORM_BLOCK:
    case gpu::Format::BC7_UNORM_BLOCK:
    case gpu::Format::BC7_SRGB_BLOCK:
        return true;
    default:
        return false;
    }
}

bool imageformats::isSupportedCompressedKTX2(const KTX2ImageInfo& _info)
{
    return _info.blockCompressed
        && _info.supercompressionScheme == 0u
        && _info.depth == 1u
        && _info.layerCount == 1u
        && _info.faceCount == 1u
        && _info.width > 0u
        && _info.height > 0u
        && isSupportedCompressedFormat(_info.format);
}

uint64_t imageformats::getKTX2CompressedLevelSize(const KTX2ImageInfo& _info, uint32_t _level)
{
    if (!isSupportedCompressedFormat(_info.format)) {
        return 0u;
    }

    uint32_t blockWidth = 1u;
    uint32_t blockHeight = 1u;
    if (!image::getBlockSize(_info.format, blockWidth, blockHeight)) {
        return 0u;
    }

    // KTX2 mip dimensions are max(1, base >> level); image::getFaceSize truncates a
    // non-square texture's last levels to 0, so compute the block counts here.
    const uint32_t width = _level >= 32u ? 1u : std::max(_info.width >> _level, 1u);
    const uint32_t height = _level >= 32u ? 1u : std::max(_info.height >> _level, 1u);
    const uint32_t depth = _level >= 32u ? 1u : std::max(_info.depth >> _level, 1u);
    const uint64_t blocksX = (static_cast<uint64_t>(width) + blockWidth - 1u) / blockWidth;
    const uint64_t blocksY = (static_cast<uint64_t>(height) + blockHeight - 1u) / blockHeight;
    return static_cast<uint64_t>(image::getFormatSize(_info.format)) * blocksX * blocksY * depth;
}

uint64_t imageformats::getKTX2CompressedDataSize(const KTX2ImageInfo& _info)
{
    if (!isSupportedCompressedFormat(_info.format)) {
        return 0u;
    }

    uint64_t total = 0u;
    for (uint32_t level = 0u; level < _info.levelCount; ++level) {
        total += getKTX2CompressedLevelSize(_info, level);
    }
    return total * static_cast<uint64_t>(_info.faceCount) * static_cast<uint64_t>(_info.layerCount);
}

const char* imageformats::getKTX2FormatName(gpu::Format _format)
{
    switch (_format) {
    case gpu::Format::BC1_RGB_UNORM_BLOCK: return "BC1_RGB_UNORM";
    case gpu::Format::BC1_RGB_SRGB_BLOCK: return "BC1_RGB_SRGB";
    case gpu::Format::BC1_RGBA_UNORM_BLOCK: return "BC1_RGBA_UNORM";
    case gpu::Format::BC1_RGBA_SRGB_BLOCK: return "BC1_RGBA_SRGB";
    case gpu::Format::BC2_UNORM_BLOCK: return "BC2_UNORM";
    case gpu::Format::BC2_SRGB_BLOCK: return "BC2_SRGB";
    case gpu::Format::BC3_UNORM_BLOCK: return "BC3_UNORM";
    case gpu::Format::BC3_SRGB_BLOCK: return "BC3_SRGB";
    case gpu::Format::BC4_UNORM_BLOCK: return "BC4_UNORM";
    case gpu::Format::BC4_SNORM_BLOCK: return "BC4_SNORM";
    case gpu::Format::BC5_UNORM_BLOCK: return "BC5_UNORM";
    case gpu::Format::BC5_SNORM_BLOCK: return "BC5_SNORM";
    case gpu::Format::BC6H_UFLOAT_BLOCK: return "BC6H_UFLOAT";
    case gpu::Format::BC6H_SFLOAT_BLOCK: return "BC6H_SFLOAT";
    case gpu::Format::BC7_UNORM_BLOCK: return "BC7_UNORM";
    case gpu::Format::BC7_SRGB_BLOCK: return "BC7_SRGB";
    case gpu::Format::ETC2_R8G8B8_UNORM_BLOCK: return "ETC2_R8G8B8_UNORM";
    case gpu::Format::ETC2_R8G8B8_SRGB_BLOCK: return "ETC2_R8G8B8_SRGB";
    case gpu::Format::ETC2_R8G8B8A8_UNORM_BLOCK: return "ETC2_R8G8B8A8_UNORM";
    case gpu::Format::ETC2_R8G8B8A8_SRGB_BLOCK: return "ETC2_R8G8B8A8_SRGB";
    case gpu::Format::ASTC_4x4_UNORM_BLOCK: return "ASTC_4x4_UNORM";
    case gpu::Format::ASTC_4x4_SRGB_BLOCK: return "ASTC_4x4_SRGB";
    case gpu::Format::UNDEFINED: return "UNDEFINED";
    default: return "UNKNOWN";
    }
}

bool imageformats::decodeKTX2Preview(const core::Vector<uint8_t>& _buffer, uint32_t _maxDimension,
                                     core::Vector<uint8_t>& _rgba, uint32_t& _width, uint32_t& _height,
                                     bool _signedSquareNormal)
{
    KTX2ImageInfo info;
    if (!getKTX2ImageInfo(_buffer, info)
        || !info.blockCompressed
        || info.supercompressionScheme != 0u
        || info.depth != 1u
        || info.layerCount != 1u
        || info.faceCount != 1u
        || !isSupportedCompressedFormat(info.format)) {
        return false;
    }

    uint32_t level = 0u;
    for (uint32_t candidate = 0u; candidate < info.levelCount; ++candidate) {
        const uint32_t w = candidate >= 32u ? 1u : std::max(info.width >> candidate, 1u);
        const uint32_t h = candidate >= 32u ? 1u : std::max(info.height >> candidate, 1u);
        level = candidate;
        if (std::max(w, h) <= std::max(_maxDimension, 1u)) {
            break;
        }
    }

    const uint32_t width = level >= 32u ? 1u : std::max(info.width >> level, 1u);
    const uint32_t height = level >= 32u ? 1u : std::max(info.height >> level, 1u);
    const uint32_t blocksX = (width + 3u) / 4u;
    const uint32_t blocksY = (height + 3u) / 4u;
    const uint64_t levelSize = getKTX2CompressedLevelSize(info, level);
    const uint64_t levelOffset = readU64LE(_buffer, 80u + static_cast<size_t>(level) * 24u);
    if (levelSize == 0u || levelOffset + levelSize > _buffer.size()) {
        return false;
    }

    const uint32_t blockBytes = image::getFormatSize(info.format);
    if (blockBytes == 0u || static_cast<uint64_t>(blocksX) * blocksY * blockBytes > levelSize) {
        return false;
    }

    const bool signedFormat = info.format == gpu::Format::BC5_SNORM_BLOCK;
    _rgba.resize(static_cast<size_t>(width) * height * 4u);
    _width = width;
    _height = height;

    const uint8_t* levelData = _buffer.data() + static_cast<size_t>(levelOffset);
    uint8_t block[4u * 4u * 4u];
    float signedBlock[4u * 4u * 2u];
    for (uint32_t blockY = 0u; blockY < blocksY; ++blockY) {
        for (uint32_t blockX = 0u; blockX < blocksX; ++blockX) {
            const uint8_t* source = levelData + (static_cast<size_t>(blockY) * blocksX + blockX) * blockBytes;
            std::memset(block, 0, sizeof(block));
            switch (info.format) {
            case gpu::Format::BC1_RGB_UNORM_BLOCK:
            case gpu::Format::BC1_RGB_SRGB_BLOCK:
            case gpu::Format::BC1_RGBA_UNORM_BLOCK:
            case gpu::Format::BC1_RGBA_SRGB_BLOCK:
                bcdec_bc1(source, block, 4u * 4u);
                break;
            case gpu::Format::BC3_UNORM_BLOCK:
            case gpu::Format::BC3_SRGB_BLOCK:
                bcdec_bc3(source, block, 4u * 4u);
                break;
            case gpu::Format::BC5_SNORM_BLOCK:
                bcdec_bc5_float(source, signedBlock, 4u * 2u, 1);
                break;
            case gpu::Format::BC7_UNORM_BLOCK:
            case gpu::Format::BC7_SRGB_BLOCK:
                bcdec_bc7(source, block, 4u * 4u);
                break;
            default:
                return false;
            }

            for (uint32_t py = 0u; py < 4u; ++py) {
                const uint32_t y = blockY * 4u + py;
                if (y >= height) {
                    break;
                }
                for (uint32_t px = 0u; px < 4u; ++px) {
                    const uint32_t x = blockX * 4u + px;
                    if (x >= width) {
                        break;
                    }
                    uint8_t* target = _rgba.data() + (static_cast<size_t>(y) * width + x) * 4u;
                    if (signedFormat) {
                        const float* texel = signedBlock + (py * 4u + px) * 2u;
                        float nx = texel[0];
                        float ny = texel[1];
                        if (_signedSquareNormal) {
                            nx = std::copysign(nx * nx, nx);
                            ny = std::copysign(ny * ny, ny);
                        }
                        const float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
                        target[0] = static_cast<uint8_t>(std::clamp((nx * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                        target[1] = static_cast<uint8_t>(std::clamp((ny * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                        target[2] = static_cast<uint8_t>(std::clamp((nz * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                        target[3] = 255u;
                    } else {
                        const uint8_t* texel = block + (py * 4u + px) * 4u;
                        target[0] = texel[0];
                        target[1] = texel[1];
                        target[2] = texel[2];
                        target[3] = texel[3];
                    }
                }
            }
        }
    }

    return true;
}

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
    KTX2ImageInfo info;
    const bool hasInfo = getKTX2ImageInfo(_buffer, info);

    // BCn files with no supercompression stay compressed all the way to glCompressedTexImage2D.
    // Anything else a compressed KTX2 can contain (other vkFormats, supercompression) keeps
    // the previous white result; the renderer logs the file and format.
    if (hasInfo && info.blockCompressed && !isSupportedCompressedKTX2(info)) {
        if (!initializeWhitePlaceholder(_image)) {
            core::debugLog(core::LogLevel::Error, "Failed to initialize KTX2 placeholder image");
            return ImageDecodingError::InitializeFailure;
        }
        return ImageDecodingResult::Success;
    }

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
        // libktx initialises the Basis transcoder's global tables on first use through an
        // unguarded static flag, and images are decoded on parallel loader threads. The
        // first transcode therefore runs alone; later ones run in parallel (P6.2).
        static std::once_flag s_firstTranscode;
        bool transcoded = false;
        std::call_once(s_firstTranscode, [&]() {
            result = ktxTexture2_TranscodeBasis(texture, tf, 0);
            transcoded = true;
        });
        if (!transcoded) {
            result = ktxTexture2_TranscodeBasis(texture, tf, 0);
        }
        if (result != KTX_SUCCESS) {
            core::debugLog(core::LogLevel::Error, "Failed to transcode texture");
            return ImageDecodingError::LoadFailure;
        }
    }

    const gpu::Format vkFormat = static_cast<gpu::Format>(texture->vkFormat);

    if (hasInfo && isSupportedCompressedKTX2(info)) {
        if (vkFormat != info.format
            || texture->baseWidth != info.width
            || texture->baseHeight != info.height
            || texture->numLevels != info.levelCount
            || texture->numFaces != info.faceCount
            || texture->numLayers != info.layerCount) {
            core::debugLog(core::LogLevel::Error, "KTX2 layout changed while loading; refusing to upload compressed data");
            ktxTexture_Destroy(ktxTexture(texture));
            return ImageDecodingError::LoadFailure;
        }

        const uint64_t totalSize = getKTX2CompressedDataSize(info);
        const uint8_t* textureData = ktxTexture_GetData(ktxTexture(texture));
        if (totalSize == 0u || totalSize > std::numeric_limits<size_t>::max() || textureData == nullptr) {
            ktxTexture_Destroy(ktxTexture(texture));
            return ImageDecodingError::LoadFailure;
        }

        std::vector<uint8_t> imageData(static_cast<size_t>(totalSize));
        size_t cursor = 0u;
        for (ktx_uint32_t level = 0; level < texture->numLevels; ++level) {
            for (ktx_uint32_t layer = 0; layer < texture->numLayers; ++layer) {
                for (ktx_uint32_t face = 0; face < texture->numFaces; ++face) {
                    ktx_size_t offset;
                    result = ktxTexture_GetImageOffset(ktxTexture(texture), level, layer, face, &offset);
                    if (result != KTX_SUCCESS) {
                        core::debugLog(core::LogLevel::Error, "Could not get offset from ktx texture");
                        ktxTexture_Destroy(ktxTexture(texture));
                        return ImageDecodingError::LoadFailure;
                    }

                    const uint64_t subimageSize = getKTX2CompressedLevelSize(info, level);
                    if (offset + subimageSize > texture->dataSize || cursor + subimageSize > imageData.size()) {
                        core::debugLog(core::LogLevel::Error, "KTX2 compressed level exceeds its image data");
                        ktxTexture_Destroy(ktxTexture(texture));
                        return ImageDecodingError::LoadFailure;
                    }

                    std::memcpy(imageData.data() + cursor, textureData + offset, static_cast<size_t>(subimageSize));
                    cursor += static_cast<size_t>(subimageSize);
                }
            }
        }

        ktxTexture_Destroy(ktxTexture(texture));

        // Keep the block grid as the image dimensions when the mip chain halves that grid,
        // which is the power-of-two case every MSFS texture uses. The pixel data itself is
        // stored sequentially and Texture::writeCompressed walks it level by level.
        const uint32_t blockBytes = image::getFormatSize(vkFormat);
        const uint32_t blockWidth = std::max((info.width + 3u) / 4u, 1u);
        const uint32_t blockHeight = std::max((info.height + 3u) / 4u, 1u);

        _image.initializeLayout(info.width, info.height, info.depth, info.faceCount, info.layerCount, info.levelCount, vkFormat);
        _image.initializeLayout(blockBytes * 8u, 0u, 0u, 0u, image::Format::INT, image::isSigned(vkFormat), false, false, true, 0u, 1u, 2u, 3u);
        _image.setWidth(blockWidth);
        _image.setHeight(blockHeight);
        if (_image.initialize(std::move(imageData))) {
            return ImageDecodingResult::Success;
        }

        // Non-power-of-two block grids do not halve exactly. Fall back to an opaque one-row
        // layout; the compressed bytes and their level-by-level order are unchanged.
        if (imageData.size() > std::numeric_limits<uint32_t>::max()) {
            return ImageDecodingError::InitializeFailure;
        }
        image::Image opaque;
        if (!opaque.initializeLayout(8u, 0u, 0u, 0u, image::Format::INT, image::isSigned(vkFormat), false, false, true, 0u, 1u, 2u, 3u)
            || !opaque.initialize(imageData.data(), imageData.size(), static_cast<uint32_t>(imageData.size()), 1u, 1u, 1u, 1u, 1u)) {
            core::debugLog(core::LogLevel::Error, "Failed to initialize compressed KTX2 image");
            return ImageDecodingError::InitializeFailure;
        }
        _image = std::move(opaque);
        return ImageDecodingResult::Success;
    }

    std::vector<uint8_t> imageData;
    (void)imageData.resize(texture->dataSize);

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
