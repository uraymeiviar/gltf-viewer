// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "core/LinearAllocator.h"
#include "core/Logger.h"

ux3d::core::LinearAllocator::LinearAllocator(size_t _TotalByteSize, uint32_t _Alignment)
    : m_TotalSize(_TotalByteSize)
    , m_pData(allocateAligned(_TotalByteSize, _Alignment))
    , m_ExternalData(false)
{
}

ux3d::core::LinearAllocator::LinearAllocator(void* _pData, size_t _TotalByteSize)
    : m_TotalSize(_TotalByteSize)
    , m_pData(_pData)
    , m_ExternalData(true)
{
}

ux3d::core::LinearAllocator::~LinearAllocator()
{
    if (m_pData != nullptr && !m_ExternalData) {
        freeAligned(m_pData);
    }
}

ux3d::core::LinearAllocator::LinearAllocator(LinearAllocator&& _Other) noexcept
    : m_TotalSize(_Other.m_TotalSize)
    , m_Allocated(_Other.m_Allocated)
    , m_pData(_Other.m_pData)
    , m_ExternalData(_Other.m_ExternalData)
{
    _Other.m_pData = nullptr;
    _Other.m_TotalSize = 0u;
    _Other.m_Allocated = 0u;
}

ux3d::core::LinearAllocator ux3d::core::LinearAllocator::child(size_t _TotalByteSize, uint32_t _Alignment)
{
    return LinearAllocator(allocate(_TotalByteSize, _Alignment), _TotalByteSize);
}

void* ux3d::core::LinearAllocator::allocate(size_t _ByteSize, uint32_t _Alignment)
{
    void* pData = reinterpret_cast<uint8_t*>(m_pData) + m_Allocated;

    size_t space = m_TotalSize - m_Allocated;
    if (assertion(alignUp(_Alignment, _ByteSize, pData, space) != nullptr, "%ull-byte upward alignment of %ull bytes failed", _Alignment, _ByteSize)) {
        m_Allocated = uintptr_t(pData) - uintptr_t(m_pData) + _ByteSize;
        return pData;
    }

    return nullptr;
}

bool ux3d::core::LinearAllocator::resize(size_t _TotalByteSize, uint32_t _Alignment)
{
    // memory is not owned by this arena
    if (assertion(m_ExternalData == false, "LinearAllocator with externaly owned memory cannot resize")) {
        return false;
    }

    void* pData = allocateAligned(_TotalByteSize, _Alignment); // asserts

    if (pData != nullptr) {
        if (m_pData != nullptr) {
            freeAligned(m_pData);
        }

        m_pData = pData;
        m_Allocated = 0u;
        m_TotalSize = _TotalByteSize;

        return true;
    }

    return false;
}

void ux3d::core::LinearAllocator::reset()
{
    m_Allocated = 0u;
}
