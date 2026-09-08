// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "Hash.h"
#include "Vector.h"

namespace ux3d::core {
template <class T>
struct Hash<core::Vector<T>> {
    uint64_t operator()(const core::Vector<T>& vec, uint64_t _seed = detail::g_seed) const noexcept
    {
        Hasher h(_seed);
        for (const T& x : vec) {
            h << x;
        }

        return h.getHash();
    }
};
} // !ux3d::core
