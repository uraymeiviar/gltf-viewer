// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_HDR
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA

#include <stb_image.h>
#include <stb_image_resize.h>
#include <stb_image_write.h>

#include <ktx.h>
#include <tinyexr.h>

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <mutex>

#include "imageformats/imageformats.h"
#include "Tiff.h"
#include "core/LoadProgress.h"
#include "core/Logger.h"
#include <memory>

using namespace ux3d;
using namespace ux3d::imageformats;

namespace {

std::mutex g_budgetMutex;
std::condition_variable g_budgetReleased;
uint64_t g_budgetBytes = 0;
uint64_t g_inFlightBytes = 0;
uint32_t g_inFlightCount = 0;
DecodeBudgetStats g_budgetStats;

uint32_t readLE32(const core::Vector<uint8_t>& _buffer, size_t _offset)
{
    uint32_t value = 0;
    if (_offset + 4 <= _buffer.size()) {
        std::memcpy(&value, _buffer.data() + _offset, 4);
    }
    return value;
}

// Decoded size from the header, without decoding; conservative where a header is not
// parsed here (WebP, EXR: 8 bytes per encoded byte).
uint64_t estimateDecodedBytes(const core::Vector<uint8_t>& _buffer, ImageEncoding _encoding)
{
    if (_encoding == ImageEncoding::TIFF) {
        try {
            const tiff::Reader reader(std::span<const uint8_t>(_buffer.data(), _buffer.size()));
            const auto size = reader.dimensions();
            return uint64_t(size[0]) * size[1] * 8u;
        } catch (const std::exception&) { return uint64_t(_buffer.size()) * 8u; }
    }
    if (_encoding == ImageEncoding::KTX2) {
        // KTX2 header: pixelWidth @20, pixelHeight @24, pixelDepth @28, layerCount @32,
        // faceCount @36; transcoded to RGBA32, plus mips (x4/3).
        const uint64_t w = std::max<uint32_t>(readLE32(_buffer, 20), 1u);
        const uint64_t h = std::max<uint32_t>(readLE32(_buffer, 24), 1u);
        const uint64_t d = std::max<uint32_t>(readLE32(_buffer, 28), 1u);
        const uint64_t layers = std::max<uint32_t>(readLE32(_buffer, 32), 1u);
        const uint64_t faces = std::max<uint32_t>(readLE32(_buffer, 36), 1u);
        return w * h * d * layers * faces * 4u * 4u / 3u;
    }
    if (_encoding == ImageEncoding::DDS) {
        // DDS: magic, then dwSize @4, dwFlags @8, dwHeight @12, dwWidth @16; up to RGBA32F.
        const uint64_t h = std::max<uint32_t>(readLE32(_buffer, 12), 1u);
        const uint64_t w = std::max<uint32_t>(readLE32(_buffer, 16), 1u);
        return w * h * 16u;
    }
    if (_encoding == ImageEncoding::WEBP || _encoding == ImageEncoding::EXR) {
        return static_cast<uint64_t>(_buffer.size()) * 8u;
    }
    int x = 0;
    int y = 0;
    int channels = 0;
    if (_buffer.size() > INT_MAX
        || !stbi_info_from_memory(static_cast<stbi_uc const*>(_buffer.data()), static_cast<int>(_buffer.size()), &x, &y, &channels)) {
        return static_cast<uint64_t>(_buffer.size()) * 8u;
    }
    const uint64_t bytesPerChannel = _encoding == ImageEncoding::HDR
        ? 4u
        : (stbi_is_16_bit_from_memory(static_cast<stbi_uc const*>(_buffer.data()), static_cast<int>(_buffer.size())) ? 2u : 1u);
    return static_cast<uint64_t>(x) * static_cast<uint64_t>(y) * 4u * bytesPerChannel;
}

// Holds an estimated share of the decode budget for one decode.
class DecodeLease {
public:
    explicit DecodeLease(uint64_t _bytes)
        : m_bytes(_bytes)
    {
        std::unique_lock lock(g_budgetMutex);
        ++g_budgetStats.decodes;
        bool waited = false;
        while (g_budgetBytes != 0 && g_inFlightCount > 0 && g_inFlightBytes + m_bytes > g_budgetBytes) {
            waited = true;
            g_budgetReleased.wait(lock);
        }
        if (waited) {
            ++g_budgetStats.waits;
        }
        g_inFlightBytes += m_bytes;
        ++g_inFlightCount;
        g_budgetStats.peakInFlightBytes = std::max(g_budgetStats.peakInFlightBytes, g_inFlightBytes);
        g_budgetStats.peakInFlightCount = std::max(g_budgetStats.peakInFlightCount, g_inFlightCount);
    }
    ~DecodeLease()
    {
        {
            std::lock_guard lock(g_budgetMutex);
            g_inFlightBytes -= m_bytes;
            --g_inFlightCount;
        }
        g_budgetReleased.notify_all();
    }
    DecodeLease(const DecodeLease&) = delete;
    DecodeLease& operator=(const DecodeLease&) = delete;

private:
    uint64_t m_bytes;
};

} // namespace

void imageformats::setDecodeMemoryBudget(uint64_t _bytes)
{
    {
        std::lock_guard lock(g_budgetMutex);
        g_budgetBytes = _bytes;
        g_budgetStats.budgetBytes = _bytes;
    }
    g_budgetReleased.notify_all();
}

DecodeBudgetStats imageformats::getDecodeBudgetStats()
{
    std::lock_guard lock(g_budgetMutex);
    return g_budgetStats;
}

void imageformats::resetDecodeBudgetStats()
{
    std::lock_guard lock(g_budgetMutex);
    g_budgetStats = DecodeBudgetStats();
    g_budgetStats.budgetBytes = g_budgetBytes;
}

uint64_t imageformats::estimateTextureBytes(const core::Vector<uint8_t>& bytes, uint32_t maxSize)
{
    const auto encoding = getEncodingFromBuffer(bytes);
    KTX2ImageInfo info;
    if (getKTX2ImageInfo(bytes, info) && info.blockCompressed) {
        uint32_t first = 0;
        while (maxSize && first+1 < info.levelCount && (std::max(1u,info.width>>first) > maxSize || std::max(1u,info.height>>first) > maxSize)) ++first;
        uint64_t total = 0;
        for (uint32_t level = first; level < info.levelCount; ++level) total += getKTX2CompressedLevelSize(info,level);
        return total;
    }
    if (encoding == ImageEncoding::DDS) {
        uint32_t w = readLE32(bytes,16), h = readLE32(bytes,12);
        while (maxSize && (w > maxSize || h > maxSize)) { w = std::max(1u,w/2); h = std::max(1u,h/2); }
        return uint64_t(w)*h*16*4/3;
    }
    int w = 0, h = 0, channels = 0;
    if (bytes.size() <= INT_MAX && stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels)) {
        while (maxSize && (w > static_cast<int>(maxSize) || h > static_cast<int>(maxSize))) {
            w = std::max(1, w/2); h = std::max(1, h/2);
        }
        const uint64_t bpc = stbi_is_16_bit_from_memory(bytes.data(), static_cast<int>(bytes.size())) ? 2 : 1;
        return uint64_t(w)*h*4*bpc*4/3;
    }
    return estimateDecodedBytes(bytes, encoding)*4/3;
}

ImageDecodingResult imageformats::decodeImageFromFile(
    const filesystem::File& _inFile,
    image::Image& _outImage,
    core::IAllocator* _inAllocator,
    const uint32_t _inRequiredChannels)
{
    core::Vector<uint8_t> buffer(_inAllocator);
    if (core::Failed(_inFile.load(buffer))) {
        return ImageDecodingError::InvalidInput;
    }

    return decodeImage(buffer, _outImage, _inRequiredChannels);
}

core::string imageformats::imageEncodingToString(ImageEncoding _imageEncoding)
{
    switch (_imageEncoding) {
    case ImageEncoding::PNG: {
        return "image/png";
    }
    case ImageEncoding::JPEG: {
        return "image/jpeg";
    }
    case ImageEncoding::HDR: {
        return "image/vnd.radiance";
    }
    case ImageEncoding::KTX2: {
        return "image/ktx2";
    }
    case ImageEncoding::WEBP: {
        return "image/webp";
    }
    case ImageEncoding::TGA: {
        return "image/x-tga";
    }
    case ImageEncoding::DDS: {
        return "image/vnd-ms.dds";
    }
    case ImageEncoding::TIFF: {
        return "image/tiff";
    }
    case ImageEncoding::EXR: {
        return "image/x-exr";
    }
    default: {
        return "";
    }
    }
}

ImageEncoding imageformats::imageEncodingFromString(const std::string& _mimeType)
{
    if (_mimeType == "image/tiff") return ImageEncoding::TIFF;
    if (_mimeType == "image/png") {
        return ImageEncoding::PNG;
    }
    if (_mimeType == "image/jpeg") {
        return ImageEncoding::JPEG;
    }
    if (_mimeType == "image/ktx2") {
        return ImageEncoding::KTX2;
    }
    if (_mimeType == "image/vnd.radiance") {
        return ImageEncoding::HDR;
    }
    if (_mimeType == "image/webp") {
        return ImageEncoding::WEBP;
    }
    if (_mimeType == "image/x-tga" || _mimeType == "image/tga") {
        return ImageEncoding::TGA;
    }
    if (_mimeType == "image/vnd-ms.dds" || _mimeType == "image/dds") {
        return ImageEncoding::DDS;
    }
    return ImageEncoding::Undefined;
}

core::string imageformats::getFileExtension(ImageEncoding _imageEncoding)
{
    switch (_imageEncoding) {
    case ImageEncoding::TIFF: return ".tif";
    case ImageEncoding::PNG: {
        return ".png";
    }
    case ImageEncoding::JPEG: {
        return ".jpg";
    }
    case ImageEncoding::HDR: {
        return ".hdr";
    }
    case ImageEncoding::KTX2: {
        return ".ktx2";
    }
    case ImageEncoding::WEBP: {
        return ".webp";
    }
    case ImageEncoding::TGA: {
        return ".tga";
    }
    case ImageEncoding::DDS: {
        return ".dds";
    }
    default: {
        return "";
    }
    }
}

ImageEncoding imageformats::getEncodingFromUri(const core::string& _inUri)
{
    if (core::endsWith(core::toLower(_inUri), ".tif") || core::endsWith(core::toLower(_inUri), ".tiff")) return ImageEncoding::TIFF;
    if (core::endsWith(core::toLower(_inUri), ".png")) {
        return ImageEncoding::PNG;
    }
    if (core::endsWith(core::toLower(_inUri), ".jpg") || core::endsWith(core::toLower(_inUri), ".jpeg")) {
        return ImageEncoding::JPEG;
    }
    if (core::endsWith(core::toLower(_inUri), ".hdr")) {
        return ImageEncoding::HDR;
    }
    if (core::endsWith(core::toLower(_inUri), ".ktx2")) {
        return ImageEncoding::KTX2;
    }
    if (core::endsWith(core::toLower(_inUri), ".webp")) {
        return ImageEncoding::WEBP;
    }
    if (core::endsWith(core::toLower(_inUri), ".tga")) {
        return ImageEncoding::TGA;
    }
    if (core::endsWith(core::toLower(_inUri), ".dds")) {
        return ImageEncoding::DDS;
    }
    if (core::endsWith(core::toLower(_inUri), ".exr")) {
        return ImageEncoding::EXR;
    }

    // default
    return ImageEncoding::Undefined;
}

ImageEncoding imageformats::getEncodingFromBuffer(const core::Vector<uint8_t>& _inBuffer)
{
    std::vector<uint8_t> vec(_inBuffer.data(), _inBuffer.data() + static_cast<std::ptrdiff_t>(_inBuffer.size()));
    return getEncodingFromBuffer(vec);
}

ImageEncoding imageformats::getEncodingFromBuffer(const std::vector<uint8_t>& _inBuffer)
{
    if (_inBuffer.size() >= 4 && ((_inBuffer[0] == 'I' && _inBuffer[1] == 'I' && (_inBuffer[2] == 42 || _inBuffer[2] == 43) && _inBuffer[3] == 0)
        || (_inBuffer[0] == 'M' && _inBuffer[1] == 'M' && _inBuffer[2] == 0 && (_inBuffer[3] == 42 || _inBuffer[3] == 43)))) return ImageEncoding::TIFF;
    if (_inBuffer.size() >= 8) {
        // see http://www.libpng.org/pub/png/spec/1.2/PNG-Rationale.html#R.PNG-file-signature
        if (_inBuffer[0] == 137 && _inBuffer[1] == 80 && _inBuffer[2] == 78 && _inBuffer[3] == 71 && _inBuffer[4] == 13 && _inBuffer[5] == 10 && _inBuffer[6] == 26 && _inBuffer[7] == 10) {
            return ImageEncoding::PNG;
        }
    }
    if (_inBuffer.size() >= 2) {
        // see https://www.garykessler.net/library/file_sigs.html
        if (_inBuffer[0] == 0xFF && _inBuffer[1] == 0xD8) {
            size_t scanIndex = _inBuffer.size() - 1;
            while (scanIndex >= 1) {
                if (_inBuffer[scanIndex - 1] == 0xFF && _inBuffer[scanIndex] == 0xD9) {
                    return ImageEncoding::JPEG;
                }
                scanIndex--;
            }
        }
    }
    if (_inBuffer.size() >= 10) {
        // see https://www.garykessler.net/library/file_sigs.html
        if (_inBuffer[0] == '#' && _inBuffer[1] == '?' && _inBuffer[2] == 'R' && _inBuffer[3] == 'A' && _inBuffer[4] == 'D' && _inBuffer[5] == 'I' && _inBuffer[6] == 'A' && _inBuffer[7] == 'N' && _inBuffer[8] == 'C' && _inBuffer[9] == 'E') {
            return ImageEncoding::HDR;
        }
    }
    if (_inBuffer.size() >= 12) {
        // see http://github.khronos.org/KTX-Specification/
        if (_inBuffer[0] == 0xAB && _inBuffer[1] == 0x4B && _inBuffer[2] == 0x54 && _inBuffer[3] == 0x58 && _inBuffer[4] == 0x20 && _inBuffer[5] == 0x32 && _inBuffer[6] == 0x30 && _inBuffer[7] == 0xBB && _inBuffer[8] == 0x0D && _inBuffer[9] == 0x0A && _inBuffer[10] == 0x1A && _inBuffer[11] == 0x0A) {
            return ImageEncoding::KTX2;
        }
    }
    if (_inBuffer.size() >= 6) {
        // see https://www.garykessler.net/library/file_sigs.html
        if (_inBuffer[0] == 0x42 && _inBuffer[1] == 0x4D) {
            size_t len = (static_cast<size_t>(_inBuffer[5]) << 24)
                | (static_cast<size_t>(_inBuffer[4]) << 16)
                | (static_cast<size_t>(_inBuffer[3]) << 8)
                | static_cast<size_t>(_inBuffer[2]);
            if (_inBuffer.size() == len) {
                return ImageEncoding::BMP;
            }
        }
    }
    if (_inBuffer.size() >= 18) {
        // see https://www.garykessler.net/library/file_sigs.html
        size_t len = _inBuffer.size();
        if (_inBuffer[len - 18] == 0x54 && _inBuffer[len - 17] == 0x52 && _inBuffer[len - 16] == 0x55 && _inBuffer[len - 15] == 0x45 && _inBuffer[len - 14] == 0x56 && _inBuffer[len - 13] == 0x49 && _inBuffer[len - 12] == 0x53 && _inBuffer[len - 11] == 0x49 && _inBuffer[len - 10] == 0x4F && _inBuffer[len - 9] == 0x4E && _inBuffer[len - 8] == 0x2D && _inBuffer[len - 7] == 0x58 && _inBuffer[len - 6] == 0x46 && _inBuffer[len - 5] == 0x49 && _inBuffer[len - 4] == 0x4C && _inBuffer[len - 3] == 0x45 && _inBuffer[len - 2] == 0x2E && _inBuffer[len - 1] == 0x00) {
            return ImageEncoding::TGA;
        }
    }

    // detect webp
    if (_inBuffer.size() >= 12) {
        // see https://developers.google.com/speed/webp/docs/riff_container
        // see https://www.garykessler.net/library/file_sigs.html
        if (_inBuffer[0] == 0x52 && _inBuffer[1] == 0x49 && _inBuffer[2] == 0x46 && _inBuffer[3] == 0x46 && _inBuffer[8] == 0x57 && _inBuffer[9] == 0x45 && _inBuffer[10] == 0x42 && _inBuffer[11] == 0x50) {
            return ImageEncoding::WEBP;
        }
    }

    // detect dds (magic 'D' 'D' 'S' ' ')
    if (_inBuffer.size() >= 4) {
        if (_inBuffer[0] == 'D' && _inBuffer[1] == 'D' && _inBuffer[2] == 'S' && _inBuffer[3] == ' ') {
            return ImageEncoding::DDS;
        }
    }

    // detect openexr (magic 0x76, 0x2f, 0x31, 0x01)
    if (_inBuffer.size() >= 4) {
        if (_inBuffer[0] == 0x76 && _inBuffer[1] == 0x2f && _inBuffer[2] == 0x31 && _inBuffer[3] == 0x01) {
            return ImageEncoding::EXR;
        }
    }

    // default
    return ImageEncoding::Undefined;
}

static ImageDecodingResult decodeEXR(
    const core::Vector<uint8_t>& _inBuffer,
    image::Image& _outImage,
    const uint32_t _inRequiredChannels)
{
    if (_inBuffer.empty()) {
        return ImageDecodingError::InvalidInput;
    }

    float* rgba = nullptr;
    int width = 0;
    int height = 0;
    const char* err = nullptr;

    int ret = LoadEXRFromMemory(
        &rgba,
        &width,
        &height,
        _inBuffer.data(),
        _inBuffer.size(),
        &err);

    if (ret != TINYEXR_SUCCESS || rgba == nullptr) {
        if (err) {
            core::logError("TinyEXR failed: %s", err);
            FreeEXRErrorMessage(err);
        }
        return ImageDecodingError::LoadFailure;
    }

    uint32_t bitsPerChannel[4] = { 32, 32, 32, 32 };
    image::Format format = image::Format::FLOAT;
    bool signedType = true;
    bool srgb = false;
    bool packed = false;
    bool compressed = false;

    _outImage.initializeLayout(
        bitsPerChannel[0],
        bitsPerChannel[1],
        bitsPerChannel[2],
        bitsPerChannel[3],
        format,
        signedType,
        srgb,
        packed,
        compressed,
        0, 1, 2, 3);

    size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    size_t byteLength = pixelCount * 4 * sizeof(float);

    bool success = _outImage.initialize(
        reinterpret_cast<const uint8_t*>(rgba),
        byteLength,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        1, 1, 1, 1);

    free(rgba);

    if (!success) {
        return ImageDecodingError::InitializeFailure;
    }

    return ImageDecodingResult::Success;
}

ImageDecodingResult imageformats::decodeImage(
    const core::Vector<uint8_t>& _inBuffer,
    image::Image& _outImage,
    const uint32_t _inRequiredChannels, std::string* _failureReason)
{
    // Checking for valid buffer. stbi memory buffer size is a native int, so avoiding overflow.
    if (_inBuffer.empty() || _inBuffer.size() > INT_MAX) {
        return ImageDecodingError::InvalidInput;
    }

    size_t width = 0;
    size_t height = 0;
    size_t depth = 1;
    uint32_t faceCount = 1;
    uint32_t arrayCount = 1;
    uint32_t levelCount = 1;

    uint32_t channelCount = 0;
    uint32_t bitsPerChannel[4] = { 0, 0, 0, 0 };
    image::Format format = image::Format::UNDEFINED;
    bool signedType = false;
    bool srgb = false;
    bool packed = false;
    bool compressed = false;
    uint32_t redIndex = 0;
    uint32_t greenIndex = 1;
    uint32_t blueIndex = 2;
    uint32_t alphaIndex = 3;

    uint8_t* imageData = nullptr;
    uint8_t* imageDataStb = nullptr;
    const ImageEncoding encoding = getEncodingFromBuffer(_inBuffer);
    // A decode holds the decoder's output and the image's copy of it at the same time.
    const DecodeLease lease(estimateDecodedBytes(_inBuffer, encoding) * 2u);
    if (encoding == ImageEncoding::TIFF) {
        tiff::Pixels pixels;
        std::string reason;
        if (!tiff::decode(std::span<const uint8_t>(_inBuffer.data(), _inBuffer.size()), pixels, reason)) {
            if (_failureReason) *_failureReason = reason;
            else core::logWarning("TIFF: %s; using error image", reason.c_str());
            return ImageDecodingError::LoadFailure;
        }
        if (!_outImage.initializeLayout(pixels.width, pixels.height, 1, 1, 1, 1, gpu::Format::R8G8B8A8_UNORM)
            || !_outImage.initialize(std::move(pixels.rgba))) return ImageDecodingError::InitializeFailure;
        return ImageDecodingResult::Success;
    } else if (encoding == ImageEncoding::KTX2) {
        return decodeKTX(_inBuffer, _outImage);
    } else if (encoding == ImageEncoding::WEBP) {
        return decodeWebP(_inBuffer, _outImage);
    } else if (encoding == ImageEncoding::DDS) {
        return decodeDDS(_inBuffer, _outImage);
    } else if (encoding == ImageEncoding::EXR) {
        return decodeEXR(_inBuffer, _outImage, _inRequiredChannels);
    } else {
        int x = 0;
        int y = 0;
        int channelsInFile = 0;
        int desiredChannels = static_cast<int>(_inRequiredChannels);

        if (encoding == ImageEncoding::HDR) {
            imageDataStb = reinterpret_cast<uint8_t*>(
                stbi_loadf_from_memory(
                    static_cast<stbi_uc const*>(&_inBuffer[0]),
                    static_cast<int>(_inBuffer.size()), &x, &y, &channelsInFile, desiredChannels));

            width = static_cast<size_t>(x);
            height = static_cast<size_t>(y);

            channelCount = static_cast<uint32_t>(channelsInFile);

            bitsPerChannel[0] = 32;
            bitsPerChannel[1] = 32;
            bitsPerChannel[2] = 32;
            bitsPerChannel[3] = (channelCount >= 4) ? 32 : 0;

            format = image::Format::FLOAT;
            signedType = true;
        } else if (encoding == ImageEncoding::PNG || encoding == ImageEncoding::JPEG || encoding == ImageEncoding::BMP || encoding == ImageEncoding::TGA || encoding == ImageEncoding::Undefined) {
            int is16Bit = stbi_is_16_bit_from_memory(
                static_cast<stbi_uc const*>(&_inBuffer[0]),
                static_cast<int>(_inBuffer.size()));

            if (is16Bit) {
                stbi_us* data16 = stbi_load_16_from_memory(
                    static_cast<stbi_uc const*>(&_inBuffer[0]),
                    static_cast<int>(_inBuffer.size()), &x, &y, &channelsInFile, desiredChannels);
                imageDataStb = reinterpret_cast<uint8_t*>(data16);

                channelCount = static_cast<uint32_t>(channelsInFile);

                bitsPerChannel[0] = 16;
                bitsPerChannel[1] = (channelCount >= 2) ? 16 : 0;
                bitsPerChannel[2] = (channelCount >= 3) ? 16 : 0;
                bitsPerChannel[3] = (channelCount >= 4) ? 16 : 0;
            } else {
                imageDataStb = static_cast<uint8_t*>(
                    stbi_load_from_memory(
                        static_cast<stbi_uc const*>(&_inBuffer[0]),
                        static_cast<int>(_inBuffer.size()), &x, &y, &channelsInFile, desiredChannels));

                channelCount = static_cast<uint32_t>(channelsInFile);

                bitsPerChannel[0] = 8;
                bitsPerChannel[1] = (channelCount >= 2) ? 8 : 0;
                bitsPerChannel[2] = (channelCount >= 3) ? 8 : 0;
                bitsPerChannel[3] = (channelCount >= 4) ? 8 : 0;
            }

            width = static_cast<size_t>(x);
            height = static_cast<size_t>(y);

            format = image::Format::NORM;
            signedType = false;

            if (encoding == ImageEncoding::JPEG) {
                _outImage.m_metaData = image::JPEGMetaData();
            }
        } else {
            return ImageDecodingError::InvalidInput;
        }
    }

    if (imageData == nullptr && imageDataStb == nullptr) {
        return ImageDecodingError::LoadFailure;
    }

    size_t pixelSize = static_cast<size_t>((bitsPerChannel[0] + bitsPerChannel[1] + bitsPerChannel[2] + bitsPerChannel[3]) / 8);

    // Reduce before the Image-owned copy is made. The lease still accounts for the full
    // decoder allocation, so several 8K decodes cannot exhaust RAM at the same time.
    std::unique_ptr<uint8_t, decltype(&stbi_image_free)> decoded(imageDataStb, stbi_image_free);
    std::vector<uint8_t> resized;
    const uint32_t limit = core::loading::state.memoryAware ? core::loading::state.textureLimit.load() : 0;
    if (limit && imageDataStb && encoding != ImageEncoding::HDR && (width > limit || height > limit)) {
        const size_t originalWidth = width, originalHeight = height;
        while (width > limit || height > limit) { width = std::max<size_t>(1, width/2); height = std::max<size_t>(1, height/2); }
        resized.resize(width*height*pixelSize);
        const int channels = static_cast<int>(pixelSize / (bitsPerChannel[0]/8));
        int ok = 0;
        if (bitsPerChannel[0] == 8) {
            ok = stbir_resize_uint8(imageDataStb, static_cast<int>(originalWidth), static_cast<int>(originalHeight), 0,
                resized.data(), static_cast<int>(width), static_cast<int>(height), 0, channels);
        } else if (bitsPerChannel[0] == 16) {
            ok = stbir_resize_uint16_generic(reinterpret_cast<uint16_t*>(imageDataStb), static_cast<int>(originalWidth), static_cast<int>(originalHeight), 0,
                reinterpret_cast<uint16_t*>(resized.data()), static_cast<int>(width), static_cast<int>(height), 0,
                channels, -1, 0, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
        }
        if (!ok) return ImageDecodingError::InitializeFailure;
        imageDataStb = resized.data();
        ++core::loading::state.reductions;
        core::log(core::loading::state.requestedTextureLimit == limit ? core::LogLevel::Info : core::LogLevel::Warning,
            "[Loading] texture reduced %zux%zu -> %zux%zu (preset/memory maximum %u)", originalWidth, originalHeight, width, height, limit);
    }

    size_t imageLength = 0;
    for (uint32_t level = 0; level < levelCount; level++) {
        imageLength += static_cast<size_t>(std::max<uint64_t>(width >> level, 1ull)) * static_cast<size_t>(std::max<uint64_t>(height >> level, 1ull)) * static_cast<size_t>(std::max<uint64_t>(depth >> level, 1ull)) * pixelSize * static_cast<size_t>(faceCount) * static_cast<size_t>(arrayCount);
    }

    bool success = _outImage.initializeLayout(
        bitsPerChannel[0],
        bitsPerChannel[1],
        bitsPerChannel[2],
        bitsPerChannel[3],
        format,
        signedType,
        srgb,
        packed,
        compressed,
        redIndex,
        greenIndex,
        blueIndex,
        alphaIndex);

    //

    if (imageData != nullptr) {
        success = success && _outImage.initialize(imageData, imageLength, static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(depth), faceCount, arrayCount, levelCount);
        delete[] imageData;
    } else if (imageDataStb != nullptr) {
        success = success && _outImage.initialize(imageDataStb, imageLength, static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(depth), faceCount, arrayCount, levelCount);
    }

    if (!success) {
        return ImageDecodingError::InitializeFailure;
    }

    return ImageDecodingResult::Success;
}

//
namespace ux3d::imageformats {
static void writeFunction(void* context, void* data, int size)
{
    if (!context || !data || !size) {
        return;
    }

    auto* outBuffer = static_cast<core::Vector<uint8_t>*>(context);
    size_t currentSize = outBuffer->size();

    if (outBuffer->resize(currentSize + static_cast<size_t>(size), nullptr)) {
        memcpy(&(*outBuffer)[currentSize], data, static_cast<size_t>(size));
    }
}
}

ImageEncodingResult imageformats::encodePNG(const image::Image& _image, core::Vector<uint8_t>& _buffer)
{
    //  PNG allows you to set the deflate compression level by setting the global
    //	variable 'stbi_write_png_compression_level' (it defaults to 8).

    if (_image.getChannelSize(image::Channel::RED) != 8) {
        core::debugLog(core::LogLevel::Error, "Encoding to PNG requires the channel size to be 8 bit");
        return ImageEncodingError::InvalidInput;
    }

    int strideInBytes = static_cast<int>(_image.getWidth() * _image.getPixelSize());

    if (false == stbi_write_png_to_func(writeFunction, static_cast<void*>(&_buffer), static_cast<int>(_image.getWidth()), static_cast<int>(_image.getHeight()), static_cast<int>(_image.getChannelCount()), static_cast<const void*>(_image.getPixels().data()), strideInBytes)) {
        core::debugLog(core::LogLevel::Error, "Could not encode PNG");
        return ImageEncodingError::WriteFailure;
    }
    return ImageEncodingResult::Success;
}

ImageEncodingResult imageformats::encodeJPEG(const image::Image& _image, const uint32_t _quality, core::Vector<uint8_t>& _buffer)
{
    int quality = static_cast<int>(_quality);

    if (false == stbi_write_jpg_to_func(writeFunction, static_cast<void*>(&_buffer), static_cast<int>(_image.getWidth()), static_cast<int>(_image.getHeight()), static_cast<int>(_image.getChannelCount()), static_cast<const void*>(_image.getPixels().data()), quality)) {
        core::debugLog(core::LogLevel::Error, "Could not encode JPEG");
        return ImageEncodingError::WriteFailure;
    }

    return ImageEncodingResult::Success;
}

ImageEncodingResult imageformats::encodeHDR(const image::Image& _image, core::Vector<uint8_t>& _buffer)
{
    if (_image.getFormat() != image::Format::FLOAT || false == _image.isSigned()) {
        core::debugLog(core::LogLevel::Error, "Encoding to HDR requires the format to be signed FLOAT");
        return ImageEncodingError::InvalidInput;
    }

    if (_image.getChannelSize(image::Channel::RED) != 32) {
        core::debugLog(core::LogLevel::Error, "Encoding to HDR requires the channel size to be 32 bits");
        return ImageEncodingError::InvalidInput;
    }

    if (false == stbi_write_hdr_to_func(writeFunction, static_cast<void*>(&_buffer), static_cast<int>(_image.getWidth()), static_cast<int>(_image.getHeight()), static_cast<int>(_image.getChannelCount()), reinterpret_cast<const float*>(_image.getPixels().data()))) {
        core::debugLog(core::LogLevel::Error, "Could not encode HDR");
        return ImageEncodingError::WriteFailure;
    }

    return ImageEncodingResult::Success;
}

// encodeImage is a function to encode raw image data to an image container file
ImageEncodingResult imageformats::encodeImage(
    const image::Image& _inImage,
    const ImageEncoding _inImageEncoding,
    core::Vector<uint8_t>& _outBuffer,
    const image::ImageMetaData& _metaData)
{
    if (_inImage.getPixels().empty()) {
        core::debugLog(core::LogLevel::Error, "Cannot encode an empty image");
        return ImageEncodingError::InvalidInput;
    }

    if (_inImage.isCompressed() || _inImage.isPacked()) {
        core::debugLog(core::LogLevel::Error, "Cannot encode a compressed or packed image");
        return ImageEncodingError::InvalidInput;
    }

    // reserve enough data to avoid expensive copying
    _outBuffer.reserve(_inImage.getPixels().size() * 4);

    if (_inImageEncoding == ImageEncoding::PNG) {
        return encodePNG(_inImage, _outBuffer);
    }

    if (_inImageEncoding == ImageEncoding::KTX2) {
        if (std::holds_alternative<image::KTXMetaData>(_metaData)) {
            return encodeKTX(_inImage, _outBuffer, std::get<image::KTXMetaData>(_metaData));
        } else {
            core::debugLog(core::LogLevel::Error, "Missing KTX2 MetaData");
        }
    }

    if (_inImageEncoding == ImageEncoding::JPEG) {
        if (std::holds_alternative<image::JPEGMetaData>(_metaData)) {
            return encodeJPEG(_inImage, std::get<image::JPEGMetaData>(_metaData).compressionRate, _outBuffer);
        } else {
            core::debugLog(core::LogLevel::Error, "Missing JPEG MetaData");
        }
    }

    if (_inImageEncoding == ImageEncoding::HDR) {
        return encodeHDR(_inImage, _outBuffer);
    }

    core::debugLog(core::LogLevel::Error, "Cannot encode image");
    return ImageEncodingError::InvalidInput;
}

ux3d::imageformats::ImageEncodingResult ux3d::imageformats::scaleImage(
    const image::Image& _inImage,
    core::Vector<uint8_t>& _outBuffer,
    const uint32_t _width, const uint32_t _height)
{
    _outBuffer.reset(); // no need to call destructor on uint8_t, reset is faster

    if (_inImage.getPixels().empty()) {
        return ImageEncodingError::InvalidInput;
    }
    if (!_inImage.isCompressed()) {
        if (!_inImage.isPacked()) {
            if (!_outBuffer.resize(static_cast<size_t>(_width)*_height*_inImage.getPixelSize(),nullptr)) return ImageEncodingError::WriteFailure;
            int inStrideInBytes = static_cast<int>(_inImage.getWidth() * _inImage.getPixelSize());
            int outStrideInBytes = static_cast<int>(_width * _inImage.getPixelSize());
            int success = 0;
            const auto channels = static_cast<int>(_inImage.getChannelCount());
            const auto bits = _inImage.getChannelSize(image::Channel::RED);
            if (bits == 8) success = stbir_resize_uint8(
                    _inImage.getPixels().data(),
                    static_cast<int>(_inImage.getWidth()),
                    static_cast<int>(_inImage.getHeight()),
                    inStrideInBytes,
                    _outBuffer.data(),
                    static_cast<int>(_width),
                    static_cast<int>(_height),
                    outStrideInBytes,
                    channels);
            else if (bits == 16 && _inImage.getFormat() != image::Format::FLOAT)
                success = stbir_resize_uint16_generic(reinterpret_cast<const uint16_t*>(_inImage.getPixels().data()),_inImage.getWidth(),_inImage.getHeight(),inStrideInBytes,
                    reinterpret_cast<uint16_t*>(_outBuffer.data()),_width,_height,outStrideInBytes,channels,-1,0,STBIR_EDGE_CLAMP,STBIR_FILTER_DEFAULT,STBIR_COLORSPACE_LINEAR,nullptr);
            else if (bits == 32 && _inImage.getFormat() == image::Format::FLOAT)
                success = stbir_resize_float(reinterpret_cast<const float*>(_inImage.getPixels().data()),_inImage.getWidth(),_inImage.getHeight(),inStrideInBytes,
                    reinterpret_cast<float*>(_outBuffer.data()),_width,_height,outStrideInBytes,channels);
            if (success) {
                return ImageEncodingResult::Success;
            }
        }
    }

    return ImageEncodingError::WriteFailure;
}

ux3d::gpu::Format ux3d::imageformats::convertToVKFormat(const image::Image& _inImage)
{
    return convertToVKFormat(_inImage, _inImage.isSrgb());
}

// only 8 bit/channel can be resolved
ux3d::gpu::Format ux3d::imageformats::convertToVKFormat(const image::Image& _inImage, bool _isSRGB)
{
    std::string layout = _inImage.getMemoryLayoutString();

    if (layout == "R8") {
        return _isSRGB ? ux3d::gpu::Format::R8_SRGB : ux3d::gpu::Format::R8_UNORM;
    }
    if (layout == "R8G8") {
        return _isSRGB ? ux3d::gpu::Format::R8G8_SRGB : ux3d::gpu::Format::R8G8_UNORM;
    }
    if (layout == "R8G8B8") {
        return _isSRGB ? ux3d::gpu::Format::R8G8B8_SRGB : ux3d::gpu::Format::R8G8B8_UNORM;
    }
    if (layout == "R8G8B8A8") {
        return _isSRGB ? ux3d::gpu::Format::R8G8B8A8_SRGB : ux3d::gpu::Format::R8G8B8A8_UNORM;
    }

    // TODO: Add more permutations, add hdr, ... to match more formats

    // No valid format found.
    return gpu::Format::UNDEFINED;
}
