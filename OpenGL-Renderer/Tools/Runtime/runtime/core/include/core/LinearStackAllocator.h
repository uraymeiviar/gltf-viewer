// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "LinearAllocator.h"

namespace ux3d {
namespace core {
    template <size_t N, class T = uint8_t>
    class LinearStackAllocator : public LinearAllocator {
    public:
        LinearStackAllocator()
            : LinearAllocator(&m_Data[0], sizeof(T[N])) {};
        ~LinearStackAllocator() = default;

    private:
        T m_Data[N];
    };
}
} // !ux3d
