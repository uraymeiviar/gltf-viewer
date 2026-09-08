// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "image/Image.h"
#include "core/Logger.h"
#include "image/ImageFormat.h"
#include <cstring>
#include <math.h>

// Exact SRGB to linear and vice versa
// https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_sRGB.txt
// http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html

using namespace ux3d;
using namespace ux3d::image;

Image::Image()
    : m_Pixels()
{
}

Image::Image(const Image& _Other)
    : m_metaData(_Other.m_metaData)
    , m_Format(_Other.m_Format)
    , m_Compressed(_Other.m_Compressed)
    , m_Packed(_Other.m_Packed)
    , m_Signed(_Other.m_Signed)
    , m_Srgb(_Other.m_Srgb)
    , m_ChannelSize { _Other.m_ChannelSize[0], _Other.m_ChannelSize[1], _Other.m_ChannelSize[2], _Other.m_ChannelSize[3] }
    , m_ChannelIndex { _Other.m_ChannelIndex[0], _Other.m_ChannelIndex[1], _Other.m_ChannelIndex[2], _Other.m_ChannelIndex[3] }
    , m_Width(_Other.m_Width)
    , m_Height(_Other.m_Height)
    , m_Depth(_Other.m_Depth)
    , m_FaceCount(_Other.m_FaceCount)
    , m_ArrayCount(_Other.m_ArrayCount)
    , m_LevelCount(_Other.m_LevelCount)
    , m_Pixels(_Other.m_Pixels)
{
}

Image::Image(Image&& _Other)
    : m_metaData(_Other.m_metaData)
    , m_Format(_Other.m_Format)
    , m_Compressed(_Other.m_Compressed)
    , m_Packed(_Other.m_Packed)
    , m_Signed(_Other.m_Signed)
    , m_Srgb(_Other.m_Srgb)
    , m_ChannelSize { _Other.m_ChannelSize[0], _Other.m_ChannelSize[1], _Other.m_ChannelSize[2], _Other.m_ChannelSize[3] }
    , m_ChannelIndex { _Other.m_ChannelIndex[0], _Other.m_ChannelIndex[1], _Other.m_ChannelIndex[2], _Other.m_ChannelIndex[3] }
    , m_Width(_Other.m_Width)
    , m_Height(_Other.m_Height)
    , m_Depth(_Other.m_Depth)
    , m_FaceCount(_Other.m_FaceCount)
    , m_ArrayCount(_Other.m_ArrayCount)
    , m_LevelCount(_Other.m_LevelCount)
    , m_Pixels(std::move(_Other.m_Pixels))
{
    _Other.m_Width = 0u;
    _Other.m_Height = 0u;
    _Other.m_Depth = 0u;
    _Other.m_FaceCount = 0u;
    _Other.m_ArrayCount = 0u;
    _Other.m_LevelCount = 0u;
    _Other.m_Format = Format::UNDEFINED;
    _Other.m_Srgb = false;
    _Other.m_Signed = false;
    _Other.m_Packed = false;
    _Other.m_Compressed = false;
    _Other.m_ChannelSize[0] = 0;
    _Other.m_ChannelSize[1] = 0;
    _Other.m_ChannelSize[2] = 0;
    _Other.m_ChannelSize[3] = 0;
    _Other.m_ChannelIndex[0] = 0;
    _Other.m_ChannelIndex[1] = 1;
    _Other.m_ChannelIndex[2] = 2;
    _Other.m_ChannelIndex[3] = 3;
}

Image& Image::operator=(const Image& other)
{
    *this = Image(other);
    return *this;
}

Image& Image::operator=(Image&& _Other)
{
    m_Pixels = std::move(_Other.m_Pixels);
    m_Width = _Other.m_Width;
    m_Height = _Other.m_Height;
    m_Depth = _Other.m_Depth;
    m_FaceCount = _Other.m_FaceCount;
    m_ArrayCount = _Other.m_ArrayCount;
    m_LevelCount = _Other.m_LevelCount;
    m_Format = _Other.m_Format;
    m_Srgb = _Other.m_Srgb;
    m_Signed = _Other.m_Signed;
    m_Packed = _Other.m_Packed;
    m_Compressed = _Other.m_Compressed;
    m_ChannelSize[0] = _Other.m_ChannelSize[0];
    m_ChannelSize[1] = _Other.m_ChannelSize[1];
    m_ChannelSize[2] = _Other.m_ChannelSize[2];
    m_ChannelSize[3] = _Other.m_ChannelSize[3];
    m_ChannelIndex[0] = _Other.m_ChannelIndex[0];
    m_ChannelIndex[1] = _Other.m_ChannelIndex[1];
    m_ChannelIndex[2] = _Other.m_ChannelIndex[2];
    m_ChannelIndex[3] = _Other.m_ChannelIndex[3];
    m_metaData = _Other.m_metaData;

    return *this;
}

bool Image::initializeLayout(
    const uint32_t _InRedSize,
    const uint32_t _InGreenSize,
    const uint32_t _InBlueSize,
    const uint32_t _InAlphaSize,
    const Format _InFormat,
    const bool _InSigned,
    const bool _InSrgb,
    const bool _InPacked,
    const bool _InCompressed,
    const uint32_t _InRedIndex,
    const uint32_t _InGreenIndex,
    const uint32_t _InBlueIndex,
    const uint32_t _InAlphaIndex)
{
    // This warning might be usefully, but it will break undo/redo. Please make sure undo/redo is fixed before this warning is enabled again!
    /*if (m_Format != Format::UNDEFINED)
	{
		core::debugLog(core::LogLevel::Warning, "Image layout initialized twice"); 
		return false;
	}*/

    if (_InRedSize == 0 || _InFormat == Format::UNDEFINED || _InRedIndex > 3 || _InGreenIndex > 3 || _InBlueIndex > 3 || _InAlphaIndex > 3) {
        core::logError("Image layout invalid");
        return false;
    }

    if (_InRedIndex == _InGreenIndex || _InRedIndex == _InBlueIndex || _InRedIndex == _InAlphaIndex || _InGreenIndex == _InBlueIndex || _InGreenIndex == _InAlphaIndex || _InBlueIndex == _InAlphaIndex) {
        core::logError("Image layout invalid");
        return false;
    }

    m_ChannelSize[0] = _InRedSize;
    m_ChannelSize[1] = _InGreenSize;
    m_ChannelSize[2] = _InBlueSize;
    m_ChannelSize[3] = _InAlphaSize;
    m_Format = _InFormat;
    m_Signed = _InSigned;
    m_Srgb = _InSrgb;
    m_Packed = _InPacked;
    m_Compressed = _InCompressed;
    m_ChannelIndex[0] = _InRedIndex;
    m_ChannelIndex[1] = _InGreenIndex;
    m_ChannelIndex[2] = _InBlueIndex;
    m_ChannelIndex[3] = _InAlphaIndex;

    return true;
}

bool Image::initializeLayout(
    uint32_t _InWidth,
    uint32_t _InHeight,
    uint32_t _InDepth,
    uint32_t _InFaceCount,
    uint32_t _InArrayCount,
    uint32_t _InLevelCount,
    gpu::Format vkFormat)
{
    m_Width = _InWidth;
    m_Height = _InHeight;
    m_Depth = _InDepth;
    m_FaceCount = _InFaceCount;
    m_ArrayCount = _InArrayCount;
    m_LevelCount = _InLevelCount;

    uint32_t channelCount = image::getChannelCount(vkFormat);

    int32_t indexRed = image::getChannelIndex(vkFormat, Channel::RED);
    int32_t indexGreen = image::getChannelIndex(vkFormat, Channel::GREEN);
    int32_t indexBlue = image::getChannelIndex(vkFormat, Channel::BLUE);
    int32_t indexAlpha = image::getChannelIndex(vkFormat, Channel::ALPHA);

    m_ChannelIndex[0] = indexRed >= 0 ? static_cast<uint32_t>(indexRed) : 0u;
    m_ChannelIndex[1] = indexGreen >= 0 ? static_cast<uint32_t>(indexGreen) : 1u;
    m_ChannelIndex[2] = indexBlue >= 0 ? static_cast<uint32_t>(indexBlue) : 2u;
    m_ChannelIndex[3] = indexAlpha >= 0 ? static_cast<uint32_t>(indexAlpha) : 3u;

    m_ChannelSize[0] = image::getChannelSize(vkFormat, m_ChannelIndex[0]);
    m_ChannelSize[1] = (channelCount >= 2) ? image::getChannelSize(vkFormat, m_ChannelIndex[1]) : 0;
    m_ChannelSize[2] = (channelCount >= 3) ? image::getChannelSize(vkFormat, m_ChannelIndex[2]) : 0;
    m_ChannelSize[3] = (channelCount >= 4) ? image::getChannelSize(vkFormat, m_ChannelIndex[3]) : 0;

    m_Signed = image::isSigned(vkFormat);
    m_Srgb = image::isSrgb(vkFormat);
    m_Packed = image::isPacked(vkFormat);
    m_Compressed = image::isCompressed(vkFormat);

    if (image::isFloat(vkFormat)) {
        m_Format = image::Format::FLOAT;
    } else {
        if (image::isNormalized(vkFormat)) {
            m_Format = image::Format::NORM;
        } else {
            m_Format = image::Format::INT;
        }
    }

    return true;
}

bool Image::initialize(
    const uint8_t* _pData,
    size_t _ByteSize,
    uint32_t _InWidth,
    uint32_t _InHeight,
    uint32_t _InDepth,
    uint32_t _InFaceCount,
    uint32_t _InArrayCount,
    uint32_t _InLevelCount)
{
    if (_pData == nullptr || _ByteSize == 0) {
        core::logError("Tried to initialize image with invalid source data");
        return false;
    }

    if (m_Format == Format::UNDEFINED || _InWidth == 0 || _InHeight == 0 || _InDepth == 0 || _InFaceCount == 0 || _InArrayCount == 0 || _InLevelCount == 0) {
        core::logError("Tried to initialize image with invalid source data layout");
        return false;
    }

    m_Width = _InWidth;
    m_Height = _InHeight;
    m_Depth = _InDepth;
    m_FaceCount = _InFaceCount;
    m_ArrayCount = _InArrayCount;
    m_LevelCount = _InLevelCount;

    size_t totalSize = getOffset(0, 0, 0, 0, 0, _InLevelCount);

    if (_ByteSize != totalSize) {
        core::logError("input byte size does not match layout");
        return false;
    }

    core::MemoryIterator<const uint8_t> begin(_pData);
    core::MemoryIterator<const uint8_t> end(_pData + _ByteSize);

    m_Pixels.assign(begin, end);
    return true;
}

bool Image::initialize(std::vector<uint8_t>&& _InPixels)
{
    if (m_Format == Format::UNDEFINED) {
        core::logError("tried to set pixel data to uninitialized image");
        return false;
    }

    size_t totalSize = getOffset(0, 0, 0, 0, 0, m_LevelCount);

    if (_InPixels.size() != totalSize) {
        core::logError("input byte size does not match layout");
        return false;
    }

    m_Pixels = std::move(_InPixels);

    return true;
}

std::string Image::getMemoryLayoutString() const
{
    //E.g.: "R8G8B8A8"
    std::string layoutString;
    for (size_t i = 0; i < getChannelCount(); ++i) {
        size_t ic = m_ChannelIndex[i];
        char letters[] = { 'R', 'G', 'B', 'A' };
        layoutString += letters[ic] + std::to_string(m_ChannelSize[ic]);
    }
    return layoutString;
}

bool Image::getPixelColor(math::vec4& _OutColor, const uint32_t _InX, const uint32_t _InY, const uint32_t _InZ, const uint32_t _InFace, const uint32_t _InArray, const uint32_t _InLevel) const
{
    if (m_Format == Format::UNDEFINED || _InX >= m_Width || _InY >= m_Height || _InZ >= m_Depth || _InFace >= m_FaceCount || _InArray >= m_ArrayCount || _InLevel >= m_LevelCount) {
        core::logError("tried to get pixel color from uninitialized image");
        return false;
    }

    _OutColor = math::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    size_t totalOffset = getOffset(_InX, _InY, _InZ, _InFace, _InArray, _InLevel);

    for (uint32_t channelIndex = 0; channelIndex < getChannelCount(); channelIndex++) {
        if (!m_Compressed) {
            if (!m_Packed) {
                uint32_t channelOffset = 0;
                const uint8_t* pixels = &m_Pixels[totalOffset + channelOffset];

                auto sn8_Pixel = static_cast<int8_t>(*pixels);
                auto un8_Pixel = static_cast<uint8_t>(*pixels);
                auto sf32_Pixel = static_cast<float>(*pixels);

                if (m_Format == Format::NORM) {
                    if (m_Signed) {
                        _OutColor[getChannelIndex((Channel)channelIndex)] = (static_cast<float>(sn8_Pixel + 128.0) / 255.0f) - 1.0f;
                    } else {
                        _OutColor[getChannelIndex((Channel)channelIndex)] = static_cast<float>(un8_Pixel) / 255.0f;
                    }
                } else if (m_Format == Format::SCALED) {
                    return false;
                } else if (m_Format == Format::INT) {
                    return false;
                } else if (m_Format == Format::FLOAT) {
                    if (m_Signed) {
                        if (m_ChannelSize[channelIndex] == 32) {
                            _OutColor[getChannelIndex((Channel)channelIndex)] = sf32_Pixel;
                        }
                    } else {
                        return false;
                    }
                }

                channelOffset += m_ChannelSize[channelIndex] / 8;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}

bool Image::setPixelColor(const math::vec4& _InColor, const uint32_t _InX, const uint32_t _InY, const uint32_t _InZ, const uint32_t _InFace, const uint32_t _InArray, const uint32_t _InLevel)
{
    if (m_Format == Format::UNDEFINED || _InX >= m_Width || _InY >= m_Height || _InZ >= m_Depth || _InFace >= m_FaceCount || _InArray >= m_ArrayCount || _InLevel >= m_LevelCount) {
        return false;
    }

    size_t totalOffset = getOffset(_InX, _InY, _InZ, _InFace, _InArray, _InLevel);

    for (uint32_t channelIndex = 0; channelIndex < getChannelCount(); channelIndex++) {
        if (!m_Compressed) {
            if (!m_Packed) {
                uint32_t channelOffset = 0;
                uint8_t* pixels = &m_Pixels[totalOffset + channelOffset];

                auto sn8_Pixel = static_cast<int8_t>((_InColor[getChannelIndex((Channel)channelIndex)] + 1.0f) * 255.0f - 128.0f);
                auto un8_Pixel = static_cast<uint8_t>(_InColor[getChannelIndex((Channel)channelIndex)] * 255.0f);
                auto sf32_Pixel = static_cast<float>(_InColor[getChannelIndex((Channel)channelIndex)]);

                if (m_Format == Format::NORM) {
                    if (m_Signed) {
                        memcpy(pixels, &sn8_Pixel, sizeof(int8_t));
                    } else {
                        memcpy(pixels, &un8_Pixel, sizeof(uint8_t));
                    }
                } else if (m_Format == Format::SCALED) {
                    return false;
                } else if (m_Format == Format::INT) {
                    return false;
                } else if (m_Format == Format::FLOAT) {
                    if (m_Signed) {
                        memcpy(pixels, &sf32_Pixel, sizeof(float));
                    } else {
                        return false;
                    }
                }

                channelOffset += m_ChannelSize[channelIndex] / 8;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}

size_t Image::getOffset(const uint32_t _InX, const uint32_t _InY, const uint32_t _InZ, const uint32_t _InFace, const uint32_t _InArray, const uint32_t _InLevel) const
{
    const size_t width = std::max(m_Width >> _InLevel, 1u);
    const size_t height = std::max(m_Height >> _InLevel, 1u);
    const size_t depth = std::max(m_Depth >> _InLevel, 1u);

    const uint32_t pixelSize = getPixelSize();

    size_t totalOffset = 0;
    for (uint32_t level = 0; level < _InLevel; level++) {
        totalOffset += static_cast<size_t>(std::max(m_Width >> level, 1u)) * static_cast<size_t>(std::max(m_Height >> level, 1u)) * static_cast<size_t>(std::max(m_Depth >> level, 1u)) * static_cast<size_t>(pixelSize) * static_cast<size_t>(m_FaceCount) * static_cast<size_t>(m_ArrayCount);
    }

    // face within the current mip level array
    totalOffset += _InX * pixelSize + _InY * width * pixelSize + _InZ * width * height * pixelSize + _InFace * width * height * depth * pixelSize + _InArray * width * height * depth * m_FaceCount * pixelSize;
    return totalOffset;
}

uint32_t Image::getPixelSize() const
{
    return (m_ChannelSize[0] + m_ChannelSize[1] + m_ChannelSize[2] + m_ChannelSize[3]) / 8;
}

uint32_t Image::getChannelCount() const
{
    uint32_t channelCount = 0;

    for (uint32_t i = 0; i < 4; i++) {
        if (m_ChannelSize[i] != 0) {
            channelCount++;
        } else {
            break;
        }
    }

    return channelCount;
}

uint32_t Image::getChannelSize(const Channel channelName) const
{
    if (channelName > Channel::ALPHA) {
        return 0;
    }

    return m_ChannelSize[getChannelIndex(channelName)];
}

uint32_t Image::getChannelIndex(const Channel channelName) const
{
    if (channelName > Channel::ALPHA) {
        return 0;
    }

    return m_ChannelIndex[(uint32_t)channelName];
}

bool Image::toLinear()
{
    if (m_Format == Format::UNDEFINED) {
        core::logError("toLinear called on uninitialized Image");
        return false;
    }

    math::vec4 color;

    for (uint32_t level = 0; level < m_LevelCount; level++) {
        for (uint32_t array = 0; array < m_ArrayCount; array++) {
            for (uint32_t face = 0; face < m_FaceCount; face++) {
                for (uint32_t z = 0; z < m_Depth; z++) {
                    for (uint32_t y = 0; y < m_Height; y++) {
                        for (uint32_t x = 0; x < m_Width; x++) {
                            if (!getPixelColor(color, x, y, z, face, array, level)) {
                                return false;
                            }

                            for (uint32_t channelIndex = 0; math::min(getChannelCount(), 4u); channelIndex++) {
                                // Alpha not touched.
                                if (getChannelIndex(static_cast<Channel>(channelIndex)) == 3u) {
                                    continue;
                                }

                                color[channelIndex] = (color[channelIndex] <= 0.04045f) ? (color[channelIndex] / 12.92f) : (powf((color[channelIndex] + 0.055f) / 1.055f, 2.4f));
                            }

                            if (!setPixelColor(color, x, y, face, array, level)) {
                                return false;
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool Image::toSRGB()
{
    if (m_Format == Format::UNDEFINED) {
        core::logError("toSRGB called on uninitialized Image");
        return false;
    }

    math::vec4 color;

    for (uint32_t level = 0; level < m_LevelCount; level++) {
        for (uint32_t array = 0; array < m_ArrayCount; array++) {
            for (uint32_t face = 0; face < m_FaceCount; face++) {
                for (uint32_t z = 0; z < m_Depth; z++) {
                    for (uint32_t y = 0; y < m_Height; y++) {
                        for (uint32_t x = 0; x < m_Width; x++) {
                            if (!getPixelColor(color, x, y, z, face, array, level)) {
                                return false;
                            }

                            for (uint32_t channelIndex = 0; channelIndex < math::min(getChannelCount(), 4u); channelIndex++) {
                                // Alpha not touched.
                                if (getChannelIndex(static_cast<Channel>(channelIndex)) == 3u) {
                                    continue;
                                }

                                color[channelIndex] = (color[channelIndex] <= 0.0031308f) ? (color[channelIndex] * 12.92f) : (1.055f * powf(color[channelIndex], 1.0f / 2.4f) - 0.055f);
                            }

                            if (!setPixelColor(color, x, y, z, face, array, level)) {
                                return false;
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}
