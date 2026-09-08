// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <algorithm>
#include <string>
#include <variant>

#include "core/Vector.h"
#include "image/ImageFormat.h"
#include "image/ImageMetaData.h"
#include "math/VectorTypes.h"

namespace ux3d {
namespace image {
    enum class Format : uint32_t {
        UNDEFINED = 0u,
        NORM = 1u,
        SCALED = 2u,
        INT = 3u,
        FLOAT = 4u
    };

    class Image {
    public:
        explicit Image();

        virtual ~Image() {};

        Image(const Image& _Other);
        Image& operator=(const Image& _Other);

        Image(Image&& _Other);
        Image& operator=(Image&& _Other);

        bool initializeLayout(
            uint32_t _InRedSize,
            uint32_t _InGreenSize,
            uint32_t _InBlueSize,
            uint32_t _InAlphaSize,
            Format _InFormat,
            bool _InSigned,
            bool _InSrgb = false,
            bool _InPacked = false,
            bool _InCompressed = false,
            uint32_t _InRedIndex = 0,
            uint32_t _InGreenIndex = 1,
            uint32_t _InBlueIndex = 2,
            uint32_t _InAlphaIndex = 3);

        bool initializeLayout(
            uint32_t _InWidth,
            uint32_t _InHeight,
            uint32_t _InDepth,
            uint32_t _InFaceCount,
            uint32_t _InArrayCount,
            uint32_t _InLevelCount,
            gpu::Format format);

        bool initialize(
            const uint8_t* _pData,
            size_t _ByteSize,
            uint32_t _InWidth,
            uint32_t _InHeight,
            uint32_t _InDepth = 1u,
            uint32_t _InFaceCount = 1u,
            uint32_t _InArrayCount = 1u,
            uint32_t _InLevelCount = 1u);

        // Returns a string representation the layout of a single pixels, such as "R8B8G8"
        std::string getMemoryLayoutString() const;

        bool initialize(std::vector<uint8_t>&& _InPixels);

        const std::vector<uint8_t>& getPixels() const { return m_Pixels; }

        uint32_t getWidth() const { return m_Width; }
        uint32_t getHeight() const { return m_Height; }
        uint32_t getDepth() const { return m_Depth; }

        uint32_t getFaceCount() const { return m_FaceCount; }
        uint32_t getArrayCount() const { return m_ArrayCount; }
        uint32_t getLevelCount() const { return m_LevelCount; }

        Format getFormat() const { return m_Format; }
        bool isCompressed() const { return m_Compressed; }
        bool isPacked() const { return m_Packed; }
        bool isSigned() const { return m_Signed; }
        bool isSrgb() const { return m_Srgb; }

        size_t getOffset(
            uint32_t _InX = 0,
            uint32_t _InY = 0,
            uint32_t _InZ = 0,
            uint32_t _InFace = 0,
            uint32_t _InArray = 0,
            uint32_t _InLevel = 0) const;
        uint32_t getPixelSize() const;
        uint32_t getChannelCount() const;
        uint32_t getChannelSize(Channel channelName) const;
        uint32_t getChannelIndex(Channel channelName) const;

        bool getPixelColor(math::vec4& _OutColor, uint32_t _InX, uint32_t _InY, uint32_t _InZ = 0u,
            uint32_t _InFace = 0u, uint32_t _InArray = 0u, uint32_t _InLevel = 0u) const;
        bool setPixelColor(const math::vec4& _InColor, uint32_t _InX, uint32_t _InY, uint32_t _InZ = 0u,
            uint32_t _InFace = 0u, uint32_t _InArray = 0u, uint32_t _InLevel = 0u);

        bool toLinear();
        bool toSRGB();

        void setWidth(uint32_t _width) { m_Width = _width; };
        void setHeight(uint32_t _height) { m_Height = _height; };

        ImageMetaData m_metaData;

    private:
        Format m_Format = Format::UNDEFINED;
        bool m_Compressed = false;
        bool m_Packed = false;
        bool m_Signed = false;
        bool m_Srgb = false;

        uint32_t m_ChannelSize[4] = { 0, 0, 0, 0 };
        uint32_t m_ChannelIndex[4] = { 0, 1, 2, 3 };

        uint32_t m_Width = 0u;
        uint32_t m_Height = 0u;
        uint32_t m_Depth = 0u;
        uint32_t m_FaceCount = 0u;
        uint32_t m_ArrayCount = 0u;
        uint32_t m_LevelCount = 0u;

        std::vector<uint8_t> m_Pixels;
    };

}
} // !ux3d
