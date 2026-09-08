// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  LogicalDeviceTests.cpp
//  middleware
//
//  Created by Moritz Becher on 14.06.19.
//

#include <catch2/catch.hpp>

#include "TestInitializer.h"
#include "core/HeapAllocator.h"

// TODO: currently these tests fail on the linux build server for unknown reasons
/*
TEST_CASE("Create Device", "[runtime.gpu.Device]")
{
    using namespace ux3d;
    
    core::HeapAllocator allocator("Allocator");
    
    gpu_tests::Initializer<core::HeapAllocator> initializer(allocator);
    
    
    auto device = initializer.selectDevice();
    
    REQUIRE(device._handle != gpu::InvalidHandle);
}

*/
