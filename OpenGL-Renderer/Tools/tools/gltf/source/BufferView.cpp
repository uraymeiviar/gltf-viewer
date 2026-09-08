// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/BufferView.h"
#include "gltf/Buffer.h"

ux3d::gltf::BufferView::BufferView(uint32_t _id)
    : ChildOfRoot(_id)
{
}

ux3d::gltf::BufferView::BufferView(uint32_t _id, const BufferView& _source)
    : ChildOfRoot(_id, _source)
    , m_pBuffer(_source.m_pBuffer)
    , m_byteOffset(_source.m_byteOffset)
    , m_byteLength(_source.m_byteLength)
    , m_byteStride(_source.m_byteStride)
    , m_target(_source.m_target)
{
}

ux3d::gltf::GltfResult ux3d::gltf::BufferView::getData(core::MemoryRange<const uint8_t>& _outData) const
{
    if (m_pBuffer == nullptr) {
        return GltfError::ObjectNotFound;
    }

    core::MemoryRange<const uint8_t> bufferData;
    m_pBuffer->getData(bufferData);

    if (bufferData.empty()) {
        return GltfError::ObjectNotFound;
    }

    _outData = core::makeRange(bufferData.data() + getByteOffset() * sizeof(uint8_t), m_byteLength);
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::BufferView::setBuffer(Buffer& _buffer)
{
    m_pBuffer = &_buffer;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::BufferView::setByteOffset(uint32_t _offset)
{
    m_byteOffset.emplace(_offset);
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::BufferView::setByteLength(uint32_t _length)
{
    if (_length < 1) {
        return GltfError::InvalidInput;
    }
    m_byteLength = _length;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::BufferView::setByteStride(uint32_t _stride)
{
    if (_stride < 4 || _stride > 253) {
        return GltfError::InvalidInput;
    }
    m_byteStride.emplace(_stride);
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::BufferView::setTarget(uint32_t _target)
{
    if (_target != TargetArrayBuffer && _target != ElementArrayBuffer) {
        return GltfError::InvalidInput;
    }
    m_target.emplace(_target);
    return GltfResult::Success;
}

bool ux3d::gltf::BufferView::isValid() const
{
    if (m_pBuffer == nullptr) {
        return false;
    }
    if (m_byteLength == 0) {
        return false;
    }
    return true;
}

ux3d::gltf::GltfResult ux3d::gltf::BufferView::getInvalidProperties(core::Vector<core::string>& _invalidProperties) const
{
    if (m_pBuffer == nullptr) {
        if (_invalidProperties.push_back("buffer") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    if (m_byteLength == 0) {
        if (_invalidProperties.push_back("byteLength") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    return GltfResult::Success;
}
