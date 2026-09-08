// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/BinaryStreamRead.h"

namespace ux3d::core {
class BinaryStream : public BinaryStreamRead {
public:
    BinaryStream();
    BinaryStream(void* _pData, size_t _byteSize);
    BinaryStream(const BinaryStream& _rhs);
    BinaryStream(BinaryStream&& _rhs) noexcept;

    BinaryStream& operator=(const BinaryStream& _rhs);
    BinaryStream& operator=(BinaryStream&& _rhs) noexcept;

    template <class T>
    BinaryStream& operator<<(const T& t);

    template <class T>
    T* currentData();
};

template <class T>
inline BinaryStream& BinaryStream::operator<<(const T& t)
{
    *currentData<T>() = t;
    m_Position += sizeof(T);
    return *this;
}

template <class T>
inline T* BinaryStream::currentData()
{
    return reinterpret_cast<T*>(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(m_pData)) + m_Position);
}
} // !ux3d::core
