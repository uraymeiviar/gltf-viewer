// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "core/BinaryStream.h"
#include "core/Logger.h"

ux3d::core::BinaryStream::BinaryStream()
{
}

ux3d::core::BinaryStream::BinaryStream(const BinaryStream& _rhs)
    : BinaryStreamRead(_rhs)
{
}

ux3d::core::BinaryStream::BinaryStream(BinaryStream&& _rhs) noexcept
    : BinaryStreamRead(std::move(_rhs))
{
}

ux3d::core::BinaryStream::BinaryStream(void* _pData, size_t _byteSize)
    : BinaryStreamRead(_pData, _byteSize)
{
}

ux3d::core::BinaryStream& ux3d::core::BinaryStream::operator=(const BinaryStream& _rhs)
{
    BinaryStreamRead::operator=(_rhs);
    return *this;
}

ux3d::core::BinaryStream& ux3d::core::BinaryStream::operator=(BinaryStream&& _rhs) noexcept
{
    BinaryStreamRead::operator=(std::move(_rhs));
    return *this;
}
