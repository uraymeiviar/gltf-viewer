// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"

#include "core/LinearAllocator.h"
#include <stdint.h>

TEST_CASE("Test allocation of zero bytes", "[runtime.core.LinearAllocator]")
{
    uint32_t arenaSize = 1024u;
    ux3d::core::LinearAllocator arena(arenaSize);
    uintptr_t start = reinterpret_cast<uintptr_t>(arena.allocate(arenaSize));
    uintptr_t end = reinterpret_cast<uintptr_t>(arena.allocate(0));
    REQUIRE(end == start + arenaSize);
}

uintptr_t allocateRecursive(ux3d::core::LinearAllocator& _Arena, size_t _RemainingSize)
{
    // allocate half of the remaining allocation size
    uintptr_t start = reinterpret_cast<uintptr_t>(_Arena.allocate((_RemainingSize + 1u) / 2u));

    // exit if there was no allocation --> start contains the address directly after the arenas memory
    if (_RemainingSize == 0u) {
        return start;
    }

    // allocate the remaining half in recursively decreasing chunks
    uintptr_t end = allocateRecursive(_Arena, _RemainingSize / 2u);

    REQUIRE(end == start + _RemainingSize);

    return end;
}

TEST_CASE("Test linear allocation", "[runtime.core.LinearAllocator]")
{
    constexpr size_t arenaSize = 1024u;
    ux3d::core::LinearAllocator arena(arenaSize);

    allocateRecursive(arena, arenaSize);
}
