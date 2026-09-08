// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "core/BinaryStreamRead.h"
#include "core/Logger.h"

ux3d::core::BinaryStreamRead::BinaryStreamRead()
{
}

ux3d::core::BinaryStreamRead::BinaryStreamRead(const BinaryStreamRead& _rhs)
    : m_pData(_rhs.m_pData)
    , m_Size(_rhs.m_Size)
    , m_Position(_rhs.m_Position)
{
}

ux3d::core::BinaryStreamRead::BinaryStreamRead(BinaryStreamRead&& _rhs) noexcept
    : m_pData(_rhs.m_pData)
    , m_Size(_rhs.m_Size)
    , m_Position(_rhs.m_Position)
{
    _rhs.m_pData = nullptr;
    _rhs.m_Size = 0;
    _rhs.m_Position = 0;
}

ux3d::core::BinaryStreamRead::BinaryStreamRead(const void* _pData, size_t _byteSize)
{
    if (_pData == nullptr || _byteSize == 0) {
        core::logWarning("BinaryStreamRead initialization: invalid parameter");
    }

    m_pData = _pData;
    m_Size = _byteSize;
}

ux3d::core::BinaryStreamRead& ux3d::core::BinaryStreamRead::operator=(const BinaryStreamRead& _rhs)
{
    if (this == &_rhs)
        return *this;

    m_pData = _rhs.m_pData;
    m_Size = _rhs.m_Size;
    m_Position = _rhs.m_Position;

    return *this;
}

ux3d::core::BinaryStreamRead& ux3d::core::BinaryStreamRead::operator=(BinaryStreamRead&& _rhs) noexcept
{
    if (this == &_rhs)
        return *this;

    m_pData = _rhs.m_pData;
    m_Size = _rhs.m_Size;
    m_Position = _rhs.m_Position;

    _rhs.m_pData = nullptr;
    _rhs.m_Size = 0;
    _rhs.m_Position = 0;

    return *this;
}

void ux3d::core::BinaryStreamRead::seekg(size_t _position)
{
    m_Position = _position;
}

size_t ux3d::core::BinaryStreamRead::tellg() const
{
    return m_Position;
}

size_t ux3d::core::BinaryStreamRead::size() const
{
    return m_Size;
}
