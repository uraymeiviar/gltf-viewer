// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "core/MemoryRange.h"
#include <optional>

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#bufferview

namespace ux3d::gltf {
class Buffer;

class BufferView : public ChildOfRoot {
public:
    static const uint32_t TargetArrayBuffer = 34962;
    static const uint32_t ElementArrayBuffer = 34963;

    BufferView(uint32_t _id); // regular constructor
    BufferView(uint32_t _id, const BufferView& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::BufferView; }

    Buffer* getBuffer() { return m_pBuffer; }
    const Buffer* getBuffer() const { return m_pBuffer; }
    GltfResult setBuffer(Buffer& _buffer);
    void unsetBuffer() { m_pBuffer = nullptr; }

    bool hasByteOffset() const { return m_byteOffset.has_value(); }
    uint32_t getByteOffset() const { return m_byteOffset.value_or(0u); }
    GltfResult setByteOffset(uint32_t _offset);

    uint32_t getByteLength() const { return m_byteLength; }
    GltfResult setByteLength(uint32_t _length);

    std::optional<uint32_t> getByteStride() const { return m_byteStride; }
    GltfResult setByteStride(uint32_t _stride);
    void unsetByteStride() { m_byteStride.reset(); }

    bool hasTarget() const { return m_target.has_value(); }
    std::optional<uint32_t> getTarget() const { return m_target; }
    GltfResult setTarget(uint32_t _target);

    GltfResult getData(core::MemoryRange<const uint8_t>& _outData) const;

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const override;

private:
    Buffer* m_pBuffer = nullptr;
    std::optional<uint32_t> m_byteOffset;
    uint32_t m_byteLength = 0u;
    std::optional<uint32_t> m_byteStride;
    std::optional<uint32_t> m_target;
};
}
