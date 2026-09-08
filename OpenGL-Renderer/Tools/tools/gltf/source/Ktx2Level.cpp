// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Ktx2Level.h"

ux3d::gltf::GltfResult ux3d::gltf::Ktx2Level::setByteOffset(uint32_t byteOffset)
{
    m_ByteOffset = byteOffset;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2Level::setByteLength(uint32_t byteLength)
{
    m_ByteLength = byteLength;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Ktx2Level::setUncompressedByteLength(uint32_t uncompressedByteLength)
{
    m_UncompressedByteLength = uncompressedByteLength;
    return GltfResult::Success;
}

bool ux3d::gltf::Ktx2Level::isValid() const
{
    return true;
}

ux3d::core::Vector<ux3d::core::string> ux3d::gltf::Ktx2Level::getInvalidProperties() const
{
    return {};
}
