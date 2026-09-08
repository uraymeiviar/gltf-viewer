// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <cstddef> // size_t
#include <cstdint>

namespace ux3d::core {
class BinaryStreamRead {
public:
    BinaryStreamRead();
    BinaryStreamRead(const void* _pData, size_t _byteSize);
    BinaryStreamRead(const BinaryStreamRead& _rhs);
    BinaryStreamRead(BinaryStreamRead&& _rhs) noexcept;

    BinaryStreamRead& operator=(const BinaryStreamRead& _rhs);
    BinaryStreamRead& operator=(BinaryStreamRead&& _rhs) noexcept;

    void seekg(size_t _position);
    size_t tellg() const;
    size_t size() const;

    template <class T>
    BinaryStreamRead& operator>>(T& t);

    template <class T>
    const T* currentData() const;

protected:
    const void* m_pData = nullptr;
    size_t m_Size = 0u; // [Byte]
    size_t m_Position = 0u; // [Byte]
};

template <class T>
inline BinaryStreamRead& BinaryStreamRead::operator>>(T& t)
{
    t = *currentData<T>();
    m_Position += sizeof(T);
    return *this;
}

template <class T>
inline const T* BinaryStreamRead::currentData() const
{
    return reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(m_pData) + m_Position);
}
} // !ux3d::core
