// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "GltfResult.h"
#include "Ktx2Level.h"
#include "core/LinkedList.h"
#include "core/String.h"

//See https://github.com/KhronosGroup/glTF/blob/khr_ktx2_ibl/extensions/2.0/Khronos/KHR_image_ktx2/schema/image.KHR_image_ktx2.schema.json

namespace ux3d {
namespace gltf {
    class Image;
    class Ktx2 {
    public:
        struct KTXMetaData {
            //[0,63]
            uint32_t KTXcubemapIncomplete = 0u;
            // pattern "^[rl]([du]|[du][oi])?$"
            core::string KTXorientation;

            uint32_t KTXglFormat[3] = { 0u, 0u, 0u };
            uint32_t KTXdxgiFormat = 0u;
            uint32_t KTXmetalPixelFormat = 0u;
            // pattern "^[rgba01]{4}$"
            core::string KTXswizzle;
            core::string KTXwriter;
            bool KTXastcDecodeRGB9E5 = true;
        };

    private:
        uint32_t m_VkFormat = 0u;
        uint32_t m_PixelWidth = 1u; // >=1
        uint32_t m_PixelHeight = 0u;
        uint32_t m_PixelDepth = 0u;
        uint32_t m_LayerCount = 0u;
        core::LinkedList<Ktx2Level> m_Levels;

        uint32_t m_FaceCount = 1u;
        uint32_t m_SupercompressionScheme = 0u;
        uint32_t m_DfdByteOffset = 0u;
        uint32_t m_DfdByteLength = 0u;
        uint32_t m_SgdByteOffset = 0u;
        uint32_t m_SgdByteLength = 0u;
        bool m_GenerateMipmaps = false;

        KTXMetaData m_MetaData;

    public:
        Ktx2() = default;
        Ktx2(const Ktx2&) = default;
        Ktx2(Ktx2&&) = default;

        uint32_t getVkFormat() const { return m_VkFormat; }
        GltfResult setVkFormat(uint32_t _VkFormat);

        uint32_t getPixelWidth() const { return m_PixelWidth; }
        GltfResult setPixelWidth(uint32_t _pixelWidth);

        uint32_t getPixelHeight() const { return m_PixelHeight; }
        GltfResult setPixelHeight(uint32_t _pixelHeight);

        uint32_t getPixelDepth() const { return m_PixelDepth; }
        GltfResult setPixelDepth(uint32_t _pixelDepth);

        uint32_t getLayerCount() const { return m_LayerCount; }
        GltfResult setLayerCount(uint32_t _LayerCount);

        uint32_t getFaceCount() const { return m_FaceCount; }
        GltfResult setFaceCount(uint32_t _FaceCount);

        uint32_t getSupercompressionScheme() const { return m_SupercompressionScheme; }
        GltfResult setSupercompressionScheme(uint32_t supercompressionScheme);

        uint32_t getDfdByteOffset() const { return m_DfdByteOffset; }
        GltfResult setDfdByteOffset(uint32_t _DfdByteOffset);

        uint32_t getDfdByteLength() const { return m_DfdByteLength; }
        GltfResult setDfdByteLength(uint32_t _DfdByteLength);

        uint32_t getSgdByteOffset() const { return m_SgdByteOffset; }
        GltfResult setSgdByteOffset(uint32_t _SgdByteOffset);

        uint32_t getSgdByteLength() const { return m_SgdByteLength; }
        GltfResult setSgdByteLength(uint32_t _SgdByteLength);

        bool getGenerateMipMaps() const { return m_GenerateMipmaps; }
        GltfResult setGenerateMipMaps(bool _GenerateMipMaps);

        KTXMetaData& getMetaData() { return m_MetaData; }
        const KTXMetaData& getMetaData() const { return m_MetaData; }

        Ktx2Level* addLevel() { return m_Levels.emplace_back(); }

        core::LinkedList<Ktx2Level>& getLevels() { return m_Levels; }
        const core::LinkedList<Ktx2Level>& getLevels() const { return m_Levels; }
    };
}
}
