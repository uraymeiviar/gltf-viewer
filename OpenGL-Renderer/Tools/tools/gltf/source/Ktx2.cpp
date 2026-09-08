// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Ktx2.h"

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setVkFormat(uint32_t _VkFormat)
{
    m_VkFormat = _VkFormat;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setPixelWidth(uint32_t _pixelWidth)
{
    if (_pixelWidth < 1) {
        return GltfError::InvalidInput;
    }
    m_PixelWidth = _pixelWidth;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setPixelHeight(uint32_t _pixelHeight)
{
    m_PixelHeight = _pixelHeight;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setPixelDepth(uint32_t _pixelDepth)
{
    m_PixelDepth = _pixelDepth;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setLayerCount(uint32_t _LayerCount)
{
    m_LayerCount = _LayerCount;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setFaceCount(uint32_t _FaceCount)
{
    m_FaceCount = _FaceCount;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setSupercompressionScheme(uint32_t _SupercompressionScheme)
{
    m_SupercompressionScheme = _SupercompressionScheme;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setDfdByteOffset(uint32_t _DfdByteOffset)
{
    m_DfdByteOffset = _DfdByteOffset;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setDfdByteLength(uint32_t _DfdByteLength)
{
    m_DfdByteLength = _DfdByteLength;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setSgdByteOffset(uint32_t _SgdByteOffset)
{
    m_SgdByteOffset = _SgdByteOffset;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setSgdByteLength(uint32_t _SgdByteLength)
{
    m_SgdByteLength = _SgdByteLength;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2::setGenerateMipMaps(bool _GenerateMipMaps)
{
    m_GenerateMipmaps = _GenerateMipMaps;
    return GltfResult::Success;
}
