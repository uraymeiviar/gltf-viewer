// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "Hash.h"
#include <tuple>

namespace ux3d::core {
template <class... T>
struct Hash<std::tuple<T...>> {
    uint64_t operator()(const std::tuple<T...>& tpl, uint64_t _seed = detail::g_seed) const noexcept
    {
        Hasher h(_seed);

        std::apply(h, tpl);

        return h.getHash();
    }
};
} // !ux3d::core
