// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include <catch2/catch.hpp>

#include "core/MemoryUtilities.h"

TEST_CASE("Underaligned allocation", "[runtime.core.MemoryUtilities]")
{
    using namespace ux3d::core;

    void* ptr = allocateAligned(64, 2);
    REQUIRE(ptr != nullptr);
    REQUIRE((size_t)ptr % sizeof(void*) == 0);
}
