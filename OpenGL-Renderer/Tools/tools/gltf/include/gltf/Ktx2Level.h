// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "GltfResult.h"
#include "core/String.h"
#include "core/Vector.h"

//See https://github.com/KhronosGroup/glTF/blob/khr_ktx2_ibl/extensions/2.0/Khronos/KHR_image_ktx2/schema/image.KHR_image_ktx2.schema.json

namespace ux3d {
namespace gltf {
    class Ktx2;
    class Ktx2Level {
    private:
        uint32_t m_ByteOffset = 0u;
        uint32_t m_ByteLength = 0u;
        uint32_t m_UncompressedByteLength = 0u;

    public:
        Ktx2Level() = default;
        Ktx2Level(const Ktx2Level&) = default;
        Ktx2Level(Ktx2Level&&) = default;

        uint32_t getByteOffset() const { return m_ByteOffset; }
        GltfResult setByteOffset(uint32_t byteOffset);

        uint32_t getByteLength() const { return m_ByteLength; }
        GltfResult setByteLength(uint32_t byteLength);

        uint32_t getUncompressedByteLength() const { return m_UncompressedByteLength; };
        GltfResult setUncompressedByteLength(uint32_t uncompressedByteLength);

        bool isValid() const;
        core::Vector<core::string> getInvalidProperties() const;
    };
}
}
