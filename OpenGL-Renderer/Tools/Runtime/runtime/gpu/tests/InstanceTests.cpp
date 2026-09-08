// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  InstanceTests.cpp
//  middleware
//
//  Created by Moritz Becher on 13.06.19.
//

#include <catch2/catch.hpp>

#include "TestInitializer.h"
#include "core/HeapAllocator.h"

TEST_CASE("Create Instance", "[runtime.gpu.Instance]")
{
    using namespace ux3d;

    core::HeapAllocator allocator;

    gpu_tests::Initializer testsInitializer(&allocator);
    auto instance = testsInitializer.createInstance();

    REQUIRE(instance._handle != gpu::InvalidHandle);
}
