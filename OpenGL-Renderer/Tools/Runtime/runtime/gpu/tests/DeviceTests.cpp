// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  DeviceTests.cpp
//
//  Created by Moritz Becher on 14.06.19.
//

#include <catch2/catch.hpp>

#include "TestInitializer.h"
#include "core/HeapAllocator.h"
#include "core/Vector.h"

// TODO: currently these tests fail on the linux build server for unknown reasons
/*
TEST_CASE("Device selection", "[runtime.gpu.Device]")
{
    using namespace ux3d;

    core::HeapAllocator allocator("Allocator");

    gpu_tests::Initializer<core::HeapAllocator> testsInitializer(allocator);
    auto instance = testsInitializer.createInstance();

    core::Vector<gpu::PhysicalDevice, core::HeapAllocator> physicalDevices;
    gpu::enumeratePhysicalDevices(instance, allocator, physicalDevices);

    gpu::PhysicalDevice* pSelectedPhysicalDevice = nullptr;
    for(auto physicalDevice : physicalDevices)
    {
        gpu::PhysicalDeviceProperties properties;
        gpu::getPhysicalDeviceProperties(allocator, physicalDevice, properties);
        if(false == properties.hasSwapchainSupport)
        {
            continue;
        }

        pSelectedPhysicalDevice = &physicalDevice;
    }

    REQUIRE(nullptr != pSelectedPhysicalDevice);
}

*/
